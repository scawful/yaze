// Related header
#include "graphics_editor.h"
#include "util/i18n/tr.h"

// C++ standard library headers
#include <algorithm>
#include <filesystem>
#include <set>

// Third-party library headers
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

// Project headers
#include "app/editor/editor_manager.h"
#include "app/editor/graphics/panels/graphics_editor_panels.h"
#include "app/editor/menu/status_bar.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/compression.h"
#include "app/gfx/util/scad_format.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/imgui_memory_editor.h"
#include "app/gui/widgets/asset_browser.h"
#include "app/platform/window.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "core/graphics_sheet_labels.h"
#include "core/project.h"
#include "core/rom_settings.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "util/file_util.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze {
namespace editor {

using gfx::kPaletteGroupAddressesKeys;
using ImGui::Button;
using ImGui::InputInt;
using ImGui::InputText;
using ImGui::SameLine;

void GraphicsEditor::Initialize() {
  if (!dependencies_.window_manager)
    return;
  auto* window_manager = dependencies_.window_manager;

  // Initialize panel components
  sheet_browser_panel_ = std::make_unique<SheetBrowserPanel>(&state_);
  sheet_browser_panel_->SetDataSources(
      rom_, game_data_,
      [this]() -> const project::YazeProject* { return project(); });
  sheet_browser_panel_->SetUndoManager(&undo_manager_);
  sheet_browser_panel_->SetLabelCallbacks(
      [this](uint16_t sheet, const std::string& label) {
        return EditProject([&](project::YazeProject& target) {
          core::SetGraphicsSheetLabel(target, sheet, label);
        });
      },
      [this](const std::string& csv_text) -> absl::StatusOr<int> {
        int written = 0;
        bool counted = false;
        RETURN_IF_ERROR(EditProject([&](project::YazeProject& target) {
          const int count = core::ImportSpritesetSheetLabels(target, csv_text);
          if (!counted) {
            written = count;
            counted = true;
          }
        }));
        return written;
      });
  pixel_editor_panel_ =
      std::make_unique<PixelEditorPanel>(&state_, rom_, &undo_manager_);
  palette_controls_panel_ =
      std::make_unique<PaletteControlsPanel>(&state_, rom_);
  link_sprite_panel_ = std::make_unique<LinkSpritePanel>(&state_, rom_);
  gfx_group_panel_ = std::make_unique<GfxGroupEditor>();
  gfx_group_panel_->SetWorkspaceState(dependencies_.gfx_group_workspace);
  gfx_group_panel_->SetRom(rom_);
  gfx_group_panel_->SetGameData(game_data_);
  gfx_group_panel_->SetHostSurfaceHint(
      "Gfx Groups: blockset/roomset/spriteset selection syncs with the "
      "Overworld "
      "editor for this ROM session (each surface has its own preview "
      "canvases).");
  paletteset_panel_ = std::make_unique<PalettesetEditorPanel>();
  paletteset_panel_->SetRom(rom_);
  paletteset_panel_->SetGameData(game_data_);

  polyhedral_panel_ = std::make_unique<PolyhedralEditorPanel>(rom_);
  polyhedral_panel_->SetRom(rom_);

  sheet_browser_panel_->Initialize();
  pixel_editor_panel_->Initialize();
  palette_controls_panel_->Initialize();
  link_sprite_panel_->Initialize();

  // Register panels using WindowContent system with callbacks
  window_manager->RegisterWindowContent(
      std::make_unique<GraphicsSheetBrowserPanel>([this]() {
        if (sheet_browser_panel_) {
          status_ = sheet_browser_panel_->Update();
        }
      }));

  window_manager->RegisterWindowContent(
      std::make_unique<GraphicsPixelEditorPanel>([this]() {
        if (pixel_editor_panel_) {
          status_ = pixel_editor_panel_->Update();
        }
      }));

  window_manager->RegisterWindowContent(
      std::make_unique<GraphicsPaletteControlsPanel>([this]() {
        if (palette_controls_panel_) {
          status_ = palette_controls_panel_->Update();
        }
      }));

  window_manager->RegisterWindowContent(
      std::make_unique<GraphicsLinkSpritePanel>([this]() {
        if (link_sprite_panel_) {
          status_ = link_sprite_panel_->Update();
        }
      }));

  window_manager->RegisterWindowContent(
      std::make_unique<GraphicsGfxGroupPanel>([this]() {
        if (gfx_group_panel_) {
          status_ = gfx_group_panel_->Update();
        }
      }));

  // Paletteset editor panel (separated from GfxGroupEditor for better UX)
  window_manager->RegisterWindowContent(
      std::make_unique<GraphicsPalettesetPanel>([this]() {
        if (paletteset_panel_) {
          status_ = paletteset_panel_->Update();
        }
      }));

  window_manager->RegisterWindowContent(
      std::make_unique<GraphicsPrototypeViewerPanel>(
          [this]() { DrawPrototypeViewer(); }));

  window_manager->RegisterWindowContent(
      std::make_unique<GraphicsPolyhedralPanel>([this]() {
        if (polyhedral_panel_) {
          bool open = true;
          polyhedral_panel_->Draw(&open);
        }
      }));
}

absl::Status GraphicsEditor::Load() {
  gfx::ScopedTimer timer("GraphicsEditor::Load");

  // Initialize all graphics sheets with appropriate palettes from ROM
  // This ensures textures are created for editing
  if (rom()->is_loaded()) {
    auto& sheets = gfx::Arena::Get().gfx_sheets();

    // Apply default palettes to all sheets based on common SNES ROM structure
    // Sheets 0-112: Use overworld/dungeon palettes
    // Sheets 113-127: Use sprite palettes
    // Sheets 128-222: Use auxiliary/menu palettes

    LOG_INFO("GraphicsEditor", "Initializing textures for %d graphics sheets",
             zelda3::kNumGfxSheets);

    int sheets_queued = 0;
    for (int i = 0; i < zelda3::kNumGfxSheets; i++) {
      if (!sheets[i].is_active() || !sheets[i].surface()) {
        continue;  // Skip inactive or surface-less sheets
      }

      // Palettes are now applied during ROM loading in LoadAllGraphicsData()
      // Just queue texture creation for sheets that don't have textures yet
      if (!sheets[i].texture()) {
        // Fix: Ensure default palettes are applied if missing
        // This handles the case where sheets are loaded but have no palette assigned
        if (sheets[i].palette().empty()) {
          // Default palette assignment logic
          if (i <= 112) {
            // Overworld/Dungeon sheets - use Dungeon Main palette (Group 0, Index 0)
            if (game_data() &&
                game_data()->palette_groups.dungeon_main.size() > 0) {
              sheets[i].SetPaletteWithTransparent(
                  game_data()->palette_groups.dungeon_main.palette(0), 0);
            }
          } else if (i >= 113 && i <= 127) {
            // Sprite sheets - use Sprites Aux1 palette (Group 4, Index 0)
            if (game_data() &&
                game_data()->palette_groups.sprites_aux1.size() > 0) {
              sheets[i].SetPaletteWithTransparent(
                  game_data()->palette_groups.sprites_aux1.palette(0), 0);
            }
          } else {
            // Menu/Aux sheets - use HUD palette if available, or fallback
            if (game_data() && game_data()->palette_groups.hud.size() > 0) {
              sheets[i].SetPaletteWithTransparent(
                  game_data()->palette_groups.hud.palette(0), 0);
            }
          }
        }

        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE, &sheets[i]);
        sheets_queued++;
      }
    }

    LOG_INFO("GraphicsEditor", "Queued texture creation for %d graphics sheets",
             sheets_queued);
  }

