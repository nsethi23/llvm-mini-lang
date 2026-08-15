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

// A CodeGen wired to a scratch `test()` function with its builder already
// positioned at the entry block, so genExpr can be exercised standalone --
// mirrors Interpreter::evaluate's `eval(expr, env)` test helper shape.
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

// IRBuilder constant-folds arithmetic/comparisons over two literal
// constants (e.g. `1 + 2` becomes the ConstantInt `3`, never an `add`
// instruction), so tests that need a real instruction in the output route
// one operand through a declared local instead of a second literal.
IdentifierExpr declareVar(ScratchFn& s, const std::string& name, TypeName type,
                          llvm::Constant* init) {
  llvm::Type* t = type == TypeName::Int     ? llvm::Type::getInt64Ty(s.ctx)
                  : type == TypeName::Float ? llvm::Type::getDoubleTy(s.ctx)
                                            : llvm::Type::getInt1Ty(s.ctx);
  llvm::AllocaInst* alloca = s.cg.builder().CreateAlloca(t, nullptr, name);
  s.cg.builder().CreateStore(init, alloca);
  s.cg.declareLocal(name, alloca, type);
  return IdentifierExpr(name, loc());
}
} // namespace

TEST(CodeGenExpr, GeneratesIntLiteral) {
  ScratchFn s;
  auto* c = llvm::dyn_cast<llvm::ConstantInt>(s.cg.genExpr(IntLiteralExpr(42, loc())));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSExtValue(), 42);
}

TEST(CodeGenExpr, GeneratesFloatLiteral) {
  ScratchFn s;
  auto* c = llvm::dyn_cast<llvm::ConstantFP>(s.cg.genExpr(FloatLiteralExpr(3.5, loc())));
  ASSERT_NE(c, nullptr);
  EXPECT_DOUBLE_EQ(c->getValueAPF().convertToDouble(), 3.5);
}

TEST(CodeGenExpr, GeneratesBoolLiteral) {
  ScratchFn s;
  auto* c = llvm::dyn_cast<llvm::ConstantInt>(s.cg.genExpr(BoolLiteralExpr(true, loc())));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getZExtValue(), 1u);
  EXPECT_TRUE(c->getType()->isIntegerTy(1));
}

TEST(CodeGenExpr, GeneratesIdentifierAsLoad) {
  ScratchFn s;
  s.cg.beginScope();
  llvm::AllocaInst* alloca =
      s.cg.builder().CreateAlloca(llvm::Type::getInt64Ty(s.ctx), nullptr, "x");
  s.cg.builder().CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(s.ctx), 7), alloca);
  s.cg.declareLocal("x", alloca, TypeName::Int);

  llvm::Value* v = s.cg.genExpr(IdentifierExpr("x", loc()));
  auto* load = llvm::dyn_cast<llvm::LoadInst>(v);
  ASSERT_NE(load, nullptr);
  EXPECT_EQ(load->getPointerOperand(), alloca);
  s.cg.endScope();
}

TEST(CodeGenExpr, GeneratesIntNegation) {
  ScratchFn s;
  s.cg.beginScope();
  IdentifierExpr x =
      declareVar(s, "x", TypeName::Int, llvm::ConstantInt::get(llvm::Type::getInt64Ty(s.ctx), 5));
  UnaryExpr neg(UnaryOp::Neg, std::make_unique<IdentifierExpr>(x), loc());
  llvm::Value* v = s.cg.genExpr(neg);
  ASSERT_TRUE(llvm::isa<llvm::BinaryOperator>(v));
  EXPECT_EQ(llvm::cast<llvm::BinaryOperator>(v)->getOpcode(), llvm::Instruction::Sub);
  s.cg.endScope();
}

TEST(CodeGenExpr, GeneratesFloatNegationAsFNeg) {
  ScratchFn s;
  s.cg.beginScope();
  IdentifierExpr x = declareVar(s, "x", TypeName::Float,
                                llvm::ConstantFP::get(llvm::Type::getDoubleTy(s.ctx), 5.0));
  UnaryExpr neg(UnaryOp::Neg, std::make_unique<IdentifierExpr>(x), loc());
  llvm::Value* v = s.cg.genExpr(neg);
  EXPECT_TRUE(llvm::isa<llvm::UnaryOperator>(v));
  s.cg.endScope();
}

