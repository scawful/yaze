#include "overworld_editor.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <future>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/core/asar_wrapper.h"
#include "app/core/features.h"
#include "app/core/platform/clipboard.h"
#include "app/core/window.h"
#include "app/editor/overworld/entity.h"
#include "app/editor/overworld/map_properties.h"
#include "app/gfx/arena.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/tilemap.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "app/zelda3/common.h"
#include "app/zelda3/overworld/overworld.h"
#include "app/zelda3/overworld/overworld_map.h"
#include "imgui/imgui.h"
#include "imgui_memory_editor.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze::editor {

using core::Renderer;
using namespace ImGui;

constexpr float kInputFieldSize = 30.f;

void OverworldEditor::Initialize() {
  // Initialize MapPropertiesSystem with canvas and bitmap data
  map_properties_system_ = std::make_unique<MapPropertiesSystem>(
      &overworld_, rom_, &maps_bmp_, &ow_map_canvas_);

  // Initialize OverworldEditorManager for v3 features
  overworld_manager_ =
      std::make_unique<OverworldEditorManager>(&overworld_, rom_, this);

  // Setup overworld canvas context menu
  SetupOverworldCanvasContextMenu();

  // Core editing tools
  gui::AddTableColumn(toolset_table_, "##Pan", [&]() {
    if (Selectable(ICON_MD_PAN_TOOL_ALT, current_mode == EditingMode::PAN)) {
      current_mode = EditingMode::PAN;
      ow_map_canvas_.set_draggable(true);
    }
    HOVER_HINT("Pan (1) - Middle click and drag");
  });
  gui::AddTableColumn(toolset_table_, "##DrawTile", [&]() {
    if (Selectable(ICON_MD_DRAW, current_mode == EditingMode::DRAW_TILE)) {
      current_mode = EditingMode::DRAW_TILE;
    }
    HOVER_HINT("Draw Tile (2)");
  });
  gui::AddTableColumn(toolset_table_, "##Entrances", [&]() {
    if (Selectable(ICON_MD_DOOR_FRONT, current_mode == EditingMode::ENTRANCES))
      current_mode = EditingMode::ENTRANCES;
    HOVER_HINT("Entrances (3)");
  });
  gui::AddTableColumn(toolset_table_, "##Exits", [&]() {
    if (Selectable(ICON_MD_DOOR_BACK, current_mode == EditingMode::EXITS))
      current_mode = EditingMode::EXITS;
    HOVER_HINT("Exits (4)");
  });
  gui::AddTableColumn(toolset_table_, "##Items", [&]() {
    if (Selectable(ICON_MD_GRASS, current_mode == EditingMode::ITEMS))
      current_mode = EditingMode::ITEMS;
    HOVER_HINT("Items (5)");
  });
  gui::AddTableColumn(toolset_table_, "##Sprites", [&]() {
    if (Selectable(ICON_MD_PEST_CONTROL_RODENT,
                   current_mode == EditingMode::SPRITES))
      current_mode = EditingMode::SPRITES;
    HOVER_HINT("Sprites (6)");
  });
  gui::AddTableColumn(toolset_table_, "##Transports", [&]() {
    if (Selectable(ICON_MD_ADD_LOCATION,
                   current_mode == EditingMode::TRANSPORTS))
      current_mode = EditingMode::TRANSPORTS;
    HOVER_HINT("Transports (7)");
  });
  gui::AddTableColumn(toolset_table_, "##Music", [&]() {
    if (Selectable(ICON_MD_MUSIC_NOTE, current_mode == EditingMode::MUSIC))
      current_mode = EditingMode::MUSIC;
    HOVER_HINT("Music (8)");
  });

  // View controls
  gui::AddTableColumn(toolset_table_, "##ZoomOut", [&]() {
    if (Button(ICON_MD_ZOOM_OUT))
      ow_map_canvas_.ZoomOut();
    HOVER_HINT("Zoom Out");
  });
  gui::AddTableColumn(toolset_table_, "##ZoomIn", [&]() {
    if (Button(ICON_MD_ZOOM_IN))
      ow_map_canvas_.ZoomIn();
    HOVER_HINT("Zoom In");
  });
  gui::AddTableColumn(toolset_table_, "##Fullscreen", [&]() {
    if (Button(ICON_MD_OPEN_IN_FULL))
      overworld_canvas_fullscreen_ = !overworld_canvas_fullscreen_;
    HOVER_HINT("Fullscreen Canvas (F11)");
  });

  // Quick access tools
  gui::AddTableColumn(toolset_table_, "##Tile16Editor", [&]() {
    if (Button(ICON_MD_GRID_VIEW))
      show_tile16_editor_ = !show_tile16_editor_;
    HOVER_HINT("Tile16 Editor (Ctrl+T)");
  });
  gui::AddTableColumn(toolset_table_, "##CopyMap", [&]() {
    if (Button(ICON_MD_CONTENT_COPY)) {
#if YAZE_LIB_PNG == 1
      std::vector<uint8_t> png_data = maps_bmp_[current_map_].GetPngData();
      if (png_data.size() > 0) {
        core::CopyImageToClipboard(png_data);
      } else {
        status_ = absl::InternalError(
            "Failed to convert overworld map surface to PNG");
      }
#else
      status_ = absl::UnimplementedError("PNG export not available in this build");
#endif
    }
    HOVER_HINT("Copy Map to Clipboard");
  });
}

absl::Status OverworldEditor::Load() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  RETURN_IF_ERROR(LoadGraphics());
  RETURN_IF_ERROR(
      tile16_editor_.Initialize(tile16_blockset_bmp_, current_gfx_bmp_,
                                *overworld_.mutable_all_tiles_types()));
  ASSIGN_OR_RETURN(entrance_tiletypes_, zelda3::LoadEntranceTileTypes(rom_));
  all_gfx_loaded_ = true;
  return absl::OkStatus();
}

absl::Status OverworldEditor::Update() {
  status_ = absl::OkStatus();
  if (overworld_canvas_fullscreen_) {
    DrawFullscreenCanvas();
    return status_;
  }

  // Replace ZEML with pure ImGui layout
  if (ImGui::BeginTabBar("##OwEditorTabBar")) {
    if (ImGui::BeginTabItem("Map Editor")) {
      DrawToolset();

      if (ImGui::BeginTable(
              "##owEditTable", 2,
              ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                  ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
                  ImGuiTableFlags_BordersV)) {
        ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Tile Selector",
                                ImGuiTableColumnFlags_WidthFixed, 256.0f);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawOverworldCanvas();

        ImGui::TableNextColumn();
        status_ = DrawTileSelector();

        ImGui::EndTable();
      }
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Tile16 Editor")) {
      if (rom_->is_loaded()) {
        status_ = tile16_editor_.Update();
      }
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Graphics Group Editor")) {
      if (rom_->is_loaded()) {
        status_ = gfx_group_editor_.Update();
      }
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Usage Statistics")) {
      if (rom_->is_loaded()) {
        status_ = UpdateUsageStats();
      }
      ImGui::EndTabItem();
    }

    // Add v3 settings tab
    if (rom_->is_loaded()) {
      status_ = overworld_manager_->DrawV3SettingsPanel();
    }

    ImGui::EndTabBar();
  }

  return status_;
}

void OverworldEditor::DrawFullscreenCanvas() {
  static bool use_work_area = true;
  static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoSavedSettings;
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos : viewport->Pos);
  ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize : viewport->Size);
  if (ImGui::Begin("Fullscreen Overworld Editor", &overworld_canvas_fullscreen_,
                   flags)) {
    // Draws the toolset for editing the Overworld.
    DrawToolset();
    DrawOverworldCanvas();
  }
  ImGui::End();
}

void OverworldEditor::DrawToolset() {
  gui::DrawTable(toolset_table_);

  if (show_tile16_editor_) {
    ImGui::Begin("Tile16 Editor", &show_tile16_editor_,
                 ImGuiWindowFlags_MenuBar);
    status_ = tile16_editor_.Update();
    ImGui::End();
  }

  if (show_gfx_group_editor_) {
    gui::BeginWindowWithDisplaySettings("Gfx Group Editor",
                                        &show_gfx_group_editor_);
    status_ = gfx_group_editor_.Update();
    gui::EndWindowWithDisplaySettings();
  }

  if (show_properties_editor_) {
    ImGui::Begin("Properties", &show_properties_editor_);
    DrawOverworldProperties();
    ImGui::End();
  }

  if (show_custom_bg_color_editor_) {
    ImGui::Begin("Custom Background Colors", &show_custom_bg_color_editor_);
    DrawCustomBackgroundColorEditor();
    ImGui::End();
  }

  if (show_overlay_editor_) {
    ImGui::Begin("Overlay Editor", &show_overlay_editor_);
    DrawOverlayEditor();
    ImGui::End();
  }

  if (show_map_properties_panel_) {
    ImGui::Begin("Map Properties Panel", &show_map_properties_panel_);
    DrawMapPropertiesPanel();
    ImGui::End();
  }

  // Keyboard shortcuts for the Overworld Editor
  if (!ImGui::IsAnyItemActive()) {
    using enum EditingMode;

    // Tool shortcuts
    if (ImGui::IsKeyDown(ImGuiKey_1)) {
      current_mode = PAN;
    } else if (ImGui::IsKeyDown(ImGuiKey_2)) {
      current_mode = DRAW_TILE;
    } else if (ImGui::IsKeyDown(ImGuiKey_3)) {
      current_mode = ENTRANCES;
    } else if (ImGui::IsKeyDown(ImGuiKey_4)) {
      current_mode = EXITS;
    } else if (ImGui::IsKeyDown(ImGuiKey_5)) {
      current_mode = ITEMS;
    } else if (ImGui::IsKeyDown(ImGuiKey_6)) {
      current_mode = SPRITES;
    } else if (ImGui::IsKeyDown(ImGuiKey_7)) {
      current_mode = TRANSPORTS;
    } else if (ImGui::IsKeyDown(ImGuiKey_8)) {
      current_mode = MUSIC;
    }

    // View shortcuts
    if (ImGui::IsKeyDown(ImGuiKey_F11)) {
      overworld_canvas_fullscreen_ = !overworld_canvas_fullscreen_;
    }

    // Toggle map lock with L key
    if (ImGui::IsKeyDown(ImGuiKey_L) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
      current_map_lock_ = !current_map_lock_;
    }

    // Toggle Tile16 editor with T key
    if (ImGui::IsKeyDown(ImGuiKey_T) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
      show_tile16_editor_ = !show_tile16_editor_;
    }
  }
}

// Column names for different ROM versions
constexpr std::array<const char*, 6> kVanillaMapSettingsColumnNames = {
    "##WorldId", "##GfxId", "##PalId", "##SprGfxId", "##SprPalId", "##MsgId"};

constexpr std::array<const char*, 7> kV2MapSettingsColumnNames = {
    "##WorldId",  "##GfxId",    "##PalId", "##MainPalId",
    "##SprGfxId", "##SprPalId", "##MsgId"};

constexpr std::array<const char*, 9> kV3MapSettingsColumnNames = {
    "##WorldId",  "##GfxId", "##PalId",   "##MainPalId", "##SprGfxId",
    "##SprPalId", "##MsgId", "##AnimGfx", "##AreaSize"};