  return absl::OkStatus();
}

void GraphicsEditor::ContributeStatus(StatusBar* status_bar) {
  if (!status_bar)
    return;

  StatusBarSegmentOptions sheet_opts;
  sheet_opts.tooltip = absl::StrFormat(
      "Sheet %d (0x%02X) — use Command Palette to jump between sheets",
      state_.current_sheet_id, state_.current_sheet_id);
  status_bar->SetCustomSegment(
      "Sheet", absl::StrFormat("0x%02X", state_.current_sheet_id),
      std::move(sheet_opts));

  if (!state_.selected_sheets.empty()) {
    status_bar->SetSelection(static_cast<int>(state_.selected_sheets.size()));
  }
  if (state_.HasUnsavedChanges()) {
    StatusBarSegmentOptions modified_opts;
    modified_opts.tooltip = absl::StrFormat(
        "%zu modified sheet%s pending save", state_.modified_sheets.size(),
        state_.modified_sheets.size() == 1 ? "" : "s");
    status_bar->SetCustomSegment(
        "Modified", absl::StrFormat("%zu", state_.modified_sheets.size()),
        std::move(modified_opts));
  }
}

absl::Status GraphicsEditor::EditProject(
    const std::function<void(project::YazeProject&)>& edit) {
  auto* editor_manager = static_cast<EditorManager*>(dependencies_.custom_data);
  project::YazeProject* snapshot = project();
  if (editor_manager == nullptr || snapshot == nullptr ||
      !editor_manager->IsCurrentProjectContextOwnedBySession(
          dependencies_.session_id)) {
    return absl::FailedPreconditionError(
        "Sheet labels need the project that owns this ROM to be active");
  }
  project::YazeProject* active = editor_manager->GetCurrentProject();
  if (active == nullptr || !active->project_opened()) {
    return absl::FailedPreconditionError(
        "Open a .yaze project to store sheet labels");
  }
  edit(*active);
  if (snapshot != active) {
    edit(*snapshot);
  }
  editor_manager->MarkCurrentProjectDirty();
  return absl::OkStatus();
}

