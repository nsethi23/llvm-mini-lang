// Entry point for the mlang driver binary.
//
// M4 scope: `--check <file>` lexes, parses, and type-checks a source file,
// printing diagnostics for any parse or sema error. `--interpret <file>`
// does the same plus tree-walks a well-typed program via the interpreter,
// exiting with `main`'s return value. M5 adds `--emit-llvm <file>`, which
// does the same checks plus emits and verifies LLVM IR. `--trace-calls`
// (M6) interprets the program and prints each function's dispatch-table
// call count. See PRD.md for the full milestone breakdown.
#include "mlang/ast/AstPrinter.h"
#include "mlang/codegen/CodeGen.h"
#include "mlang/interpreter/Interpreter.h"
#include "mlang/interpreter/RuntimeError.h"
#include "mlang/lexer/Lexer.h"
#include "mlang/lexer/TokenKind.h"
#include "mlang/parser/Parser.h"
#include "mlang/sema/Sema.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <sstream>

namespace {

void printUsage() {
  llvm::errs() << "usage: mlang --dump-tokens <file>\n"
                  "       mlang --dump-ast <file>\n"
                  "       mlang --check <file>\n"
                  "       mlang --interpret <file>\n"
                  "       mlang --emit-llvm <file>\n"
                  "       mlang --trace-calls <file>\n";
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

int checkProgram(const std::string& path) {
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

  mlang::Sema sema(program);
  sema.check();
  for (const mlang::Diagnostic& diag : sema.diagnostics())
    llvm::errs() << path << ":" << diag.loc.line << ":" << diag.loc.column
                 << ": error: " << diag.message << "\n";
  return sema.diagnostics().empty() ? 0 : 1;
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

  mlang::Sema sema(program);
  sema.check();
  for (const mlang::Diagnostic& diag : sema.diagnostics())
    llvm::errs() << path << ":" << diag.loc.line << ":" << diag.loc.column
                 << ": error: " << diag.message << "\n";
  if (!sema.diagnostics().empty())
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

// PRD.md M6 demo: runs a program through the interpreter and prints each
// function's call count -- the count DispatchTable accumulates on every
// invoke(), which M7's promotion decision will read from directly.
int traceCalls(const std::string& path) {
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

  mlang::Sema sema(program);
  sema.check();
  for (const mlang::Diagnostic& diag : sema.diagnostics())
    llvm::errs() << path << ":" << diag.loc.line << ":" << diag.loc.column
                 << ": error: " << diag.message << "\n";
  if (!sema.diagnostics().empty())
    return 1;

  mlang::Interpreter interp(program);
  int exitCode = 0;
  try {
    exitCode = static_cast<int>(interp.run());
  } catch (const mlang::RuntimeError& err) {
    llvm::errs() << path << ":" << err.loc.line << ":" << err.loc.column
                 << ": error: " << err.message << "\n";
    exitCode = 1;
  }

  llvm::outs() << "-- call counts --\n";
  for (const auto& [name, count] : interp.dispatchTable().callCounts())
    llvm::outs() << name << ": " << count << "\n";

  return exitCode;
}

int emitLlvm(const std::string& path) {
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

  mlang::Sema sema(program);
  sema.check();
  for (const mlang::Diagnostic& diag : sema.diagnostics())
    llvm::errs() << path << ":" << diag.loc.line << ":" << diag.loc.column
                 << ": error: " << diag.message << "\n";
  if (!sema.diagnostics().empty())
    return 1;

  llvm::LLVMContext ctx;
  mlang::CodeGen codegen(program, ctx);
  codegen.generate();

  std::string verifyErrors;
  llvm::raw_string_ostream verifyOs(verifyErrors);
  if (llvm::verifyModule(codegen.module(), &verifyOs)) {
    llvm::errs() << "mlang: generated module failed verification:\n" << verifyErrors;
    return 1;
  }

  codegen.module().print(llvm::outs(), nullptr);
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--dump-tokens")
    return dumpTokens(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "--dump-ast")
    return dumpAst(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "--check")
    return checkProgram(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "--interpret")
    return interpret(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "--emit-llvm")
    return emitLlvm(argv[2]);
  if (argc == 3 && std::string(argv[1]) == "--trace-calls")
    return traceCalls(argv[2]);

  printUsage();
  return argc == 1 ? 0 : 1;
}
