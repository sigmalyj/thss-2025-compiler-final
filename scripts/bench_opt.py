#!/usr/bin/env python3
"""Benchmark compiler runtime with and without machine-independent optimizations.

This script runs the functional test suite twice:
- Baseline: optimizations enabled (default)
- No-opt: with SYSY_NO_OPT=1 to disable optimizer

It reports wall-clock time for each run and whether tests passed.
"""
import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEST_CMD = ["python3", "run-test.py"]


def run_once(env):
    start = time.perf_counter()
    proc = subprocess.run(TEST_CMD, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    duration = time.perf_counter() - start
    output = proc.stdout.decode(errors="replace")
    passed = "All tests passed" in output
    return duration, passed, output


def main():
    base_env = os.environ.copy()

    print("Running with optimizations (default)...")
    dur_opt, pass_opt, _ = run_once(base_env)
    print(f"  time: {dur_opt:.2f}s, passed: {pass_opt}")

    env_noopt = base_env.copy()
    env_noopt["SYSY_NO_OPT"] = "1"
    print("Running without optimizations (SYSY_NO_OPT=1)...")
    dur_noopt, pass_noopt, _ = run_once(env_noopt)
    print(f"  time: {dur_noopt:.2f}s, passed: {pass_noopt}")

    speedup = dur_noopt / dur_opt if dur_opt > 0 else 0.0
    print("\nSummary:")
    print(f"  optimized:   {dur_opt:.2f}s (pass={pass_opt})")
    print(f"  no-opt:      {dur_noopt:.2f}s (pass={pass_noopt})")
    print(f"  speedup:     {speedup:.2f}x faster vs no-opt")


if __name__ == "__main__":
    main()
