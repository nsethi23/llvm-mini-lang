// Proves the M6 dispatch-layer mechanism the PRD demo asks for: every
// function call increments its dispatch entry's call count, and patching
// an entry's trampoline mid-run redirects subsequent calls to the new
// target without the caller's code (evaluateCall) changing at all.
#include "mlang/ast/Decl.h"
#include "mlang/interpreter/Interpreter.h"

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

TEST(InterpreterDispatch, EveryFunctionStartsWithAnInstalledInterpreterEntry) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  Interpreter interp(program);

  EXPECT_TRUE(interp.dispatchTable().contains("double"));
  EXPECT_EQ(interp.dispatchTable().callCount("double"), 0u);
}

TEST(InterpreterDispatch, CallIncrementsTheCalleesCallCount) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  Interpreter interp(program);
  Environment env;

  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IntLiteralExpr>(21, loc()));
  CallExpr call("double", std::move(args), loc());

  interp.evaluate(call, env);
  EXPECT_EQ(interp.dispatchTable().callCount("double"), 1u);

  interp.evaluate(call, env);
  EXPECT_EQ(interp.dispatchTable().callCount("double"), 2u);
}

TEST(InterpreterDispatch, RecursiveSelfCallsAllCountTowardTheSameEntry) {
  Program program;
  program.functions.push_back(makeFibFn());
  Interpreter interp(program);
  Environment env;

  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IntLiteralExpr>(10, loc()));
  CallExpr call("fib", std::move(args), loc());

  EXPECT_EQ(std::get<int64_t>(interp.evaluate(call, env)), 55);
  // fib(10) makes 177 total invocations of fib (itself plus every
  // recursive self-call) before returning.
  EXPECT_EQ(interp.dispatchTable().callCount("fib"), 177u);
}

// This is the M6 demo requirement from PRD.md: manually patch one
// function's dispatch entry mid-run and show subsequent calls resolve
// through the new target -- the exact mechanism M7's promotion needs.
TEST(InterpreterDispatch, RedirectingAnEntryMidRunChangesWhatSubsequentCallsResolveTo) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  Interpreter interp(program);
  Environment env;

  std::vector<ExprPtr> firstCallArgs;
  firstCallArgs.push_back(std::make_unique<IntLiteralExpr>(21, loc()));
  CallExpr firstCall("double", std::move(firstCallArgs), loc());
  EXPECT_EQ(std::get<int64_t>(interp.evaluate(firstCall, env)), 42);

  // Stand in for what M7's promotion does: swap the entry's target for a
  // different implementation entirely -- here, a stub that always returns
  // -1 regardless of its argument, standing in for a distinct compiled
  // implementation.
  interp.dispatchTable().redirect(
      "double", [](std::vector<Value>, SourceLocation) { return Value{int64_t{-1}}; });

  std::vector<ExprPtr> secondCallArgs;
  secondCallArgs.push_back(std::make_unique<IntLiteralExpr>(21, loc()));
  CallExpr secondCall("double", std::move(secondCallArgs), loc());
  EXPECT_EQ(std::get<int64_t>(interp.evaluate(secondCall, env)), -1);

  // The call counter still tracks total calls through the entry, across
  // the redirect.
  EXPECT_EQ(interp.dispatchTable().callCount("double"), 2u);
}
