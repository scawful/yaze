#include "app/editor/overworld/panels/usage_statistics_panel.h"

#include "app/editor/core/content_registry.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/usage_statistics_card.h"

namespace yaze::editor {

void UsageStatisticsPanel::Draw(bool* p_open) {
  auto* editor = ContentRegistry::Context::current_editor();
  if (!editor) return;

  if (auto* ow_editor = dynamic_cast<OverworldEditor*>(editor)) {
    // Delegate to the existing UsageStatisticsCard
    if (auto* card = ow_editor->usage_stats_card()) {
      card->Draw(p_open);
    }
  }
}

REGISTER_PANEL(UsageStatisticsPanel);

}  // namespace yaze::editor
