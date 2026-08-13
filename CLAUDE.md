# CLAUDE.md

This file is read by Claude Code (and any Claude-based agent) at the start of every
session in this repository. It defines how work should be done here, not just what
the project is. Follow it exactly — it exists to keep this repo looking like it was
built deliberately, one real milestone at a time, not generated in one shot.

## Project summary

**llvm-mini-lang** is a small statically-typed expression/scripting language with a full,
real compiler pipeline: hand-written lexer → recursive-descent parser → AST →
semantic analysis (type checking) → LLVM IR codegen → JIT execution via LLVM ORC.
A tree-walking interpreter is built first as a correctness oracle and a baseline
for benchmarking the JIT speedup.

See `PRD.md` for the full spec, milestone breakdown, and success criteria. Read
that file before starting any work — it is the source of truth for scope and
sequencing. This file (`CLAUDE.md`) governs *how* to work, not *what* to build.

## Non-negotiable working rules

### 1. Work one milestone at a time, in order

`PRD.md` defines milestones M0–M9. Do not skip ahead, do not silently combine
milestones, and do not build M5 (codegen) scaffolding while "just doing" M2
(parser). If a milestone reveals that an earlier one needs rework, stop, say so
explicitly, and fix the earlier milestone in its own commit(s) before proceeding.

### 2. Commit frequently and atomically — this is critical

**Never batch an entire milestone into a single commit, and never implement
multiple milestones before committing.** Each commit should represent one
logically complete, buildable, testable unit of work — roughly the size of "add
the lexer's number/string tokenization" or "add the `if/else` grammar rule and its
parser tests," not "implement the whole parser."

Rules for commits:
- Run the test suite before every commit. Do not commit red tests (except a
  commit whose entire purpose is a failing test written before the fix — a
  legitimate TDD red step — and even then, follow it with a green commit
  immediately after).
- Use [Conventional Commits](https://www.conventionalcommits.org/) style:
  `feat(lexer): tokenize integer and float literals`
  `feat(parser): parse if/else statements`
  `test(parser): add if/else parser test cases`
  `fix(codegen): correct sign extension on i1->i64 cast`
  `docs: add M2 milestone notes to PRD`
  `chore(ci): add clang-format check to workflow`
- Within a single milestone, expect somewhere between 4 and 15 commits, not 1
  and not 60. A milestone landing as one giant commit is a signal you batched
  work — split it before pushing, or if already committed, note it and do
  better on the next milestone.
- Commit messages describe *what changed and why*, not "wip" or "more work."
- Push after each milestone's commits are green, so the commit history itself
  becomes a readable build log of the project.

### 3. Test as you go, not at the end

Every language feature (a new token type, a new grammar rule, a new AST node, a
new codegen case) gets a test in the same commit or the very next one. Use the
tree-walking interpreter's output as the oracle for codegen tests: the same
llvm-mini-lang source should produce the same result whether interpreted or JIT-compiled.
Golden-file tests (`.mlang` source in, expected output/exit-code out) are the
primary test format — keep them in `tests/golden/`.

### 4. Use `code-review-graph` for context efficiency

This repo is configured with [code-review-graph](https://github.com/tirth8205/code-review-graph)
(CRG) via MCP. It builds a structural graph of the codebase (Tree-sitter-based)
so you can query blast radius, callers/callees, and review context instead of
re-reading whole files or the whole repo on every turn.

- At the start of a session, if the graph seems stale or this is the first
  session after cloning, run `build_or_update_graph_tool` (or the CLI:
  `code-review-graph build`).
- Before making a change to an existing file (not greenfield new-file work),
  prefer `get_impact_radius_tool` / `get_review_context_tool` over reading
  unrelated files wholesale — this is the whole point of having it installed:
  don't burn tokens re-reading the parser when you're only touching codegen.
- After a batch of edits, `detect_changes_tool` gives a risk-scored summary —
  use it before writing the commit message so the message reflects actual
  blast radius, not guesswork.
- Do not disable or route around CRG to save a step; the token savings compound
  over a multi-week project like this one.

### 5. Keep milestones demoable

Each milestone should end in something that can actually be run and shown, not
just code that compiles. M1 (lexer) should have a `--dump-tokens` CLI flag. M2
(parser) should have a `--dump-ast` flag. M6 (JIT) should have a working REPL.
If a milestone can't be demoed in under 30 seconds from a fresh clone, it isn't
done — add the CLI plumbing before closing it out.

### 6. Benchmarks are part of the deliverable, not an afterthought

M8 exists specifically to produce real numbers (JIT vs. interpreter vs. Python
on equivalent code, e.g. naive recursive Fibonacci and a tight numeric loop).
Benchmark code lives in `bench/`, is scripted (not manual), and its output is
what gets pasted into the README — not hand-picked/rounded numbers.

## Repository layout

```
mlang/
├── CLAUDE.md                 # this file
├── PRD.md                    # full spec + milestones
├── README.md                 # public-facing, updated as milestones land
├── CMakeLists.txt
├── include/mlang/            # public headers
├── src/
│   ├── lexer/
│   ├── parser/
│   ├── ast/
│   ├── sema/                 # type checking
│   ├── interpreter/          # tree-walking oracle
│   ├── codegen/              # LLVM IR generation
│   └── jit/                  # ORC JIT driver, REPL
├── tests/
│   ├── unit/                 # lexer/parser/sema unit tests (Catch2 or GoogleTest)
│   └── golden/               # .mlang source + expected output pairs
├── bench/                    # benchmark scripts + example programs
├── examples/                 # sample .mlang programs
├── docs/                     # language reference, architecture notes
└── .github/workflows/ci.yml  # build + test on push
```

## Build & test commands

```bash
# configure + build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# run all tests
ctest --test-dir build --output-on-failure

# run golden tests only
./build/tests/golden_runner tests/golden/

# dump tokens / AST for a source file (debugging aids expected from M1/M2 on)
./build/mlang --dump-tokens examples/fib.mlang
./build/mlang --dump-ast examples/fib.mlang

# REPL (available from M6 on)
./build/mlang
```

## Coding standards

- C++20, LLVM's own coding conventions where they don't conflict with the above
  (2-space indent is *not* required — use `.clang-format` checked into the repo;
  run `clang-format` before every commit).
- No raw `new`/`delete` — use smart pointers or LLVM's own memory management
  utilities (e.g. `std::unique_ptr` for AST nodes, LLVM's `IRBuilder` idioms for
  codegen).
- Every public header gets a one-paragraph doc comment explaining its role in
  the pipeline (lexer → parser → sema → codegen → jit).
- Prefer small, single-responsibility `.cpp` files over one large file per
  pipeline stage — makes CRG's blast-radius queries actually useful.

## What "done" looks like for the whole project

By the end of M9, `git log --oneline` should read as a clear, incremental build
log from empty repo to a working JIT-compiled language with benchmarks — the
commit history is itself part of the portfolio artifact, not just the final
code. If you (Claude) are ever about to make a commit that would make the
history look like it was generated in a handful of giant dumps, stop and split
the work instead.
