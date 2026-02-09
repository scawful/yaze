#ifndef YAZE_APP_EDITOR_SYSTEM_DEFAULT_EDITOR_FACTORIES_H_
#define YAZE_APP_EDITOR_SYSTEM_DEFAULT_EDITOR_FACTORIES_H_

namespace yaze::editor {

class EditorRegistry;

// Registers the built-in editor factories used to construct an EditorSet for a
// ROM session.
void RegisterDefaultEditorFactories(EditorRegistry* registry);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_DEFAULT_EDITOR_FACTORIES_H_

