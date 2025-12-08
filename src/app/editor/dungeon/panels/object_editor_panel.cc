// Related header
#include "object_editor_panel.h"
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
#include "app/gui/core/ui_helpers.h"
#include "rom/rom.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_parser.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

ObjectEditorPanel::ObjectEditorPanel(
    gfx::IRenderer* renderer, Rom* rom, DungeonCanvasViewer* canvas_viewer,
    std::shared_ptr<zelda3::DungeonObjectEditor> object_editor)
    : renderer_(renderer),
      rom_(rom),
      canvas_viewer_(canvas_viewer),
      object_selector_(rom),
      object_editor_(object_editor) {
  emulator_preview_.Initialize(renderer, rom);

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

  // Wire up selection change callback for property panel sync
  SetupSelectionCallbacks();
}

void ObjectEditorPanel::SetupSelectionCallbacks() {
  if (!canvas_viewer_ || selection_callbacks_setup_)
    return;

  auto& interaction = canvas_viewer_->object_interaction();
  interaction.SetSelectionChangeCallback([this]() { OnSelectionChanged(); });

  selection_callbacks_setup_ = true;
}

void ObjectEditorPanel::OnSelectionChanged() {
  if (!canvas_viewer_)
    return;

  auto& interaction = canvas_viewer_->object_interaction();
  cached_selection_count_ = interaction.GetSelectionCount();

  // Sync with backend editor if available
  if (object_editor_) {
    auto indices = interaction.GetSelectedObjectIndices();
    // Clear backend selection first
    (void)object_editor_->ClearSelection();

    // Add each selected index to backend
    for (size_t idx : indices) {
      (void)object_editor_->AddToSelection(idx);
    }
  }
}

void ObjectEditorPanel::Draw(bool* p_open) {
  const auto& theme = AgentUI::GetTheme();

  // Door Section (Collapsible)
  if (ImGui::CollapsingHeader(ICON_MD_DOOR_FRONT " Doors", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawDoorSection();
  }

  ImGui::Separator();

  // Object Browser - takes up available space
  float available_height = ImGui::GetContentRegionAvail().y;
  // Reserve space for status indicator at bottom
  float reserved_height = 60.0f;
  // Reduce browser height when static editor is open to give it more space
  if (static_editor_open_) {
    reserved_height += 200.0f;
  }
  float browser_height = std::max(150.0f, available_height - reserved_height);
  
  ImGui::BeginChild("ObjectBrowserRegion", ImVec2(0, browser_height), true);
  DrawObjectSelector();
  ImGui::EndChild();

  ImGui::Separator();

  // Static Object Editor (if open)
  if (static_editor_open_) {
    DrawStaticObjectEditor();
    ImGui::Separator();
  }

  // Status indicator: show current interaction state
  {
    bool is_placing = has_preview_object_ && canvas_viewer_ &&
                      canvas_viewer_->object_interaction().IsObjectLoaded();
    if (!is_placing && has_preview_object_) {
      has_preview_object_ = false;
    }
    if (is_placing) {
      ImGui::TextColored(theme.status_warning,
                         ICON_MD_ADD_CIRCLE " Placing: Object 0x%02X",
                         preview_object_.id_);
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel")) {
        CancelPlacement();
      }
    } else {
      ImGui::TextColored(
          theme.text_secondary_gray, ICON_MD_MOUSE
          " Selection Mode - Click to select, drag to multi-select");
      ImGui::TextColored(theme.text_secondary_gray, ICON_MD_MENU
                         " Right-click the canvas for Cut/Copy/Paste options");
    }
  }

  // Current object info
  DrawSelectedObjectInfo();

  ImGui::Separator();

  // Emulator Preview (Collapsible)
  bool preview_open = ImGui::CollapsingHeader(ICON_MD_MONITOR " Preview");
  show_emulator_preview_ = preview_open;

  if (preview_open) {
    ImGui::PushID("PreviewSection");
    DrawEmulatorPreview();
    ImGui::PopID();
  }

  // Handle keyboard shortcuts
  HandleKeyboardShortcuts();
}

void ObjectEditorPanel::SelectObject(int obj_id) {
  object_selector_.SelectObject(obj_id);
}

void ObjectEditorPanel::SetAgentOptimizedLayout(bool enabled) {
  // In agent mode, we might force tabs open or change layout
  (void)enabled;
}

void ObjectEditorPanel::DrawObjectSelector() {
  // Delegate to the DungeonObjectSelector component
  object_selector_.DrawObjectAssetBrowser();
}

