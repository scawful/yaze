#include "app/editor/overworld/panels/v3_settings_panel.h"

#include "app/editor/overworld/overworld_editor.h"

namespace yaze {
namespace editor {

void V3SettingsPanel::Draw(bool* p_open) {
  editor_->DrawV3Settings();
}

}  // namespace editor
}  // namespace yaze
