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
#include "mlang/ast/Stmt.h"
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

  // Type-checks a single statement in the given scope, in the context of
  // `fn` (return statements are checked against fn's declared return
  // type). Public so statement checking is independently testable,
  // matching Interpreter::execute's shape.
  void checkStmt(const Stmt& stmt, Scope& scope, const FunctionDecl& fn);

  // Type-checks a block in a fresh child scope of `parentScope`, matching
  // Interpreter::executeBlock's scoping.
  void checkBlock(const BlockStmt& block, Scope& parentScope, const FunctionDecl& fn);

  // Type-checks the whole program: duplicate/builtin-shadowing function
  // names, main()'s signature, and every function's body plus its
  // all-paths-return flow. Returns true (and leaves diagnostics() empty)
  // iff the program is well-typed.
  bool check();

  // Type-checks a single function's params, body, and all-paths-return
  // flow -- everything check() does per-function, without the whole-
  // program duplicate-name/main() checks. Public so the REPL (PRD.md M9)
  // can validate one newly-defined function at a time against the
  // functions already known to this Sema instance, matching how a REPL
  // grows a program incrementally rather than checking it all at once.
  void checkFunction(const FunctionDecl& fn);

  const std::vector<Diagnostic>& diagnostics() const {
    return diags_;
  }

private:
  SemaType checkUnary(const UnaryExpr& expr, Scope& scope);
  SemaType checkBinary(const BinaryExpr& expr, Scope& scope);
  SemaType checkCast(const CastExpr& expr, Scope& scope);
  SemaType checkCall(const CallExpr& expr, Scope& scope);

  // Conservative static flow check: true only if every path through the
  // statement/block is guaranteed to hit a `return` (a `while` body never
  // counts, since its condition may be false on entry).
  static bool stmtAlwaysReturns(const Stmt& stmt);
  static bool blockAlwaysReturns(const BlockStmt& block);

  void error(SourceLocation loc, std::string message);

  const Program& program_;
  std::vector<Diagnostic> diags_;
  std::unordered_map<std::string, const FunctionDecl*> functions_;
};

} // namespace mlang

#endif // MLANG_SEMA_SEMA_H
