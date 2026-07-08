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
} // namespace

TEST(InterpreterStmt, LetDefinesVariableInScope) {
  Interpreter interp = makeInterp();
  Environment env;
  LetStmt let("x", TypeName::Int, std::make_unique<IntLiteralExpr>(5, loc()), loc());
  interp.execute(let, env);
  ASSERT_NE(env.find("x"), nullptr);
  EXPECT_EQ(std::get<int64_t>(*env.find("x")), 5);
}

TEST(InterpreterStmt, AssignUpdatesExistingVariable) {
  Interpreter interp = makeInterp();
  Environment env;
  env.define("x", Value{int64_t{1}});
  AssignStmt assign("x", std::make_unique<IntLiteralExpr>(9, loc()), loc());
  interp.execute(assign, env);
  EXPECT_EQ(std::get<int64_t>(*env.find("x")), 9);
}

TEST(InterpreterStmt, AssignToUndefinedVariableThrows) {
  Interpreter interp = makeInterp();
  Environment env;
  AssignStmt assign("missing", std::make_unique<IntLiteralExpr>(9, loc()), loc());
  EXPECT_THROW(interp.execute(assign, env), RuntimeError);
}

TEST(InterpreterStmt, ReturnThrowsReturnSignalCarryingValue) {
  Interpreter interp = makeInterp();
  Environment env;
  ReturnStmt ret(std::make_unique<IntLiteralExpr>(7, loc()), loc());
  // ReturnSignal is a private type -- observe it indirectly via
  // executeBlock/callFunction in later tests. Here we only confirm *some*
  // exception unwinds out, since std::exception isn't its base.
  EXPECT_ANY_THROW(interp.execute(ret, env));
}

TEST(InterpreterStmt, ExprStmtEvaluatesAndDiscardsResult) {
  Interpreter interp = makeInterp();
  Environment env;
  ExprStmt stmt(std::make_unique<IntLiteralExpr>(1, loc()), loc());
  EXPECT_NO_THROW(interp.execute(stmt, env));
}

TEST(InterpreterStmt, ExecuteBlockCreatesChildScopeThatDoesNotLeakOut) {
  Interpreter interp = makeInterp();
  Environment outer;
  std::vector<StmtPtr> stmts;
  stmts.push_back(std::make_unique<LetStmt>("y", TypeName::Int,
                                            std::make_unique<IntLiteralExpr>(3, loc()), loc()));
  BlockStmt block(std::move(stmts), loc());
  interp.executeBlock(block, outer);
  EXPECT_EQ(outer.find("y"), nullptr); // block-local, doesn't leak into outer
}

TEST(InterpreterStmt, ExecuteBlockCanReadOuterScope) {
  Interpreter interp = makeInterp();
  Environment outer;
  outer.define("x", Value{int64_t{10}});
  std::vector<StmtPtr> stmts;
  stmts.push_back(
      std::make_unique<AssignStmt>("x", std::make_unique<IntLiteralExpr>(20, loc()), loc()));
  BlockStmt block(std::move(stmts), loc());
  interp.executeBlock(block, outer);
  EXPECT_EQ(std::get<int64_t>(*outer.find("x")), 20); // assignment DOES propagate outward
}

TEST(InterpreterStmt, MultipleStatementsExecuteInOrder) {
  Interpreter interp = makeInterp();
  Environment env;
  std::vector<StmtPtr> stmts;
  stmts.push_back(std::make_unique<LetStmt>("a", TypeName::Int,
                                            std::make_unique<IntLiteralExpr>(1, loc()), loc()));
  stmts.push_back(
      std::make_unique<AssignStmt>("a", std::make_unique<IntLiteralExpr>(2, loc()), loc()));
  BlockStmt block(std::move(stmts), loc());
  interp.executeBlock(block, env);
  // 'a' was block-local (declared with let inside the block), so it
  // shouldn't leak, but the assign should have applied to that local 'a'
  // without error.
  EXPECT_EQ(env.find("a"), nullptr);
}
