// PRD.md M7 cross-check: for every <name>.mlang golden program, runs it
// three ways -- (a) pure-interpreted, (b) with hot-swap promotion enabled
// at a low threshold (so promotion happens mid-run for anything
// recursive), and (c) with a threshold of 1 (as close to "fully JIT" as
// this architecture gets, since the call that crosses the threshold always
// finishes on whatever trampoline it started with) -- and requires all
// three to produce identical stdout and exit code, and to match the
// existing <name>.expected/<name>.exitcode golden files.
//
// Usage: promotion_cross_check_runner <directory>
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

struct RunResult {
  bool ranOk;
  std::string failure;
  std::string output;
  int exitCode;
};

RunResult runWith(const mlang::Program& program, uint64_t promotionThreshold) {
  std::string captured;
  llvm::raw_string_ostream os(captured);
  mlang::Interpreter interp(program, os);
  if (promotionThreshold > 0)
    interp.enablePromotion(promotionThreshold);

  try {
    int exitCode = static_cast<int>(interp.run());
    os.flush();
    return {true, "", rtrim(captured), exitCode};
  } catch (const mlang::RuntimeError& err) {
    return {false, "runtime error: " + err.message, "", 0};
  }
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
  if (!parser.diagnostics().empty())
    return {name, false, "parse error: " + parser.diagnostics()[0].message};

  RunResult pureInterpreted = runWith(program, /*promotionThreshold=*/0);
  RunResult midPromotion = runWith(program, /*promotionThreshold=*/20);
  RunResult fullyJit = runWith(program, /*promotionThreshold=*/1);

  for (const auto& [label, result] :
       {std::pair{"pure-interpreted", &pureInterpreted}, std::pair{"mid-promotion", &midPromotion},
        std::pair{"fully-jit", &fullyJit}}) {
    if (!result->ranOk)
      return {name, false, std::string(label) + ": " + result->failure};
  }

  if (pureInterpreted.output != midPromotion.output || pureInterpreted.output != fullyJit.output)
    return {name, false,
            "output diverges across tiers:\n  interpreted: " + pureInterpreted.output +
                "\n  mid-promotion: " + midPromotion.output + "\n  fully-jit: " + fullyJit.output};
  if (pureInterpreted.exitCode != midPromotion.exitCode ||
      pureInterpreted.exitCode != fullyJit.exitCode)
    return {
        name, false,
        "exit code diverges across tiers: interpreted=" + std::to_string(pureInterpreted.exitCode) +
            " mid-promotion=" + std::to_string(midPromotion.exitCode) +
            " fully-jit=" + std::to_string(fullyJit.exitCode)};

  if (pureInterpreted.output != expectedOut)
    return {name, false,
            "stdout mismatch:\n  expected: " + expectedOut +
                "\n  actual:   " + pureInterpreted.output};
  if (pureInterpreted.exitCode != expectedExitCode)
    return {name, false,
            "exit code mismatch: expected " + std::to_string(expectedExitCode) + ", got " +
                std::to_string(pureInterpreted.exitCode)};
  return {name, true, ""};
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    llvm::errs() << "usage: promotion_cross_check_runner <directory>\n";
    return 1;
  }

  fs::path dir(argv[1]);
  std::vector<fs::path> mlangFiles;
  for (const auto& entry : fs::directory_iterator(dir))
    if (entry.path().extension() == ".mlang")
      mlangFiles.push_back(entry.path());
  std::sort(mlangFiles.begin(), mlangFiles.end());

  if (mlangFiles.empty()) {
    llvm::errs() << "promotion_cross_check_runner: no .mlang files found in " << dir.string()
                 << "\n";
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
