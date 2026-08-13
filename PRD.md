# PRD: llvm-mini-lang — A Small Compiled Language with an LLVM JIT

## 1. Summary

llvm-mini-lang is a small, statically-typed language that compiles to native machine
code via LLVM, with a JIT-executed REPL. The project's purpose is to build and
demonstrate a complete, correct, benchmarked compiler pipeline — lexer, parser,
type checker, LLVM IR codegen, and ORC JIT execution — as a portfolio-grade
systems project, in the tradition of small "toy but real" language
implementations (Kaleidoscope, but taken further: static types, a real test
suite, and honest before/after benchmarks against both a tree-walking
interpreter and Python).

This is not a product with external users. The "customer" is the project
itself: a rigorous, well-tested, well-documented, incrementally-built compiler
that can be explained and defended in a technical interview.

## 2. Motivation

Most student systems projects stop at "an interpreter." A compiler that emits
real LLVM IR and JITs it to native code touches nearly every core CS
fundamental at once — parsing theory, type systems, SSA-form IR, register
allocation (handled by LLVM, but understanding *why* it's needed matters),
and machine code execution — and is rarely attempted end-to-end by students,
which is exactly why it's worth doing properly instead of half-finishing it.
The secondary motivation is direct: understanding how LLVM actually works
(rather than treating `clang`/`rustc` as black boxes) is broadly useful,
including for low-latency/HFT-adjacent work where "how does my code actually
get to machine instructions" stops being a rhetorical question.

## 3. Goals

- A complete, working pipeline: source text → tokens → AST → type-checked AST
  → LLVM IR → JIT-executed native code, with a REPL.
- Static typing (int, float, bool, at minimum), including type-checking
  errors reported with source locations.
- Functions with parameters, return values, and recursion (needed for the
  Fibonacci benchmark to mean anything).
- A tree-walking interpreter built first, used as (a) a fast feedback loop
  during language design and (b) a correctness oracle every codegen test is
  checked against.
- Real, scripted, reproducible benchmarks: JIT-compiled llvm-mini-lang vs. the
  tree-walking interpreter vs. equivalent Python, on at least two workloads
  (naive recursive Fibonacci, a tight numeric loop).
- A commit history that reads as an honest incremental build, per `CLAUDE.md`.

## 4. Non-goals (explicitly out of scope for v1)

- Garbage collection / heap-allocated data structures (arrays, strings-as-
  objects, structs) — v1 is scalars (int, float, bool) and functions only.
  A stretch milestone (M7.5) may add fixed-size arrays if time allows; GC is
  out of scope entirely.
- Standalone `.o`/executable emission (AOT compilation) — v1 targets the JIT
  only. Emitting object files via LLVM's `TargetMachine` is a natural v2
  addition, not required for v1.
- A package/module system, standard library beyond a handful of built-in
  functions (`print`, basic math).
- IDE tooling (LSP, syntax highlighting extension) — nice-to-have, not
  required.
- Optimizing beyond what LLVM's stock `-O2` pass pipeline gives for free.
  Writing custom LLVM passes is an explicit stretch goal, not core scope.

## 5. Language spec (v1)

### 5.1 Types
`int` (i64), `float` (double), `bool`. No implicit coercion between int and
float — explicit cast syntax (`as`) required.

### 5.2 Syntax sketch
```
fn fib(n: int) -> int {
    if n < 2 {
        return n;
    } else {
        return fib(n - 1) + fib(n - 2);
    }
}

fn sum_to(n: int) -> int {
    let total: int = 0;
    let i: int = 0;
    while i < n {
        total = total + i;
        i = i + 1;
    }
    return total;
}

fn main() -> int {
    let x: int = 10;
    let result: int = fib(x);
    print(result);
    return 0;
}
```

### 5.3 Grammar (informal, EBNF-ish — finalized during M2)
```
program     := function*
function    := "fn" IDENT "(" params? ")" "->" type block
params      := param ("," param)*
param       := IDENT ":" type
type        := "int" | "float" | "bool"
block       := "{" stmt* "}"
stmt        := let_stmt | assign_stmt | return_stmt | if_stmt | while_stmt | expr_stmt
let_stmt    := "let" IDENT ":" type "=" expr ";"
assign_stmt := IDENT "=" expr ";"
return_stmt := "return" expr? ";"
if_stmt     := "if" expr block ("else" block)?
while_stmt  := "while" expr block
expr_stmt   := expr ";"
expr        := ... (standard precedence climbing: or, and, equality,
                     comparison, additive, multiplicative, unary, call, primary)
```

`assign_stmt` was added during M2: the original grammar had `let` for
declaration but nothing for reassignment, so a `while` loop had no way to
make progress toward its own exit condition (short of a side-effecting
call). Disambiguated from `expr_stmt` by one token of lookahead — `IDENT`
immediately followed by `=` (not `==`) is an assignment, everything else
falls through to `expr_stmt`. `assign_stmt` is statement-only in v1 (not a
chainable expression) and only targets a bare local variable — no
`a.b = c`-style targets, since v1 has no compound/heap types (sec. 4).

## 6. Architecture

```
source (.ml)
   │
   ▼
 Lexer            hand-written, no external lexer generator
   │  tokens
   ▼
 Parser           recursive-descent + precedence climbing for expressions
   │  AST
   ▼
 Sema             type checking, scope resolution, error reporting w/ spans
   │  typed AST
   ├──────────────► Interpreter   (tree-walking; oracle + fast dev loop)
   │  typed AST
   ▼
 Codegen           walks typed AST, emits LLVM IR via IRBuilder
   │  LLVM Module
   ▼
 JIT (ORC)         LLJIT, resolves symbols, executes `main`
```

## 7. Milestones

Each milestone below is sized to be its own multi-commit unit of work per
`CLAUDE.md`'s commit discipline. Do not start milestone N+1 before N's tests
are green and its demo command works from a fresh clone.

### M0 — Repo scaffold
CMake project skeleton, directory layout, `.clang-format`, CI workflow (build +
`ctest` on push), MIT/Apache-2.0 LICENSE, empty `README.md` with project
description and a "status: in progress" badge, `CLAUDE.md` and this `PRD.md`
committed, `code-review-graph` installed and graph built.
**Demo:** `cmake --build build` succeeds on a clean checkout; CI badge green.

### M1 — Lexer
Hand-written lexer: identifiers, keywords, int/float literals, string
literals (for `print`), operators, punctuation, comments, line/column
tracking for error messages.
**Demo:** `mlang --dump-tokens examples/fib.mlang` prints a token stream.

### M2 — Parser → AST
Recursive-descent parser producing a typed-but-unchecked AST: functions,
`let`, assignment, `if`/`else`, `while`, `return`, expression statements,
full expression grammar with correct precedence/associativity. Parser error
recovery (reports multiple syntax errors per file where reasonable, not
just the first).
**Demo:** `mlang --dump-ast examples/fib.mlang` prints the AST (s-expression
or JSON form).

### M3 — Tree-walking interpreter
Direct AST evaluation: variable environments/scopes, function calls
(recursive), control flow, arithmetic/comparison/logical operators. This is
the correctness oracle for every later codegen test.
**Demo:** `mlang --interpret examples/fib.mlang` runs and prints correct
output for all `examples/*.mlang` programs.

### M4 — Semantic analysis (type checking)
Symbol table with scoping, type inference for `let` where annotated,
type-checking of expressions/statements/function signatures/return paths,
clear diagnostics with source spans (`error: expected int, found bool at
fib.mlang:4:12`). Reject the program before it ever reaches codegen if it's
ill-typed.
**Demo:** a suite of intentionally-broken `.mlang` files in
`tests/golden/errors/` each produce the expected diagnostic.

### M5 — LLVM IR codegen
Walk the typed AST, emit LLVM IR via `IRBuilder`: function definitions,
arithmetic/comparison ops, control flow via basic blocks and branches,
`alloca`+`load`/`store` for locals (mem2reg will clean this up), function
calls. Verify every emitted module with `llvm::verifyModule`.
**Demo:** `mlang --emit-llvm examples/fib.mlang` prints valid `.ll` text that
passes the LLVM verifier.

### M6 — JIT execution + REPL
Wire up `LLJIT` (LLVM ORC), resolve and call `main`, return its exit code
from the `mlang` binary. Build a REPL mode that JITs and executes each typed
top-level statement/expression interactively.
**Demo:** `./mlang examples/fib.mlang` runs and exits with the right code;
`./mlang` with no args drops into a working REPL.

### M7 — Functions polish: recursion, multiple params, correctness suite
Harden function calls (multiple params, recursion depth, correct calling
convention), expand `tests/golden/` to cover every language feature combined
(nested control flow, recursive + mutually-shaped calls), cross-check every
golden test's output between interpreter and JIT paths (must match exactly).
**Demo:** `ctest` green across unit + golden + interpreter/JIT cross-check
suites.

### M8 — Benchmarks
Scripted benchmark harness in `bench/`: naive recursive Fibonacci (n=30+) and
a tight numeric loop (e.g. sum 10M iterations), each run as (a) JIT-compiled
llvm-mini-lang, (b) tree-walking llvm-mini-lang interpreter, (c) equivalent Python script.
Report wall-clock time and computed speedup ratios. Numbers go into
`bench/results.md` and get pulled into the README, generated by the script,
not hand-edited.
**Demo:** `./bench/run.sh` regenerates `bench/results.md` from scratch,
reproducibly.

### M9 — Docs & polish
Full language reference in `docs/`, architecture diagram (pipeline stages),
`README.md` rewritten with the benchmark numbers, quickstart, and example
programs front and center. Tag `v1.0.0`.
**Demo:** a stranger can clone the repo, follow the README, build it, run
the REPL, and run the benchmarks with no undocumented steps.

### Stretch (optional, only after M9)
- M10: AOT `.o` emission via `TargetMachine` + a tiny runtime, producing a
  real standalone executable (not just JIT).
- M11: Fixed-size arrays / a minimal struct type.
- M12: A custom LLVM optimization pass (e.g. strength-reduction on the
  Fibonacci-style recursive pattern) with before/after IR + benchmark deltas.

## 8. Success metrics

- All milestones M0–M9 complete, each independently demoable per its "Demo"
  line above.
- `ctest` fully green in CI on every commit to `main` from M1 onward.
- JIT-compiled llvm-mini-lang shows a measurable, reproducible speedup over both the
  tree-walking interpreter and Python on the M8 benchmark suite (order-of-
  magnitude speedup is the expected/interesting result, not a hard
  requirement — the honesty of the number matters more than its size).
- Commit history (per `CLAUDE.md`) reads as a genuine incremental build: no
  milestone lands as a single commit; `git log --oneline --graph` tells a
  believable story from empty repo to v1.0.0 tag.

## 9. Risks

| Risk | Mitigation |
|---|---|
| LLVM API churn / version mismatch on different dev machines | Pin an LLVM version in `README.md` + CI (e.g. LLVM 18), document install steps per OS |
| Scope creep into GC / modules / stdlib | Non-goals section above is the guardrail — revisit only after M9 tag |
| Parser edge cases (operator precedence bugs) eating time | Precedence-climbing tests written exhaustively in M2 before moving to M3 |
| Benchmark numbers looking cherry-picked | Benchmarks are scripted and regenerate `results.md` automatically (M8) — no manual editing of numbers |

## 10. Open questions (resolve during M2, not before)

- Exact error-message format/style for the type checker (M4) — decide once
  a few real error cases exist, not speculatively now.
- Whether `bool` short-circuits `and`/`or` at the AST level or codegen level
  — decide during M5 based on what's cleanest in LLVM IR (likely: codegen
  emits actual branches for short-circuit, not just an `and`/`or` instruction).