void ObjectEditorPanel::DrawDoorSection() {
  const auto& theme = AgentUI::GetTheme();

  // Common door types for the grid
  static constexpr std::array<zelda3::DoorType, 20> kDoorTypes = {{
    zelda3::DoorType::NormalDoor,
    zelda3::DoorType::NormalDoorLower,
    zelda3::DoorType::CaveExit,
    zelda3::DoorType::DoubleSidedShutter,
    zelda3::DoorType::EyeWatchDoor,
    zelda3::DoorType::SmallKeyDoor,
    zelda3::DoorType::BigKeyDoor,
    zelda3::DoorType::SmallKeyStairsUp,
    zelda3::DoorType::SmallKeyStairsDown,
    zelda3::DoorType::DashWall,
    zelda3::DoorType::BombableDoor,
    zelda3::DoorType::ExplodingWall,
    zelda3::DoorType::CurtainDoor,
    zelda3::DoorType::BottomSidedShutter,
    zelda3::DoorType::TopSidedShutter,
    zelda3::DoorType::FancyDungeonExit,
    zelda3::DoorType::WaterfallDoor,
    zelda3::DoorType::ExitMarker,
    zelda3::DoorType::LayerSwapMarker,
    zelda3::DoorType::DungeonSwapMarker,
  }};

  // Placement mode indicator
  if (door_placement_mode_) {
    ImGui::TextColored(theme.status_warning, 
        ICON_MD_PLACE " Placing: %s - Click wall to place",
        std::string(zelda3::GetDoorTypeName(selected_door_type_)).c_str());
    if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel")) {
      door_placement_mode_ = false;
      if (canvas_viewer_) {
        canvas_viewer_->object_interaction().SetDoorPlacementMode(false, 
            zelda3::DoorType::NormalDoor);
      }
    }
    ImGui::Separator();
  }

  // Door type selector grid with preview thumbnails
  ImGui::Text(ICON_MD_CATEGORY " Select Door Type:");
  
  constexpr float kPreviewSize = 32.0f;
  constexpr int kItemsPerRow = 5;
  float panel_width = ImGui::GetContentRegionAvail().x;
  int items_per_row = std::max(1, static_cast<int>(panel_width / (kPreviewSize + 8)));
  
  ImGui::BeginChild("##DoorTypeGrid", ImVec2(0, 80), true, ImGuiWindowFlags_HorizontalScrollbar);
  
  int col = 0;
  for (size_t i = 0; i < kDoorTypes.size(); ++i) {
    auto door_type = kDoorTypes[i];
    bool is_selected = (selected_door_type_ == door_type);
    
    ImGui::PushID(static_cast<int>(i));
    
    // Color-coded button for door type
    ImVec4 button_color;
    // Color-code by door category
    int type_val = static_cast<int>(door_type);
    if (type_val <= 0x12) {  // Standard doors
      button_color = ImVec4(0.3f, 0.5f, 0.7f, 1.0f);  // Blue
    } else if (type_val <= 0x1E) {  // Shutter/special
      button_color = ImVec4(0.7f, 0.5f, 0.3f, 1.0f);  // Orange
    } else {  // Markers
      button_color = ImVec4(0.5f, 0.7f, 0.3f, 1.0f);  // Green
    }
    
    if (is_selected) {
      button_color.x += 0.2f;
      button_color.y += 0.2f;
      button_color.z += 0.2f;
    }
    
    ImGui::PushStyleColor(ImGuiCol_Button, button_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
        ImVec4(button_color.x + 0.1f, button_color.y + 0.1f, button_color.z + 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
        ImVec4(button_color.x + 0.2f, button_color.y + 0.2f, button_color.z + 0.2f, 1.0f));
    
    // Draw button with door type abbreviation
    std::string label = absl::StrFormat("%02X", type_val);
    if (ImGui::Button(label.c_str(), ImVec2(kPreviewSize, kPreviewSize))) {
      selected_door_type_ = door_type;
      door_placement_mode_ = true;
      if (canvas_viewer_) {
        canvas_viewer_->object_interaction().SetDoorPlacementMode(true, 
            selected_door_type_);
      }
    }
    
    ImGui::PopStyleColor(3);
    
    // Tooltip with full name
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s (0x%02X)\nClick to select for placement",
          std::string(zelda3::GetDoorTypeName(door_type)).c_str(), type_val);
    }
    
    // Selection highlight
    if (is_selected) {
      ImVec2 min = ImGui::GetItemRectMin();
      ImVec2 max = ImGui::GetItemRectMax();
      ImGui::GetWindowDrawList()->AddRect(
          min, max, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
    }
    
    ImGui::PopID();
    
    col++;
    if (col < items_per_row && i < kDoorTypes.size() - 1) {
      ImGui::SameLine();
    } else {
      col = 0;
    }
  }
  
  ImGui::EndChild();

  // Show current room's doors
  auto* rooms = object_selector_.get_rooms();
  if (rooms && current_room_id_ >= 0 && current_room_id_ < 296) {
    const auto& room = (*rooms)[current_room_id_];
    const auto& doors = room.GetDoors();

    if (!doors.empty()) {
      ImGui::Text(ICON_MD_LIST " Room Doors (%zu):", doors.size());
      
      ImGui::BeginChild("##DoorList", ImVec2(0, 80), true);
      for (size_t i = 0; i < doors.size(); ++i) {
        const auto& door = doors[i];
        auto [tile_x, tile_y] = door.GetTileCoords();

        ImGui::PushID(static_cast<int>(i));
        
        std::string type_name(zelda3::GetDoorTypeName(door.type));
        std::string dir_name(zelda3::GetDoorDirectionName(door.direction));
        
        ImGui::Text("[%zu] %s (%s)", i, type_name.c_str(), dir_name.c_str());
        ImGui::SameLine();
        ImGui::TextColored(theme.text_secondary_gray, "@ (%d,%d)", tile_x, tile_y);
        
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MD_DELETE "##Del")) {
          auto& mutable_room = (*rooms)[current_room_id_];
          mutable_room.RemoveDoor(i);
        }
        
        ImGui::PopID();
      }
      ImGui::EndChild();
    } else {
      ImGui::TextColored(theme.text_secondary_gray, 
          ICON_MD_INFO " No doors in this room");
    }
  }
}

