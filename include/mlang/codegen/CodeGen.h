// LLVM IR codegen: walks a well-typed AST (already accepted by Sema, see
// mlang/sema/Sema.h) and emits LLVM IR via IRBuilder. Mirrors Interpreter's
// public shape (per-expression/per-statement/whole-program generation) but
// targets LLVM values instead of the tree-walking Value variant (PRD.md M5).
#ifndef MLANG_CODEGEN_CODEGEN_H
#define MLANG_CODEGEN_CODEGEN_H

#include "mlang/ast/Decl.h"
#include "mlang/ast/Expr.h"
#include "mlang/ast/Stmt.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mlang {

class CodeGen {
public:
  CodeGen(const Program& program, llvm::LLVMContext& ctx, std::string moduleName = "mlang");

  llvm::Module& module() {
    return *module_;
  }
  llvm::IRBuilder<>& builder() {
    return *builder_;
  }

  // Transfers ownership of the generated module to the caller -- used by
  // the JIT driver (PRD.md M7), which must hand LLVM's ORC layer a module
  // it owns outright. Leaves this CodeGen without a module; only call after
  // generate() and only once.
  std::unique_ptr<llvm::Module> releaseModule() {
    return std::move(module_);
  }

  // Generates a single expression's IR at the builder's current insertion
  // point, returning the produced value. Public so expression codegen is
  // independently testable, matching Interpreter::evaluate's and
  // Sema::checkExpr's shape -- callers position the builder in a function
  // of their own (see tests/unit/codegen for the pattern).
  llvm::Value* genExpr(const Expr& expr);

  // Generates a single statement at the builder's current insertion point.
  // Public for the same reason as genExpr.
  void genStmt(const Stmt& stmt);

  // Generates a block in a fresh child scope of the current scope, matching
  // Interpreter::executeBlock's and Sema::checkBlock's scoping.
  void genBlock(const BlockStmt& block);

  // Opens/closes a lexical scope for local variables. Exposed so tests can
  // declare locals directly (see declareLocal) without going through a
  // full function body.
  void beginScope();
  void endScope();

  // Introduces `name` as a local bound to `alloca` in the current (innermost)
  // scope, shadowing any outer binding with the same name -- mirrors
  // Environment::define/Scope::define.
  void declareLocal(const std::string& name, llvm::AllocaInst* alloca, TypeName type);

  // Generates the whole program into the module: declares every function's
  // signature first (so calls resolve regardless of definition order), then
  // emits each body. Does not verify the module -- callers run
  // llvm::verifyModule themselves so they control how/where the resulting
  // diagnostic text is reported (see main.cpp's --emit-llvm).
  void generate();

private:
  // A local variable's storage plus its static type, needed to pick the
  // right LLVM instruction (int vs. float vs. bool) when the variable is
  // later read or assigned. Mirrors sema/Scope.h's name->type map, but
  // keyed to an alloca instead of a SemaType.
  struct Local {
    llvm::AllocaInst* alloca;
    TypeName type;
  };

  llvm::Type* llvmType(TypeName type) const;

  // The static type of `expr`, assuming the program already passed Sema
  // (see mlang/sema/Sema.h::checkExpr, which this mirrors without the
  // diagnostic/error-recovery machinery -- codegen never runs on an
  // ill-typed program). Needed to pick int/float/bool instruction variants
  // and the right print() runtime helper.
  TypeName exprType(const Expr& expr) const;

  llvm::Value* genUnary(const UnaryExpr& expr);
  llvm::Value* genBinary(const BinaryExpr& expr);
  llvm::Value* genCast(const CastExpr& expr);
  llvm::Value* genCall(const CallExpr& expr);
  llvm::Value* genPrintCall(const CallExpr& call);

  // Looks up (declaring on first use) an external `void @name(argType)`
  // runtime function -- print() lowers to a call to one of these rather
  // than inlining formatting logic into every call site. Only declared
  // here (M5); defined and linked in by the JIT driver in M6.
  llvm::FunctionCallee runtimePrintFn(const std::string& name, llvm::Type* argType);

  llvm::AllocaInst* createEntryAlloca(llvm::Function* fn, const std::string& name,
                                      llvm::Type* type);

  llvm::Function* declareFunction(const FunctionDecl& fn);
  void genFunction(const FunctionDecl& fn, llvm::Function* F);

  const Local* findLocal(const std::string& name) const;

  const Program& program_;
  llvm::LLVMContext& ctx_;
  std::string moduleName_;
  std::unique_ptr<llvm::Module> module_;
  std::unique_ptr<llvm::IRBuilder<>> builder_;
  std::unordered_map<std::string, llvm::Function*> functions_;
  std::unordered_map<std::string, const FunctionDecl*> functionDecls_;
  std::vector<std::unordered_map<std::string, Local>> scopes_;
};

} // namespace mlang

#endif // MLANG_CODEGEN_CODEGEN_H
