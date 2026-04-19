#include "app/editor/overworld/ui/analytics/usage_statistics_view.h"

#include "app/editor/overworld/ui/shared/overworld_window_context.h"
#include "app/editor/overworld/usage_statistics_card.h"
#include "app/editor/registry/panel_registration.h"

namespace yaze::editor {

void UsageStatisticsView::Draw(bool* p_open) {
  const auto ctx = CurrentOverworldWindowContext();
  if (!ctx)
    return;

  // Delegate to the existing UsageStatisticsCard
  if (auto* card = ctx.editor->usage_stats_card()) {
    card->Draw(p_open);
  }
}

REGISTER_PANEL(UsageStatisticsView);

}  // namespace yaze::editor