absl::Status GraphicsEditor::Save() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Only save sheets that have been modified
  if (!state_.HasUnsavedChanges()) {
    LOG_INFO("GraphicsEditor", "No modified sheets to save");
    return absl::OkStatus();
  }
  if (game_data() == nullptr) {
    return absl::FailedPreconditionError("Game data not loaded");
  }
  // The edited sheets live in the shared Arena, which holds the last loaded
  // ROM's sheets. Never write another open ROM's pixels into this one.
  if (gfx::Arena::Get().gfx_sheets_owner() != game_data()) {
    return absl::FailedPreconditionError(
        "Graphics sheets in memory belong to another open ROM. Switch back to "
        "the ROM they were edited in, or reopen this ROM, before saving "
        "graphics");
  }

  LOG_INFO("GraphicsEditor", "Saving %zu modified graphics sheets",
           state_.modified_sheets.size());

  zelda3::GfxSheetWritePolicy policy;
  if (const auto* project = this->project(); project != nullptr) {
    ASSIGN_OR_RETURN(policy, core::BuildGfxSheetWritePolicy(*project));
  }

  // Refuse the whole batch before writing when any sheet cannot be saved, so
  // a save never lands only part of the user's edits.
  auto& sheets = gfx::Arena::Get().gfx_sheets();
  std::vector<std::string> refused;
  for (uint16_t sheet_id : state_.modified_sheets) {
    if (sheet_id >= zelda3::kNumGfxSheets) {
      refused.push_back(absl::StrFormat("0x%02X (out of range)", sheet_id));
    } else if (policy.reserved_sheets.count(sheet_id) != 0) {
      refused.push_back(
          absl::StrFormat("0x%02X (reserved by the project)", sheet_id));
    } else if (zelda3::GetGfxSheetStorageKind(sheet_id) ==
               zelda3::GfxSheetStorageKind::kCompressed2bpp) {
      refused.push_back(absl::StrFormat("0x%02X (2bpp, read-only)", sheet_id));
    } else if (!sheets[sheet_id].is_active()) {
      refused.push_back(absl::StrFormat("0x%02X (not loaded)", sheet_id));
    } else if (std::any_of(sheets[sheet_id].vector().begin(),
                           sheets[sheet_id].vector().end(),
                           [](uint8_t index) { return index > 7; })) {
      // 3bpp sheets store colors 0-7; packing masks higher indices silently.
      refused.push_back(
          absl::StrFormat("0x%02X (uses colors above index 7)", sheet_id));
    }
  }
  if (!refused.empty()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Graphics save refused for sheet(s) %s. Discard those edits to save "
        "the rest.",
        absl::StrJoin(refused, ", ")));
  }

  const auto version_constants =
      zelda3::kVersionConstantsMap.at(game_data()->version);
  zelda3::GfxSheetPointerTables tables;
  tables.bank = core::RomSettings::Get().GetAddressOr(
      core::RomAddressKey::kOverworldGfxPtr1,
      version_constants.kOverworldGfxPtr1);
  tables.high = core::RomSettings::Get().GetAddressOr(
      core::RomAddressKey::kOverworldGfxPtr2,
      version_constants.kOverworldGfxPtr2);
  tables.low = core::RomSettings::Get().GetAddressOr(
      core::RomAddressKey::kOverworldGfxPtr3,
      version_constants.kOverworldGfxPtr3);

  // Each WriteGfxSheet call restores its own bytes on failure; this snapshot
  // also undoes sheets written earlier in the batch.
  const std::vector<uint8_t> rom_snapshot = rom_->vector();
  const bool rom_was_dirty = rom_->dirty();
  std::set<uint16_t> saved_sheets;
  for (uint16_t sheet_id : state_.modified_sheets) {
    const auto snes_data =
        gfx::IndexedToSnesSheet(sheets[sheet_id].vector(), /*bpp=*/3);
    auto result =
        zelda3::WriteGfxSheet(*rom_, sheet_id, snes_data, policy, tables);
    if (!result.ok()) {
      rom_->mutable_vector() = rom_snapshot;
      rom_->set_dirty(rom_was_dirty);
      return absl::Status(result.status().code(),
                          absl::StrFormat("Graphics sheet 0x%02X not saved: %s",
                                          sheet_id, result.status().message()));
    }
    LOG_INFO("GraphicsEditor", "Saved sheet %02X (%zu bytes, %s) at 0x%06X",
             sheet_id, result->new_stored_size,
             result->placement == zelda3::GfxSheetPlacement::kRelocated
                 ? "relocated"
                 : "in place",
             result->new_pc);
    saved_sheets.insert(sheet_id);
  }

  // Clear modified tracking after successful save
  state_.ClearModifiedSheets(saved_sheets);
  return absl::OkStatus();
}