void ObjectEditorPanel::DrawEmulatorPreview() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::TextColored(theme.text_secondary_gray,
                     ICON_MD_INFO " Real-time object rendering preview");
  gui::HelpMarker(
      "Uses SNES emulation to render objects accurately.\n"
      "May impact performance.");

  ImGui::Separator();

  ImGui::BeginChild("##EmulatorPreviewRegion", ImVec2(0, 260), true);
  emulator_preview_.Render();
  ImGui::EndChild();
}

void ObjectEditorPanel::DrawSelectedObjectInfo() {
  const auto& theme = AgentUI::GetTheme();

  // Show selection state at top - with extra safety checks
  if (canvas_viewer_ && canvas_viewer_->HasRooms()) {
    auto& interaction = canvas_viewer_->object_interaction();
    auto selected = interaction.GetSelectedObjectIndices();

    if (!selected.empty()) {
      ImGui::TextColored(theme.status_success,
                         ICON_MD_CHECK_CIRCLE " Selected:");
      ImGui::SameLine();

      if (selected.size() == 1) {
        if (object_editor_) {
          const auto& objects = object_editor_->GetObjects();
          if (selected[0] < objects.size()) {
            const auto& obj = objects[selected[0]];
            ImGui::Text("Object #%zu (ID: 0x%02X)", selected[0], obj.id_);
            ImGui::TextColored(theme.text_secondary_gray,
                               "  Position: (%d, %d)  Size: 0x%02X  Layer: %s",
                               obj.x_, obj.y_, obj.size_,
                               obj.layer_ == zelda3::RoomObject::BG1   ? "BG1"
                               : obj.layer_ == zelda3::RoomObject::BG2 ? "BG2"
                                                                       : "BG3");
          }
        } else {
          ImGui::Text("1 object");
        }
      } else {
        ImGui::Text("%zu objects", selected.size());
        ImGui::SameLine();
        ImGui::TextColored(theme.text_secondary_gray,
                           "(Shift+click to add, Ctrl+click to toggle)");
      }
      ImGui::Separator();
    }
  }

  ImGui::BeginGroup();

  ImGui::TextColored(theme.text_info, ICON_MD_INFO " Current:");

  if (has_preview_object_) {
    ImGui::SameLine();
    ImGui::Text("ID: 0x%02X", preview_object_.id_);
    ImGui::SameLine();
    ImGui::Text("Layer: %s",
                preview_object_.layer_ == zelda3::RoomObject::BG1   ? "BG1"
                : preview_object_.layer_ == zelda3::RoomObject::BG2 ? "BG2"
                                                                    : "BG3");
  }

  ImGui::EndGroup();
  ImGui::Separator();

  // Delegate property editing to the backend
  if (object_editor_) {
    object_editor_->DrawPropertyUI();
  }
}

