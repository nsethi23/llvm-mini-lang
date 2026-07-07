// Thrown by the interpreter for failures that (with no sema pass yet, see
// PRD.md M4) can only be caught at runtime: undefined variables/functions,
// arity mismatches, division by zero, type mismatches in an operation.
#ifndef MLANG_INTERPRETER_RUNTIMEERROR_H
#define MLANG_INTERPRETER_RUNTIMEERROR_H

#include "mlang/lexer/Token.h"

#include <string>

namespace mlang {

struct RuntimeError {
  std::string message;
  SourceLocation loc;
};

} // namespace mlang

#endif // MLANG_INTERPRETER_RUNTIMEERROR_H
