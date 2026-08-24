#!/usr/bin/env python3
"""Regenerates bench/results.md (and the benchmark section of README.md)
from scratch by actually running the compiled `mlang` binary and Python,
timing wall-clock, and cross-checking every run's output for agreement --
see CLAUDE.md's "Benchmarks are part of the deliverable, not an
afterthought" and PRD.md M8.

Usage: run.py [--mlang PATH]
"""
import argparse
import datetime
import platform
import statistics
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BENCH_DIR = REPO_ROOT / "bench"

# The tuning knob PRD.md M8 asks to report alongside every number: too low
# and promotion overhead dominates a short-lived call; too high and a
# function that would clearly benefit never gets compiled. 1000 is low
# enough that fib(30)'s ~2.7M calls spend a negligible fraction of the run
# interpreted, but high enough that sum_loop's 10,000 calls to sumChunk()
# show a real, visible interpreted warm-up phase before promotion.
PROMOTION_THRESHOLD = 1000

COLD_TRIALS = 1  # cold-interpreted is ~30-50s/run; one trial keeps this
                 # script's own runtime reasonable, and it's CPU-bound
                 # single-threaded work with negligible run-to-run variance.
FAST_TRIALS = 5  # tiered and Python are both sub-second; median of 5.

BENCHMARKS = [
    {
        "key": "fib",
        "title": "fib(30) recursive",
        "description": "naive recursive Fibonacci, n=30 (~2.7M calls)",
        "program": BENCH_DIR / "programs" / "fib.mlang",
    },
    {
        "key": "sum",
        "title": "sum_loop (10,000 x 1,000 = 10M iterations)",
        "description": "10,000 calls to a function looping 1,000 iterations each",
        "program": BENCH_DIR / "programs" / "sum_loop.mlang",
    },
]


def time_run(cmd):
    """Runs `cmd` once, returning (wall_seconds, stdout). Raises on nonzero exit."""
    start = time.perf_counter()
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    elapsed = time.perf_counter() - start
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(cmd)}\n{result.stderr}")
    return elapsed, result.stdout.strip()


def strip_promotion_trace(text):
    """--trace-promotions interleaves "<fn> promoted to native code after N
    calls" lines into stdout alongside the program's own output. Strip them
    before comparing tiered output against the cold-interpreted/Python
    output for the correctness cross-check -- the trace line is expected
    and desired, just not part of the program's actual output.
    """
    return "\n".join(line for line in text.splitlines()
                     if "promoted to native code after" not in line)


def time_median(cmd, trials):
    times = []
    output = None
    for _ in range(trials):
        elapsed, out = time_run(cmd)
        times.append(elapsed)
        if output is None:
            output = out
        elif out != output:
            raise RuntimeError(f"non-deterministic output from {' '.join(cmd)}: "
                               f"{output!r} vs {out!r}")
    return statistics.median(times), output


def run_benchmark(mlang, bench):
    program = str(bench["program"])
    cold_time, cold_out = time_median([mlang, "--interpret", program], COLD_TRIALS)
    tiered_time, tiered_out = time_median(
        [mlang, "--trace-promotions", program, str(PROMOTION_THRESHOLD)], FAST_TRIALS)
    python_time, python_out = time_median(
        [sys.executable, str(BENCH_DIR / "reference.py"), bench["key"]], FAST_TRIALS)

    tiered_program_out = strip_promotion_trace(tiered_out)
    if not (cold_out == tiered_program_out == python_out):
        raise RuntimeError(
            f"{bench['title']}: outputs disagree across tiers -- "
            f"cold={cold_out!r} tiered={tiered_program_out!r} python={python_out!r}")

    return {
        **bench,
        "output": cold_out,
        "cold_time": cold_time,
        "tiered_time": tiered_time,
        "python_time": python_time,
        "tiered_speedup_vs_cold": cold_time / tiered_time,
        "tiered_speedup_vs_python": python_time / tiered_time,
        "cold_slowdown_vs_python": cold_time / python_time,
    }


def fmt_seconds(s):
    return f"{s:.4f}s" if s < 10 else f"{s:.2f}s"


def detect_llvm_version():
    candidates = ["llvm-config-18", "llvm-config"]
    try:
        brew = subprocess.run(["brew", "--prefix", "llvm@18"], capture_output=True, text=True)
        if brew.returncode == 0:
            candidates.append(f"{brew.stdout.strip()}/bin/llvm-config")
    except FileNotFoundError:
        pass
    for candidate in candidates:
        try:
            result = subprocess.run([candidate, "--version"], capture_output=True, text=True)
        except FileNotFoundError:
            continue
        if result.returncode == 0:
            return result.stdout.strip()
    return "unknown"


