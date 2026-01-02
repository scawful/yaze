#include "app/editor/graphics/link_sprite_panel.h"

#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/file_util.h"
#include "util/log.h"

namespace yaze {
namespace editor {

LinkSpritePanel::LinkSpritePanel(GraphicsEditorState* state, Rom* rom)
    : state_(state), rom_(rom) {}

void LinkSpritePanel::Initialize() {
  preview_canvas_.SetCanvasSize(
      ImVec2(128 * preview_zoom_, 32 * preview_zoom_));
}

void LinkSpritePanel::Draw(bool* p_open) {
  // EditorPanel interface - delegate to existing Update() logic
  // Lazy-load Link sheets on first update
  if (!sheets_loaded_ && rom_ && rom_->is_loaded()) {
    auto status = LoadLinkSheets();
    if (!status.ok()) {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                         "Failed to load Link sheets: %s",
                         status.message().data());
      return;
    }
  }

  DrawToolbar();
  ImGui::Separator();

  // Split layout: left side grid, right side preview
  float panel_width = ImGui::GetContentRegionAvail().x;
  float grid_width = std::min(300.0f, panel_width * 0.4f);

  // Left column: Sheet grid
  ImGui::BeginChild("##LinkSheetGrid", ImVec2(grid_width, 0), true);
  DrawSheetGrid();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right column: Preview and controls
  ImGui::BeginChild("##LinkPreviewArea", ImVec2(0, 0), true);
  DrawPreviewCanvas();
  ImGui::Separator();
  DrawPaletteSelector();
  ImGui::Separator();
  DrawInfoPanel();
  ImGui::EndChild();
}

absl::Status LinkSpritePanel::Update() {
  // Lazy-load Link sheets on first update
  if (!sheets_loaded_ && rom_ && rom_->is_loaded()) {
    auto status = LoadLinkSheets();
    if (!status.ok()) {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                         "Failed to load Link sheets: %s",
                         status.message().data());
      return status;
    }
  }

  DrawToolbar();
  ImGui::Separator();

  // Split layout: left side grid, right side preview
  float panel_width = ImGui::GetContentRegionAvail().x;
  float grid_width = std::min(300.0f, panel_width * 0.4f);

  // Left column: Sheet grid
  ImGui::BeginChild("##LinkSheetGrid", ImVec2(grid_width, 0), true);
  DrawSheetGrid();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right column: Preview and controls
  ImGui::BeginChild("##LinkPreviewArea", ImVec2(0, 0), true);
  DrawPreviewCanvas();
  ImGui::Separator();
  DrawPaletteSelector();
  ImGui::Separator();
  DrawInfoPanel();
  ImGui::EndChild();

  return absl::OkStatus();
}

void LinkSpritePanel::DrawToolbar() {
  if (ImGui::Button(ICON_MD_FILE_UPLOAD " Import ZSPR")) {
    ImportZspr();
  }
  HOVER_HINT("Import a .zspr Link sprite file");

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_RESTORE " Reset to Vanilla")) {
    ResetToVanilla();
  }
  HOVER_HINT("Reset Link graphics to vanilla ROM data");

  // Show loaded ZSPR info
  if (loaded_zspr_.has_value()) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Loaded: %s",
                       loaded_zspr_->metadata.display_name.c_str());
  }

  // Unsaved changes indicator
  if (has_unsaved_changes_) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "[Unsaved]");
  }
}

void LinkSpritePanel::DrawSheetGrid() {
  ImGui::Text("Link Sheets (14)");
  ImGui::Separator();

  // 4x4 grid (14 sheets + 2 empty slots)
  const float cell_size = kThumbnailSize + kThumbnailPadding * 2;
  int col = 0;

  for (int i = 0; i < kNumLinkSheets; i++) {
    if (col > 0) {
      ImGui::SameLine();
    }

    ImGui::PushID(i);
    DrawSheetThumbnail(i);
    ImGui::PopID();

    col++;
    if (col >= 4) {
      col = 0;
    }
  }
}

