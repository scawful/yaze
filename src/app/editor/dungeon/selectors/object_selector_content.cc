// Related header
#include "object_selector_content.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// Third-party library headers
#include "absl/strings/str_format.h"
#include "editor/dungeon/dungeon_canvas_viewer.h"
#include "gfx/types/snes_palette.h"
#include "imgui/imgui.h"

// Project headers
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_limits.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/dungeon_validator.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/object_parser.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

ObjectSelectorContent::ObjectSelectorContent(
    gfx::IRenderer* renderer, Rom* rom, DungeonCanvasViewer* canvas_viewer,
    std::shared_ptr<zelda3::DungeonObjectEditor> object_editor)
    : renderer_(renderer),
      rom_(rom),
      canvas_viewer_(canvas_viewer),
      object_selector_(rom),
      object_editor_(object_editor) {
  // Initialize object parser for static editor info lookup
  if (rom) {
    object_parser_ = std::make_unique<zelda3::ObjectParser>(rom);
  }

  // Wire up object selector callback
  object_selector_.SetObjectSelectedCallback(
      [this](const zelda3::RoomObject& obj) {
        preview_object_ = obj;
        has_preview_object_ = true;
        if (canvas_viewer_) {
          canvas_viewer_->SetPreviewObject(preview_object_);
          canvas_viewer_->SetObjectInteractionEnabled(true);
        }

        // Sync with backend editor if available
        if (object_editor_) {
          object_editor_->SetMode(zelda3::DungeonObjectEditor::Mode::kInsert);
          object_editor_->SetCurrentObjectType(obj.id_);
        }
      });

  // Wire up double-click callback for static object editor
  object_selector_.SetObjectDoubleClickCallback(
      [this](int obj_id) { OpenStaticObjectEditor(obj_id); });
}

DungeonCanvasViewer* ObjectSelectorContent::ResolveCanvasViewer() {
  if (canvas_viewer_provider_) {
    canvas_viewer_ = canvas_viewer_provider_();
  }
  return canvas_viewer_;
}

void ObjectSelectorContent::Draw(bool* p_open) {
  (void)p_open;
  ResolveCanvasViewer();

  const auto& theme = AgentUI::GetTheme();
  const int max_objects = static_cast<int>(zelda3::kMaxTileObjects);
  const int max_sprites = static_cast<int>(zelda3::kMaxTotalSprites);
  const int max_doors = static_cast<int>(zelda3::kMaxDoors);

  // Check if placement was blocked by ROM limits
  if (canvas_viewer_) {
    auto& coordinator =
        canvas_viewer_->object_interaction().entity_coordinator();

    auto& tile_handler = coordinator.tile_handler();
    if (tile_handler.was_placement_blocked()) {
      const auto reason = tile_handler.placement_block_reason();
      tile_handler.clear_placement_blocked();
      switch (reason) {
        case TileObjectHandler::PlacementBlockReason::kObjectLimit:
          SetPlacementError(absl::StrFormat(
              "Object limit reached (%d max) - placement blocked",
              max_objects));
          break;
        case TileObjectHandler::PlacementBlockReason::kInvalidRoom:
          SetPlacementError("Invalid room target - placement blocked");
          break;
        case TileObjectHandler::PlacementBlockReason::kNone:
        default:
          SetPlacementError("Object placement blocked");
          break;
      }
    }

    auto& sprite_handler = coordinator.sprite_handler();
    if (sprite_handler.was_placement_blocked()) {
      const auto reason = sprite_handler.placement_block_reason();
      sprite_handler.clear_placement_blocked();
      switch (reason) {
        case SpriteInteractionHandler::PlacementBlockReason::kSpriteLimit:
          SetPlacementError(absl::StrFormat(
              "Sprite limit reached (%d max) - placement blocked",
              max_sprites));
          break;
        case SpriteInteractionHandler::PlacementBlockReason::kInvalidRoom:
          SetPlacementError("Invalid room target - sprite placement blocked");
          break;
        case SpriteInteractionHandler::PlacementBlockReason::kNone:
        default:
          SetPlacementError("Sprite placement blocked");
          break;
      }
    }

    auto& door_handler = coordinator.door_handler();
    if (door_handler.was_placement_blocked()) {
      const auto reason = door_handler.placement_block_reason();
      door_handler.clear_placement_blocked();
      switch (reason) {
        case DoorInteractionHandler::PlacementBlockReason::kDoorLimit:
          SetPlacementError(absl::StrFormat(
              "Door limit reached (%d max) - placement blocked", max_doors));
          break;
        case DoorInteractionHandler::PlacementBlockReason::kInvalidPosition:
          SetPlacementError("Invalid door position - must be near a wall");
          break;
        case DoorInteractionHandler::PlacementBlockReason::kInvalidRoom:
          SetPlacementError("Invalid room target - door placement blocked");
          break;
        case DoorInteractionHandler::PlacementBlockReason::kNone:
        default:
          SetPlacementError("Door placement blocked");
          break;
      }
    }
  }

  DrawRoomValidationBar();
  DrawInteractionSummary();

  float available_height = ImGui::GetContentRegionAvail().y;
  float browser_height =
      std::max(220.0f, available_height - (static_editor_open_ ? 260.0f : 0.0f));

  gui::SectionHeader(ICON_MD_CATEGORY, "Object Selector", theme.text_info);
  ImGui::TextColored(
      theme.text_secondary_gray,
      "Choose an object to place. Double-click an entry to inspect its draw "
      "routine or open the tile editor.");
  ImGui::BeginChild("ObjectBrowserRegion", ImVec2(0, browser_height), true);
  DrawObjectSelector();
  ImGui::EndChild();

  if (static_editor_open_) {
    ImGui::Spacing();
    DrawStaticObjectEditor();
  }
}

