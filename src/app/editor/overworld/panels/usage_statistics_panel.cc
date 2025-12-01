#include "app/editor/overworld/panels/usage_statistics_panel.h"

#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/usage_statistics_card.h"

namespace yaze {
namespace editor {

void UsageStatisticsPanel::Draw(bool* p_open) {
  // Delegate to the existing UsageStatisticsCard
  // This card already exists and just needs to be wrapped
  if (auto* card = editor_->usage_stats_card()) {
    card->Draw(p_open);
  }
}

}  // namespace editor
}  // namespace yaze
