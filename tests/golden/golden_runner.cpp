// Golden-file test runner (see CLAUDE.md's "Build & test commands" and
// PRD.md M3's demo). For every <name>.mlang in the given directory, lexes,
// parses, and interprets it, then compares captured stdout against
// <name>.expected (required) and the exit code against <name>.exitcode
// (optional, defaults to "0").
//
// Usage: golden_runner <directory>
#include "mlang/interpreter/Interpreter.h"
#include "mlang/interpreter/RuntimeError.h"
#include "mlang/lexer/Lexer.h"
#include "mlang/parser/Parser.h"

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

struct CaseResult {
  std::string name;
  bool passed;
  std::string failure;
};

CaseResult runCase(const fs::path& mlangFile) {
  std::string name = mlangFile.stem().string();
  fs::path expectedFile = mlangFile;
  expectedFile.replace_extension(".expected");
  fs::path exitCodeFile = mlangFile;
  exitCodeFile.replace_extension(".exitcode");

  if (!fs::exists(expectedFile))
    return {name, false, "missing " + expectedFile.filename().string()};

  std::string expectedOut = rtrim(readFile(expectedFile));
  int expectedExitCode = fs::exists(exitCodeFile) ? std::stoi(readFile(exitCodeFile)) : 0;

  mlang::Lexer lexer(readFile(mlangFile));
  mlang::Parser parser(lexer.tokenize());
  mlang::Program program = parser.parseProgram();
  if (!parser.diagnostics().empty()) {
    std::string msg = "parse error: " + parser.diagnostics()[0].message;
    return {name, false, msg};
  }

  std::string captured;
  llvm::raw_string_ostream os(captured);
  mlang::Interpreter interp(program, os);

  int actualExitCode = 0;
  try {
    actualExitCode = static_cast<int>(interp.run());
  } catch (const mlang::RuntimeError& err) {
    return {name, false, "runtime error: " + err.message};
  }
  os.flush();
  std::string actualOut = rtrim(captured);

  if (actualOut != expectedOut)
    return {name, false,
            "stdout mismatch:\n  expected: " + expectedOut + "\n  actual:   " + actualOut};
  if (actualExitCode != expectedExitCode)
    return {name, false,
            "exit code mismatch: expected " + std::to_string(expectedExitCode) + ", got " +
                std::to_string(actualExitCode)};
  return {name, true, ""};
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    llvm::errs() << "usage: golden_runner <directory>\n";
    return 1;
  }

  fs::path dir(argv[1]);
  std::vector<fs::path> mlangFiles;
  for (const auto& entry : fs::directory_iterator(dir))
    if (entry.path().extension() == ".mlang")
      mlangFiles.push_back(entry.path());
  std::sort(mlangFiles.begin(), mlangFiles.end());

  if (mlangFiles.empty()) {
    llvm::errs() << "golden_runner: no .mlang files found in " << dir.string() << "\n";
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
