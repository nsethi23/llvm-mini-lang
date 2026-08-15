#include "mlang/ast/Decl.h"
#include "mlang/sema/Sema.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
SourceLocation loc(int line = 1, int col = 1) {
  return {line, col};
}

Sema makeSema() {
  static Program program;
  return Sema(program);
}

FunctionDecl intFn(const std::string& name = "f") {
  return FunctionDecl{name, {}, TypeName::Int, nullptr, loc()};
}
} // namespace

TEST(SemaStmt, LetDefinesVariableWithDeclaredType) {
  Sema sema = makeSema();
  Scope scope;
  FunctionDecl fn = intFn();
  LetStmt let("x", TypeName::Int, std::make_unique<IntLiteralExpr>(5, loc()), loc());
  sema.checkStmt(let, scope, fn);
  ASSERT_NE(scope.find("x"), nullptr);
  EXPECT_EQ(*scope.find("x"), SemaType::Int);
  EXPECT_TRUE(sema.diagnostics().empty());
}

TEST(SemaStmt, LetWithMismatchedInitTypeIsError) {
  Sema sema = makeSema();
  Scope scope;
  FunctionDecl fn = intFn();
  LetStmt let("x", TypeName::Int, std::make_unique<BoolLiteralExpr>(true, loc()), loc());
  sema.checkStmt(let, scope, fn);
  EXPECT_EQ(sema.diagnostics().size(), 1u);
  // still defines 'x' as its declared type so later uses don't cascade
  ASSERT_NE(scope.find("x"), nullptr);
  EXPECT_EQ(*scope.find("x"), SemaType::Int);
}

TEST(SemaStmt, AssignToExistingVariableOfMatchingTypeOk) {
  Sema sema = makeSema();
  Scope scope;
  FunctionDecl fn = intFn();
  scope.define("x", SemaType::Int);
  AssignStmt assign("x", std::make_unique<IntLiteralExpr>(9, loc()), loc());
  sema.checkStmt(assign, scope, fn);
  EXPECT_TRUE(sema.diagnostics().empty());
}

TEST(SemaStmt, AssignTypeMismatchIsError) {
  Sema sema = makeSema();
  Scope scope;
  FunctionDecl fn = intFn();
  scope.define("x", SemaType::Int);
  AssignStmt assign("x", std::make_unique<BoolLiteralExpr>(true, loc()), loc());
  sema.checkStmt(assign, scope, fn);
  EXPECT_EQ(sema.diagnostics().size(), 1u);
}

TEST(SemaStmt, AssignToUndefinedVariableIsError) {
  Sema sema = makeSema();
  Scope scope;
  FunctionDecl fn = intFn();
  AssignStmt assign("missing", std::make_unique<IntLiteralExpr>(9, loc()), loc());
  sema.checkStmt(assign, scope, fn);
  ASSERT_EQ(sema.diagnostics().size(), 1u);
  EXPECT_EQ(sema.diagnostics()[0].message, "undefined variable 'missing' in assignment");
}

TEST(SemaStmt, ReturnMatchingFunctionTypeOk) {
  Sema sema = makeSema();
  Scope scope;
  FunctionDecl fn = intFn();
  ReturnStmt ret(std::make_unique<IntLiteralExpr>(7, loc()), loc());
  sema.checkStmt(ret, scope, fn);
  EXPECT_TRUE(sema.diagnostics().empty());
}

TEST(SemaStmt, ReturnWrongTypeIsError) {
  Sema sema = makeSema();
  Scope scope;
  FunctionDecl fn = intFn();
  ReturnStmt ret(std::make_unique<BoolLiteralExpr>(true, loc()), loc());
  sema.checkStmt(ret, scope, fn);
  EXPECT_EQ(sema.diagnostics().size(), 1u);
}

TEST(SemaStmt, BareReturnIsError) {
  Sema sema = makeSema();
  Scope scope;
  FunctionDecl fn = intFn();
  ReturnStmt ret(nullptr, loc());
  sema.checkStmt(ret, scope, fn);
  ASSERT_EQ(sema.diagnostics().size(), 1u);
  EXPECT_EQ(sema.diagnostics()[0].message, "missing return value; 'f' returns int");
}

TEST(SemaStmt, IfConditionMustBeBool) {
  Sema sema = makeSema();
  Scope scope;
  FunctionDecl fn = intFn();
  IfStmt ifs(std::make_unique<IntLiteralExpr>(1, loc()),
             std::make_unique<BlockStmt>(std::vector<StmtPtr>{}, loc()), nullptr, loc());
  sema.checkStmt(ifs, scope, fn);
  EXPECT_EQ(sema.diagnostics().size(), 1u);
}

TEST(SemaStmt, WhileConditionMustBeBool) {
  Sema sema = makeSema();
  Scope scope;
  FunctionDecl fn = intFn();
  WhileStmt whileStmt(std::make_unique<IntLiteralExpr>(1, loc()),
                       std::make_unique<BlockStmt>(std::vector<StmtPtr>{}, loc()), loc());
  sema.checkStmt(whileStmt, scope, fn);
  EXPECT_EQ(sema.diagnostics().size(), 1u);
}

TEST(SemaStmt, BlockCreatesChildScopeThatDoesNotLeakOut) {
  Sema sema = makeSema();
  Scope outer;
  FunctionDecl fn = intFn();
  std::vector<StmtPtr> stmts;
  stmts.push_back(std::make_unique<LetStmt>("y", TypeName::Int,
                                            std::make_unique<IntLiteralExpr>(3, loc()), loc()));
  BlockStmt block(std::move(stmts), loc());
  sema.checkBlock(block, outer, fn);
  EXPECT_EQ(outer.find("y"), nullptr);
  EXPECT_TRUE(sema.diagnostics().empty());
}

TEST(SemaStmt, BlockCanReadOuterScope) {
  Sema sema = makeSema();
  Scope outer;
  FunctionDecl fn = intFn();
  outer.define("x", SemaType::Int);
  std::vector<StmtPtr> stmts;
  stmts.push_back(
      std::make_unique<AssignStmt>("x", std::make_unique<IntLiteralExpr>(20, loc()), loc()));
  BlockStmt block(std::move(stmts), loc());
  sema.checkBlock(block, outer, fn);
  EXPECT_TRUE(sema.diagnostics().empty());
}
