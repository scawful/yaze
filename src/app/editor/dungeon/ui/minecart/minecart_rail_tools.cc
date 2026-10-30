#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"

#include <algorithm>

#include "app/gui/automation/widget_auto_register.h"
#include "imgui/imgui.h"

namespace yaze::editor {

void MinecartTrackEditorPanel::DrawRailTools() {
  if (placement_brush_) {
    const auto& brush = *placement_brush_;
    static constexpr const char* kDepartures[] = {"south", "north", "east",
                                                  "west"};
    const uint8_t collision =
        brush.collision_only ? brush.collision : brush.node.collision;
    const char* departure = collision >= 0xB7 && collision <= 0xBA
                                ? kDepartures[collision - 0xB7]
                                : "unknown";
    if (brush.collision_only) {
      ImGui::TextWrapped("End stop: depart %s - room %03X", departure,
                         brush.room_id);
    } else if (brush.node.kind == MinecartNodeKind::kCartStart) {
      ImGui::TextWrapped("Cart %d: depart %s - room %03X", brush.node.id,
                         departure, brush.room_id);
    } else {
      ImGui::TextWrapped("Active node: %s, channel %d - room %03X",
                         brush.node.kind == MinecartNodeKind::kSwitch
                             ? "switch"
                             : "switch track",
                         brush.node.id, brush.room_id);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Selected node brush. Collision $%02X. Escape cancels.",
                        collision);
    }
    if (ImGui::SmallButton("Cancel node")) {
      CancelNodePlacement();
    }
    gui::AutoRegisterLastItem("button", "Cancel node");
  }
  if (!ImGui::BeginTabBar("##RailTools")) {
    return;
  }
  if (ImGui::BeginTabItem("Rails")) {
    ImGui::Checkbox("Auto collision", &automatic_rail_collision_);
    gui::AutoRegisterLastItem("checkbox", "Auto collision");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Rails pair their verified collision with placement and Undo. "
          "Existing stops, switches and unrelated collision are preserved.");
    }
    if (draw_rail_palette_ && draw_rail_palette_()) {
      set_canvas_editing(false);
      set_collision_stamping(false);
      CancelNodePlacement();
    }
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("Nodes")) {
    DrawNodeTools();
    ImGui::EndTabItem();
  }
  if (!ImGui::BeginTabItem("Behavior")) {
    ImGui::EndTabBar();
    return;
  }
  bool stamping = collision_stamping_;
  ImGui::BeginDisabled(!apply_collision_stamp_);
  if (ImGui::Checkbox("Stamp rail behavior", &stamping)) {
    set_collision_stamping(stamping);
  }
  gui::AutoRegisterLastItem("checkbox", "Stamp rail behavior");
  ImGui::EndDisabled();
  if (!collision_stamping_) {
    ImGui::EndTabItem();
    ImGui::EndTabBar();
    return;
  }

  struct Behavior {
    uint8_t value;
    const char* label;
  };
  static constexpr Behavior kBehaviors[] = {
      {0xB0, "Straight: horizontal"},
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
      {0xBB, "Junction: south / east / west"},
      {0xBC, "Junction: north / east / west"},
      {0xBD, "Junction: north / south / west"},
      {0xBE, "Junction: north / south / east"},
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
  ImGui::TextDisabled("Click to stamp. Drag a matching node to move it.");
  ImGui::EndTabItem();
  ImGui::EndTabBar();
}

