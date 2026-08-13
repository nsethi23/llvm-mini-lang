#include "mlang/ast/Type.h"

namespace mlang {

std::string_view typeName(TypeName type) {
  switch (type) {
  case TypeName::Int:
    return "int";
  case TypeName::Float:
    return "float";
  case TypeName::Bool:
    return "bool";
  }
  return "unknown";
}

} // namespace mlang
