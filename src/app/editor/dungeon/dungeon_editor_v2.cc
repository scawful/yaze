// Related header
#include "dungeon_editor_v2.h"

// C system headers
#include <cstdio>

// C++ standard library headers
#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Third-party library headers
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "imgui/imgui.h"

// Project headers
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/panels/dungeon_entrance_list_panel.h"
#include "app/editor/dungeon/panels/dungeon_entrances_panel.h"
#include "app/editor/dungeon/panels/dungeon_map_panel.h"
#include "app/editor/dungeon/panels/custom_collision_panel.h"
#include "app/editor/dungeon/panels/water_fill_panel.h"
#include "app/editor/dungeon/panels/dungeon_settings_panel.h"
#include "app/editor/dungeon/panels/dungeon_workbench_panel.h"
#include "app/editor/dungeon/panels/dungeon_palette_editor_panel.h"
#include "app/editor/dungeon/panels/dungeon_room_graphics_panel.h"
#include "app/editor/dungeon/panels/dungeon_room_matrix_panel.h"
#include "app/editor/dungeon/panels/dungeon_room_selector_panel.h"
#include "app/editor/dungeon/panels/item_editor_panel.h"
#include "app/editor/dungeon/panels/minecart_track_editor_panel.h"
#include "app/editor/dungeon/panels/object_editor_panel.h"
#include "app/editor/dungeon/panels/room_tag_editor_panel.h"
#include "app/editor/dungeon/panels/sprite_editor_panel.h"
#include "app/editor/editor_manager.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/ui/toast_manager.h"
#include "app/emu/mesen/mesen_client_registry.h"
#include "app/emu/mesen/mesen_socket_client.h"
#include "app/emu/render/emulator_render_service.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/core/icons.h"
#include "core/features.h"
#include "core/project.h"
#include "rom/snes.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/dungeon_validator.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/water_fill_zone.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

namespace {

bool IsSingleBitMask(uint8_t mask) {
  return mask != 0 && (mask & (mask - 1)) == 0;
}

int MaskIndex(uint8_t mask) {
  switch (mask) {
    case 0x01:
      return 0;
    case 0x02:
      return 1;
    case 0x04:
      return 2;
    case 0x08:
      return 3;
    case 0x10:
      return 4;
    case 0x20:
      return 5;
    case 0x40:
      return 6;
    case 0x80:
      return 7;
    default:
      return -1;
  }
}

absl::Status SaveWaterFillZones(Rom* rom, std::array<zelda3::Room, 0x128>& rooms) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  bool any_dirty = false;
  for (const auto& room : rooms) {
    if (room.water_fill_dirty()) {
      any_dirty = true;
      break;
    }
  }
  if (!any_dirty) {
    return absl::OkStatus();
  }

  std::vector<zelda3::WaterFillZoneEntry> zones;
  zones.reserve(8);
  for (int room_id = 0; room_id < static_cast<int>(rooms.size()); ++room_id) {
    auto& room = rooms[room_id];
    if (!room.has_water_fill_zone()) {
      continue;
    }

    const int tile_count = room.WaterFillTileCount();
    if (tile_count <= 0) {
      continue;
    }
    if (tile_count > 255) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Water fill zone in room 0x%02X has %d tiles (max 255)",
          room_id, tile_count));
    }

    zelda3::WaterFillZoneEntry z;
    z.room_id = room_id;
    z.sram_bit_mask = room.water_fill_sram_bit_mask();
    z.fill_offsets.reserve(static_cast<size_t>(tile_count));

    const auto& map = room.water_fill_zone().tiles;
    for (size_t i = 0; i < map.size(); ++i) {
      if (map[i] != 0) {
        z.fill_offsets.push_back(static_cast<uint16_t>(i));
      }
    }
    zones.push_back(std::move(z));
  }

  if (zones.size() > 8) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Too many water fill zones: %zu (max 8 fits in $7EF411 bitfield)",
        zones.size()));
  }

  uint8_t used_masks = 0;
  int room_for_bit[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
  std::vector<zelda3::WaterFillZoneEntry*> unassigned;
  for (auto& z : zones) {
    if (z.sram_bit_mask == 0) {
      unassigned.push_back(&z);
      continue;
    }
    if (!IsSingleBitMask(z.sram_bit_mask) || MaskIndex(z.sram_bit_mask) < 0) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Invalid SRAM bit mask 0x%02X for room 0x%02X (must be one of 0x01..0x80)",
          z.sram_bit_mask, z.room_id));
    }
    const int bit = MaskIndex(z.sram_bit_mask);
    if ((used_masks & z.sram_bit_mask) != 0) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Duplicate SRAM bit mask 0x%02X used by rooms 0x%02X and 0x%02X",
          z.sram_bit_mask, room_for_bit[bit], z.room_id));
    }
    used_masks |= z.sram_bit_mask;
    room_for_bit[bit] = z.room_id;
  }

  std::sort(unassigned.begin(), unassigned.end(),
            [](const zelda3::WaterFillZoneEntry* a,
               const zelda3::WaterFillZoneEntry* b) {
              return a->room_id < b->room_id;
            });

  constexpr uint8_t kBits[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
  for (auto* z : unassigned) {
    uint8_t assigned = 0;
    for (uint8_t bit : kBits) {
      if ((used_masks & bit) == 0) {
        assigned = bit;
        break;
      }
    }
    if (assigned == 0) {
      return absl::ResourceExhaustedError(
          "No free SRAM bits left in $7EF411 for water fill zones");
    }
    z->sram_bit_mask = assigned;
    used_masks |= assigned;
    rooms[z->room_id].set_water_fill_sram_bit_mask(assigned);
  }

  RETURN_IF_ERROR(zelda3::WriteWaterFillTable(rom, zones));
  for (auto& room : rooms) {
    room.ClearWaterFillDirty();
  }

  return absl::OkStatus();
}

}  // namespace

DungeonEditorV2::~DungeonEditorV2() {
  // Clear viewer references in panels BEFORE room_viewers_ is destroyed.
  // Panels are owned by PanelManager and outlive this editor, so they need
  // to have their viewer pointers cleared to prevent dangling pointer access.
  if (object_editor_panel_) {
    object_editor_panel_->SetCanvasViewer(nullptr);
  }
  if (sprite_editor_panel_) {
    sprite_editor_panel_->SetCanvasViewer(nullptr);
  }
  if (item_editor_panel_) {
    item_editor_panel_->SetCanvasViewer(nullptr);
  }
  if (custom_collision_panel_) {
    custom_collision_panel_->SetCanvasViewer(nullptr);
    custom_collision_panel_->SetInteraction(nullptr);
  }
  if (water_fill_panel_) {
    water_fill_panel_->SetCanvasViewer(nullptr);
    water_fill_panel_->SetInteraction(nullptr);
  }
  if (dungeon_settings_panel_) {
    dungeon_settings_panel_->SetCanvasViewer(nullptr);
  }
}