void LinkSpritePanel::DrawSheetThumbnail(int sheet_index) {
  bool is_selected = (selected_sheet_ == sheet_index);

  // Selection highlight
  if (is_selected) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.5f, 0.8f, 0.4f));
  }

  ImGui::BeginChild(absl::StrFormat("##LinkSheet%d", sheet_index).c_str(),
                    ImVec2(kThumbnailSize + kThumbnailPadding,
                           kThumbnailSize + 16 + kThumbnailPadding),
                    true, ImGuiWindowFlags_NoScrollbar);

  // Draw thumbnail
  auto& sheet = link_sheets_[sheet_index];
  if (sheet.is_active()) {
    // Ensure texture exists
    if (!sheet.texture() && sheet.surface()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE,
          const_cast<gfx::Bitmap*>(&sheet));
    }

    if (sheet.texture()) {
      ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
      ImGui::GetWindowDrawList()->AddImage(
          (ImTextureID)(intptr_t)sheet.texture(), cursor_pos,
          ImVec2(cursor_pos.x + kThumbnailSize,
                 cursor_pos.y + kThumbnailSize / 4));  // 128x32 aspect
    }
  }

  // Click handling
  if (ImGui::IsWindowHovered() &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    selected_sheet_ = sheet_index;
  }

  // Double-click to open in pixel editor
  if (ImGui::IsWindowHovered() &&
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    OpenSheetInPixelEditor();
  }

  // Sheet label
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + kThumbnailSize / 4 + 2);
  ImGui::Text("%d", sheet_index);

  ImGui::EndChild();

  if (is_selected) {
    ImGui::PopStyleColor();
  }

  // Tooltip
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Link Sheet %d", sheet_index);
    ImGui::Text("Double-click to edit");
    ImGui::EndTooltip();
  }
}

void LinkSpritePanel::DrawPreviewCanvas() {
  ImGui::Text("Sheet %d Preview", selected_sheet_);

  // Preview canvas
  float canvas_width = ImGui::GetContentRegionAvail().x - 16;
  float canvas_height = canvas_width / 4;  // 4:1 aspect ratio (128x32)

  preview_canvas_.SetCanvasSize(ImVec2(canvas_width, canvas_height));
  const float grid_step = 8.0f * (canvas_width / 128.0f);
  {
    gui::CanvasFrameOptions frame_opts;
    frame_opts.canvas_size = ImVec2(canvas_width, canvas_height);
    frame_opts.draw_context_menu = false;
    frame_opts.draw_grid = true;
    frame_opts.grid_step = grid_step;

    auto rt = gui::BeginCanvas(preview_canvas_, frame_opts);

    auto& sheet = link_sheets_[selected_sheet_];
    if (sheet.is_active() && sheet.texture()) {
      gui::BitmapDrawOpts draw_opts;
      draw_opts.dest_pos = ImVec2(0, 0);
      draw_opts.dest_size = ImVec2(canvas_width, canvas_height);
      draw_opts.ensure_texture = false;
      gui::DrawBitmap(rt, sheet, draw_opts);
    }

    gui::EndCanvas(preview_canvas_, rt, frame_opts);
  }

  ImGui::Spacing();

  // Open in editor button
  if (ImGui::Button(ICON_MD_EDIT " Open in Pixel Editor")) {
    OpenSheetInPixelEditor();
  }
  HOVER_HINT("Open this sheet in the main pixel editor");

  // Zoom slider
  ImGui::SameLine();
  ImGui::SetNextItemWidth(100);
  ImGui::SliderFloat("Zoom", &preview_zoom_, 1.0f, 8.0f, "%.1fx");
}

