// The v1 type system: int (i64), float (double), bool. No user-defined
// types, no implicit int/float coercion (PRD.md sec. 5.1).
#ifndef MLANG_AST_TYPE_H
#define MLANG_AST_TYPE_H

#include <string_view>

namespace mlang {

enum class TypeName {
  Int,
  Float,
  Bool,
};

std::string_view typeName(TypeName type);

} // namespace mlang

#endif // MLANG_AST_TYPE_H