// =============================================================================
// Static Object Editor (opened via double-click)
// =============================================================================

void ObjectEditorPanel::OpenStaticObjectEditor(int object_id) {
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
      room.LoadRoomGraphics(room.blockset);
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
      for (size_t pal_idx = 0; pal_idx < palette_group.size() && color_index < 256; ++pal_idx) {
        const auto& pal = palette_group[pal_idx];
        for (size_t i = 0; i < pal.size() && color_index < 256; ++i) {
          ImVec4 rgb = pal[i].rgb();
          colors[color_index++] = {
              static_cast<Uint8>(rgb.x),
              static_cast<Uint8>(rgb.y),
              static_cast<Uint8>(rgb.z),
              255
          };
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
      if (bitmap.modified() && bitmap.surface() && bitmap.mutable_data().size() > 0) {
        SDL_LockSurface(bitmap.surface());
        size_t surface_size = bitmap.surface()->h * bitmap.surface()->pitch;
        size_t data_size = bitmap.mutable_data().size();
        if (surface_size >= data_size) {
          memcpy(bitmap.surface()->pixels, bitmap.mutable_data().data(), data_size);
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

void ObjectEditorPanel::CloseStaticObjectEditor() {
  static_editor_open_ = false;
  static_editor_object_id_ = -1;

  // Clear the visual indicator in object selector
  object_selector_.SetStaticEditorObjectId(-1);
}

void ObjectEditorPanel::DrawStaticObjectEditor() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::PushStyleColor(
      ImGuiCol_Header, ImVec4(0.15f, 0.25f, 0.35f, 1.0f));  // Slate blue header
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                        ImVec4(0.20f, 0.30f, 0.40f, 1.0f));

  bool header_open = ImGui::CollapsingHeader(
      absl::StrFormat(ICON_MD_CONSTRUCTION " Object 0x%02X - %s",
                      static_editor_object_id_,
                      static_editor_draw_info_.routine_name.c_str())
          .c_str(),
      ImGuiTreeNodeFlags_DefaultOpen);

  ImGui::PopStyleColor(2);

  if (header_open) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));

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
          // TODO: Implement ASM export (Phase 5)
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Export object draw routine as ASM (Phase 5)");
        }

        ImGui::Spacing();

        // Close button at bottom
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(0.6f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button(ICON_MD_CLOSE " Close", ImVec2(-1, 0))) {
          CloseStaticObjectEditor();
        }
        ImGui::PopStyleColor(2);
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

    ImGui::PopStyleVar();
  }
}

// =============================================================================
// Keyboard Shortcuts
// =============================================================================

void ObjectEditorPanel::HandleKeyboardShortcuts() {
  if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    return;
  }

  const ImGuiIO& io = ImGui::GetIO();

  // Ctrl+A: Select all objects
  if (ImGui::IsKeyPressed(ImGuiKey_A) && io.KeyCtrl && !io.KeyShift) {
    SelectAllObjects();
  }

  // Ctrl+Shift+A: Deselect all
  if (ImGui::IsKeyPressed(ImGuiKey_A) && io.KeyCtrl && io.KeyShift) {
    DeselectAllObjects();
  }

  // Delete: Remove selected objects
  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    DeleteSelectedObjects();
  }

  // Ctrl+D: Duplicate selected objects
  if (ImGui::IsKeyPressed(ImGuiKey_D) && io.KeyCtrl) {
    DuplicateSelectedObjects();
  }

  // Ctrl+C: Copy selected objects
  if (ImGui::IsKeyPressed(ImGuiKey_C) && io.KeyCtrl) {
    CopySelectedObjects();
  }

  // Ctrl+V: Paste objects
  if (ImGui::IsKeyPressed(ImGuiKey_V) && io.KeyCtrl) {
    PasteObjects();
  }

  // Ctrl+Z: Undo
  if (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl && !io.KeyShift) {
    if (object_editor_) {
      object_editor_->Undo();
    }
  }

  // Ctrl+Shift+Z or Ctrl+Y: Redo
  if ((ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl && io.KeyShift) ||
      (ImGui::IsKeyPressed(ImGuiKey_Y) && io.KeyCtrl)) {
    if (object_editor_) {
      object_editor_->Redo();
    }
  }

  // G: Toggle grid
  if (ImGui::IsKeyPressed(ImGuiKey_G) && !io.KeyCtrl) {
    show_grid_ = !show_grid_;
  }

  // I: Toggle object ID labels
  if (ImGui::IsKeyPressed(ImGuiKey_I) && !io.KeyCtrl) {
    show_object_ids_ = !show_object_ids_;
  }

  // Arrow keys: Nudge selected objects
  if (!io.KeyCtrl) {
    int dx = 0, dy = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
      dx = -1;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
      dx = 1;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
      dy = -1;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
      dy = 1;

    if (dx != 0 || dy != 0) {
      NudgeSelectedObjects(dx, dy);
    }
  }

  // Tab: Cycle through objects
  if (ImGui::IsKeyPressed(ImGuiKey_Tab) && !io.KeyCtrl) {
    CycleObjectSelection(io.KeyShift ? -1 : 1);
  }

  // Escape: Cancel placement or deselect all
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    if (has_preview_object_ && canvas_viewer_ &&
        canvas_viewer_->object_interaction().IsObjectLoaded()) {
      CancelPlacement();
    } else {
      DeselectAllObjects();
    }
  }
}