void OverworldEditor::DrawOverworldMapSettings() {
  static uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];

  // Determine column count and names based on ROM version
  int column_count = 6;  // Vanilla
  if (asm_version >= 2 && asm_version != 0xFF)
    column_count = 7;  // v2
  if (asm_version >= 3 && asm_version != 0xFF)
    column_count = 9;  // v3

  if (BeginTable(kOWMapTable.data(), column_count, kOWMapFlags, ImVec2(0, 0),
                 -1)) {
    // Setup columns based on version
    if (asm_version == 0xFF) {
      // Vanilla ROM
      for (const auto& name : kVanillaMapSettingsColumnNames)
        ImGui::TableSetupColumn(name);
    } else if (asm_version >= 3) {
      // v3+ ROM
      for (const auto& name : kV3MapSettingsColumnNames)
        ImGui::TableSetupColumn(name);
    } else if (asm_version >= 2) {
      // v2 ROM
      for (const auto& name : kV2MapSettingsColumnNames)
        ImGui::TableSetupColumn(name);
    }

    // Header with ROM version indicator and upgrade option
    if (asm_version == 0xFF) {
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Vanilla ROM");
      if (ImGui::Button(ICON_MD_UPGRADE " Upgrade to v3")) {
        // Show upgrade dialog
        ImGui::OpenPopup("UpgradeROMVersion");
      }
      HOVER_HINT("Upgrade ROM to support ZSCustomOverworld features");
    } else {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                         "ZSCustomOverworld v%d", asm_version);
      if (asm_version < 3 && ImGui::Button(ICON_MD_UPGRADE " Upgrade to v3")) {
        ImGui::OpenPopup("UpgradeROMVersion");
      }
    }

    // ROM Upgrade Dialog
    if (ImGui::BeginPopupModal("UpgradeROMVersion", NULL,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Upgrade ROM to ZSCustomOverworld v3");
      ImGui::Separator();
      ImGui::Text("This will enable advanced features like:");
      ImGui::BulletText("Custom area sizes (1x1, 2x2, 2x1, 1x2)");
      ImGui::BulletText("Enhanced palette controls");
      ImGui::BulletText("Animated graphics support");
      ImGui::BulletText("Custom background colors");
      ImGui::BulletText("Advanced overlay system");
      ImGui::Separator();

      // Show ASM application option if feature flag is enabled
      if (core::FeatureFlags::get().overworld.kApplyZSCustomOverworldASM) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                           ICON_MD_CODE " ASM Patch Application Enabled");
        ImGui::Text(
            "ZSCustomOverworld ASM will be automatically applied to ROM");
        ImGui::Separator();
      } else {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                           ICON_MD_INFO " ASM Patch Application Disabled");
        ImGui::Text("Only version marker will be set. Enable in Feature Flags");
        ImGui::Text("for full ASM functionality.");
        ImGui::Separator();
      }

      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                         "Warning: This will modify your ROM!");

      if (ImGui::Button(ICON_MD_CHECK " Upgrade", ImVec2(120, 0))) {
        // Apply ASM if feature flag is enabled
        if (core::FeatureFlags::get().overworld.kApplyZSCustomOverworldASM) {
          auto asm_status = ApplyZSCustomOverworldASM(3);
          if (!asm_status.ok()) {
            // Show error but still set version marker
            util::logf("Failed to apply ZSCustomOverworld ASM: %s",
                       asm_status.ToString().c_str());
          }
        }

        // Set the ROM version marker
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied] = 3;
        asm_version = 3;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_CANCEL " Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    // World selector (always present)
    TableNextColumn();
    ImGui::SetNextItemWidth(120.f);
    if (ImGui::Combo("##world", &current_world_, kWorldList.data(), 3)) {
      // Update current map when world changes
      RefreshOverworldMap();
    }

    // Area Graphics (always present)
    TableNextColumn();
    ImGui::BeginGroup();
    if (gui::InputHexByte(ICON_MD_IMAGE " Graphics",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_area_graphics(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    ImGui::EndGroup();

    // Area Palette (always present)
    TableNextColumn();
    ImGui::BeginGroup();
    if (gui::InputHexByte(ICON_MD_PALETTE " Palette",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_area_palette(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      status_ = RefreshMapPalette();
      RefreshOverworldMap();
    }
    ImGui::EndGroup();

    // Main Palette (v2+ only)
    if (asm_version >= 2 && asm_version != 0xFF) {
      TableNextColumn();
      ImGui::BeginGroup();
      if (gui::InputHexByte(ICON_MD_COLOR_LENS " Main Pal",
                            overworld_.mutable_overworld_map(current_map_)
                                ->mutable_main_palette(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        status_ = RefreshMapPalette();
        RefreshOverworldMap();
      }
      ImGui::EndGroup();
    }

    // Sprite Graphics (always present)
    TableNextColumn();
    ImGui::BeginGroup();
    if (gui::InputHexByte(ICON_MD_PETS " Spr Gfx",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_sprite_graphics(game_state_),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    ImGui::EndGroup();

    // Sprite Palette (always present)
    TableNextColumn();
    ImGui::BeginGroup();
    if (gui::InputHexByte(ICON_MD_COLORIZE " Spr Pal",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_sprite_palette(game_state_),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    ImGui::EndGroup();

    // Message ID (always present)
    TableNextColumn();
    ImGui::BeginGroup();
    if (gui::InputHexWord(ICON_MD_MESSAGE " Msg ID",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_message_id(),
                          kInputFieldSize + 20)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    ImGui::EndGroup();

    // Animated GFX (v3+ only)
    if (asm_version >= 3 && asm_version != 0xFF) {
      TableNextColumn();
      ImGui::BeginGroup();
      if (gui::InputHexByte(ICON_MD_ANIMATION " Anim GFX",
                            overworld_.mutable_overworld_map(current_map_)
                                ->mutable_animated_gfx(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
      ImGui::EndGroup();

      // Area Size (v3+ only)
      TableNextColumn();
      ImGui::BeginGroup();
      static const char* area_size_names[] = {"Small", "Large", "Wide", "Tall"};
      int current_area_size =
          static_cast<int>(overworld_.overworld_map(current_map_)->area_size());
      ImGui::SetNextItemWidth(80.f);
      if (ImGui::Combo(ICON_MD_ASPECT_RATIO " Size", &current_area_size,
                       area_size_names, 4)) {
        overworld_.mutable_overworld_map(current_map_)
            ->SetAreaSize(static_cast<zelda3::AreaSizeEnum>(current_area_size));
        RefreshOverworldMap();
      }
      ImGui::EndGroup();
    }

    // Additional controls row
    ImGui::TableNextRow();
    TableNextColumn();
    ImGui::SetNextItemWidth(100.f);
    if (ImGui::Combo("##GameState", &game_state_, kGamePartComboString.data(),
                     3)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    HOVER_HINT("Game progression state for sprite graphics/palettes");

    TableNextColumn();
    if (ImGui::Checkbox(
            ICON_MD_BLUR_ON " Mosaic",
            overworld_.mutable_overworld_map(current_map_)->mutable_mosaic())) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    HOVER_HINT("Enable Mosaic effect for the current map");

    ImGui::EndTable();
  }
}

void OverworldEditor::DrawCustomOverworldMapSettings() {
  if (BeginTable(kOWMapTable.data(), 9, kOWMapFlags, ImVec2(0, 0), -1)) {
    for (const auto& name : kV3MapSettingsColumnNames)
      ImGui::TableSetupColumn(name);

    TableNextColumn();
    ImGui::SetNextItemWidth(120.f);
    ImGui::Combo("##world", &current_world_, kWorldList.data(), 3);

    TableNextColumn();

    if (ImGui::Button("Tile Graphics", ImVec2(120, 0))) {
      ImGui::OpenPopup("TileGraphicsPopup");
    }
    if (ImGui::BeginPopup("TileGraphicsPopup")) {
      static const std::array<std::string, 8> kCustomMapSettingsColumnNames = {
          "TileGfx0", "TileGfx1", "TileGfx2", "TileGfx3",
          "TileGfx4", "TileGfx5", "TileGfx6", "TileGfx7"};
      for (int i = 0; i < 8; ++i) {
        ImGui::BeginGroup();
        if (gui::InputHexByte(kCustomMapSettingsColumnNames[i].data(),
                              overworld_.mutable_overworld_map(current_map_)
                                  ->mutable_custom_tileset(i),
                              kInputFieldSize)) {
          RefreshMapProperties();
          RefreshOverworldMap();
        }
        ImGui::EndGroup();
      }
      ImGui::EndPopup();
    }

    TableNextColumn();
    ImGui::BeginGroup();
    if (gui::InputHexByte("Palette",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_area_palette(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      status_ = RefreshMapPalette();
      RefreshOverworldMap();
    }
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexByte("Spr Gfx",
                      overworld_.mutable_overworld_map(current_map_)
                          ->mutable_sprite_graphics(game_state_),
                      kInputFieldSize);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexByte("Spr Palette",
                      overworld_.mutable_overworld_map(current_map_)
                          ->mutable_sprite_palette(game_state_),
                      kInputFieldSize);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexWord(
        "Msg Id",
        overworld_.mutable_overworld_map(current_map_)->mutable_message_id(),
        kInputFieldSize + 20);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::SetNextItemWidth(100.f);
    ImGui::Combo("##World", &game_state_, kGamePartComboString.data(), 3);

    TableNextColumn();
    ImGui::Checkbox(
        "##mosaic",
        overworld_.mutable_overworld_map(current_map_)->mutable_mosaic());
    HOVER_HINT("Enable Mosaic effect for the current map");

    TableNextColumn();
    // Add area size selection for v3 support
    static uint8_t asm_version =
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version != 0xFF) {
      if (BeginTable("AreaSizeTable", 2, kOWMapFlags, ImVec2(0, 0), -1)) {
        ImGui::TableSetupColumn("Area Size");
        ImGui::TableSetupColumn("Value");

        TableNextColumn();
        Text("Area Size");

        TableNextColumn();
        static const char* area_size_names[] = {"Small (1x1)", "Large (2x2)",
                                                "Wide (2x1)", "Tall (1x2)"};
        int current_area_size = static_cast<int>(
            overworld_.overworld_map(current_map_)->area_size());
        if (ImGui::Combo("##AreaSize", &current_area_size, area_size_names,
                         4)) {
          overworld_.mutable_overworld_map(current_map_)
              ->SetAreaSize(
                  static_cast<zelda3::AreaSizeEnum>(current_area_size));
          RefreshOverworldMap();
        }

        ImGui::EndTable();
      }
    }

    // Add additional v3 features
    if (asm_version >= 3 && asm_version != 0xFF) {
      Separator();
      Text("ZSCustomOverworld v3 Features:");

      // Main Palette
      if (gui::InputHexByte("Main Palette",
                            overworld_.mutable_overworld_map(current_map_)
                                ->mutable_main_palette(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        status_ = RefreshMapPalette();
        RefreshOverworldMap();
      }

      // Animated GFX
      if (gui::InputHexByte("Animated GFX",
                            overworld_.mutable_overworld_map(current_map_)
                                ->mutable_animated_gfx(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }

      // Subscreen Overlay
      if (gui::InputHexWord("Subscreen Overlay",
                            overworld_.mutable_overworld_map(current_map_)
                                ->mutable_subscreen_overlay(),
                            kInputFieldSize + 20)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
    }

    ImGui::EndTable();
  }
}

void OverworldEditor::DrawOverworldMaps() {
  int xx = 0;
  int yy = 0;
  for (int i = 0; i < 0x40; i++) {
    int world_index = i + (current_world_ * 0x40);
    int scale = static_cast<int>(ow_map_canvas_.global_scale());
    int map_x = (xx * kOverworldMapSize * scale);
    int map_y = (yy * kOverworldMapSize * scale);
    ow_map_canvas_.DrawBitmap(maps_bmp_[world_index], map_x, map_y,
                              ow_map_canvas_.global_scale());
    xx++;
    if (xx >= 8) {
      yy++;
      xx = 0;
    }
  }
}

void OverworldEditor::DrawOverworldEdits() {
  // Determine which overworld map the user is currently editing.
  auto mouse_position = ow_map_canvas_.drawn_tile_position();
  int map_x = mouse_position.x / kOverworldMapSize;
  int map_y = mouse_position.y / kOverworldMapSize;
  current_map_ = map_x + map_y * 8;
  if (current_world_ == 1) {
    current_map_ += 0x40;
  } else if (current_world_ == 2) {
    current_map_ += 0x80;
  }

  // Render the updated map bitmap.
  RenderUpdatedMapBitmap(
      mouse_position, gfx::GetTilemapData(tile16_blockset_, current_tile16_));

  // Calculate the correct superX and superY values
  int superY = current_map_ / 8;
  int superX = current_map_ % 8;
  int mouse_x = mouse_position.x;
  int mouse_y = mouse_position.y;
  // Calculate the correct tile16_x and tile16_y positions
  int tile16_x = (mouse_x % kOverworldMapSize) / (kOverworldMapSize / 32);
  int tile16_y = (mouse_y % kOverworldMapSize) / (kOverworldMapSize / 32);

  // Update the overworld_.map_tiles() based on tile16 ID and current world
  auto& selected_world =
      (current_world_ == 0)   ? overworld_.mutable_map_tiles()->light_world
      : (current_world_ == 1) ? overworld_.mutable_map_tiles()->dark_world
                              : overworld_.mutable_map_tiles()->special_world;

  int index_x = superX * 32 + tile16_x;
  int index_y = superY * 32 + tile16_y;

  selected_world[index_x][index_y] = current_tile16_;
}

void OverworldEditor::RenderUpdatedMapBitmap(
    const ImVec2& click_position, const std::vector<uint8_t>& tile_data) {
  // Calculate the tile index for x and y based on the click_position
  int tile_index_x =
      (static_cast<int>(click_position.x) % kOverworldMapSize) / kTile16Size;
  int tile_index_y =
      (static_cast<int>(click_position.y) % kOverworldMapSize) / kTile16Size;

  // Calculate the pixel start position based on tile index and tile size
  ImVec2 start_position;
  start_position.x = static_cast<float>(tile_index_x * kTile16Size);
  start_position.y = static_cast<float>(tile_index_y * kTile16Size);

  // Update the bitmap's pixel data based on the start_position and tile_data
  gfx::Bitmap& current_bitmap = maps_bmp_[current_map_];
  for (int y = 0; y < kTile16Size; ++y) {
    for (int x = 0; x < kTile16Size; ++x) {
      int pixel_index =
          (start_position.y + y) * kOverworldMapSize + (start_position.x + x);
      current_bitmap.WriteToPixel(pixel_index, tile_data[y * kTile16Size + x]);
    }
  }

  current_bitmap.set_modified(true);
}

void OverworldEditor::CheckForOverworldEdits() {
  CheckForSelectRectangle();

  // User has selected a tile they want to draw from the blockset
  // and clicked on the canvas.
  if (!blockset_canvas_.points().empty() &&
      !ow_map_canvas_.select_rect_active() &&
      ow_map_canvas_.DrawTilemapPainter(tile16_blockset_, current_tile16_)) {
    DrawOverworldEdits();
  }

  if (ow_map_canvas_.select_rect_active()) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      auto& selected_world =
          (current_world_ == 0) ? overworld_.mutable_map_tiles()->light_world
          : (current_world_ == 1)
              ? overworld_.mutable_map_tiles()->dark_world
              : overworld_.mutable_map_tiles()->special_world;
      // new_start_pos and new_end_pos
      auto start = ow_map_canvas_.selected_points()[0];
      auto end = ow_map_canvas_.selected_points()[1];

      // Calculate the bounds of the rectangle in terms of 16x16 tile indices
      int start_x = std::floor(start.x / kTile16Size) * kTile16Size;
      int start_y = std::floor(start.y / kTile16Size) * kTile16Size;
      int end_x = std::floor(end.x / kTile16Size) * kTile16Size;
      int end_y = std::floor(end.y / kTile16Size) * kTile16Size;

      if (start_x > end_x)
        std::swap(start_x, end_x);
      if (start_y > end_y)
        std::swap(start_y, end_y);

      constexpr int local_map_size = 512;  // Size of each local map
      // Number of tiles per local map (since each tile is 16x16)
      constexpr int tiles_per_local_map = local_map_size / kTile16Size;

      for (int y = start_y, i = 0; y <= end_y; y += kTile16Size) {
        for (int x = start_x; x <= end_x; x += kTile16Size, ++i) {
          // Determine which local map (512x512) the tile is in
          int local_map_x = x / local_map_size;
          int local_map_y = y / local_map_size;

          // Calculate the tile's position within its local map
          int tile16_x = (x % local_map_size) / kTile16Size;
          int tile16_y = (y % local_map_size) / kTile16Size;

          // Calculate the index within the overall map structure
          int index_x = local_map_x * tiles_per_local_map + tile16_x;
          int index_y = local_map_y * tiles_per_local_map + tile16_y;
          overworld_.set_current_world(current_world_);
          overworld_.set_current_map(current_map_);
          int tile16_id = overworld_.GetTileFromPosition(
              ow_map_canvas_.selected_tiles()[i]);
          selected_world[index_x][index_y] = tile16_id;
        }
      }

      RefreshOverworldMap();
    }
  }
}

void OverworldEditor::CheckForSelectRectangle() {
  ow_map_canvas_.DrawSelectRect(current_map_);

  // Single tile case
  if (ow_map_canvas_.selected_tile_pos().x != -1) {
    current_tile16_ =
        overworld_.GetTileFromPosition(ow_map_canvas_.selected_tile_pos());
    ow_map_canvas_.set_selected_tile_pos(ImVec2(-1, -1));
  }

  static std::vector<int> tile16_ids;
  if (ow_map_canvas_.select_rect_active()) {
    // Get the tile16 IDs from the selected tile ID positions
    if (tile16_ids.size() != 0) {
      tile16_ids.clear();
    }

    if (ow_map_canvas_.selected_tiles().size() > 0) {
      // Set the current world and map in overworld for proper tile lookup
      overworld_.set_current_world(current_world_);
      overworld_.set_current_map(current_map_);
      for (auto& each : ow_map_canvas_.selected_tiles()) {
        tile16_ids.push_back(overworld_.GetTileFromPosition(each));
      }
    }
  }
  // Create a composite image of all the tile16s selected
  ow_map_canvas_.DrawBitmapGroup(tile16_ids, tile16_blockset_, 0x10,
                                 ow_map_canvas_.global_scale());
}

absl::Status OverworldEditor::Copy() {
  if (!context_)
    return absl::FailedPreconditionError("No editor context");
  // If a rectangle selection exists, copy its tile16 IDs into shared clipboard
  if (ow_map_canvas_.select_rect_active() &&
      !ow_map_canvas_.selected_tiles().empty()) {
    std::vector<int> ids;
    ids.reserve(ow_map_canvas_.selected_tiles().size());
    overworld_.set_current_world(current_world_);
    overworld_.set_current_map(current_map_);
    for (const auto& pos : ow_map_canvas_.selected_tiles()) {
      ids.push_back(overworld_.GetTileFromPosition(pos));
    }
    // Determine width/height in tile16 based on selection bounds
    const auto start = ow_map_canvas_.selected_points()[0];
    const auto end = ow_map_canvas_.selected_points()[1];
    const int start_x =
        static_cast<int>(std::floor(std::min(start.x, end.x) / 16.0f));
    const int end_x =
        static_cast<int>(std::floor(std::max(start.x, end.x) / 16.0f));
    const int start_y =
        static_cast<int>(std::floor(std::min(start.y, end.y) / 16.0f));
    const int end_y =
        static_cast<int>(std::floor(std::max(start.y, end.y) / 16.0f));
    const int width = end_x - start_x + 1;
    const int height = end_y - start_y + 1;

    context_->shared_clipboard.overworld_tile16_ids = std::move(ids);
    context_->shared_clipboard.overworld_width = width;
    context_->shared_clipboard.overworld_height = height;
    context_->shared_clipboard.has_overworld_tile16 = true;
    return absl::OkStatus();
  }
  // Single tile copy fallback
  if (current_tile16_ >= 0) {
    context_->shared_clipboard.overworld_tile16_ids = {current_tile16_};
    context_->shared_clipboard.overworld_width = 1;
    context_->shared_clipboard.overworld_height = 1;
    context_->shared_clipboard.has_overworld_tile16 = true;
    return absl::OkStatus();
  }
  return absl::FailedPreconditionError("Nothing selected to copy");
}

absl::Status OverworldEditor::Paste() {
  if (!context_)
    return absl::FailedPreconditionError("No editor context");
  if (!context_->shared_clipboard.has_overworld_tile16) {
    return absl::FailedPreconditionError("Clipboard empty");
  }
  if (ow_map_canvas_.points().empty() &&
      ow_map_canvas_.selected_tile_pos().x == -1) {
    return absl::FailedPreconditionError("No paste target");
  }

  // Determine paste anchor position (use current mouse drawn tile position)
  const ImVec2 anchor = ow_map_canvas_.drawn_tile_position();

  // Compute anchor in tile16 grid within the current map
  const int tile16_x =
      (static_cast<int>(anchor.x) % kOverworldMapSize) / kTile16Size;
  const int tile16_y =
      (static_cast<int>(anchor.y) % kOverworldMapSize) / kTile16Size;

  auto& selected_world =
      (current_world_ == 0)   ? overworld_.mutable_map_tiles()->light_world
      : (current_world_ == 1) ? overworld_.mutable_map_tiles()->dark_world
                              : overworld_.mutable_map_tiles()->special_world;

  const int superY = current_map_ / 8;
  const int superX = current_map_ % 8;
  const int tiles_per_local_map = 512 / kTile16Size;

  const int width = context_->shared_clipboard.overworld_width;
  const int height = context_->shared_clipboard.overworld_height;
  const auto& ids = context_->shared_clipboard.overworld_tile16_ids;

  // Guard
  if (width * height != static_cast<int>(ids.size())) {
    return absl::InternalError("Clipboard dimensions mismatch");
  }

  for (int dy = 0; dy < height; ++dy) {
    for (int dx = 0; dx < width; ++dx) {
      const int id = ids[dy * width + dx];
      const int gx = tile16_x + dx;
      const int gy = tile16_y + dy;

      const int global_x = superX * 32 + gx;
      const int global_y = superY * 32 + gy;
      if (global_x < 0 || global_x >= 256 || global_y < 0 || global_y >= 256)
        continue;
      selected_world[global_x][global_y] = id;
    }
  }

  RefreshOverworldMap();
  return absl::OkStatus();
}

absl::Status OverworldEditor::CheckForCurrentMap() {
  // 4096x4096, 512x512 maps and some are larges maps 1024x1024
  const auto mouse_position = ImGui::GetIO().MousePos;
  const int large_map_size = 1024;
  const auto canvas_zero_point = ow_map_canvas_.zero_point();

  // Calculate which small map the mouse is currently over
  int map_x = (mouse_position.x - canvas_zero_point.x) / kOverworldMapSize;
  int map_y = (mouse_position.y - canvas_zero_point.y) / kOverworldMapSize;

  // Calculate the index of the map in the `maps_bmp_` vector
  int hovered_map = map_x + map_y * 8;
  if (current_world_ == 1) {
    hovered_map += 0x40;
  } else if (current_world_ == 2) {
    hovered_map += 0x80;
  }

  // Only update current_map_ if not locked
  if (!current_map_lock_) {
    current_map_ = hovered_map;
    current_parent_ = overworld_.overworld_map(current_map_)->parent();
  }

  const int current_highlighted_map = current_map_;

  if (overworld_.overworld_map(current_map_)->is_large_map() ||
      overworld_.overworld_map(current_map_)->large_index() != 0) {
    const int highlight_parent =
        overworld_.overworld_map(current_highlighted_map)->parent();
    const int parent_map_x = highlight_parent % 8;
    const int parent_map_y = highlight_parent / 8;
    ow_map_canvas_.DrawOutline(parent_map_x * kOverworldMapSize,
                               parent_map_y * kOverworldMapSize, large_map_size,
                               large_map_size);
  } else {
    // Calculate map coordinates accounting for world offset
    int current_map_x, current_map_y;
    if (current_world_ == 0) {
      // Light World (0x00-0x3F)
      current_map_x = current_highlighted_map % 8;
      current_map_y = current_highlighted_map / 8;
    } else if (current_world_ == 1) {
      // Dark World (0x40-0x7F)
      current_map_x = (current_highlighted_map - 0x40) % 8;
      current_map_y = (current_highlighted_map - 0x40) / 8;
    } else {
      // Special World (0x80-0x9F) - use display coordinates based on current_world_
      // The special world maps are displayed in the same 8x8 grid as LW/DW
      current_map_x = (current_highlighted_map - 0x80) % 8;
      current_map_y = (current_highlighted_map - 0x80) / 8;
    }
    ow_map_canvas_.DrawOutline(current_map_x * kOverworldMapSize,
                               current_map_y * kOverworldMapSize,
                               kOverworldMapSize, kOverworldMapSize);
  }

  if (maps_bmp_[current_map_].modified() ||
      ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    RefreshOverworldMap();
    RETURN_IF_ERROR(RefreshTile16Blockset());

    // Ensure tile16 blockset is fully updated before rendering
    if (tile16_blockset_.atlas.is_active()) {
      Renderer::Get().UpdateBitmap(&tile16_blockset_.atlas);

      // Clear any cached tile bitmaps to force re-rendering
      tile16_blockset_.tile_bitmaps.clear();
    }

    Renderer::Get().UpdateBitmap(&maps_bmp_[current_map_]);
    maps_bmp_[current_map_].set_modified(false);
  }

  // If double clicked, toggle the current map
  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) {
    current_map_lock_ = !current_map_lock_;
  }

  return absl::OkStatus();
}

void OverworldEditor::CheckForMousePan() {
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    previous_mode = current_mode;
    current_mode = EditingMode::PAN;
    ow_map_canvas_.set_draggable(true);
    middle_mouse_dragging_ = true;
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle) &&
      current_mode == EditingMode::PAN && middle_mouse_dragging_) {
    current_mode = previous_mode;
    ow_map_canvas_.set_draggable(false);
    middle_mouse_dragging_ = false;
  }
}

void OverworldEditor::DrawOverworldCanvas() {
  if (all_gfx_loaded_) {
    // Use ASM version with flag as override to determine UI
    uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    bool use_custom_overworld =
        (asm_version != 0xFF) ||
        core::FeatureFlags::get().overworld.kLoadCustomOverworld;

    if (use_custom_overworld) {
      map_properties_system_->DrawSimplifiedMapSettings(
          current_world_, current_map_, current_map_lock_,
          show_map_properties_panel_, show_custom_bg_color_editor_,
          show_overlay_editor_, show_overlay_preview_, game_state_,
          reinterpret_cast<int&>(current_mode));
    } else {
      DrawOverworldMapSettings();
    }
    Separator();
  }

  gui::BeginNoPadding();
  gui::BeginChildBothScrollbars(7);
  ow_map_canvas_.DrawBackground();
  gui::EndNoPadding();

  CheckForMousePan();
  if (current_mode == EditingMode::PAN) {
    ow_map_canvas_.DrawContextMenu();
  } else {
    ow_map_canvas_.set_draggable(false);
    // Handle map interaction with middle-click instead of right-click
    HandleMapInteraction();
  }

  if (overworld_.is_loaded()) {
    DrawOverworldMaps();
    DrawOverworldExits(ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());
    DrawOverworldEntrances(ow_map_canvas_.zero_point(),
                           ow_map_canvas_.scrolling());
    DrawOverworldItems();
    DrawOverworldSprites();

    // Draw overlay preview if enabled
    if (show_overlay_preview_) {
      map_properties_system_->DrawOverlayPreviewOnMap(
          current_map_, current_world_, show_overlay_preview_);
    }

    if (current_mode == EditingMode::DRAW_TILE) {
      CheckForOverworldEdits();
    }
    if (IsItemHovered())
      status_ = CheckForCurrentMap();
  }

  ow_map_canvas_.DrawGrid();
  ow_map_canvas_.DrawOverlay();
  EndChild();

  // Handle mouse wheel activity
  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    ImGui::SetScrollX(ImGui::GetScrollX() + ImGui::GetIO().MouseWheelH * 16.0f);
    ImGui::SetScrollY(ImGui::GetScrollY() + ImGui::GetIO().MouseWheel * 16.0f);
  }
}

absl::Status OverworldEditor::DrawTile16Selector() {
  gui::BeginPadding(3);
  ImGui::BeginGroup();
  gui::BeginChildWithScrollbar("##Tile16SelectorScrollRegion");
  blockset_canvas_.DrawBackground();
  gui::EndPadding();  // Fixed: was EndNoPadding()

  blockset_canvas_.DrawContextMenu();
  blockset_canvas_.DrawBitmap(tile16_blockset_.atlas, /*border_offset=*/2,
                              map_blockset_loaded_, /*scale=*/2);

  // Improved tile interaction detection - use proper canvas interaction
  bool tile_selected = false;
  if (blockset_canvas_.DrawTileSelector(32.0f)) {
    tile_selected = true;
  } else if (ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
             blockset_canvas_.IsMouseHovering()) {
    // Secondary detection for direct clicks
    tile_selected = true;
  }

  if (tile_selected && blockset_canvas_.HasValidSelection()) {
    auto tile_pos = blockset_canvas_.GetLastClickPosition();
    int grid_x = static_cast<int>(tile_pos.x / 32);
    int grid_y = static_cast<int>(tile_pos.y / 32);
    int id = grid_x + grid_y * 8;

    if (id != current_tile16_ && id >= 0 && id < 512) {
      current_tile16_ = id;
      RETURN_IF_ERROR(tile16_editor_.SetCurrentTile(id));
      show_tile16_editor_ = true;
      util::logf("Selected Tile16: %d (grid: %d,%d)", id, grid_x, grid_y);
    }
  }

  blockset_canvas_.DrawGrid();
  blockset_canvas_.DrawOverlay();

  EndChild();
  ImGui::EndGroup();
  return absl::OkStatus();
}

void OverworldEditor::DrawTile8Selector() {
  graphics_bin_canvas_.DrawBackground();
  graphics_bin_canvas_.DrawContextMenu();
  if (all_gfx_loaded_) {
    int key = 0;
    for (auto& value : gfx::Arena::Get().gfx_sheets()) {
      int offset = 0x40 * (key + 1);
      int top_left_y = graphics_bin_canvas_.zero_point().y + 2;
      if (key >= 1) {
        top_left_y = graphics_bin_canvas_.zero_point().y + 0x40 * key;
      }
      auto texture = value.texture();
      graphics_bin_canvas_.draw_list()->AddImage(
          (ImTextureID)(intptr_t)texture,
          ImVec2(graphics_bin_canvas_.zero_point().x + 2, top_left_y),
          ImVec2(graphics_bin_canvas_.zero_point().x + 0x100,
                 graphics_bin_canvas_.zero_point().y + offset));
      key++;
    }
  }
  graphics_bin_canvas_.DrawGrid();
  graphics_bin_canvas_.DrawOverlay();
}

absl::Status OverworldEditor::DrawAreaGraphics() {
  if (overworld_.is_loaded()) {
    // Always ensure current map graphics are loaded
    if (!current_graphics_set_.contains(current_map_)) {
      overworld_.set_current_map(current_map_);
      palette_ = overworld_.current_area_palette();
      gfx::Bitmap bmp;
      Renderer::Get().CreateAndRenderBitmap(0x80, kOverworldMapSize, 0x08,
                                            overworld_.current_graphics(), bmp,
                                            palette_);
      current_graphics_set_[current_map_] = bmp;
    }
  }

  gui::BeginPadding(3);
  ImGui::BeginGroup();
  gui::BeginChildWithScrollbar("##AreaGraphicsScrollRegion");
  current_gfx_canvas_.DrawBackground();
  gui::EndPadding();
  {
    current_gfx_canvas_.DrawContextMenu();
    if (current_graphics_set_.contains(current_map_) &&
        current_graphics_set_[current_map_].is_active()) {
      current_gfx_canvas_.DrawBitmap(current_graphics_set_[current_map_], 2, 2,
                                     2.0f);
    }
    current_gfx_canvas_.DrawTileSelector(32.0f);
    current_gfx_canvas_.DrawGrid();
    current_gfx_canvas_.DrawOverlay();
  }
  EndChild();
  ImGui::EndGroup();
  return absl::OkStatus();
}

absl::Status OverworldEditor::DrawTileSelector() {
  if (BeginTabBar(kTileSelectorTab.data(),
                  ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (BeginTabItem("Tile16")) {
      status_ = DrawTile16Selector();
      EndTabItem();
    }
    if (BeginTabItem("Tile8")) {
      gui::BeginPadding(3);
      gui::BeginChildWithScrollbar("##Tile8SelectorScrollRegion");
      DrawTile8Selector();
      EndChild();
      gui::EndNoPadding();
      EndTabItem();
    }
    if (BeginTabItem("Area Graphics")) {
      status_ = DrawAreaGraphics();
      EndTabItem();
    }
    if (BeginTabItem("Scratch Space")) {
      status_ = DrawScratchSpace();
      EndTabItem();
    }
    EndTabBar();
  }
  return absl::OkStatus();
}

void OverworldEditor::DrawOverworldEntrances(ImVec2 canvas_p0, ImVec2 scrolling,
                                             bool holes) {
  int i = 0;
  for (auto& each : overworld_.entrances()) {
    if (each.map_id_ < 0x40 + (current_world_ * 0x40) &&
        each.map_id_ >= (current_world_ * 0x40) && !each.deleted) {
      auto color = ImVec4(255, 0, 255, 100);
      if (each.is_hole_) {
        color = ImVec4(255, 255, 0, 200);
      }
      ow_map_canvas_.DrawRect(each.x_, each.y_, 16, 16, color);
      std::string str = util::HexByte(each.entrance_id_);

      if (current_mode == EditingMode::ENTRANCES) {
        HandleEntityDragging(&each, canvas_p0, scrolling, is_dragging_entity_,
                             dragged_entity_, current_entity_);

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          jump_to_tab_ = each.entrance_id_;
        }

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_entrance_id_ = i;
          current_entrance_ = each;
        }
      }

      ow_map_canvas_.DrawText(str, each.x_, each.y_);
    }
    i++;
  }

  if (DrawEntranceInserterPopup()) {
    // Get the deleted entrance ID and insert it at the mouse position
    auto deleted_entrance_id = overworld_.deleted_entrances().back();
    overworld_.deleted_entrances().pop_back();
    auto& entrance = overworld_.entrances()[deleted_entrance_id];
    entrance.map_id_ = current_map_;
    entrance.entrance_id_ = deleted_entrance_id;
    entrance.x_ = ow_map_canvas_.hover_mouse_pos().x;
    entrance.y_ = ow_map_canvas_.hover_mouse_pos().y;
    entrance.deleted = false;
  }

  if (current_mode == EditingMode::ENTRANCES) {
    const auto is_hovering =
        IsMouseHoveringOverEntity(current_entrance_, canvas_p0, scrolling);

    if (!is_hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Entrance Inserter");
    } else {
      if (DrawOverworldEntrancePopup(
              overworld_.entrances()[current_entrance_id_])) {
        overworld_.entrances()[current_entrance_id_] = current_entrance_;
      }

      if (overworld_.entrances()[current_entrance_id_].deleted) {
        overworld_.mutable_deleted_entrances()->emplace_back(
            current_entrance_id_);
      }
    }
  }
}

void OverworldEditor::DrawOverworldExits(ImVec2 canvas_p0, ImVec2 scrolling) {
  int i = 0;
  for (auto& each : *overworld_.mutable_exits()) {
    if (each.map_id_ < 0x40 + (current_world_ * 0x40) &&
        each.map_id_ >= (current_world_ * 0x40) && !each.deleted_) {
      ow_map_canvas_.DrawRect(each.x_, each.y_, 16, 16,
                              ImVec4(255, 255, 255, 150));
      if (current_mode == EditingMode::EXITS) {
        each.entity_id_ = i;
        HandleEntityDragging(&each, ow_map_canvas_.zero_point(),
                             ow_map_canvas_.scrolling(), is_dragging_entity_,
                             dragged_entity_, current_entity_, true);

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          jump_to_tab_ = each.room_id_;
        }

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_exit_id_ = i;
          current_exit_ = each;
          current_entity_ = &each;
          current_entity_->entity_id_ = i;
          ImGui::OpenPopup("Exit editor");
        }
      }

      std::string str = util::HexByte(i);
      ow_map_canvas_.DrawText(str, each.x_, each.y_);
    }
    i++;
  }

  DrawExitInserterPopup();
  if (current_mode == EditingMode::EXITS) {
    const auto hovering = IsMouseHoveringOverEntity(
        overworld_.mutable_exits()->at(current_exit_id_),
        ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Exit Inserter");
    } else {
      if (DrawExitEditorPopup(
              overworld_.mutable_exits()->at(current_exit_id_))) {
        overworld_.mutable_exits()->at(current_exit_id_) = current_exit_;
      }
    }
  }
}

void OverworldEditor::DrawOverworldItems() {
  int i = 0;
  for (auto& item : *overworld_.mutable_all_items()) {
    // Get the item's bitmap and real X and Y positions
    if (item.room_map_id_ < 0x40 + (current_world_ * 0x40) &&
        item.room_map_id_ >= (current_world_ * 0x40) && !item.deleted) {
      ow_map_canvas_.DrawRect(item.x_, item.y_, 16, 16, ImVec4(255, 0, 0, 150));

      if (current_mode == EditingMode::ITEMS) {
        // Check if this item is being clicked and dragged
        HandleEntityDragging(&item, ow_map_canvas_.zero_point(),
                             ow_map_canvas_.scrolling(), is_dragging_entity_,
                             dragged_entity_, current_entity_);

        const auto hovering = IsMouseHoveringOverEntity(
            item, ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());
        if (hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_item_id_ = i;
          current_item_ = item;
          current_entity_ = &item;
        }
      }
      std::string item_name = "";
      if (item.id_ < zelda3::kSecretItemNames.size()) {
        item_name = zelda3::kSecretItemNames[item.id_];
      } else {
        item_name = absl::StrFormat("0x%02X", item.id_);
      }
      ow_map_canvas_.DrawText(item_name, item.x_, item.y_);
    }
    i++;
  }

  DrawItemInsertPopup();
  if (current_mode == EditingMode::ITEMS) {
    const auto hovering = IsMouseHoveringOverEntity(
        overworld_.mutable_all_items()->at(current_item_id_),
        ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Item Inserter");
    } else {
      if (DrawItemEditorPopup(
              overworld_.mutable_all_items()->at(current_item_id_))) {
        overworld_.mutable_all_items()->at(current_item_id_) = current_item_;
      }
    }
  }
}

void OverworldEditor::DrawOverworldSprites() {
  int i = 0;
  for (auto& sprite : *overworld_.mutable_sprites(game_state_)) {
    // Filter sprites by current world - only show sprites for the current world
    if (!sprite.deleted() && sprite.map_id() < 0x40 + (current_world_ * 0x40) &&
        sprite.map_id() >= (current_world_ * 0x40)) {
      // Sprites are already stored with global coordinates (realX, realY from
      // ROM loading) So we can use sprite.x_ and sprite.y_ directly
      int sprite_x = sprite.x_;
      int sprite_y = sprite.y_;

      // Temporarily update sprite coordinates for entity interaction
      int original_x = sprite.x_;
      int original_y = sprite.y_;

      ow_map_canvas_.DrawRect(sprite_x, sprite_y, kTile16Size, kTile16Size,
                              /*magenta=*/ImVec4(255, 0, 255, 150));
      if (current_mode == EditingMode::SPRITES) {
        HandleEntityDragging(&sprite, ow_map_canvas_.zero_point(),
                             ow_map_canvas_.scrolling(), is_dragging_entity_,
                             dragged_entity_, current_entity_);
        if (IsMouseHoveringOverEntity(sprite, ow_map_canvas_.zero_point(),
                                      ow_map_canvas_.scrolling()) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_sprite_id_ = i;
          current_sprite_ = sprite;
        }
      }
      if (core::FeatureFlags::get().overworld.kDrawOverworldSprites) {
        if (sprite_previews_[sprite.id()].is_active()) {
          ow_map_canvas_.DrawBitmap(sprite_previews_[sprite.id()], sprite_x,
                                    sprite_y, 2.0f);
        }
      }

      ow_map_canvas_.DrawText(absl::StrFormat("%s", sprite.name()), sprite_x,
                              sprite_y);

      // Restore original coordinates
      sprite.x_ = original_x;
      sprite.y_ = original_y;
    }
    i++;
  }

  DrawSpriteInserterPopup();
  if (current_mode == EditingMode::SPRITES) {
    const auto hovering = IsMouseHoveringOverEntity(
        overworld_.mutable_sprites(game_state_)->at(current_sprite_id_),
        ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Sprite Inserter");
    } else {
      if (DrawSpriteEditorPopup(overworld_.mutable_sprites(game_state_)
                                    ->at(current_sprite_id_))) {
        overworld_.mutable_sprites(game_state_)->at(current_sprite_id_) =
            current_sprite_;
      }
    }
  }
}

absl::Status OverworldEditor::Save() {
  if (core::FeatureFlags::get().overworld.kSaveOverworldMaps) {
    RETURN_IF_ERROR(overworld_.CreateTile32Tilemap());
    RETURN_IF_ERROR(overworld_.SaveMap32Tiles());
    RETURN_IF_ERROR(overworld_.SaveMap16Tiles());
    RETURN_IF_ERROR(overworld_.SaveOverworldMaps());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldEntrances) {
    RETURN_IF_ERROR(overworld_.SaveEntrances());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldExits) {
    RETURN_IF_ERROR(overworld_.SaveExits());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldItems) {
    RETURN_IF_ERROR(overworld_.SaveItems());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldProperties) {
    RETURN_IF_ERROR(overworld_.SaveMapProperties());
    RETURN_IF_ERROR(overworld_.SaveMusic());
  }
  return absl::OkStatus();
}

absl::Status OverworldEditor::LoadGraphics() {
  util::logf("Loading overworld.");
  // Load the Link to the Past overworld.
  RETURN_IF_ERROR(overworld_.Load(rom_));
  palette_ = overworld_.current_area_palette();

  util::logf("Loading overworld graphics.");
  // Create the area graphics image
  Renderer::Get().CreateAndRenderBitmap(0x80, kOverworldMapSize, 0x40,
                                        overworld_.current_graphics(),
                                        current_gfx_bmp_, palette_);

  util::logf("Loading overworld tileset.");
  // Create the tile16 blockset image
  Renderer::Get().CreateAndRenderBitmap(0x80, 0x2000, 0x08,
                                        overworld_.tile16_blockset_data(),
                                        tile16_blockset_bmp_, palette_);
  map_blockset_loaded_ = true;

  // Copy the tile16 data into individual tiles.
  auto tile16_blockset_data = overworld_.tile16_blockset_data();
  util::logf("Loading overworld tile16 graphics.");

  tile16_blockset_ =
      gfx::CreateTilemap(tile16_blockset_data, 0x80, 0x2000, kTile16Size,
                         zelda3::kNumTile16Individual, palette_);

  util::logf("Loading overworld maps.");
  // Render the overworld maps loaded from the ROM.
  for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
    overworld_.set_current_map(i);
    auto palette = overworld_.current_area_palette();
    try {
      Renderer::Get().CreateAndRenderBitmap(
          kOverworldMapSize, kOverworldMapSize, 0x80,
          overworld_.current_map_bitmap_data(), maps_bmp_[i], palette);
    } catch (const std::bad_alloc& e) {
      std::cout << "Error: " << e.what() << std::endl;
      continue;
    }
  }

  if (core::FeatureFlags::get().overworld.kDrawOverworldSprites) {
    RETURN_IF_ERROR(LoadSpriteGraphics());
  }

  return absl::OkStatus();
}

absl::Status OverworldEditor::LoadSpriteGraphics() {
  // Render the sprites for each Overworld map
  const int depth = 0x10;
  for (int i = 0; i < 3; i++)
    for (auto const& sprite : *overworld_.mutable_sprites(i)) {
      int width = sprite.width();
      int height = sprite.height();
      if (width == 0 || height == 0) {
        continue;
      }
      if (sprite_previews_.size() < sprite.id()) {
        sprite_previews_.resize(sprite.id() + 1);
      }
      sprite_previews_[sprite.id()].Create(width, height, depth,
                                           *sprite.preview_graphics());
      sprite_previews_[sprite.id()].SetPalette(palette_);
      Renderer::Get().RenderBitmap(&(sprite_previews_[sprite.id()]));
    }
  return absl::OkStatus();
}

void OverworldEditor::RefreshChildMap(int map_index) {
  overworld_.mutable_overworld_map(map_index)->LoadAreaGraphics();
  status_ = overworld_.mutable_overworld_map(map_index)->BuildTileset();
  PRINT_IF_ERROR(status_);
  status_ = overworld_.mutable_overworld_map(map_index)->BuildTiles16Gfx(
      *overworld_.mutable_tiles16(), overworld_.tiles16().size());
  PRINT_IF_ERROR(status_);
  status_ = overworld_.mutable_overworld_map(map_index)->BuildBitmap(
      overworld_.GetMapTiles(current_world_));
  maps_bmp_[map_index].set_data(
      overworld_.mutable_overworld_map(map_index)->bitmap_data());
  maps_bmp_[map_index].set_modified(true);
  PRINT_IF_ERROR(status_);
}

void OverworldEditor::RefreshOverworldMap() {
  std::vector<std::future<void>> futures;
  std::array<int, 4> indices = {0, 0, 0, 0};

  auto refresh_map_async = [this](int map_index) {
    RefreshChildMap(map_index);
  };

  int source_map_id = current_map_;
  bool is_large = overworld_.overworld_map(current_map_)->is_large_map();
  if (is_large) {
    source_map_id = current_parent_;
    // We need to update the map and its siblings if it's a large map
    for (int i = 1; i < 4; i++) {
      int sibling_index = overworld_.overworld_map(source_map_id)->parent() + i;
      if (i >= 2)
        sibling_index += 6;
      futures.push_back(
          std::async(std::launch::async, refresh_map_async, sibling_index));
      indices[i] = sibling_index;
    }
  }
  indices[0] = source_map_id;
  futures.push_back(
      std::async(std::launch::async, refresh_map_async, source_map_id));

  for (auto& each : futures) {
    each.wait();
    each.get();
  }
  int n = is_large ? 4 : 1;
  // We do texture updating on the main thread
  for (int i = 0; i < n; ++i) {
    Renderer::Get().UpdateBitmap(&maps_bmp_[indices[i]]);
  }
}

absl::Status OverworldEditor::RefreshMapPalette() {
  RETURN_IF_ERROR(
      overworld_.mutable_overworld_map(current_map_)->LoadPalette());
  const auto current_map_palette = overworld_.current_area_palette();

  if (overworld_.overworld_map(current_map_)->is_large_map()) {
    // We need to update the map and its siblings if it's a large map
    for (int i = 1; i < 4; i++) {
      int sibling_index = overworld_.overworld_map(current_map_)->parent() + i;
      if (i >= 2)
        sibling_index += 6;
      RETURN_IF_ERROR(
          overworld_.mutable_overworld_map(sibling_index)->LoadPalette());
      maps_bmp_[sibling_index].SetPalette(current_map_palette);
    }
  }

  maps_bmp_[current_map_].SetPalette(current_map_palette);
  return absl::OkStatus();
}

void OverworldEditor::RefreshMapProperties() {
  const auto& current_ow_map = *overworld_.mutable_overworld_map(current_map_);
  if (current_ow_map.is_large_map()) {
    // We need to copy the properties from the parent map to the children
    for (int i = 1; i < 4; i++) {
      int sibling_index = current_ow_map.parent() + i;
      if (i >= 2) {
        sibling_index += 6;
      }
      auto& map = *overworld_.mutable_overworld_map(sibling_index);
      map.set_area_graphics(current_ow_map.area_graphics());
      map.set_area_palette(current_ow_map.area_palette());
      map.set_sprite_graphics(game_state_,
                              current_ow_map.sprite_graphics(game_state_));
      map.set_sprite_palette(game_state_,
                             current_ow_map.sprite_palette(game_state_));
      map.set_message_id(current_ow_map.message_id());
    }
  }
}

absl::Status OverworldEditor::RefreshTile16Blockset() {
  if (current_blockset_ ==
      overworld_.overworld_map(current_map_)->area_graphics()) {
    return absl::OkStatus();
  }
  current_blockset_ = overworld_.overworld_map(current_map_)->area_graphics();

  overworld_.set_current_map(current_map_);
  palette_ = overworld_.current_area_palette();

  const auto tile16_data = overworld_.tile16_blockset_data();

  gfx::UpdateTilemap(tile16_blockset_, tile16_data);
  tile16_blockset_.atlas.SetPalette(palette_);
  return absl::OkStatus();
}

void OverworldEditor::DrawCustomBackgroundColorEditor() {
  static uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];

  if (asm_version < 2 || asm_version == 0xFF) {
    Text(
        "Custom background colors are only available in ZSCustomOverworld v2+");
    return;
  }

  // Check if area-specific background colors are enabled
  bool bg_enabled =
      (*rom_)[zelda3::OverworldCustomAreaSpecificBGEnabled] != 0x00;
  if (Checkbox("Enable Area-Specific Background Colors", &bg_enabled)) {
    (*rom_)[zelda3::OverworldCustomAreaSpecificBGEnabled] =
        bg_enabled ? 0x01 : 0x00;
  }

  if (!bg_enabled) {
    Text("Area-specific background colors are disabled.");
    return;
  }

  Separator();

  // Display current map's background color
  Text("Current Map: %d (0x%02X)", current_map_, current_map_);

  // Get current background color
  uint16_t current_bg_color =
      (*rom_)[zelda3::OverworldCustomAreaSpecificBGPalette +
              (current_map_ * 2)] |
      ((*rom_)[zelda3::OverworldCustomAreaSpecificBGPalette +
               (current_map_ * 2) + 1]
       << 8);

  // Convert SNES color to ImVec4
  ImVec4 current_color =
      ImVec4(((current_bg_color & 0x1F) * 8) / 255.0f,
             (((current_bg_color >> 5) & 0x1F) * 8) / 255.0f,
             (((current_bg_color >> 10) & 0x1F) * 8) / 255.0f, 1.0f);

  // Color picker
  if (ColorPicker4(
          "Background Color", (float*)&current_color,
          ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB)) {
    // Convert ImVec4 back to SNES color
    uint16_t new_color =
        (static_cast<uint16_t>(current_color.x * 31) & 0x1F) |
        ((static_cast<uint16_t>(current_color.y * 31) & 0x1F) << 5) |
        ((static_cast<uint16_t>(current_color.z * 31) & 0x1F) << 10);

    // Write to ROM
    (*rom_)[zelda3::OverworldCustomAreaSpecificBGPalette + (current_map_ * 2)] =
        new_color & 0xFF;
    (*rom_)[zelda3::OverworldCustomAreaSpecificBGPalette + (current_map_ * 2) +
            1] = (new_color >> 8) & 0xFF;

    // Update the overworld map
    overworld_.mutable_overworld_map(current_map_)
        ->set_area_specific_bg_color(new_color);

    // Refresh the map
    RefreshOverworldMap();
  }

  Separator();

  // Show color preview
  Text("Color Preview:");
  ImGui::ColorButton("##bg_preview", current_color,
                     ImGuiColorEditFlags_NoTooltip, ImVec2(100, 50));

  SameLine();
  Text("SNES Color: 0x%04X", current_bg_color);
}

void OverworldEditor::DrawOverlayEditor() {
  static uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];

  // Handle vanilla ROMs
  if (asm_version == 0xFF) {
    Text("Vanilla ROM - Overlay Information:");
    Separator();

    Text("Current Map: %d (0x%02X)", current_map_, current_map_);

    // Show vanilla subscreen overlay information
    Text("Vanilla ROM - Subscreen Overlays:");
    Text("Subscreen overlays in vanilla ROMs reference special area maps");
    Text("(0x80-0x9F) for visual effects like fog, rain, backgrounds.");

    Separator();
    if (Checkbox("Show Subscreen Overlay Preview", &show_overlay_preview_)) {
      // Toggle subscreen overlay preview
    }

    if (show_overlay_preview_) {
      DrawOverlayPreview();
    }

    Separator();
    Text(
        "Note: Vanilla subscreen overlays are read-only. Use ZSCustomOverworld "
        "v1+ for "
        "editable subscreen overlays.");
    return;
  }

  // Subscreen overlays are available for all versions for LW and DW maps
  // Check if subscreen overlays are enabled (for custom overworld ROMs)
  if (asm_version != 0xFF) {
    bool overlay_enabled =
        (*rom_)[zelda3::OverworldCustomSubscreenOverlayEnabled] != 0x00;
    if (Checkbox("Enable Subscreen Overlays", &overlay_enabled)) {
      (*rom_)[zelda3::OverworldCustomSubscreenOverlayEnabled] =
          overlay_enabled ? 0x01 : 0x00;
    }

    if (!overlay_enabled) {
      Text("Subscreen overlays are disabled.");
      return;
    }
  }

  Separator();

  // Display current map's subscreen overlay
  Text("Current Map: %d (0x%02X)", current_map_, current_map_);

  // Get current subscreen overlay ID
  uint16_t current_overlay =
      (*rom_)[zelda3::OverworldCustomSubscreenOverlayArray +
              (current_map_ * 2)] |
      ((*rom_)[zelda3::OverworldCustomSubscreenOverlayArray +
               (current_map_ * 2) + 1]
       << 8);

  // Subscreen overlay ID input
  if (gui::InputHexWord("Subscreen Overlay ID", &current_overlay, 100)) {
    // Write to ROM
    (*rom_)[zelda3::OverworldCustomSubscreenOverlayArray + (current_map_ * 2)] =
        current_overlay & 0xFF;
    (*rom_)[zelda3::OverworldCustomSubscreenOverlayArray + (current_map_ * 2) +
            1] = (current_overlay >> 8) & 0xFF;

    // Update the overworld map
    overworld_.mutable_overworld_map(current_map_)
        ->set_subscreen_overlay(current_overlay);

    // Refresh the map
    RefreshOverworldMap();
  }

  Separator();

  // Show subscreen overlay information
  Text("Subscreen Overlay Information:");
  Text("ID: 0x%04X", current_overlay);

  if (current_overlay == 0x00FF) {
    Text("No overlay");
  } else if (current_overlay == 0x0093) {
    Text("Triforce Room Curtain");
  } else if (current_overlay == 0x0094) {
    Text("Under the Bridge");
  } else if (current_overlay == 0x0095) {
    Text("Sky Background (LW Death Mountain)");
  } else if (current_overlay == 0x0096) {
    Text("Pyramid Background");
  } else if (current_overlay == 0x0097) {
    Text("First Fog Overlay (Master Sword Area)");
  } else if (current_overlay == 0x009C) {
    Text("Lava Background (DW Death Mountain)");
  } else if (current_overlay == 0x009D) {
    Text("Second Fog Overlay (Lost Woods/Skull Woods)");
  } else if (current_overlay == 0x009E) {
    Text("Tree Canopy (Forest)");
  } else if (current_overlay == 0x009F) {
    Text("Rain Effect (Misery Mire)");
  } else {
    Text("Custom overlay");
  }
}

void OverworldEditor::DrawOverlayPreview() {
  if (!show_overlay_preview_)
    return;

  Text("Subscreen Overlay Preview:");
  Separator();

  // Get the subscreen overlay ID from the current map
  uint16_t overlay_id =
      overworld_.overworld_map(current_map_)->subscreen_overlay();

  // Show subscreen overlay information
  Text("Subscreen Overlay ID: 0x%04X", overlay_id);

  // Show subscreen overlay description based on common overlay IDs
  std::string overlay_desc = "";
  if (overlay_id == 0x0093) {
    overlay_desc = "Triforce Room Curtain";
  } else if (overlay_id == 0x0094) {
    overlay_desc = "Under the Bridge";
  } else if (overlay_id == 0x0095) {
    overlay_desc = "Sky Background (LW Death Mountain)";
  } else if (overlay_id == 0x0096) {
    overlay_desc = "Pyramid Background";
  } else if (overlay_id == 0x0097) {
    overlay_desc = "First Fog Overlay (Master Sword Area)";
  } else if (overlay_id == 0x009C) {
    overlay_desc = "Lava Background (DW Death Mountain)";
  } else if (overlay_id == 0x009D) {
    overlay_desc = "Second Fog Overlay (Lost Woods/Skull Woods)";
  } else if (overlay_id == 0x009E) {
    overlay_desc = "Tree Canopy (Forest)";
  } else if (overlay_id == 0x009F) {
    overlay_desc = "Rain Effect (Misery Mire)";
  } else if (overlay_id == 0x00FF) {
    overlay_desc = "No Subscreen Overlay";
  } else {
    overlay_desc = "Custom subscreen overlay effect";
  }
  Text("Description: %s", overlay_desc.c_str());

  Separator();

  // Map subscreen overlay ID to special area map for preview
  int overlay_map_index = -1;
  if (overlay_id >= 0x80 && overlay_id < 0xA0) {
    overlay_map_index = overlay_id;
  }

  if (overlay_map_index >= 0 && overlay_map_index < zelda3::kNumOverworldMaps) {
    Text("Subscreen Overlay Source Map: %d (0x%02X)", overlay_map_index,
         overlay_map_index);

    // Get the subscreen overlay map's bitmap
    const auto& overlay_bitmap = maps_bmp_[overlay_map_index];

    if (overlay_bitmap.is_active()) {
      // Display the subscreen overlay map bitmap
      ImVec2 image_size(256, 256);  // Scale down for preview
      ImGui::Image((ImTextureID)(intptr_t)overlay_bitmap.texture(), image_size);

      Separator();
      Text("This subscreen overlay would be displayed semi-transparently");
      Text("on top of the current map when active.");

      // Show drawing order info
      if (overlay_id == 0x0095 || overlay_id == 0x0096 ||
          overlay_id == 0x009C) {
        Text("Note: This subscreen overlay is drawn as a background");
        Text("(behind the main map tiles).");
      } else {
        Text("Note: This subscreen overlay is drawn on top of");
        Text("the main map tiles.");
      }
    } else {
      Text("Subscreen overlay map bitmap not available");
    }
  } else {
    Text("Unknown subscreen overlay ID: 0x%04X", overlay_id);
    Text("Could not determine subscreen overlay source map");
  }
}

void OverworldEditor::DrawMapLockControls() {
  if (current_map_lock_) {
    PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
    Text("Map Locked: %d (0x%02X)", current_map_, current_map_);
    PopStyleColor();

    if (Button("Unlock Map")) {
      current_map_lock_ = false;
    }
  } else {
    Text("Map: %d (0x%02X) - Click to lock", current_map_, current_map_);
    if (Button("Lock Map")) {
      current_map_lock_ = true;
    }
  }
}

void OverworldEditor::DrawOverworldContextMenu() {
  // Get the current map from mouse position
  auto mouse_position = ow_map_canvas_.drawn_tile_position();
  int map_x = mouse_position.x / kOverworldMapSize;
  int map_y = mouse_position.y / kOverworldMapSize;
  int hovered_map = map_x + map_y * 8;
  if (current_world_ == 1) {
    hovered_map += 0x40;
  } else if (current_world_ == 2) {
    hovered_map += 0x80;
  }

  // Only show context menu if we're hovering over a valid map
  if (hovered_map >= 0 && hovered_map < 0xA0) {
    if (ImGui::BeginPopupContextWindow("OverworldMapContext")) {
      Text("Map %d (0x%02X)", hovered_map, hovered_map);
      Separator();

      // Map lock controls
      if (current_map_lock_ && current_map_ == hovered_map) {
        PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        Text("Currently Locked");
        PopStyleColor();
        if (MenuItem("Unlock Map")) {
          current_map_lock_ = false;
        }
      } else {
        if (MenuItem("Lock to This Map")) {
          current_map_lock_ = true;
          current_map_ = hovered_map;
        }
      }

      Separator();

      // Quick access to map settings
      if (MenuItem("Map Properties")) {
        show_properties_editor_ = true;
        current_map_ = hovered_map;
      }

      // Custom overworld features
      static uint8_t asm_version =
          (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
      if (asm_version >= 3 && asm_version != 0xFF) {
        if (MenuItem("Custom Background Color")) {
          show_custom_bg_color_editor_ = true;
          current_map_ = hovered_map;
        }

        if (MenuItem("Subscreen Overlay Settings")) {
          show_overlay_editor_ = true;
          current_map_ = hovered_map;
        }
      } else if (asm_version == 0xFF) {
        // Show vanilla subscreen overlay information for LW and DW maps only
        bool is_special_overworld_map =
            (hovered_map >= 0x80 && hovered_map < 0xA0);
        if (!is_special_overworld_map) {
          if (MenuItem("View Subscreen Overlay")) {
            show_overlay_editor_ = true;
            current_map_ = hovered_map;
          }
        }
      }

      Separator();

      // Canvas controls
      if (MenuItem("Reset Canvas Position")) {
        ow_map_canvas_.set_scrolling(ImVec2(0, 0));
      }

      if (MenuItem("Zoom to Fit")) {
        ow_map_canvas_.set_global_scale(1.0f);
        ow_map_canvas_.set_scrolling(ImVec2(0, 0));
      }

      ImGui::EndPopup();
    }
  }
}

void OverworldEditor::HandleMapInteraction() {
  // Handle middle-click for map interaction instead of right-click
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) &&
      ImGui::IsItemHovered()) {
    // Get the current map from mouse position
    auto mouse_position = ow_map_canvas_.drawn_tile_position();
    int map_x = mouse_position.x / kOverworldMapSize;
    int map_y = mouse_position.y / kOverworldMapSize;
    int hovered_map = map_x + map_y * 8;
    if (current_world_ == 1) {
      hovered_map += 0x40;
    } else if (current_world_ == 2) {
      hovered_map += 0x80;
    }

    // Only interact if we're hovering over a valid map
    if (hovered_map >= 0 && hovered_map < 0xA0) {
      // Toggle map lock or open properties panel
      if (current_map_lock_ && current_map_ == hovered_map) {
        current_map_lock_ = false;
      } else {
        current_map_lock_ = true;
        current_map_ = hovered_map;
        show_map_properties_panel_ = true;
      }
    }
  }

  // Handle double-click to open properties panel
  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
      ImGui::IsItemHovered()) {
    show_map_properties_panel_ = true;
  }
}

void OverworldEditor::DrawMapPropertiesPanel() {
  if (!overworld_.is_loaded()) {
    Text("No overworld loaded");
    return;
  }

  // Header with map info and lock status
  ImGui::BeginGroup();
  if (current_map_lock_) {
    PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
    Text("Map Locked: %d (0x%02X)", current_map_, current_map_);
    PopStyleColor();
  } else {
    Text("Current Map: %d (0x%02X)", current_map_, current_map_);
  }

  SameLine();
  if (Button(current_map_lock_ ? "Unlock" : "Lock")) {
    current_map_lock_ = !current_map_lock_;
  }
  ImGui::EndGroup();

  Separator();

  // Create tabs for different property categories
  if (BeginTabBar("MapPropertiesTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
    // Basic Properties Tab
    if (BeginTabItem("Basic Properties")) {
      if (BeginTable(
              "BasicProperties", 2,
              ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed,
                                150);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        TableNextColumn();
        Text("World");
        TableNextColumn();
        ImGui::SetNextItemWidth(100.f);
        if (ImGui::Combo("##world", &current_world_, kWorldList.data(), 3)) {
          // Update current map based on world change
          if (current_map_ >= 0x40 && current_world_ == 0) {
            current_map_ -= 0x40;
          } else if (current_map_ < 0x40 && current_world_ == 1) {
            current_map_ += 0x40;
          } else if (current_map_ < 0x80 && current_world_ == 2) {
            current_map_ += 0x80;
          } else if (current_map_ >= 0x80 && current_world_ != 2) {
            current_map_ -= 0x80;
          }
        }

        TableNextColumn();
        Text("Area Graphics");
        TableNextColumn();
        if (gui::InputHexByte("##AreaGfx",
                              overworld_.mutable_overworld_map(current_map_)
                                  ->mutable_area_graphics(),
                              kInputFieldSize)) {
          RefreshMapProperties();
          RefreshOverworldMap();
        }

        TableNextColumn();
        Text("Area Palette");
        TableNextColumn();
        if (gui::InputHexByte("##AreaPal",
                              overworld_.mutable_overworld_map(current_map_)
                                  ->mutable_area_palette(),
                              kInputFieldSize)) {
          RefreshMapProperties();
          status_ = RefreshMapPalette();
          RefreshOverworldMap();
        }

        TableNextColumn();
        Text("Message ID");
        TableNextColumn();
        if (gui::InputHexWord("##MsgId",
                              overworld_.mutable_overworld_map(current_map_)
                                  ->mutable_message_id(),
                              kInputFieldSize + 20)) {
          RefreshMapProperties();
          RefreshOverworldMap();
        }

        TableNextColumn();
        Text("Mosaic Effect");
        TableNextColumn();
        if (ImGui::Checkbox("##mosaic",
                            overworld_.mutable_overworld_map(current_map_)
                                ->mutable_mosaic())) {
          RefreshMapProperties();
          RefreshOverworldMap();
        }
        HOVER_HINT("Enable Mosaic effect for the current map");

        ImGui::EndTable();
      }
      EndTabItem();
    }

    // Sprite Properties Tab
    if (BeginTabItem("Sprite Properties")) {
      if (BeginTable(
              "SpriteProperties", 2,
              ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed,
                                150);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        TableNextColumn();
        Text("Game State");
        TableNextColumn();
        ImGui::SetNextItemWidth(100.f);
        if (ImGui::Combo("##GameState", &game_state_,
                         kGamePartComboString.data(), 3)) {
          RefreshMapProperties();
          RefreshOverworldMap();
        }

        TableNextColumn();
        Text("Sprite Graphics 1");
        TableNextColumn();
        if (gui::InputHexByte("##SprGfx1",
                              overworld_.mutable_overworld_map(current_map_)
                                  ->mutable_sprite_graphics(1),
                              kInputFieldSize)) {
          RefreshMapProperties();
          RefreshOverworldMap();
        }

        TableNextColumn();
        Text("Sprite Graphics 2");
        TableNextColumn();
        if (gui::InputHexByte("##SprGfx2",
                              overworld_.mutable_overworld_map(current_map_)
                                  ->mutable_sprite_graphics(2),
                              kInputFieldSize)) {
          RefreshMapProperties();
          RefreshOverworldMap();
        }

        TableNextColumn();
        Text("Sprite Palette 1");
        TableNextColumn();
        if (gui::InputHexByte("##SprPal1",
                              overworld_.mutable_overworld_map(current_map_)
                                  ->mutable_sprite_palette(1),
                              kInputFieldSize)) {
          RefreshMapProperties();
          RefreshOverworldMap();
        }

        TableNextColumn();
        Text("Sprite Palette 2");
        TableNextColumn();
        if (gui::InputHexByte("##SprPal2",
                              overworld_.mutable_overworld_map(current_map_)
                                  ->mutable_sprite_palette(2),
                              kInputFieldSize)) {
          RefreshMapProperties();
          RefreshOverworldMap();
        }

        ImGui::EndTable();
      }
      EndTabItem();
    }

    // Custom Overworld Features Tab
    static uint8_t asm_version =
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version != 0xFF && BeginTabItem("Custom Features")) {
      if (BeginTable(
              "CustomFeatures", 2,
              ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed,
                                150);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        TableNextColumn();
        Text("Area Size");
        TableNextColumn();
        static const char* area_size_names[] = {"Small (1x1)", "Large (2x2)",
                                                "Wide (2x1)", "Tall (1x2)"};
        int current_area_size = static_cast<int>(
            overworld_.overworld_map(current_map_)->area_size());
        ImGui::SetNextItemWidth(120.f);
        if (ImGui::Combo("##AreaSize", &current_area_size, area_size_names,
                         4)) {
          overworld_.mutable_overworld_map(current_map_)
              ->SetAreaSize(
                  static_cast<zelda3::AreaSizeEnum>(current_area_size));
          RefreshOverworldMap();
        }

        if (asm_version >= 2) {
          TableNextColumn();
          Text("Main Palette");
          TableNextColumn();
          if (gui::InputHexByte("##MainPal",
                                overworld_.mutable_overworld_map(current_map_)
                                    ->mutable_main_palette(),
                                kInputFieldSize)) {
            RefreshMapProperties();
            status_ = RefreshMapPalette();
            RefreshOverworldMap();
          }
        }

        if (asm_version >= 3) {
          TableNextColumn();
          Text("Animated GFX");
          TableNextColumn();
          if (gui::InputHexByte("##AnimGfx",
                                overworld_.mutable_overworld_map(current_map_)
                                    ->mutable_animated_gfx(),
                                kInputFieldSize)) {
            RefreshMapProperties();
            RefreshOverworldMap();
          }

          TableNextColumn();
          Text("Subscreen Overlay");
          TableNextColumn();
          if (gui::InputHexWord("##SubOverlay",
                                overworld_.mutable_overworld_map(current_map_)
                                    ->mutable_subscreen_overlay(),
                                kInputFieldSize + 20)) {
            RefreshMapProperties();
            RefreshOverworldMap();
          }
        }

        ImGui::EndTable();
      }

      Separator();

      // Quick action buttons
      ImGui::BeginGroup();
      if (Button("Custom Background Color")) {
        show_custom_bg_color_editor_ = !show_custom_bg_color_editor_;
      }
      SameLine();
      if (Button("Overlay Settings")) {
        show_overlay_editor_ = !show_overlay_editor_;
      }
      ImGui::EndGroup();

      EndTabItem();
    }

    // Tile Graphics Tab
    if (BeginTabItem("Tile Graphics")) {
      Text("Custom Tile Graphics (8 sheets per map):");
      Separator();

      if (BeginTable(
              "TileGraphics", 4,
              ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Sheet", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("GFX ID", ImGuiTableColumnFlags_WidthFixed,
                                120);
        ImGui::TableSetupColumn("Sheet", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("GFX ID", ImGuiTableColumnFlags_WidthFixed,
                                120);

        for (int i = 0; i < 4; i++) {
          TableNextColumn();
          Text("Sheet %d", i);
          TableNextColumn();
          if (gui::InputHexByte(absl::StrFormat("Sheet %d GFX", i).c_str(),
                                overworld_.mutable_overworld_map(current_map_)
                                    ->mutable_custom_tileset(i),
                                100.f)) {
            RefreshMapProperties();
            RefreshOverworldMap();
          }

          TableNextColumn();
          Text("Sheet %d", i + 4);
          TableNextColumn();
          if (gui::InputHexByte(absl::StrFormat("Sheet %d GFX", i + 4).c_str(),
                                overworld_.mutable_overworld_map(current_map_)
                                    ->mutable_custom_tileset(i + 4),
                                100.f)) {
            RefreshMapProperties();
            RefreshOverworldMap();
          }
        }

        ImGui::EndTable();
      }
      EndTabItem();
    }

    EndTabBar();
  }
}

void OverworldEditor::SetupOverworldCanvasContextMenu() {
  // Clear any existing context menu items
  ow_map_canvas_.ClearContextMenuItems();

  // Add overworld-specific context menu items
  gui::Canvas::ContextMenuItem lock_item;
  lock_item.label = current_map_lock_ ? "Unlock Map" : "Lock to This Map";
  lock_item.callback = [this]() {
    current_map_lock_ = !current_map_lock_;
    if (current_map_lock_) {
      // Get the current map from mouse position
      auto mouse_position = ow_map_canvas_.drawn_tile_position();
      int map_x = mouse_position.x / kOverworldMapSize;
      int map_y = mouse_position.y / kOverworldMapSize;
      int hovered_map = map_x + map_y * 8;
      if (current_world_ == 1) {
        hovered_map += 0x40;
      } else if (current_world_ == 2) {
        hovered_map += 0x80;
      }
      if (hovered_map >= 0 && hovered_map < 0xA0) {
        current_map_ = hovered_map;
      }
    }
  };
  ow_map_canvas_.AddContextMenuItem(lock_item);

  // Map Properties
  gui::Canvas::ContextMenuItem properties_item;
  properties_item.label = "Map Properties";
  properties_item.callback = [this]() {
    show_map_properties_panel_ = true;
  };
  ow_map_canvas_.AddContextMenuItem(properties_item);

  // Custom overworld features (only show if v3+)
  static uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  if (asm_version >= 3 && asm_version != 0xFF) {
    // Custom Background Color
    gui::Canvas::ContextMenuItem bg_color_item;
    bg_color_item.label = "Custom Background Color";
    bg_color_item.callback = [this]() {
      show_custom_bg_color_editor_ = true;
    };
    ow_map_canvas_.AddContextMenuItem(bg_color_item);

    // Overlay Settings
    gui::Canvas::ContextMenuItem overlay_item;
    overlay_item.label = "Overlay Settings";
    overlay_item.callback = [this]() {
      show_overlay_editor_ = true;
    };
    ow_map_canvas_.AddContextMenuItem(overlay_item);
  }

  // Canvas controls
  gui::Canvas::ContextMenuItem reset_pos_item;
  reset_pos_item.label = "Reset Canvas Position";
  reset_pos_item.callback = [this]() {
    ow_map_canvas_.set_scrolling(ImVec2(0, 0));
  };
  ow_map_canvas_.AddContextMenuItem(reset_pos_item);

  gui::Canvas::ContextMenuItem zoom_fit_item;
  zoom_fit_item.label = "Zoom to Fit";
  zoom_fit_item.callback = [this]() {
    ow_map_canvas_.set_global_scale(1.0f);
    ow_map_canvas_.set_scrolling(ImVec2(0, 0));
  };
  ow_map_canvas_.AddContextMenuItem(zoom_fit_item);
}

void OverworldEditor::DrawOverworldProperties() {
  static bool init_properties = false;

  if (!init_properties) {
    for (int i = 0; i < 0x40; i++) {
      std::string area_graphics_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i)->area_graphics());
      properties_canvas_.mutable_labels(OverworldProperty::LW_AREA_GFX)
          ->push_back(area_graphics_str);

      area_graphics_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->area_graphics());
      properties_canvas_.mutable_labels(OverworldProperty::DW_AREA_GFX)
          ->push_back(area_graphics_str);

      std::string area_palette_str =
          absl::StrFormat("%02hX", overworld_.overworld_map(i)->area_palette());
      properties_canvas_.mutable_labels(OverworldProperty::LW_AREA_PAL)
          ->push_back(area_palette_str);

      area_palette_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->area_palette());
      properties_canvas_.mutable_labels(OverworldProperty::DW_AREA_PAL)
          ->push_back(area_palette_str);
      std::string sprite_gfx_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i)->sprite_graphics(1));
      properties_canvas_.mutable_labels(OverworldProperty::LW_SPR_GFX_PART1)
          ->push_back(sprite_gfx_str);

      sprite_gfx_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i)->sprite_graphics(2));
      properties_canvas_.mutable_labels(OverworldProperty::LW_SPR_GFX_PART2)
          ->push_back(sprite_gfx_str);

      sprite_gfx_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->sprite_graphics(1));
      properties_canvas_.mutable_labels(OverworldProperty::DW_SPR_GFX_PART1)
          ->push_back(sprite_gfx_str);

      sprite_gfx_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->sprite_graphics(2));
      properties_canvas_.mutable_labels(OverworldProperty::DW_SPR_GFX_PART2)
          ->push_back(sprite_gfx_str);

      std::string sprite_palette_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i)->sprite_palette(1));
      properties_canvas_.mutable_labels(OverworldProperty::LW_SPR_PAL_PART1)
          ->push_back(sprite_palette_str);

      sprite_palette_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i)->sprite_palette(2));
      properties_canvas_.mutable_labels(OverworldProperty::LW_SPR_PAL_PART2)
          ->push_back(sprite_palette_str);

      sprite_palette_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->sprite_palette(1));
      properties_canvas_.mutable_labels(OverworldProperty::DW_SPR_PAL_PART1)
          ->push_back(sprite_palette_str);

      sprite_palette_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->sprite_palette(2));
      properties_canvas_.mutable_labels(OverworldProperty::DW_SPR_PAL_PART2)
          ->push_back(sprite_palette_str);
    }
    init_properties = true;
  }

  Text("Area Gfx LW/DW");
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_AREA_GFX);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::DW_AREA_GFX);
  ImGui::Separator();

  Text("Sprite Gfx LW/DW");
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_SPR_GFX_PART1);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::DW_SPR_GFX_PART1);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_SPR_GFX_PART2);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::DW_SPR_GFX_PART2);
  ImGui::Separator();

  Text("Area Pal LW/DW");
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_AREA_PAL);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::DW_AREA_PAL);

  static bool show_gfx_group = false;
  Checkbox("Show Gfx Group Editor", &show_gfx_group);
  if (show_gfx_group) {
    gui::BeginWindowWithDisplaySettings("Gfx Group Editor", &show_gfx_group);
    status_ = gfx_group_editor_.Update();
    gui::EndWindowWithDisplaySettings();
  }
}

