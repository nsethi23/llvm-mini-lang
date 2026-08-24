#include "mlang/jit/Repl.h"

#include "llvm/Support/raw_ostream.h"

#include <gtest/gtest.h>

#include <sstream>

using namespace mlang;

namespace {
std::string runSession(const std::string& input) {
  std::istringstream in(input);
  std::string captured;
  llvm::raw_string_ostream out(captured);
  Repl repl(in, out);
  repl.run();
  out.flush();
  return captured;
}
} // namespace

TEST(ReplTest, EvaluatesAndPrintsABareExpression) {
  std::string out = runSession("1 + 2\n:quit\n");
  EXPECT_NE(out.find("3\n"), std::string::npos);
}

TEST(ReplTest, LetBindingPersistsAcrossLines) {
  std::string out = runSession("let x: int = 5;\nx + 1\n:quit\n");
  EXPECT_NE(out.find("6\n"), std::string::npos);
}

TEST(ReplTest, DefinesAndCallsAFunction) {
  std::string out = runSession("fn double(n: int) -> int { return n + n; }\n"
                               "double(21)\n:quit\n");
  EXPECT_NE(out.find("defined fn double(n: int) -> int"), std::string::npos);
  EXPECT_NE(out.find("42\n"), std::string::npos);
}

TEST(ReplTest, RedefiningAFunctionReplacesItsBehavior) {
  std::string out = runSession("fn f(n: int) -> int { return 1; }\n"
                               "f(0)\n"
                               "fn f(n: int) -> int { return 2; }\n"
                               "f(0)\n:quit\n");
  EXPECT_NE(out.find("defined fn f"), std::string::npos);
  EXPECT_NE(out.find("redefined fn f"), std::string::npos);
  EXPECT_NE(out.find("1\n"), std::string::npos);
  EXPECT_NE(out.find("2\n"), std::string::npos);
}

TEST(ReplTest, RecursiveFunctionPromotesMidSessionAndKeepsWorking) {
  std::string program = "fn fib(n: int) -> int {\n"
                        "    if n < 2 {\n"
                        "        return n;\n"
                        "    } else {\n"
                        "        return fib(n - 1) + fib(n - 2);\n"
                        "    }\n"
                        "}\n";
  std::string out = runSession(program + "fib(10)\n:quit\n");
  EXPECT_NE(out.find("promoted to native code after"), std::string::npos);
  EXPECT_NE(out.find("55\n"), std::string::npos);
}

TEST(ReplTest, SyntaxErrorReportsAndKeepsSessionAlive) {
  std::string out = runSession("let x: int = ;\nlet y: int = 1;\ny\n:quit\n");
  EXPECT_NE(out.find("error:"), std::string::npos);
  EXPECT_NE(out.find("1\n"), std::string::npos);
}

TEST(ReplTest, TypeErrorReportsAndKeepsSessionAlive) {
  std::string out = runSession("let x: int = true;\nlet y: int = 2;\ny\n:quit\n");
  EXPECT_NE(out.find("error:"), std::string::npos);
  EXPECT_NE(out.find("2\n"), std::string::npos);
}

TEST(ReplTest, UndefinedVariableReportsAnError) {
  std::string out = runSession("nope\n:quit\n");
  EXPECT_NE(out.find("error:"), std::string::npos);
  EXPECT_NE(out.find("undefined variable"), std::string::npos);
}

TEST(ReplTest, TopLevelReturnIsRejectedEvenNestedInsideIf) {
  std::string out1 = runSession("return 5;\n:quit\n");
  EXPECT_NE(out1.find("'return' is only valid inside a function body"), std::string::npos);

  std::string out2 = runSession("if true { return 5; }\n:quit\n");
  EXPECT_NE(out2.find("'return' is only valid inside a function body"), std::string::npos);
}

TEST(ReplTest, CannotRedefinePrint) {
  std::string out = runSession("fn print(x: int) -> int { return x; }\n:quit\n");
  EXPECT_NE(out.find("'print' is a built-in function and cannot be redefined"), std::string::npos);
}

TEST(ReplTest, MultiLineFunctionDefinitionIsAccumulatedViaBraceBalance) {
  std::string out = runSession("fn addOne(n: int) -> int {\n"
                               "    return n + 1;\n"
                               "}\n"
                               "addOne(4)\n:quit\n");
  EXPECT_NE(out.find("defined fn addOne(n: int) -> int"), std::string::npos);
  EXPECT_NE(out.find("5\n"), std::string::npos);
}

TEST(ReplTest, BlockScopingMatchesFileSemantics) {
  std::string out = runSession("let y: int = 1;\n"
                               "if y > 0 { let y: int = 99; print(y); }\n"
                               "print(y);\n:quit\n");
  size_t first99 = out.find("99\n");
  size_t last1 = out.rfind("1\n");
  ASSERT_NE(first99, std::string::npos);
  ASSERT_NE(last1, std::string::npos);
  EXPECT_LT(first99, last1);
}

TEST(ReplTest, EofExitsCleanlyWithoutAQuitCommand) {
  std::string out = runSession("let x: int = 1;\nx\n");
  EXPECT_NE(out.find("1\n"), std::string::npos);
}

TEST(ReplTest, HelpCommandPrintsUsage) {
  std::string out = runSession(":help\n:quit\n");
  EXPECT_NE(out.find("Multi-line input is supported"), std::string::npos);
}