void DungeonEditorV2::Initialize() {
  if (dependencies_.renderer) {
    renderer_ = dependencies_.renderer;
  }
  const bool rom_changed = dependencies_.rom && dependencies_.rom != rom_;
  if (rom_changed) {
    SetRom(dependencies_.rom);
    // The system owns ROM-backed views; ensure it matches the current ROM.
    dungeon_editor_system_.reset();
  }
  if (rom_ && !dungeon_editor_system_) {
    dungeon_editor_system_ = zelda3::CreateDungeonEditorSystem(rom_);
    if (game_data_) {
      dungeon_editor_system_->SetGameData(game_data_);
    }
  }

  // Setup docking class for room windows
  room_window_class_.DockingAllowUnclassed = true;
  room_window_class_.DockingAlwaysTabBar = true;

  if (!dependencies_.panel_manager)
    return;
  auto* panel_manager = dependencies_.panel_manager;

  // Register panels with PanelManager (no boolean flags - visibility is
  // managed entirely by PanelManager::ShowPanel/HidePanel/IsPanelVisible)
  panel_manager->RegisterPanel(
      {.card_id = "dungeon.workbench",
       .display_name = "Dungeon Workbench",
       .window_title = " Dungeon Workbench",
       .icon = ICON_MD_WORKSPACES,
       .category = "Dungeon",
       .shortcut_hint = "",
       .visibility_flag = nullptr,
       .priority = 5,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to edit dungeon rooms"});

  panel_manager->RegisterPanel(
      {.card_id = kRoomSelectorId,
       .display_name = "Room List",
       .window_title = " Room List",
       .icon = ICON_MD_LIST,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+R",
       .visibility_flag = nullptr,
       .priority = 20,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to browse dungeon rooms"});

  panel_manager->RegisterPanel(
      {.card_id = kEntranceListId,
       .display_name = "Entrance List",
       .window_title = " Entrance List",
       .icon = ICON_MD_DOOR_FRONT,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+E",
       .visibility_flag = nullptr,
       .priority = 25,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to browse dungeon entrances"});

  panel_manager->RegisterPanel(
      {.card_id = "dungeon.entrance_properties",
       .display_name = "Entrance Properties",
       .window_title = " Entrance Properties",
       .icon = ICON_MD_TUNE,
       .category = "Dungeon",
       .shortcut_hint = "",
       .visibility_flag = nullptr,
       .priority = 26,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to edit entrance properties"});

  panel_manager->RegisterPanel(
      {.card_id = kRoomMatrixId,
       .display_name = "Room Matrix",
       .window_title = " Room Matrix",
       .icon = ICON_MD_GRID_VIEW,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+M",
       .visibility_flag = nullptr,
       .priority = 30,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to view the room matrix"});

  panel_manager->RegisterPanel(
      {.card_id = kRoomGraphicsId,
       .display_name = "Room Graphics",
       .window_title = " Room Graphics",
       .icon = ICON_MD_IMAGE,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+G",
       .visibility_flag = nullptr,
       .priority = 50,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to view room graphics"});

  panel_manager->RegisterPanel(
      {.card_id = kPaletteEditorId,
       .display_name = "Palette Editor",
       .window_title = " Palette Editor",
       .icon = ICON_MD_PALETTE,
       .category = "Dungeon",
       // Avoid conflicting with the global Command Palette (Ctrl/Cmd+Shift+P).
       .shortcut_hint = "Ctrl+Shift+Alt+P",
       .visibility_flag = nullptr,
       .priority = 70,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to edit dungeon palettes"});

  panel_manager->RegisterPanel(
      {.card_id = "dungeon.room_tags",
       .display_name = "Room Tags",
       .window_title = " Room Tags",
       .icon = ICON_MD_LABEL,
       .category = "Dungeon",
       .shortcut_hint = "",
       .visibility_flag = nullptr,
       .priority = 45,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to view room tags"});

  panel_manager->RegisterPanel(
      {.card_id = "dungeon.dungeon_map",
       .display_name = "Dungeon Map",
       .window_title = " Dungeon Map",
       .icon = ICON_MD_MAP,
       .category = "Dungeon",
       .shortcut_hint = "Ctrl+Shift+D",
       .visibility_flag = nullptr,
       .priority = 32,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM to view the dungeon map"});

  // Show default panels on startup
  if (core::FeatureFlags::get().dungeon.kUseWorkbench) {
    panel_manager->ShowPanel("dungeon.workbench");
  } else {
    panel_manager->ShowPanel(kRoomSelectorId);
    panel_manager->ShowPanel(kRoomMatrixId);
  }

  // Wire intent-aware callback for double-click / context menu
  room_selector_.SetRoomSelectedWithIntentCallback(
      [this](int room_id, RoomSelectionIntent intent) {
        OnRoomSelected(room_id, intent);
      });

  // Register EditorPanel instances
  panel_manager->RegisterEditorPanel(std::make_unique<DungeonRoomSelectorPanel>(
      &room_selector_, [this](int room_id) { OnRoomSelected(room_id); }));

  panel_manager->RegisterEditorPanel(std::make_unique<DungeonEntranceListPanel>(
      &room_selector_,
      [this](int entrance_id) { OnEntranceSelected(entrance_id); }));

  {
    auto matrix_panel = std::make_unique<DungeonRoomMatrixPanel>(
        &current_room_id_, &active_rooms_,
        [this](int room_id) { OnRoomSelected(room_id); },
        [this](int old_room, int new_room) {
          SwapRoomInPanel(old_room, new_room);
        },
        &rooms_);
    matrix_panel->SetRoomIntentCallback(
        [this](int room_id, RoomSelectionIntent intent) {
          OnRoomSelected(room_id, intent);
        });
    panel_manager->RegisterEditorPanel(std::move(matrix_panel));
  }

  {
    auto dungeon_map = std::make_unique<DungeonMapPanel>(
        &current_room_id_, &active_rooms_,
        [this](int room_id) { OnRoomSelected(room_id); }, &rooms_);
    dungeon_map->SetRoomIntentCallback(
        [this](int room_id, RoomSelectionIntent intent) {
          OnRoomSelected(room_id, intent);
        });
    if (dependencies_.project) {
      dungeon_map->SetHackManifest(&dependencies_.project->hack_manifest);
    }
    panel_manager->RegisterEditorPanel(std::move(dungeon_map));
  }

  panel_manager->RegisterEditorPanel(std::make_unique<DungeonWorkbenchPanel>(
      &room_selector_, &current_room_id_, &workbench_previous_room_id_,
      &workbench_split_view_enabled_, &workbench_compare_room_id_,
      &workbench_layout_state_,
      [this](int room_id) { OnRoomSelected(room_id); },
      [this](int room_id) {
        auto status = SaveRoom(room_id);
        if (!status.ok()) {
          LOG_ERROR("DungeonEditorV2", "Save Room failed: %s",
                    status.message().data());
          if (dependencies_.toast_manager) {
            dependencies_.toast_manager->Show(
                absl::StrFormat("Save Room failed: %s", status.message()),
                ToastType::kError);
          }
          return;
        }
        if (dependencies_.toast_manager) {
          dependencies_.toast_manager->Show("Room saved", ToastType::kSuccess);
        }
      },
      [this]() { return GetWorkbenchViewer(); },
      [this]() { return GetWorkbenchCompareViewer(); },
      [this]() -> const std::deque<int>& { return recent_rooms_; },
      [this](int room_id) {
        recent_rooms_.erase(
            std::remove(recent_rooms_.begin(), recent_rooms_.end(), room_id),
            recent_rooms_.end());
      },
      [this](const std::string& id) { ShowPanel(id); }, rom_));

  panel_manager->RegisterEditorPanel(std::make_unique<DungeonEntrancesPanel>(
      &entrances_, &current_entrance_id_,
      [this](int entrance_id) { OnEntranceSelected(entrance_id); }));

  // Note: DungeonRoomGraphicsPanel and DungeonPaletteEditorPanel are registered
  // in Load() after their dependencies (renderer_, palette_editor_) are initialized
}

absl::Status DungeonEditorV2::Load() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Load object dimension table for accurate hit-testing
  auto& dim_table = zelda3::ObjectDimensionTable::Get();
  if (!dim_table.IsLoaded()) {
    RETURN_IF_ERROR(dim_table.LoadFromRom(rom_));
  }

  RETURN_IF_ERROR(room_loader_.LoadRoomEntrances(entrances_));

  if (!game_data()) {
    return absl::FailedPreconditionError("GameData not available");
  }
  auto dungeon_main_pal_group = game_data()->palette_groups.dungeon_main;
  current_palette_ = dungeon_main_pal_group[current_palette_group_id_];
  ASSIGN_OR_RETURN(current_palette_group_,
                   gfx::CreatePaletteGroupFromLargePalette(current_palette_));

  room_selector_.set_rooms(&rooms_);
  room_selector_.set_entrances(&entrances_);
  room_selector_.set_active_rooms(active_rooms_);
  room_selector_.SetRoomSelectedCallback(
      [this](int room_id) { OnRoomSelected(room_id); });
  room_selector_.SetRoomSelectedWithIntentCallback(
      [this](int room_id, RoomSelectionIntent intent) {
        OnRoomSelected(room_id, intent);
      });

  // Canvas viewers are lazily created in GetViewerForRoom

  if (!render_service_) {
    render_service_ =
        std::make_unique<emu::render::EmulatorRenderService>(rom_);
    auto status = render_service_->Initialize();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to initialize render service: %s",
                status.message().data());
    }
  }

  if (game_data()) {
    gfx::PaletteManager::Get().Initialize(game_data());
  } else {
    gfx::PaletteManager::Get().Initialize(rom_);
  }

  palette_editor_.Initialize(game_data());

  // Register panels that depend on initialized state (renderer, palette_editor_)
  if (dependencies_.panel_manager) {
    auto graphics_panel = std::make_unique<DungeonRoomGraphicsPanel>(
        &current_room_id_, &rooms_, renderer_);
    room_graphics_panel_ = graphics_panel.get();
    dependencies_.panel_manager->RegisterEditorPanel(std::move(graphics_panel));
    dependencies_.panel_manager->RegisterEditorPanel(
        std::make_unique<DungeonPaletteEditorPanel>(&palette_editor_));
  }

  dungeon_editor_system_ = std::make_unique<zelda3::DungeonEditorSystem>(rom_);
  (void)dungeon_editor_system_->Initialize();
  dungeon_editor_system_->SetCurrentRoom(current_room_id_);

  // Initialize unified object editor panel
  // Note: Initially passing nullptr for viewer, will be set on selection
  auto object_editor = std::make_unique<ObjectEditorPanel>(
      renderer_, rom_, nullptr, dungeon_editor_system_->GetObjectEditor());

  // Wire up object change callback to trigger room re-rendering
  dungeon_editor_system_->GetObjectEditor()->SetObjectChangedCallback(
      [this](size_t /*object_index*/, const zelda3::RoomObject& /*object*/) {
        if (current_room_id_ >= 0 && current_room_id_ < (int)rooms_.size()) {
          rooms_[current_room_id_].RenderRoomGraphics();
        }
      });

  // Set rooms and initial palette group for correct preview rendering
  object_editor->SetRooms(&rooms_);
  object_editor->SetCurrentPaletteGroup(current_palette_group_);

  // Keep raw pointer for later access
  object_editor_panel_ = object_editor.get();

  // Propagate game_data to the object editor panel if available
  if (game_data()) {
    object_editor_panel_->SetGameData(game_data());
  }
  if (dependencies_.project) {
    object_editor_panel_->object_selector().SetCustomObjectsFolder(
        dependencies_.project->GetAbsolutePath(
            dependencies_.project->custom_objects_folder));
  }

  // Register the ObjectEditorPanel directly (it inherits from EditorPanel)
  // Panel manager takes ownership
  if (dependencies_.panel_manager) {
    dependencies_.panel_manager->RegisterEditorPanel(std::move(object_editor));

    // Register sprite and item editor panels with canvas viewer = nullptr
    // They will get the viewer reference in OnRoomSelected when a room is selected
    auto sprite_panel = std::make_unique<SpriteEditorPanel>(&current_room_id_,
                                                            &rooms_, nullptr);
    sprite_editor_panel_ = sprite_panel.get();
    dependencies_.panel_manager->RegisterEditorPanel(std::move(sprite_panel));

    auto item_panel =
        std::make_unique<ItemEditorPanel>(&current_room_id_, &rooms_, nullptr);
    item_editor_panel_ = item_panel.get();
    dependencies_.panel_manager->RegisterEditorPanel(std::move(item_panel));

    auto collision_panel = std::make_unique<CustomCollisionPanel>(nullptr, nullptr); // Placeholder, will be set in OnRoomSelected
    custom_collision_panel_ = collision_panel.get();
    dependencies_.panel_manager->RegisterEditorPanel(std::move(collision_panel));

    auto water_fill_panel =
        std::make_unique<WaterFillPanel>(nullptr, nullptr);  // Placeholder
    water_fill_panel_ = water_fill_panel.get();
    dependencies_.panel_manager->RegisterEditorPanel(std::move(water_fill_panel));

    auto settings_panel = std::make_unique<DungeonSettingsPanel>(nullptr);
    settings_panel->SetSaveRoomCallback([this](int id) { SaveRoom(id); });
    settings_panel->SetSaveAllRoomsCallback([this]() { SaveAllRooms(); });
    settings_panel->SetCurrentRoomId(&current_room_id_);
    dungeon_settings_panel_ = settings_panel.get();
    dependencies_.panel_manager->RegisterEditorPanel(std::move(settings_panel));

    // Room Tag Editor Panel
    {
      auto room_tag_panel = std::make_unique<RoomTagEditorPanel>();
      room_tag_panel->SetProject(dependencies_.project);
      room_tag_panel->SetRooms(&rooms_);
      room_tag_panel->SetCurrentRoomId(current_room_id_);
      room_tag_editor_panel_ = room_tag_panel.get();
      dependencies_.panel_manager->RegisterEditorPanel(
          std::move(room_tag_panel));
    }

    // Feature Flag: Custom Objects / Minecart Tracks
    if (core::FeatureFlags::get().kEnableCustomObjects) {
      if (!minecart_track_editor_panel_) {
        auto minecart_panel = std::make_unique<MinecartTrackEditorPanel>();
        minecart_track_editor_panel_ = minecart_panel.get();
        dependencies_.panel_manager->RegisterEditorPanel(
            std::move(minecart_panel));
      }

      if (dependencies_.project) {
        // Update project root for track editor
        if (minecart_track_editor_panel_) {
          minecart_track_editor_panel_->SetProjectRoot(
              dependencies_.project->code_folder);
          minecart_track_editor_panel_->SetRooms(&rooms_);
          minecart_track_editor_panel_->SetRom(rom_);
          minecart_track_editor_panel_->SetProject(dependencies_.project);
          minecart_track_editor_panel_->SetRoomNavigationCallback(
              [this](int room_id) { OnRoomSelected(room_id); });
        }

        // Initialize custom object manager with project-configured path
        if (!dependencies_.project->custom_objects_folder.empty()) {
          zelda3::CustomObjectManager::Get().Initialize(
              dependencies_.project->GetAbsolutePath(
                  dependencies_.project->custom_objects_folder));
        }
      }
    }
  } else {
    owned_object_editor_panel_ = std::move(object_editor);
  }

  palette_editor_.SetOnPaletteChanged([this](int /*palette_id*/) {
    for (int i = 0; i < active_rooms_.Size; i++) {
      int room_id = active_rooms_[i];
      if (room_id >= 0 && room_id < (int)rooms_.size()) {
        rooms_[room_id].RenderRoomGraphics();
      }
    }
  });

  // Oracle of Secrets: load editor-authored water fill zones (best-effort).
  {
    for (auto& room : rooms_) {
      room.ClearWaterFillZone();
      room.ClearWaterFillDirty();
    }

    bool legacy_imported = false;
    std::vector<zelda3::WaterFillZoneEntry> zones;

    auto zones_or = zelda3::LoadWaterFillTable(rom_);
    if (zones_or.ok()) {
      zones = std::move(zones_or.value());
    } else {
      LOG_WARN("DungeonEditorV2", "WaterFillTable parse failed: %s",
               zones_or.status().message().data());
      if (dependencies_.toast_manager) {
        dependencies_.toast_manager->Show(
            absl::StrFormat("WaterFill table parse failed: %s",
                            zones_or.status().message()),
            ToastType::kWarning);
      }
    }

    if (zones.empty()) {
      std::string sym_path;
      if (dependencies_.project &&
          !dependencies_.project->symbols_filename.empty()) {
        sym_path = dependencies_.project->GetAbsolutePath(
            dependencies_.project->symbols_filename);
      }
      auto legacy_or = zelda3::LoadLegacyWaterGateZones(rom_, sym_path);
      if (legacy_or.ok()) {
        zones = std::move(legacy_or.value());
        legacy_imported = !zones.empty();
      } else {
        LOG_WARN("DungeonEditorV2", "Legacy water gate import failed: %s",
                 legacy_or.status().message().data());
      }
    }

    for (const auto& z : zones) {
      if (z.room_id < 0 || z.room_id >= static_cast<int>(rooms_.size())) {
        continue;
      }
      auto& room = rooms_[z.room_id];
      room.set_water_fill_sram_bit_mask(z.sram_bit_mask);
      for (uint16_t off : z.fill_offsets) {
        const int x = static_cast<int>(off % 64);
        const int y = static_cast<int>(off / 64);
        room.SetWaterFillTile(x, y, true);
      }

      if (!legacy_imported) {
        room.ClearWaterFillDirty();
      }
    }

    if (legacy_imported && dependencies_.toast_manager) {
      dependencies_.toast_manager->Show(
          "Imported legacy water gate zones (save to write new table)",
          ToastType::kInfo);
    }
  }

  is_loaded_ = true;
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Update() {
  const auto& theme = AgentUI::GetTheme();
  if (room_window_class_.ClassId == 0) {
    room_window_class_.ClassId = ImGui::GetID("DungeonRoomClass");
  }

  if (!is_loaded_) {
    gui::PanelWindow loading_card("Dungeon Editor Loading", ICON_MD_CASTLE);
    loading_card.SetDefaultSize(400, 200);
    if (loading_card.Begin()) {
      ImGui::TextColored(theme.text_secondary_gray, "Loading dungeon data...");
      ImGui::TextWrapped(
          "Independent editor cards will appear once ROM data is loaded.");
    }
    loading_card.End();
    return absl::OkStatus();
  }

  if (!core::FeatureFlags::get().dungeon.kUseWorkbench) {
    DrawRoomPanels();
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    // Delegate delete to current room viewer
    if (auto* viewer = GetViewerForRoom(current_room_id_)) {
      viewer->DeleteSelectedObjects();
    }
  }

  // Keyboard Shortcuts (only if not typing in a text field)
  if (!ImGui::GetIO().WantTextInput) {
    // Room Cycling (Ctrl+Tab)
    if (ImGui::IsKeyPressed(ImGuiKey_Tab) && ImGui::GetIO().KeyCtrl) {
      if (core::FeatureFlags::get().dungeon.kUseWorkbench) {
        if (recent_rooms_.size() > 1) {
          int current_idx = -1;
          for (int i = 0; i < static_cast<int>(recent_rooms_.size()); ++i) {
            if (recent_rooms_[i] == current_room_id_) {
              current_idx = i;
              break;
            }
          }
          if (current_idx != -1) {
            int next_idx;
            if (ImGui::GetIO().KeyShift) {
              next_idx = (current_idx + 1) % static_cast<int>(recent_rooms_.size());
            } else {
              next_idx =
                  (current_idx - 1 + static_cast<int>(recent_rooms_.size())) %
                  static_cast<int>(recent_rooms_.size());
            }
            OnRoomSelected(recent_rooms_[next_idx]);
          }
        }
      } else if (active_rooms_.size() > 1) {
        int current_idx = -1;
        for (int i = 0; i < active_rooms_.size(); ++i) {
          if (active_rooms_[i] == current_room_id_) {
            current_idx = i;
            break;
          }
        }

        if (current_idx != -1) {
          int next_idx;
          if (ImGui::GetIO().KeyShift) {
            next_idx =
                (current_idx - 1 + active_rooms_.size()) % active_rooms_.size();
          } else {
            next_idx = (current_idx + 1) % active_rooms_.size();
          }
          OnRoomSelected(active_rooms_[next_idx]);
        }
      }
    }

    // Adjacent Room Navigation (Ctrl+Arrows)
    if (ImGui::GetIO().KeyCtrl) {
      int next_room = -1;
      const int kCols = 16;
      const int kTotalRooms = 0x128;

      if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        if (current_room_id_ >= kCols)
          next_room = current_room_id_ - kCols;
      } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        if (current_room_id_ < kTotalRooms - kCols)
          next_room = current_room_id_ + kCols;
      } else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        if (current_room_id_ % kCols > 0)
          next_room = current_room_id_ - 1;
      } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        if (current_room_id_ % kCols < kCols - 1 &&
            current_room_id_ < kTotalRooms - 1)
          next_room = current_room_id_ + 1;
      }

      if (next_room != -1) {
        if (core::FeatureFlags::get().dungeon.kUseWorkbench) {
          OnRoomSelected(next_room, /*request_focus=*/false);
        } else {
          SwapRoomInPanel(current_room_id_, next_room);
        }
      }
    }
  }

  // Process any pending room swaps after all drawing is complete
  // This prevents ImGui state corruption from modifying collections mid-frame
  ProcessPendingSwap();

  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Save() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  const auto& flags = core::FeatureFlags::get().dungeon;

  if (flags.kSavePalettes &&
      gfx::PaletteManager::Get().HasUnsavedChanges()) {
    auto status = gfx::PaletteManager::Get().SaveAllToRom();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save palette changes: %s",
                status.message().data());
      return status;
    }
    LOG_INFO("DungeonEditorV2", "Saved %zu modified colors to ROM",
             gfx::PaletteManager::Get().GetModifiedColorCount());
  }

  if (flags.kSaveObjects || flags.kSaveSprites || flags.kSaveRoomHeaders) {
    for (int room_id = 0; room_id < static_cast<int>(rooms_.size()); ++room_id) {
      auto status = SaveRoomData(room_id);
      if (!status.ok()) {
        return status;
      }
    }
  }

  if (flags.kSaveTorches) {
    auto status = zelda3::SaveAllTorches(rom_, rooms_);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save torches: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSavePits) {
    auto status = zelda3::SaveAllPits(rom_);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save pits: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveBlocks) {
    auto status = zelda3::SaveAllBlocks(rom_);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save blocks: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveCollision) {
    auto status = zelda3::SaveAllCollision(rom_, absl::MakeSpan(rooms_));
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save collision: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveWaterFillZones) {
    auto status = SaveWaterFillZones(rom_, rooms_);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save water fill zones: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveChests) {
    auto status = zelda3::SaveAllChests(rom_, rooms_);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save chests: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSavePotItems) {
    auto status = zelda3::SaveAllPotItems(rom_, rooms_);
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save pot items: %s",
                status.message().data());
      return status;
    }
  }

  return absl::OkStatus();
}

std::vector<std::pair<uint32_t, uint32_t>>
DungeonEditorV2::CollectWriteRanges() const {
  std::vector<std::pair<uint32_t, uint32_t>> ranges;

  if (!rom_ || !rom_->is_loaded()) {
    return ranges;
  }

  const auto& flags = core::FeatureFlags::get().dungeon;
  const auto& rom_data = rom_->vector();

  // Oracle of Secrets: the water fill table lives in a reserved tail region.
  // Include it in write-range reporting whenever we have dirty water fill data,
  // even if no rooms are currently loaded (SaveWaterFillZones() is room-indexed
  // and independent of room loading state).
  if (flags.kSaveWaterFillZones &&
      zelda3::kWaterFillTableEnd <= static_cast<int>(rom_data.size())) {
    for (const auto& room : rooms_) {
      if (room.water_fill_dirty()) {
        ranges.emplace_back(zelda3::kWaterFillTableStart,
                            zelda3::kWaterFillTableEnd);
        break;
      }
    }
  }

  // Custom collision writes update the pointer table and append blobs into the
  // expanded collision region. SaveAllCollision() is room-indexed, so include
  // these ranges whenever any room has dirty custom collision data.
  if (flags.kSaveCollision) {
    const int ptrs_size = zelda3::kNumberOfRooms * 3;
    const bool has_ptr_table =
        (zelda3::kCustomCollisionRoomPointers + ptrs_size <=
         static_cast<int>(rom_data.size()));
    const bool has_data_region =
        (zelda3::kCustomCollisionDataSoftEnd <=
         static_cast<int>(rom_data.size()));
    if (has_ptr_table && has_data_region) {
      for (const auto& room : rooms_) {
        if (room.custom_collision_dirty()) {
          ranges.emplace_back(zelda3::kCustomCollisionRoomPointers,
                              zelda3::kCustomCollisionRoomPointers + ptrs_size);
          ranges.emplace_back(zelda3::kCustomCollisionDataPosition,
                              zelda3::kCustomCollisionDataSoftEnd);
          break;
        }
      }
    }
  }

  for (const auto& room : rooms_) {
    if (!room.IsLoaded()) {
      continue;
    }
    int room_id = room.id();

    // Header range
    if (flags.kSaveRoomHeaders) {
      if (zelda3::kRoomHeaderPointer + 2 < static_cast<int>(rom_data.size())) {
        int header_ptr_table = (rom_data[zelda3::kRoomHeaderPointer + 2] << 16) |
                               (rom_data[zelda3::kRoomHeaderPointer + 1] << 8) |
                               rom_data[zelda3::kRoomHeaderPointer];
        header_ptr_table = yaze::SnesToPc(header_ptr_table);
        int table_offset = header_ptr_table + (room_id * 2);

        if (table_offset + 1 < static_cast<int>(rom_data.size())) {
          int address = (rom_data[zelda3::kRoomHeaderPointerBank] << 16) |
                        (rom_data[table_offset + 1] << 8) |
                        rom_data[table_offset];
          int header_location = yaze::SnesToPc(address);
          ranges.emplace_back(header_location, header_location + 14);
        }
      }
    }

    // Object range
    if (flags.kSaveObjects) {
      if (zelda3::kRoomObjectPointer + 2 < static_cast<int>(rom_data.size())) {
        int obj_ptr_table = (rom_data[zelda3::kRoomObjectPointer + 2] << 16) |
                            (rom_data[zelda3::kRoomObjectPointer + 1] << 8) |
                            rom_data[zelda3::kRoomObjectPointer];
        obj_ptr_table = yaze::SnesToPc(obj_ptr_table);
        int entry_offset = obj_ptr_table + (room_id * 3);

        if (entry_offset + 2 < static_cast<int>(rom_data.size())) {
          int tile_addr = (rom_data[entry_offset + 2] << 16) |
                          (rom_data[entry_offset + 1] << 8) |
                          rom_data[entry_offset];
          int objects_location = yaze::SnesToPc(tile_addr);

          auto encoded = room.EncodeObjects();
          ranges.emplace_back(objects_location,
                              objects_location + encoded.size() + 2);
        }
      }
    }
  }

  return ranges;
}

absl::Status DungeonEditorV2::SaveRoom(int room_id) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  const auto& flags = core::FeatureFlags::get().dungeon;
  if (flags.kSavePalettes &&
      gfx::PaletteManager::Get().HasUnsavedChanges()) {
    auto status = gfx::PaletteManager::Get().SaveAllToRom();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save palette changes: %s",
                status.message().data());
      return status;
    }
  }
  if (flags.kSaveObjects || flags.kSaveSprites || flags.kSaveRoomHeaders) {
    RETURN_IF_ERROR(SaveRoomData(room_id));
  }

  if (flags.kSaveTorches) {
    RETURN_IF_ERROR(zelda3::SaveAllTorches(rom_, rooms_));
  }
  if (flags.kSavePits) {
    RETURN_IF_ERROR(zelda3::SaveAllPits(rom_));
  }
  if (flags.kSaveBlocks) {
    RETURN_IF_ERROR(zelda3::SaveAllBlocks(rom_));
  }
  if (flags.kSaveCollision) {
    RETURN_IF_ERROR(zelda3::SaveAllCollision(rom_, absl::MakeSpan(rooms_)));
  }
  if (flags.kSaveWaterFillZones) {
    RETURN_IF_ERROR(SaveWaterFillZones(rom_, rooms_));
  }
  if (flags.kSaveChests) {
    RETURN_IF_ERROR(zelda3::SaveAllChests(rom_, rooms_));
  }
  if (flags.kSavePotItems) {
    RETURN_IF_ERROR(zelda3::SaveAllPotItems(rom_, rooms_));
  }

  return absl::OkStatus();
}

int DungeonEditorV2::LoadedRoomCount() const {
  int count = 0;
  for (const auto& room : rooms_) {
    if (room.IsLoaded()) {
      ++count;
    }
  }
  return count;
}

absl::Status DungeonEditorV2::SaveRoomData(int room_id) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    return absl::InvalidArgumentError("Invalid room ID");
  }

  auto& room = rooms_[room_id];
  if (!room.IsLoaded()) {
    return absl::OkStatus();
  }

  // ROM safety: validate loaded room content before writing any bytes.
  {
    zelda3::DungeonValidator validator;
    const auto result = validator.ValidateRoom(room);
    for (const auto& w : result.warnings) {
      LOG_WARN("DungeonEditorV2", "Room 0x%03X validation warning: %s", room_id,
               w.c_str());
    }
    if (!result.is_valid) {
      for (const auto& e : result.errors) {
        LOG_ERROR("DungeonEditorV2", "Room 0x%03X validation error: %s",
                  room_id, e.c_str());
      }
      if (dependencies_.toast_manager) {
        dependencies_.toast_manager->Show(
            absl::StrFormat(
                "Save blocked: room 0x%03X failed validation (%zu error(s))",
                room_id, result.errors.size()),
            ToastType::kError);
      }
      return absl::FailedPreconditionError(
          absl::StrFormat("Room 0x%03X failed validation", room_id));
    }
  }

  const auto& flags = core::FeatureFlags::get().dungeon;

  // HACK MANIFEST VALIDATION
  if (dependencies_.project && dependencies_.project->hack_manifest.loaded()) {
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    const auto& manifest = dependencies_.project->hack_manifest;
    const auto& rom_data = rom_->vector();

    // 1. Validate Header Range
    if (flags.kSaveRoomHeaders) {
      if (zelda3::kRoomHeaderPointer + 2 < static_cast<int>(rom_data.size())) {
        int header_ptr_table = (rom_data[zelda3::kRoomHeaderPointer + 2] << 16) |
                               (rom_data[zelda3::kRoomHeaderPointer + 1] << 8) |
                               rom_data[zelda3::kRoomHeaderPointer];
        header_ptr_table = yaze::SnesToPc(header_ptr_table);
        int table_offset = header_ptr_table + (room_id * 2);

        if (table_offset + 1 < static_cast<int>(rom_data.size())) {
          int address = (rom_data[zelda3::kRoomHeaderPointerBank] << 16) |
                        (rom_data[table_offset + 1] << 8) |
                        rom_data[table_offset];
          int header_location = yaze::SnesToPc(address);
          ranges.emplace_back(header_location, header_location + 14);
        }
      }
    }

    // 2. Validate Object Range
    if (flags.kSaveObjects) {
      if (zelda3::kRoomObjectPointer + 2 < static_cast<int>(rom_data.size())) {
        int obj_ptr_table = (rom_data[zelda3::kRoomObjectPointer + 2] << 16) |
                            (rom_data[zelda3::kRoomObjectPointer + 1] << 8) |
                            rom_data[zelda3::kRoomObjectPointer];
        obj_ptr_table = yaze::SnesToPc(obj_ptr_table);
        int entry_offset = obj_ptr_table + (room_id * 3);

        if (entry_offset + 2 < static_cast<int>(rom_data.size())) {
          int tile_addr = (rom_data[entry_offset + 2] << 16) |
                          (rom_data[entry_offset + 1] << 8) |
                          rom_data[entry_offset];
          int objects_location = yaze::SnesToPc(tile_addr);

          // Estimate size based on current encoding
          // Note: we check the *target* location (where we will write)
          // The EncodeObjects() size is what we *will* write.
          // We add 2 bytes for the size/header that SaveObjects writes.
          auto encoded = room.EncodeObjects();
          ranges.emplace_back(objects_location,
                              objects_location + encoded.size() + 2);
        }
      }
    }

    // `ranges` are PC offsets (ROM file offsets). The hack manifest is in SNES
    // address space (LoROM), so convert before analysis.
    auto conflicts = manifest.AnalyzePcWriteRanges(ranges);
    if (!conflicts.empty()) {
      const auto write_policy = dependencies_.project->rom_metadata.write_policy;
      std::string error_msg =
          absl::StrFormat("Hack manifest write conflicts while saving room 0x%03X:\n\n",
                          room_id);
      for (const auto& conflict : conflicts) {
        absl::StrAppend(
            &error_msg,
            absl::StrFormat("- Address 0x%06X is %s", conflict.address,
                            core::AddressOwnershipToString(conflict.ownership)));
        if (!conflict.module.empty()) {
          absl::StrAppend(&error_msg, " (Module: ", conflict.module, ")");
        }
        absl::StrAppend(&error_msg, "\n");
      }

      if (write_policy == project::RomWritePolicy::kAllow) {
        LOG_DEBUG("DungeonEditorV2", "%s", error_msg.c_str());
      } else {
        LOG_WARN("DungeonEditorV2", "%s", error_msg.c_str());
      }

      if (dependencies_.toast_manager &&
          write_policy == project::RomWritePolicy::kWarn) {
        dependencies_.toast_manager->Show(
            "Save warning: write conflict with hack manifest (see log)",
            ToastType::kWarning);
      }

      if (write_policy == project::RomWritePolicy::kBlock) {
        if (dependencies_.toast_manager) {
          dependencies_.toast_manager->Show(
              "Save blocked: write conflict with hack manifest (see log)",
              ToastType::kError);
        }
        return absl::PermissionDeniedError("Write conflict with Hack Manifest");
      }
    }
  }

  if (flags.kSaveObjects) {
    auto status = room.SaveObjects();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save room objects: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveSprites) {
    auto status = room.SaveSprites();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save room sprites: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveRoomHeaders) {
    auto status = room.SaveRoomHeader();
    if (!status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save room header: %s",
                status.message().data());
      return status;
    }
  }

  if (flags.kSaveObjects && dungeon_editor_system_) {
    auto sys_status = dungeon_editor_system_->SaveRoom(room.id());
    if (!sys_status.ok()) {
      LOG_ERROR("DungeonEditorV2", "Failed to save room system data: %s",
                sys_status.message().data());
    }
  }

  return absl::OkStatus();
}

void DungeonEditorV2::DrawRoomPanels() {
  for (int i = 0; i < active_rooms_.Size; i++) {
    int room_id = active_rooms_[i];
    std::string card_id = absl::StrFormat("dungeon.room_%d", room_id);
    bool panel_visible = true;
    if (dependencies_.panel_manager) {
      panel_visible = dependencies_.panel_manager->IsPanelVisible(card_id);
    }

    if (!panel_visible) {
      dependencies_.panel_manager->UnregisterPanel(card_id);
      room_cards_.erase(room_id);
      active_rooms_.erase(active_rooms_.Data + i);
      ReleaseRoomPanelSlotId(room_id);
      // Clean up viewer
      room_viewers_.erase(room_id);
      i--;
      continue;
    }

    bool is_pinned = dependencies_.panel_manager &&
                     dependencies_.panel_manager->IsPanelPinned(card_id);
    std::string active_category =
        dependencies_.panel_manager
            ? dependencies_.panel_manager->GetActiveCategory()
            : "";

    if (active_category != "Dungeon" && !is_pinned) {
      continue;
    }

    // Ensure room card exists (should have been created by ShowRoomPanel/ShowPanel)
    if (room_cards_.find(room_id) == room_cards_.end()) {
        ShowRoomPanel(room_id);
    }

    auto& room_card = room_cards_[room_id];
    bool open = true;

    ImGui::SetNextWindowClass(&room_window_class_);
    if (room_dock_id_ == 0) {
      room_dock_id_ = ImGui::GetID("DungeonRoomDock");
    }
    ImGui::SetNextWindowDockID(room_dock_id_, ImGuiCond_FirstUseEver);

    if (room_card->Begin(&open)) {
      if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        OnRoomSelected(room_id, /*request_focus=*/false);
      }
      DrawRoomTab(room_id);
    }
    room_card->End();

    if (!open) {
      if (dependencies_.panel_manager) {
        dependencies_.panel_manager->UnregisterPanel(card_id);
      }

      room_cards_.erase(room_id);
      active_rooms_.erase(active_rooms_.Data + i);
      room_viewers_.erase(room_id);
      ReleaseRoomPanelSlotId(room_id);
      i--;
    }
  }
}

int DungeonEditorV2::GetOrCreateRoomPanelSlotId(int room_id) {
  if (auto it = room_panel_slot_ids_.find(room_id);
      it != room_panel_slot_ids_.end()) {
    return it->second;
  }
  const int slot_id = next_room_panel_slot_id_++;
  room_panel_slot_ids_[room_id] = slot_id;
  return slot_id;
}

void DungeonEditorV2::ReleaseRoomPanelSlotId(int room_id) {
  room_panel_slot_ids_.erase(room_id);
}

void DungeonEditorV2::DrawRoomTab(int room_id) {
  const auto& theme = AgentUI::GetTheme();
  if (room_id < 0 || room_id >= 0x128) {
    ImGui::Text("Invalid room ID: %d", room_id);
    return;
  }

  auto& room = rooms_[room_id];

  if (!room.IsLoaded()) {
    auto status = room_loader_.LoadRoom(room_id, room);
    if (!status.ok()) {
      ImGui::TextColored(theme.text_error_red, "Failed to load room: %s",
                         status.message().data());
      return;
    }

    if (dungeon_editor_system_) {
      auto sys_status = dungeon_editor_system_->ReloadRoom(room_id);
      if (!sys_status.ok()) {
        LOG_ERROR("DungeonEditorV2", "Failed to load system data: %s",
                  sys_status.message().data());
      }
    }
  }

  if (room.IsLoaded()) {
    bool needs_render = false;

    // Chronological Step 1: Load Room Data from ROM
    // This reads the 14-byte room header (blockset, palette, effect, tags)
    // Reference: kRoomHeaderPointer (0xB5DD)
    if (room.blocks().empty()) {
      room.LoadRoomGraphics(room.blockset);
      needs_render = true;
      LOG_DEBUG("[DungeonEditorV2]", "Loaded room %d graphics from ROM",
                room_id);
    }

    // Chronological Step 2: Load Objects from ROM
    // This reads the variable-length object stream (subtype 1, 2, 3 objects)
    // Reference: kRoomObjectPointer (0x874C)
    // CRITICAL: This step decodes floor1/floor2 bytes which dictate the floor
    // pattern
    if (room.GetTileObjects().empty()) {
      room.LoadObjects();
      needs_render = true;
      LOG_DEBUG("[DungeonEditorV2]", "Loaded room %d objects from ROM",
                room_id);
    }

    // Chronological Step 3: Render Graphics to Bitmaps
    // This executes the draw routines (bank_01.asm logic) to populate BG1/BG2
    // buffers Sequence:
    // 1. Draw Floor (from floor1/floor2)
    // 2. Draw Layout (walls/floors from object list)
    // 3. Draw Objects (subtypes 1, 2, 3)
    auto& bg1_bitmap = room.bg1_buffer().bitmap();
    if (needs_render || !bg1_bitmap.is_active() || bg1_bitmap.width() == 0) {
      room.RenderRoomGraphics();
      LOG_DEBUG("[DungeonEditorV2]", "Rendered room %d to bitmaps", room_id);
    }
  }

  if (room.IsLoaded()) {
    ImGui::TextColored(theme.text_success_green, ICON_MD_CHECK " Loaded");
  } else {
    ImGui::TextColored(theme.text_error_red, ICON_MD_PENDING " Not Loaded");
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Objects: %zu", room.GetTileObjects().size());

  // Warp to Room button — sends room ID to running Mesen2 emulator
  ImGui::SameLine();
  {
    auto client = emu::mesen::MesenClientRegistry::GetClient();
    bool connected = client && client->IsConnected();
    if (!connected) {
      ImGui::BeginDisabled();
    }
    std::string warp_label =
        absl::StrFormat(ICON_MD_ROCKET_LAUNCH " Warp##warp_%03X", room_id);
    if (ImGui::SmallButton(warp_label.c_str())) {
      auto status = client->WriteWord(0x7E00A0, static_cast<uint16_t>(room_id));
      if (status.ok()) {
        if (dependencies_.toast_manager) {
          dependencies_.toast_manager->Show(
              absl::StrFormat("Warped to room 0x%03X", room_id),
              ToastType::kSuccess);
        }
      } else {
        if (dependencies_.toast_manager) {
          dependencies_.toast_manager->Show(
              absl::StrFormat("Warp failed: %s", status.message()),
              ToastType::kError);
        }
      }
    }
    if (!connected) {
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Connect to Mesen2 first");
      }
    } else {
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Warp to room 0x%03X in Mesen2", room_id);
      }
    }
  }

  ImGui::Separator();

  // Use per-room viewer
  if (auto* viewer = GetViewerForRoom(room_id)) {
    viewer->DrawDungeonCanvas(room_id);
  }
}

