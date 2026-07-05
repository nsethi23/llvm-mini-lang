#include "mlang/ast/Decl.h"
#include "mlang/ast/Expr.h"
#include "mlang/ast/Stmt.h"

#include <gtest/gtest.h>

using namespace mlang;

TEST(AstNodes, LiteralExprsHoldTheirValues) {
  IntLiteralExpr i(42, {1, 1});
  EXPECT_EQ(i.kind, ExprKind::IntLiteral);
  EXPECT_EQ(i.value, 42);

  FloatLiteralExpr f(3.14, {1, 1});
  EXPECT_DOUBLE_EQ(f.value, 3.14);

  BoolLiteralExpr b(true, {1, 1});
  EXPECT_TRUE(b.value);

  StringLiteralExpr s("hi", {1, 1});
  EXPECT_EQ(s.value, "hi");
}

TEST(AstNodes, BinaryExprOwnsItsOperands) {
  auto lhs = std::make_unique<IntLiteralExpr>(1, SourceLocation{1, 1});
  auto rhs = std::make_unique<IntLiteralExpr>(2, SourceLocation{1, 5});
  BinaryExpr add(BinaryOp::Add, std::move(lhs), std::move(rhs), {1, 3});
  EXPECT_EQ(add.kind, ExprKind::Binary);
  EXPECT_EQ(static_cast<IntLiteralExpr*>(add.lhs.get())->value, 1);
  EXPECT_EQ(static_cast<IntLiteralExpr*>(add.rhs.get())->value, 2);
  EXPECT_EQ(binaryOpName(add.op), "+");
}

TEST(AstNodes, CallExprHoldsCalleeAndArgs) {
  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IdentifierExpr>("n", SourceLocation{1, 1}));
  CallExpr call("fib", std::move(args), {1, 1});
  EXPECT_EQ(call.callee, "fib");
  ASSERT_EQ(call.args.size(), 1u);
}

TEST(AstNodes, AssignStmtHoldsNameAndValue) {
  auto value = std::make_unique<IntLiteralExpr>(5, SourceLocation{1, 1});
  AssignStmt assign("x", std::move(value), {1, 1});
  EXPECT_EQ(assign.kind, StmtKind::Assign);
  EXPECT_EQ(assign.name, "x");
  EXPECT_EQ(static_cast<IntLiteralExpr*>(assign.value.get())->value, 5);
}

TEST(AstNodes, IfStmtHoldsOptionalElseBlock) {
  auto cond = std::make_unique<BoolLiteralExpr>(true, SourceLocation{1, 1});
  std::vector<StmtPtr> thenStmts;
  auto thenBlock = std::make_unique<BlockStmt>(std::move(thenStmts), SourceLocation{1, 1});
  IfStmt ifs(std::move(cond), std::move(thenBlock), nullptr, {1, 1});
  EXPECT_EQ(ifs.kind, StmtKind::If);
  EXPECT_EQ(ifs.elseBlock, nullptr);
}

TEST(AstNodes, FunctionDeclHoldsParamsAndReturnType) {
  FunctionDecl fn;
  fn.name = "fib";
  fn.params.push_back(Param{"n", TypeName::Int, {1, 1}});
  fn.returnType = TypeName::Int;
  EXPECT_EQ(fn.params.size(), 1u);
  EXPECT_EQ(typeName(fn.returnType), "int");
}
