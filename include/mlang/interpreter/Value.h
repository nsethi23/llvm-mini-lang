// Runtime value representation for the tree-walking interpreter. Mirrors
// the v1 type system (int, float, bool) plus string, which only appears as
// an immediate literal (e.g. a print() argument) -- there is no `string`
// entry in TypeName since v1 has no string-typed variables (PRD.md sec.
// 5.1).
#ifndef MLANG_INTERPRETER_VALUE_H
#define MLANG_INTERPRETER_VALUE_H

#include <cstdint>
#include <string>
#include <variant>

namespace mlang {

using Value = std::variant<int64_t, double, bool, std::string>;

// Human-readable rendering used by the print() builtin and golden tests.
std::string valueToString(const Value& value);

// Name of the active alternative, for runtime type-mismatch diagnostics.
std::string_view valueTypeName(const Value& value);

} // namespace mlang

#endif // MLANG_INTERPRETER_VALUE_H