absl::Status GraphicsEditor::Update() {
  // Panels are now drawn via WorkspaceWindowManager::DrawAllVisiblePanels()
  // This Update() only handles editor-level state and keyboard shortcuts

  // Handle editor-level keyboard shortcuts
  HandleEditorShortcuts();

  CLEAR_AND_RETURN_STATUS(status_)
  return absl::OkStatus();
}

absl::Status GraphicsEditor::Undo() {
  return undo_manager_.Undo();
}

absl::Status GraphicsEditor::Redo() {
  return undo_manager_.Redo();
}

void GraphicsEditor::HandleEditorShortcuts() {
  // Skip if ImGui wants keyboard input
  if (ImGui::GetIO().WantTextInput) {
    return;
  }

  // Tool shortcuts (only when graphics editor is active)
  if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
    state_.SetTool(PixelTool::kSelect);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_B, false)) {
    state_.SetTool(PixelTool::kPencil);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
    state_.SetTool(PixelTool::kEraser);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_G, false) && !ImGui::GetIO().KeyCtrl) {
    state_.SetTool(PixelTool::kFill);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_I, false)) {
    state_.SetTool(PixelTool::kEyedropper);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_L, false) && !ImGui::GetIO().KeyCtrl) {
    state_.SetTool(PixelTool::kLine);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !ImGui::GetIO().KeyCtrl) {
    state_.SetTool(PixelTool::kRectangle);
  }

  // Zoom shortcuts
  if (ImGui::IsKeyPressed(ImGuiKey_Equal, false) ||
      ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)) {
    state_.ZoomIn();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) ||
      ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)) {
    state_.ZoomOut();
  }

  // Grid toggle (Ctrl+G)
  if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false)) {
    state_.show_grid = !state_.show_grid;
  }

  // Sheet navigation
  if (ImGui::IsKeyPressed(ImGuiKey_PageDown, false)) {
    NextSheet();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_PageUp, false)) {
    PrevSheet();
  }
}

