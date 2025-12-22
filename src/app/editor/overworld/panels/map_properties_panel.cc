#include "app/editor/overworld/panels/map_properties_panel.h"

#include "absl/status/status.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze {
namespace editor {

void MapPropertiesPanel::Draw(bool* p_open) {
  // Call the existing map properties drawing
  // The map_properties_ member handles all the UI
  editor_->DrawMapProperties();
}

}  // namespace editor
}  // namespace yaze
