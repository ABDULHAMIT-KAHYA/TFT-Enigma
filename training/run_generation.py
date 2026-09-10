import argparse
import json
import shutil
import subprocess
from pathlib import Path


def run(cmd, cwd=None):
    print("$", " ".join(str(c) for c in cmd))
    completed = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)
    if completed.stdout:
        print(completed.stdout)
    if completed.stderr:
        print(completed.stderr)
    if completed.returncode != 0:
        raise SystemExit(completed.returncode)
    return completed.stdout


def parse_eval(text):
    metrics = {"raw": text}
    for line in text.splitlines():
        if line.startswith("Candidate learned"):
            metrics["candidate"] = parse_metric_line(line)
        elif line.startswith("Baseline heuristic"):
            metrics["baseline"] = parse_metric_line(line)
        elif line.startswith("Head-to-head placement advantage="):
            metrics["placementAdvantage"] = float(line.split("=", 1)[1])
        elif line.startswith("Promotion decision:"):
            metrics["decision"] = line.split(":", 1)[1].strip()
        elif line.startswith("Agent evaluation complete"):
            metrics["summary"] = line
    return metrics


def parse_metric_line(line):
    out = {}
    for part in line.split("|")[1:]:
        key, value = part.strip().split("=", 1)
        out[key] = float(value)
    return out


def maybe_promote(model_dir: Path, generation: int, eval_metrics: dict, margin: float):
    promoted = eval_metrics.get("placementAdvantage", -999.0) >= margin and eval_metrics.get("decision") == "PROMOTED"
    models = model_dir.parent
    history = models / "history" / f"gen_{generation:03d}"
    history.mkdir(parents=True, exist_ok=True)
    for name in ["policy.json", "value.json", "metrics.json"]:
        src = model_dir / name
        if src.exists():
            shutil.copy2(src, history / name)
    (history / "evaluation.json").write_text(json.dumps(eval_metrics, indent=2, sort_keys=True))
    if promoted:
        shutil.copy2(model_dir / "policy.json", models / "champion_policy.json")
        shutil.copy2(model_dir / "value.json", models / "champion_value.json")
        champion = {
            "generation": generation,
            "promoted": True,
            "evaluation": eval_metrics,
        }
        (models / "champion_metadata.json").write_text(json.dumps(champion, indent=2, sort_keys=True))
    return promoted, history


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", default="simulator/src/engine.exe")
    parser.add_argument("--generation", type=int, default=1)
    parser.add_argument("--games", type=int, default=20)
    parser.add_argument("--eval-games", type=int, default=10)
    parser.add_argument("--base-seed", type=int, default=10000)
    parser.add_argument("--eval-seed", type=int, default=500000)
    parser.add_argument("--promotion-margin", type=float, default=0.10)
    args = parser.parse_args()

    root = Path.cwd()
    engine = root / args.engine
    results = root / "simulator" / "src" / "results"
    models = root / "models"
    results.mkdir(parents=True, exist_ok=True)
    models.mkdir(parents=True, exist_ok=True)

    dataset = results / f"gen_{args.generation:03d}_selfplay.json"
    model_dir = models / f"gen_{args.generation:03d}"

    run([str(engine), "--selfplay", str(args.games), "--selfplay-seed", str(args.base_seed), "--selfplay-output", str(dataset)], cwd=engine.parent)
    run(["python", "training/train.py", "--input", str(dataset), "--output", str(model_dir)], cwd=root)
    eval_text = run([str(engine), "--evaluate-agents", str(args.eval_games), "--eval-seed", str(args.eval_seed), "--policy-model", str(model_dir / "policy.json")], cwd=engine.parent)
    eval_metrics = parse_eval(eval_text)
    promoted, history = maybe_promote(model_dir, args.generation, eval_metrics, args.promotion_margin)

    report = {
        "generation": args.generation,
        "selfplayGames": args.games,
        "evalGames": args.eval_games,
        "dataset": str(dataset),
        "modelDir": str(model_dir),
        "historyDir": str(history),
        "promoted": promoted,
        "evaluation": eval_metrics,
    }
    report_path = model_dir / "generation_report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True))
    print("PROMOTED:", "YES" if promoted else "NO")
    print("Report:", report_path)


if __name__ == "__main__":
    main()
