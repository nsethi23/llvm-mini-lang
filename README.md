# llvm-mini-lang

[![CI](https://github.com/nsethi23/llvm-mini-lang/actions/workflows/ci.yml/badge.svg)](https://github.com/nsethi23/llvm-mini-lang/actions/workflows/ci.yml)

llvm-mini-lang is a small, statically-typed language that compiles to native machine
code via LLVM, with a JIT-executed REPL. The project's purpose is to build and
demonstrate a complete, correct, benchmarked compiler pipeline — lexer, parser,
type checker, LLVM IR codegen, and ORC JIT execution — as a portfolio-grade
systems project, in the tradition of small "toy but real" language
implementations (Kaleidoscope, but taken further: static types, a real test
suite, and honest before/after benchmarks against both a tree-walking
interpreter and Python).

**Status: M1 (lexer) complete.**

See [`PRD.md`](PRD.md) for the full spec and milestone breakdown, and
[`CLAUDE.md`](CLAUDE.md) for how this repo is built/worked on (one milestone
at a time, small atomic commits).

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
```

### Run the driver

```bash
./build/mlang --dump-tokens examples/fib.mlang
```

Prints the token stream (kind, lexeme, `line:column`) for a `.mlang` source
file. `--dump-ast` lands in M2, a working REPL in M6 — see `PRD.md` for the
full milestone list.

## Development

- `.clang-format` (LLVM style base) — run `clang-format -i` on changed files
  before committing.
- Commit discipline, milestone sequencing, and use of the `code-review-graph`
  MCP tool for context-efficient review are documented in `CLAUDE.md`.