void DungeonEditorV2::OnRoomSelected(int room_id, RoomSelectionIntent intent) {
  switch (intent) {
    case RoomSelectionIntent::kFocusInWorkbench:
      OnRoomSelected(room_id, /*request_focus=*/true);
      break;
    case RoomSelectionIntent::kOpenStandalone: {
      // Force standalone panel even in workbench mode:
      // Run the common state update, then skip the workbench early-return
      // by going directly to the per-room panel path.
      if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
        return;
      }
      // Update shared state (same as OnRoomSelected with request_focus=true)
      OnRoomSelected(room_id, /*request_focus=*/false);
      // Now force-open a standalone panel for this room
      ShowRoomPanel(room_id);
      break;
      break;
    }
    case RoomSelectionIntent::kPreview:
      OnRoomSelected(room_id, /*request_focus=*/false);
      break;
  }
}

void DungeonEditorV2::OnRoomSelected(int room_id, bool request_focus) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    LOG_WARN("DungeonEditorV2", "Ignoring invalid room selection: %d", room_id);
    return;
  }
  if (room_id != current_room_id_) {
    workbench_previous_room_id_ = current_room_id_;
  }
  current_room_id_ = room_id;
  room_selector_.set_current_room_id(static_cast<uint16_t>(room_id));

  // Track recent rooms (remove if already present, add to front)
  recent_rooms_.erase(
      std::remove(recent_rooms_.begin(), recent_rooms_.end(), room_id),
      recent_rooms_.end());
  recent_rooms_.push_front(room_id);
  if (recent_rooms_.size() > kMaxRecentRooms) {
    recent_rooms_.pop_back();
  }

  if (dungeon_editor_system_) {
    dungeon_editor_system_->SetExternalRoom(&rooms_[room_id]);
  }

  // Update all sub-panels (Object Editor, Sprite Editor, etc.)
  SyncPanelsToRoom(room_id);

  // Sync palette with current room (must happen before early return for focus changes)

  // Sync palette with current room (must happen before early return for focus changes)
  if (room_id >= 0 && room_id < (int)rooms_.size()) {
    auto& room = rooms_[room_id];
    if (!room.IsLoaded()) {
      room_loader_.LoadRoom(room_id, room);
    }

    if (room.IsLoaded()) {
      current_palette_id_ = room.palette;
      palette_editor_.SetCurrentPaletteId(current_palette_id_);

      // Update viewer and object editor palette
      if (auto* viewer = GetViewerForRoom(room_id)) {
        viewer->SetCurrentPaletteId(current_palette_id_);

        if (game_data()) {
          auto dungeon_main_pal_group =
              game_data()->palette_groups.dungeon_main;
          if (current_palette_id_ < (int)dungeon_main_pal_group.size()) {
            current_palette_ = dungeon_main_pal_group[current_palette_id_];
            auto result =
                gfx::CreatePaletteGroupFromLargePalette(current_palette_);
            if (result.ok()) {
              current_palette_group_ = result.value();
              viewer->SetCurrentPaletteGroup(current_palette_group_);
              if (object_editor_panel_) {
                object_editor_panel_->SetCurrentPaletteGroup(
                    current_palette_group_);
              }
              // Sync palette to graphics panel for proper sheet coloring
              if (room_graphics_panel_) {
                room_graphics_panel_->SetCurrentPaletteGroup(
                    current_palette_group_);
              }
            }
          }
        }
      }
    }
  }

  // Workbench mode uses a single stable window and does not spawn per-room
  // panels. Keep selection + panels in sync and return.
  if (core::FeatureFlags::get().dungeon.kUseWorkbench) {
    if (dependencies_.panel_manager && request_focus) {
      // Only force-show if it's already visible or if it's the first initialization
      // This avoids obtrusive behavior when the user explicitly closed it.
      if (dependencies_.panel_manager->IsPanelVisible("dungeon.workbench")) {
        dependencies_.panel_manager->ShowPanel("dungeon.workbench");
      }
    }
    return;
  }

  // Check if room is already open
  for (int i = 0; i < active_rooms_.Size; i++) {
    if (active_rooms_[i] == room_id) {
      // Always ensure panel is visible, even if already in active_rooms_
      if (dependencies_.panel_manager) {
        std::string card_id = absl::StrFormat("dungeon.room_%d", room_id);
        dependencies_.panel_manager->ShowPanel(card_id);
      }
      if (request_focus) {
        FocusRoom(room_id);
      }
      return;
    }
  }

  active_rooms_.push_back(room_id);
  room_selector_.set_active_rooms(active_rooms_);

  if (dependencies_.panel_manager) {
    // Use unified ResourceLabelProvider for room names
    std::string room_name = absl::StrFormat(
        "[%03X] %s", room_id, zelda3::GetRoomLabel(room_id).c_str());

    std::string base_card_id = absl::StrFormat("dungeon.room_%d", room_id);

    dependencies_.panel_manager->RegisterPanel(
        {.card_id = base_card_id,
         .display_name = room_name,
         .window_title = ICON_MD_GRID_ON " " + room_name,
         .icon = ICON_MD_GRID_ON,
         .category = "Dungeon",
         .shortcut_hint = "",
         .visibility_flag = nullptr,
         .priority = 200 + room_id});

    dependencies_.panel_manager->ShowPanel(base_card_id);
  }
}