absl::Status OverworldEditor::UpdateUsageStats() {
  if (BeginTable("UsageStatsTable", 3, kOWEditFlags, ImVec2(0, 0))) {
    TableSetupColumn("Entrances");
    TableSetupColumn("Grid", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    if (BeginChild("UnusedSpritesetScroll", ImVec2(0, 0), true,
                   ImGuiWindowFlags_HorizontalScrollbar)) {
      for (int i = 0; i < 0x81; i++) {
        auto entrance_name = rom_->resource_label()->CreateOrGetLabel(
            "Dungeon Entrance Names", util::HexByte(i),
            zelda3::kEntranceNames[i]);
        std::string str = absl::StrFormat("%#x - %s", i, entrance_name);
        if (Selectable(str.c_str(), selected_entrance_ == i,
                       overworld_.entrances().at(i).deleted
                           ? ImGuiSelectableFlags_Disabled
                           : 0)) {
          selected_entrance_ = i;
          selected_usage_map_ = overworld_.entrances().at(i).map_id_;
          properties_canvas_.set_highlight_tile_id(selected_usage_map_);
        }
        if (IsItemHovered()) {
          BeginTooltip();
          Text("Entrance ID: %d", i);
          Text("Map ID: %d", overworld_.entrances().at(i).map_id_);
          Text("Entrance ID: %d", overworld_.entrances().at(i).entrance_id_);
          Text("X: %d", overworld_.entrances().at(i).x_);
          Text("Y: %d", overworld_.entrances().at(i).y_);
          Text("Deleted? %s",
               overworld_.entrances().at(i).deleted ? "Yes" : "No");
          EndTooltip();
        }
      }
      EndChild();
    }

    TableNextColumn();
    DrawUsageGrid();

    TableNextColumn();
    DrawOverworldProperties();

    EndTable();
  }
  return absl::OkStatus();
}

void OverworldEditor::DrawUsageGrid() {
  // Create a grid of 8x8 squares
  int total_squares = 128;
  int squares_wide = 8;
  int squares_tall = (total_squares + squares_wide - 1) /
                     squares_wide;  // Ceiling of total_squares/squares_wide

  // Loop through each row
  for (int row = 0; row < squares_tall; ++row) {
    NewLine();

    for (int col = 0; col < squares_wide; ++col) {
      if (row * squares_wide + col >= total_squares) {
        break;
      }
      // Determine if this square should be highlighted
      bool highlight = selected_usage_map_ == (row * squares_wide + col);

      // Set highlight color if needed
      if (highlight) {
        PushStyleColor(ImGuiCol_Button,
                       ImVec4(1.0f, 0.5f, 0.0f,
                              1.0f));  // Or any highlight color
      }

      // Create a button or selectable for each square
      if (Button("##square", ImVec2(20, 20))) {
        // Switch over to the room editor tab
        // and add a room tab by the ID of the square
        // that was clicked
      }

      // Reset style if it was highlighted
      if (highlight) {
        PopStyleColor();
      }

      // Check if the square is hovered
      if (IsItemHovered()) {
        // Display a tooltip with all the room properties
      }

      // Keep squares in the same line
      SameLine();
    }
  }
}

void OverworldEditor::DrawDebugWindow() {
  Text("Current Map: %d", current_map_);
  Text("Current Tile16: %d", current_tile16_);
  int relative_x = (int)ow_map_canvas_.drawn_tile_position().x % 512;
  int relative_y = (int)ow_map_canvas_.drawn_tile_position().y % 512;
  Text("Current Tile16 Drawn Position (Relative): %d, %d", relative_x,
       relative_y);

  // Print the size of the overworld map_tiles per world
  Text("Light World Map Tiles: %d",
       (int)overworld_.mutable_map_tiles()->light_world.size());
  Text("Dark World Map Tiles: %d",
       (int)overworld_.mutable_map_tiles()->dark_world.size());
  Text("Special World Map Tiles: %d",
       (int)overworld_.mutable_map_tiles()->special_world.size());

  static bool view_lw_map_tiles = false;
  static MemoryEditor mem_edit;
  // Let's create buttons which let me view containers in the memory editor
  if (Button("View Light World Map Tiles")) {
    view_lw_map_tiles = !view_lw_map_tiles;
  }

  if (view_lw_map_tiles) {
    mem_edit.DrawContents(
        overworld_.mutable_map_tiles()->light_world[current_map_].data(),
        overworld_.mutable_map_tiles()->light_world[current_map_].size());
  }
}

absl::Status OverworldEditor::Clear() {
  overworld_.Destroy();
  current_graphics_set_.clear();
  all_gfx_loaded_ = false;
  map_blockset_loaded_ = false;
  return absl::OkStatus();
}

absl::Status OverworldEditor::ApplyZSCustomOverworldASM(int target_version) {
  if (!core::FeatureFlags::get().overworld.kApplyZSCustomOverworldASM) {
    return absl::FailedPreconditionError(
        "ZSCustomOverworld ASM application is disabled in feature flags");
  }

  // Validate target version
  if (target_version < 2 || target_version > 3) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid target version: %d. Must be 2 or 3.", target_version));
  }

  // Check current ROM version
  uint8_t current_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  if (current_version != 0xFF && current_version >= target_version) {
    return absl::AlreadyExistsError(absl::StrFormat(
        "ROM is already version %d or higher", current_version));
  }

  util::logf("Applying ZSCustomOverworld ASM v%d to ROM...", target_version);

  // Initialize Asar wrapper
  auto asar_wrapper = std::make_unique<app::core::AsarWrapper>();
  RETURN_IF_ERROR(asar_wrapper->Initialize());

  // Create backup of ROM data
  std::vector<uint8_t> original_rom_data = rom_->vector();
  std::vector<uint8_t> working_rom_data = original_rom_data;

  try {
    // Determine which ASM file to apply
    std::string asm_file_path;
    if (target_version == 3) {
      asm_file_path = "assets/asm/yaze.asm";  // Master file with v3
    } else {
      asm_file_path = "assets/asm/ZSCustomOverworld.asm";  // v2 standalone
    }

    // Check if ASM file exists
    if (!std::filesystem::exists(asm_file_path)) {
      return absl::NotFoundError(
          absl::StrFormat("ASM file not found: %s", asm_file_path));
    }

    // Apply the ASM patch
    auto patch_result =
        asar_wrapper->ApplyPatch(asm_file_path, working_rom_data);
    if (!patch_result.ok()) {
      return absl::InternalError(absl::StrFormat(
          "Failed to apply ASM patch: %s", patch_result.status().message()));
    }

    const auto& result = patch_result.value();
    if (!result.success) {
      std::string error_details = "ASM patch failed with errors:\n";
      for (const auto& error : result.errors) {
        error_details += "  - " + error + "\n";
      }
      if (!result.warnings.empty()) {
        error_details += "Warnings:\n";
        for (const auto& warning : result.warnings) {
          error_details += "  - " + warning + "\n";
        }
      }
      return absl::InternalError(error_details);
    }

    // Update ROM with patched data
    RETURN_IF_ERROR(rom_->LoadFromData(working_rom_data, false));

    // Update version marker and feature flags
    RETURN_IF_ERROR(UpdateROMVersionMarkers(target_version));

    // Log symbols found during patching
    util::logf("ASM patch applied successfully. Found %zu symbols:",
               result.symbols.size());
    for (const auto& symbol : result.symbols) {
      util::logf("  %s @ $%06X", symbol.name.c_str(), symbol.address);
    }

    // Refresh overworld data to reflect changes
    RETURN_IF_ERROR(overworld_.Load(rom_));

    util::logf("ZSCustomOverworld v%d successfully applied to ROM",
               target_version);
    return absl::OkStatus();

  } catch (const std::exception& e) {
    // Restore original ROM data on any exception
    auto restore_result = rom_->LoadFromData(original_rom_data, false);
    if (!restore_result.ok()) {
      util::logf("Failed to restore ROM data: %s",
                 restore_result.message().data());
    }
    return absl::InternalError(
        absl::StrFormat("Exception during ASM application: %s", e.what()));
  }
}

