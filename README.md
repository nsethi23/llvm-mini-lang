# llvm-mini-lang

[![CI](https://github.com/nsethi23/llvm-mini-lang/actions/workflows/ci.yml/badge.svg)](https://github.com/nsethi23/llvm-mini-lang/actions/workflows/ci.yml)

llvm-mini-lang is a small, statically-typed language with a **tiered execution
engine**: every function starts running on a tree-walking interpreter, and a
function that gets called enough times is promoted -- live, mid-program, via
LLVM's ORC JIT -- to native machine code. That's not a compile-then-run demo;
it's the same shape as V8 (Ignition → Sparkplug → TurboFan), HotSpot (C1 → C2),
and LuaJIT (interpret, then trace hot loops into compiled code), scaled down
to a project one person can actually finish and explain end to end. See
[`docs/architecture.md`](docs/architecture.md) for the full pipeline and why
tiering is a meaningfully different design than a Kaleidoscope-style one-shot
compiler.

**Status: v1.0.0 -- all of M0–M9 complete** (lexer → parser → sema →
tree-walking interpreter → LLVM codegen → hot-swap JIT promotion →
benchmarks → REPL). See [`PRD.md`](PRD.md) for the full milestone breakdown
and [`CLAUDE.md`](CLAUDE.md) for how this repo was built (one milestone at a
time, small atomic commits -- `git log --oneline` reads as a real build log).

## Quickstart

```bash
git clone https://github.com/nsethi23/llvm-mini-lang.git
cd llvm-mini-lang
brew install llvm@18 cmake ninja                     # see Prerequisites below for Linux
cmake -S . -B build -DCMAKE_PREFIX_PATH=$(brew --prefix llvm@18)
cmake --build build -j
./build/mlang                                         # launch the REPL
```

Inside the REPL:

```
>>> fn fib(n: int) -> int {
...     if n < 2 { return n; } else { return fib(n - 1) + fib(n - 2); }
... }
defined fn fib(n: int) -> int
>>> fib(25)
fib promoted to native code after 10 calls
75025
```

That promotion line isn't decoration -- `fib` really did just get compiled to
native code mid-session and the dispatch table really did get repointed at
it, live, while the REPL kept running. Run `./bench/run.sh` to see what that
buys: a full comparison against cold-interpreted mlang and Python.

## Example programs

```
// examples/fib.mlang
fn fib(n: int) -> int {
    if n < 2 {
        return n;
    } else {
        return fib(n - 1) + fib(n - 2);
    }
}

fn main() -> int {
    let x: int = 10;
    let result: int = fib(x);
    print(result);
    return 0;
}
```

```bash
./build/mlang --interpret examples/fib.mlang     # tree-walked
./build/mlang --trace-promotions examples/fib.mlang 5   # promotes mid-run
```

More in [`examples/`](examples/): `loop_sum.mlang` (iterative control flow)
and `casts_and_bools.mlang` (`as` casts, short-circuit `&&`/`||`). Full
language spec in [`docs/language-reference.md`](docs/language-reference.md).

## Build

### Prerequisites

This project targets **LLVM 18**. Install steps per OS:

**macOS (Homebrew):**

```bash
brew install llvm@18 cmake ninja

# llvm@18 is keg-only (not symlinked into /opt/homebrew), so point CMake at it
# explicitly when configuring (see below).
```

**Linux (Ubuntu 24.04 / Debian, matches what CI uses):**

```bash
sudo apt-get update
sudo apt-get install -y llvm-18-dev libllvm18 cmake ninja-build
```

For other distros without LLVM 18 in the default repos, use the official
[LLVM apt repository](https://apt.llvm.org) or build LLVM 18 from source.

### Configure + build

```bash
# macOS: point CMake at the keg-only LLVM install
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=$(brew --prefix llvm@18)

# Linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=$(llvm-config-18 --cmakedir)

cmake --build build -j
```

### Run tests

```bash
ctest --test-dir build --output-on-failure

# golden tests only (tests/golden/*.mlang cross-checked against the interpreter)
./build/tests/golden_runner tests/golden/

# sema error golden tests (tests/golden/errors/*.mlang, each expected to
# produce a specific diagnostic)
./build/tests/sema_error_runner tests/golden/errors/

# codegen golden tests (every well-typed tests/golden/*.mlang program must
# codegen to a module that passes llvm::verifyModule)
./build/tests/codegen_verify_runner tests/golden/

# promotion cross-check: every golden program must produce identical
# output whether pure-interpreted, promoted mid-run, or promoted on its
# first call
./build/tests/promotion_cross_check_runner tests/golden/
```

## CLI reference

```bash
./build/mlang --dump-tokens <file>              # token stream
./build/mlang --dump-ast <file>                 # s-expression AST
./build/mlang --check <file>                    # type-check only
./build/mlang --interpret <file>                # type-check + tree-walking interpreter
./build/mlang --emit-llvm <file>                # type-check + LLVM IR, verified
./build/mlang --trace-calls <file>               # interpret + print each function's call count
./build/mlang --trace-promotions <file> [threshold]   # interpret with hot-swap promotion enabled
./build/mlang                                    # REPL (same pipeline, one statement at a time)
```

`--dump-tokens` prints the token stream (kind, lexeme, `line:column`).
`--dump-ast` lexes, parses, and prints the AST as an s-expression; on a
syntax error it prints `file:line:col: error: ...` diagnostics instead (one
per broken function, not just the first) and exits non-zero. `--check`
additionally runs sema (scope resolution, type checking of every
expression/statement/function signature, and an all-paths-return check on
every function body) and prints the same `file:line:col: error: ...` style
diagnostics for any type error, without running the program. `--interpret`
runs the same checks and, if the program is well-typed, tree-walks it,
exiting with `main`'s return value -- this interpreter is also the
correctness oracle every JIT result is checked against. `--emit-llvm` runs
the same checks and, if the program is well-typed, walks the AST via
`IRBuilder` to emit LLVM IR -- function definitions, arithmetic/comparison
ops, `alloca`-based locals, `if`/`while` control flow via basic blocks,
function calls, and `print()` lowered to a runtime helper -- verifies the
module with `llvm::verifyModule`, and prints the resulting `.ll` text.
`--trace-calls` interprets the program through the per-function dispatch
table and prints each function's call count once it finishes -- the
mechanism `--trace-promotions` builds on. `--trace-promotions <file>
[threshold]` (default threshold 10) interprets with hot-swap promotion
enabled: once a function's call count crosses the threshold, it's compiled
through a real ORC `LLJIT` and its dispatch entry is redirected to the
compiled code live, mid-program -- printing `"<fn> promoted to native code
after N calls"` the moment it happens, including while that function's own
earlier recursive calls are still unwinding on the interpreter's call
stack. Running `mlang` with **no arguments** launches an interactive REPL
over the same pipeline -- see [`docs/architecture.md`](docs/architecture.md)
for the full mechanism, and its "Known limitations" section for what's
deliberately out of scope (no on-stack replacement, synchronous
compilation, no de-optimization).

## Benchmarks

`./bench/run.sh` regenerates the numbers below (and
[`bench/results.md`](bench/results.md)) from scratch, reproducibly: it
builds a Release binary if needed, runs `bench/programs/*.mlang` cold-
interpreted, tiered (with hot-swap promotion enabled), and the equivalent
Python 3 program, timing wall-clock and cross-checking every run's output
agrees before recording it.

<!-- BENCHMARKS:START -->

Numbers below are from the most recent `./bench/run.sh` (generated 2026-08-23 22:30 EDT; promotion threshold **1000** calls). Full methodology and machine info in [`bench/results.md`](bench/results.md).

| Benchmark | cold-interpreted | tiered (this project) | Python 3 | tiered vs. cold |
|---|---|---|---|---|
| fib(30) recursive | 25.32s | 0.0218s | 0.0763s | 1160.4x faster |
| sum_loop (10,000 x 1,000 = 10M iterations) | 1.2192s | 0.1587s | 0.2157s | 7.7x faster |

<!-- BENCHMARKS:END -->

The honest part of that comparison: the tree-walking interpreter alone is
*not* competitive with Python (exception-based `return` unwinding on every
call is expensive) -- it's only once the JIT tier kicks in that this project
pulls ahead. That gap is the entire point of building a tiered engine
instead of shipping the interpreter alone.

## Development

- `.clang-format` (LLVM style base) -- run `clang-format -i` on changed files
  before committing.
- Commit discipline, milestone sequencing, and use of the `code-review-graph`
  MCP tool for context-efficient review are documented in `CLAUDE.md`.
- [`docs/language-reference.md`](docs/language-reference.md): full language
  spec (types, grammar, operator precedence, scoping).
- [`docs/architecture.md`](docs/architecture.md): the tiered pipeline, the
  in-flight-recursion correctness edge case, and known limitations.
