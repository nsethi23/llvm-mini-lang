#include "mlang/sema/Scope.h"

namespace mlang {

void Scope::define(const std::string& name, SemaType type) {
  vars_[name] = type;
}

const SemaType* Scope::find(const std::string& name) const {
  auto it = vars_.find(name);
  if (it != vars_.end())
    return &it->second;
  if (parent_)
    return parent_->find(name);
  return nullptr;
}

} // namespace mlang
