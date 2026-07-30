#!/usr/bin/env python3
"""Python reference implementations of bench/programs/*.mlang, run as a
subprocess by bench/run.py so its process-startup overhead is measured the
same way mlang's is -- comparing "python3 <script>" wall time against
"mlang <flags> <file>" wall time, not against a warm in-process loop.

Usage: reference.py fib | sum
"""
import sys


def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)


def sum_chunk(n):
    total = 0
    i = 0
    while i < n:
        total += i
        i += 1
    return total


def run_fib():
    print(fib(30))


def run_sum():
    outer_iters = 10000
    chunk_size = 1000
    grand_total = 0
    for _ in range(outer_iters):
        grand_total += sum_chunk(chunk_size)
    print(grand_total)


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in ("fib", "sum"):
        sys.exit("usage: reference.py fib | sum")
    {"fib": run_fib, "sum": run_sum}[sys.argv[1]]()
