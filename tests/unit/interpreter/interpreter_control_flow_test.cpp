#include "mlang/ast/Decl.h"
#include "mlang/interpreter/Interpreter.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
SourceLocation loc(int line = 1, int col = 1) {
  return {line, col};
}

Interpreter makeInterp() {
  static Program program;
  return Interpreter(program);
}

std::unique_ptr<BlockStmt> blockOf(std::vector<StmtPtr> stmts) {
  return std::make_unique<BlockStmt>(std::move(stmts), loc());
}
} // namespace

TEST(InterpreterControlFlow, IfTrueRunsThenBranch) {
  Interpreter interp = makeInterp();
  Environment env;
  env.define("x", Value{int64_t{0}});

  std::vector<StmtPtr> thenStmts;
  thenStmts.push_back(
      std::make_unique<AssignStmt>("x", std::make_unique<IntLiteralExpr>(1, loc()), loc()));
  IfStmt ifs(std::make_unique<BoolLiteralExpr>(true, loc()), blockOf(std::move(thenStmts)), nullptr,
             loc());

  interp.execute(ifs, env);
  EXPECT_EQ(std::get<int64_t>(*env.find("x")), 1);
}

TEST(InterpreterControlFlow, IfFalseSkipsThenBranchWithNoElse) {
  Interpreter interp = makeInterp();
  Environment env;
  env.define("x", Value{int64_t{0}});

  std::vector<StmtPtr> thenStmts;
  thenStmts.push_back(
      std::make_unique<AssignStmt>("x", std::make_unique<IntLiteralExpr>(1, loc()), loc()));
  IfStmt ifs(std::make_unique<BoolLiteralExpr>(false, loc()), blockOf(std::move(thenStmts)),
             nullptr, loc());

  interp.execute(ifs, env);
  EXPECT_EQ(std::get<int64_t>(*env.find("x")), 0);
}

TEST(InterpreterControlFlow, IfFalseRunsElseBranch) {
  Interpreter interp = makeInterp();
  Environment env;
  env.define("x", Value{int64_t{0}});

  std::vector<StmtPtr> thenStmts;
  thenStmts.push_back(
      std::make_unique<AssignStmt>("x", std::make_unique<IntLiteralExpr>(1, loc()), loc()));
  std::vector<StmtPtr> elseStmts;
  elseStmts.push_back(
      std::make_unique<AssignStmt>("x", std::make_unique<IntLiteralExpr>(2, loc()), loc()));
  IfStmt ifs(std::make_unique<BoolLiteralExpr>(false, loc()), blockOf(std::move(thenStmts)),
             blockOf(std::move(elseStmts)), loc());

  interp.execute(ifs, env);
  EXPECT_EQ(std::get<int64_t>(*env.find("x")), 2);
}

TEST(InterpreterControlFlow, NonBoolIfConditionThrows) {
  Interpreter interp = makeInterp();
  Environment env;
  std::vector<StmtPtr> thenStmts;
  IfStmt ifs(std::make_unique<IntLiteralExpr>(1, loc()), blockOf(std::move(thenStmts)), nullptr,
             loc());
  EXPECT_THROW(interp.execute(ifs, env), RuntimeError);
}

TEST(InterpreterControlFlow, WhileLoopsUntilConditionFalse) {
  Interpreter interp = makeInterp();
  Environment env;
  env.define("i", Value{int64_t{0}});

  // while i < 5 { i = i + 1; }
  auto cond =
      std::make_unique<BinaryExpr>(BinaryOp::Lt, std::make_unique<IdentifierExpr>("i", loc()),
                                   std::make_unique<IntLiteralExpr>(5, loc()), loc());
  std::vector<StmtPtr> bodyStmts;
  bodyStmts.push_back(std::make_unique<AssignStmt>(
      "i",
      std::make_unique<BinaryExpr>(BinaryOp::Add, std::make_unique<IdentifierExpr>("i", loc()),
                                   std::make_unique<IntLiteralExpr>(1, loc()), loc()),
      loc()));
  WhileStmt whileStmt(std::move(cond), blockOf(std::move(bodyStmts)), loc());

  interp.execute(whileStmt, env);
  EXPECT_EQ(std::get<int64_t>(*env.find("i")), 5);
}

TEST(InterpreterControlFlow, WhileFalseConditionNeverRunsBody) {
  Interpreter interp = makeInterp();
  Environment env;
  env.define("ran", Value{false});

  std::vector<StmtPtr> bodyStmts;
  bodyStmts.push_back(
      std::make_unique<AssignStmt>("ran", std::make_unique<BoolLiteralExpr>(true, loc()), loc()));
  WhileStmt whileStmt(std::make_unique<BoolLiteralExpr>(false, loc()),
                      blockOf(std::move(bodyStmts)), loc());

  interp.execute(whileStmt, env);
  EXPECT_EQ(std::get<bool>(*env.find("ran")), false);
}

TEST(InterpreterControlFlow, NestedIfInsideWhile) {
  Interpreter interp = makeInterp();
  Environment env;
  env.define("i", Value{int64_t{0}});
  env.define("evens", Value{int64_t{0}});

  // while i < 4 { if i == 0 { evens = evens + 1; } i = i + 1; }
  auto whileCond =
      std::make_unique<BinaryExpr>(BinaryOp::Lt, std::make_unique<IdentifierExpr>("i", loc()),
                                   std::make_unique<IntLiteralExpr>(4, loc()), loc());
  auto ifCond =
      std::make_unique<BinaryExpr>(BinaryOp::Eq, std::make_unique<IdentifierExpr>("i", loc()),
                                   std::make_unique<IntLiteralExpr>(0, loc()), loc());
  std::vector<StmtPtr> ifThenStmts;
  ifThenStmts.push_back(std::make_unique<AssignStmt>(
      "evens",
      std::make_unique<BinaryExpr>(BinaryOp::Add, std::make_unique<IdentifierExpr>("evens", loc()),
                                   std::make_unique<IntLiteralExpr>(1, loc()), loc()),
      loc()));
  std::vector<StmtPtr> bodyStmts;
  bodyStmts.push_back(
      std::make_unique<IfStmt>(std::move(ifCond), blockOf(std::move(ifThenStmts)), nullptr, loc()));
  bodyStmts.push_back(std::make_unique<AssignStmt>(
      "i",
      std::make_unique<BinaryExpr>(BinaryOp::Add, std::make_unique<IdentifierExpr>("i", loc()),
                                   std::make_unique<IntLiteralExpr>(1, loc()), loc()),
      loc()));
  WhileStmt whileStmt(std::move(whileCond), blockOf(std::move(bodyStmts)), loc());

  interp.execute(whileStmt, env);
  EXPECT_EQ(std::get<int64_t>(*env.find("i")), 4);
  EXPECT_EQ(std::get<int64_t>(*env.find("evens")), 1); // only i == 0 matched
}
