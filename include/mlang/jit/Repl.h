// Interactive REPL (PRD.md M9): a thin CLI loop around the same
// lex -> parse -> sema -> Interpreter pipeline a file run uses. Hot-swap
// promotion (PRD.md M7) is enabled throughout, so a function defined and
// called enough times at the prompt promotes to native code exactly like
// it would in a file, printing the same promotion trace -- the REPL isn't
// a separate execution mode, just a different way of feeding statements
// into the tiered dispatch path.
#ifndef MLANG_JIT_REPL_H
#define MLANG_JIT_REPL_H

#include "mlang/ast/Decl.h"
#include "mlang/interpreter/Environment.h"
#include "mlang/interpreter/Interpreter.h"
#include "mlang/parser/Diagnostic.h"
#include "mlang/sema/Scope.h"

#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <istream>
#include <memory>
#include <string>

namespace mlang {

class Repl {
public:
  // `in`/`out` are injectable so tests can drive a session without a real
  // terminal. `promotionThreshold` is the same tuning knob --trace-
  // promotions takes on a file run.
  Repl(std::istream& in, llvm::raw_ostream& out, uint64_t promotionThreshold = 10);

  // Reads and evaluates input until EOF or a quit command. Returns the
  // process exit code (always 0 -- a bad line reports an error and keeps
  // the session going, matching a REPL's whole point).
  int run();

  // Evaluates one already-assembled, brace-balanced chunk of source (a
  // function definition, or one or more `;`-terminated statements, or a
  // single bare expression). Public so a session can be driven
  // programmatically (tests) without going through run()'s line-reading
  // and continuation-prompt logic.
  void evalChunk(const std::string& source);

  bool wantsQuit() const {
    return quit_;
  }

private:
  void evalFunctionDecl(FunctionDecl fn);
  void evalAsStatements(const std::string& source);
  void evalAsExpression(const std::string& source);
  void rebuildInterpreter();
  void reportDiagnostics(const std::vector<Diagnostic>& diags);

  std::istream& in_;
  llvm::raw_ostream& out_;
  uint64_t promotionThreshold_;
  bool quit_ = false;

  Program program_;
  Environment replEnv_;
  Scope replScope_;
  std::unique_ptr<Interpreter> interp_;
};

} // namespace mlang

#endif // MLANG_JIT_REPL_H
