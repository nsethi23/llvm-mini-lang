// Entry point for the mlang driver binary.
//
// M1 scope: `--dump-tokens <file>` lexes a source file and prints its token
// stream. Parser/sema/codegen/jit flags land in later milestones -- see
// PRD.md for the breakdown.
#include "mlang/lexer/Lexer.h"
#include "mlang/lexer/TokenKind.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <sstream>

namespace {

void printUsage() {
  llvm::errs() << "usage: mlang --dump-tokens <file>\n";
}

int dumpTokens(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    llvm::errs() << "mlang: could not open '" << path << "'\n";
    return 1;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  mlang::Lexer lexer(buffer.str());
  bool sawError = false;
  for (const mlang::Token& tok : lexer.tokenize()) {
    llvm::outs() << tok.loc.line << ":" << tok.loc.column << "\t" << mlang::tokenKindName(tok.kind)
                 << "\t" << tok.lexeme << "\n";
    if (tok.kind == mlang::TokenKind::Error)
      sawError = true;
  }
  return sawError ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--dump-tokens")
    return dumpTokens(argv[2]);

  printUsage();
  return argc == 1 ? 0 : 1;
}