TEST(CodeGenExpr, GeneratesBoolNot) {
  ScratchFn s;
  s.cg.beginScope();
  IdentifierExpr b =
      declareVar(s, "b", TypeName::Bool, llvm::ConstantInt::get(llvm::Type::getInt1Ty(s.ctx), 1));
  UnaryExpr notExpr(UnaryOp::Not, std::make_unique<IdentifierExpr>(b), loc());
  llvm::Value* v = s.cg.genExpr(notExpr);
  ASSERT_TRUE(llvm::isa<llvm::BinaryOperator>(v));
  EXPECT_EQ(llvm::cast<llvm::BinaryOperator>(v)->getOpcode(), llvm::Instruction::Xor);
  s.cg.endScope();
}

TEST(CodeGenExpr, GeneratesIntAddAsAdd) {
  ScratchFn s;
  s.cg.beginScope();
  IdentifierExpr x =
      declareVar(s, "x", TypeName::Int, llvm::ConstantInt::get(llvm::Type::getInt64Ty(s.ctx), 1));
  BinaryExpr add(BinaryOp::Add, std::make_unique<IdentifierExpr>(x),
                 std::make_unique<IntLiteralExpr>(2, loc()), loc());
  llvm::Value* v = s.cg.genExpr(add);
  ASSERT_TRUE(llvm::isa<llvm::BinaryOperator>(v));
  EXPECT_EQ(llvm::cast<llvm::BinaryOperator>(v)->getOpcode(), llvm::Instruction::Add);
  s.cg.endScope();
}

TEST(CodeGenExpr, GeneratesFloatAddAsFAdd) {
  ScratchFn s;
  s.cg.beginScope();
  IdentifierExpr x = declareVar(s, "x", TypeName::Float,
                                llvm::ConstantFP::get(llvm::Type::getDoubleTy(s.ctx), 1.0));
  BinaryExpr add(BinaryOp::Add, std::make_unique<IdentifierExpr>(x),
                 std::make_unique<FloatLiteralExpr>(2.0, loc()), loc());
  llvm::Value* v = s.cg.genExpr(add);
  ASSERT_TRUE(llvm::isa<llvm::BinaryOperator>(v));
  EXPECT_EQ(llvm::cast<llvm::BinaryOperator>(v)->getOpcode(), llvm::Instruction::FAdd);
  s.cg.endScope();
}

TEST(CodeGenExpr, GeneratesIntComparisonAsICmp) {
  ScratchFn s;
  s.cg.beginScope();
  IdentifierExpr x =
      declareVar(s, "x", TypeName::Int, llvm::ConstantInt::get(llvm::Type::getInt64Ty(s.ctx), 1));
  BinaryExpr lt(BinaryOp::Lt, std::make_unique<IdentifierExpr>(x),
                std::make_unique<IntLiteralExpr>(2, loc()), loc());
  llvm::Value* v = s.cg.genExpr(lt);
  auto* icmp = llvm::dyn_cast<llvm::ICmpInst>(v);
  ASSERT_NE(icmp, nullptr);
  EXPECT_EQ(icmp->getPredicate(), llvm::ICmpInst::ICMP_SLT);
  s.cg.endScope();
}

TEST(CodeGenExpr, GeneratesFloatComparisonAsFCmp) {
  ScratchFn s;
  s.cg.beginScope();
  IdentifierExpr x = declareVar(s, "x", TypeName::Float,
                                llvm::ConstantFP::get(llvm::Type::getDoubleTy(s.ctx), 1.0));
  BinaryExpr lt(BinaryOp::Lt, std::make_unique<IdentifierExpr>(x),
                std::make_unique<FloatLiteralExpr>(2.0, loc()), loc());
  llvm::Value* v = s.cg.genExpr(lt);
  EXPECT_TRUE(llvm::isa<llvm::FCmpInst>(v));
  s.cg.endScope();
}

TEST(CodeGenExpr, ShortCircuitAndCreatesRhsAndMergeBlocks) {
  ScratchFn s;
  BinaryExpr andExpr(BinaryOp::And, std::make_unique<BoolLiteralExpr>(false, loc()),
                     std::make_unique<BoolLiteralExpr>(true, loc()), loc());
  llvm::Value* v = s.cg.genExpr(andExpr);
  EXPECT_TRUE(llvm::isa<llvm::LoadInst>(v));
  EXPECT_EQ(s.fn->size(), 3u); // entry, rhs, logiccont
}