void ObjectEditorPanel::CancelPlacement() {
  has_preview_object_ = false;
  if (canvas_viewer_) {
    canvas_viewer_->ClearPreviewObject();
    canvas_viewer_->object_interaction().CancelPlacement();
  }
}

void ObjectEditorPanel::SelectAllObjects() {
  if (!canvas_viewer_ || !object_editor_)
    return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& objects = object_editor_->GetObjects();
  std::vector<size_t> all_indices;

  for (size_t i = 0; i < objects.size(); ++i) {
    all_indices.push_back(i);
  }

  interaction.SetSelectedObjects(all_indices);
}

void ObjectEditorPanel::DeselectAllObjects() {
  if (!canvas_viewer_)
    return;
  canvas_viewer_->object_interaction().ClearSelection();
}

void ObjectEditorPanel::DeleteSelectedObjects() {
  if (!object_editor_ || !canvas_viewer_)
    return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty())
    return;

  // Show confirmation for bulk delete (more than 5 objects)
  if (selected.size() > 5) {
    show_delete_confirmation_modal_ = true;
    return;
  }

  PerformDelete();
}

void ObjectEditorPanel::PerformDelete() {
  if (!object_editor_ || !canvas_viewer_)
    return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty())
    return;

  // Delete in reverse order to maintain indices
  std::vector<size_t> sorted_indices(selected.begin(), selected.end());
  std::sort(sorted_indices.rbegin(), sorted_indices.rend());

  for (size_t idx : sorted_indices) {
    object_editor_->DeleteObject(idx);
  }

  interaction.ClearSelection();
}

void ObjectEditorPanel::DuplicateSelectedObjects() {
  if (!object_editor_ || !canvas_viewer_)
    return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty())
    return;

  std::vector<size_t> new_indices;

  for (size_t idx : selected) {
    auto new_idx = object_editor_->DuplicateObject(idx, 1, 1);
    if (new_idx.has_value()) {
      new_indices.push_back(*new_idx);
    }
  }

  interaction.SetSelectedObjects(new_indices);
}

void ObjectEditorPanel::CopySelectedObjects() {
  if (!object_editor_ || !canvas_viewer_)
    return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty())
    return;

  object_editor_->CopySelectedObjects(selected);
}

void ObjectEditorPanel::PasteObjects() {
  if (!object_editor_ || !canvas_viewer_)
    return;

  auto new_indices = object_editor_->PasteObjects();

  if (!new_indices.empty()) {
    canvas_viewer_->object_interaction().SetSelectedObjects(new_indices);
  }
}

void ObjectEditorPanel::NudgeSelectedObjects(int dx, int dy) {
  if (!object_editor_ || !canvas_viewer_)
    return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty())
    return;

  for (size_t idx : selected) {
    object_editor_->MoveObject(idx, dx, dy);
  }
}

void ObjectEditorPanel::CycleObjectSelection(int direction) {
  if (!canvas_viewer_ || !object_editor_)
    return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();
  const auto& objects = object_editor_->GetObjects();

  size_t total_objects = objects.size();
  if (total_objects == 0)
    return;

  size_t current_idx = selected.empty() ? 0 : selected.front();
  size_t next_idx = (current_idx + direction + total_objects) % total_objects;

  interaction.SetSelectedObjects({next_idx});
}

void ObjectEditorPanel::ScrollToObject(size_t index) {
  if (!canvas_viewer_ || !object_editor_)
    return;

  const auto& objects = object_editor_->GetObjects();
  if (index >= objects.size())
    return;

  // TODO: Implement ScrollTo in DungeonCanvasViewer
  (void)objects;
}

}  // namespace editor
}  // namespace yaze
