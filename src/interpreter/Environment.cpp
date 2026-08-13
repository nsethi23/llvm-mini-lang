#include "mlang/interpreter/Environment.h"

namespace mlang {

void Environment::define(const std::string& name, Value value) {
  vars_[name] = std::move(value);
}

bool Environment::assign(const std::string& name, Value value) {
  auto it = vars_.find(name);
  if (it != vars_.end()) {
    it->second = std::move(value);
    return true;
  }
  if (parent_)
    return parent_->assign(name, std::move(value));
  return false;
}

const Value* Environment::find(const std::string& name) const {
  auto it = vars_.find(name);
  if (it != vars_.end())
    return &it->second;
  if (parent_)
    return parent_->find(name);
  return nullptr;
}

} // namespace mlang
