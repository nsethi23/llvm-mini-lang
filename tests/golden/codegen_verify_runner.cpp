// Codegen golden-file runner (see PRD.md M5's demo: "mlang --emit-llvm
// examples/fib.mlang prints valid .ll text that passes the LLVM
// verifier"). For every <name>.mlang in the given directory, lexes,
// parses, sema-checks, and codegens it, then runs llvm::verifyModule on
// the result. Every program in tests/golden/ is well-typed by
// construction (the ill-typed ones live in tests/golden/errors/, covered
// by sema_error_runner instead), so this is purely a "codegen never emits
// a broken module" regression check across the whole example corpus.
//
// Usage: codegen_verify_runner <directory>
#include "mlang/codegen/CodeGen.h"
#include "mlang/lexer/Lexer.h"
#include "mlang/parser/Parser.h"
#include "mlang/sema/Sema.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& path) {
  std::ifstream file(path);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

struct CaseResult {
  std::string name;
  bool passed;
  std::string failure;
};

CaseResult runCase(const fs::path& mlangFile) {
  std::string name = mlangFile.stem().string();

  mlang::Lexer lexer(readFile(mlangFile));
  mlang::Parser parser(lexer.tokenize());
  mlang::Program program = parser.parseProgram();
  if (!parser.diagnostics().empty())
    return {name, false, "parse error: " + parser.diagnostics()[0].message};

  mlang::Sema sema(program);
  sema.check();
  if (!sema.diagnostics().empty())
    return {name, false, "sema error: " + sema.diagnostics()[0].message};

  llvm::LLVMContext ctx;
  mlang::CodeGen codegen(program, ctx);
  codegen.generate();

  std::string errors;
  llvm::raw_string_ostream os(errors);
  if (llvm::verifyModule(codegen.module(), &os))
    return {name, false, "module failed verification:\n" + errors};

  return {name, true, ""};
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    llvm::errs() << "usage: codegen_verify_runner <directory>\n";
    return 1;
  }

  fs::path dir(argv[1]);
  std::vector<fs::path> mlangFiles;
  for (const auto& entry : fs::directory_iterator(dir))
    if (entry.path().extension() == ".mlang")
      mlangFiles.push_back(entry.path());
  std::sort(mlangFiles.begin(), mlangFiles.end());

  if (mlangFiles.empty()) {
    llvm::errs() << "codegen_verify_runner: no .mlang files found in " << dir.string() << "\n";
    return 1;
  }

  int failures = 0;
  for (const fs::path& file : mlangFiles) {
    CaseResult result = runCase(file);
    if (result.passed) {
      llvm::outs() << "PASS " << result.name << "\n";
    } else {
      llvm::outs() << "FAIL " << result.name << ": " << result.failure << "\n";
      failures++;
    }
  }

  llvm::outs() << (mlangFiles.size() - failures) << "/" << mlangFiles.size() << " passed\n";
  return failures == 0 ? 0 : 1;
}
