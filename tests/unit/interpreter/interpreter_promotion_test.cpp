// Proves PRD.md M7's headline scenario end to end, through the same
// Interpreter path --interpret and --trace-promotions use: a recursive
// function promotes mid-run without corrupting the interpreted frames
// already on the call stack when it crosses the threshold, and the final
// result is identical to a pure-interpreted run of the same program.
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

// fn main() -> int { return fib(10); }
Program makeFibProgram() {
  Program program;
  program.functions.push_back(makeFibFn());

  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IntLiteralExpr>(10, loc()));
  std::vector<StmtPtr> body;
  body.push_back(std::make_unique<ReturnStmt>(
      std::make_unique<CallExpr>("fib", std::move(args), loc()), loc()));
  main.body = blockOf(std::move(body));
  program.functions.push_back(std::move(main));
  return program;
}
} // namespace

TEST(InterpreterPromotion, PromotionDisabledByDefault) {
  Program program = makeFibProgram();
  Interpreter interp(program);
  EXPECT_EQ(interp.run(), 55);
  // No Promoter attached, so nothing about the dispatch entry changed --
  // it's still whatever install() bound at construction.
  EXPECT_EQ(interp.dispatchTable().callCount("fib"), 177u);
}

TEST(InterpreterPromotion, LowThresholdPromotesMidRunAndStillMatchesInterpretedResult) {
  Program program = makeFibProgram();
  Interpreter interp(program);

  std::string traced;
  llvm::raw_string_ostream traceOs(traced);
  // fib(10) makes 177 calls; a threshold of 20 guarantees promotion
  // happens mid-recursion, well before the top-level call returns.
  interp.enablePromotion(/*threshold=*/20, &traceOs);

  EXPECT_EQ(interp.run(), 55);
  EXPECT_NE(traced.find("fib promoted to native code after 20 calls"), std::string::npos);
}

TEST(InterpreterPromotion, FullyJitThresholdOfOneStillMatchesInterpretedResult) {
  Program program = makeFibProgram();
  Interpreter interp(program);
  // A threshold of 1 promotes on the very first call to `fib` -- as close
  // to "fully JIT" as this architecture gets, since the call that crosses
  // the threshold always finishes on whatever trampoline it started with.
  interp.enablePromotion(/*threshold=*/1);

  EXPECT_EQ(interp.run(), 55);
}

TEST(InterpreterPromotion, PureInterpretedMidPromotionAndFullyJitRunsAgreeOnOutput) {
  auto run = [](uint64_t threshold) {
    Program program = makeFibProgram();
    std::string output;
    llvm::raw_string_ostream os(output);
    Interpreter interp(program, os);
    if (threshold > 0)
      interp.enablePromotion(threshold);
    int64_t result = interp.run();
    return std::make_pair(result, output);
  };

  auto pureInterpreted = run(/*threshold=*/0);
  auto midPromotion = run(/*threshold=*/20);
  auto fullyJit = run(/*threshold=*/1);

  EXPECT_EQ(pureInterpreted.first, 55);
  EXPECT_EQ(pureInterpreted, midPromotion);
  EXPECT_EQ(pureInterpreted, fullyJit);
}