void ObjectSelectorContent::SelectObject(int obj_id) {
  object_selector_.SelectObject(obj_id);
}

void ObjectSelectorContent::SetAgentOptimizedLayout(bool enabled) {
  // In agent mode, we might force tabs open or change layout
  (void)enabled;
}

void ObjectSelectorContent::SetPlacementError(const std::string& message) {
  // Avoid refreshing the timer for repeated identical errors; keeps the
  // message stable during rapid blocked clicks.
  if (message == last_placement_error_ && placement_error_time_ >= 0.0) {
    double elapsed = ImGui::GetTime() - placement_error_time_;
    if (elapsed < kPlacementErrorDuration) {
      return;
    }
  }
  last_placement_error_ = message;
  placement_error_time_ = ImGui::GetTime();
}

void ObjectSelectorContent::DrawObjectSelector() {
  // Delegate to the DungeonObjectSelector component
  object_selector_.DrawObjectAssetBrowser();
}

void ObjectSelectorContent::DrawInteractionSummary() {
  const auto& theme = AgentUI::GetTheme();
  auto* viewer = ResolveCanvasViewer();
  const size_t selection_count =
      viewer ? viewer->object_interaction().GetSelectionCount() : 0;

  gui::SectionHeader(ICON_MD_TOUCH_APP, "Interaction", theme.text_info);

  bool is_placing = has_preview_object_ && canvas_viewer_ &&
                    canvas_viewer_->object_interaction().IsObjectLoaded();
  if (!is_placing && has_preview_object_) {
    has_preview_object_ = false;
  }

  if (is_placing) {
    ImGui::TextColored(theme.status_warning,
                       ICON_MD_ADD_CIRCLE " Placement ready: 0x%03X %s",
                       preview_object_.id_,
                       zelda3::GetObjectName(preview_object_.id_).c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel")) {
      CancelPlacement();
    }
  } else if (selection_count == 1) {
    ImGui::TextColored(theme.status_success,
                       ICON_MD_CHECK_CIRCLE " 1 room object selected");
    if (open_object_editor_callback_) {
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW " Open Object Editor")) {
        open_object_editor_callback_();
      }
    }
  } else if (selection_count > 1) {
    ImGui::TextColored(theme.status_success,
                       ICON_MD_SELECT_ALL " %zu room objects selected",
                       selection_count);
    if (open_object_editor_callback_) {
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW " Open Object Editor")) {
        open_object_editor_callback_();
      }
    }
  } else {
    ImGui::TextColored(theme.text_secondary_gray,
                       ICON_MD_MOUSE
                       " Browse objects below to place them. Click room "
                       "objects to edit them in the Object Editor.");
  }

  if (!last_placement_error_.empty()) {
    double elapsed = ImGui::GetTime() - placement_error_time_;
    if (elapsed < kPlacementErrorDuration) {
      ImGui::TextColored(theme.status_error, ICON_MD_ERROR " %s",
                         last_placement_error_.c_str());
    } else {
      last_placement_error_.clear();
    }
  }
}

// =============================================================================
// Static Object Editor (opened via double-click)
// =============================================================================

