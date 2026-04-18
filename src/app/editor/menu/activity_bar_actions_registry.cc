#include "app/editor/menu/activity_bar_actions_registry.h"

#include <algorithm>
#include <utility>

namespace yaze {
namespace editor {

void MoreActionsRegistry::Register(MoreAction action) {
  auto it = std::find_if(
      actions_.begin(), actions_.end(),
      [&](const MoreAction& existing) { return existing.id == action.id; });
  if (it != actions_.end()) {
    *it = std::move(action);
    return;
  }
  actions_.push_back(std::move(action));
}

void MoreActionsRegistry::Unregister(const std::string& id) {
  actions_.erase(
      std::remove_if(actions_.begin(), actions_.end(),
                     [&](const MoreAction& a) { return a.id == id; }),
      actions_.end());
}

void MoreActionsRegistry::Clear() { actions_.clear(); }

void MoreActionsRegistry::ForEach(
    const std::function<void(const MoreAction&)>& fn) const {
  for (const auto& action : actions_) {
    fn(action);
  }
}

}  // namespace editor
}  // namespace yaze
