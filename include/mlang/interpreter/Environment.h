// A single lexical scope: a name->Value map with a pointer to its
// enclosing scope. Blocks, function calls, and if/while bodies each get
// their own Environment chained to the one they're nested in.
#ifndef MLANG_INTERPRETER_ENVIRONMENT_H
#define MLANG_INTERPRETER_ENVIRONMENT_H

#include "mlang/interpreter/Value.h"

#include <string>
#include <unordered_map>

namespace mlang {

class Environment {
public:
  explicit Environment(Environment* parent = nullptr) : parent_(parent) {}

  // Introduces a new binding in this scope (shadowing any outer one with
  // the same name).
  void define(const std::string& name, Value value);

  // Reassigns an existing binding, searching outward through enclosing
  // scopes. Returns false if the name isn't bound anywhere.
  bool assign(const std::string& name, Value value);

  // Looks up a binding, searching outward through enclosing scopes.
  // Returns nullptr if unbound.
  const Value* find(const std::string& name) const;

private:
  Environment* parent_;
  std::unordered_map<std::string, Value> vars_;
};

} // namespace mlang

#endif // MLANG_INTERPRETER_ENVIRONMENT_H