void ObjectSelectorContent::OpenStaticObjectEditor(int object_id) {
  static_editor_open_ = true;
  static_editor_object_id_ = object_id;
  static_preview_rendered_ = false;

  // Sync with object selector for visual indicator
  object_selector_.SetStaticEditorObjectId(object_id);

  // Fetch draw routine info for this object
  if (object_parser_) {
    static_editor_draw_info_ = object_parser_->GetObjectDrawInfo(object_id);
  }

  // Render the object preview using ObjectDrawer
  auto* rooms = object_selector_.get_rooms();
  if (rom_ && rom_->is_loaded() && rooms && current_room_id_ >= 0 &&
      current_room_id_ < static_cast<int>(rooms->size())) {
    auto& room = (*rooms)[current_room_id_];

    // Ensure room graphics are loaded
    if (!room.IsLoaded()) {
      room.LoadRoomGraphics(room.blockset());
    }

    // Clear preview buffer and initialize bitmap
    static_preview_buffer_.ClearBuffer();
    static_preview_buffer_.EnsureBitmapInitialized();

    // Create a preview object at top-left of canvas (tile 2,2 = pixel 16,16)
    // to fit within the 128x128 preview area with some margin
    zelda3::RoomObject preview_obj(object_id, 2, 2, 0x12, 0);
    preview_obj.SetRom(rom_);
    preview_obj.EnsureTilesLoaded();

    if (preview_obj.tiles().empty()) {
      return;  // No tiles to draw
    }

    // Get room graphics data
    const uint8_t* gfx_data = room.get_gfx_buffer().data();

    // Apply palette to bitmap
    auto& bitmap = static_preview_buffer_.bitmap();
    gfx::PaletteGroup palette_group;
    auto* game_data = object_selector_.game_data();
    if (game_data && !game_data->palette_groups.dungeon_main.empty()) {
      // Use the entire dungeon_main palette group
      palette_group = game_data->palette_groups.dungeon_main;

      std::vector<SDL_Color> colors(256);
      size_t color_index = 0;
      for (size_t pal_idx = 0;
           pal_idx < palette_group.size() && color_index < 256; ++pal_idx) {
        const auto& pal = palette_group[pal_idx];
        for (size_t i = 0; i < pal.size() && color_index < 256; ++i) {
          ImVec4 rgb = pal[i].rgb();
          colors[color_index++] = {static_cast<Uint8>(rgb.x),
                                   static_cast<Uint8>(rgb.y),
                                   static_cast<Uint8>(rgb.z), 255};
        }
      }
      colors[255] = {0, 0, 0, 0};  // Transparent
      bitmap.SetPalette(colors);
      if (bitmap.surface()) {
        SDL_SetColorKey(bitmap.surface(), SDL_TRUE, 255);
        SDL_SetSurfaceBlendMode(bitmap.surface(), SDL_BLENDMODE_BLEND);
      }
    }

    // Create drawer with room's graphics data
    zelda3::ObjectDrawer drawer(rom_, current_room_id_, gfx_data);
    drawer.InitializeDrawRoutines();

    auto status = drawer.DrawObject(preview_obj, static_preview_buffer_,
                                    static_preview_buffer_, palette_group);
    if (status.ok()) {
      // Sync bitmap data to SDL surface
      if (bitmap.modified() && bitmap.surface() &&
          bitmap.mutable_data().size() > 0) {
        SDL_LockSurface(bitmap.surface());
        size_t surface_size = bitmap.surface()->h * bitmap.surface()->pitch;
        size_t data_size = bitmap.mutable_data().size();
        if (surface_size >= data_size) {
          memcpy(bitmap.surface()->pixels, bitmap.mutable_data().data(),
                 data_size);
        }
        SDL_UnlockSurface(bitmap.surface());
      }

      // Create texture
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &bitmap);
      gfx::Arena::Get().ProcessTextureQueue(renderer_);

      static_preview_rendered_ = bitmap.texture() != nullptr;
    }
  }
}

void ObjectSelectorContent::CloseStaticObjectEditor() {
  static_editor_open_ = false;
  static_editor_object_id_ = -1;

  // Clear the visual indicator in object selector
  object_selector_.SetStaticEditorObjectId(-1);
}

