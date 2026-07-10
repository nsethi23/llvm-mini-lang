// Tree-walking interpreter: evaluates a typed-but-unchecked AST directly.
// This is the correctness oracle every codegen test (M5+) is checked
// against -- the same .mlang source must produce the same result whether
// interpreted or JIT-compiled.
#ifndef MLANG_INTERPRETER_INTERPRETER_H
#define MLANG_INTERPRETER_INTERPRETER_H

#include "mlang/ast/Decl.h"
#include "mlang/ast/Expr.h"
#include "mlang/ast/Stmt.h"
#include "mlang/interpreter/Environment.h"
#include "mlang/interpreter/RuntimeError.h"
#include "mlang/interpreter/Value.h"

#include "llvm/Support/raw_ostream.h"

#include <unordered_map>
#include <vector>

namespace mlang {

class Interpreter {
public:
  // `out` receives print() output; overridable so tests can capture it
  // without touching real stdout.
  explicit Interpreter(const Program& program, llvm::raw_ostream& out = llvm::outs());

  // Evaluates a single expression in the given scope. Public so expression
  // evaluation is independently testable; statement/call execution builds
  // on top of it in later commits. Throws RuntimeError on failure.
  Value evaluate(const Expr& expr, Environment& env);

  // Executes a single statement in the given scope. `return` unwinds via
  // ReturnSignal, which only a function call (added in a later commit) is
  // expected to catch -- calling this directly on a `return` statement in
  // isolation (as tests do) lets that exception propagate to the caller.
  void execute(const Stmt& stmt, Environment& env);

  // Executes a block in a fresh child scope of `parentEnv`.
  void executeBlock(const BlockStmt& block, Environment& parentEnv);

  // Finds and calls `main` with no arguments, returning its (required to be
  // int) return value as the process exit code. Throws RuntimeError if
  // there's no `main` or it doesn't return int.
  int64_t run();

private:
  // Thrown by a `return` statement to unwind out of the current function
  // call. Never escapes a public entry point that represents a complete
  // function invocation.
  struct ReturnSignal {
    Value value;
  };

  Value evaluateBinary(BinaryOp op, const Value& lhs, const Value& rhs, SourceLocation loc);
  Value evaluateUnary(UnaryOp op, const Value& operand, SourceLocation loc);
  Value evaluateCast(const Value& operand, TypeName target, SourceLocation loc);
  Value evaluateCall(const CallExpr& call, Environment& env);
  Value callFunction(const FunctionDecl& fn, std::vector<Value> args, SourceLocation callLoc);
  Value callBuiltin(const std::string& name, std::vector<Value>& args, SourceLocation loc);

  [[noreturn]] void error(SourceLocation loc, const std::string& message);

  const Program& program_;
  llvm::raw_ostream& out_;
  std::unordered_map<std::string, const FunctionDecl*> functions_;
};

} // namespace mlang

#endif // MLANG_INTERPRETER_INTERPRETER_H
