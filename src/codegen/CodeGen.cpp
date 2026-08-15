#include "mlang/codegen/CodeGen.h"

#include "llvm/IR/Constants.h"
#include "llvm/Support/ErrorHandling.h"

#include <cassert>

namespace mlang {

CodeGen::CodeGen(const Program& program, llvm::LLVMContext& ctx, std::string moduleName)
    : program_(program), ctx_(ctx), moduleName_(std::move(moduleName)) {
  module_ = std::make_unique<llvm::Module>(moduleName_, ctx_);
  builder_ = std::make_unique<llvm::IRBuilder<>>(ctx_);
  for (const FunctionDecl& fn : program_.functions)
    functionDecls_[fn.name] = &fn;
}

llvm::Type* CodeGen::llvmType(TypeName type) const {
  switch (type) {
  case TypeName::Int:
    return llvm::Type::getInt64Ty(ctx_);
  case TypeName::Float:
    return llvm::Type::getDoubleTy(ctx_);
  case TypeName::Bool:
    return llvm::Type::getInt1Ty(ctx_);
  }
  llvm_unreachable("unhandled TypeName");
}

TypeName CodeGen::exprType(const Expr& expr) const {
  switch (expr.kind) {
  case ExprKind::IntLiteral:
    return TypeName::Int;
  case ExprKind::FloatLiteral:
    return TypeName::Float;
  case ExprKind::BoolLiteral:
    return TypeName::Bool;
  case ExprKind::StringLiteral:
    llvm_unreachable("string literals only appear as print()'s immediate argument");
  case ExprKind::Identifier: {
    const auto& id = static_cast<const IdentifierExpr&>(expr);
    const Local* local = findLocal(id.name);
    assert(local && "undefined identifier reached codegen -- sema should have rejected this");
    return local->type;
  }
  case ExprKind::Unary: {
    const auto& u = static_cast<const UnaryExpr&>(expr);
    return u.op == UnaryOp::Not ? TypeName::Bool : exprType(*u.operand);
  }
  case ExprKind::Binary: {
    const auto& b = static_cast<const BinaryExpr&>(expr);
    switch (b.op) {
    case BinaryOp::Eq:
    case BinaryOp::Ne:
    case BinaryOp::Lt:
    case BinaryOp::Le:
    case BinaryOp::Gt:
    case BinaryOp::Ge:
    case BinaryOp::And:
    case BinaryOp::Or:
      return TypeName::Bool;
    default:
      return exprType(*b.lhs);
    }
  }
  case ExprKind::Call: {
    const auto& call = static_cast<const CallExpr&>(expr);
    if (call.callee == "print")
      return TypeName::Int; // matches Interpreter::callBuiltin's `print` return value
    auto it = functionDecls_.find(call.callee);
    assert(it != functionDecls_.end() && "undefined function reached codegen");
    return it->second->returnType;
  }
  case ExprKind::Cast:
    return static_cast<const CastExpr&>(expr).targetType;
  }
  llvm_unreachable("unhandled ExprKind");
}

const CodeGen::Local* CodeGen::findLocal(const std::string& name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end())
      return &found->second;
  }
  return nullptr;
}

void CodeGen::beginScope() {
  scopes_.emplace_back();
}

void CodeGen::endScope() {
  scopes_.pop_back();
}

void CodeGen::declareLocal(const std::string& name, llvm::AllocaInst* alloca, TypeName type) {
  scopes_.back()[name] = Local{alloca, type};
}

llvm::Value* CodeGen::genExpr(const Expr& expr) {
  switch (expr.kind) {
  case ExprKind::IntLiteral:
    return llvm::ConstantInt::get(llvmType(TypeName::Int),
                                   static_cast<const IntLiteralExpr&>(expr).value, true);
  case ExprKind::FloatLiteral:
    return llvm::ConstantFP::get(llvmType(TypeName::Float),
                                  static_cast<const FloatLiteralExpr&>(expr).value);
  case ExprKind::BoolLiteral:
    return llvm::ConstantInt::get(llvmType(TypeName::Bool),
                                   static_cast<const BoolLiteralExpr&>(expr).value ? 1 : 0);
  case ExprKind::StringLiteral:
    return builder_->CreateGlobalString(static_cast<const StringLiteralExpr&>(expr).value, "str");
  case ExprKind::Identifier: {
    const auto& id = static_cast<const IdentifierExpr&>(expr);
    const Local* local = findLocal(id.name);
    assert(local && "undefined identifier reached codegen -- sema should have rejected this");
    return builder_->CreateLoad(llvmType(local->type), local->alloca, id.name);
  }
  case ExprKind::Unary:
    return genUnary(static_cast<const UnaryExpr&>(expr));
  case ExprKind::Binary:
    return genBinary(static_cast<const BinaryExpr&>(expr));
  case ExprKind::Call:
    return genCall(static_cast<const CallExpr&>(expr));
  case ExprKind::Cast:
    return genCast(static_cast<const CastExpr&>(expr));
  }
  llvm_unreachable("unhandled ExprKind");
}

