// A single lexical scope for sema: a name->SemaType map with a pointer to
// its enclosing scope. Mirrors interpreter/Environment.h's shape exactly
// (same parent-chaining, same shadow-on-redefine semantics) so sema
// accepts precisely the scoping the interpreter executes -- sema must not
// be stricter or looser about shadowing than the oracle it's meant to
// reject programs ahead of.
#ifndef MLANG_SEMA_SCOPE_H
#define MLANG_SEMA_SCOPE_H

#include "mlang/sema/SemaType.h"

#include <string>
#include <unordered_map>

namespace mlang {

class Scope {
public:
  explicit Scope(Scope* parent = nullptr) : parent_(parent) {}

  // Introduces a new binding in this scope (shadowing any outer one with
  // the same name, and overwriting a same-scope redeclaration).
  void define(const std::string& name, SemaType type);

  // Looks up a binding, searching outward through enclosing scopes.
  // Returns nullptr if unbound.
  const SemaType* find(const std::string& name) const;

private:
  Scope* parent_;
  std::unordered_map<std::string, SemaType> vars_;
};

} // namespace mlang

#endif // MLANG_SEMA_SCOPE_H
