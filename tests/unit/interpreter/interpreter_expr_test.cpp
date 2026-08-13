#include "mlang/ast/Decl.h"
#include "mlang/interpreter/Interpreter.h"

#include "llvm/Support/raw_ostream.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
SourceLocation loc(int line = 1, int col = 1) {
  return {line, col};
}

Value eval(const Expr& expr, Environment& env) {
  Program program;
  Interpreter interp(program);
  return interp.evaluate(expr, env);
}
} // namespace

TEST(InterpreterExpr, EvaluatesLiterals) {
  Environment env;
  EXPECT_EQ(std::get<int64_t>(eval(IntLiteralExpr(42, loc()), env)), 42);
  EXPECT_DOUBLE_EQ(std::get<double>(eval(FloatLiteralExpr(3.5, loc()), env)), 3.5);
  EXPECT_EQ(std::get<bool>(eval(BoolLiteralExpr(true, loc()), env)), true);
  EXPECT_EQ(std::get<std::string>(eval(StringLiteralExpr("hi", loc()), env)), "hi");
}

TEST(InterpreterExpr, EvaluatesIdentifierFromEnvironment) {
  Environment env;
  env.define("x", Value{int64_t{7}});
  EXPECT_EQ(std::get<int64_t>(eval(IdentifierExpr("x", loc()), env)), 7);
}

TEST(InterpreterExpr, UndefinedIdentifierThrows) {
  Environment env;
  EXPECT_THROW(eval(IdentifierExpr("missing", loc()), env), RuntimeError);
}

TEST(InterpreterExpr, EvaluatesIntArithmetic) {
  Environment env;
  auto lhs = std::make_unique<IntLiteralExpr>(7, loc());
  auto rhs = std::make_unique<IntLiteralExpr>(3, loc());
  BinaryExpr add(BinaryOp::Add, std::move(lhs), std::move(rhs), loc());
  EXPECT_EQ(std::get<int64_t>(eval(add, env)), 10);
}

TEST(InterpreterExpr, IntDivisionByZeroThrows) {
  Environment env;
  auto lhs = std::make_unique<IntLiteralExpr>(1, loc());
  auto rhs = std::make_unique<IntLiteralExpr>(0, loc());
  BinaryExpr div(BinaryOp::Div, std::move(lhs), std::move(rhs), loc());
  EXPECT_THROW(eval(div, env), RuntimeError);
}

TEST(InterpreterExpr, MixedIntFloatArithmeticThrows) {
  Environment env;
  auto lhs = std::make_unique<IntLiteralExpr>(1, loc());
  auto rhs = std::make_unique<FloatLiteralExpr>(1.0, loc());
  BinaryExpr add(BinaryOp::Add, std::move(lhs), std::move(rhs), loc());
  EXPECT_THROW(eval(add, env), RuntimeError);
}

TEST(InterpreterExpr, EvaluatesComparison) {
  Environment env;
  auto lhs = std::make_unique<IntLiteralExpr>(1, loc());
  auto rhs = std::make_unique<IntLiteralExpr>(2, loc());
  BinaryExpr lt(BinaryOp::Lt, std::move(lhs), std::move(rhs), loc());
  EXPECT_EQ(std::get<bool>(eval(lt, env)), true);
}

TEST(InterpreterExpr, EvaluatesEqualityAcrossTypesAsFalse) {
  Environment env;
  auto lhs = std::make_unique<IntLiteralExpr>(1, loc());
  auto rhs = std::make_unique<BoolLiteralExpr>(true, loc());
  BinaryExpr eq(BinaryOp::Eq, std::move(lhs), std::move(rhs), loc());
  EXPECT_EQ(std::get<bool>(eval(eq, env)), false);
}

TEST(InterpreterExpr, ShortCircuitsAndWithoutEvaluatingRhs) {
  Environment env; // rhs references an undefined variable -- must not be evaluated
  auto lhs = std::make_unique<BoolLiteralExpr>(false, loc());
  auto rhs = std::make_unique<IdentifierExpr>("undefined", loc());
  BinaryExpr andExpr(BinaryOp::And, std::move(lhs), std::move(rhs), loc());
  EXPECT_EQ(std::get<bool>(eval(andExpr, env)), false);
}

TEST(InterpreterExpr, ShortCircuitsOrWithoutEvaluatingRhs) {
  Environment env;
  auto lhs = std::make_unique<BoolLiteralExpr>(true, loc());
  auto rhs = std::make_unique<IdentifierExpr>("undefined", loc());
  BinaryExpr orExpr(BinaryOp::Or, std::move(lhs), std::move(rhs), loc());
  EXPECT_EQ(std::get<bool>(eval(orExpr, env)), true);
}

TEST(InterpreterExpr, EvaluatesUnaryNegAndNot) {
  Environment env;
  UnaryExpr neg(UnaryOp::Neg, std::make_unique<IntLiteralExpr>(5, loc()), loc());
  EXPECT_EQ(std::get<int64_t>(eval(neg, env)), -5);

  UnaryExpr notExpr(UnaryOp::Not, std::make_unique<BoolLiteralExpr>(true, loc()), loc());
  EXPECT_EQ(std::get<bool>(eval(notExpr, env)), false);
}

TEST(InterpreterExpr, EvaluatesIntToFloatCast) {
  Environment env;
  CastExpr cast(std::make_unique<IntLiteralExpr>(3, loc()), TypeName::Float, loc());
  EXPECT_DOUBLE_EQ(std::get<double>(eval(cast, env)), 3.0);
}

TEST(InterpreterExpr, EvaluatesFloatToIntCastTruncates) {
  Environment env;
  CastExpr cast(std::make_unique<FloatLiteralExpr>(3.9, loc()), TypeName::Int, loc());
  EXPECT_EQ(std::get<int64_t>(eval(cast, env)), 3);
}

TEST(InterpreterExpr, UnsupportedCastThrows) {
  Environment env;
  CastExpr cast(std::make_unique<BoolLiteralExpr>(true, loc()), TypeName::Int, loc());
  EXPECT_THROW(eval(cast, env), RuntimeError);
}
