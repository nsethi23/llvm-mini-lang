// Static type checker: walks the parsed-but-unchecked AST and rejects
// ill-typed programs before they reach the interpreter or codegen (PRD.md
// M4). Collects Diagnostics rather than throwing, so a whole file's type
// errors can be reported at once -- mirroring Parser's error-recovery
// philosophy (see parser/Diagnostic.h) rather than the interpreter's
// throw-on-first-error RuntimeError.
#ifndef MLANG_SEMA_SEMA_H
#define MLANG_SEMA_SEMA_H

#include "mlang/ast/Decl.h"
#include "mlang/ast/Expr.h"
#include "mlang/parser/Diagnostic.h"
#include "mlang/sema/Scope.h"
#include "mlang/sema/SemaType.h"

#include <unordered_map>
#include <vector>

namespace mlang {

class Sema {
public:
  explicit Sema(const Program& program);

  // Type-checks a single expression in the given scope, returning its
  // type (SemaType::Error if the expression is ill-typed -- the specific
  // diagnostic has already been recorded). Public so expression checking
  // is independently testable, matching Interpreter::evaluate's shape.
  SemaType checkExpr(const Expr& expr, Scope& scope);

  const std::vector<Diagnostic>& diagnostics() const { return diags_; }

private:
  SemaType checkUnary(const UnaryExpr& expr, Scope& scope);
  SemaType checkBinary(const BinaryExpr& expr, Scope& scope);
  SemaType checkCast(const CastExpr& expr, Scope& scope);
  SemaType checkCall(const CallExpr& expr, Scope& scope);

  void error(SourceLocation loc, std::string message);

  const Program& program_;
  std::vector<Diagnostic> diags_;
  std::unordered_map<std::string, const FunctionDecl*> functions_;
};

} // namespace mlang

#endif // MLANG_SEMA_SEMA_H
