import json
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple

import numpy as np

from config import ACTION_FIELDS, ACTION_SCHEMA_VERSION, FEATURE_SCHEMA_VERSION, VALIDATION_SEED_MOD


def _action_vector(action: Dict) -> List[float]:
    return [float(action.get(field, -1 if field != "actionType" else 0)) for field in ACTION_FIELDS]


def state_action_features(state: Sequence[float], action: Sequence[float]) -> np.ndarray:
    s = np.asarray(state, dtype=np.float32)
    a = np.asarray(action, dtype=np.float32)
    # Small, deterministic normalization keeps linear training stable and is mirrored in C++.
    s_norm = s / 100.0
    a_norm = a / 100.0
    interactions = np.outer(s_norm, a_norm).reshape(-1)
    return np.concatenate([s_norm, a_norm, interactions]).astype(np.float32)


@dataclass
class DecisionRecord:
    game_seed: int
    state: List[float]
    legal_actions: List[List[float]]
    chosen_action: List[float]
    chosen_index: int
    reward: float
    placement: int


def _same_action(a: Sequence[float], b: Sequence[float]) -> bool:
    return len(a) == len(b) and all(float(x) == float(y) for x, y in zip(a, b))


def load_dataset(path: str) -> Tuple[List[DecisionRecord], Dict]:
    data = json.loads(Path(path).read_text())
    if int(data.get("schemaVersion", 0)) < 2:
        raise ValueError("self-play dataset must have schemaVersion >= 2")
    if int(data.get("featureSchemaVersion", 0)) != FEATURE_SCHEMA_VERSION:
        raise ValueError("unexpected feature schema version")
    if int(data.get("actionSchemaVersion", 0)) != ACTION_SCHEMA_VERSION:
        raise ValueError("unexpected action schema version")

    records: List[DecisionRecord] = []
    for raw in data.get("trainingRecords", []):
        state = [float(x) for x in raw["stateFeatures"]["values"]]
        legal = [_action_vector(a) for a in raw.get("legalActions", [])]
        chosen = _action_vector(raw.get("chosenAction", {}))
        chosen_index = -1
        for i, action in enumerate(legal):
            if _same_action(action, chosen):
                chosen_index = i
                break
        if chosen_index < 0 and legal:
            # Keep the record usable but explicit; this should be rare and visible in metrics.
            chosen_index = 0
            chosen = legal[0]
        if not legal:
            continue
        records.append(DecisionRecord(
            game_seed=int(raw["gameSeed"]),
            state=state,
            legal_actions=legal,
            chosen_action=chosen,
            chosen_index=chosen_index,
            reward=float(raw["reward"]),
            placement=int(raw["placement"]),
        ))
    return records, data


def split_by_seed(records: Sequence[DecisionRecord]) -> Tuple[List[DecisionRecord], List[DecisionRecord]]:
    train: List[DecisionRecord] = []
    valid: List[DecisionRecord] = []
    for rec in records:
        if rec.game_seed % VALIDATION_SEED_MOD == 0:
            valid.append(rec)
        else:
            train.append(rec)
    if not valid and records:
        valid = list(records[-max(1, len(records) // 5):])
        train = list(records[:-len(valid)])
    return train, valid


def policy_matrix(record: DecisionRecord) -> np.ndarray:
    return np.stack([state_action_features(record.state, a) for a in record.legal_actions], axis=0)


def value_matrix(records: Sequence[DecisionRecord]) -> np.ndarray:
    return np.asarray([r.state for r in records], dtype=np.float32) / 100.0


def rewards(records: Sequence[DecisionRecord]) -> np.ndarray:
    return np.asarray([r.reward for r in records], dtype=np.float32)