absl::Status OverworldEditor::UpdateROMVersionMarkers(int target_version) {
  // Set the main version marker
  (*rom_)[zelda3::OverworldCustomASMHasBeenApplied] =
      static_cast<uint8_t>(target_version);

  // Enable feature flags based on target version
  if (target_version >= 2) {
    // v2+ features
    (*rom_)[zelda3::OverworldCustomAreaSpecificBGEnabled] = 0x01;
    (*rom_)[zelda3::OverworldCustomMainPaletteEnabled] = 0x01;

    util::logf("Enabled v2+ features: Custom BG colors, Main palettes");
  }

  if (target_version >= 3) {
    // v3 features
    (*rom_)[zelda3::OverworldCustomSubscreenOverlayEnabled] = 0x01;
    (*rom_)[zelda3::OverworldCustomAnimatedGFXEnabled] = 0x01;
    (*rom_)[zelda3::OverworldCustomTileGFXGroupEnabled] = 0x01;
    (*rom_)[zelda3::OverworldCustomMosaicEnabled] = 0x01;

    util::logf(
        "Enabled v3+ features: Subscreen overlays, Animated GFX, Tile GFX "
        "groups, Mosaic");

    // Initialize area size data for v3 (set all areas to small by default)
    for (int i = 0; i < 0xA0; i++) {
      (*rom_)[zelda3::kOverworldScreenSize + i] =
          static_cast<uint8_t>(zelda3::AreaSizeEnum::SmallArea);
    }

    // Set appropriate sizes for known large areas
    const std::vector<int> large_areas = {
        0x00, 0x02, 0x05, 0x07, 0x0A, 0x0B, 0x0F, 0x10, 0x11, 0x12,
        0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1D,
        0x1E, 0x25, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x30,
        0x32, 0x33, 0x34, 0x35, 0x37, 0x3A, 0x3B, 0x3C, 0x3F};

    for (int area_id : large_areas) {
      if (area_id < 0xA0) {
        (*rom_)[zelda3::kOverworldScreenSize + area_id] =
            static_cast<uint8_t>(zelda3::AreaSizeEnum::LargeArea);
      }
    }

    util::logf("Initialized area size data for %zu areas", large_areas.size());
  }

  util::logf("ROM version markers updated to v%d", target_version);
  return absl::OkStatus();
}