void ObjectSelectorContent::DrawStaticObjectEditor() {
  const auto& theme = AgentUI::GetTheme();

  gui::StyleColorGuard header_colors({
      {ImGuiCol_Header, ImVec4(0.15f, 0.25f, 0.35f, 1.0f)},
      {ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.30f, 0.40f, 1.0f)},
  });

  bool header_open = ImGui::CollapsingHeader(
      absl::StrFormat(ICON_MD_CONSTRUCTION " Object 0x%02X - %s",
                      static_editor_object_id_,
                      static_editor_draw_info_.routine_name.c_str())
          .c_str(),
      ImGuiTreeNodeFlags_DefaultOpen);

  if (header_open) {
    gui::StyleVarGuard frame_pad_guard(ImGuiStyleVar_FramePadding,
                                       ImVec2(8, 6));

    // Two-column layout: Info | Preview
    if (ImGui::BeginTable("StaticEditorLayout", 2,
                          ImGuiTableFlags_BordersInnerV)) {
      ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed, 200);
      ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);

      ImGui::TableNextRow();

      // Left column: Object information
      ImGui::TableNextColumn();
      {
        // Object ID with hex/decimal display
        ImGui::TextColored(theme.text_info, ICON_MD_TAG " Object ID");
        ImGui::SameLine();
        ImGui::Text("0x%02X (%d)", static_editor_object_id_,
                    static_editor_object_id_);

        ImGui::Spacing();

        // Draw routine info
        ImGui::TextColored(theme.text_info, ICON_MD_BRUSH " Draw Routine");
        ImGui::Indent();
        ImGui::Text("ID: %d", static_editor_draw_info_.draw_routine_id);
        ImGui::Text("Name: %s", static_editor_draw_info_.routine_name.c_str());
        ImGui::Unindent();

        ImGui::Spacing();

        // Tile and size info
        ImGui::TextColored(theme.text_info, ICON_MD_GRID_VIEW " Tile Info");
        ImGui::Indent();
        ImGui::Text("Tile Count: %d", static_editor_draw_info_.tile_count);
        ImGui::Text("Orientation: %s",
                    static_editor_draw_info_.is_horizontal ? "Horizontal"
                    : static_editor_draw_info_.is_vertical ? "Vertical"
                                                           : "Both");
        if (static_editor_draw_info_.both_layers) {
          ImGui::TextColored(theme.status_warning, ICON_MD_LAYERS " Both BG");
        }
        ImGui::Unindent();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Action buttons (vertical layout)
        if (ImGui::Button(ICON_MD_CONTENT_COPY " Copy ID", ImVec2(-1, 0))) {
          ImGui::SetClipboardText(
              absl::StrFormat("0x%02X", static_editor_object_id_).c_str());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Copy object ID to clipboard");
        }

        if (ImGui::Button(ICON_MD_CODE " Export ASM", ImVec2(-1, 0))) {
          const std::string asm_preview = absl::StrFormat(
              "; Object 0x%02X (%s)\n"
              "; Auto-generated preview stub\n"
              "org $000000\n"
              "dw $%02X ; object id\n"
              "; draw_routine=%d tile_count=%d\n",
              static_editor_object_id_, static_editor_draw_info_.routine_name,
              static_editor_object_id_,
              static_editor_draw_info_.draw_routine_id,
              static_editor_draw_info_.tile_count);
          ImGui::SetClipboardText(asm_preview.c_str());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Copy ASM preview stub to clipboard");
        }

        if (ImGui::Button(ICON_MD_GRID_ON " Edit Tiles", ImVec2(-1, 0))) {
          if (tile_editor_callback_) {
            tile_editor_callback_(
                static_cast<int16_t>(static_editor_object_id_));
          }
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Open tile editor to rearrange 8x8 tiles");
        }

        ImGui::Spacing();

        // Close button at bottom
        if (gui::DangerButton(ICON_MD_CLOSE " Close", ImVec2(-1, 0))) {
          CloseStaticObjectEditor();
        }
      }

      // Right column: Preview canvas
      ImGui::TableNextColumn();
      {
        ImGui::TextColored(theme.text_secondary_gray, "Preview:");

        gui::PreviewPanelOpts preview_opts;
        preview_opts.canvas_size = ImVec2(128, 128);
        preview_opts.render_popups = false;
        preview_opts.grid_step = 0.0f;
        preview_opts.ensure_texture = true;

        gui::CanvasFrameOptions frame_opts;
        frame_opts.canvas_size = preview_opts.canvas_size;
        frame_opts.draw_context_menu = false;
        frame_opts.draw_grid = preview_opts.grid_step > 0.0f;
        if (preview_opts.grid_step > 0.0f) {
          frame_opts.grid_step = preview_opts.grid_step;
        }
        frame_opts.draw_overlay = true;
        frame_opts.render_popups = preview_opts.render_popups;

        auto rt = gui::BeginCanvas(static_preview_canvas_, frame_opts);

        if (static_preview_rendered_) {
          auto& bitmap = static_preview_buffer_.bitmap();
          gui::RenderPreviewPanel(rt, bitmap, preview_opts);
        } else {
          gui::RenderPreviewPanel(rt, static_preview_buffer_.bitmap(),
                                  preview_opts);
          static_preview_canvas_.AddTextAt(
              ImVec2(24, 56), "No preview available",
              ImGui::GetColorU32(theme.text_secondary_gray));
        }
        gui::EndCanvas(static_preview_canvas_, rt, frame_opts);

        // Usage hint
        ImGui::Spacing();
        ImGui::TextColored(theme.text_secondary_gray, ICON_MD_INFO
                           " Double-click objects in browser\n"
                           "to view their draw routine info.");
      }

      ImGui::EndTable();
    }
  }
}

