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
  A stretch milestone (see the Stretch section under §7) may add fixed-size
  arrays if time allows; GC is out of scope entirely.
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

Compilation front-loads exactly as before (source → tokens → AST → typed
AST) — that part of the pipeline is untouched by the M6+ pivot below.
What changed is *how a typed AST gets executed*: it's no longer "compile
the whole program once, then run it." Execution is decided **per function,
at runtime**, through a dispatch table that starts every function pointed
at the interpreter and redirects individual functions to compiled native
code once they run hot — the same tree-walking Interpreter and the same
IRBuilder-based Codegen from M3/M5 are both still here, just invoked
adaptively instead of as two alternate top-level modes:

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
   ▼
 Dispatch table    one entry per function; every entry starts bound to
                    that function's interpreter trampoline (M6)
   │
   │   every call is resolved through this table, by function name
   ▼
 Interpreter        tree-walking (M3); increments the callee's call
 — cold path        counter on every invocation
   │
   │   counter crosses promotion threshold N (M7)
   ▼
 Codegen             walks the SAME typed AST --emit-llvm already uses
 (M5, unchanged)      (M5), emits LLVM IR via IRBuilder
   │  LLVM Module
   ▼
 JIT (ORC)           LLJIT compiles the function to native code; the
                      dispatch table entry is patched in place, live,
                      without restarting execution or disturbing calls
                      already in flight
   │
   ▼
 native code         — hot path — every future call to this function,
                       including its own recursive self-calls, resolves
                       through the same patched dispatch entry directly;
                       the interpreter is never re-entered for it
