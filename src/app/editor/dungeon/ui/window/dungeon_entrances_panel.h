#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_entrance_camera.h"
#include "app/editor/dungeon/dungeon_entrance_edit_policy.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_render_context.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
#include "zelda3/common.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonEntrancesPanel
 * @brief WindowContent for displaying and editing dungeon entrances
 *
 * This panel provides a list of all dungeon entrances with their properties.
 * Users can select entrances to navigate to their associated rooms.
 *
 * @see WindowContent - Base interface
 */
class DungeonEntrancesPanel : public WindowContent {
 public:
  DungeonEntrancesPanel(
      std::array<zelda3::RoomEntrance, zelda3::kNumDungeonEntranceSlots>*
          entrances,
      std::array<zelda3::DungeonSpawnPoint, zelda3::kNumDungeonSpawnPoints>*
          spawn_points,
      int* current_entrance_id, std::function<void(int)> on_entrance_selected)
      : entrances_(entrances),
        spawn_points_(spawn_points),
        current_entrance_id_(current_entrance_id),
        on_entrance_selected_(std::move(on_entrance_selected)) {}

  // ==========================================================================
  // WindowContent Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.entrance_properties"; }
  std::string GetDisplayName() const override { return "Entrances"; }
  std::string GetIcon() const override { return ICON_MD_DOOR_FRONT; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 26; }
  std::string GetWorkflowGroup() const override { return "Core"; }

  using CameraStateProvider =
      std::function<std::optional<DungeonEntranceCameraState>(int)>;
  using CameraRepairCallback = std::function<absl::Status(int)>;
  using EntrancePropertyEditCallback = std::function<void(
      int, const zelda3::RoomEntrance&, const zelda3::RoomEntrance&)>;
  using SpawnPropertyEditCallback = std::function<void(
      int, const zelda3::DungeonSpawnPoint&, const zelda3::DungeonSpawnPoint&)>;

  void SetPropertyEditCallbacks(EntrancePropertyEditCallback entrance_callback,
                                SpawnPropertyEditCallback spawn_callback) {
    entrance_property_edit_callback_ = std::move(entrance_callback);
    spawn_property_edit_callback_ = std::move(spawn_callback);
  }

  std::shared_ptr<const int> property_edit_gesture() const {
    return property_edit_gesture_;
  }

  void SetCameraTools(CameraStateProvider state_provider,
                      CameraRepairCallback repair_callback,
                      std::function<void()> data_changed_callback = {}) {
    camera_state_provider_ = std::move(state_provider);
    camera_repair_callback_ = std::move(repair_callback);
    camera_data_changed_callback_ = std::move(data_changed_callback);
  }
  bool OwnsNavigationShortcutFocus() const {
    if (!navigation_shortcut_focus_ || ImGui::GetCurrentContext() == nullptr) {
      return false;
    }
    return navigation_shortcut_last_draw_frame_ >= ImGui::GetFrameCount() - 1;
  }

  void SetGraphicsPreview(std::function<DungeonRenderContext()> context,
                          std::function<bool()> automatic,
                          std::function<void(int)> select) {
    preview_context_ = std::move(context);
    preview_automatic_ = std::move(automatic);
    select_preview_ = std::move(select);
  }

  void SetProject(const project::YazeProject* project) { project_ = project; }

