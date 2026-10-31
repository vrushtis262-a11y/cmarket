
#!/usr/bin/env python3

import argparse
import json
import platform
import statistics
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


BENCHMARKS = (
    "baseline_benchmark",
    "latency_benchmark",
    "throughput_benchmark",
    "concurrent_benchmark",
)


def run_once(executable, timeout):
    started = time.perf_counter()

    completed = subprocess.run(
        [str(executable)],
        capture_output=True,
        text=True,
        timeout=timeout,
        check=False,
    )

    elapsed = time.perf_counter() - started

    if completed.returncode != 0:
        raise RuntimeError(
            f"{executable.name} exited with code "
            f"{completed.returncode}\n"
            f"{completed.stdout}\n{completed.stderr}"
        )

    return elapsed, completed.stdout


def benchmark(executable, warmups, runs, timeout):
    for _ in range(warmups):
        run_once(executable, timeout)

    durations = []
    outputs = []

    for _ in range(runs):
        elapsed, output = run_once(executable, timeout)
        durations.append(elapsed)
        outputs.append(output)

    return {
        "runs": runs,
        "warmups": warmups,
        "seconds": {
            "median": statistics.median(durations),
            "mean": statistics.mean(durations),
            "minimum": min(durations),
            "maximum": max(durations),
            "samples": durations,
        },
        "sample_output": outputs[-1],
    }


def main():
    parser = argparse.ArgumentParser(
        description="Run the cmarket benchmark suite."
    )

    parser.add_argument(
        "--build-dir",
        type=Path,
        default=Path("build-release"),
    )

    parser.add_argument(
        "--warmups",
        type=int,
        default=1,
    )

    parser.add_argument(
        "--runs",
        type=int,
        default=3,
    )

    parser.add_argument(
        "--timeout",
        type=int,
        default=300,
    )

    parser.add_argument(
        "--output",
        type=Path,
        default=Path("benchmark-results.json"),
    )

    args = parser.parse_args()

    if args.runs < 1 or args.warmups < 0 or args.timeout < 1:
        parser.error(
            "runs and timeout must be positive; "
            "warmups cannot be negative"
        )

    build_dir = args.build_dir.resolve()

    results = {
        "timestamp_utc": datetime.now(
            timezone.utc
        ).isoformat(),
        "platform": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "python": platform.python_version(),
        },
        "build_directory": str(build_dir),
        "benchmarks": {},
    }

    print("\ncmarket Benchmark Suite")
    print("=" * 72)

    for name in BENCHMARKS:
        executable = build_dir / name

        if not executable.is_file():
            print(
                f"Missing executable: {executable}",
                file=sys.stderr,
            )
            return 1

        print(f"\nRunning {name}...", flush=True)

        result = benchmark(
            executable,
            args.warmups,
            args.runs,
            args.timeout,
        )

        results["benchmarks"][name] = result

        timing = result["seconds"]

        print(
            f"  Median: {timing['median']:.4f} s"
        )
        print(
            f"  Mean:   {timing['mean']:.4f} s"
        )
        print(
            f"  Min:    {timing['minimum']:.4f} s"
        )
        print(
            f"  Max:    {timing['maximum']:.4f} s"
        )

    args.output.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    args.output.write_text(
        json.dumps(results, indent=2) + "\n",
        encoding="utf-8",
    )

    print(
        f"\nResults saved to: {args.output}"
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
