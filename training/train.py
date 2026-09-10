import argparse
import json
import math
import random
from pathlib import Path

import numpy as np
import torch
from torch import nn

from config import ACTION_FIELDS, TrainConfig
from dataset import load_dataset, policy_matrix, rewards, split_by_seed, value_matrix
from models import LinearPolicyScorer, LinearValueModel


def _set_seed(seed: int):
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.use_deterministic_algorithms(True)


def _policy_metrics(model, records):
    if not records:
        return {"loss": 0.0, "top1": 0.0, "top3": 0.0}
    loss_fn = nn.CrossEntropyLoss()
    total_loss = 0.0
    top1 = 0
    top3 = 0
    with torch.no_grad():
        for rec in records:
            x = torch.tensor(policy_matrix(rec), dtype=torch.float32)
            scores = model(x).unsqueeze(0)
            target = torch.tensor([rec.chosen_index], dtype=torch.long)
            total_loss += float(loss_fn(scores, target).item())
            order = torch.argsort(scores[0], descending=True)
            if int(order[0]) == rec.chosen_index:
                top1 += 1
            if rec.chosen_index in [int(i) for i in order[: min(3, len(order))]]:
                top3 += 1
    n = len(records)
    return {"loss": total_loss / n, "top1": top1 / n, "top3": top3 / n}


def _value_metrics(model, records):
    if not records:
        return {"loss": 0.0, "mae": 0.0, "correlation": 0.0}
    x = torch.tensor(value_matrix(records), dtype=torch.float32)
    y = torch.tensor(rewards(records), dtype=torch.float32)
    with torch.no_grad():
        pred = model(x)
        loss = float(nn.functional.mse_loss(pred, y).item())
        mae = float(torch.mean(torch.abs(pred - y)).item())
    p = pred.detach().cpu().numpy()
    t = y.detach().cpu().numpy()
    corr = 0.0 if len(records) < 2 or np.std(p) == 0 or np.std(t) == 0 else float(np.corrcoef(p, t)[0, 1])
    return {"loss": loss, "mae": mae, "correlation": corr}


def _export_linear(path: Path, kind: str, model, metadata):
    weight = model.linear.weight.detach().cpu().numpy()[0].astype(float).tolist()
    bias = float(model.linear.bias.detach().cpu().numpy()[0])
    payload = {
        "format": "tft-galaxy-linear-model-v1",
        "kind": kind,
        "weights": weight,
        "bias": bias,
        "metadata": metadata,
    }
    path.write_text(json.dumps(payload, indent=2, sort_keys=True))


def train(input_path: str, output_dir: str, cfg: TrainConfig):
    _set_seed(cfg.seed)
    records, raw = load_dataset(input_path)
    train_records, valid_records = split_by_seed(records)
    if not train_records or not valid_records:
        raise RuntimeError("dataset split produced empty train or validation records")

    first_policy_dim = policy_matrix(train_records[0]).shape[1]
    state_dim = len(train_records[0].state)
    policy = LinearPolicyScorer(first_policy_dim)
    value = LinearValueModel(state_dim)

    policy_opt = torch.optim.AdamW(policy.parameters(), lr=cfg.lr, weight_decay=cfg.weight_decay)
    value_opt = torch.optim.AdamW(value.parameters(), lr=cfg.lr, weight_decay=cfg.weight_decay)
    policy_loss_fn = nn.CrossEntropyLoss()
    value_loss_fn = nn.SmoothL1Loss()

    for _ in range(cfg.policy_epochs):
        for rec in train_records:
            x = torch.tensor(policy_matrix(rec), dtype=torch.float32)
            target = torch.tensor([rec.chosen_index], dtype=torch.long)
            scores = policy(x).unsqueeze(0)
            loss = policy_loss_fn(scores, target)
            policy_opt.zero_grad()
            loss.backward()
            policy_opt.step()

    x_value = torch.tensor(value_matrix(train_records), dtype=torch.float32)
    y_value = torch.tensor(rewards(train_records), dtype=torch.float32)
    for _ in range(cfg.value_epochs):
        pred = value(x_value)
        loss = value_loss_fn(pred, y_value)
        value_opt.zero_grad()
        loss.backward()
        value_opt.step()

    output = Path(output_dir)
    output.mkdir(parents=True, exist_ok=True)
    metadata = {
        "sourceDataset": str(input_path),
        "datasetSchemaVersion": raw.get("schemaVersion"),
        "featureSchemaVersion": raw.get("featureSchemaVersion"),
        "actionSchemaVersion": raw.get("actionSchemaVersion"),
        "stateDim": state_dim,
        "actionDim": len(ACTION_FIELDS),
        "policyFeatureDim": first_policy_dim,
        "trainRecords": len(train_records),
        "validationRecords": len(valid_records),
        "trainSeeds": sorted({r.game_seed for r in train_records}),
        "validationSeeds": sorted({r.game_seed for r in valid_records}),
        "config": cfg.__dict__,
    }
    policy_train = _policy_metrics(policy, train_records)
    policy_valid = _policy_metrics(policy, valid_records)
    value_train = _value_metrics(value, train_records)
    value_valid = _value_metrics(value, valid_records)
    metrics = {
        "policy": {"train": policy_train, "validation": policy_valid},
        "value": {"train": value_train, "validation": value_valid},
        "metadata": metadata,
    }

    _export_linear(output / "policy.json", "policy", policy, metadata)
    _export_linear(output / "value.json", "value", value, metadata)
    (output / "metrics.json").write_text(json.dumps(metrics, indent=2, sort_keys=True))
    print(json.dumps(metrics, indent=2, sort_keys=True))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--seed", type=int, default=1337)
    parser.add_argument("--policy-epochs", type=int, default=8)
    parser.add_argument("--value-epochs", type=int, default=8)
    parser.add_argument("--lr", type=float, default=0.01)
    args = parser.parse_args()
    cfg = TrainConfig(seed=args.seed, policy_epochs=args.policy_epochs, value_epochs=args.value_epochs, lr=args.lr)
    train(args.input, args.output, cfg)


if __name__ == "__main__":
    main()
