// Entry point for the mlang driver binary.
//
// M0 scope only: this is a placeholder that proves the CMake + LLVM
// toolchain links correctly end to end. The lexer/parser/sema/codegen/jit
// pipeline stages are wired in starting at M1 -- see PRD.md for the
// milestone breakdown and CLAUDE.md for how work here should proceed.
#include "llvm/Support/raw_ostream.h"
#include "llvm/Config/llvm-config.h"

int main() {
  llvm::outs() << "mlang: scaffold OK (linked against LLVM "
               << LLVM_VERSION_STRING << ")\n";
  return 0;
}
