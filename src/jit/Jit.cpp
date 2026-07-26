#include "mlang/jit/Jit.h"

#include "mlang/codegen/CodeGen.h"

#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"

#include <cstring>
#include <mutex>

namespace mlang {

namespace {

// print()'s runtime helpers (declared but never defined by CodeGen -- see
// CodeGen::runtimePrintFn) need a concrete definition once code that calls
// them actually runs. Formatting mirrors Value.cpp's valueToString exactly,
// so interpreted and JIT-compiled output are byte-for-byte identical (the
// invariant PRD.md M7's cross-check tests rely on).
llvm::raw_ostream* g_printOut = &llvm::outs();

extern "C" void mlang_print_int(int64_t v) {
  *g_printOut << std::to_string(v) << "\n";
}
extern "C" void mlang_print_float(double v) {
  *g_printOut << std::to_string(v) << "\n";
}
extern "C" void mlang_print_bool(bool v) {
  *g_printOut << (v ? "true" : "false") << "\n";
}
extern "C" void mlang_print_str(const char* s) {
  *g_printOut << s << "\n";
}

// Emits `mlang.entry.<fn.name>`, a wrapper around the already-generated
// `@<fn.name>` with a uniform boxed-argument signature
// `void(i64* argsBits, i64* outBits)`. Every conversion here is chosen from
// `fn`'s statically-known, already-Sema-checked signature, so unlike a
// generic runtime marshaler this needs no runtime type tag.
void generateEntryThunk(llvm::Module& module, const FunctionDecl& fn) {
  llvm::LLVMContext& ctx = module.getContext();
  llvm::IRBuilder<> builder(ctx);

  llvm::Type* i64Ty = llvm::Type::getInt64Ty(ctx);
  llvm::Type* ptrTy = llvm::PointerType::getUnqual(ctx);
  llvm::Type* voidTy = llvm::Type::getVoidTy(ctx);

  llvm::FunctionType* thunkType = llvm::FunctionType::get(voidTy, {ptrTy, ptrTy}, false);
  llvm::Function* thunk = llvm::Function::Create(thunkType, llvm::Function::ExternalLinkage,
                                                 "mlang.entry." + fn.name, &module);
  llvm::Argument* argsBits = thunk->getArg(0);
  llvm::Argument* outBits = thunk->getArg(1);

  llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", thunk);
  builder.SetInsertPoint(entry);

  std::vector<llvm::Value*> callArgs;
  callArgs.reserve(fn.params.size());
  for (size_t i = 0; i < fn.params.size(); i++) {
    llvm::Value* slot =
        builder.CreateConstInBoundsGEP1_64(i64Ty, argsBits, static_cast<uint64_t>(i));
    llvm::Value* bits = builder.CreateLoad(i64Ty, slot, "argbits");

    llvm::Value* native = nullptr;
    switch (fn.params[i].type) {
    case TypeName::Int:
      native = bits;
      break;
    case TypeName::Float:
      native = builder.CreateBitCast(bits, llvm::Type::getDoubleTy(ctx), "argf");
      break;
    case TypeName::Bool:
      native = builder.CreateTrunc(bits, llvm::Type::getInt1Ty(ctx), "argb");
      break;
    }
    callArgs.push_back(native);
  }

  llvm::Function* target = module.getFunction(fn.name);
  llvm::Value* result = builder.CreateCall(target, callArgs, "result");

  llvm::Value* resultBits = nullptr;
  switch (fn.returnType) {
  case TypeName::Int:
    resultBits = result;
    break;
  case TypeName::Float:
    resultBits = builder.CreateBitCast(result, i64Ty, "resultbits");
    break;
  case TypeName::Bool:
    resultBits = builder.CreateZExt(result, i64Ty, "resultbits");
    break;
  }
  builder.CreateStore(resultBits, outBits);
  builder.CreateRetVoid();
}

void registerRuntimeSymbols(llvm::orc::LLJIT& jit) {
  llvm::orc::SymbolMap symbols;
  auto& es = jit.getExecutionSession();
  symbols[es.intern("mlang_print_int")] = {llvm::orc::ExecutorAddr::fromPtr(&mlang_print_int),
                                           llvm::JITSymbolFlags::Exported};
  symbols[es.intern("mlang_print_float")] = {llvm::orc::ExecutorAddr::fromPtr(&mlang_print_float),
                                             llvm::JITSymbolFlags::Exported};
  symbols[es.intern("mlang_print_bool")] = {llvm::orc::ExecutorAddr::fromPtr(&mlang_print_bool),
                                            llvm::JITSymbolFlags::Exported};
  symbols[es.intern("mlang_print_str")] = {llvm::orc::ExecutorAddr::fromPtr(&mlang_print_str),
                                           llvm::JITSymbolFlags::Exported};
  llvm::cantFail(jit.getMainJITDylib().define(llvm::orc::absoluteSymbols(std::move(symbols))));
}

} // namespace

Jit::Jit(const Program& program, llvm::raw_ostream& printOut)
    : program_(program), printOut_(printOut) {}

int64_t Jit::boxValue(const Value& value) {
  if (const auto* i = std::get_if<int64_t>(&value))
    return *i;
  if (const auto* d = std::get_if<double>(&value)) {
    int64_t bits;
    std::memcpy(&bits, d, sizeof(bits));
    return bits;
  }
  if (const auto* b = std::get_if<bool>(&value))
    return *b ? 1 : 0;
  llvm_unreachable("string values never cross the native entry-thunk boundary");
}

Value Jit::unboxValue(int64_t bits, TypeName type) {
  switch (type) {
  case TypeName::Int:
    return Value{bits};
  case TypeName::Float: {
    double d;
    std::memcpy(&d, &bits, sizeof(d));
    return Value{d};
  }
  case TypeName::Bool:
    return Value{bits != 0};
  }
  llvm_unreachable("unhandled TypeName");
}

void Jit::ensureCompiled() {
  if (compiled_)
    return;
  compiled_ = true;

  static std::once_flag nativeTargetInit;
  std::call_once(nativeTargetInit, [] {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
  });

  g_printOut = &printOut_;

  auto ctx = std::make_unique<llvm::LLVMContext>();
  CodeGen codegen(program_, *ctx);
  codegen.generate();
  for (const FunctionDecl& fn : program_.functions)
    generateEntryThunk(codegen.module(), fn);

  std::string verifyErrors;
  llvm::raw_string_ostream verifyOs(verifyErrors);
  if (llvm::verifyModule(codegen.module(), &verifyOs))
    llvm::report_fatal_error(llvm::Twine("mlang: JIT module failed verification:\n") +
                             verifyErrors);

  auto jitOrErr = llvm::orc::LLJITBuilder().create();
  if (!jitOrErr)
    llvm::report_fatal_error(llvm::Twine("mlang: failed to create LLJIT: ") +
                             llvm::toString(jitOrErr.takeError()));
  jit_ = std::move(*jitOrErr);

  registerRuntimeSymbols(*jit_);

  llvm::orc::ThreadSafeModule tsm(codegen.releaseModule(), std::move(ctx));
  if (auto err = jit_->addIRModule(std::move(tsm)))
    llvm::report_fatal_error(llvm::Twine("mlang: failed to add module to JIT: ") +
                             llvm::toString(std::move(err)));
}

Jit::EntryThunk Jit::compileAndLookup(const std::string& name) {
  ensureCompiled();
  auto sym = jit_->lookup("mlang.entry." + name);
  if (!sym)
    llvm::report_fatal_error(llvm::Twine("mlang: JIT lookup failed for '") + name +
                             "': " + llvm::toString(sym.takeError()));
  return sym->toPtr<EntryThunk>();
}

} // namespace mlang