void MinecartTrackEditorPanel::DrawNodeTools() {
  ImGui::SetNextItemWidth(ImGui::CalcTextSize("00000").x +
                          2 * ImGui::GetStyle().FramePadding.x);
  const bool cart_id_changed = ImGui::InputInt("Cart ID", &node_id_, 0, 0);
  node_id_ = std::clamp(node_id_, 0, 31);
  if (cart_id_changed)
    RefreshPlacementToolIdentity(MinecartNodeKind::kCartStart);
  gui::AutoRegisterLastItem("input", "Cart ID");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Cart identity and route slot, 0-31. "
        "Move an existing cart using its room marker.");
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!available_node_id_);
  if (ImGui::SmallButton("Unused##CartId")) {
    ReportToolStatus(SelectAvailableNodeId(MinecartNodeKind::kCartStart));
  }
  gui::AutoRegisterLastItem("button", "Find unused cart ID");
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(
        "Choose a reserved route slot after checking every room's carts.");
  }
  ImGui::EndDisabled();
  ImGui::SetNextItemWidth(ImGui::CalcTextSize("00000").x +
                          2 * ImGui::GetStyle().FramePadding.x);
  const bool channel_changed = ImGui::InputInt("Channel", &node_channel_, 0, 0);
  node_channel_ = std::clamp(node_channel_, 0, 31);
  if (channel_changed)
    RefreshPlacementToolIdentity(MinecartNodeKind::kSwitch);
  gui::AutoRegisterLastItem("input", "Switch channel");
  ImGui::SameLine();
  ImGui::BeginDisabled(!available_node_id_);
  if (ImGui::SmallButton("Unused##SwitchChannel")) {
    ReportToolStatus(SelectAvailableNodeId(MinecartNodeKind::kSwitch));
  }
  gui::AutoRegisterLastItem("button", "Find unused switch channel");
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(
        "Choose a free channel through the project's switch validation.");
  }
  ImGui::EndDisabled();
  ImGui::SetNextItemWidth(-1);
  if (ImGui::Combo("##Departure", &node_departure_,
                   "Depart south\0Depart north\0Depart east\0Depart west\0") &&
      selected_placement_tool_ &&
      (selected_placement_tool_->collision_only ||
       selected_placement_tool_->kind == MinecartNodeKind::kCartStart)) {
    CancelNodeDrag();
    const auto stop = static_cast<uint8_t>(0xB7 + node_departure_);
    selected_placement_tool_->collision = stop;
    selected_placement_tool_->node_collision = stop;
    BindPlacementBrushToRoom();
  }
  gui::AutoRegisterLastItem("combo", "Departure");
  ImGui::SetNextItemWidth(-1);
  if (ImGui::Combo("##SwitchOrientation", &node_switch_orientation_,
                   "Switch: right / down\0Switch: right / up\0"
                   "Switch: left / down\0Switch: left / up\0") &&
      selected_placement_tool_ && !selected_placement_tool_->collision_only &&
      selected_placement_tool_->kind == MinecartNodeKind::kSwitchTrack) {
    CancelNodeDrag();
    selected_placement_tool_->node_collision =
        static_cast<uint8_t>(0xD0 + node_switch_orientation_);
    BindPlacementBrushToRoom();
  }
  gui::AutoRegisterLastItem("combo", "Switch orientation");
  auto row = [&](const char* label, const char* help,
                 MinecartToolPayload payload) {
    ImGui::BeginDisabled(payload.collision_only ? !apply_collision_stamp_
                                                : !place_node_);
    const bool selected =
        selected_placement_tool_ &&
        selected_placement_tool_->collision_only == payload.collision_only &&
        (payload.collision_only
             ? selected_placement_tool_->collision == payload.collision &&
                   selected_placement_tool_->size == payload.size
             : selected_placement_tool_->kind == payload.node.kind &&
                   selected_placement_tool_->id == payload.node.id &&
                   selected_placement_tool_->node_collision ==
                       payload.node.collision);
    if (ImGui::Selectable(label, selected, 0,
                          ImVec2(0, ImGui::GetFrameHeight()))) {
      SetPlacementBrush(payload);
    }
    gui::AutoRegisterLastItem("selectable", label);
    if (ImGui::BeginDragDropSource()) {
      payload.owner = this;
      payload.revision = canvas_revision_;
      payload.room_id = active_room_;
      ImGui::SetDragDropPayload(kMinecartNodeDragPayload, &payload,
                                sizeof(payload), ImGuiCond_Once);
      ImGui::TextUnformatted(label);
      ImGui::EndDragDropSource();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s\nDrag to the room, or click then place.", help);
    }
    ImGui::EndDisabled();
  };
  const auto stop = static_cast<uint8_t>(0xB7 + node_departure_);
  const auto rotating = static_cast<uint8_t>(0xD0 + node_switch_orientation_);
  row("Cart spawn", "Adds a cart, its route start and a stop.",
      {{MinecartNodeKind::kCartStart, node_id_, stop}});
  row("End stop", "Sets the cart's departure direction.", {{}, true, stop, 1});
  row("Switch", "Links a lever to this channel.",
      {{MinecartNodeKind::kSwitch, node_channel_, 0}});
  row("Switch track", "Places a rotating track and its collision.",
      {{MinecartNodeKind::kSwitchTrack, node_channel_, rotating}});
  ImGui::TextDisabled("Choose rail corners and T junctions in Rails.");
}

}  // namespace yaze::editor