void DungeonEditorV2::OnEntranceSelected(int entrance_id) {
  if (entrance_id < 0 || entrance_id >= static_cast<int>(entrances_.size())) {
    return;
  }
  int room_id = entrances_[entrance_id].room_;
  OnRoomSelected(room_id);
}

void DungeonEditorV2::SaveAllRooms() {
  auto status = Save();
  if (status.ok()) {
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show("All rooms saved", ToastType::kSuccess);
    }
  } else {
    LOG_ERROR("DungeonEditorV2", "SaveAllRooms failed: %s",
              status.message().data());
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show(
          absl::StrFormat("Save all failed: %s", status.message()),
          ToastType::kError);
    }
  }
}

void DungeonEditorV2::add_room(int room_id) {
  OnRoomSelected(room_id);
}

void DungeonEditorV2::FocusRoom(int room_id) {
  auto it = room_cards_.find(room_id);
  if (it != room_cards_.end()) {
    it->second->Focus();
  }
}

void DungeonEditorV2::SelectObject(int obj_id) {
  if (object_editor_panel_) {
    ShowPanel(kObjectToolsId);
    object_editor_panel_->SelectObject(obj_id);
  }
}

void DungeonEditorV2::SetAgentMode(bool enabled) {
  if (enabled && dependencies_.panel_manager) {
    ShowPanel(kRoomSelectorId);
    ShowPanel(kObjectToolsId);
    ShowPanel(kRoomGraphicsId);
    if (object_editor_panel_) {
      object_editor_panel_->SetAgentOptimizedLayout(true);
    }
  }
}

