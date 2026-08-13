#include "mlang/ast/AstPrinter.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
SourceLocation loc(int line = 1, int col = 1) {
  return {line, col};
}
} // namespace

TEST(AstPrinter, PrintsLiterals) {
  EXPECT_EQ(printAst(static_cast<const Expr&>(IntLiteralExpr(42, loc()))), "(int 42)");
  EXPECT_EQ(printAst(static_cast<const Expr&>(BoolLiteralExpr(true, loc()))), "(bool true)");
}

TEST(AstPrinter, PrintsBinaryExpr) {
  auto lhs = std::make_unique<IdentifierExpr>("n", loc());
  auto rhs = std::make_unique<IntLiteralExpr>(2, loc());
  BinaryExpr lt(BinaryOp::Lt, std::move(lhs), std::move(rhs), loc());
  EXPECT_EQ(printAst(static_cast<const Expr&>(lt)), "(binary < (id n) (int 2))");
}

TEST(AstPrinter, PrintsCallExpr) {
  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IdentifierExpr>("n", loc()));
  CallExpr call("fib", std::move(args), loc());
  EXPECT_EQ(printAst(static_cast<const Expr&>(call)), "(call fib (id n))");
}

TEST(AstPrinter, PrintsIfStmtWithoutElse) {
  auto cond = std::make_unique<BoolLiteralExpr>(true, loc());
  std::vector<StmtPtr> thenStmts;
  thenStmts.push_back(std::make_unique<ReturnStmt>(nullptr, loc()));
  auto thenBlock = std::make_unique<BlockStmt>(std::move(thenStmts), loc());
  IfStmt ifs(std::move(cond), std::move(thenBlock), nullptr, loc());

  EXPECT_EQ(printAst(static_cast<const Stmt&>(ifs)), "(if (bool true)\n  (block\n    (return)))");
}

TEST(AstPrinter, PrintsFullFunctionProgram) {
  // fn fib(n: int) -> int { if n < 2 { return n; } }
  Program program;
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
  auto thenBlock = std::make_unique<BlockStmt>(std::move(thenStmts), loc());
  std::vector<StmtPtr> bodyStmts;
  bodyStmts.push_back(
      std::make_unique<IfStmt>(std::move(cond), std::move(thenBlock), nullptr, loc()));
  fn.body = std::make_unique<BlockStmt>(std::move(bodyStmts), loc());
  program.functions.push_back(std::move(fn));

  std::string expected = "(program\n"
                         "  (fn fib ((n int)) int\n"
                         "    (block\n"
                         "      (if (binary < (id n) (int 2))\n"
                         "        (block\n"
                         "          (return (id n)))))))";
  EXPECT_EQ(printAst(program), expected);
}
