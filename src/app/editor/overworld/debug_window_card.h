#ifndef YAZE_APP_EDITOR_OVERWORLD_DEBUG_WINDOW_CARD_H_
#define YAZE_APP_EDITOR_OVERWORLD_DEBUG_WINDOW_CARD_H_

namespace yaze::editor {

class DebugWindowCard {
 public:
  DebugWindowCard();
  ~DebugWindowCard() = default;

  void Draw(bool* p_open = nullptr);
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_DEBUG_WINDOW_CARD_H_
