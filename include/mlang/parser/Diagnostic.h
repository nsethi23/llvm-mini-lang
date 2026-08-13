// A single parser diagnostic: a source location plus a human-readable
// message. The parser collects these rather than stopping at the first
// error, so a whole file's syntax errors can be reported at once.
#ifndef MLANG_PARSER_DIAGNOSTIC_H
#define MLANG_PARSER_DIAGNOSTIC_H

#include "mlang/lexer/Token.h"

#include <string>

namespace mlang {

struct Diagnostic {
  SourceLocation loc;
  std::string message;
};

} // namespace mlang

#endif // MLANG_PARSER_DIAGNOSTIC_H