llvm::Value* CodeGen::genUnary(const UnaryExpr& expr) {
  llvm::Value* operand = genExpr(*expr.operand);
  if (expr.op == UnaryOp::Not)
    return builder_->CreateNot(operand, "nottmp");
  if (exprType(*expr.operand) == TypeName::Float)
    return builder_->CreateFNeg(operand, "negtmp");
  return builder_->CreateNeg(operand, "negtmp");
}

llvm::Value* CodeGen::genBinary(const BinaryExpr& expr) {
  if (expr.op == BinaryOp::And || expr.op == BinaryOp::Or) {
    llvm::Function* fn = builder_->GetInsertBlock()->getParent();
    llvm::Value* lhs = genExpr(*expr.lhs);
    llvm::AllocaInst* result = createEntryAlloca(fn, "logictmp", llvmType(TypeName::Bool));
    builder_->CreateStore(lhs, result);

    llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(ctx_, "rhs", fn);
    llvm::BasicBlock* contBB = llvm::BasicBlock::Create(ctx_, "logiccont", fn);
    // Short-circuit: && skips the rhs when lhs is already false, || skips
    // it when lhs is already true (mirrors Interpreter::evaluate's Binary
    // case, which never evaluates the rhs subexpression in that case).
    if (expr.op == BinaryOp::And)
      builder_->CreateCondBr(lhs, rhsBB, contBB);
    else
      builder_->CreateCondBr(lhs, contBB, rhsBB);

    builder_->SetInsertPoint(rhsBB);
    llvm::Value* rhs = genExpr(*expr.rhs);
    builder_->CreateStore(rhs, result);
    builder_->CreateBr(contBB);

    builder_->SetInsertPoint(contBB);
    return builder_->CreateLoad(llvmType(TypeName::Bool), result, "logicresult");
  }

  llvm::Value* lhs = genExpr(*expr.lhs);
  TypeName lhsType = exprType(*expr.lhs);
  llvm::Value* rhs = genExpr(*expr.rhs);
  bool isFloat = lhsType == TypeName::Float;

  switch (expr.op) {
  case BinaryOp::Add:
    return isFloat ? builder_->CreateFAdd(lhs, rhs, "addtmp")
                   : builder_->CreateAdd(lhs, rhs, "addtmp");
  case BinaryOp::Sub:
    return isFloat ? builder_->CreateFSub(lhs, rhs, "subtmp")
                   : builder_->CreateSub(lhs, rhs, "subtmp");
  case BinaryOp::Mul:
    return isFloat ? builder_->CreateFMul(lhs, rhs, "multmp")
                   : builder_->CreateMul(lhs, rhs, "multmp");
  case BinaryOp::Div:
    return isFloat ? builder_->CreateFDiv(lhs, rhs, "divtmp")
                   : builder_->CreateSDiv(lhs, rhs, "divtmp");
  case BinaryOp::Mod:
    return isFloat ? builder_->CreateFRem(lhs, rhs, "modtmp")
                   : builder_->CreateSRem(lhs, rhs, "modtmp");
  case BinaryOp::Eq:
    return isFloat ? builder_->CreateFCmpOEQ(lhs, rhs, "eqtmp")
                   : builder_->CreateICmpEQ(lhs, rhs, "eqtmp");
  case BinaryOp::Ne:
    return isFloat ? builder_->CreateFCmpONE(lhs, rhs, "netmp")
                   : builder_->CreateICmpNE(lhs, rhs, "netmp");
  case BinaryOp::Lt:
    return isFloat ? builder_->CreateFCmpOLT(lhs, rhs, "lttmp")
                   : builder_->CreateICmpSLT(lhs, rhs, "lttmp");
  case BinaryOp::Le:
    return isFloat ? builder_->CreateFCmpOLE(lhs, rhs, "letmp")
                   : builder_->CreateICmpSLE(lhs, rhs, "letmp");
  case BinaryOp::Gt:
    return isFloat ? builder_->CreateFCmpOGT(lhs, rhs, "gttmp")
                   : builder_->CreateICmpSGT(lhs, rhs, "gttmp");
  case BinaryOp::Ge:
    return isFloat ? builder_->CreateFCmpOGE(lhs, rhs, "getmp")
                   : builder_->CreateICmpSGE(lhs, rhs, "getmp");
  case BinaryOp::And:
  case BinaryOp::Or:
    llvm_unreachable("short-circuit ops handled above");
  }
  llvm_unreachable("unhandled BinaryOp");
}

