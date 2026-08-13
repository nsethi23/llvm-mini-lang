#include "mlang/ast/AstPrinter.h"
#include "mlang/lexer/Lexer.h"
#include "mlang/parser/Parser.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
std::string parseProgramToString(const std::string& source) {
  Lexer lexer(source);
  Parser parser(lexer.tokenize());
  Program program = parser.parseProgram();
  return printAst(program);
}
} // namespace

TEST(ParserProgram, ParsesFunctionWithNoParams) {
  EXPECT_EQ(parseProgramToString("fn main() -> int { return 0; }"),
            "(program\n  (fn main () int\n    (block\n      (return (int 0)))))");
}

TEST(ParserProgram, ParsesFunctionWithOneParam) {
  EXPECT_EQ(parseProgramToString("fn fib(n: int) -> int { return n; }"),
            "(program\n  (fn fib ((n int)) int\n    (block\n      (return (id n)))))");
}

TEST(ParserProgram, ParsesFunctionWithMultipleParams) {
  EXPECT_EQ(parseProgramToString("fn add(a: int, b: int) -> int { return a + b; }"),
            "(program\n  (fn add ((a int) (b int)) int\n    (block\n      (return (binary + "
            "(id a) (id b))))))");
}

TEST(ParserProgram, ParsesMultipleFunctions) {
  Lexer lexer("fn f() -> int { return 1; } fn g() -> int { return 2; }");
  Parser parser(lexer.tokenize());
  Program program = parser.parseProgram();
  ASSERT_EQ(program.functions.size(), 2u);
  EXPECT_EQ(program.functions[0].name, "f");
  EXPECT_EQ(program.functions[1].name, "g");
}

TEST(ParserProgram, ParsesFullFibExample) {
  std::string src = "fn fib(n: int) -> int {\n"
                    "    if n < 2 {\n"
                    "        return n;\n"
                    "    } else {\n"
                    "        return fib(n - 1) + fib(n - 2);\n"
                    "    }\n"
                    "}\n"
                    "fn main() -> int {\n"
                    "    let x: int = 10;\n"
                    "    let result: int = fib(x);\n"
                    "    print(result);\n"
                    "    return 0;\n"
                    "}\n";
  Lexer lexer(src);
  Parser parser(lexer.tokenize());
  Program program = parser.parseProgram();
  EXPECT_TRUE(parser.diagnostics().empty());
  ASSERT_EQ(program.functions.size(), 2u);
  EXPECT_EQ(program.functions[0].name, "fib");
  EXPECT_EQ(program.functions[0].params.size(), 1u);
  EXPECT_EQ(program.functions[1].name, "main");
}

TEST(ParserProgram, ReportsDiagnosticOnMissingArrow) {
  Lexer lexer("fn f() int { return 0; }");
  Parser parser(lexer.tokenize());
  Program program = parser.parseProgram();
  ASSERT_EQ(parser.diagnostics().size(), 1u);
  EXPECT_EQ(parser.diagnostics()[0].message, "expected '->' after parameter list");
}

TEST(ParserProgram, ReportsDiagnosticOnMissingReturnType) {
  Lexer lexer("fn f() -> { return 0; }");
  Parser parser(lexer.tokenize());
  Program program = parser.parseProgram();
  ASSERT_EQ(parser.diagnostics().size(), 1u);
}
