#include "mlang/ast/Decl.h"
#include "mlang/sema/Sema.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
SourceLocation loc(int line = 1, int col = 1) {
  return {line, col};
}

std::unique_ptr<BlockStmt> blockOf(std::vector<StmtPtr> stmts) {
  return std::make_unique<BlockStmt>(std::move(stmts), loc());
}

std::unique_ptr<BlockStmt> returning(TypeName type, ExprPtr value) {
  std::vector<StmtPtr> stmts;
  stmts.push_back(std::make_unique<ReturnStmt>(std::move(value), loc()));
  return blockOf(std::move(stmts));
}
} // namespace

TEST(SemaProgram, WellTypedProgramChecksClean) {
  Program program;
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  main.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  main.loc = loc();
  program.functions.push_back(std::move(main));

  Sema sema(program);
  EXPECT_TRUE(sema.check());
  EXPECT_TRUE(sema.diagnostics().empty());
}

TEST(SemaProgram, MissingMainIsError) {
  Program program;
  FunctionDecl fn;
  fn.name = "helper";
  fn.returnType = TypeName::Int;
  fn.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  fn.loc = loc();
  program.functions.push_back(std::move(fn));

  Sema sema(program);
  EXPECT_FALSE(sema.check());
  bool foundMissingMain = false;
  for (const Diagnostic& d : sema.diagnostics())
    if (d.message == "no 'main' function defined")
      foundMissingMain = true;
  EXPECT_TRUE(foundMissingMain);
}

TEST(SemaProgram, MainWithParamsIsError) {
  Program program;
  FunctionDecl main;
  main.name = "main";
  main.params.push_back(Param{"x", TypeName::Int, loc()});
  main.returnType = TypeName::Int;
  main.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  main.loc = loc();
  program.functions.push_back(std::move(main));

  Sema sema(program);
  EXPECT_FALSE(sema.check());
}

TEST(SemaProgram, MainReturningNonIntIsError) {
  Program program;
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Bool;
  main.body = returning(TypeName::Bool, std::make_unique<BoolLiteralExpr>(true, loc()));
  main.loc = loc();
  program.functions.push_back(std::move(main));

  Sema sema(program);
  EXPECT_FALSE(sema.check());
}

TEST(SemaProgram, DuplicateFunctionNameIsError) {
  Program program;
  for (int i = 0; i < 2; i++) {
    FunctionDecl fn;
    fn.name = "dup";
    fn.returnType = TypeName::Int;
    fn.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
    fn.loc = loc();
    program.functions.push_back(std::move(fn));
  }
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  main.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  main.loc = loc();
  program.functions.push_back(std::move(main));

  Sema sema(program);
  EXPECT_FALSE(sema.check());
  bool foundDup = false;
  for (const Diagnostic& d : sema.diagnostics())
    if (d.message == "function 'dup' is already defined")
      foundDup = true;
  EXPECT_TRUE(foundDup);
}

TEST(SemaProgram, RedefiningPrintIsError) {
  Program program;
  FunctionDecl fn;
  fn.name = "print";
  fn.returnType = TypeName::Int;
  fn.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  fn.loc = loc();
  program.functions.push_back(std::move(fn));
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  main.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  main.loc = loc();
  program.functions.push_back(std::move(main));

  Sema sema(program);
  EXPECT_FALSE(sema.check());
}

TEST(SemaProgram, DuplicateParamNameIsError) {
  Program program;
  FunctionDecl fn;
  fn.name = "f";
  fn.params.push_back(Param{"a", TypeName::Int, loc()});
  fn.params.push_back(Param{"a", TypeName::Int, loc()});
  fn.returnType = TypeName::Int;
  fn.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  fn.loc = loc();
  program.functions.push_back(std::move(fn));
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  main.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  main.loc = loc();
  program.functions.push_back(std::move(main));

  Sema sema(program);
  EXPECT_FALSE(sema.check());
}

TEST(SemaProgram, FunctionMissingReturnOnAllPathsIsError) {
  Program program;
  FunctionDecl fn;
  fn.name = "f";
  fn.returnType = TypeName::Int;
  fn.body = blockOf({}); // falls off the end without returning
  fn.loc = loc();
  program.functions.push_back(std::move(fn));
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  main.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  main.loc = loc();
  program.functions.push_back(std::move(main));

  Sema sema(program);
  EXPECT_FALSE(sema.check());
}

TEST(SemaProgram, IfElseWhereBothBranchesReturnSatisfiesAllPaths) {
  Program program;
  FunctionDecl fn;
  fn.name = "f";
  fn.returnType = TypeName::Int;
  std::vector<StmtPtr> stmts;
  auto thenBlock = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(1, loc()));
  auto elseBlock = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(2, loc()));
  stmts.push_back(std::make_unique<IfStmt>(std::make_unique<BoolLiteralExpr>(true, loc()),
                                            std::move(thenBlock), std::move(elseBlock), loc()));
  fn.body = blockOf(std::move(stmts));
  fn.loc = loc();
  program.functions.push_back(std::move(fn));
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  main.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  main.loc = loc();
  program.functions.push_back(std::move(main));

  Sema sema(program);
  EXPECT_TRUE(sema.check());
}

TEST(SemaProgram, IfWithoutElseDoesNotSatisfyAllPaths) {
  Program program;
  FunctionDecl fn;
  fn.name = "f";
  fn.returnType = TypeName::Int;
  std::vector<StmtPtr> stmts;
  auto thenBlock = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(1, loc()));
  stmts.push_back(std::make_unique<IfStmt>(std::make_unique<BoolLiteralExpr>(true, loc()),
                                            std::move(thenBlock), nullptr, loc()));
  fn.body = blockOf(std::move(stmts));
  fn.loc = loc();
  program.functions.push_back(std::move(fn));
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  main.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  main.loc = loc();
  program.functions.push_back(std::move(main));

  Sema sema(program);
  EXPECT_FALSE(sema.check());
}

TEST(SemaProgram, WhileLoopNeverSatisfiesAllPaths) {
  Program program;
  FunctionDecl fn;
  fn.name = "f";
  fn.returnType = TypeName::Int;
  std::vector<StmtPtr> stmts;
  auto body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(1, loc()));
  stmts.push_back(
      std::make_unique<WhileStmt>(std::make_unique<BoolLiteralExpr>(true, loc()), std::move(body), loc()));
  fn.body = blockOf(std::move(stmts));
  fn.loc = loc();
  program.functions.push_back(std::move(fn));
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  main.body = returning(TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()));
  main.loc = loc();
  program.functions.push_back(std::move(main));

  Sema sema(program);
  EXPECT_FALSE(sema.check());
}
