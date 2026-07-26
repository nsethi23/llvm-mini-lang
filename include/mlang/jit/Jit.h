// ORC JIT driver (PRD.md M7). Compiles a whole Program via the existing M5
// CodeGen path, plus one small per-function "entry thunk" that gives every
// function a uniform, boxed-argument C ABI -- that uniform signature is
// what lets Promoter call an arbitrary mlang function's native code from
// C++ without a hand-written shim per function signature.
//
// Compilation is whole-program and lazy: the first promotion request pays
// for compiling every function once; after that, looking up any other
// function's entry thunk is a cheap symbol lookup. Only the functions whose
// dispatch entries actually get redirected run natively -- everything else
// keeps going through the interpreter, even though native code exists for
// it too (PRD.md's per-function promotion is about which dispatch entries
// get patched, not about how much code the JIT happens to have compiled).
#ifndef MLANG_JIT_JIT_H
#define MLANG_JIT_JIT_H

#include "mlang/ast/Decl.h"
#include "mlang/interpreter/Value.h"

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <memory>
#include <string>

namespace mlang {

class Jit {
public:
  // A function's native entry point, uniform across every mlang function
  // regardless of its actual signature: `argsBits[i]` holds parameter i's
  // value bit-reinterpreted as i64 (see boxValue/unboxValue), and the
  // function stores its bit-reinterpreted result through `outBits`.
  using EntryThunk = void (*)(const int64_t* argsBits, int64_t* outBits);

  // `printOut` is where JIT-compiled print() calls write -- mirrors
  // Interpreter's constructor parameter so interpreted and JIT-compiled
  // output can be captured identically in tests.
  explicit Jit(const Program& program, llvm::raw_ostream& printOut = llvm::outs());

  // Compiles the whole program on first call (subsequent calls reuse the
  // same JIT'd module) and returns `name`'s entry thunk. Aborts the process
  // via llvm::report_fatal_error on a codegen/verification/JIT failure --
  // these indicate a bug in this compiler, not a recoverable user error,
  // matching how Sema having already accepted the program guarantees
  // CodeGen won't hit an ill-typed construct.
  EntryThunk compileAndLookup(const std::string& name);

  // Bit-reinterprets `value` into the i64 the entry thunk ABI passes
  // around. Static + free of `type` because a Value's active alternative
  // already encodes which conversion applies.
  static int64_t boxValue(const Value& value);

  // Inverse of boxValue -- `type` disambiguates what `bits` means since the
  // ABI itself carries no type tag (both sides already know the function's
  // static signature from the typed AST).
  static Value unboxValue(int64_t bits, TypeName type);

private:
  void ensureCompiled();

  const Program& program_;
  llvm::raw_ostream& printOut_;
  std::unique_ptr<llvm::orc::LLJIT> jit_;
  bool compiled_ = false;
};

} // namespace mlang

#endif // MLANG_JIT_JIT_H
