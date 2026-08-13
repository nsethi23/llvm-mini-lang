// Entry point for the mlang driver binary.
//
// M3 scope: `--interpret <file>` lexes, parses, and tree-walks a source
// file via the interpreter, exiting with `main`'s return value. Sema/
// codegen/jit flags land in later milestones -- see PRD.md for the
// breakdown.
#include "mlang/ast/AstPrinter.h"
#include "mlang/interpreter/Interpreter.h"
#include "mlang/interpreter/RuntimeError.h"
#include "mlang/lexer/Lexer.h"
#include "mlang/lexer/TokenKind.h"
#include "mlang/parser/Parser.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <sstream>

namespace {

void printUsage() {
  llvm::errs() << "usage: mlang --dump-tokens <file>\n"
                  "       mlang --dump-ast <file>\n"
                  "       mlang --interpret <file>\n";
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

int interpret(const std::string& path) {
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

  mlang::Interpreter interp(program);
  try {
    return static_cast<int>(interp.run());
  } catch (const mlang::RuntimeError& err) {
    llvm::errs() << path << ":" << err.loc.line << ":" << err.loc.column
                 << ": error: " << err.message << "\n";
    return 1;
  }
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--dump-tokens")
    return dumpTokens(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "--dump-ast")
    return dumpAst(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "--interpret")
    return interpret(argv[2]);

  printUsage();
  return argc == 1 ? 0 : 1;
}
