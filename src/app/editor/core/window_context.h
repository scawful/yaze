#ifndef YAZE_APP_EDITOR_CORE_WINDOW_CONTEXT_H_
#define YAZE_APP_EDITOR_CORE_WINDOW_CONTEXT_H_

#include <string>

#include "app/editor/core/content_registry.h"

namespace yaze::editor {

template <typename EditorT>
struct TypedWindowContext {
  EditorT* editor = nullptr;

  explicit operator bool() const { return editor != nullptr; }
  EditorT* operator->() const { return editor; }
};

template <typename EditorT>
inline TypedWindowContext<EditorT> CurrentTypedWindowContext(
    const std::string& category) {
  return {.editor = static_cast<EditorT*>(
              ContentRegistry::Context::editor_window_context(category))};
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_CORE_WINDOW_CONTEXT_H_