void DungeonEditorV2::ProcessDeferredTextures() {
  gfx::Arena::Get().ProcessTextureQueue(renderer_);
}

void DungeonEditorV2::HandleObjectPlaced(const zelda3::RoomObject& obj) {
  if (current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_.size())) {
    LOG_ERROR("DungeonEditorV2", "Cannot place object: Invalid room ID %d",
              current_room_id_);
    return;
  }

  auto& room = rooms_[current_room_id_];

  LOG_INFO("DungeonEditorV2",
           "Placing object ID=0x%02X at position (%d,%d) in room %03X", obj.id_,
           obj.x_, obj.y_, current_room_id_);

  room.RenderRoomGraphics();
  LOG_DEBUG("DungeonEditorV2",
            "Object placed and room re-rendered successfully");
}

void DungeonEditorV2::OpenGraphicsEditorForObject(
    int room_id, const zelda3::RoomObject& object) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    LOG_WARN("DungeonEditorV2",
             "Edit Graphics ignored (invalid room id %d)", room_id);
    return;
  }

  auto* editor_manager =
      static_cast<EditorManager*>(dependencies_.custom_data);
  if (!editor_manager) {
    LOG_WARN("DungeonEditorV2",
             "Edit Graphics ignored (editor manager unavailable)");
    return;
  }

  auto& room = rooms_[room_id];
  room.LoadRoomGraphics(room.blockset);

  uint16_t sheet_id = 0;
  uint16_t tile_index = 0;
  bool resolved_sheet = false;
  if (auto tiles_or = object.GetTiles(); tiles_or.ok() &&
      !tiles_or.value().empty()) {
    const uint16_t tile_id = tiles_or.value().front().id_;
    const size_t block_index = static_cast<size_t>(tile_id / 64);
    const auto blocks = room.blocks();
    if (block_index < blocks.size()) {
      sheet_id = blocks[block_index];
      resolved_sheet = true;
    }
    const int tiles_per_row = gfx::kTilesheetWidth / 8;
    const int tiles_per_col = gfx::kTilesheetHeight / 8;
    const int tiles_per_sheet = tiles_per_row * tiles_per_col;
    if (tiles_per_sheet > 0) {
      tile_index = static_cast<uint16_t>(tile_id % tiles_per_sheet);
    }
  }

  editor_manager->SwitchToEditor(EditorType::kGraphics, true);
  if (auto* editor_set = editor_manager->GetCurrentEditorSet()) {
    if (auto* graphics = editor_set->GetGraphicsEditor()) {
      if (resolved_sheet) {
        graphics->SelectSheet(sheet_id);
        graphics->HighlightTile(
            sheet_id, tile_index,
            absl::StrFormat("Object 0x%02X", object.id_));
      }
    }
  }

  if (dependencies_.panel_manager) {
    dependencies_.panel_manager->ShowAllPanelsInCategory("Graphics");
  }
}