```

## 7. Milestones

Each milestone below is sized to be its own multi-commit unit of work per
`CLAUDE.md`'s commit discipline. Do not start milestone N+1 before N's tests
are green and its demo command works from a fresh clone.

### M0 — Repo scaffold
**Status: ✅ Complete.**
CMake project skeleton, directory layout, `.clang-format`, CI workflow (build +
`ctest` on push), MIT/Apache-2.0 LICENSE, empty `README.md` with project
description and a "status: in progress" badge, `CLAUDE.md` and this `PRD.md`
committed, `code-review-graph` installed and graph built.
**Demo:** `cmake --build build` succeeds on a clean checkout; CI badge green.

### M1 — Lexer
**Status: ✅ Complete.**
Hand-written lexer: identifiers, keywords, int/float literals, string
literals (for `print`), operators, punctuation, comments, line/column
tracking for error messages.
**Demo:** `mlang --dump-tokens examples/fib.mlang` prints a token stream.

### M2 — Parser → AST
**Status: ✅ Complete.**
Recursive-descent parser producing a typed-but-unchecked AST: functions,
`let`, assignment, `if`/`else`, `while`, `return`, expression statements,
full expression grammar with correct precedence/associativity. Parser error
recovery (reports multiple syntax errors per file where reasonable, not
just the first).
**Demo:** `mlang --dump-ast examples/fib.mlang` prints the AST (s-expression
or JSON form).

### M3 — Tree-walking interpreter
**Status: ✅ Complete.**
Direct AST evaluation: variable environments/scopes, function calls
(recursive), control flow, arithmetic/comparison/logical operators. This is
the correctness oracle for every later codegen test.
**Demo:** `mlang --interpret examples/fib.mlang` runs and prints correct
output for all `examples/*.mlang` programs.

### M4 — Semantic analysis (type checking)
**Status: ✅ Complete.**
Symbol table with scoping, type inference for `let` where annotated,
type-checking of expressions/statements/function signatures/return paths,
clear diagnostics with source spans (`error: expected int, found bool at
fib.mlang:4:12`). Reject the program before it ever reaches codegen if it's
ill-typed.
**Demo:** a suite of intentionally-broken `.mlang` files in
`tests/golden/errors/` each produce the expected diagnostic.

### M5 — LLVM IR codegen
**Status: ✅ Complete.**
Walk the typed AST, emit LLVM IR via `IRBuilder`: function definitions,
arithmetic/comparison ops, control flow via basic blocks and branches,
`alloca`+`load`/`store` for locals (mem2reg will clean this up), function
calls. Verify every emitted module with `llvm::verifyModule`.
**Demo:** `mlang --emit-llvm examples/fib.mlang` prints valid `.ll` text that
passes the LLVM verifier.

**M6 onward is a deliberate pivot away from the original one-shot-JIT plan.**
The original M6–M9 compiled a whole program once via LLVM and ran the
result (the Kaleidoscope-tutorial shape). Instead, execution becomes
**tiered**, the way V8, HotSpot, and LuaJIT actually work: every function
starts running through the M3 interpreter; a function that's called often
gets promoted, live and mid-program, to JIT-compiled native code via the
existing M5 codegen path. M0–M5 (front end through one-shot codegen) are
unchanged and are exactly what the tiered model builds on — the pivot is
about *when and how a typed AST gets executed*, not about redoing the
front end. See §6 for the updated architecture.

### M6 — Profiling + dispatch layer
**Status: ✅ Complete.**
Add call counters to the interpreter (one per function, incremented on
every invocation) and an indirect call-dispatch table — one entry per
function, keyed by name. Every entry initially points at that function's
**interpreter trampoline** (a stub that just invokes the M3 tree-walking
interpreter for it). No promotion logic yet: the only goal of this
milestone is proving a dispatch entry can be redirected to a different
target without the caller's code changing, since M7 depends on that
mechanism working correctly first.
**Demo:** a dev-facing flag (e.g. `--trace-calls`) that runs a program and
prints each function's call count, plus a unit test that manually patches
one function's dispatch entry mid-run and shows subsequent calls resolve
through the new target.

### M7 — Hot-swap promotion
**Status: ✅ Complete.**
Threshold-triggered promotion: once a function's call count crosses a
configurable threshold N, that function is run through the existing M5
codegen path, JIT-compiled via ORC, and its dispatch table entry is patched
to the compiled function pointer — live, mid-program, without restarting
execution or disturbing calls already in flight. All *future* calls to that
function, including its own recursive self-calls, are redirected to the
compiled path; the interpreter is never re-entered for it afterward.

The key edge case to get right: **a recursive function may still have
interpreted stack frames calling itself at the exact moment it crosses the
threshold** (e.g. `fib(10)` promotes while `fib(7)`, `fib(4)`, ... are still
executing on the interpreter's call stack). Promotion must not corrupt or
double-count those in-flight calls — the frames already running finish
however they started, and it's only the *next* call through the dispatch
table that takes the newly-compiled path. Cross-check every golden test's
output between (a) pure-interpreted, (b) mid-promotion, and (c) fully-JIT
runs — they must match exactly (mirrors the old M7's interpreter/JIT
cross-check goal, extended to the promotion boundary itself).
**Demo:** running a recursive fib program with a low promotion threshold
and a `--trace-promotions` flag prints "fib promoted to native code after N
calls" mid-run, and the final result is identical to a pure-interpreted run
of the same program.

### M8 — Benchmarks with a warm-up story
Scripted, reproducible benchmark harness in `bench/`, comparing (a)
cold-interpreted llvm-mini-lang, (b) tiered llvm-mini-lang (interpret, then
promote once hot — the real end-to-end story this project is built around),
and (c) equivalent Python, on both a recursive workload (naive Fibonacci,
n=30+) and an iterative workload (e.g. sum 10M iterations). Report
wall-clock time, computed speedup ratios, *and* the promotion threshold
value used for the run — the threshold is a real tuning knob that shapes
the numbers (too low and promotion overhead dominates small workloads; too
high and short-lived functions never get compiled), so it's reported
alongside the results, not buried as an implementation detail. Numbers go
into `bench/results.md` and get pulled into the README, generated by the
script, not hand-edited.
**Demo:** `./bench/run.sh` regenerates `bench/results.md` from scratch,
reproducibly, including the promotion threshold used for each run.

### M9 — REPL, docs & polish
Build a REPL mode that runs each typed top-level statement/expression
through the same tiered dispatch path as a file run (it's a thin CLI loop
around the M7 execution engine, not a separate execution mode). Full
language reference in `docs/`, architecture diagram matching §6's
dispatch-table model, and `README.md` rewritten with the benchmark numbers,
quickstart, and example programs front and center. The README's core pitch
is reframed around *why* tiered execution is a meaningfully different
design than compiling everything up front — naming V8, HotSpot, and LuaJIT
by name as the production systems this mirrors, rather than presenting the
project as another compile-then-run demo. Tag `v1.0.0`.
**Demo:** a stranger can clone the repo, follow the README, build it, run
the REPL, and run the benchmarks with no undocumented steps.

### Stretch (optional, only after M9)
- M10 (highest-leverage stretch goal): **background/async compilation** —
  once a function crosses the promotion threshold, hand it to LLVM's
  codegen+JIT pipeline on a worker thread instead of compiling
  synchronously on the call path that triggered promotion. The triggering
  call (and every call until compilation finishes) keeps running
  interpreted, and the dispatch entry only patches over once the compiled
  code is actually ready. This is the difference between "promotion causes
  a visible stutter" and "hot paths never notice compilation happening at
  all," which is the real reason production tiered JITs compile off the
  hot path.
- M11: AOT `.o` emission via `TargetMachine` + a tiny runtime, producing a
  real standalone executable (not just JIT) — this would compile every
  function up front rather than tier, so it's a genuinely separate
  execution mode from the rest of v1, not an extension of the dispatch
  table.
- M12: Fixed-size arrays / a minimal struct type.
- M13: A custom LLVM optimization pass (e.g. strength-reduction on the
  Fibonacci-style recursive pattern) with before/after IR + benchmark deltas.

## 8. Success metrics

- All milestones M0–M9 complete, each independently demoable per its "Demo"
  line above.
- `ctest` fully green in CI on every commit to `main` from M1 onward.
- **Tiered-execution correctness**: a program's output and exit code are
  byte-identical no matter which of its functions happen to be
  interpreted, mid-promotion, or fully JIT-compiled at the moment they're
  called. This supersedes the old "interpreter and JIT output match"
  bar — promotion has to be provably invisible to a caller at *every*
  point in a function's lifecycle, not just checked once at the two
  static endpoints (never-promoted vs. fully-promoted). The M7
  in-flight-recursion edge case (§7) is the sharpest version of this
  property and gets its own test coverage, not just incidental coverage
  from the golden suite.
- Tiered llvm-mini-lang (interpret cold, JIT hot per M7) shows a
  measurable, reproducible speedup over both cold-interpreted
  llvm-mini-lang and Python on the M8 benchmark suite, and the report
  states the promotion threshold used to produce the numbers (order-of-
  magnitude speedup is the expected/interesting result, not a hard
  requirement — the honesty of the number, including the threshold that
  shaped it, matters more than its size).
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
