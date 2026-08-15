#include "mlang/ast/Decl.h"
#include "mlang/sema/Sema.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
SourceLocation loc(int line = 1, int col = 1) {
  return {line, col};
}

SemaType check(const Expr& expr, Scope& scope, Sema& sema) {
  return sema.checkExpr(expr, scope);
}
} // namespace

TEST(SemaExpr, ChecksLiterals) {
  Program program;
  Sema sema(program);
  Scope scope;
  EXPECT_EQ(check(IntLiteralExpr(1, loc()), scope, sema), SemaType::Int);
  EXPECT_EQ(check(FloatLiteralExpr(1.0, loc()), scope, sema), SemaType::Float);
  EXPECT_EQ(check(BoolLiteralExpr(true, loc()), scope, sema), SemaType::Bool);
  EXPECT_EQ(check(StringLiteralExpr("hi", loc()), scope, sema), SemaType::String);
  EXPECT_TRUE(sema.diagnostics().empty());
}

TEST(SemaExpr, IdentifierResolvesFromScope) {
  Program program;
  Sema sema(program);
  Scope scope;
  scope.define("x", SemaType::Int);
  EXPECT_EQ(check(IdentifierExpr("x", loc()), scope, sema), SemaType::Int);
}

TEST(SemaExpr, UndefinedIdentifierIsError) {
  Program program;
  Sema sema(program);
  Scope scope;
  EXPECT_EQ(check(IdentifierExpr("missing", loc()), scope, sema), SemaType::Error);
  ASSERT_EQ(sema.diagnostics().size(), 1u);
  EXPECT_EQ(sema.diagnostics()[0].message, "undefined variable 'missing'");
}

TEST(SemaExpr, IntArithmeticIsInt) {
  Program program;
  Sema sema(program);
  Scope scope;
  BinaryExpr add(BinaryOp::Add, std::make_unique<IntLiteralExpr>(1, loc()),
                  std::make_unique<IntLiteralExpr>(2, loc()), loc());
  EXPECT_EQ(check(add, scope, sema), SemaType::Int);
  EXPECT_TRUE(sema.diagnostics().empty());
}

TEST(SemaExpr, MixedIntFloatArithmeticIsError) {
  Program program;
  Sema sema(program);
  Scope scope;
  BinaryExpr add(BinaryOp::Add, std::make_unique<IntLiteralExpr>(1, loc()),
                  std::make_unique<FloatLiteralExpr>(1.0, loc()), loc());
  EXPECT_EQ(check(add, scope, sema), SemaType::Error);
  ASSERT_EQ(sema.diagnostics().size(), 1u);
}

TEST(SemaExpr, ComparisonIsBool) {
  Program program;
  Sema sema(program);
  Scope scope;
  BinaryExpr lt(BinaryOp::Lt, std::make_unique<IntLiteralExpr>(1, loc()),
                 std::make_unique<IntLiteralExpr>(2, loc()), loc());
  EXPECT_EQ(check(lt, scope, sema), SemaType::Bool);
}

TEST(SemaExpr, EqualityAcrossTypesIsError) {
  Program program;
  Sema sema(program);
  Scope scope;
  BinaryExpr eq(BinaryOp::Eq, std::make_unique<IntLiteralExpr>(1, loc()),
                 std::make_unique<BoolLiteralExpr>(true, loc()), loc());
  EXPECT_EQ(check(eq, scope, sema), SemaType::Error);
}

TEST(SemaExpr, AndRequiresBoolOperands) {
  Program program;
  Sema sema(program);
  Scope scope;
  BinaryExpr andExpr(BinaryOp::And, std::make_unique<BoolLiteralExpr>(true, loc()),
                      std::make_unique<IntLiteralExpr>(1, loc()), loc());
  EXPECT_EQ(check(andExpr, scope, sema), SemaType::Error);
}

TEST(SemaExpr, UnaryNegRequiresNumeric) {
  Program program;
  Sema sema(program);
  Scope scope;
  UnaryExpr neg(UnaryOp::Neg, std::make_unique<IntLiteralExpr>(1, loc()), loc());
  EXPECT_EQ(check(neg, scope, sema), SemaType::Int);

  UnaryExpr badNeg(UnaryOp::Neg, std::make_unique<BoolLiteralExpr>(true, loc()), loc());
  EXPECT_EQ(check(badNeg, scope, sema), SemaType::Error);
}