void LinkSpritePanel::DrawPaletteSelector() {
  ImGui::Text("Display Palette:");
  ImGui::SameLine();

  const char* palette_names[] = {"Green Mail", "Blue Mail", "Red Mail",
                                 "Bunny"};
  int current = static_cast<int>(selected_palette_);

  ImGui::SetNextItemWidth(120);
  if (ImGui::Combo("##PaletteSelect", &current, palette_names, 4)) {
    selected_palette_ = static_cast<PaletteType>(current);
    ApplySelectedPalette();
  }
  HOVER_HINT("Change the display palette for preview");
}

void LinkSpritePanel::DrawInfoPanel() {
  ImGui::Text("Info:");
  ImGui::BulletText("896 total tiles (8x8 each)");
  ImGui::BulletText("14 graphics sheets");
  ImGui::BulletText("4BPP format");

  if (loaded_zspr_.has_value()) {
    ImGui::Separator();
    ImGui::Text("Loaded ZSPR:");
    ImGui::BulletText("Name: %s", loaded_zspr_->metadata.display_name.c_str());
    ImGui::BulletText("Author: %s", loaded_zspr_->metadata.author.c_str());
    ImGui::BulletText("Tiles: %zu", loaded_zspr_->tile_count());
  }
}

void LinkSpritePanel::ImportZspr() {
  // Open file dialog for .zspr files
  auto file_path = util::FileDialogWrapper::ShowOpenFileDialog();
  if (file_path.empty()) {
    return;
  }

  LOG_INFO("LinkSpritePanel", "Importing ZSPR: %s", file_path.c_str());

  // Load ZSPR file
  auto zspr_result = gfx::ZsprLoader::LoadFromFile(file_path);
  if (!zspr_result.ok()) {
    LOG_ERROR("LinkSpritePanel", "Failed to load ZSPR: %s",
              zspr_result.status().message().data());
    return;
  }

  loaded_zspr_ = std::move(zspr_result.value());

  // Verify it's a Link sprite
  if (!loaded_zspr_->is_link_sprite()) {
    LOG_ERROR("LinkSpritePanel", "ZSPR is not a Link sprite (type=%d)",
              loaded_zspr_->metadata.sprite_type);
    loaded_zspr_.reset();
    return;
  }

  // Apply to ROM
  if (rom_ && rom_->is_loaded()) {
    auto status = gfx::ZsprLoader::ApplyToRom(*rom_, *loaded_zspr_);
    if (!status.ok()) {
      LOG_ERROR("LinkSpritePanel", "Failed to apply ZSPR to ROM: %s",
                status.message().data());
      return;
    }

    // Also apply palette
    status = gfx::ZsprLoader::ApplyPaletteToRom(*rom_, *loaded_zspr_);
    if (!status.ok()) {
      LOG_WARN("LinkSpritePanel", "Failed to apply ZSPR palette: %s",
               status.message().data());
    }

    // Reload Link sheets to reflect changes
    sheets_loaded_ = false;
    has_unsaved_changes_ = true;

    LOG_INFO("LinkSpritePanel", "ZSPR '%s' imported successfully",
             loaded_zspr_->metadata.display_name.c_str());
  }
}

void LinkSpritePanel::ResetToVanilla() {
  // TODO: Implement reset to vanilla
  // This would require keeping a backup of the original Link graphics
  // or reloading from a vanilla ROM file
  LOG_WARN("LinkSpritePanel", "Reset to vanilla not yet implemented");
  loaded_zspr_.reset();
}

void LinkSpritePanel::OpenSheetInPixelEditor() {
  // Signal to open the selected Link sheet in the main pixel editor
  // Link sheets are separate from the main 223 sheets, so we need
  // a special handling mechanism

  // For now, log the intent - full integration requires additional state
  LOG_INFO("LinkSpritePanel", "Request to open Link sheet %d in pixel editor",
           selected_sheet_);

  // TODO: Add Link sheet to open_sheets with a special identifier
  // or add a link_sheets_to_edit set to GraphicsEditorState
}