  // Shared preview selector: regular IDs are not the combined list slots,
  // whose first entries are dedicated spawn points.
  void DrawGraphicsContext(float width = -1.0f) {
    if (!entrances_ || !spawn_points_ || !current_entrance_id_)
      return;
    if (*current_entrance_id_ < 0 ||
        *current_entrance_id_ >= static_cast<int>(entrances_->size()))
      *current_entrance_id_ = zelda3::kNumDungeonSpawnPoints;
    const int selected = *current_entrance_id_;
    const uint8_t main_gfx = selected < zelda3::kNumDungeonSpawnPoints
                                 ? (*spawn_points_)[selected].main_gfx
                                 : (*entrances_)[selected].blockset_;
    std::string preview = absl::StrFormat(
        "%s / GFX %02X", DungeonEntrancePreviewLabel(selected), main_gfx);
    const bool automatic = preview_automatic_ && preview_automatic_();
    if (preview_context_) {
      const auto context = preview_context_();
      const std::string name =
          automatic && !context.owner_name.empty() ? context.owner_name
          : context.entrance_slot >= zelda3::kNumDungeonSpawnPoints
              ? EntranceLabel(context.entrance_slot)
              : DungeonEntrancePreviewLabel(context.entrance_slot);
      preview =
          context.uses_entrance()
              ? absl::StrFormat(
                    "%s: %s / %s", automatic ? "Auto" : "Manual", name,
                    DungeonEntrancePreviewLabel(context.entrance_slot))
              : absl::StrFormat(
                    "Auto: %s / GFX %02X",
                    context.source == DungeonRenderContextSource::kAmbiguous
                        ? "Choose entrance"
                        : "Room fallback",
                    context.effective_blockset());
    }
    ImGui::SetNextItemWidth(width);
    const bool open = ImGui::BeginCombo(
        "##GraphicsEntrance", preview.c_str(),
        select_preview_ ? ImGuiComboFlags_HeightLarge : ImGuiComboFlags_None);
    gui::AutoRegisterLastItem("combo", "Graphics entrance");
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip(
          "Auto follows the room's dungeon. Choosing an entrance pins its "
          "graphics until you choose Auto again. This changes only the "
          "preview.");
    if (open) {
      if (select_preview_) {
        if (ImGui::Selectable("Auto (follow room)", automatic))
          select_preview_(-1);
        gui::AutoRegisterLastItem("selectable", "Auto (follow room)");
        ImGui::Separator();
        // Keep Auto visible while the entrance list scrolls to a manual choice.
        ImGui::BeginChild("##EntranceChoices",
                          ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 8));
      }
      for (int slot = 0; slot < static_cast<int>(entrances_->size()); ++slot) {
        const std::string entry = DungeonEntrancePreviewLabel(slot);
        const std::string label = slot >= zelda3::kNumDungeonSpawnPoints
                                      ? entry + " - " + EntranceLabel(slot)
                                      : entry;
        if (ImGui::Selectable(label.c_str(), !automatic && slot == selected)) {
          if (select_preview_) {
            select_preview_(slot);
            ImGui::CloseCurrentPopup();
          } else {
            *current_entrance_id_ = slot;
            if (on_entrance_selected_)
              on_entrance_selected_(slot);
          }
        }
        gui::AutoRegisterLastItem("selectable", entry);
        if (ImGui::IsItemHovered() && slot >= zelda3::kNumDungeonSpawnPoints)
          ImGui::SetTooltip("%s", EntranceLabel(slot).c_str());
        if (slot == selected) {
          ImGui::SetItemDefaultFocus();
          if (select_preview_ && ImGui::IsWindowAppearing())
            ImGui::SetScrollHereY(0.5f);
        }
      }
      if (select_preview_)
        ImGui::EndChild();
      ImGui::EndCombo();
    }
  }

  void DrawPropertiesInWorkbench() {
    if (preview_context_ && preview_automatic_ && preview_automatic_() &&
        !preview_context_().uses_entrance()) {
      ImGui::TextWrapped("Choose an entrance to edit its properties.");
      return;
    }
    if (entrances_ && spawn_points_ && current_entrance_id_ &&
        *current_entrance_id_ >= 0 &&
        *current_entrance_id_ < static_cast<int>(entrances_->size()))
      DrawSelectedProperties();
  }

  // ==========================================================================
  // WindowContent Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    (void)p_open;
    if (!entrances_ || !spawn_points_ || !current_entrance_id_)
      return;
    if (*current_entrance_id_ < 0 ||
        *current_entrance_id_ >= static_cast<int>(entrances_->size())) {
      *current_entrance_id_ = 0;
    }
    navigation_shortcut_last_draw_frame_ = ImGui::GetFrameCount();
    navigation_shortcut_focus_ =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

    const bool split_layout = ImGui::GetContentRegionAvail().x >= 620.0f;
    if (split_layout &&
        ImGui::BeginTable("##EntranceNavigator", 2,
                          ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Entrances", ImGuiTableColumnFlags_WidthFixed,
                              300.0f);
      ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      DrawEntranceList();
      ImGui::TableNextColumn();
      DrawSelectedProperties();
      ImGui::EndTable();
      return;
    }

    if (ImGui::BeginTabBar("##EntranceNavigatorTabs")) {
      if (ImGui::BeginTabItem(ICON_MD_DOOR_FRONT " Entrances")) {
        DrawEntranceList();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(ICON_MD_TUNE " Properties")) {
        DrawSelectedProperties();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }

 private:
  float HexPropertyWidth(int digits) const {
    constexpr char hexadecimal[] = "0123456789ABCDEF";
    float glyph_width = 0.0f;
    for (int index = 0; index < 16; ++index) {
      const char glyph[] = {hexadecimal[index], '\0'};
      glyph_width = std::max(glyph_width, ImGui::CalcTextSize(glyph).x);
    }
    return glyph_width * digits + ImGui::GetStyle().FramePadding.x * 2.0f +
           2.0f;
  }

  void ContinuePropertyRow(const char* label, int digits) const {
    const float right =
        ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float next_width =
        ImGui::CalcTextSize(label).x + spacing + HexPropertyWidth(digits);
    if (ImGui::GetItemRectMax().x + spacing + next_width <= right)
      ImGui::SameLine();
  }

  template <typename T>
  bool DrawHexProperty(const char* label, T* value, const char* widget_label,
                       bool continue_row = false) {
    constexpr int digits = sizeof(T) == 1 ? 2 : 4;
    if (continue_row)
      ContinuePropertyRow(label, digits);
    const float width = HexPropertyWidth(digits);
    // InputScalarLeft draws its label with Text(), so wrapping the label keeps
    // the existing input ID and wheel/typing behavior even for a long label.
    const float label_width =
        std::max(1.0f, ImGui::GetContentRegionAvail().x - width -
                           ImGui::GetStyle().ItemSpacing.x);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + label_width);
    bool changed = false;
    if constexpr (sizeof(T) == 1)
      changed = gui::InputHexByte(label, value, width, true);
    else
      changed = gui::InputHexWord(label, value, width, true);
    ImGui::PopTextWrapPos();
    RegisterPropertyInput(label, widget_label);
    return changed;
  }

  std::string EntranceLabel(int slot) const {
    const int entrance_id = slot - zelda3::kNumDungeonSpawnPoints;
    if (!entrances_ || entrance_id < 0 ||
        slot >= static_cast<int>(entrances_->size()))
      return DungeonEntrancePreviewLabel(slot);
    return dungeon_project_labels::GetEntranceLabel(project_, entrance_id,
                                                    (*entrances_)[slot].room_);
  }

  void DrawCameraRepairPreview() {
    if (!camera_repair_preview_.has_value()) {
      return;
    }
    const auto validation =
        ValidateDungeonEntranceCamera(*camera_repair_preview_);
    if (!ImGui::BeginPopupModal("Camera Repair Preview##DungeonEntrance",
                                nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      return;
    }

    ImGui::TextWrapped(
        "Review the derived camera values before changing this entrance. "
        "The repair is one undoable action.");
    ImGui::Separator();
    if (ImGui::BeginTable(
            "##CameraRepairValues", 3,
            ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("Field");
      ImGui::TableSetupColumn("Current");
      ImGui::TableSetupColumn("Derived");
      ImGui::TableHeadersRow();
      const auto draw_word_row = [](const char* label, uint16_t current,
                                    uint16_t derived) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::Text("0x%04X", current);
        ImGui::TableNextColumn();
        ImGui::Text("0x%04X", derived);
      };
      draw_word_row("Scroll X", camera_repair_preview_->camera_x,
                    validation.expected.camera_x);
      draw_word_row("Scroll Y", camera_repair_preview_->camera_y,
                    validation.expected.camera_y);
      draw_word_row("Trigger X", camera_repair_preview_->trigger_x,
                    validation.expected.trigger_x);
      draw_word_row("Trigger Y", camera_repair_preview_->trigger_y,
                    validation.expected.trigger_y);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted("Quadrant");
      ImGui::TableNextColumn();
      ImGui::Text("0x%02X", camera_repair_preview_->quadrant);
      ImGui::TableNextColumn();
      if (validation.quadrant_repair_safe) {
        ImGui::Text("0x%02X", validation.expected.quadrant);
      } else {
        ImGui::TextUnformatted("Preserved");
      }
      ImGui::EndTable();
    }
    ImGui::TextWrapped(
        "Boundary pages: [%02X %02X %02X %02X | %02X %02X %02X %02X] "
        "-> [%02X %02X %02X %02X | %02X %02X %02X %02X]",
        camera_repair_preview_->boundaries[0],
        camera_repair_preview_->boundaries[1],
        camera_repair_preview_->boundaries[2],
        camera_repair_preview_->boundaries[3],
        camera_repair_preview_->boundaries[4],
        camera_repair_preview_->boundaries[5],
        camera_repair_preview_->boundaries[6],
        camera_repair_preview_->boundaries[7],
        validation.expected.boundaries[0], validation.expected.boundaries[1],
        validation.expected.boundaries[2], validation.expected.boundaries[3],
        validation.expected.boundaries[4], validation.expected.boundaries[5],
        validation.expected.boundaries[6], validation.expected.boundaries[7]);
    if (!camera_repair_error_.empty()) {
      ImGui::TextWrapped("Repair failed: %s", camera_repair_error_.c_str());
    }

    ImGui::BeginDisabled(!validation.can_repair() || !camera_repair_callback_);
    if (ImGui::Button("Apply Safe Repair")) {
      const absl::Status status =
          camera_repair_callback_
              ? camera_repair_callback_(camera_repair_slot_)
              : absl::FailedPreconditionError(
                    "Camera repair callback is unavailable");
      if (status.ok()) {
        camera_repair_preview_.reset();
        camera_repair_slot_ = -1;
        camera_repair_error_.clear();
        ImGui::CloseCurrentPopup();
      } else {
        camera_repair_error_ = std::string(status.message());
      }
    }
    gui::AutoRegisterLastItem("button", "ApplyCameraRepair");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      camera_repair_preview_.reset();
      camera_repair_slot_ = -1;
      camera_repair_error_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  void DrawCameraSafety(int slot_index) {
    if (!camera_state_provider_) {
      return;
    }
    const auto state = camera_state_provider_(slot_index);
    if (!state.has_value()) {
      ImGui::TextDisabled("Camera validation unavailable.");
      return;
    }
    const auto validation = ValidateDungeonEntranceCamera(*state);
    ImGui::SeparatorText("Camera safety");
    ImGui::PushTextWrapPos(0.0f);
    if (!validation.geometry_valid()) {
      ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f),
                         "Unsafe camera geometry");
      for (const auto& error : validation.errors) {
        ImGui::Bullet();
        ImGui::SameLine();
        ImGui::TextWrapped("%s", error.c_str());
      }
      if (validation.can_repair()) {
        ImGui::TextWrapped(
            "The room and player position are valid, so a previewed repair "
            "can replace these derived fields.");
      }
    } else if (validation.matches_derived()) {
      ImGui::TextColored(ImVec4(0.35f, 0.80f, 0.45f, 1.0f),
                         "Camera values match the player position.");
    } else {
      ImGui::TextWrapped(
          "Camera geometry is safe, but %zu derived value group(s) differ.",
          validation.differences.size());
      for (const auto& difference : validation.differences) {
        ImGui::Bullet();
        ImGui::SameLine();
        ImGui::TextWrapped("%s", difference.c_str());
      }
    }

    ImGui::PopTextWrapPos();
    ImGui::BeginDisabled(!validation.can_repair() || !camera_repair_callback_);
    if (ImGui::Button("Preview Camera Repair", ImVec2(-1, 0))) {
      camera_repair_preview_ = *state;
      camera_repair_slot_ = slot_index;
      camera_repair_error_.clear();
      ImGui::OpenPopup("Camera Repair Preview##DungeonEntrance");
    }
    gui::AutoRegisterLastItem("button", "PreviewCameraRepair");
    ImGui::EndDisabled();
    DrawCameraRepairPreview();
  }

  void DrawSelectedProperties() {
    ObservePropertyEditGesture();
    gui::AutoWidgetScope scope("Dungeon/Entrances");
    ImGui::PushID(*current_entrance_id_);
    if (*current_entrance_id_ < zelda3::kNumDungeonSpawnPoints) {
      DrawSpawnPointProperties(*current_entrance_id_);
    } else {
      DrawRegularEntranceProperties(*current_entrance_id_);
    }
    ImGui::PopID();
  }

  bool DrawCameraBoundaries(const char* table_id,
                            const std::array<uint8_t*, 8>& boundaries) {
    bool changed = false;
    const float value_width = HexPropertyWidth(2);
    const float mode_width = ImGui::CalcTextSize("Full room").x;
    const float column_width =
        std::max(value_width, ImGui::CalcTextSize("North").x);
    const bool narrow = ImGui::GetContentRegionAvail().x <
                        mode_width + column_width * 4.0f +
                            ImGui::GetStyle().CellPadding.x * 10.0f;
    if (!ImGui::BeginTable(table_id, narrow ? 2 : 5,
                           ImGuiTableFlags_BordersInnerV |
                               ImGuiTableFlags_SizingStretchProp)) {
      return false;
    }
    if (narrow) {
      ImGui::TableSetupColumn("Boundary", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed,
                              value_width);
    } else {
      ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed,
                              mode_width);
      for (const auto* direction : {"North", "East", "South", "West"})
        ImGui::TableSetupColumn(direction);
      ImGui::TableHeadersRow();
    }
    constexpr std::array<int, 4> kQuadrantOrder = {0, 6, 2, 4};
    constexpr std::array<int, 4> kFullRoomOrder = {1, 7, 3, 5};
    const auto draw_row = [&changed, &boundaries, narrow, value_width, this](
                              const char* label,
                              const std::array<int, 4>& order) {
      if (!narrow) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
      }
      constexpr const char* directions[] = {"North", "East", "South", "West"};
      for (size_t direction = 0; direction < order.size(); ++direction) {
        const int index = order[direction];
        if (narrow) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextWrapped("%s %s", label, directions[direction]);
        }
        ImGui::TableNextColumn();
        ImGui::PushID(index);
        changed |= gui::InputHexByte("##Boundary", boundaries[index],
                                     value_width, true);
        RegisterPropertyInput(
            "##Boundary", absl::StrFormat("CameraBoundary%d", index).c_str());
        ImGui::PopID();
      }
    };
    draw_row("Quadrant", kQuadrantOrder);
    draw_row("Full room", kFullRoomOrder);
    ImGui::EndTable();
    return changed;
  }

  void DrawEntranceList() {
    entrance_filter_.Draw(ICON_MD_SEARCH " Filter",
                          ImGui::GetContentRegionAvail().x);
    constexpr int kNumSpawnPoints = zelda3::kNumDungeonSpawnPoints;
    constexpr int kNumEntrances = zelda3::kNumRegularDungeonEntrances;
    constexpr int kTotalEntries = zelda3::kNumDungeonEntranceSlots;

    if (ImGui::BeginChild("##EntrancesList", ImVec2(0, 0), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      for (int i = 0; i < kTotalEntries; i++) {
        std::string entrance_name;
        if (i < kNumSpawnPoints) {
          entrance_name = absl::StrFormat("Spawn Point %d", i);
        } else {
          const int entrance_id = i - kNumSpawnPoints;
          entrance_name = entrance_id < kNumEntrances
                              ? EntranceLabel(i)
                              : absl::StrFormat("Unknown %d", i);
        }

        const int room_id = i < kNumSpawnPoints ? (*spawn_points_)[i].room_id
                                                : (*entrances_)[i].room_;
        const std::string room_name =
            dungeon_project_labels::GetRoomLabel(project_, room_id);
        const std::string label = absl::StrFormat(
            "%s - %s -> %s (%03X)", DungeonEntrancePreviewLabel(i),
            entrance_name, room_name, room_id);
        if (!entrance_filter_.PassFilter(label.c_str())) {
          continue;
        }

        const bool is_selected = (*current_entrance_id_ == i);
        if (ImGui::Selectable(label.c_str(), is_selected)) {
          *current_entrance_id_ = i;
          if (on_entrance_selected_) {
            on_entrance_selected_(i);
          }
        }
      }
    }
    ImGui::EndChild();
  }

  void DrawSpawnPointProperties(int slot_index) {
    auto& spawn = (*spawn_points_)[slot_index];
    const auto before = spawn;
    const bool properties_editable =
        CanEditDungeonSpawnPoint(slot_index, spawn);
    bool changed = false;

    ImGui::Text(tr("Spawn Point %d"), slot_index);
    if (!properties_editable) {
      ImGui::TextWrapped(tr(
          "Spawn point data is unavailable; reload the ROM before editing."));
    }
    ImGui::BeginDisabled(!properties_editable);

    changed |=
        DrawHexProperty("Entrance ID", &spawn.entrance_id, "SpawnEntranceId");
    changed |= DrawHexProperty("Room ID", &spawn.room_id, "SpawnRoomId");
    changed |= DrawHexProperty("Dungeon ID", &spawn.dungeon_id,
                               "SpawnDungeonId", true);
    changed |= DrawHexProperty("Main GFX", &spawn.main_gfx, "SpawnMainGfx");
    changed |= DrawHexProperty("Song", &spawn.song, "SpawnSong", true);
    changed |= DrawHexProperty("Floor", &spawn.floor, "SpawnFloor", true);

    ImGui::Separator();
    changed |= DrawHexProperty("Player X", &spawn.x_coordinate, "SpawnPlayerX");
    changed |=
        DrawHexProperty("Player Y", &spawn.y_coordinate, "SpawnPlayerY", true);
    changed |=
        DrawHexProperty("Overworld Door Tilemap", &spawn.overworld_door_tilemap,
                        "SpawnDoorTilemap");
    ImGui::EndDisabled();

    CommitSpawnPropertyEdit(slot_index, before, changed);
    DrawCameraSafety(slot_index);

    const auto camera_before = spawn;
    changed = false;
    const bool camera_open = ImGui::CollapsingHeader("Advanced camera data");
    gui::AutoRegisterLastItem("header", "AdvancedCameraData");
    if (camera_open) {
      ImGui::BeginDisabled(!properties_editable);
      changed |= DrawHexProperty("Camera Trigger X", &spawn.camera_trigger_x,
                                 "SpawnCameraTriggerX");
      changed |= DrawHexProperty("Camera Trigger Y", &spawn.camera_trigger_y,
                                 "SpawnCameraTriggerY", true);
      changed |= DrawHexProperty("Horizontal Scroll", &spawn.horizontal_scroll,
                                 "SpawnHorizontalScroll");
      changed |= DrawHexProperty("Vertical Scroll", &spawn.vertical_scroll,
                                 "SpawnVerticalScroll", true);
      changed |= DrawHexProperty("Layer", &spawn.layer, "SpawnLayer");
      changed |= DrawHexProperty("Spawn Quadrant", &spawn.quadrant,
                                 "SpawnQuadrant", true);
      changed |=
          DrawHexProperty("Scroll Controller", &spawn.camera_scroll_controller,
                          "SpawnScrollController", true);

      ImGui::SeparatorText(tr("Camera Boundaries"));
      changed |= DrawCameraBoundaries("##SpawnCameraBoundaries",
                                      {&spawn.camera_scroll_boundaries[0],
                                       &spawn.camera_scroll_boundaries[1],
                                       &spawn.camera_scroll_boundaries[2],
                                       &spawn.camera_scroll_boundaries[3],
                                       &spawn.camera_scroll_boundaries[4],
                                       &spawn.camera_scroll_boundaries[5],
                                       &spawn.camera_scroll_boundaries[6],
                                       &spawn.camera_scroll_boundaries[7]});
      ImGui::EndDisabled();
    }

    CommitSpawnPropertyEdit(slot_index, camera_before, changed);
  }

  void DrawRegularEntranceProperties(int slot_index) {
    auto& entrance = (*entrances_)[slot_index];
    const auto before = entrance;
    const bool properties_editable =
        CanEditDungeonEntrance(slot_index, entrance);
    bool changed = false;

    ImGui::Text(tr("Entrance ID: %04X"), entrance.entrance_id_);
    ImGui::BeginDisabled(!properties_editable);
    changed |= DrawHexProperty("Room ID", &entrance.room_, "RegularRoomId");
    changed |= DrawHexProperty("Dungeon ID", &entrance.dungeon_id_,
                               "RegularDungeonId", true);
    changed |=
        DrawHexProperty("Blockset", &entrance.blockset_, "RegularBlockset");
    changed |= DrawHexProperty("Music", &entrance.music_, "RegularMusic", true);
    changed |= DrawHexProperty("Floor", &entrance.floor_, "RegularFloor", true);

    ImGui::Separator();
    changed |=
        DrawHexProperty("Player X   ", &entrance.x_position_, "RegularPlayerX");
    changed |= DrawHexProperty("Player Y   ", &entrance.y_position_,
                               "RegularPlayerY", true);
    changed |= DrawHexProperty("Exit", &entrance.exit_, "RegularExit");
    ImGui::EndDisabled();

    CommitEntrancePropertyEdit(slot_index, before, changed);
    DrawCameraSafety(slot_index);

    const auto camera_before = entrance;
    changed = false;
    const bool camera_open = ImGui::CollapsingHeader("Advanced camera data");
    gui::AutoRegisterLastItem("header", "AdvancedCameraData");
    if (camera_open) {
      ImGui::BeginDisabled(!properties_editable);
      changed |=
          DrawHexProperty("Camera Trigger X", &entrance.camera_trigger_x_,
                          "RegularCameraTriggerX");
      changed |=
          DrawHexProperty("Camera Trigger Y", &entrance.camera_trigger_y_,
                          "RegularCameraTriggerY", true);
      changed |=
          DrawHexProperty("Scroll X", &entrance.camera_x_, "RegularScrollX");
      changed |= DrawHexProperty("Scroll Y", &entrance.camera_y_,
                                 "RegularScrollY", true);
      changed |= DrawHexProperty("Scroll Quadrant", &entrance.scroll_quadrant_,
                                 "RegularScrollQuadrant");

      ImGui::SeparatorText(tr("Camera Boundaries"));
      changed |= DrawCameraBoundaries(
          "##RegularCameraBoundaries",
          {&entrance.camera_boundary_qn_, &entrance.camera_boundary_fn_,
           &entrance.camera_boundary_qs_, &entrance.camera_boundary_fs_,
           &entrance.camera_boundary_qw_, &entrance.camera_boundary_fw_,
           &entrance.camera_boundary_qe_, &entrance.camera_boundary_fe_});
      ImGui::EndDisabled();
    }

    CommitEntrancePropertyEdit(slot_index, camera_before, changed);
  }

  void CommitEntrancePropertyEdit(int slot_index,
                                  const zelda3::RoomEntrance& before,
                                  bool changed) {
    auto& entrance = (*entrances_)[slot_index];
    if (!MarkDungeonEntranceDirtyIfEditable(slot_index, entrance, changed)) {
      return;
    }
    if (entrance_property_edit_callback_) {
      ObservePropertyEditGesture();
      entrance_property_edit_callback_(slot_index, before, entrance);
    }
    if (camera_data_changed_callback_) {
      camera_data_changed_callback_();
    }
  }

  void CommitSpawnPropertyEdit(int slot_index,
                               const zelda3::DungeonSpawnPoint& before,
                               bool changed) {
    auto& spawn = (*spawn_points_)[slot_index];
    if (!MarkDungeonSpawnPointDirtyIfEditable(slot_index, spawn, changed)) {
      return;
    }
    if (spawn_property_edit_callback_) {
      ObservePropertyEditGesture();
      spawn_property_edit_callback_(slot_index, before, spawn);
    }
    if (camera_data_changed_callback_) {
      camera_data_changed_callback_();
    }
  }

  void ObservePropertyEditGesture();
  void RegisterPropertyInput(const char* field_label, const char* widget_label);

  std::array<zelda3::RoomEntrance, zelda3::kNumDungeonEntranceSlots>*
      entrances_ = nullptr;
  std::array<zelda3::DungeonSpawnPoint, zelda3::kNumDungeonSpawnPoints>*
      spawn_points_ = nullptr;
  int* current_entrance_id_ = nullptr;
  const project::YazeProject* project_ = nullptr;
  std::function<void(int)> on_entrance_selected_;
  std::function<DungeonRenderContext()> preview_context_;
  std::function<bool()> preview_automatic_;
  std::function<void(int)> select_preview_;
  CameraStateProvider camera_state_provider_;
  CameraRepairCallback camera_repair_callback_;
  EntrancePropertyEditCallback entrance_property_edit_callback_;
  SpawnPropertyEditCallback spawn_property_edit_callback_;
  ImGuiID property_edit_active_id_ = 0;
  std::shared_ptr<const int> property_edit_gesture_;
  std::function<void()> camera_data_changed_callback_;
  std::optional<DungeonEntranceCameraState> camera_repair_preview_;
  int camera_repair_slot_ = -1;
  std::string camera_repair_error_;
  bool navigation_shortcut_focus_ = false;
  int navigation_shortcut_last_draw_frame_ = -1;
  ImGuiTextFilter entrance_filter_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCES_PANEL_H_
