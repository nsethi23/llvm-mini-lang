// Sema diagnostic golden-file runner (PRD.md M4 demo: "a suite of
// intentionally-broken .mlang files in tests/golden/errors/ each produce
// the expected diagnostic"). For every <name>.mlang in the given
// directory, lexes, parses, and type-checks it, then compares the
// formatted diagnostics (one "<file>:<line>:<col>: error: <message>" line
// per diagnostic, parser errors first) against <name>.expected. Every
// case here is expected to fail parsing or sema -- a case with zero
// diagnostics is itself a test failure.
//
// Usage: sema_error_runner <directory>
#include "mlang/lexer/Lexer.h"
#include "mlang/parser/Parser.h"
#include "mlang/sema/Sema.h"

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

std::string rtrim(std::string s) {
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
    s.pop_back();
  return s;
}

std::string formatDiag(const std::string& fileName, const mlang::Diagnostic& diag) {
  return fileName + ":" + std::to_string(diag.loc.line) + ":" + std::to_string(diag.loc.column) +
         ": error: " + diag.message;
}

struct CaseResult {
  std::string name;
  bool passed;
  std::string failure;
};

CaseResult runCase(const fs::path& mlangFile) {
  std::string name = mlangFile.stem().string();
  fs::path expectedFile = mlangFile;
  expectedFile.replace_extension(".expected");

  if (!fs::exists(expectedFile))
    return {name, false, "missing " + expectedFile.filename().string()};

  std::string expected = rtrim(readFile(expectedFile));
  std::string fileName = mlangFile.filename().string();

  mlang::Lexer lexer(readFile(mlangFile));
  mlang::Parser parser(lexer.tokenize());
  mlang::Program program = parser.parseProgram();

  std::vector<std::string> lines;
  for (const mlang::Diagnostic& diag : parser.diagnostics())
    lines.push_back(formatDiag(fileName, diag));

  if (parser.diagnostics().empty()) {
    mlang::Sema sema(program);
    sema.check();
    for (const mlang::Diagnostic& diag : sema.diagnostics())
      lines.push_back(formatDiag(fileName, diag));
  }

  if (lines.empty())
    return {name, false, "expected a diagnostic, but the program checked clean"};

  std::string actual;
  for (size_t i = 0; i < lines.size(); i++) {
    if (i)
      actual += "\n";
    actual += lines[i];
  }

  if (actual != expected)
    return {name, false, "diagnostic mismatch:\n  expected: " + expected + "\n  actual:   " + actual};
  return {name, true, ""};
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    llvm::errs() << "usage: sema_error_runner <directory>\n";
    return 1;
  }

  fs::path dir(argv[1]);
  std::vector<fs::path> mlangFiles;
  for (const auto& entry : fs::directory_iterator(dir))
    if (entry.path().extension() == ".mlang")
      mlangFiles.push_back(entry.path());
  std::sort(mlangFiles.begin(), mlangFiles.end());

  if (mlangFiles.empty()) {
    llvm::errs() << "sema_error_runner: no .mlang files found in " << dir.string() << "\n";
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