// Scratch space canvas methods
absl::Status OverworldEditor::DrawScratchSpace() {
  // Slot selector
  Text("Scratch Space Slot:");
  for (int i = 0; i < 4; i++) {
    if (i > 0)
      SameLine();
    bool is_current = (current_scratch_slot_ == i);
    if (is_current)
      PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.7f, 0.4f, 1.0f));
    if (Button(std::to_string(i + 1).c_str(), ImVec2(25, 25))) {
      current_scratch_slot_ = i;
    }
    if (is_current)
      PopStyleColor();
  }

  SameLine();
  if (Button("Save Selection")) {
    RETURN_IF_ERROR(SaveCurrentSelectionToScratch(current_scratch_slot_));
  }
  SameLine();
  if (Button("Load")) {
    RETURN_IF_ERROR(LoadScratchToSelection(current_scratch_slot_));
  }
  SameLine();
  if (Button("Clear")) {
    RETURN_IF_ERROR(ClearScratchSpace(current_scratch_slot_));
  }

  // Selection transfer buttons
  Separator();
  Text("Selection Transfer:");
  if (Button(ICON_MD_DOWNLOAD " From Overworld")) {
    // Transfer current overworld selection to scratch space
    if (ow_map_canvas_.select_rect_active() &&
        !ow_map_canvas_.selected_tiles().empty()) {
      RETURN_IF_ERROR(SaveCurrentSelectionToScratch(current_scratch_slot_));
    }
  }
  HOVER_HINT("Copy current overworld selection to this scratch slot");

  SameLine();
  if (Button(ICON_MD_UPLOAD " To Clipboard")) {
    // Copy scratch selection to clipboard for pasting in overworld
    if (scratch_canvas_.select_rect_active() &&
        !scratch_canvas_.selected_tiles().empty()) {
      // Copy scratch selection to clipboard
      std::vector<int> scratch_tile_ids;
      for (const auto& tile_pos : scratch_canvas_.selected_tiles()) {
        int tile_x = static_cast<int>(tile_pos.x) / 32;
        int tile_y = static_cast<int>(tile_pos.y) / 32;
        if (tile_x >= 0 && tile_x < 32 && tile_y >= 0 && tile_y < 32) {
          scratch_tile_ids.push_back(
              scratch_spaces_[current_scratch_slot_].tile_data[tile_x][tile_y]);
        }
      }
      if (!scratch_tile_ids.empty() && context_) {
        const auto& points = scratch_canvas_.selected_points();
        int width =
            std::abs(static_cast<int>((points[1].x - points[0].x) / 32)) + 1;
        int height =
            std::abs(static_cast<int>((points[1].y - points[0].y) / 32)) + 1;
        context_->shared_clipboard.overworld_tile16_ids =
            std::move(scratch_tile_ids);
        context_->shared_clipboard.overworld_width = width;
        context_->shared_clipboard.overworld_height = height;
        context_->shared_clipboard.has_overworld_tile16 = true;
      }
    }
  }
  HOVER_HINT("Copy scratch selection to clipboard for pasting in overworld");

  if (context_ && context_->shared_clipboard.has_overworld_tile16) {
    Text(ICON_MD_CONTENT_PASTE
         " Pattern ready! Use Shift+Click to stamp, or paste in overworld");
  }

  Text("Slot %d: %s (%dx%d)", current_scratch_slot_ + 1,
       scratch_spaces_[current_scratch_slot_].name.c_str(),
       scratch_spaces_[current_scratch_slot_].width,
       scratch_spaces_[current_scratch_slot_].height);
  Text(
      "Select tiles from Tile16 tab or make selections in overworld, then draw "
      "here!");

  // Initialize scratch bitmap with proper size based on scratch space dimensions
  auto& current_slot = scratch_spaces_[current_scratch_slot_];
  if (!current_slot.scratch_bitmap.is_active()) {
    // Create bitmap based on scratch space dimensions (each tile is 16x16)
    int bitmap_width = current_slot.width * 16;
    int bitmap_height = current_slot.height * 16;
    std::vector<uint8_t> empty_data(bitmap_width * bitmap_height, 0);
    current_slot.scratch_bitmap.Create(bitmap_width, bitmap_height, 8,
                                       empty_data);
    if (all_gfx_loaded_) {
      palette_ = overworld_.current_area_palette();
      current_slot.scratch_bitmap.SetPalette(palette_);
      core::Renderer::Get().RenderBitmap(&current_slot.scratch_bitmap);
    }
  }

  // Draw the scratch space canvas with dynamic sizing
  gui::BeginPadding(3);
  ImGui::BeginGroup();

  // Set proper content size for scrolling based on scratch space dimensions
  ImVec2 scratch_content_size(current_slot.width * 16 + 4,
                              current_slot.height * 16 + 4);
  gui::BeginChildWithScrollbar("##ScratchSpaceScrollRegion",
                               scratch_content_size);
  scratch_canvas_.DrawBackground();
  gui::EndPadding();

  // Disable context menu for scratch space to allow right-click selection
  scratch_canvas_.SetContextMenuEnabled(false);

  // Draw the scratch bitmap with proper scaling
  if (current_slot.scratch_bitmap.is_active()) {
    scratch_canvas_.DrawBitmap(current_slot.scratch_bitmap, 2, 2, 1.0f);
  }

  // Simplified scratch space - just basic tile drawing like the original
  if (map_blockset_loaded_) {
    scratch_canvas_.DrawTileSelector(32.0f);
  }

  scratch_canvas_.DrawGrid();
  scratch_canvas_.DrawOverlay();

  EndChild();
  ImGui::EndGroup();

  return absl::OkStatus();
}

