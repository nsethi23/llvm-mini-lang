// Tree-walking interpreter: evaluates a typed-but-unchecked AST directly.
// This is the correctness oracle every codegen test (M5+) is checked
// against -- the same .mlang source must produce the same result whether
// interpreted or JIT-compiled.
#ifndef MLANG_INTERPRETER_INTERPRETER_H
#define MLANG_INTERPRETER_INTERPRETER_H

#include "mlang/ast/Decl.h"
#include "mlang/ast/Expr.h"
#include "mlang/interpreter/Environment.h"
#include "mlang/interpreter/RuntimeError.h"
#include "mlang/interpreter/Value.h"

#include "llvm/Support/raw_ostream.h"

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

private:
  Value evaluateBinary(BinaryOp op, const Value& lhs, const Value& rhs, SourceLocation loc);
  Value evaluateUnary(UnaryOp op, const Value& operand, SourceLocation loc);
  Value evaluateCast(const Value& operand, TypeName target, SourceLocation loc);

  [[noreturn]] void error(SourceLocation loc, const std::string& message);

  const Program& program_;
  llvm::raw_ostream& out_;
};

} // namespace mlang

#endif // MLANG_INTERPRETER_INTERPRETER_H
