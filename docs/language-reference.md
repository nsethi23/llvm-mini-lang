# mlang language reference

mlang is a small, statically-typed expression/scripting language. This
document is the full reference for v1.0.0 -- everything the lexer, parser,
and sema accept, and how the interpreter/JIT execute it. For *how* a
program runs (interpreted, promoted, or REPL), see
[`architecture.md`](architecture.md); this file is only about the language
itself.

## Hello, mlang

```
fn main() -> int {
    print("hello, mlang");
    return 0;
}
```

Every program is a flat list of function declarations. Execution starts at
`main`, which must take no parameters and return `int` -- its return value
becomes the process exit code.

## Types

v1 has exactly three value types, plus one literal-only type:

| Type | Description |
|---|---|
| `int` | 64-bit signed integer (`i64`) |
| `float` | 64-bit floating point (`double`) |
| `bool` | `true` / `false` |
| `string` | literal-only -- the only place a string can appear is as `print`'s argument; there is no `string`-typed variable, parameter, return type, or operator |

There is no implicit coercion between `int` and `float` anywhere --
arithmetic, comparisons, `let` initializers, assignments, arguments, and
return values all require an exact type match. Convert explicitly with
`as` (see [Casts](#casts)).

## Declarations

### Functions

```
fn name(param1: type1, param2: type2) -> returnType {
    // body
}
```

- Zero or more comma-separated parameters, each `name: type`.
- A return type is required (no inferred/`void` return).
- Every path through the body must `return` a value of the declared type
  -- sema rejects a function where some path can fall off the end (a
  `while` body is never assumed to run, so a `return` only inside a loop
  never counts as covering the path after it).
- Functions may recurse, including mutual recursion, since every
  function's signature is known before any body is checked (Sema's
  whole-program pass; the REPL relaxes this -- see
  [Differences in the REPL](#differences-in-the-repl)).
- `print` is a reserved built-in name and cannot be redefined.

### `let`

```
let name: type = expr;
```

Declares a new, block-scoped, **mutable** binding. The declared type and
the initializer's type must match exactly (no coercion). Shadowing an
outer binding of the same name is allowed; redeclaring the same name
*within the same block* overwrites the earlier one rather than erroring.

### Assignment

```
name = expr;
```

Reassigns an existing binding (found by searching outward through
enclosing scopes -- the same resolution `let` shadowing uses). Assigning
to a name with no matching `let` anywhere in scope is a sema error. The
value's type must match the variable's declared type exactly.

## Statements

| Statement | Form |
|---|---|
| `let` | `let name: type = expr;` |
| assignment | `name = expr;` |
| `return` | `return expr;` (required inside every function; see [Types](#types) for return-type matching) |
| expression statement | `expr;` -- evaluated for its side effect (only `print(...)` calls are useful here, since there's no other observable side effect) |
| `if` / `if`-`else` | `if cond { ... }` or `if cond { ... } else { ... }` -- `cond` must be `bool` |
| `while` | `while cond { ... }` -- `cond` must be `bool`, re-checked before each iteration |
| block | `{ ... }` -- opens a fresh child scope; a `let` inside is invisible once the block ends |

There is no `for` loop, no `break`/`continue`, and no `else if` chaining
sugar (write nested `if`/`else { if ... }` instead).

## Expressions

### Literals

- Integers: `42`, `0`, `-7` (the `-` is unary negation applied to `7`, not
  part of the literal's lexeme)
- Floats: `3.14`, `0.5` -- a `.` is required; there is no exponent syntax
- Booleans: `true`, `false`
- Strings: `"..."`, double-quoted, with `\"`, `\\`, `\n`, `\t` escapes --
  only valid as `print`'s argument

### Operators, by precedence (lowest to highest)

| Precedence | Operators | Associativity |
|---|---|---|
| 1 (lowest) | `\|\|` | left |
| 2 | `&&` | left |
| 3 | `==`  `!=` | left |
| 4 | `<`  `<=`  `>`  `>=` | left |
| 5 | `+`  `-` (binary) | left |
| 6 | `*`  `/`  `%` | left |
| 7 | unary `-`, unary `!` | right (prefix) |
| 8 | `as` (cast) | left |
| 9 (highest) | function call `f(...)`, parenthesized `(...)` | -- |

- `+` `-` `*` `/` `%` require both operands to be the same type, `int` or
  `float` (no mixed arithmetic). Integer `/` and `%` are truncating and
  raise a runtime error on division by zero (there is no static
  divide-by-zero check -- `1 / n` with a variable `n` is only caught at
  run time if `n` turns out to be `0`).
- `<` `<=` `>` `>=` require both operands to be the same type, `int` or
  `float`; the result is `bool`.
- `==` `!=` work on any matching pair of `int`, `float`, or `bool`
  operands.
- `&&` and `||` require `bool` operands on both sides and **short-circuit**
  -- the right operand is not evaluated when the left already determines
  the result (`false && f()` never calls `f`; `true || f()` never calls
  `f`).
- Unary `-` requires `int` or `float`; unary `!` requires `bool`.

### Casts

```
expr as type
```

Allowed conversions: `int as float`, `float as int` (truncates toward
zero), and a type cast to its own type (a no-op). There is no `bool as
int`/`int as bool` or any cast involving `string`.

### Calls

```
name(arg1, arg2, ...)
```

Argument count and each argument's type must match the callee's
declared parameters exactly. The only built-in function is:

```
print(x)
```

`print` takes exactly one argument of any single type (`int`, `float`,
`bool`, or a string literal) and writes its value followed by a newline.
It returns `int` (`0`), so `print(x);` as a statement is the normal usage,
though `print(x)` is a valid expression like any other call.

Rendering: `int` prints as a plain decimal integer; `float` prints via
`std::to_string`'s fixed-notation formatting (e.g. `3.140000`); `bool`
prints as `true`/`false`.

## Comments

`// ...` -- from `//` to end of line. There are no block comments.

## Scoping

- Function parameters are visible throughout the function body.
- Each `{ ... }` (function body, `if`/`else` arm, `while` body, or a bare
  nested block) opens a new child scope. A `let` inside is only visible
  until that block's closing `}`.
- There are no closures and no global/module-level variables -- every
  function call starts with a fresh top-level scope containing only its
  parameters. A function cannot see another function's locals, and two
  calls to the same function never share state.

## What's not in v1

No arrays, no structs/records, no strings as a real value type (only
`print`-argument literals), no modules/imports, no generics, no operator
overloading, no `for` loops, no closures. See `PRD.md`'s "Non-goals"
section for the full list and rationale -- these are deliberate scope
cuts for a portfolio-sized compiler project, not oversights.

## Differences in the REPL

The REPL (`./build/mlang` with no arguments) runs the exact same
lex/parse/sema/execute pipeline a file does, with two practical
differences from whole-file compilation:

- **Definition order matters.** A file's Sema pass sees every function's
  signature before checking any body, so functions can call each other
  regardless of source order (including mutual recursion). The REPL
  grows its program one definition at a time, so a function can only call
  itself (recursion) and functions *already* defined in earlier input --
  not one defined later in the same session.
- **Redefinition is allowed.** Typing `fn f(...) { ... }` again for a
  name that already exists replaces the old definition (validated first;
  a broken redefinition is rejected and the old one keeps working) --
  useful for fixing a mistake interactively. A whole file rejects a
  duplicate function name outright.

See [`architecture.md`](architecture.md) for how the REPL still runs every
call through the same tiered dispatch path as a file.