void OverworldEditor::DrawScratchSpaceEdits() {
  // Handle painting like the main overworld - continuous drawing
  auto mouse_position = scratch_canvas_.drawn_tile_position();

  // Use the scratch canvas scale and grid settings
  float canvas_scale = scratch_canvas_.global_scale();
  int grid_size =
      32;  // 32x32 grid for scratch space (matches kOverworldCanvasSize)

  // Calculate tile position using proper canvas scaling
  int tile_x = static_cast<int>(mouse_position.x) / grid_size;
  int tile_y = static_cast<int>(mouse_position.y) / grid_size;

  // Get current scratch slot dimensions
  auto& current_slot = scratch_spaces_[current_scratch_slot_];
  int max_width = current_slot.width > 0 ? current_slot.width : 20;
  int max_height = current_slot.height > 0 ? current_slot.height : 30;

  // Bounds check for current scratch space dimensions
  if (tile_x >= 0 && tile_x < max_width && tile_y >= 0 && tile_y < max_height) {
    // Bounds check for our tile_data array (always 32x32 max)
    if (tile_x < 32 && tile_y < 32) {
      current_slot.tile_data[tile_x][tile_y] = current_tile16_;
    }

    // Update the bitmap immediately for visual feedback
    UpdateScratchBitmapTile(tile_x, tile_y, current_tile16_);

    // Mark this scratch space as in use
    if (!current_slot.in_use) {
      current_slot.in_use = true;
      current_slot.name =
          absl::StrFormat("Layout %d", current_scratch_slot_ + 1);
    }
  }
}

