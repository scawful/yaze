#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_DEBUG_CONTROLS_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_DEBUG_CONTROLS_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonDebugControlsPanel
 * @brief EditorPanel for dungeon debugging tools and controls
 *
 * This panel provides runtime debug controls for development including
 * logging settings, rendering controls, texture management, and memory info.
 *
 * @see EditorPanel - Base interface
 */
class DungeonDebugControlsPanel : public EditorPanel {
 public:
  DungeonDebugControlsPanel(
      int* current_room_id,
      std::array<zelda3::Room, 0x128>* rooms,
      gfx::IRenderer* renderer,
      std::function<absl::Status()> save_callback,
      std::function<absl::Status(int)> reload_room_callback,
      std::function<void()> close_all_rooms_callback)
      : current_room_id_(current_room_id),
        rooms_(rooms),
        renderer_(renderer),
        save_callback_(std::move(save_callback)),
        reload_room_callback_(std::move(reload_room_callback)),
        close_all_rooms_callback_(std::move(close_all_rooms_callback)) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.debug_controls"; }
  std::string GetDisplayName() const override { return "Debug Controls"; }
  std::string GetIcon() const override { return ICON_MD_BUG_REPORT; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 80; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    ImGui::TextWrapped("Runtime debug controls for development");
    ImGui::Separator();

    // ===== LOGGING CONTROLS =====
    ImGui::SeparatorText(ICON_MD_TERMINAL " Logging");

    bool debug_enabled = util::LogManager::instance().IsDebugEnabled();
    if (ImGui::Checkbox("Enable DEBUG Logs", &debug_enabled)) {
      if (debug_enabled) {
        util::LogManager::instance().EnableDebugLogging();
      } else {
        util::LogManager::instance().DisableDebugLogging();
      }
    }

    const char* log_levels[] = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
    int current_level =
        static_cast<int>(util::LogManager::instance().GetLogLevel());
    if (ImGui::Combo("Log Level", &current_level, log_levels, 5)) {
      util::LogManager::instance().SetLogLevel(
          static_cast<util::LogLevel>(current_level));
    }

    ImGui::Separator();

    // ===== ROOM RENDERING CONTROLS =====
    ImGui::SeparatorText(ICON_MD_IMAGE " Rendering");

    if (current_room_id_ && rooms_ &&
        *current_room_id_ >= 0 &&
        *current_room_id_ < static_cast<int>(rooms_->size())) {
      auto& room = (*rooms_)[*current_room_id_];

      ImGui::Text("Current Room: %03X", *current_room_id_);
      ImGui::Text("Objects: %zu", room.GetTileObjects().size());
      ImGui::Text("Sprites: %zu", room.GetSprites().size());

      if (ImGui::Button(ICON_MD_REFRESH " Force Re-render", ImVec2(-FLT_MIN, 0))) {
        room.LoadRoomGraphics(room.blockset);
        room.LoadObjects();
        room.RenderRoomGraphics();
      }

      if (ImGui::Button(ICON_MD_CLEANING_SERVICES " Clear Room Buffers",
                        ImVec2(-FLT_MIN, 0))) {
        room.ClearTileObjects();
      }

      ImGui::Separator();

      // Floor graphics override
      ImGui::Text("Floor Graphics Override:");
      uint8_t floor1 = room.floor1();
      uint8_t floor2 = room.floor2();
      static uint8_t floor_min = 0;
      static uint8_t floor_max = 15;
      if (ImGui::SliderScalar("Floor1", ImGuiDataType_U8, &floor1, &floor_min,
                              &floor_max)) {
        room.set_floor1(floor1);
        if (room.rom() && room.rom()->is_loaded()) {
          room.RenderRoomGraphics();
        }
      }
      if (ImGui::SliderScalar("Floor2", ImGuiDataType_U8, &floor2, &floor_min,
                              &floor_max)) {
        room.set_floor2(floor2);
        if (room.rom() && room.rom()->is_loaded()) {
          room.RenderRoomGraphics();
        }
      }
    } else {
      ImGui::TextDisabled("No room selected");
    }

    ImGui::Separator();

    // ===== TEXTURE CONTROLS =====
    ImGui::SeparatorText(ICON_MD_TEXTURE " Textures");

    if (ImGui::Button(ICON_MD_DELETE_SWEEP " Process Texture Queue",
                      ImVec2(-FLT_MIN, 0))) {
      gfx::Arena::Get().ProcessTextureQueue(renderer_);
    }

    ImGui::Text("Arena Graphics Sheets: %zu",
                gfx::Arena::Get().gfx_sheets().size());

    ImGui::Separator();

    // ===== QUICK ACTIONS =====
    ImGui::SeparatorText(ICON_MD_FLASH_ON " Quick Actions");

    if (ImGui::Button(ICON_MD_SAVE " Save All Rooms", ImVec2(-FLT_MIN, 0))) {
      if (save_callback_) {
        save_callback_();
      }
    }

    if (ImGui::Button(ICON_MD_REPLAY " Reload Current Room",
                      ImVec2(-FLT_MIN, 0))) {
      if (current_room_id_ && reload_room_callback_) {
        reload_room_callback_(*current_room_id_);
      }
    }

    if (ImGui::Button(ICON_MD_CLOSE " Close All Rooms", ImVec2(-FLT_MIN, 0))) {
      if (close_all_rooms_callback_) {
        close_all_rooms_callback_();
      }
    }
  }

 private:
  int* current_room_id_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  gfx::IRenderer* renderer_ = nullptr;
  std::function<absl::Status()> save_callback_;
  std::function<absl::Status(int)> reload_room_callback_;
  std::function<void()> close_all_rooms_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_DEBUG_CONTROLS_PANEL_H_