// =============================================================================
// Room Validation Bar
// =============================================================================

void ObjectSelectorContent::DrawRoomValidationBar() {
  auto* rooms = object_selector_.get_rooms();
  if (!rooms || current_room_id_ < 0 ||
      current_room_id_ >= zelda3::kNumberOfRooms) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  const auto& room = (*rooms)[current_room_id_];

  // Gather counts
  size_t object_count = room.GetTileObjects().size();
  size_t sprite_count = room.GetSprites().size();
  size_t door_count = room.GetDoors().size();

  // Count chests (objects in 0xF9-0xFD range)
  int chest_count = 0;
  for (const auto& obj : room.GetTileObjects()) {
    if (obj.id_ >= 0xF9 && obj.id_ <= 0xFD) {
      chest_count++;
    }
  }

  // Canonical limits shared across handlers/validator.
  const int kMaxObjects = static_cast<int>(zelda3::kMaxTileObjects);
  const int kMaxSprites = static_cast<int>(zelda3::kMaxTotalSprites);
  const int kMaxDoors = static_cast<int>(zelda3::kMaxDoors);
  const int kMaxChests = static_cast<int>(zelda3::kMaxChests);

  // Helper to pick color based on usage ratio
  auto usage_color = [&](size_t count, int max_val) -> ImVec4 {
    float ratio = static_cast<float>(count) / static_cast<float>(max_val);
    if (ratio >= 1.0f) {
      return theme.status_error;
    }
    if (ratio >= 0.75f) {
      return theme.status_warning;
    }
    return theme.text_secondary_gray;
  };

  // Compact inline counters
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 2));

  ImGui::TextColored(usage_color(object_count, kMaxObjects),
                     ICON_MD_WIDGETS " %zu/%d", object_count, kMaxObjects);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Objects: %zu of %d maximum", object_count, kMaxObjects);
  }

  ImGui::SameLine();
  ImGui::TextColored(usage_color(sprite_count, kMaxSprites),
                     ICON_MD_PEST_CONTROL " %zu/%d", sprite_count, kMaxSprites);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Sprites: %zu of %d maximum", sprite_count, kMaxSprites);
  }

  ImGui::SameLine();
  ImGui::TextColored(usage_color(door_count, kMaxDoors),
                     ICON_MD_DOOR_FRONT " %zu/%d", door_count, kMaxDoors);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Doors: %zu of %d maximum", door_count, kMaxDoors);
  }

  ImGui::SameLine();
  ImGui::TextColored(usage_color(chest_count, kMaxChests),
                     ICON_MD_INVENTORY_2 " %d/%d", chest_count, kMaxChests);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Chests: %d of %d maximum", chest_count, kMaxChests);
  }

  ImGui::PopStyleVar();

  // Run full validation and show warnings/errors inline
  zelda3::DungeonValidator validator;
  auto result = validator.ValidateRoom(room);
  if (!result.errors.empty() || !result.warnings.empty()) {
    for (const auto& err : result.errors) {
      ImGui::TextColored(theme.status_error, ICON_MD_ERROR " %s", err.c_str());
    }
    for (const auto& warn : result.warnings) {
      ImGui::TextColored(theme.status_warning, ICON_MD_WARNING " %s",
                         warn.c_str());
    }
  }

  ImGui::Separator();
}

void ObjectSelectorContent::CancelPlacement() {
  has_preview_object_ = false;
  if (canvas_viewer_) {
    canvas_viewer_->ClearPreviewObject();
    canvas_viewer_->object_interaction().CancelPlacement();
  }
}

}  // namespace editor
}  // namespace yaze
