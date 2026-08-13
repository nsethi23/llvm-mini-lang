#include "mlang/interpreter/Value.h"

namespace mlang {

std::string valueToString(const Value& value) {
  if (const auto* i = std::get_if<int64_t>(&value))
    return std::to_string(*i);
  if (const auto* d = std::get_if<double>(&value))
    return std::to_string(*d);
  if (const auto* b = std::get_if<bool>(&value))
    return *b ? "true" : "false";
  return std::get<std::string>(value);
}

std::string_view valueTypeName(const Value& value) {
  if (std::holds_alternative<int64_t>(value))
    return "int";
  if (std::holds_alternative<double>(value))
    return "float";
  if (std::holds_alternative<bool>(value))
    return "bool";
  return "string";
}

} // namespace mlang
