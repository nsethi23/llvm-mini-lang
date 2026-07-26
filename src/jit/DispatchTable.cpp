#include "mlang/jit/DispatchTable.h"

#include <cassert>

namespace mlang {

void DispatchTable::install(const std::string& name, Trampoline trampoline) {
  assert(entries_.find(name) == entries_.end() && "install() on an already-installed entry");
  entries_[name] = Entry{std::move(trampoline), 0};
  insertionOrder_.push_back(name);
}

void DispatchTable::redirect(const std::string& name, Trampoline trampoline) {
  auto it = entries_.find(name);
  assert(it != entries_.end() && "redirect() on a name with no entry");
  it->second.trampoline = std::move(trampoline);
}

Value DispatchTable::invoke(const std::string& name, std::vector<Value> args, SourceLocation loc) {
  auto it = entries_.find(name);
  assert(it != entries_.end() && "invoke() on a name with no entry");
  it->second.callCount++;

  // Captured before the hook runs -- see the invoke() doc comment on why
  // this ordering matters for promotion mid-recursion.
  Trampoline trampoline = it->second.trampoline;
  if (hook_)
    hook_(name, it->second.callCount);
  return trampoline(std::move(args), loc);
}

bool DispatchTable::contains(const std::string& name) const {
  return entries_.find(name) != entries_.end();
}

uint64_t DispatchTable::callCount(const std::string& name) const {
  auto it = entries_.find(name);
  return it == entries_.end() ? 0 : it->second.callCount;
}

std::vector<std::pair<std::string, uint64_t>> DispatchTable::callCounts() const {
  std::vector<std::pair<std::string, uint64_t>> result;
  result.reserve(insertionOrder_.size());
  for (const std::string& name : insertionOrder_)
    result.emplace_back(name, entries_.at(name).callCount);
  return result;
}

} // namespace mlang
