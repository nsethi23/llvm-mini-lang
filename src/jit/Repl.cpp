#include "mlang/jit/Repl.h"

#include "mlang/interpreter/RuntimeError.h"
#include "mlang/lexer/Lexer.h"
#include "mlang/parser/Parser.h"
#include "mlang/sema/Sema.h"

#include <algorithm>
#include <cctype>

namespace mlang {

namespace {

std::string trim(const std::string& s) {
  size_t begin = s.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos)
    return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(begin, end - begin + 1);
}

// A REPL line's brace/paren balance, ignoring anything inside a string
// literal or a `//` comment -- so a multi-line `fn`/`if`/`while` can be
// typed across several lines (with a continuation prompt) before it's
// handed to the lexer, without miscounting a `{` that happens to appear
// inside a print() string argument.
int braceDelta(const std::string& line) {
  int delta = 0;
  bool inString = false;
  for (size_t i = 0; i < line.size(); i++) {
    char c = line[i];
    if (inString) {
      if (c == '\\')
        i++; // skip the escaped character
      else if (c == '"')
        inString = false;
      continue;
    }
    if (c == '"') {
      inString = true;
    } else if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
      break; // rest of the line is a comment
    } else if (c == '{' || c == '(') {
      delta++;
    } else if (c == '}' || c == ')') {
      delta--;
    }
  }
  return delta;
}

// A bare `return` at REPL top level (even nested inside if/while) has
// nowhere to unwind to -- Interpreter::execute's ReturnSignal is only ever
// caught by a function call. Rejecting it here, before execution, avoids
// that private exception type escaping uncaught.
bool containsReturn(const Stmt& stmt) {
  switch (stmt.kind) {
  case StmtKind::Return:
    return true;
  case StmtKind::Block: {
    const auto& block = static_cast<const BlockStmt&>(stmt);
    return std::any_of(block.stmts.begin(), block.stmts.end(),
                       [](const StmtPtr& s) { return containsReturn(*s); });
  }
  case StmtKind::If: {
    const auto& ifs = static_cast<const IfStmt&>(stmt);
    if (containsReturn(*ifs.thenBlock))
      return true;
    return ifs.elseBlock && containsReturn(*ifs.elseBlock);
  }
  case StmtKind::While:
    return containsReturn(*static_cast<const WhileStmt&>(stmt).body);
  default:
    return false;
  }
}

std::string renderSignature(const FunctionDecl& fn) {
  std::string sig = "fn " + fn.name + "(";
  for (size_t i = 0; i < fn.params.size(); i++) {
    if (i > 0)
      sig += ", ";
    sig += fn.params[i].name + ": " + std::string(typeName(fn.params[i].type));
  }
  sig += ") -> " + std::string(typeName(fn.returnType));
  return sig;
}

// A dummy function context for checkStmt()'s return-type-checking
// parameter -- never actually exercised, since containsReturn() rejects
// any input containing a `return` before checkStmt ever sees it.
const FunctionDecl& replStmtContext() {
  static FunctionDecl ctx = [] {
    FunctionDecl fn;
    fn.name = "<repl>";
    fn.returnType = TypeName::Int;
    return fn;
  }();
  return ctx;
}

} // namespace

Repl::Repl(std::istream& in, llvm::raw_ostream& out, uint64_t promotionThreshold)
    : in_(in), out_(out), promotionThreshold_(promotionThreshold) {
  rebuildInterpreter();
}

void Repl::rebuildInterpreter() {
  interp_ = std::make_unique<Interpreter>(program_, out_);
  interp_->enablePromotion(promotionThreshold_, &out_);
}

void Repl::reportDiagnostics(const std::vector<Diagnostic>& diags) {
  for (const Diagnostic& diag : diags)
    out_ << "error: " << diag.loc.line << ":" << diag.loc.column << ": " << diag.message << "\n";
}

void Repl::evalFunctionDecl(FunctionDecl fn) {
  if (fn.name == "print") {
    out_ << "error: 'print' is a built-in function and cannot be redefined\n";
    return;
  }

  auto existing = std::find_if(program_.functions.begin(), program_.functions.end(),
                               [&](const FunctionDecl& f) { return f.name == fn.name; });
  int existingIndex = existing == program_.functions.end()
                          ? -1
                          : static_cast<int>(existing - program_.functions.begin());

  // Tentatively append at the back so the function can reference itself
  // (Sema's functions_ map keys by name -- a later same-named entry wins
  // during construction, so self-calls resolve to this new definition,
  // not a stale one at existingIndex).
  program_.functions.push_back(std::move(fn));

  Sema sema(program_);
  sema.checkFunction(program_.functions.back());

  if (!sema.diagnostics().empty()) {
    reportDiagnostics(sema.diagnostics());
    program_.functions.pop_back();
    // The push_back above may have reallocated, invalidating every
    // pointer the current interp_ captured into program_.functions --
    // rebuild regardless of the rollback so it never dereferences a
    // stale one.
    rebuildInterpreter();
    return;
  }

  if (existingIndex >= 0)
    program_.functions.erase(program_.functions.begin() + existingIndex);

  out_ << (existingIndex >= 0 ? "redefined " : "defined ")
       << renderSignature(program_.functions.back()) << "\n";
  rebuildInterpreter();
}

