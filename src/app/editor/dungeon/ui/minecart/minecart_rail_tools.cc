#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"

#include "app/gui/automation/widget_auto_register.h"
#include "imgui/imgui.h"

namespace yaze::editor {

void MinecartTrackEditorPanel::DrawRailTools() {
  if (!ImGui::CollapsingHeader("Rails and behavior",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;
  ImGui::TextWrapped(
      "Choose artwork, then place it on the room canvas. "
      "Rail behavior is stamped separately below.");
  if (draw_rail_palette_ && draw_rail_palette_()) {
    set_canvas_editing(false);
    set_collision_stamping(false);
  }
  ImGui::Separator();
  bool stamping = collision_stamping_;
  ImGui::BeginDisabled(!apply_collision_stamp_);
  if (ImGui::Checkbox("Stamp rail behavior", &stamping)) {
    set_collision_stamping(stamping);
  }
  gui::AutoRegisterLastItem("checkbox", "Stamp rail behavior");
  ImGui::EndDisabled();
  if (!collision_stamping_)
    return;

  struct Behavior {
    uint8_t value;
    const char* label;
  };
  static constexpr Behavior kBehaviors[] = {{0xB0, "Straight: horizontal"},
                                            {0xB1, "Straight: vertical"},
                                            {0xB2, "Corner: right / down"},
                                            {0xB3, "Corner: right / up"},
                                            {0xB4, "Corner: left / down"},
                                            {0xB5, "Corner: left / up"},
                                            {0xB6, "Crossing"},
                                            {0xB7, "Stop: depart south"},
                                            {0xB8, "Stop: depart north"},
                                            {0xB9, "Stop: depart east"},
                                            {0xBA, "Stop: depart west"},
                                            {0xBB, "Junction: north"},
                                            {0xBC, "Junction: south"},
                                            {0xBD, "Junction: east"},
                                            {0xBE, "Junction: west"},
                                            {0xD0, "Switch: right / down"},
                                            {0xD1, "Switch: right / up"},
                                            {0xD2, "Switch: left / down"},
                                            {0xD3, "Switch: left / up"},
                                            {0x00, "Erase rail behavior"}};
  const char* selected = kBehaviors[1].label;
  for (const auto& behavior : kBehaviors) {
    if (behavior.value == collision_stamp_value_)
      selected = behavior.label;
  }
  ImGui::SetNextItemWidth(-1);
  const bool open = ImGui::BeginCombo("##RailBehavior", selected);
  gui::AutoRegisterLastItem("combo", "RailBehavior");
  if (open) {
    for (const auto& behavior : kBehaviors) {
      if (ImGui::Selectable(behavior.label,
                            behavior.value == collision_stamp_value_)) {
        collision_stamp_value_ = behavior.value;
      }
      gui::AutoRegisterLastItem("selectable", behavior.label);
    }
    ImGui::EndCombo();
  }
  ImGui::Checkbox("2 x 2 cells (16 pixels)", &collision_stamp_large_);
  ImGui::TextWrapped(
      "Upper layer only. Hover to preview; click to stamp. "
      "Each click is one Undo step. Click the room canvas, then Escape "
      "leaves stamping.");
  ImGui::TextWrapped(
      "Ordinary rails preserve stops, switches and other "
      "collision. Use a stop or switch tool to change its direction. "
      "Erase rail behavior removes rails, stops and switches only; "
      "artwork is unchanged.");
}

}  // namespace yaze::editor
