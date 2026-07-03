// Entry point for the mlang driver binary.
//
// M2 scope: `--dump-ast <file>` lexes and parses a source file and prints
// its AST as an s-expression. Sema/codegen/jit flags land in later
// milestones -- see PRD.md for the breakdown.
#include "mlang/ast/AstPrinter.h"
#include "mlang/lexer/Lexer.h"
#include "mlang/lexer/TokenKind.h"
#include "mlang/parser/Parser.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <sstream>

namespace {

void printUsage() {
  llvm::errs() << "usage: mlang --dump-tokens <file>\n"
                  "       mlang --dump-ast <file>\n";
}

std::string readFileOrEmpty(const std::string& path, bool& ok) {
  std::ifstream file(path);
  if (!file) {
    llvm::errs() << "mlang: could not open '" << path << "'\n";
    ok = false;
    return "";
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  ok = true;
  return buffer.str();
}

int dumpTokens(const std::string& path) {
  bool ok = false;
  std::string source = readFileOrEmpty(path, ok);
  if (!ok)
    return 1;

  mlang::Lexer lexer(source);
  bool sawError = false;
  for (const mlang::Token& tok : lexer.tokenize()) {
    llvm::outs() << tok.loc.line << ":" << tok.loc.column << "\t" << mlang::tokenKindName(tok.kind)
                 << "\t" << tok.lexeme << "\n";
    if (tok.kind == mlang::TokenKind::Error)
      sawError = true;
  }
  return sawError ? 1 : 0;
}

int dumpAst(const std::string& path) {
  bool ok = false;
  std::string source = readFileOrEmpty(path, ok);
  if (!ok)
    return 1;

  mlang::Lexer lexer(source);
  mlang::Parser parser(lexer.tokenize());
  mlang::Program program = parser.parseProgram();

  for (const mlang::Diagnostic& diag : parser.diagnostics())
    llvm::errs() << path << ":" << diag.loc.line << ":" << diag.loc.column
                 << ": error: " << diag.message << "\n";
  if (!parser.diagnostics().empty())
    return 1;

  llvm::outs() << mlang::printAst(program) << "\n";
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--dump-tokens")
    return dumpTokens(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "--dump-ast")
    return dumpAst(argv[2]);

  printUsage();
  return argc == 1 ? 0 : 1;
}
