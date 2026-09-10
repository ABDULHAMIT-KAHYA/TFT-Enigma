import argparse
import json
from pathlib import Path

from dataset import load_dataset, split_by_seed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    args = parser.parse_args()
    records, raw = load_dataset(args.input)
    train, valid = split_by_seed(records)
    report = {
        "schemaVersion": raw.get("schemaVersion"),
        "featureSchemaVersion": raw.get("featureSchemaVersion"),
        "actionSchemaVersion": raw.get("actionSchemaVersion"),
        "records": len(records),
        "trainRecords": len(train),
        "validationRecords": len(valid),
        "trainSeeds": sorted({r.game_seed for r in train}),
        "validationSeeds": sorted({r.game_seed for r in valid}),
        "stateDim": len(records[0].state) if records else 0,
        "legalActionsFirstRecord": len(records[0].legal_actions) if records else 0,
    }
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
