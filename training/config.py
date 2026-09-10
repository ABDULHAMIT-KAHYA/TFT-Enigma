from dataclasses import dataclass

FEATURE_SCHEMA_VERSION = 2
ACTION_SCHEMA_VERSION = 1

ACTION_FIELDS = [
    "actionType",
    "shopIndex",
    "boardIndex",
    "benchIndex",
    "itemIndex",
    "targetX",
    "targetY",
    "unitContentId",
    "auxiliaryId",
    "goldCost",
]

DEFAULT_SEED = 1337
DEFAULT_POLICY_EPOCHS = 8
DEFAULT_VALUE_EPOCHS = 8
DEFAULT_LR = 0.01
DEFAULT_WEIGHT_DECAY = 1e-5
VALIDATION_SEED_MOD = 5

@dataclass(frozen=True)
class TrainConfig:
    seed: int = DEFAULT_SEED
    policy_epochs: int = DEFAULT_POLICY_EPOCHS
    value_epochs: int = DEFAULT_VALUE_EPOCHS
    lr: float = DEFAULT_LR
    weight_decay: float = DEFAULT_WEIGHT_DECAY
