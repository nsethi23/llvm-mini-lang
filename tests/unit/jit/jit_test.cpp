#include "mlang/ast/Decl.h"
#include "mlang/jit/Jit.h"

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

// fn avg(a: float, b: float) -> float { return (a + b) / 2.0; }
FunctionDecl makeAvgFn() {
  FunctionDecl fn;
  fn.name = "avg";
  fn.params.push_back(Param{"a", TypeName::Float, loc()});
  fn.params.push_back(Param{"b", TypeName::Float, loc()});
  fn.returnType = TypeName::Float;
  std::vector<StmtPtr> body;
  auto sum =
      std::make_unique<BinaryExpr>(BinaryOp::Add, std::make_unique<IdentifierExpr>("a", loc()),
                                   std::make_unique<IdentifierExpr>("b", loc()), loc());
  body.push_back(std::make_unique<ReturnStmt>(
      std::make_unique<BinaryExpr>(BinaryOp::Div, std::move(sum),
                                   std::make_unique<FloatLiteralExpr>(2.0, loc()), loc()),
      loc()));
  fn.body = blockOf(std::move(body));
  return fn;
}

// fn isPositive(n: int) -> bool { return n > 0; }
FunctionDecl makeIsPositiveFn() {
  FunctionDecl fn;
  fn.name = "isPositive";
  fn.params.push_back(Param{"n", TypeName::Int, loc()});
  fn.returnType = TypeName::Bool;
  std::vector<StmtPtr> body;
  body.push_back(std::make_unique<ReturnStmt>(
      std::make_unique<BinaryExpr>(BinaryOp::Gt, std::make_unique<IdentifierExpr>("n", loc()),
                                   std::make_unique<IntLiteralExpr>(0, loc()), loc()),
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

TEST(JitTest, BoxAndUnboxRoundTripEveryValueKind) {
  EXPECT_EQ(std::get<int64_t>(Jit::unboxValue(Jit::boxValue(Value{int64_t{42}}), TypeName::Int)),
            42);
  EXPECT_DOUBLE_EQ(std::get<double>(Jit::unboxValue(Jit::boxValue(Value{3.5}), TypeName::Float)),
                   3.5);
  EXPECT_EQ(std::get<bool>(Jit::unboxValue(Jit::boxValue(Value{true}), TypeName::Bool)), true);
  EXPECT_EQ(std::get<bool>(Jit::unboxValue(Jit::boxValue(Value{false}), TypeName::Bool)), false);
}

TEST(JitTest, CompilesAndCallsAnIntFunctionThroughItsEntryThunk) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  Jit jit(program);

  Jit::EntryThunk thunk = jit.compileAndLookup("double");
  int64_t args[] = {21};
  int64_t out = 0;
  thunk(args, &out);
  EXPECT_EQ(out, 42);
}

TEST(JitTest, CompilesAndCallsAFloatFunctionThroughItsEntryThunk) {
  Program program;
  program.functions.push_back(makeAvgFn());
  Jit jit(program);

  Jit::EntryThunk thunk = jit.compileAndLookup("avg");
  int64_t args[] = {Jit::boxValue(Value{3.0}), Jit::boxValue(Value{5.0})};
  int64_t out = 0;
  thunk(args, &out);
  EXPECT_DOUBLE_EQ(std::get<double>(Jit::unboxValue(out, TypeName::Float)), 4.0);
}

TEST(JitTest, CompilesAndCallsABoolFunctionThroughItsEntryThunk) {
  Program program;
  program.functions.push_back(makeIsPositiveFn());
  Jit jit(program);

  Jit::EntryThunk thunk = jit.compileAndLookup("isPositive");
  int64_t args[] = {5};
  int64_t out = 0;
  thunk(args, &out);
  EXPECT_EQ(std::get<bool>(Jit::unboxValue(out, TypeName::Bool)), true);

  args[0] = -5;
  thunk(args, &out);
  EXPECT_EQ(std::get<bool>(Jit::unboxValue(out, TypeName::Bool)), false);
}

TEST(JitTest, CompiledRecursiveFunctionCallsItselfNatively) {
  Program program;
  program.functions.push_back(makeFibFn());
  Jit jit(program);

  Jit::EntryThunk thunk = jit.compileAndLookup("fib");
  int64_t args[] = {10};
  int64_t out = 0;
  thunk(args, &out);
  EXPECT_EQ(out, 55);
}

TEST(JitTest, PrintFromJitCompiledCodeWritesToTheProvidedStream) {
  Program program;
  FunctionDecl fn;
  fn.name = "announce";
  fn.returnType = TypeName::Int;
  std::vector<ExprPtr> printArgs;
  printArgs.push_back(std::make_unique<IntLiteralExpr>(7, loc()));
  std::vector<StmtPtr> body;
  body.push_back(std::make_unique<ExprStmt>(
      std::make_unique<CallExpr>("print", std::move(printArgs), loc()), loc()));
  body.push_back(std::make_unique<ReturnStmt>(std::make_unique<IntLiteralExpr>(0, loc()), loc()));
  fn.body = blockOf(std::move(body));
  program.functions.push_back(std::move(fn));

  std::string captured;
  llvm::raw_string_ostream os(captured);
  Jit jit(program, os);

  Jit::EntryThunk thunk = jit.compileAndLookup("announce");
  int64_t out = 0;
  thunk(nullptr, &out);
  EXPECT_EQ(captured, "7\n");
}