void OverworldEditor::DrawScratchSpacePattern() {
  // Handle drawing patterns from overworld selections
  auto mouse_position = scratch_canvas_.drawn_tile_position();

  // Use 32x32 grid size (same as scratch canvas grid)
  int start_tile_x = static_cast<int>(mouse_position.x) / 32;
  int start_tile_y = static_cast<int>(mouse_position.y) / 32;

  // Get the selected tiles from overworld via clipboard
  if (!context_ || !context_->shared_clipboard.has_overworld_tile16) {
    return;
  }

  const auto& tile_ids = context_->shared_clipboard.overworld_tile16_ids;
  int pattern_width = context_->shared_clipboard.overworld_width;
  int pattern_height = context_->shared_clipboard.overworld_height;

  if (tile_ids.empty())
    return;

  auto& current_slot = scratch_spaces_[current_scratch_slot_];
  int max_width = current_slot.width > 0 ? current_slot.width : 20;
  int max_height = current_slot.height > 0 ? current_slot.height : 30;

  // Draw the pattern to scratch space
  int idx = 0;
  for (int py = 0; py < pattern_height && (start_tile_y + py) < max_height;
       ++py) {
    for (int px = 0; px < pattern_width && (start_tile_x + px) < max_width;
         ++px) {
      if (idx < static_cast<int>(tile_ids.size())) {
        int tile_id = tile_ids[idx];
        int scratch_x = start_tile_x + px;
        int scratch_y = start_tile_y + py;

        // Bounds check for tile_data array
        if (scratch_x >= 0 && scratch_x < 32 && scratch_y >= 0 &&
            scratch_y < 32) {
          current_slot.tile_data[scratch_x][scratch_y] = tile_id;
          UpdateScratchBitmapTile(scratch_x, scratch_y, tile_id);
        }
        idx++;
      }
    }
  }

  // Mark scratch space as modified
  current_slot.in_use = true;
  if (current_slot.name == "Empty") {
    current_slot.name =
        absl::StrFormat("Pattern %dx%d", pattern_width, pattern_height);
  }
}