void GraphicsEditor::DrawPrototypeViewer() {
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::TextWrapped(tr(
        "No ROM loaded — CGX/SCR/COL/BIN and clipboard tools work without one. "
        "Load a ROM when you want vanilla palette presets or to save graphics "
        "back into a cartridge image."));
    ImGui::Spacing();
  }
  if (!prototype_import_feedback_.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s",
                       prototype_import_feedback_.c_str());
    if (ImGui::SmallButton(tr("Dismiss##prototype_import_feedback"))) {
      prototype_import_feedback_.clear();
    }
    ImGui::Separator();
  }

  if (open_memory_editor_) {
    ImGui::Begin("Memory Editor", &open_memory_editor_);
    status_ = DrawMemoryEditor();
    ImGui::End();
  }

  constexpr ImGuiTableFlags kGfxEditFlags = ImGuiTableFlags_Reorderable |
                                            ImGuiTableFlags_Resizable |
                                            ImGuiTableFlags_SizingStretchSame;

  BEGIN_TABLE("#gfxEditTable", 4, kGfxEditFlags)
  SETUP_COLUMN("File Import (BIN, CGX, ROM)")
  SETUP_COLUMN("Palette (COL)")
  ImGui::TableSetupColumn("Tilemaps and Objects (SCR, PNL, OBJ)",
                          ImGuiTableColumnFlags_WidthFixed);
  SETUP_COLUMN("Graphics Preview")
  TABLE_HEADERS()
  NEXT_COLUMN() {
    status_ = DrawCgxImport();
    status_ = DrawClipboardImport();
    status_ = DrawFileImport();
    status_ = DrawExperimentalFeatures();
  }

  NEXT_COLUMN() {
    status_ = DrawPaletteControls();
  }

  NEXT_COLUMN()
  scr_canvas_.GetConfig().role = gui::CanvasRole::kCompositeOutput;
  gui::BitmapCanvasPipeline(scr_canvas_, scr_bitmap_, 0x200, 0x200, 0x20,
                            scr_loaded_, false, 0);
  status_ = DrawScrImport();

  NEXT_COLUMN()
  if (super_donkey_) {
    // Super Donkey prototype graphics
    for (size_t i = 0; i < num_sheets_to_load_ && i < gfx_sheets_.size(); i++) {
      if (gfx_sheets_[i].is_active() && gfx_sheets_[i].texture()) {
        ImGui::Image((ImTextureID)(intptr_t)gfx_sheets_[i].texture(),
                     ImVec2(128, 32));
        if ((i + 1) % 4 != 0) {
          ImGui::SameLine();
        }
      }
    }
  } else if (cgx_loaded_ && col_file_) {
    // Load the CGX graphics
    import_canvas_.GetConfig().role = gui::CanvasRole::kCompositeOutput;
    gui::BitmapCanvasPipeline(import_canvas_, cgx_bitmap_, 0x100, 16384, 0x20,
                              cgx_loaded_, true, 5);
  } else {
    // Load the BIN/Clipboard Graphics
    import_canvas_.GetConfig().role = gui::CanvasRole::kCompositeOutput;
    gui::BitmapCanvasPipeline(import_canvas_, bin_bitmap_, 0x100, 16384, 0x20,
                              gfx_loaded_, true, 2);
  }
  END_TABLE()
}

// =============================================================================
// Prototype Viewer Import Methods
// =============================================================================

