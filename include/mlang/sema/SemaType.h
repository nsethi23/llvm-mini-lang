// The type domain sema reasons about. A superset of TypeName: string
// literals need a type to flow through expression checking even though
// v1 has no string-typed variables (mirrors Value's rationale, see
// interpreter/Value.h), and Error is a sentinel returned after a
// diagnostic has already been emitted so one mistake doesn't cascade into
// a wall of follow-on type-mismatch errors for the same expression tree.
#ifndef MLANG_SEMA_SEMATYPE_H
#define MLANG_SEMA_SEMATYPE_H

#include "mlang/ast/Type.h"

#include <string_view>

namespace mlang {

enum class SemaType {
  Int,
  Float,
  Bool,
  String,
  Error,
};

SemaType toSemaType(TypeName type);
std::string_view semaTypeName(SemaType type);

} // namespace mlang

#endif // MLANG_SEMA_SEMATYPE_H
