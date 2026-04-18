#ifndef YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_ACTIONS_REGISTRY_H_
#define YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_ACTIONS_REGISTRY_H_

#include <functional>
#include <string>
#include <vector>

namespace yaze {
namespace editor {

// A single action surfaced by the ActivityBar's "More Actions" popup.
// `icon` may be null (menus render label only).
// `enabled_fn` may be null (action is always enabled).
struct MoreAction {
  std::string id;
  std::string label;
  const char* icon = nullptr;
  std::function<void()> on_invoke;
  std::function<bool()> enabled_fn;
};

// Extensible registry for "More Actions" entries in the activity bar.
// Preserves insertion order so downstream ordering is predictable for users
// and for test assertions. Registering an action whose id already exists
// replaces the prior entry in-place (keeps the one-id invariant without
// reshuffling the tail).
class MoreActionsRegistry {
 public:
  void Register(MoreAction action);
  void Unregister(const std::string& id);
  void Clear();
  void ForEach(const std::function<void(const MoreAction&)>& fn) const;
  size_t size() const { return actions_.size(); }
  bool empty() const { return actions_.empty(); }

 private:
  std::vector<MoreAction> actions_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_ACTIONS_REGISTRY_H_