TEST(SemaExpr, UnaryNotRequiresBool) {
  Program program;
  Sema sema(program);
  Scope scope;
  UnaryExpr notExpr(UnaryOp::Not, std::make_unique<BoolLiteralExpr>(true, loc()), loc());
  EXPECT_EQ(check(notExpr, scope, sema), SemaType::Bool);

  UnaryExpr badNot(UnaryOp::Not, std::make_unique<IntLiteralExpr>(1, loc()), loc());
  EXPECT_EQ(check(badNot, scope, sema), SemaType::Error);
}

TEST(SemaExpr, CastIntToFloatOk) {
  Program program;
  Sema sema(program);
  Scope scope;
  CastExpr cast(std::make_unique<IntLiteralExpr>(1, loc()), TypeName::Float, loc());
  EXPECT_EQ(check(cast, scope, sema), SemaType::Float);
}

TEST(SemaExpr, CastBoolToIntIsError) {
  Program program;
  Sema sema(program);
  Scope scope;
  CastExpr cast(std::make_unique<BoolLiteralExpr>(true, loc()), TypeName::Int, loc());
  EXPECT_EQ(check(cast, scope, sema), SemaType::Error);
}

TEST(SemaExpr, PrintAcceptsSingleArgument) {
  Program program;
  Sema sema(program);
  Scope scope;
  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IntLiteralExpr>(1, loc()));
  CallExpr call("print", std::move(args), loc());
  EXPECT_EQ(check(call, scope, sema), SemaType::Int);
  EXPECT_TRUE(sema.diagnostics().empty());
}

TEST(SemaExpr, PrintRejectsWrongArity) {
  Program program;
  Sema sema(program);
  Scope scope;
  CallExpr call("print", {}, loc());
  EXPECT_EQ(check(call, scope, sema), SemaType::Error);
  ASSERT_EQ(sema.diagnostics().size(), 1u);
}

TEST(SemaExpr, CallsUserFunctionWithMatchingArgTypes) {
  Program program;
  program.functions.push_back(FunctionDecl{
      "add", {Param{"a", TypeName::Int, loc()}, Param{"b", TypeName::Int, loc()}}, TypeName::Int,
      nullptr, loc()});
  Sema sema(program);
  Scope scope;
  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IntLiteralExpr>(1, loc()));
  args.push_back(std::make_unique<IntLiteralExpr>(2, loc()));
  CallExpr call("add", std::move(args), loc());
  EXPECT_EQ(check(call, scope, sema), SemaType::Int);
  EXPECT_TRUE(sema.diagnostics().empty());
}

TEST(SemaExpr, CallRejectsArityMismatch) {
  Program program;
  program.functions.push_back(
      FunctionDecl{"id", {Param{"a", TypeName::Int, loc()}}, TypeName::Int, nullptr, loc()});
  Sema sema(program);
  Scope scope;
  CallExpr call("id", {}, loc());
  EXPECT_EQ(check(call, scope, sema), SemaType::Error);
}

TEST(SemaExpr, CallRejectsArgTypeMismatch) {
  Program program;
  program.functions.push_back(
      FunctionDecl{"id", {Param{"a", TypeName::Int, loc()}}, TypeName::Int, nullptr, loc()});
  Sema sema(program);
  Scope scope;
  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<BoolLiteralExpr>(true, loc()));
  CallExpr call("id", std::move(args), loc());
  EXPECT_EQ(check(call, scope, sema), SemaType::Error);
}

TEST(SemaExpr, CallToUndefinedFunctionIsError) {
  Program program;
  Sema sema(program);
  Scope scope;
  CallExpr call("missing", {}, loc());
  EXPECT_EQ(check(call, scope, sema), SemaType::Error);
  ASSERT_EQ(sema.diagnostics().size(), 1u);
  EXPECT_EQ(sema.diagnostics()[0].message, "undefined function 'missing'");
}
