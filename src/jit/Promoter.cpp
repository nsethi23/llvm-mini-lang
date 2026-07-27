#include "mlang/jit/Promoter.h"

namespace mlang {

Promoter::Promoter(const Program& program, DispatchTable& dispatch, uint64_t threshold,
                   llvm::raw_ostream* traceOut, llvm::raw_ostream& printOut)
    : program_(program), dispatch_(dispatch), threshold_(threshold), traceOut_(traceOut),
      printOut_(printOut) {
  for (const FunctionDecl& fn : program_.functions)
    functionsByName_[fn.name] = &fn;
}

void Promoter::attach() {
  dispatch_.setPromotionHook(
      [this](const std::string& name, uint64_t callCount) { maybePromote(name, callCount); });
}

void Promoter::maybePromote(const std::string& name, uint64_t callCount) {
  if (callCount < threshold_ || promoted_.count(name))
    return;
  promoted_.insert(name);

  const FunctionDecl* fn = functionsByName_.at(name);
  if (!jit_)
    jit_ = std::make_unique<Jit>(program_, printOut_);
  Jit::EntryThunk thunk = jit_->compileAndLookup(name);

  dispatch_.redirect(name, [thunk, fn](std::vector<Value> args, SourceLocation) -> Value {
    std::vector<int64_t> argBits;
    argBits.reserve(args.size());
    for (const Value& arg : args)
      argBits.push_back(Jit::boxValue(arg));

    int64_t outBits = 0;
    thunk(argBits.empty() ? nullptr : argBits.data(), &outBits);
    return Jit::unboxValue(outBits, fn->returnType);
  });

  if (traceOut_)
    *traceOut_ << name << " promoted to native code after " << callCount << " calls\n";
}

} // namespace mlang