void DungeonEditorV2::SwapRoomInPanel(int old_room_id, int new_room_id) {
  // Defer the swap until after the current frame's draw phase completes
  // This prevents modifying data structures while ImGui is still using them
  if (new_room_id < 0 || new_room_id >= static_cast<int>(rooms_.size())) {
    return;
  }
  pending_swap_.old_room_id = old_room_id;
  pending_swap_.new_room_id = new_room_id;
  pending_swap_.pending = true;
}

void DungeonEditorV2::ProcessPendingSwap() {
  if (!pending_swap_.pending) {
    return;
  }

  int old_room_id = pending_swap_.old_room_id;
  int new_room_id = pending_swap_.new_room_id;
  pending_swap_.pending = false;

  // Find the position of old_room in active_rooms_
  int swap_index = -1;
  for (int i = 0; i < active_rooms_.Size; i++) {
    if (active_rooms_[i] == old_room_id) {
      swap_index = i;
      break;
    }
  }

  if (swap_index < 0) {
    // Old room not found in active rooms, just select the new one
    OnRoomSelected(new_room_id);
    return;
  }

  // Avoid swapping into an already-open room panel (the per-room maps assume
  // room IDs are unique). In this case, just focus/select the existing room.
  for (int i = 0; i < active_rooms_.Size; i++) {
    if (i != swap_index && active_rooms_[i] == new_room_id) {
      OnRoomSelected(new_room_id);
      return;
    }
  }

  // Preserve the old panel's stable ImGui window identity by transferring its
  // slot id to the new room id.
  int slot_id = -1;
  if (auto it = room_panel_slot_ids_.find(old_room_id);
      it != room_panel_slot_ids_.end()) {
    slot_id = it->second;
    room_panel_slot_ids_.erase(it);
  } else {
    slot_id = next_room_panel_slot_id_++;
  }
  room_panel_slot_ids_[new_room_id] = slot_id;

  // Preserve the viewer instance so canvas pan/zoom and UI state don't reset
  // when navigating with arrows (swap-in-panel).
  if (auto it = room_viewers_.find(old_room_id); it != room_viewers_.end()) {
    room_viewers_[new_room_id] = std::move(it->second);
    room_viewers_.erase(it);
  }

  // Replace old room with new room in active_rooms_
  active_rooms_[swap_index] = new_room_id;
  room_selector_.set_active_rooms(active_rooms_);

  // Unregister old panel
  if (dependencies_.panel_manager) {
    std::string old_card_id = absl::StrFormat("dungeon.room_%d", old_room_id);
    const bool old_pinned = dependencies_.panel_manager->IsPanelPinned(old_card_id);
    dependencies_.panel_manager->UnregisterPanel(old_card_id);

    // Register new panel
    // Use unified ResourceLabelProvider for room names
    std::string new_room_name = absl::StrFormat(
        "[%03X] %s", new_room_id, zelda3::GetRoomLabel(new_room_id).c_str());

    std::string new_card_id = absl::StrFormat("dungeon.room_%d", new_room_id);

    dependencies_.panel_manager->RegisterPanel(
        {.card_id = new_card_id,
         .display_name = new_room_name,
         .window_title = ICON_MD_GRID_ON " " + new_room_name,
         .icon = ICON_MD_GRID_ON,
         .category = "Dungeon",
         .shortcut_hint = "",
         .visibility_flag = nullptr,
         .priority = 200 + new_room_id});

    if (old_pinned) {
      dependencies_.panel_manager->SetPanelPinned(new_card_id, true);
    }
    dependencies_.panel_manager->ShowPanel(new_card_id);
  }

  // Clean up old room's card and viewer
  room_cards_.erase(old_room_id);

  // Update current selection
  OnRoomSelected(new_room_id, /*request_focus=*/false);
}