llvm::Value* CodeGen::genCast(const CastExpr& expr) {
  llvm::Value* operand = genExpr(*expr.operand);
  TypeName from = exprType(*expr.operand);
  TypeName to = expr.targetType;
  if (from == to)
    return operand;
  if (from == TypeName::Int && to == TypeName::Float)
    return builder_->CreateSIToFP(operand, llvmType(TypeName::Float), "sitofp");
  if (from == TypeName::Float && to == TypeName::Int)
    return builder_->CreateFPToSI(operand, llvmType(TypeName::Int), "fptosi");
  llvm_unreachable("unsupported cast reached codegen -- sema should have rejected it");
}

llvm::FunctionCallee CodeGen::runtimePrintFn(const std::string& name, llvm::Type* argType) {
  llvm::FunctionType* fnType = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx_), {argType}, false);
  return module_->getOrInsertFunction(name, fnType);
}

llvm::Value* CodeGen::genPrintCall(const CallExpr& call) {
  const Expr& arg = *call.args[0];
  llvm::Value* result = llvm::ConstantInt::get(llvmType(TypeName::Int), 0);

  if (arg.kind == ExprKind::StringLiteral) {
    llvm::Value* str = genExpr(arg);
    llvm::Type* ptrTy = llvm::PointerType::getUnqual(ctx_);
    builder_->CreateCall(runtimePrintFn("mlang_print_str", ptrTy), {str});
    return result;
  }

  llvm::Value* value = genExpr(arg);
  switch (exprType(arg)) {
  case TypeName::Int:
    builder_->CreateCall(runtimePrintFn("mlang_print_int", llvmType(TypeName::Int)), {value});
    break;
  case TypeName::Float:
    builder_->CreateCall(runtimePrintFn("mlang_print_float", llvmType(TypeName::Float)), {value});
    break;
  case TypeName::Bool:
    builder_->CreateCall(runtimePrintFn("mlang_print_bool", llvmType(TypeName::Bool)), {value});
    break;
  }
  return result;
}

llvm::Value* CodeGen::genCall(const CallExpr& call) {
  if (call.callee == "print")
    return genPrintCall(call);

  auto it = functions_.find(call.callee);
  assert(it != functions_.end() && "undefined function reached codegen");
  std::vector<llvm::Value*> args;
  args.reserve(call.args.size());
  for (const ExprPtr& arg : call.args)
    args.push_back(genExpr(*arg));
  return builder_->CreateCall(it->second, args, "calltmp");
}

