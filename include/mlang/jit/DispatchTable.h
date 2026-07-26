// Indirect call-dispatch table (PRD.md M6). Every mlang function gets one
// entry, initially bound to an interpreter trampoline; M7 will patch a
// promoted function's entry to a JIT-compiled trampoline instead. Callers
// always go through invoke() by name, so redirecting an entry's trampoline
// changes what every future call resolves to without any caller-side code
// changing -- that indirection is the whole point of this milestone.
#ifndef MLANG_JIT_DISPATCHTABLE_H
#define MLANG_JIT_DISPATCHTABLE_H

#include "mlang/interpreter/Value.h"
#include "mlang/lexer/Token.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mlang {

class DispatchTable {
public:
  // Executes a call to some function given its arguments, returning the
  // function's result. M6 only ever installs interpreter trampolines; M7
  // installs ones that call into JIT-compiled native code instead.
  using Trampoline = std::function<Value(std::vector<Value> args, SourceLocation loc)>;

  // Creates the entry for `name`, bound to `trampoline`, with a call count
  // of 0. Asserts `name` has no existing entry -- use redirect() to change
  // an already-installed entry's target.
  void install(const std::string& name, Trampoline trampoline);

  // Redirects an existing entry to a new trampoline without touching its
  // call counter. This is the mechanism M7's promotion patches through:
  // callers keep invoking by name, unaware the target changed underneath
  // them. Asserts `name` has an existing entry.
  void redirect(const std::string& name, Trampoline trampoline);

  // Increments `name`'s call counter and invokes its current trampoline.
  // Asserts `name` has an existing entry -- there is no fallback target.
  //
  // The trampoline actually invoked for THIS call is captured before the
  // promotion hook (below) runs, so a hook that calls redirect() on `name`
  // never affects the call that triggered it -- only calls made afterward
  // (including recursive self-calls made while this one is still on the
  // stack) see the new target. That is what lets a promotion mid-recursion
  // leave already-in-flight interpreter frames alone (PRD.md M7).
  Value invoke(const std::string& name, std::vector<Value> args, SourceLocation loc);

  // Called synchronously from invoke(), after the call counter increments
  // but before that call's trampoline runs, as (name, newCallCount). M6
  // itself has no promotion policy; this is the extension point M7's
  // Promoter attaches to in order to decide when to compile and redirect.
  using PromotionHook = std::function<void(const std::string& name, uint64_t callCount)>;
  void setPromotionHook(PromotionHook hook) {
    hook_ = std::move(hook);
  }

  bool contains(const std::string& name) const;
  uint64_t callCount(const std::string& name) const;

  // All entries as (name, callCount) pairs, in install() order -- used by
  // the `--trace-calls` CLI flag.
  std::vector<std::pair<std::string, uint64_t>> callCounts() const;

private:
  struct Entry {
    Trampoline trampoline;
    uint64_t callCount = 0;
  };

  std::unordered_map<std::string, Entry> entries_;
  std::vector<std::string> insertionOrder_;
  PromotionHook hook_;
};

} // namespace mlang

#endif // MLANG_JIT_DISPATCHTABLE_H
