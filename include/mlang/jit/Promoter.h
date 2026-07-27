// Threshold-triggered hot-swap promotion (PRD.md M7). A Promoter attaches
// to a DispatchTable via a promotion hook: once a function's call count
// crosses `threshold`, it JIT-compiles that function through Jit and
// redirects its dispatch entry to the compiled native code, live. It never
// re-promotes a function once its entry is redirected.
#ifndef MLANG_JIT_PROMOTER_H
#define MLANG_JIT_PROMOTER_H

#include "mlang/ast/Decl.h"
#include "mlang/jit/DispatchTable.h"
#include "mlang/jit/Jit.h"

#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace mlang {

class Promoter {
public:
  // `traceOut`, if non-null, receives one line per promotion:
  // "<name> promoted to native code after <count> calls\n" -- the
  // `--trace-promotions` CLI demo. `printOut` is where the promoted
  // function's own print() calls write once running natively.
  Promoter(const Program& program, DispatchTable& dispatch, uint64_t threshold,
           llvm::raw_ostream* traceOut = nullptr, llvm::raw_ostream& printOut = llvm::outs());

  // Installs this Promoter's hook on the DispatchTable passed at
  // construction. Must be called after every function's interpreter
  // trampoline has already been install()ed (Interpreter's constructor
  // order already guarantees this).
  void attach();

  bool isPromoted(const std::string& name) const {
    return promoted_.count(name) != 0;
  }

private:
  void maybePromote(const std::string& name, uint64_t callCount);

  const Program& program_;
  DispatchTable& dispatch_;
  uint64_t threshold_;
  llvm::raw_ostream* traceOut_;
  std::unique_ptr<Jit> jit_;
  llvm::raw_ostream& printOut_;
  std::unordered_map<std::string, const FunctionDecl*> functionsByName_;
  std::unordered_set<std::string> promoted_;
};

} // namespace mlang

#endif // MLANG_JIT_PROMOTER_H
