#include "mlang/ast/Decl.h"
#include "mlang/interpreter/Interpreter.h"

#include "llvm/Support/raw_ostream.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
SourceLocation loc(int line = 1, int col = 1) {
  return {line, col};
}

std::unique_ptr<BlockStmt> blockOf(std::vector<StmtPtr> stmts) {
  return std::make_unique<BlockStmt>(std::move(stmts), loc());
}

// fn double(n: int) -> int { return n + n; }
FunctionDecl makeDoubleFn() {
  FunctionDecl fn;
  fn.name = "double";
  fn.params.push_back(Param{"n", TypeName::Int, loc()});
  fn.returnType = TypeName::Int;
  std::vector<StmtPtr> body;
  body.push_back(std::make_unique<ReturnStmt>(
      std::make_unique<BinaryExpr>(BinaryOp::Add, std::make_unique<IdentifierExpr>("n", loc()),
                                   std::make_unique<IdentifierExpr>("n", loc()), loc()),
      loc()));
  fn.body = blockOf(std::move(body));
  return fn;
}

// fn fib(n: int) -> int { if n < 2 { return n; } else { return fib(n-1) + fib(n-2); } }
FunctionDecl makeFibFn() {
  FunctionDecl fn;
  fn.name = "fib";
  fn.params.push_back(Param{"n", TypeName::Int, loc()});
  fn.returnType = TypeName::Int;

  auto cond =
      std::make_unique<BinaryExpr>(BinaryOp::Lt, std::make_unique<IdentifierExpr>("n", loc()),
                                   std::make_unique<IntLiteralExpr>(2, loc()), loc());
  std::vector<StmtPtr> thenStmts;
  thenStmts.push_back(
      std::make_unique<ReturnStmt>(std::make_unique<IdentifierExpr>("n", loc()), loc()));

  std::vector<ExprPtr> fibNMinus1Args;
  fibNMinus1Args.push_back(
      std::make_unique<BinaryExpr>(BinaryOp::Sub, std::make_unique<IdentifierExpr>("n", loc()),
                                   std::make_unique<IntLiteralExpr>(1, loc()), loc()));
  auto fibNMinus1 = std::make_unique<CallExpr>("fib", std::move(fibNMinus1Args), loc());

  std::vector<ExprPtr> fibNMinus2Args;
  fibNMinus2Args.push_back(
      std::make_unique<BinaryExpr>(BinaryOp::Sub, std::make_unique<IdentifierExpr>("n", loc()),
                                   std::make_unique<IntLiteralExpr>(2, loc()), loc()));
  auto fibNMinus2 = std::make_unique<CallExpr>("fib", std::move(fibNMinus2Args), loc());

  std::vector<StmtPtr> elseStmts;
  elseStmts.push_back(std::make_unique<ReturnStmt>(
      std::make_unique<BinaryExpr>(BinaryOp::Add, std::move(fibNMinus1), std::move(fibNMinus2),
                                   loc()),
      loc()));

  std::vector<StmtPtr> body;
  body.push_back(std::make_unique<IfStmt>(std::move(cond), blockOf(std::move(thenStmts)),
                                          blockOf(std::move(elseStmts)), loc()));
  fn.body = blockOf(std::move(body));
  return fn;
}
} // namespace

TEST(InterpreterCall, CallsFunctionWithArgument) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  Interpreter interp(program);
  Environment env;

  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IntLiteralExpr>(21, loc()));
  CallExpr call("double", std::move(args), loc());

  EXPECT_EQ(std::get<int64_t>(interp.evaluate(call, env)), 42);
}

TEST(InterpreterCall, RecursiveFibCall) {
  Program program;
  program.functions.push_back(makeFibFn());
  Interpreter interp(program);
  Environment env;

  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IntLiteralExpr>(10, loc()));
  CallExpr call("fib", std::move(args), loc());

  EXPECT_EQ(std::get<int64_t>(interp.evaluate(call, env)), 55);
}

TEST(InterpreterCall, WrongArgCountThrows) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  Interpreter interp(program);
  Environment env;

  CallExpr call("double", {}, loc());
  EXPECT_THROW(interp.evaluate(call, env), RuntimeError);
}

TEST(InterpreterCall, UndefinedFunctionThrows) {
  Program program;
  Interpreter interp(program);
  Environment env;

  CallExpr call("nonexistent", {}, loc());
  EXPECT_THROW(interp.evaluate(call, env), RuntimeError);
}

TEST(InterpreterCall, PrintWritesValueAndNewlineToProvidedStream) {
  Program program;
  std::string captured;
  llvm::raw_string_ostream os(captured);
  Interpreter interp(program, os);
  Environment env;

  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IntLiteralExpr>(7, loc()));
  CallExpr call("print", std::move(args), loc());
  interp.evaluate(call, env);

  EXPECT_EQ(captured, "7\n");
}

TEST(InterpreterCall, PrintWrongArgCountThrows) {
  Program program;
  Interpreter interp(program);
  Environment env;
  CallExpr call("print", {}, loc());
  EXPECT_THROW(interp.evaluate(call, env), RuntimeError);
}

TEST(InterpreterCall, FunctionCallGetsFreshScopeNotCallersLocals) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  Interpreter interp(program);
  Environment env;
  env.define("n", Value{int64_t{999}}); // caller's 'n' must not leak into the callee

  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IntLiteralExpr>(5, loc()));
  CallExpr call("double", std::move(args), loc());

  EXPECT_EQ(std::get<int64_t>(interp.evaluate(call, env)), 10);
}
