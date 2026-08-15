#include "mlang/ast/Decl.h"
#include "mlang/codegen/CodeGen.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
SourceLocation loc(int line = 1, int col = 1) {
  return {line, col};
}

std::unique_ptr<BlockStmt> blockOf(std::vector<StmtPtr> stmts) {
  return std::make_unique<BlockStmt>(std::move(stmts), loc());
}

struct ScratchFn {
  llvm::LLVMContext ctx;
  Program program;
  CodeGen cg;
  llvm::Function* fn;

  ScratchFn() : cg(program, ctx) {
    llvm::FunctionType* fnType = llvm::FunctionType::get(llvm::Type::getInt64Ty(ctx), false);
    fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, "test", &cg.module());
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    cg.builder().SetInsertPoint(bb);
  }
};
} // namespace

TEST(CodeGenStmt, LetCreatesLoadableLocal) {
  ScratchFn s;
  s.cg.beginScope();
  s.cg.genStmt(LetStmt("x", TypeName::Int, std::make_unique<IntLiteralExpr>(9, loc()), loc()));

  llvm::Value* v = s.cg.genExpr(IdentifierExpr("x", loc()));
  EXPECT_TRUE(llvm::isa<llvm::LoadInst>(v));
  s.cg.endScope();
}

TEST(CodeGenStmt, AssignStoresNewValue) {
  ScratchFn s;
  s.cg.beginScope();
  s.cg.genStmt(LetStmt("x", TypeName::Int, std::make_unique<IntLiteralExpr>(1, loc()), loc()));
  s.cg.genStmt(AssignStmt("x", std::make_unique<IntLiteralExpr>(2, loc()), loc()));

  llvm::BasicBlock* bb = s.cg.builder().GetInsertBlock();
  ASSERT_TRUE(llvm::isa<llvm::StoreInst>(&bb->back()));
  auto* store = llvm::cast<llvm::StoreInst>(&bb->back());
  EXPECT_EQ(llvm::cast<llvm::ConstantInt>(store->getValueOperand())->getSExtValue(), 2);
  s.cg.endScope();
}

TEST(CodeGenStmt, ReturnCreatesTerminatingRetInstruction) {
  ScratchFn s;
  s.cg.genStmt(ReturnStmt(std::make_unique<IntLiteralExpr>(0, loc()), loc()));

  llvm::Instruction* term = s.cg.builder().GetInsertBlock()->getTerminator();
  ASSERT_NE(term, nullptr);
  EXPECT_TRUE(llvm::isa<llvm::ReturnInst>(term));
}

TEST(CodeGenStmt, NestedBlockShadowsThenOuterIsVisibleAgain) {
  ScratchFn s;
  s.cg.beginScope();
  s.cg.genStmt(LetStmt("x", TypeName::Int, std::make_unique<IntLiteralExpr>(1, loc()), loc()));

  std::vector<StmtPtr> inner;
  inner.push_back(
      std::make_unique<LetStmt>("x", TypeName::Int, std::make_unique<IntLiteralExpr>(2, loc()), loc()));
  s.cg.genStmt(*blockOf(std::move(inner)));

  // After the inner block's scope is popped, "x" resolves to the outer
  // binding again -- matches Environment/Scope's shadow-then-restore
  // semantics (see interpreter/Environment.h, sema/Scope.h).
  llvm::Value* v = s.cg.genExpr(IdentifierExpr("x", loc()));
  EXPECT_TRUE(llvm::isa<llvm::LoadInst>(v));
  s.cg.endScope();
}

TEST(CodeGenStmt, IfCreatesThenElseAndMergeBlocks) {
  ScratchFn s;
  std::vector<StmtPtr> thenStmts;
  thenStmts.push_back(
      std::make_unique<ExprStmt>(std::make_unique<IntLiteralExpr>(1, loc()), loc()));
  std::vector<StmtPtr> elseStmts;
  elseStmts.push_back(
      std::make_unique<ExprStmt>(std::make_unique<IntLiteralExpr>(2, loc()), loc()));

  IfStmt ifs(std::make_unique<BoolLiteralExpr>(true, loc()), blockOf(std::move(thenStmts)),
            blockOf(std::move(elseStmts)), loc());
  s.cg.genStmt(ifs);

  EXPECT_EQ(s.fn->size(), 4u); // entry, then, else, ifcont
}

TEST(CodeGenStmt, IfWithoutElseStillCreatesElseBlockThatFallsThrough) {
  ScratchFn s;
  std::vector<StmtPtr> thenStmts;
  thenStmts.push_back(
      std::make_unique<ExprStmt>(std::make_unique<IntLiteralExpr>(1, loc()), loc()));

  IfStmt ifs(std::make_unique<BoolLiteralExpr>(true, loc()), blockOf(std::move(thenStmts)), nullptr,
            loc());
  s.cg.genStmt(ifs);

  EXPECT_EQ(s.fn->size(), 4u); // entry, then, else (empty, branches to merge), ifcont
}

TEST(CodeGenStmt, WhileCreatesCondBodyAndAfterBlocks) {
  ScratchFn s;
  std::vector<StmtPtr> bodyStmts;
  bodyStmts.push_back(
      std::make_unique<ExprStmt>(std::make_unique<IntLiteralExpr>(1, loc()), loc()));

  WhileStmt whileStmt(std::make_unique<BoolLiteralExpr>(false, loc()), blockOf(std::move(bodyStmts)),
                      loc());
  s.cg.genStmt(whileStmt);

  EXPECT_EQ(s.fn->size(), 4u); // entry, whilecond, whilebody, whileend
}