absl::Status GraphicsEditor::DrawCgxImport() {
  gui::TextWithSeparators("Cgx Import");
  InputInt(tr("BPP"), &current_bpp_);

  InputText("##CGXFile", &cgx_file_name_);
  SameLine();

  if (ImGui::Button(tr("Open CGX"))) {
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
    cgx_file_name_ = filename;
    cgx_file_path_ = std::filesystem::absolute(filename).string();
    is_open_ = true;
    cgx_loaded_ = true;
  }

  if (ImGui::Button(tr("Copy CGX Path"))) {
    ImGui::SetClipboardText(cgx_file_path_.c_str());
  }

  if (ImGui::Button(tr("Load CGX Data"))) {
    status_ = gfx::LoadCgx(current_bpp_, cgx_file_path_, cgx_data_,
                           decoded_cgx_, extra_cgx_data_);
    if (!status_.ok()) {
      prototype_import_feedback_ = absl::StrCat("[CGX] ", status_.message());
      return absl::OkStatus();
    }
    prototype_import_feedback_.clear();

    cgx_bitmap_.Create(0x80, 0x200, 8, decoded_cgx_);
    if (col_file_) {
      cgx_bitmap_.SetPalette(decoded_col_);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &cgx_bitmap_);
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawScrImport() {
  InputText("##ScrFile", &scr_file_name_);

  if (ImGui::Button(tr("Open SCR"))) {
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
    scr_file_name_ = filename;
    scr_file_path_ = std::filesystem::absolute(filename).string();
    is_open_ = true;
    scr_loaded_ = true;
  }

  InputInt(tr("SCR Mod"), &scr_mod_value_);

  if (ImGui::Button(tr("Load Scr Data"))) {
    status_ = gfx::LoadScr(scr_file_path_, scr_mod_value_, scr_data_);
    if (!status_.ok()) {
      prototype_import_feedback_ = absl::StrCat("[SCR] ", status_.message());
      return absl::OkStatus();
    }

    decoded_scr_data_.resize(0x100 * 0x100);
    status_ = gfx::DrawScrWithCgx(current_bpp_, scr_data_, decoded_scr_data_,
                                  decoded_cgx_);
    if (!status_.ok()) {
      prototype_import_feedback_ =
          absl::StrCat("[SCR draw] ", status_.message());
      return absl::OkStatus();
    }
    prototype_import_feedback_.clear();

    scr_bitmap_.Create(0x100, 0x100, 8, decoded_scr_data_);
    if (scr_loaded_) {
      scr_bitmap_.SetPalette(decoded_col_);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &scr_bitmap_);
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawPaletteControls() {
  gui::TextWithSeparators("COL Import");
  InputText("##ColFile", &col_file_name_);
  SameLine();

  if (ImGui::Button(tr("Open COL"))) {
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
    col_file_name_ = filename;
    col_file_path_ = std::filesystem::absolute(filename).string();
    status_ = temp_rom_.LoadFromFile(col_file_path_);
    auto col_data_ = gfx::GetColFileData(temp_rom_.mutable_data());
    if (col_file_palette_group_.size() != 0) {
      col_file_palette_group_.clear();
    }
    auto col_file_palette_group_status =
        gfx::CreatePaletteGroupFromColFile(col_data_);
    if (col_file_palette_group_status.ok()) {
      col_file_palette_group_ = col_file_palette_group_status.value();
    }
    col_file_palette_ = gfx::SnesPalette(col_data_);

    // gigaleak dev format based code
    decoded_col_ = gfx::DecodeColFile(col_file_path_);
    col_file_ = true;
    is_open_ = true;
  }
  HOVER_HINT(".COL, .BAK");

  if (ImGui::Button(tr("Copy Col Path"))) {
    ImGui::SetClipboardText(col_file_path_.c_str());
  }

  if (rom()->is_loaded()) {
    gui::TextWithSeparators("ROM Palette");
    gui::InputHex("Palette Index", &current_palette_index_);
    ImGui::Combo(tr("Palette"), &current_palette_, kPaletteGroupAddressesKeys,
                 IM_ARRAYSIZE(kPaletteGroupAddressesKeys));
  }

  if (col_file_palette_.size() != 0) {
    gui::SelectablePalettePipeline(current_palette_index_, refresh_graphics_,
                                   col_file_palette_);
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawObjImport() {
  gui::TextWithSeparators("OBJ Import");

  InputText("##ObjFile", &obj_file_path_);
  SameLine();

  if (ImGui::Button(tr("Open OBJ"))) {
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
    obj_file_path_ = std::filesystem::absolute(filename).string();
    status_ = temp_rom_.LoadFromFile(obj_file_path_);
    is_open_ = true;
    obj_loaded_ = true;
  }
  HOVER_HINT(".OBJ, .BAK");

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawTilemapImport() {
  gui::TextWithSeparators("Tilemap Import");

  InputText("##TMapFile", &tilemap_file_path_);
  SameLine();

  if (ImGui::Button(tr("Open Tilemap"))) {
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
    tilemap_file_path_ = std::filesystem::absolute(filename).string();
    status_ = tilemap_rom_.LoadFromFile(tilemap_file_path_);
    status_ = tilemap_rom_.LoadFromFile(tilemap_file_path_);

    // Extract the high and low bytes from the file.
    auto decomp_sheet = gfx::lc_lz2::DecompressV2(tilemap_rom_.data(), 0, 0x800,
                                                  gfx::lc_lz2::kNintendoMode1,
                                                  tilemap_rom_.size());
    tilemap_loaded_ = true;
    is_open_ = true;
  }
  HOVER_HINT(".DAT, .BIN, .HEX");

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawFileImport() {
  gui::TextWithSeparators("BIN Import");

  InputText("##ROMFile", &file_path_);
  SameLine();

  if (ImGui::Button(tr("Open BIN"))) {
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
    file_path_ = filename;
    status_ = temp_rom_.LoadFromFile(file_path_);
    is_open_ = true;
  }
  HOVER_HINT(".BIN, .HEX");

  if (Button(tr("Copy File Path"))) {
    ImGui::SetClipboardText(file_path_.c_str());
  }

  gui::InputHex("BIN Offset", &current_offset_);
  gui::InputHex("BIN Size", &bin_size_);

  if (Button(tr("Decompress BIN"))) {
    if (file_path_.empty()) {
      return absl::InvalidArgumentError(
          "Please select a file before decompressing.");
    }
    RETURN_IF_ERROR(DecompressImportData(bin_size_))
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawClipboardImport() {
  gui::TextWithSeparators("Clipboard Import");
  if (Button(tr("Paste From Clipboard"))) {
    const char* text = ImGui::GetClipboardText();
    if (text) {
      const auto clipboard_data =
          std::vector<uint8_t>(text, text + strlen(text));
      ImGui::MemFree((void*)text);
      status_ = temp_rom_.LoadFromData(clipboard_data);
      is_open_ = true;
      open_memory_editor_ = true;
    }
  }
  gui::InputHex("Offset", &clipboard_offset_);
  gui::InputHex("Size", &clipboard_size_);
  gui::InputHex("Num Sheets", &num_sheets_to_load_);

  if (Button(tr("Decompress Clipboard Data"))) {
    if (temp_rom_.is_loaded()) {
      status_ = DecompressImportData(0x40000);
    } else {
      status_ = absl::InvalidArgumentError(
          "Please paste data into the clipboard before "
          "decompressing.");
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawExperimentalFeatures() {
  gui::TextWithSeparators("Experimental");
  if (Button(tr("Decompress Super Donkey Full"))) {
    if (file_path_.empty()) {
      return absl::InvalidArgumentError(
          "Please select `super_donkey_1.bin` before "
          "importing.");
    }
    RETURN_IF_ERROR(DecompressSuperDonkey())
  }
  ImGui::SetItemTooltip(
      tr("Requires `super_donkey_1.bin` to be imported under the "
         "BIN import section."));
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawMemoryEditor() {
  std::string title = "Memory Editor";
  if (is_open_) {
    static yaze::gui::MemoryEditorWidget mem_edit;
    mem_edit.DrawWindow(title.c_str(), temp_rom_.mutable_data(),
                        temp_rom_.size());
  }
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DecompressImportData(int size) {
  ASSIGN_OR_RETURN(import_data_,
                   gfx::lc_lz2::DecompressV2(temp_rom_.data(), current_offset_,
                                             size, 1, temp_rom_.size()));

  auto converted_sheet = gfx::SnesTo8bppSheet(import_data_, 3);
  bin_bitmap_.Create(gfx::kTilesheetWidth, 0x2000, gfx::kTilesheetDepth,
                     converted_sheet);

  if (rom()->is_loaded() && game_data()) {
    auto palette_group = game_data()->palette_groups.overworld_animated;
    z3_rom_palette_ = palette_group[current_palette_];
    if (col_file_) {
      bin_bitmap_.SetPalette(col_file_palette_);
    } else {
      bin_bitmap_.SetPalette(z3_rom_palette_);
    }
  }

  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &bin_bitmap_);
  gfx_loaded_ = true;

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DecompressSuperDonkey() {
  int i = 0;
  for (const auto& offset : kSuperDonkeyTiles) {
    int offset_value =
        std::stoi(offset, nullptr, 16);  // convert hex string to int
    ASSIGN_OR_RETURN(auto decompressed_data,
                     gfx::lc_lz2::DecompressV2(temp_rom_.data(), offset_value,
                                               0x1000, 1, temp_rom_.size()));
    auto converted_sheet = gfx::SnesTo8bppSheet(decompressed_data, 3);
    gfx_sheets_[i] = gfx::Bitmap(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                                 gfx::kTilesheetDepth, converted_sheet);
    if (col_file_) {
      gfx_sheets_[i].SetPalette(
          col_file_palette_group_[current_palette_index_]);
    } else {
      // ROM palette
      if (!game_data()) {
        return absl::FailedPreconditionError("GameData not available");
      }
      auto palette_group = game_data()->palette_groups.get_group(
          kPaletteGroupAddressesKeys[current_palette_]);
      z3_rom_palette_ = palette_group->palette(current_palette_index_);
      gfx_sheets_[i].SetPalette(z3_rom_palette_);
    }

    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &gfx_sheets_[i]);
    i++;
  }

  for (const auto& offset : kSuperDonkeySprites) {
    int offset_value =
        std::stoi(offset, nullptr, 16);  // convert hex string to int
    ASSIGN_OR_RETURN(auto decompressed_data,
                     gfx::lc_lz2::DecompressV2(temp_rom_.data(), offset_value,
                                               0x1000, 1, temp_rom_.size()));
    auto converted_sheet = gfx::SnesTo8bppSheet(decompressed_data, 3);
    gfx_sheets_[i] = gfx::Bitmap(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                                 gfx::kTilesheetDepth, converted_sheet);
    if (col_file_) {
      gfx_sheets_[i].SetPalette(
          col_file_palette_group_[current_palette_index_]);
    } else {
      // ROM palette
      if (game_data()) {
        auto palette_group = game_data()->palette_groups.get_group(
            kPaletteGroupAddressesKeys[current_palette_]);
        z3_rom_palette_ = palette_group->palette(current_palette_index_);
        gfx_sheets_[i].SetPalette(z3_rom_palette_);
      }
    }

    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &gfx_sheets_[i]);
    i++;
  }
  super_donkey_ = true;
  num_sheets_to_load_ = i;

  return absl::OkStatus();
}

void GraphicsEditor::NextSheet() {
  if (state_.current_sheet_id + 1 < zelda3::kNumGfxSheets) {
    state_.current_sheet_id++;
  }
}

void GraphicsEditor::PrevSheet() {
  if (state_.current_sheet_id > 0) {
    state_.current_sheet_id--;
  }
}

void GraphicsEditor::SelectSheet(uint16_t sheet_id) {
  if (sheet_id >= zelda3::kNumGfxSheets) {
    return;
  }
  state_.SelectSheet(sheet_id);
}

void GraphicsEditor::HighlightTile(uint16_t sheet_id, uint16_t tile_index,
                                   const std::string& label,
                                   double duration_secs) {
  if (sheet_id >= zelda3::kNumGfxSheets) {
    return;
  }
  state_.HighlightTile(sheet_id, tile_index, label, duration_secs);
}

}  // namespace editor
}  // namespace yaze