void CodeGen::genStmt(const Stmt& stmt) {
  switch (stmt.kind) {
  case StmtKind::Let: {
    const auto& let = static_cast<const LetStmt&>(stmt);
    llvm::Value* init = genExpr(*let.init);
    llvm::Function* fn = builder_->GetInsertBlock()->getParent();
    llvm::AllocaInst* alloca = createEntryAlloca(fn, let.name, llvmType(let.type));
    builder_->CreateStore(init, alloca);
    declareLocal(let.name, alloca, let.type);
    return;
  }
  case StmtKind::Assign: {
    const auto& assign = static_cast<const AssignStmt&>(stmt);
    llvm::Value* value = genExpr(*assign.value);
    const Local* local = findLocal(assign.name);
    assert(local && "undefined variable reached codegen -- sema should have rejected this");
    builder_->CreateStore(value, local->alloca);
    return;
  }
  case StmtKind::Return: {
    const auto& ret = static_cast<const ReturnStmt&>(stmt);
    builder_->CreateRet(genExpr(*ret.value));
    return;
  }
  case StmtKind::Expr:
    genExpr(*static_cast<const ExprStmt&>(stmt).expr);
    return;
  case StmtKind::If: {
    const auto& ifs = static_cast<const IfStmt&>(stmt);
    llvm::Value* cond = genExpr(*ifs.cond);
    llvm::Function* fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(ctx_, "then", fn);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(ctx_, "else", fn);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(ctx_, "ifcont", fn);
    builder_->CreateCondBr(cond, thenBB, elseBB);

    builder_->SetInsertPoint(thenBB);
    genBlock(*ifs.thenBlock);
    if (!builder_->GetInsertBlock()->getTerminator())
      builder_->CreateBr(mergeBB);

    builder_->SetInsertPoint(elseBB);
    if (ifs.elseBlock)
      genBlock(*ifs.elseBlock);
    if (!builder_->GetInsertBlock()->getTerminator())
      builder_->CreateBr(mergeBB);

    builder_->SetInsertPoint(mergeBB);
    return;
  }
  case StmtKind::While: {
    const auto& whileStmt = static_cast<const WhileStmt&>(stmt);
    llvm::Function* fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(ctx_, "whilecond", fn);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(ctx_, "whilebody", fn);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(ctx_, "whileend", fn);

    builder_->CreateBr(condBB);
    builder_->SetInsertPoint(condBB);
    llvm::Value* cond = genExpr(*whileStmt.cond);
    builder_->CreateCondBr(cond, bodyBB, afterBB);

    builder_->SetInsertPoint(bodyBB);
    genBlock(*whileStmt.body);
    if (!builder_->GetInsertBlock()->getTerminator())
      builder_->CreateBr(condBB);

    builder_->SetInsertPoint(afterBB);
    return;
  }
  case StmtKind::Block:
    genBlock(static_cast<const BlockStmt&>(stmt));
    return;
  }
  llvm_unreachable("unhandled StmtKind");
}

void CodeGen::genBlock(const BlockStmt& block) {
  beginScope();
  for (const StmtPtr& stmt : block.stmts)
    genStmt(*stmt);
  endScope();
}

llvm::AllocaInst* CodeGen::createEntryAlloca(llvm::Function* fn, const std::string& name,
                                             llvm::Type* type) {
  llvm::IRBuilder<> entryBuilder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
  return entryBuilder.CreateAlloca(type, nullptr, name);
}

llvm::Function* CodeGen::declareFunction(const FunctionDecl& fn) {
  std::vector<llvm::Type*> paramTypes;
  paramTypes.reserve(fn.params.size());
  for (const Param& p : fn.params)
    paramTypes.push_back(llvmType(p.type));

  llvm::FunctionType* fnType = llvm::FunctionType::get(llvmType(fn.returnType), paramTypes, false);
  llvm::Function* F =
      llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, fn.name, module_.get());

  size_t i = 0;
  for (llvm::Argument& arg : F->args())
    arg.setName(fn.params[i++].name);
  return F;
}

void CodeGen::genFunction(const FunctionDecl& fn, llvm::Function* F) {
  llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx_, "entry", F);
  builder_->SetInsertPoint(entry);

  beginScope();
  size_t i = 0;
  for (llvm::Argument& arg : F->args()) {
    const Param& p = fn.params[i++];
    llvm::AllocaInst* alloca = createEntryAlloca(F, p.name, llvmType(p.type));
    builder_->CreateStore(&arg, alloca);
    declareLocal(p.name, alloca, p.type);
  }

  genBlock(*fn.body);

  // A block can be left without a terminator only when it's provably
  // unreachable (e.g. an if/else merge point where both arms already
  // returned) -- Sema::blockAlwaysReturns guarantees every *reachable*
  // path through a well-typed function ends in a return. `unreachable`
  // closes it out so the module still verifies.
  if (!builder_->GetInsertBlock()->getTerminator())
    builder_->CreateUnreachable();

  endScope();
}

void CodeGen::generate() {
  for (const FunctionDecl& fn : program_.functions)
    functions_[fn.name] = declareFunction(fn);
  for (const FunctionDecl& fn : program_.functions)
    genFunction(fn, functions_[fn.name]);
}

} // namespace mlang
