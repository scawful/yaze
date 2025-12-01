#ifndef YAZE_APP_EDITOR_OVERWORLD_USAGE_STATISTICS_CARD_H_
#define YAZE_APP_EDITOR_OVERWORLD_USAGE_STATISTICS_CARD_H_

#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

class UsageStatisticsCard {
 public:
  UsageStatisticsCard(zelda3::Overworld* overworld);
  ~UsageStatisticsCard() = default;

  void Draw(bool* p_open = nullptr);

 private:
  void DrawUsageGrid();
  void DrawUsageStates();

  zelda3::Overworld* overworld_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_USAGE_STATISTICS_CARD_H_