DungeonCanvasViewer* DungeonEditorV2::GetViewerForRoom(int room_id) {
  if (core::FeatureFlags::get().dungeon.kUseWorkbench) {
    (void)room_id;
    return GetWorkbenchViewer();
  }

  auto it = room_viewers_.find(room_id);
  if (it == room_viewers_.end()) {
    auto viewer = std::make_unique<DungeonCanvasViewer>(rom_);
    viewer->SetCompactHeaderMode(false);
    viewer->SetRoomDetailsExpanded(true);
    DungeonCanvasViewer* viewer_ptr = viewer.get();
    viewer->SetRooms(&rooms_);
    viewer->SetRenderer(renderer_);
    viewer->SetCurrentPaletteGroup(current_palette_group_);
    viewer->SetCurrentPaletteId(current_palette_id_);
    viewer->SetGameData(game_data_);

    // These hooks must remain correct even when a room panel swaps rooms while
    // keeping the same viewer instance (to preserve canvas pan/zoom + UI
    // state). Use the viewer's best-effort current room context instead of
    // capturing room_id at creation time.
    viewer->object_interaction().SetMutationHook([this, viewer_ptr]() {
      const int rid = viewer_ptr ? viewer_ptr->current_room_id() : -1;
      if (rid >= 0 && rid < static_cast<int>(rooms_.size())) {
        const auto domain =
            viewer_ptr->object_interaction().last_mutation_domain();
        if (domain == MutationDomain::kTileObjects) {
          BeginUndoSnapshot(rid);
        } else if (domain == MutationDomain::kCustomCollision) {
          BeginCollisionUndoSnapshot(rid);
        } else if (domain == MutationDomain::kWaterFill) {
          BeginWaterFillUndoSnapshot(rid);
        }
      }
    });

    viewer->object_interaction().SetCacheInvalidationCallback(
        [this, viewer_ptr]() {
          const int rid = viewer_ptr ? viewer_ptr->current_room_id() : -1;
          if (rid >= 0 && rid < static_cast<int>(rooms_.size())) {
            const auto domain =
                viewer_ptr->object_interaction().last_invalidation_domain();
            if (domain == MutationDomain::kTileObjects) {
              rooms_[rid].MarkObjectsDirty();
              rooms_[rid].RenderRoomGraphics();
              // Drag edits invalidate incrementally; finalize once the drag ends
              // (TileObjectHandler emits an extra invalidation on release).
              const auto mode =
                  viewer_ptr->object_interaction().mode_manager().GetMode();
              if (mode != InteractionMode::DraggingObjects) {
                FinalizeUndoAction(rid);
              }
            } else if (domain == MutationDomain::kCustomCollision) {
              const auto mode =
                  viewer_ptr->object_interaction().mode_manager().GetMode();
              const auto& st =
                  viewer_ptr->object_interaction().mode_manager().GetModeState();
              if (mode == InteractionMode::PaintCollision && st.is_painting) {
                return;
              }
              FinalizeCollisionUndoAction(rid);
            } else if (domain == MutationDomain::kWaterFill) {
              const auto mode =
                  viewer_ptr->object_interaction().mode_manager().GetMode();
              const auto& st =
                  viewer_ptr->object_interaction().mode_manager().GetModeState();
              if (mode == InteractionMode::PaintWaterFill && st.is_painting) {
                return;
              }
              FinalizeWaterFillUndoAction(rid);
            }
          }
        });

    viewer->object_interaction().SetObjectPlacedCallback(
        [this](const zelda3::RoomObject& obj) { HandleObjectPlaced(obj); });

    if (dungeon_editor_system_) {
      viewer->SetEditorSystem(dungeon_editor_system_.get());
    }
    viewer->SetRoomNavigationCallback([this](int target_room) {
      if (target_room >= 0 && target_room < static_cast<int>(rooms_.size())) {
        OnRoomSelected(target_room);
      }
    });
    // Swap callback swaps the room in the current panel instead of opening new
    viewer->SetRoomSwapCallback([this](int old_room, int new_room) {
      SwapRoomInPanel(old_room, new_room);
    });
    viewer->SetShowObjectPanelCallback([this]() { ShowPanel(kObjectToolsId); });
    viewer->SetShowSpritePanelCallback(
        [this]() { ShowPanel("dungeon.sprite_editor"); });
    viewer->SetShowItemPanelCallback(
        [this]() { ShowPanel("dungeon.item_editor"); });
    viewer->SetShowRoomListCallback([this]() { ShowPanel(kRoomSelectorId); });
    viewer->SetShowRoomMatrixCallback([this]() { ShowPanel(kRoomMatrixId); });
    viewer->SetShowEntranceListCallback(
        [this]() { ShowPanel(kEntranceListId); });
    viewer->SetShowRoomGraphicsCallback(
        [this]() { ShowPanel(kRoomGraphicsId); });
    viewer->SetShowDungeonSettingsCallback(
        [this]() { ShowPanel("dungeon.settings"); });
    viewer->SetEditGraphicsCallback(
        [this](int target_room_id, const zelda3::RoomObject& object) {
          OpenGraphicsEditorForObject(target_room_id, object);
        });
    viewer->SetSaveRoomCallback([this](int target_room_id) {
      auto status = SaveRoom(target_room_id);
      if (!status.ok()) {
        LOG_ERROR("DungeonEditorV2", "Save Room failed: %s",
                  status.message().data());
        if (dependencies_.toast_manager) {
          dependencies_.toast_manager->Show(
              absl::StrFormat("Save Room failed: %s", status.message()),
              ToastType::kError);
        }
        return;
      }
      if (dependencies_.toast_manager) {
        dependencies_.toast_manager->Show("Room saved", ToastType::kSuccess);
      }
    });

    // Wire up pinning for room panels
    if (dependencies_.panel_manager) {
      std::string card_id = absl::StrFormat("dungeon.room_%d", room_id);
      viewer->SetPinned(dependencies_.panel_manager->IsPanelPinned(card_id));
      viewer->SetPinCallback([this, card_id, room_id](bool pinned) {
        if (dependencies_.panel_manager) {
          dependencies_.panel_manager->SetPanelPinned(card_id, pinned);
          // Sync state back to viewer in all panels showing this room
          if (auto* v = GetViewerForRoom(room_id)) {
            v->SetPinned(pinned);
          }
        }
      });
    }

    viewer->SetMinecartTrackPanel(minecart_track_editor_panel_);
    viewer->SetProject(dependencies_.project);

    room_viewers_[room_id] = std::move(viewer);
    return room_viewers_[room_id].get();
  }

  // Update pinned state from manager even if viewer already exists
  if (dependencies_.panel_manager) {
    std::string card_id = absl::StrFormat("dungeon.room_%d", room_id);
    it->second->SetPinned(dependencies_.panel_manager->IsPanelPinned(card_id));
    // Ensure pin callback matches the current room_id, even if the viewer
    // instance was preserved across an in-panel room swap.
    it->second->SetPinCallback([this, card_id, room_id](bool pinned) {
      if (dependencies_.panel_manager) {
        dependencies_.panel_manager->SetPanelPinned(card_id, pinned);
        if (auto* v = GetViewerForRoom(room_id)) {
          v->SetPinned(pinned);
        }
      }
    });
  }

  return it->second.get();
}

DungeonCanvasViewer* DungeonEditorV2::GetWorkbenchViewer() {
  if (!workbench_viewer_) {
    workbench_viewer_ = std::make_unique<DungeonCanvasViewer>(rom_);
    auto* viewer = workbench_viewer_.get();
    viewer->SetCompactHeaderMode(true);
    viewer->SetRoomDetailsExpanded(false);
    viewer->SetHeaderVisible(false);
    viewer->SetRooms(&rooms_);
    viewer->SetRenderer(renderer_);
    viewer->SetCurrentPaletteGroup(current_palette_group_);
    viewer->SetCurrentPaletteId(current_palette_id_);
    viewer->SetGameData(game_data_);

    // Workbench uses a single viewer; these hooks use the viewer's current room
    // context (set at DrawDungeonCanvas start) so room switching stays correct.
    viewer->object_interaction().SetMutationHook([this, viewer]() {
      const int rid = viewer ? viewer->current_room_id() : -1;
      if (rid >= 0 && rid < static_cast<int>(rooms_.size())) {
        const auto domain = viewer->object_interaction().last_mutation_domain();
        if (domain == MutationDomain::kTileObjects) {
          BeginUndoSnapshot(rid);
        } else if (domain == MutationDomain::kCustomCollision) {
          BeginCollisionUndoSnapshot(rid);
        } else if (domain == MutationDomain::kWaterFill) {
          BeginWaterFillUndoSnapshot(rid);
        }
      }
    });
    viewer->object_interaction().SetCacheInvalidationCallback([this, viewer]() {
      const int rid = viewer ? viewer->current_room_id() : -1;
      if (rid >= 0 && rid < static_cast<int>(rooms_.size())) {
        const auto domain =
            viewer->object_interaction().last_invalidation_domain();
        if (domain == MutationDomain::kTileObjects) {
          rooms_[rid].MarkObjectsDirty();
          rooms_[rid].RenderRoomGraphics();
          const auto mode = viewer->object_interaction().mode_manager().GetMode();
          if (mode != InteractionMode::DraggingObjects) {
            FinalizeUndoAction(rid);
          }
        } else if (domain == MutationDomain::kCustomCollision) {
          const auto mode = viewer->object_interaction().mode_manager().GetMode();
          const auto& st = viewer->object_interaction().mode_manager().GetModeState();
          if (mode == InteractionMode::PaintCollision && st.is_painting) {
            return;
          }
          FinalizeCollisionUndoAction(rid);
        } else if (domain == MutationDomain::kWaterFill) {
          const auto mode = viewer->object_interaction().mode_manager().GetMode();
          const auto& st = viewer->object_interaction().mode_manager().GetModeState();
          if (mode == InteractionMode::PaintWaterFill && st.is_painting) {
            return;
          }
          FinalizeWaterFillUndoAction(rid);
        }
      }
    });

    viewer->object_interaction().SetObjectPlacedCallback(
        [this](const zelda3::RoomObject& obj) { HandleObjectPlaced(obj); });

    if (dungeon_editor_system_) {
      viewer->SetEditorSystem(dungeon_editor_system_.get());
    }

    // In workbench mode, arrow navigation swaps the current room without
    // changing window identities.
    viewer->SetRoomSwapCallback([this](int /*old_room*/, int new_room) {
      OnRoomSelected(new_room, /*request_focus=*/false);
    });
    viewer->SetRoomNavigationCallback([this](int target_room) {
      OnRoomSelected(target_room, /*request_focus=*/false);
    });

    viewer->SetShowObjectPanelCallback([this]() { ShowPanel(kObjectToolsId); });
    viewer->SetShowSpritePanelCallback(
        [this]() { ShowPanel("dungeon.sprite_editor"); });
    viewer->SetShowItemPanelCallback(
        [this]() { ShowPanel("dungeon.item_editor"); });
    viewer->SetShowRoomListCallback([this]() { ShowPanel(kRoomSelectorId); });
    viewer->SetShowRoomMatrixCallback([this]() { ShowPanel(kRoomMatrixId); });
    viewer->SetShowEntranceListCallback(
        [this]() { ShowPanel(kEntranceListId); });
    viewer->SetShowRoomGraphicsCallback(
        [this]() { ShowPanel(kRoomGraphicsId); });
    viewer->SetShowDungeonSettingsCallback(
        [this]() { ShowPanel("dungeon.settings"); });
    viewer->SetEditGraphicsCallback(
        [this](int target_room_id, const zelda3::RoomObject& object) {
          OpenGraphicsEditorForObject(target_room_id, object);
        });
    viewer->SetSaveRoomCallback([this](int target_room_id) {
      auto status = SaveRoom(target_room_id);
      if (!status.ok()) {
        LOG_ERROR("DungeonEditorV2", "Save Room failed: %s",
                  status.message().data());
        if (dependencies_.toast_manager) {
          dependencies_.toast_manager->Show(
              absl::StrFormat("Save Room failed: %s", status.message()),
              ToastType::kError);
        }
        return;
      }
      if (dependencies_.toast_manager) {
        dependencies_.toast_manager->Show("Room saved", ToastType::kSuccess);
      }
    });

    viewer->SetMinecartTrackPanel(minecart_track_editor_panel_);
    viewer->SetProject(dependencies_.project);
  }

  return workbench_viewer_.get();
}

DungeonCanvasViewer* DungeonEditorV2::GetWorkbenchCompareViewer() {
  if (!workbench_compare_viewer_) {
    workbench_compare_viewer_ = std::make_unique<DungeonCanvasViewer>(rom_);
    auto* viewer = workbench_compare_viewer_.get();
    viewer->SetCompactHeaderMode(true);
    viewer->SetRoomDetailsExpanded(false);
    viewer->SetRooms(&rooms_);
    viewer->SetRenderer(renderer_);
    viewer->SetCurrentPaletteGroup(current_palette_group_);
    viewer->SetCurrentPaletteId(current_palette_id_);
    viewer->SetGameData(game_data_);

    // Compare viewer is read-only by default: no object selection/mutation, but
    // still allows canvas pan/zoom.
    viewer->SetObjectInteractionEnabled(false);
    viewer->SetHeaderReadOnly(true);
    viewer->SetHeaderVisible(false);

    if (dungeon_editor_system_) {
      // Allows consistent rendering paths that depend on the editor system, but
      // interaction is still disabled.
      viewer->SetEditorSystem(dungeon_editor_system_.get());
    }

    viewer->SetMinecartTrackPanel(minecart_track_editor_panel_);
    viewer->SetProject(dependencies_.project);
  }

  return workbench_compare_viewer_.get();
}