TEST(CodeGenExpr, ShortCircuitOrCreatesRhsAndMergeBlocks) {
  ScratchFn s;
  BinaryExpr orExpr(BinaryOp::Or, std::make_unique<BoolLiteralExpr>(true, loc()),
                    std::make_unique<BoolLiteralExpr>(false, loc()), loc());
  llvm::Value* v = s.cg.genExpr(orExpr);
  EXPECT_TRUE(llvm::isa<llvm::LoadInst>(v));
  EXPECT_EQ(s.fn->size(), 3u);
}

TEST(CodeGenExpr, GeneratesIntToFloatCast) {
  ScratchFn s;
  s.cg.beginScope();
  IdentifierExpr x =
      declareVar(s, "x", TypeName::Int, llvm::ConstantInt::get(llvm::Type::getInt64Ty(s.ctx), 3));
  CastExpr cast(std::make_unique<IdentifierExpr>(x), TypeName::Float, loc());
  llvm::Value* v = s.cg.genExpr(cast);
  EXPECT_TRUE(llvm::isa<llvm::SIToFPInst>(v));
  s.cg.endScope();
}

TEST(CodeGenExpr, GeneratesFloatToIntCast) {
  ScratchFn s;
  s.cg.beginScope();
  IdentifierExpr x = declareVar(s, "x", TypeName::Float,
                                llvm::ConstantFP::get(llvm::Type::getDoubleTy(s.ctx), 3.9));
  CastExpr cast(std::make_unique<IdentifierExpr>(x), TypeName::Int, loc());
  llvm::Value* v = s.cg.genExpr(cast);
  EXPECT_TRUE(llvm::isa<llvm::FPToSIInst>(v));
  s.cg.endScope();
}

TEST(CodeGenExpr, IdentityCastIsANoOp) {
  ScratchFn s;
  auto operand = std::make_unique<IntLiteralExpr>(3, loc());
  llvm::Value* operandValue = s.cg.genExpr(*operand);
  CastExpr cast(std::move(operand), TypeName::Int, loc());
  // exprType/genExpr recompute independently, so just check no cast instruction
  // was needed by re-running genExpr on a fresh identical literal.
  llvm::Value* v = s.cg.genExpr(cast);
  EXPECT_TRUE(llvm::isa<llvm::ConstantInt>(v));
  (void)operandValue;
}

TEST(CodeGenExpr, PrintIntCallsRuntimeHelperAndReturnsZero) {
  ScratchFn s;
  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<IntLiteralExpr>(5, loc()));
  CallExpr call("print", std::move(args), loc());

  llvm::Value* v = s.cg.genExpr(call);
  ASSERT_TRUE(llvm::isa<llvm::ConstantInt>(v));
  EXPECT_EQ(llvm::cast<llvm::ConstantInt>(v)->getSExtValue(), 0);

  llvm::Function* helper = s.cg.module().getFunction("mlang_print_int");
  ASSERT_NE(helper, nullptr);
  EXPECT_TRUE(helper->isDeclaration());
}

TEST(CodeGenExpr, PrintStringCallsRuntimeHelper) {
  ScratchFn s;
  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<StringLiteralExpr>("hi", loc()));
  CallExpr call("print", std::move(args), loc());

  s.cg.genExpr(call);
  llvm::Function* helper = s.cg.module().getFunction("mlang_print_str");
  ASSERT_NE(helper, nullptr);
}

TEST(CodeGenExpr, PrintFloatCallsRuntimeHelper) {
  ScratchFn s;
  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<FloatLiteralExpr>(1.5, loc()));
  CallExpr call("print", std::move(args), loc());

  s.cg.genExpr(call);
  ASSERT_NE(s.cg.module().getFunction("mlang_print_float"), nullptr);
}

TEST(CodeGenExpr, PrintBoolCallsRuntimeHelper) {
  ScratchFn s;
  std::vector<ExprPtr> args;
  args.push_back(std::make_unique<BoolLiteralExpr>(true, loc()));
  CallExpr call("print", std::move(args), loc());

  s.cg.genExpr(call);
  ASSERT_NE(s.cg.module().getFunction("mlang_print_bool"), nullptr);
}
