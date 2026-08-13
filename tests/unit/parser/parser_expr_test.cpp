#include "mlang/ast/AstPrinter.h"
#include "mlang/lexer/Lexer.h"
#include "mlang/parser/Parser.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
std::string parseExprToString(const std::string& source) {
  Lexer lexer(source);
  Parser parser(lexer.tokenize());
  ExprPtr expr = parser.parseExpression();
  if (!expr)
    return "<parse error>";
  return printAst(*expr);
}
} // namespace

TEST(ParserExpr, ParsesLiterals) {
  EXPECT_EQ(parseExprToString("42"), "(int 42)");
  EXPECT_EQ(parseExprToString("3.14"), "(float 3.140000)");
  EXPECT_EQ(parseExprToString("true"), "(bool true)");
  EXPECT_EQ(parseExprToString("false"), "(bool false)");
  EXPECT_EQ(parseExprToString(R"("hi")"), "(str \"hi\")");
  EXPECT_EQ(parseExprToString("x"), "(id x)");
}

TEST(ParserExpr, ParsesArithmeticWithCorrectPrecedence) {
  // * binds tighter than +
  EXPECT_EQ(parseExprToString("1 + 2 * 3"), "(binary + (int 1) (binary * (int 2) (int 3)))");
  // left-associative
  EXPECT_EQ(parseExprToString("1 - 2 - 3"), "(binary - (binary - (int 1) (int 2)) (int 3))");
  EXPECT_EQ(parseExprToString("(1 + 2) * 3"), "(binary * (binary + (int 1) (int 2)) (int 3))");
}

TEST(ParserExpr, ParsesComparisonAndEquality) {
  EXPECT_EQ(parseExprToString("1 < 2"), "(binary < (int 1) (int 2))");
  EXPECT_EQ(parseExprToString("1 == 2"), "(binary == (int 1) (int 2))");
  // == binds looser than <
  EXPECT_EQ(parseExprToString("1 < 2 == 3 < 4"),
            "(binary == (binary < (int 1) (int 2)) (binary < (int 3) (int 4)))");
}

TEST(ParserExpr, ParsesLogicalAndOrWithCorrectPrecedence) {
  // && binds tighter than ||
  EXPECT_EQ(parseExprToString("true || false && true"),
            "(binary || (bool true) (binary && (bool false) (bool true)))");
}

TEST(ParserExpr, ParsesUnaryOperators) {
  EXPECT_EQ(parseExprToString("-5"), "(unary - (int 5))");
  EXPECT_EQ(parseExprToString("!true"), "(unary ! (bool true))");
  EXPECT_EQ(parseExprToString("--5"), "(unary - (unary - (int 5)))");
}

TEST(ParserExpr, ParsesCastExpression) {
  EXPECT_EQ(parseExprToString("x as float"), "(cast (id x) float)");
}

TEST(ParserExpr, ParsesCallExpression) {
  EXPECT_EQ(parseExprToString("fib(n)"), "(call fib (id n))");
  EXPECT_EQ(parseExprToString("fib(n - 1, n - 2)"),
            "(call fib (binary - (id n) (int 1)) (binary - (id n) (int 2)))");
  EXPECT_EQ(parseExprToString("noargs()"), "(call noargs)");
}

TEST(ParserExpr, ParsesFibRecursiveCallExpression) {
  EXPECT_EQ(parseExprToString("fib(n - 1) + fib(n - 2)"),
            "(binary + (call fib (binary - (id n) (int 1))) (call fib (binary - (id n) (int 2))))");
}

TEST(ParserExpr, ReportsDiagnosticOnUnexpectedToken) {
  Lexer lexer("1 +");
  Parser parser(lexer.tokenize());
  ExprPtr expr = parser.parseExpression();
  EXPECT_EQ(expr, nullptr);
  ASSERT_EQ(parser.diagnostics().size(), 1u);
  EXPECT_EQ(parser.diagnostics()[0].message, "expected an expression");
}

TEST(ParserExpr, ReportsDiagnosticOnUnclosedParen) {
  Lexer lexer("(1 + 2");
  Parser parser(lexer.tokenize());
  ExprPtr expr = parser.parseExpression();
  EXPECT_EQ(expr, nullptr);
  ASSERT_EQ(parser.diagnostics().size(), 1u);
}