absl::Status DungeonEditorV2::Undo() {
  // Finalize any in-progress edit before undoing.
  if (pending_undo_.room_id >= 0) {
    FinalizeUndoAction(pending_undo_.room_id);
  }
  if (pending_collision_undo_.room_id >= 0) {
    FinalizeCollisionUndoAction(pending_collision_undo_.room_id);
  }
  if (pending_water_fill_undo_.room_id >= 0) {
    FinalizeWaterFillUndoAction(pending_water_fill_undo_.room_id);
  }
  return undo_manager_.Undo();
}

absl::Status DungeonEditorV2::Redo() {
  // Finalize any in-progress edit before redoing.
  if (pending_undo_.room_id >= 0) {
    FinalizeUndoAction(pending_undo_.room_id);
  }
  if (pending_collision_undo_.room_id >= 0) {
    FinalizeCollisionUndoAction(pending_collision_undo_.room_id);
  }
  if (pending_water_fill_undo_.room_id >= 0) {
    FinalizeWaterFillUndoAction(pending_water_fill_undo_.room_id);
  }
  return undo_manager_.Redo();
}

absl::Status DungeonEditorV2::Cut() {
  if (auto* viewer = GetViewerForRoom(current_room_id_)) {
    viewer->object_interaction().HandleCopySelected();
    viewer->object_interaction().HandleDeleteSelected();
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Copy() {
  if (auto* viewer = GetViewerForRoom(current_room_id_)) {
    viewer->object_interaction().HandleCopySelected();
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Paste() {
  if (auto* viewer = GetViewerForRoom(current_room_id_)) {
    viewer->object_interaction().HandlePasteObjects();
  }
  return absl::OkStatus();
}

void DungeonEditorV2::BeginUndoSnapshot(int room_id) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  // If there's already a pending snapshot, finalize it first. This handles:
  // 1. Drag operations where NotifyMutation fires once at drag start but
  //    NotifyInvalidateCache doesn't fire until later (or the next mutation).
  // 2. The rare case where two mutations fire for different rooms.
  if (pending_undo_.room_id >= 0) {
    FinalizeUndoAction(pending_undo_.room_id);
  }

  pending_undo_.room_id = room_id;
  pending_undo_.before_objects = rooms_[room_id].GetTileObjects();
}

void DungeonEditorV2::FinalizeUndoAction(int room_id) {
  if (pending_undo_.room_id < 0 || pending_undo_.room_id != room_id)
    return;
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto after_objects = rooms_[room_id].GetTileObjects();

  auto action = std::make_unique<DungeonObjectsAction>(
      room_id, std::move(pending_undo_.before_objects),
      std::move(after_objects),
      [this](int rid, const std::vector<zelda3::RoomObject>& objects) {
        RestoreRoomObjects(rid, objects);
      });
  undo_manager_.Push(std::move(action));

  pending_undo_.room_id = -1;
  pending_undo_.before_objects.clear();
}

void DungeonEditorV2::SyncPanelsToRoom(int room_id) {
  // Update object editor card with current viewer
  if (object_editor_panel_) {
    object_editor_panel_->SetCurrentRoom(room_id);
    object_editor_panel_->SetCanvasViewer(GetViewerForRoom(room_id));
  }

  // Update sprite and item editor panels with current viewer
  if (sprite_editor_panel_) {
    sprite_editor_panel_->SetCanvasViewer(GetViewerForRoom(room_id));
  }
  if (item_editor_panel_) {
    item_editor_panel_->SetCanvasViewer(GetViewerForRoom(room_id));
  }
  if (custom_collision_panel_) {
    auto* viewer = GetViewerForRoom(room_id);
    custom_collision_panel_->SetCanvasViewer(viewer);
    if (viewer) {
      custom_collision_panel_->SetInteraction(&viewer->object_interaction());
    }
  }
  if (water_fill_panel_) {
    auto* viewer = GetViewerForRoom(room_id);
    water_fill_panel_->SetCanvasViewer(viewer);
    if (viewer) {
      water_fill_panel_->SetInteraction(&viewer->object_interaction());
    }
  }

  if (dungeon_settings_panel_) {
    dungeon_settings_panel_->SetCanvasViewer(GetViewerForRoom(room_id));
  }

  if (room_tag_editor_panel_) {
    room_tag_editor_panel_->SetCurrentRoomId(room_id);
  }
}

void DungeonEditorV2::ShowRoomPanel(int room_id) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    return;
  }

  std::string card_id = absl::StrFormat("dungeon.room_%d", room_id);
  
  if (dependencies_.panel_manager) {
    if (!dependencies_.panel_manager->GetPanelDescriptor(
            dependencies_.panel_manager->GetActiveSessionId(), card_id)) {
      std::string room_name = absl::StrFormat(
          "[%03X] %s", room_id, zelda3::GetRoomLabel(room_id).c_str());
      dependencies_.panel_manager->RegisterPanel(
          {.card_id = card_id,
           .display_name = room_name,
           .window_title = ICON_MD_GRID_ON " " + room_name,
           .icon = ICON_MD_GRID_ON,
           .category = "Dungeon",
           .shortcut_hint = "",
           .visibility_flag = nullptr,
           .priority = 200 + room_id});
    }
    dependencies_.panel_manager->ShowPanel(card_id);
  }

  // Create or update the PanelWindow for this room
  if (room_cards_.find(room_id) == room_cards_.end()) {
    std::string base_name = absl::StrFormat(
        "[%03X] %s", room_id, zelda3::GetRoomLabel(room_id).c_str());
    const int slot_id = GetOrCreateRoomPanelSlotId(room_id);
    std::string card_name_str = absl::StrFormat(
        "%s###RoomPanelSlot%d", MakePanelTitle(base_name).c_str(), slot_id);
    
    auto card = std::make_shared<gui::PanelWindow>(
        card_name_str.c_str(), ICON_MD_GRID_ON);
    card->SetDefaultSize(620, 700);
    room_cards_[room_id] = card;
  }
}

void DungeonEditorV2::RestoreRoomObjects(
    int room_id, const std::vector<zelda3::RoomObject>& objects) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto& room = rooms_[room_id];
  room.GetTileObjects() = objects;
  room.RenderRoomGraphics();
}

void DungeonEditorV2::BeginCollisionUndoSnapshot(int room_id) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  if (pending_collision_undo_.room_id >= 0) {
    FinalizeCollisionUndoAction(pending_collision_undo_.room_id);
  }

  pending_collision_undo_.room_id = room_id;
  pending_collision_undo_.before = rooms_[room_id].custom_collision();
}

void DungeonEditorV2::FinalizeCollisionUndoAction(int room_id) {
  if (pending_collision_undo_.room_id < 0 ||
      pending_collision_undo_.room_id != room_id) {
    return;
  }
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto after = rooms_[room_id].custom_collision();
  if (pending_collision_undo_.before.has_data == after.has_data &&
      pending_collision_undo_.before.tiles == after.tiles) {
    pending_collision_undo_.room_id = -1;
    pending_collision_undo_.before = {};
    return;
  }

  auto action = std::make_unique<DungeonCustomCollisionAction>(
      room_id, std::move(pending_collision_undo_.before), std::move(after),
      [this](int rid, const zelda3::CustomCollisionMap& map) {
        RestoreRoomCustomCollision(rid, map);
      });
  undo_manager_.Push(std::move(action));

  pending_collision_undo_.room_id = -1;
  pending_collision_undo_.before = {};
}

void DungeonEditorV2::RestoreRoomCustomCollision(
    int room_id, const zelda3::CustomCollisionMap& map) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto& room = rooms_[room_id];
  room.custom_collision() = map;
  room.MarkCustomCollisionDirty();
}

namespace {

WaterFillSnapshot MakeWaterFillSnapshot(const zelda3::Room& room) {
  WaterFillSnapshot snap;
  snap.sram_bit_mask = room.water_fill_sram_bit_mask();

  const auto& zone = room.water_fill_zone();
  // Preserve deterministic ordering (ascending offsets) for stable diffs.
  for (size_t i = 0; i < zone.tiles.size(); ++i) {
    if (zone.tiles[i] != 0) {
      snap.offsets.push_back(static_cast<uint16_t>(i));
    }
  }
  return snap;
}

}  // namespace

void DungeonEditorV2::BeginWaterFillUndoSnapshot(int room_id) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  if (pending_water_fill_undo_.room_id >= 0) {
    FinalizeWaterFillUndoAction(pending_water_fill_undo_.room_id);
  }

  pending_water_fill_undo_.room_id = room_id;
  pending_water_fill_undo_.before = MakeWaterFillSnapshot(rooms_[room_id]);
}

void DungeonEditorV2::FinalizeWaterFillUndoAction(int room_id) {
  if (pending_water_fill_undo_.room_id < 0 ||
      pending_water_fill_undo_.room_id != room_id) {
    return;
  }
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto after = MakeWaterFillSnapshot(rooms_[room_id]);
  if (pending_water_fill_undo_.before.sram_bit_mask == after.sram_bit_mask &&
      pending_water_fill_undo_.before.offsets == after.offsets) {
    pending_water_fill_undo_.room_id = -1;
    pending_water_fill_undo_.before = {};
    return;
  }

  auto action = std::make_unique<DungeonWaterFillAction>(
      room_id, std::move(pending_water_fill_undo_.before), std::move(after),
      [this](int rid, const WaterFillSnapshot& snap) {
        RestoreRoomWaterFill(rid, snap);
      });
  undo_manager_.Push(std::move(action));

  pending_water_fill_undo_.room_id = -1;
  pending_water_fill_undo_.before = {};
}

void DungeonEditorV2::RestoreRoomWaterFill(int room_id,
                                          const WaterFillSnapshot& snap) {
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size()))
    return;

  auto& room = rooms_[room_id];
  room.ClearWaterFillZone();
  room.set_water_fill_sram_bit_mask(snap.sram_bit_mask);
  for (uint16_t off : snap.offsets) {
    const int x = static_cast<int>(off % 64);
    const int y = static_cast<int>(off / 64);
    room.SetWaterFillTile(x, y, /*filled=*/true);
  }
  room.MarkWaterFillDirty();
}

}  // namespace yaze::editor