void OverworldEditor::UpdateScratchBitmapTile(int tile_x, int tile_y,
                                              int tile_id, int slot) {
  // Use current slot if not specified
  if (slot == -1)
    slot = current_scratch_slot_;

  // Get the tile data from the tile16 blockset
  auto tile_data = gfx::GetTilemapData(tile16_blockset_, tile_id);
  if (tile_data.empty())
    return;

  auto& scratch_slot = scratch_spaces_[slot];

  // Use canvas grid size (32x32) for consistent scaling
  const int grid_size = 32;
  int scratch_bitmap_width = scratch_slot.scratch_bitmap.width();
  int scratch_bitmap_height = scratch_slot.scratch_bitmap.height();

  // Calculate pixel position in scratch bitmap
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      int src_index = y * 16 + x;

      // Scale to grid size - each tile takes up grid_size x grid_size pixels
      int dst_x = tile_x * grid_size + x + x;  // Double scaling for 32x32 grid
      int dst_y = tile_y * grid_size + y + y;

      // Bounds check for scratch bitmap
      if (dst_x >= 0 && dst_x < scratch_bitmap_width && dst_y >= 0 &&
          dst_y < scratch_bitmap_height &&
          src_index < static_cast<int>(tile_data.size())) {

        // Write 2x2 pixel blocks to fill the 32x32 grid space
        for (int py = 0; py < 2 && (dst_y + py) < scratch_bitmap_height; ++py) {
          for (int px = 0; px < 2 && (dst_x + px) < scratch_bitmap_width;
               ++px) {
            int dst_index = (dst_y + py) * scratch_bitmap_width + (dst_x + px);
            scratch_slot.scratch_bitmap.WriteToPixel(dst_index,
                                                     tile_data[src_index]);
          }
        }
      }
    }
  }

  scratch_slot.scratch_bitmap.set_modified(true);
  core::Renderer::Get().UpdateBitmap(&scratch_slot.scratch_bitmap);
  scratch_slot.in_use = true;
}

absl::Status OverworldEditor::SaveCurrentSelectionToScratch(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch slot");
  }

  if (ow_map_canvas_.select_rect_active() &&
      !ow_map_canvas_.selected_tiles().empty()) {
    // Calculate actual selection dimensions from overworld rectangle
    const auto& selected_points = ow_map_canvas_.selected_points();
    if (selected_points.size() >= 2) {
      const auto start = selected_points[0];
      const auto end = selected_points[1];

      // Calculate width and height in tiles
      int selection_width =
          std::abs(static_cast<int>((end.x - start.x) / 16)) + 1;
      int selection_height =
          std::abs(static_cast<int>((end.y - start.y) / 16)) + 1;

      // Update scratch space dimensions to match selection
      scratch_spaces_[slot].width = std::max(1, std::min(selection_width, 32));
      scratch_spaces_[slot].height =
          std::max(1, std::min(selection_height, 32));
      scratch_spaces_[slot].in_use = true;
      scratch_spaces_[slot].name =
          absl::StrFormat("Selection %dx%d", scratch_spaces_[slot].width,
                          scratch_spaces_[slot].height);

      // Recreate bitmap with new dimensions
      int bitmap_width = scratch_spaces_[slot].width * 16;
      int bitmap_height = scratch_spaces_[slot].height * 16;
      std::vector<uint8_t> empty_data(bitmap_width * bitmap_height, 0);
      scratch_spaces_[slot].scratch_bitmap.Create(bitmap_width, bitmap_height,
                                                  8, empty_data);
      if (all_gfx_loaded_) {
        palette_ = overworld_.current_area_palette();
        scratch_spaces_[slot].scratch_bitmap.SetPalette(palette_);
        core::Renderer::Get().RenderBitmap(
            &scratch_spaces_[slot].scratch_bitmap);
      }

      // Save selected tiles to scratch data with proper layout
      overworld_.set_current_world(current_world_);
      overworld_.set_current_map(current_map_);

      int idx = 0;
      for (int y = 0;
           y < scratch_spaces_[slot].height &&
           idx < static_cast<int>(ow_map_canvas_.selected_tiles().size());
           ++y) {
        for (int x = 0;
             x < scratch_spaces_[slot].width &&
             idx < static_cast<int>(ow_map_canvas_.selected_tiles().size());
             ++x) {
          if (idx < static_cast<int>(ow_map_canvas_.selected_tiles().size())) {
            int tile_id = overworld_.GetTileFromPosition(
                ow_map_canvas_.selected_tiles()[idx]);
            if (x < 32 && y < 32) {
              scratch_spaces_[slot].tile_data[x][y] = tile_id;
            }
            // Update the bitmap immediately
            UpdateScratchBitmapTile(x, y, tile_id, slot);
            idx++;
          }
        }
      }
    }
  } else {
    // Default single-tile scratch space
    scratch_spaces_[slot].width = 16;  // Default size
    scratch_spaces_[slot].height = 16;
    scratch_spaces_[slot].name = absl::StrFormat("Map %d Area", current_map_);
    scratch_spaces_[slot].in_use = true;
  }

  return absl::OkStatus();
}

absl::Status OverworldEditor::LoadScratchToSelection(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch slot");
  }

  if (!scratch_spaces_[slot].in_use) {
    return absl::FailedPreconditionError("Scratch slot is empty");
  }

  // Placeholder - could restore tiles to current map position
  util::logf("Loading scratch slot %d: %s", slot,
             scratch_spaces_[slot].name.c_str());

  return absl::OkStatus();
}

absl::Status OverworldEditor::ClearScratchSpace(int slot) {
  if (slot < 0 || slot >= 4) {
    return absl::InvalidArgumentError("Invalid scratch slot");
  }

  scratch_spaces_[slot].in_use = false;
  scratch_spaces_[slot].name = "Empty";

  // Clear the bitmap
  if (scratch_spaces_[slot].scratch_bitmap.is_active()) {
    auto& data = scratch_spaces_[slot].scratch_bitmap.mutable_data();
    std::fill(data.begin(), data.end(), 0);
    scratch_spaces_[slot].scratch_bitmap.set_modified(true);
    core::Renderer::Get().UpdateBitmap(&scratch_spaces_[slot].scratch_bitmap);
  }

  return absl::OkStatus();
}

// Removed DrawScratchSpaceSelection - now using canvas built-in system

}  // namespace yaze::editor