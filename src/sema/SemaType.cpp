#include "mlang/sema/SemaType.h"

namespace mlang {

SemaType toSemaType(TypeName type) {
  switch (type) {
  case TypeName::Int:
    return SemaType::Int;
  case TypeName::Float:
    return SemaType::Float;
  case TypeName::Bool:
    return SemaType::Bool;
  }
  return SemaType::Error;
}

std::string_view semaTypeName(SemaType type) {
  switch (type) {
  case SemaType::Int:
    return "int";
  case SemaType::Float:
    return "float";
  case SemaType::Bool:
    return "bool";
  case SemaType::String:
    return "string";
  case SemaType::Error:
    return "<error>";
  }
  return "unknown";
}

} // namespace mlang
