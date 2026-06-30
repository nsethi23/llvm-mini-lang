#include "mlang/ast/AstPrinter.h"
#include "mlang/lexer/Lexer.h"
#include "mlang/parser/Parser.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
std::string parseBlockToString(const std::string& source) {
  Lexer lexer(source);
  Parser parser(lexer.tokenize());
  auto block = parser.parseBlock();
  if (!block)
    return "<parse error>";
  return printAst(*block);
}
} // namespace

TEST(ParserStmt, ParsesEmptyBlock) {
  EXPECT_EQ(parseBlockToString("{}"), "(block)");
}

TEST(ParserStmt, ParsesLetStmt) {
  EXPECT_EQ(parseBlockToString("{ let x: int = 5; }"), "(block\n  (let x int (int 5)))");
}

TEST(ParserStmt, ParsesLetStmtWithExpressionInitializer) {
  EXPECT_EQ(parseBlockToString("{ let y: float = 1.0 + 2.0; }"),
            "(block\n  (let y float (binary + (float 1.000000) (float 2.000000))))");
}

TEST(ParserStmt, ParsesReturnWithValue) {
  EXPECT_EQ(parseBlockToString("{ return x + 1; }"),
            "(block\n  (return (binary + (id x) (int 1))))");
}

TEST(ParserStmt, ParsesBareReturn) {
  EXPECT_EQ(parseBlockToString("{ return; }"), "(block\n  (return))");
}

TEST(ParserStmt, ParsesExprStmt) {
  EXPECT_EQ(parseBlockToString("{ print(x); }"), "(block\n  (exprstmt (call print (id x))))");
}

TEST(ParserStmt, ParsesMultipleStatementsInOrder) {
  EXPECT_EQ(parseBlockToString("{ let x: int = 1; let y: int = 2; return x + y; }"),
            "(block\n"
            "  (let x int (int 1))\n"
            "  (let y int (int 2))\n"
            "  (return (binary + (id x) (id y))))");
}

TEST(ParserStmt, MissingSemicolonIsAParseError) {
  Lexer lexer("{ let x: int = 5 }");
  Parser parser(lexer.tokenize());
  auto block = parser.parseBlock();
  EXPECT_EQ(block, nullptr);
  ASSERT_EQ(parser.diagnostics().size(), 1u);
  EXPECT_EQ(parser.diagnostics()[0].message, "expected ';' after let statement");
}

TEST(ParserStmt, UnclosedBlockIsAParseError) {
  Lexer lexer("{ let x: int = 5;");
  Parser parser(lexer.tokenize());
  auto block = parser.parseBlock();
  EXPECT_EQ(block, nullptr);
  ASSERT_EQ(parser.diagnostics().size(), 1u);
}