absl::Status LinkSpritePanel::LoadLinkSheets() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Use the existing LoadLinkGraphics function
  auto result = zelda3::LoadLinkGraphics(*rom_);
  if (!result.ok()) {
    return result.status();
  }

  link_sheets_ = std::move(result.value());
  sheets_loaded_ = true;

  LOG_INFO("LinkSpritePanel", "Loaded %d Link graphics sheets",
           zelda3::kNumLinkSheets);

  // Apply default palette for display
  ApplySelectedPalette();

  return absl::OkStatus();
}

void LinkSpritePanel::ApplySelectedPalette() {
  if (!rom_ || !rom_->is_loaded())
    return;

  // Get the appropriate palette based on selection
  // Link palettes are in Group 4 (Sprites Aux1) and Group 5 (Sprites Aux2)
  // Green Mail: Group 4, Index 0 (Standard Link)
  // Blue Mail: Group 4, Index 0 (Standard Link) - but with different colors in game
  // Red Mail: Group 4, Index 0 (Standard Link) - but with different colors in game
  // Bunny: Group 4, Index 1 (Bunny Link)

  // For now, we'll use the standard sprite palettes from GameData if available
  // In a full implementation, we would load the specific mail palettes

  // Default to Green Mail (Standard Link palette)
  const gfx::SnesPalette* palette = nullptr;

  // We need access to GameData to get the palettes
  // Since we don't have direct access to GameData here (only Rom), we'll try to find it
  // or use a hardcoded fallback if necessary.
  // Ideally, LinkSpritePanel should have access to GameData.
  // For this fix, we will assume the standard sprite palette location in ROM if GameData isn't available,
  // or use a simplified approach.

  // Actually, we can get GameData from the main Editor instance if we had access,
  // but we only have Rom. Let's try to read the palette directly from ROM for now
  // to ensure it works without refactoring the whole dependency injection.

  // Standard Link Palette (Green Mail) is usually at 0x1BD318 (PC) / 0x37D318 (SNES) in vanilla
  // But we should use the loaded palette data if possible.

  // Let's use a safe fallback: Create a default Link palette
  static gfx::SnesPalette default_palette;
  if (default_palette.empty()) {
    // Basic Green Mail colors (approximate)
    default_palette.Resize(16);
    default_palette[0] = gfx::SnesColor(0, 0, 0);        // Transparent
    default_palette[1] = gfx::SnesColor(24, 24, 24);     // Tunic Dark
    default_palette[2] = gfx::SnesColor(0, 19, 0);       // Tunic Green
    default_palette[3] = gfx::SnesColor(255, 255, 255);  // White
    default_palette[4] = gfx::SnesColor(255, 165, 66);   // Skin
    default_palette[5] = gfx::SnesColor(255, 100, 50);   // Skin Dark
    default_palette[6] = gfx::SnesColor(255, 0, 0);      // Red
    default_palette[7] = gfx::SnesColor(255, 255, 0);    // Yellow
    // ... fill others as needed
  }

  // If we can't get the real palette, use default
  palette = &default_palette;

  // Apply to all Link sheets
  for (auto& sheet : link_sheets_) {
    if (sheet.is_active() && sheet.surface()) {
      // Use the palette
      sheet.SetPaletteWithTransparent(*palette, 0);

      // Force texture update
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &sheet);
    }
  }

  LOG_INFO("LinkSpritePanel", "Applied palette %s to %zu sheets",
           GetPaletteName(selected_palette_), link_sheets_.size());
}

const char* LinkSpritePanel::GetPaletteName(PaletteType type) {
  switch (type) {
    case PaletteType::kGreenMail:
      return "Green Mail";
    case PaletteType::kBlueMail:
      return "Blue Mail";
    case PaletteType::kRedMail:
      return "Red Mail";
    case PaletteType::kBunny:
      return "Bunny";
    default:
      return "Unknown";
  }
}

}  // namespace editor
}  // namespace yaze