def render_results_md(results, mlang, generated_at):
    llvm_version = detect_llvm_version()

    lines = []
    lines.append("# Benchmark results")
    lines.append("")
    lines.append("Generated by `bench/run.py` -- do not hand-edit; run `./bench/run.sh` to "
                 "regenerate.")
    lines.append("")
    lines.append(f"- Generated: {generated_at}")
    lines.append(f"- Machine: {platform.platform()}, {platform.processor() or platform.machine()}")
    lines.append(f"- Python: {platform.python_version()}")
    lines.append(f"- LLVM: {llvm_version}")
    lines.append(f"- Promotion threshold: **{PROMOTION_THRESHOLD}** calls "
                 "(see PRD.md M8 -- this is a tuning knob, not a fixed constant; "
                 "too low and promotion overhead dominates, too high and a "
                 "function never gets compiled)")
    lines.append(f"- Trials: {COLD_TRIALS} for cold-interpreted (slow, low variance), "
                 f"{FAST_TRIALS} for tiered and Python (fast; median reported)")
    lines.append("")
    lines.append("| Benchmark | cold-interpreted | tiered (this project) | Python 3 | "
                 "tiered vs. cold | tiered vs. Python |")
    lines.append("|---|---|---|---|---|---|")
    for r in results:
        lines.append(
            f"| {r['title']} | {fmt_seconds(r['cold_time'])} | {fmt_seconds(r['tiered_time'])} "
            f"| {fmt_seconds(r['python_time'])} | {r['tiered_speedup_vs_cold']:.1f}x faster "
            f"| {r['tiered_speedup_vs_python']:.1f}x "
            f"{'faster' if r['tiered_speedup_vs_python'] >= 1 else 'slower'} |")
    lines.append("")
    lines.append("## Notes")
    lines.append("")
    for r in results:
        lines.append(f"- **{r['title']}**: {r['description']}. Output: `{r['output']}`. "
                     f"cold-interpreted is {r['cold_slowdown_vs_python']:.1f}x slower than "
                     "Python -- the tree-walking interpreter alone is not competitive; "
                     "the JIT tier is what makes this project's numbers interesting.")
    lines.append("")
    lines.append("Each program's output is cross-checked identical across cold-interpreted, "
                 "tiered, and Python before its time is recorded (see this script's "
                 "run_benchmark()).")
    lines.append("")
    return "\n".join(lines)


def render_readme_section(results, generated_at):
    lines = []
    lines.append(f"Numbers below are from the most recent `./bench/run.sh` "
                 f"(generated {generated_at}; promotion threshold "
                 f"**{PROMOTION_THRESHOLD}** calls). Full methodology and machine "
                 "info in [`bench/results.md`](bench/results.md).")
    lines.append("")
    lines.append("| Benchmark | cold-interpreted | tiered (this project) | Python 3 | "
                 "tiered vs. cold |")
    lines.append("|---|---|---|---|---|")
    for r in results:
        lines.append(
            f"| {r['title']} | {fmt_seconds(r['cold_time'])} | {fmt_seconds(r['tiered_time'])} "
            f"| {fmt_seconds(r['python_time'])} | {r['tiered_speedup_vs_cold']:.1f}x faster |")
    return "\n".join(lines)


def update_readme(section_text):
    readme = REPO_ROOT / "README.md"
    text = readme.read_text()
    start_marker = "<!-- BENCHMARKS:START -->"
    end_marker = "<!-- BENCHMARKS:END -->"
    start = text.find(start_marker)
    end = text.find(end_marker)
    if start == -1 or end == -1:
        raise RuntimeError(f"README.md is missing {start_marker}/{end_marker} markers")
    new_text = (text[:start + len(start_marker)] + "\n\n" + section_text + "\n\n" +
               text[end:])
    readme.write_text(new_text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mlang", default=str(REPO_ROOT / "build" / "mlang"),
                        help="path to the mlang binary")
    args = parser.parse_args()

    mlang = args.mlang
    if not Path(mlang).is_file():
        sys.exit(f"error: mlang binary not found at {mlang} -- build it first "
                 "(see CLAUDE.md's Build & test commands)")

    generated_at = datetime.datetime.now().astimezone().strftime("%Y-%m-%d %H:%M %Z")

    results = []
    for bench in BENCHMARKS:
        print(f"running {bench['title']}...", file=sys.stderr)
        results.append(run_benchmark(mlang, bench))

    results_md = render_results_md(results, mlang, generated_at)
    (BENCH_DIR / "results.md").write_text(results_md)
    update_readme(render_readme_section(results, generated_at))

    print(results_md)


if __name__ == "__main__":
    main()
