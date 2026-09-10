import argparse
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--generations", type=int, default=3)
    parser.add_argument("--games-per-generation", type=int, default=50)
    parser.add_argument("--eval-games", type=int, default=20)
    parser.add_argument("--base-seed", type=int, default=10000)
    parser.add_argument("--eval-seed", type=int, default=500000)
    args = parser.parse_args()

    for gen in range(1, args.generations + 1):
        print(f"=== GENERATION {gen} ===")
        cmd = [
            "python", "training/run_generation.py",
            "--generation", str(gen),
            "--games", str(args.games_per_generation),
            "--eval-games", str(args.eval_games),
            "--base-seed", str(args.base_seed + (gen - 1) * args.games_per_generation),
            "--eval-seed", str(args.eval_seed + (gen - 1) * args.eval_games),
        ]
        subprocess.run(cmd, check=True)


if __name__ == "__main__":
    main()