void Repl::evalAsStatements(const std::string& source) {
  Lexer lexer("{ " + source + " }");
  Parser parser(lexer.tokenize());
  std::unique_ptr<BlockStmt> block = parser.parseBlock();
  if (!block || !parser.diagnostics().empty()) {
    evalAsExpression(source);
    return;
  }

  for (const StmtPtr& stmt : block->stmts) {
    if (containsReturn(*stmt)) {
      out_ << "error: 'return' is only valid inside a function body\n";
      return;
    }
  }

  Sema sema(program_);
  for (const StmtPtr& stmt : block->stmts)
    sema.checkStmt(*stmt, replScope_, replStmtContext());
  if (!sema.diagnostics().empty()) {
    reportDiagnostics(sema.diagnostics());
    return;
  }

  try {
    for (const StmtPtr& stmt : block->stmts)
      interp_->execute(*stmt, replEnv_);
  } catch (const RuntimeError& err) {
    out_ << "error: " << err.loc.line << ":" << err.loc.column << ": " << err.message << "\n";
  }
}

void Repl::evalAsExpression(const std::string& source) {
  Lexer lexer(source);
  Parser parser(lexer.tokenize());
  ExprPtr expr = parser.parseExpression();
  if (!expr || !parser.diagnostics().empty()) {
    reportDiagnostics(parser.diagnostics());
    if (parser.diagnostics().empty())
      out_ << "error: could not parse input as a statement or expression\n";
    return;
  }

  Sema sema(program_);
  SemaType type = sema.checkExpr(*expr, replScope_);
  if (!sema.diagnostics().empty()) {
    reportDiagnostics(sema.diagnostics());
    return;
  }
  (void)type;

  try {
    Value result = interp_->evaluate(*expr, replEnv_);
    out_ << valueToString(result) << "\n";
  } catch (const RuntimeError& err) {
    out_ << "error: " << err.loc.line << ":" << err.loc.column << ": " << err.message << "\n";
  }
}

void Repl::evalChunk(const std::string& source) {
  std::string trimmed = trim(source);
  if (trimmed.empty())
    return;
  if (trimmed == "exit" || trimmed == "quit" || trimmed == ":exit" || trimmed == ":quit" ||
      trimmed == ":q") {
    quit_ = true;
    return;
  }
  if (trimmed == ":help") {
    out_ << "Type a function definition (fn name(...) -> type { ... }), a "
            "';'-terminated statement (let/assign/if/while/print), or a bare "
            "expression to evaluate and print its value.\n"
            "Multi-line input is supported -- keep typing until braces/parens "
            "balance.\n"
            ":quit / :exit / :q / exit / quit -- leave the REPL.\n";
    return;
  }

  Lexer lexer(trimmed);
  std::vector<Token> tokens = lexer.tokenize();
  if (!tokens.empty() && tokens.front().kind == TokenKind::KwFn) {
    Parser parser(tokens);
    Program parsed = parser.parseProgram();
    if (!parser.diagnostics().empty() || parsed.functions.size() != 1) {
      reportDiagnostics(parser.diagnostics());
      return;
    }
    evalFunctionDecl(std::move(parsed.functions.front()));
    return;
  }

  evalAsStatements(trimmed);
}

int Repl::run() {
  out_ << "mlang REPL -- tiered execution enabled (promotion threshold " << promotionThreshold_
       << "). Type :help for usage, :quit to leave.\n";

  std::string buffer;
  int depth = 0;
  while (!quit_) {
    out_ << (depth > 0 ? "... " : ">>> ");
    out_.flush();
    std::string line;
    if (!std::getline(in_, line)) {
      out_ << "\n";
      break;
    }

    depth += braceDelta(line);
    buffer += line;
    buffer += "\n";

    if (depth > 0)
      continue; // keep reading a multi-line definition/block

    depth = 0; // a stray extra '}' shouldn't leave the prompt stuck
    std::string chunk = std::move(buffer);
    buffer.clear();
    evalChunk(chunk);
  }
  return 0;
}

} // namespace mlang
