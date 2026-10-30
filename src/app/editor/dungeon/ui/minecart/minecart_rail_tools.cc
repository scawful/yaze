#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"

#include <algorithm>

#include "app/gui/automation/widget_auto_register.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/custom_object.h"

namespace yaze::editor {

void MinecartTrackEditorPanel::CancelAssemblyPreview() {
  drawing_path_ = placing_assembly_ = false;
  path_waypoints_.clear();
  assembly_preview_.reset();
  assembly_room_ = preview_x_ = preview_y_ = -1;
  assembly_hover_x_ = assembly_hover_y_ = -1;
  assembly_error_.clear();
  ++assembly_preview_revision_;
}

void MinecartTrackEditorPanel::BeginRailPath(bool pit) {
  CancelNodePlacement();
  set_canvas_editing(false);
  collision_stamping_ = false;
  drawing_path_ = true;
  path_pit_ = pit;
  assembly_room_ = active_room_;
  assembly_revision_ = canvas_revision_;
}

void MinecartTrackEditorPanel::UpdateAssemblyPreview(int room, int x, int y,
                                                     bool click) {
  if (!assembly_tool_active() || !preview_assembly_)
    return;
  if (drawing_path_ &&
      (room != assembly_room_ || canvas_revision_ != assembly_revision_)) {
    CancelAssemblyPreview();
    assembly_error_ = "Room changed; begin the path again";
    return;
  }
  if (placing_assembly_ && room != assembly_room_) {
    assembly_room_ = room;
    assembly_revision_ = canvas_revision_;
    preview_x_ = preview_y_ = -1;
    assembly_hover_x_ = assembly_hover_y_ = -1;
    assembly_preview_.reset();
  }
  if (placing_assembly_ && canvas_revision_ != assembly_revision_) {
    CancelAssemblyPreview();
    assembly_error_ = "Project or tool changed; choose the assembly again";
    return;
  }
  if (drawing_path_ && !click)
    return;
  if (placing_assembly_ && !click && preview_x_ >= 0)
    return;
  if (placing_assembly_ && !click && x == assembly_hover_x_ &&
      y == assembly_hover_y_)
    return;
  assembly_hover_x_ = x;
  assembly_hover_y_ = y;
  auto points = path_waypoints_;
  std::optional<DungeonSelectionClipboard> clipboard;
  if (drawing_path_) {
    if (!points.empty() && points.back() == std::pair{x, y})
      return;
    points.emplace_back(x, y);
    if (points.size() == 1) {
      path_waypoints_ = points;
      return;
    }
    auto built = BuildMinecartRailPath(
        points,
        path_pit_ ? zelda3::MinecartRailStyle::kPit
                  : zelda3::MinecartRailStyle::kFloor,
        zelda3::CustomObjectManager::Get().GetMinecartRailProfile());
    if (!built.ok()) {
      assembly_error_ = std::string(built.status().message());
      assembly_preview_.reset();
      return;
    }
    clipboard = std::move(*built);
    x = points.front().first;
    y = points.front().second;
  } else {
    clipboard = assembly_copy_;
  }
  if (!clipboard)
    return;
  auto plan = preview_assembly_(room, *clipboard, x, y, assembly_mapping_);
  ++assembly_preview_revision_;
  if (!plan.ok()) {
    assembly_error_ = std::string(plan.status().message());
    assembly_preview_.reset();
    return;
  }
  assembly_error_.clear();
  assembly_preview_ = std::move(*plan);
  if (drawing_path_)
    path_waypoints_ = std::move(points);
  else if (click) {
    preview_x_ = x;
    preview_y_ = y;
  }
}

absl::Status MinecartTrackEditorPanel::ApplyAssemblyPreview() {
  if (!assembly_preview_ || !apply_assembly_)
    return absl::FailedPreconditionError("Create a valid rail preview first");
  const auto snapshot = *assembly_preview_;
  auto result = apply_assembly_(snapshot);
  if (result.ok())
    CancelAssemblyPreview();
  else {
    assembly_error_ = std::string(result.message());
    assembly_preview_.reset();
  }
  return result;
}

void MinecartTrackEditorPanel::DrawAssemblyTools() {
  ImGui::BeginDisabled(!preview_assembly_);
  if (ImGui::Button("Draw floor path"))
    BeginRailPath(false);
  gui::AutoRegisterLastItem("button", "Draw floor path");
  ImGui::SameLine();
  if (ImGui::Button("Draw pit path"))
    BeginRailPath(true);
  gui::AutoRegisterLastItem("button", "Draw pit path");
  ImGui::EndDisabled();
  ImGui::BeginDisabled(!capture_assembly_);
  if (ImGui::Button("Capture selected assembly")) {
    auto copied = capture_assembly_(active_room_);
    if (!copied.ok())
      assembly_error_ = std::string(copied.status().message());
    else {
      CancelAssemblyPreview();
      assembly_copy_ = std::move(*copied);
      assembly_mapping_ = {};
      for (const auto& sprite : assembly_copy_->sprites)
        (sprite.id == 0xA3 ? assembly_mapping_.carts
                           : assembly_mapping_.channels)[sprite.subtype] = -1;
    }
  }
  gui::AutoRegisterLastItem("button", "Capture selected assembly");
  ImGui::EndDisabled();
  if (assembly_copy_) {
    ImGui::TextWrapped(
        "Copied %zu rails and %zu nodes. Map new identities before preview.",
        assembly_copy_->objects.size(), assembly_copy_->sprites.size());
    auto mapping = [&](std::map<int, int>& ids, const char* label) {
      for (auto& [source, target] : ids) {
        const auto name = std::string(label) + " " + std::to_string(source) +
                          " ->##" + label + std::to_string(source);
        ImGui::SetNextItemWidth(68);
        if (ImGui::InputInt(name.c_str(), &target, 0, 0)) {
          target = std::clamp(target, -1, 31);
          assembly_preview_.reset();
          preview_x_ = preview_y_ = -1;
          assembly_hover_x_ = assembly_hover_y_ = -1;
        }
        gui::AutoRegisterLastItem("input", name);
      }
    };
    mapping(assembly_mapping_.carts, "Cart");
    mapping(assembly_mapping_.channels, "Channel");
    if (ImGui::Button("Place translated copy")) {
      CancelNodePlacement();
      set_canvas_editing(false);
      collision_stamping_ = false;
      placing_assembly_ = true;
      assembly_room_ = active_room_;
      assembly_revision_ = canvas_revision_;
    }
    gui::AutoRegisterLastItem("button", "Place translated copy");
  }
  if (assembly_tool_active()) {
    ImGui::TextWrapped("%s", drawing_path_
                                 ? "Click orthogonal waypoints on the 16px "
                                   "grid. Apply commits the clicked path."
                                 : "Click a target anchor. Preview includes "
                                   "art, collision and mapped nodes.");
    ImGui::BeginDisabled(!assembly_preview_ || !apply_assembly_);
    if (ImGui::Button("Apply assembly"))
      ReportToolStatus(ApplyAssemblyPreview());
    gui::AutoRegisterLastItem("button", "Apply assembly");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel preview"))
      CancelAssemblyPreview();
    gui::AutoRegisterLastItem("button", "Cancel preview");
    if (assembly_preview_)
      ImGui::Text("Preview: %zu new rails, %zu new nodes",
                  assembly_preview_->selection.after.objects.size() -
                      assembly_preview_->selection.before.objects.size(),
                  assembly_preview_->selection.after.sprites.size() -
                      assembly_preview_->selection.before.sprites.size());
  }
  if (!assembly_error_.empty())
    ImGui::TextWrapped("%s", assembly_error_.c_str());
}

void MinecartTrackEditorPanel::DrawRailTools() {
  DrawAssemblyTools();
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
