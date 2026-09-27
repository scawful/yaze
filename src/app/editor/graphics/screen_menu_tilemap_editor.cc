#include "app/editor/graphics/screen_menu_tilemap_editor.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/graphics/screen_undo_actions.h"
#include "app/editor/registry/undo_action.h"
#include "app/editor/registry/undo_manager.h"
#include "app/emu/debug/symbol_provider.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/core/color.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "util/file_util.h"
#include "util/i18n/tr.h"
#include "util/indexed_png.h"
#include "util/macro.h"
#include "zelda3/screen/menu_tilemap_sources.h"

namespace yaze {
namespace editor {

namespace fs = std::filesystem;

namespace {

// menu_offset(row,col) = row*64 + col*2 -- see Core/symbols.asm in Oracle
// of Secrets. The five mask icons on the "Masks & Rings" page 3 prototype
// (Menu/menu_page3.asm, Menu_Page3_Draw) are each a 2x2 block of tiles
// written directly into the $1000 buffer at these (row, col) anchors, from
// the 4-word *GFX tables in Menu/menu_gfx_table.asm. These are *not*
// stored in ring_box.tilemap itself -- the frame file only has the empty
// background under them -- so this is an optional read-only overlay, not
// something this editor can paint into the tilemap file. The title text
// ("MASKS  RINGS" at menu_offset(6,10)) and the six ring slots
// (Menu_DrawMagicRingsInBox, ownership-dependent) are *not* included here:
// the title needs the menu's font/text encoding table and the ring slots
// depend on which rings are owned, both well beyond "a few regexes" to
// derive safely, per the brief.
struct MaskIconAnchor {
  int row;
  int col;
  std::array<uint16_t, 4>
      words;  // top-left, top-right, bottom-left, bottom-right
};
// Values transcribed from Menu/menu_gfx_table.asm (DekuMaskGFX,
// ZoraMaskGFX, WolfMaskGFX, BunnyHoodGFX, StoneMaskGFX) and the anchor
// (row, col) + GFX-table pairing from Menu_Page3_Draw in
// Menu/menu_page3.asm.
constexpr MaskIconAnchor kPage3MaskIcons[] = {
    {16, 5, {0x2066, 0x6066, 0x2076, 0x6076}},   // DekuMaskGFX
    {16, 8, {0x2C88, 0x6C88, 0x2C89, 0x6C89}},   // ZoraMaskGFX
    {16, 11, {0x3086, 0x7086, 0x3087, 0x7087}},  // WolfMaskGFX
    {16, 14, {0x3469, 0x7469, 0x3479, 0x7479}},  // BunnyHoodGFX
    {16, 17, {0x30B4, 0x30B5, 0x30C4, 0x30C5}},  // StoneMaskGFX
};

std::vector<std::array<uint8_t, 4>> BuildRgbaPalette(
    const std::array<gfx::SnesColor, 32>& colors) {
  std::vector<std::array<uint8_t, 4>> out(32);
  for (int i = 0; i < 32; ++i) {
    auto rgb = colors[i].rgb();
    uint8_t alpha = (i % 4 == 0) ? 0 : 255;
    out[i] = {static_cast<uint8_t>(rgb.x), static_cast<uint8_t>(rgb.y),
              static_cast<uint8_t>(rgb.z), alpha};
  }
  return out;
}

gfx::SnesPalette BuildSnesPalette(
    const std::array<gfx::SnesColor, 32>& colors) {
  return gfx::SnesPalette(
      std::vector<gfx::SnesColor>(colors.begin(), colors.end()));
}

}  // namespace

MenuTilemapEditorUI::~MenuTilemapEditorUI() {
  DestroyReferenceTexture();
}

void MenuTilemapEditorUI::DestroyReferenceTexture() {
  if (ref_texture_) {
    SDL_DestroyTexture(ref_texture_);
    ref_texture_ = nullptr;
    ref_texture_width_ = 0;
    ref_texture_height_ = 0;
  }
}

void MenuTilemapEditorUI::SetReferenceImage(const std::vector<uint8_t>& rgba,
                                            int width, int height) {
  DestroyReferenceTexture();
  if (width <= 0 || height <= 0)
    return;

  SDL_Renderer* renderer = nullptr;
  SDL_Window* window = SDL_GetMouseFocus();
  if (!window)
    window = SDL_GetKeyboardFocus();
  if (window)
    renderer = SDL_GetRenderer(window);
  if (!renderer)
    return;

  ref_texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                   SDL_TEXTUREACCESS_STATIC, width, height);
  if (!ref_texture_)
    return;
  SDL_SetTextureBlendMode(ref_texture_, SDL_BLENDMODE_BLEND);
  SDL_UpdateTexture(ref_texture_, nullptr, rgba.data(), width * 4);
  ref_texture_width_ = width;
  ref_texture_height_ = height;
  ref_loaded_ = true;
}

void MenuTilemapEditorUI::Draw(Rom* rom, zelda3::GameData* game_data,
                               project::YazeProject* project,
                               UndoManager* undo_manager) {
  const auto& theme = AgentUI::GetTheme();

  DrawFileBar(rom, project);
  ImGui::Separator();

  if (!loaded_) {
    ImGui::TextWrapped(
        "%s", tr("Open a .tilemap/.bin file, or pick one from the project "
                 "list above, to begin editing."));
    if (!open_error_.empty()) {
      ImGui::TextColored(theme.text_error_red, "%s", open_error_.c_str());
    }
    return;
  }

  DrawSourceBar(rom, game_data, project);
  ImGui::Separator();

  if (show_reload_prompt_) {
    ImGui::TextColored(theme.status_warning, "%s",
                       tr("This file changed on disk. Reload it? Your "
                          "unsaved edits will be lost if you do."));
    if (ImGui::Button(tr("Reload"))) {
      RevertCurrent();
      show_reload_prompt_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Keep My Edits"))) {
      show_reload_prompt_ = false;
    }
    ImGui::Separator();
  }

  DrawToolbar();
  ImGui::Separator();

  if (ImGui::BeginTable("##MenuTilemapLayout", 2, ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Tools", ImGuiTableColumnFlags_WidthFixed, 220);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawMainCanvas();
    DrawHoverReadout();
    DrawOverlayControls();

    ImGui::TableSetColumnIndex(1);
    DrawTilePicker();

    ImGui::EndTable();
  }

  ImGui::Separator();
  DrawStatusBar();

  // ShortcutManager gives the Screen editor its own Ctrl/Cmd+S while
  // that editor is active (Scope::kEditor keyed to EditorType::kScreen --
  // see shortcut_configurator.cc); this panel-local accelerator only fires
  // while the mouse is over this panel's content, so it never competes
  // with either the global save or the editor-wide one.
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
      (ImGui::GetIO().KeyMods & (ImGuiMod_Ctrl | ImGuiMod_Super)) &&
      ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    SaveCurrent();
  }

  (void)undo_manager;  // used by stroke commit paths below
}

void MenuTilemapEditorUI::DrawFileBar(Rom* rom, project::YazeProject* project) {
  if (ImGui::Button(tr("Open..."))) {
    util::FileDialogOptions options;
    options.filters.push_back({"Menu tilemap", "tilemap,bin"});
    options.filters.push_back({"All files", "*"});
    std::string path = util::FileDialogWrapper::ShowOpenFileDialog(options);
    if (!path.empty()) {
      LoadFile(path);
    }
  }

  if (project != nullptr) {
    if (project_files_stale_) {
      RefreshProjectFileList(project);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(280);
    std::string preview = loaded_ ? util::GetFileName(doc_.path())
                                  : std::string(tr("Project files..."));
    if (ImGui::BeginCombo("##MenuTilemapProjectFiles", preview.c_str())) {
      if (project_has_page3_) {
        ImGui::TextDisabled("%s", tr("Masks & Rings (page 3)"));
        ImGui::Separator();
      }
      for (const auto& entry : project_files_) {
        bool is_current = loaded_ && entry.full_path == doc_.path();
        std::string label = entry.label;
        if (is_current && doc_.dirty())
          label += " *";
        if (ImGui::Selectable(label.c_str(), is_current)) {
          LoadFile(entry.full_path);
        }
      }
      ImGui::EndCombo();
    }
  }

  if (loaded_) {
    ImGui::SameLine();
    ImGui::Text("%s%s", util::GetFileName(doc_.path()).c_str(),
                doc_.dirty() ? " *" : "");
    ImGui::SameLine();
    if (ImGui::Button(tr("Save")))
      SaveCurrent();
    ImGui::SameLine();
    if (ImGui::Button(tr("Save As...")))
      SaveCurrentAs();
    ImGui::SameLine();
    if (ImGui::Button(tr("Revert")))
      RevertCurrent();

    CheckExternalChange();
  }
}

void MenuTilemapEditorUI::RefreshProjectFileList(
    project::YazeProject* project) {
  project_files_.clear();
  project_files_stale_ = false;
  if (project == nullptr || project->filepath.empty())
    return;

  fs::path root = fs::path(project->filepath).parent_path();
  project_has_page3_ = fs::exists(root / "Menu" / "menu_page3.asm");

  auto add_dir = [&](const fs::path& dir) {
    std::error_code ec;
    if (!fs::exists(dir, ec))
      return;
    for (const auto& it : fs::directory_iterator(dir, ec)) {
      if (ec)
        break;
      if (!it.is_regular_file())
        continue;
      auto ext = it.path().extension().string();
      if (ext != ".tilemap" && ext != ".bin")
        continue;
      FileListEntry entry;
      entry.full_path = it.path().string();
      entry.label = fs::relative(it.path(), root, ec).string();
      if (ec)
        entry.label = it.path().filename().string();
      project_files_.push_back(std::move(entry));
    }
  };
  add_dir(root / "Menu" / "tilemaps");
  add_dir(root / "Menu" / "rings");

  std::sort(project_files_.begin(), project_files_.end(),
            [](const FileListEntry& a, const FileListEntry& b) {
              return a.label < b.label;
            });
  if (project_has_page3_) {
    // ring_box.tilemap drives the "Masks & Rings" page 3 prototype
    // (Menu_Page3_Draw / Menu_DrawRingBox) -- surface it first.
    auto it = std::find_if(project_files_.begin(), project_files_.end(),
                           [](const FileListEntry& e) {
                             return util::GetFileName(e.full_path) ==
                                    "ring_box.tilemap";
                           });
    if (it != project_files_.end() && it != project_files_.begin()) {
      std::rotate(project_files_.begin(), it, it + 1);
    }
  }
}

absl::Status MenuTilemapEditorUI::LoadFile(const std::string& path) {
  zelda3::MenuTilemapDocument new_doc;
  absl::Status status = new_doc.LoadFromFile(path);
  if (!status.ok()) {
    open_error_ = std::string(status.message());
    return status;
  }
  doc_ = std::move(new_doc);
  loaded_ = true;
  open_error_.clear();
  show_reload_prompt_ = false;
  has_selection_ = false;
  RebuildRenderTextures();
  return absl::OkStatus();
}

absl::Status MenuTilemapEditorUI::SaveCurrent() {
  if (!loaded_)
    return absl::FailedPreconditionError("nothing loaded");
  return doc_.Save();
}

absl::Status MenuTilemapEditorUI::SaveCurrentAs() {
  if (!loaded_)
    return absl::FailedPreconditionError("nothing loaded");
  std::string default_name = util::GetFileName(doc_.path());
  std::string path = util::FileDialogWrapper::ShowSaveFileDialog(
      default_name, util::GetFileExtension(doc_.path()));
  if (path.empty())
    return absl::CancelledError("save cancelled");
  return doc_.SaveAs(path);
}

absl::Status MenuTilemapEditorUI::RevertCurrent() {
  if (!loaded_)
    return absl::FailedPreconditionError("nothing loaded");
  RETURN_IF_ERROR(doc_.Revert());
  RebuildRenderTextures();
  return absl::OkStatus();
}

void MenuTilemapEditorUI::CheckExternalChange() {
  if (!loaded_ || show_reload_prompt_)
    return;
  auto changed = doc_.ExternalChangeDetected();
  if (changed.ok() && *changed) {
    show_reload_prompt_ = true;
  }
}

void MenuTilemapEditorUI::DrawSourceBar(Rom* rom, zelda3::GameData* game_data,
                                        project::YazeProject* project) {
  bool changed = false;

  // Auto-fill the symbol file from the project the first time one is
  // available, so the common case (an Oracle-style project with
  // symbols_filename set) needs no manual picking. A user-picked path
  // (symbols_path_from_project_ == false after an explicit "..." pick)
  // is never clobbered by this.
  if (project != nullptr && !project->symbols_filename.empty() &&
      (last_symbols_path_.empty() || symbols_path_from_project_)) {
    fs::path root = fs::path(project->filepath).parent_path();
    std::string resolved = (root / project->symbols_filename).string();
    if (resolved != last_symbols_path_) {
      last_symbols_path_ = resolved;
      symbols_path_from_project_ = true;
      changed = true;
    }
  }
  ImGui::TextUnformatted(tr("CHR:"));
  ImGui::SameLine();
  int chr_kind = static_cast<int>(chr_source_kind_);
  if (ImGui::RadioButton("ROM##chr", chr_kind == 0)) {
    chr_source_kind_ = ChrSourceKind::kRom;
    changed = true;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("File##chr", chr_kind == 1)) {
    chr_source_kind_ = ChrSourceKind::kFile;
    changed = true;
  }
  if (chr_source_kind_ == ChrSourceKind::kFile) {
    ImGui::SameLine();
    if (ImGui::Button("...##chrfile")) {
      util::FileDialogOptions options;
      options.filters.push_back({"2bpp CHR", "bin,chr,4bpp,2bpp"});
      std::string path = util::FileDialogWrapper::ShowOpenFileDialog(options);
      if (!path.empty()) {
        chr_file_path_ = path;
        changed = true;
      }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", chr_file_path_.empty()
                                  ? "(none)"
                                  : util::GetFileName(chr_file_path_).c_str());
  }

  ImGui::TextUnformatted(tr("Palette:"));
  ImGui::SameLine();
  int pal_kind = static_cast<int>(palette_source_kind_);
  if (ImGui::RadioButton("Symbol##pal", pal_kind == 0)) {
    palette_source_kind_ = PaletteSourceKind::kSymbol;
    changed = true;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("HUD##pal", pal_kind == 1)) {
    palette_source_kind_ = PaletteSourceKind::kHud;
    changed = true;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("File##pal", pal_kind == 2)) {
    palette_source_kind_ = PaletteSourceKind::kFile;
    changed = true;
  }
  if (palette_source_kind_ == PaletteSourceKind::kSymbol) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s", palette_label_.c_str());
    if (ImGui::InputText("##palLabel", buf, sizeof(buf))) {
      palette_label_ = buf;
      changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("...##symfile")) {
      util::FileDialogOptions options;
      options.filters.push_back({"Symbol file", "sym,mlb"});
      std::string path = util::FileDialogWrapper::ShowOpenFileDialog(options);
      if (!path.empty()) {
        last_symbols_path_ = path;
        symbols_path_from_project_ = false;
        changed = true;
      }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s",
                        last_symbols_path_.empty()
                            ? "(no symbols loaded)"
                            : util::GetFileName(last_symbols_path_).c_str());
  } else if (palette_source_kind_ == PaletteSourceKind::kFile) {
    ImGui::SameLine();
    if (ImGui::Button("...##palfile")) {
      util::FileDialogOptions options;
      options.filters.push_back({"Palette/CGRAM dump", "pal,bin,cgram"});
      std::string path = util::FileDialogWrapper::ShowOpenFileDialog(options);
      if (!path.empty()) {
        palette_file_path_ = path;
        changed = true;
      }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s",
                        palette_file_path_.empty()
                            ? "(none)"
                            : util::GetFileName(palette_file_path_).c_str());
  }

  if (changed || !sources_ready_) {
    ResolveSources(rom, game_data);
  }

  const auto& theme = AgentUI::GetTheme();
  if (!source_status_.empty()) {
    ImGui::TextColored(
        sources_ready_ ? theme.status_success : theme.text_error_red, "%s",
        source_status_.c_str());
  }
}

absl::Status MenuTilemapEditorUI::ResolveSources(Rom* rom,
                                                 zelda3::GameData* game_data) {
  sources_ready_ = false;

  if (chr_source_kind_ == ChrSourceKind::kFile) {
    auto result = zelda3::ResolveMenuChrFromFile(chr_file_path_);
    if (!result.ok()) {
      source_status_ = absl::StrFormat("CHR: %s", result.status().message());
      return result.status();
    }
    chr_sheet_ = std::move(*result);
  } else {
    if (rom == nullptr || !rom->is_loaded()) {
      source_status_ = "CHR: no ROM loaded";
      return absl::FailedPreconditionError(source_status_);
    }
    auto result = zelda3::ResolveMenuChrFromRom(*rom);
    if (!result.ok()) {
      source_status_ = absl::StrFormat("CHR: %s", result.status().message());
      return result.status();
    }
    chr_sheet_ = std::move(*result);
  }

  if (palette_source_kind_ == PaletteSourceKind::kFile) {
    auto result = zelda3::ResolveMenuPaletteFromFile(palette_file_path_);
    if (!result.ok()) {
      source_status_ =
          absl::StrFormat("Palette: %s", result.status().message());
      return result.status();
    }
    palette_colors_ = *result;
  } else if (palette_source_kind_ == PaletteSourceKind::kHud) {
    if (game_data == nullptr) {
      source_status_ = "Palette: no GameData loaded";
      return absl::FailedPreconditionError(source_status_);
    }
    auto result = zelda3::ResolveMenuPaletteFromHud(*game_data);
    if (!result.ok()) {
      source_status_ =
          absl::StrFormat("Palette: %s", result.status().message());
      return result.status();
    }
    palette_colors_ = *result;
  } else {
    if (rom == nullptr || !rom->is_loaded()) {
      source_status_ = "Palette: no ROM loaded";
      return absl::FailedPreconditionError(source_status_);
    }
    emu::debug::SymbolProvider symbols;
    // Symbols come from the project's symbols_filename; DrawSourceBar()
    // doesn't have that path directly, so this source only resolves once
    // the caller has separately confirmed a symbol table is loaded. For
    // now this falls through to an explicit error naming the constraint
    // rather than silently doing nothing -- see docs/public for how to
    // wire a real project's .sym path in.
    if (last_symbols_path_.empty()) {
      source_status_ =
          "Palette: no symbol file loaded (use --symbols via z3ed for "
          "headless rendering, or the HUD/File source in the panel)";
      return absl::FailedPreconditionError(source_status_);
    }
    RETURN_IF_ERROR(symbols.LoadSymbolFile(last_symbols_path_));
    auto result =
        zelda3::ResolveMenuPaletteFromSymbol(*rom, symbols, palette_label_);
    if (!result.ok()) {
      source_status_ =
          absl::StrFormat("Palette: %s", result.status().message());
      return result.status();
    }
    palette_colors_ = *result;
  }

  sources_ready_ = true;
  source_status_ =
      absl::StrFormat("CHR: %s tiles, Palette: ready",
                      std::to_string(chr_sheet_.size() / 64).c_str());
  RebuildRenderTextures();
  return absl::OkStatus();
}

void MenuTilemapEditorUI::RebuildRenderTextures() {
  if (!loaded_)
    return;
  auto chr_fn = zelda3::MakeChrPixelFn(chr_sheet_);
  std::vector<uint8_t> indexed = doc_.RenderIndexed(chr_fn);

  if (show_dynamic_layer_) {
    // Display-only overlay: paints the page-3 mask icons directly into the
    // rendered pixels (never into doc_'s bytes -- these tiles are written
    // by ASM at runtime, not stored in ring_box.tilemap itself).
    int width = doc_.render_width();
    int height = doc_.render_height();
    for (const auto& icon : kPage3MaskIcons) {
      for (int sub = 0; sub < 4; ++sub) {
        gfx::TileInfo info = gfx::WordToTileInfo(icon.words[sub]);
        int cell_row = icon.row + sub / 2;
        int cell_col = icon.col + sub % 2;
        int base_x = cell_col * 8;
        int base_y = cell_row * 8;
        if (base_x + 8 > width || base_y + 8 > height)
          continue;
        for (int y = 0; y < 8; ++y) {
          int sy = info.vertical_mirror_ ? 7 - y : y;
          for (int x = 0; x < 8; ++x) {
            int sx = info.horizontal_mirror_ ? 7 - x : x;
            uint8_t color = chr_fn(info.id_, sx, sy) & 0x03;
            uint8_t index =
                static_cast<uint8_t>((info.palette_ & 0x07) * 4 + color);
            size_t idx = static_cast<size_t>(base_y + y) * width + (base_x + x);
            if (color != 0)
              indexed[idx] = index;  // color 0 = transparent
          }
        }
      }
    }
  }

  canvas_bitmap_.Create(doc_.render_width(), doc_.render_height(), 8, indexed);
  canvas_bitmap_.SetPalette(BuildSnesPalette(palette_colors_));
  gfx::Arena::Get().QueueTextureCommand(
      canvas_bitmap_.is_active() ? gfx::Arena::TextureCommandType::UPDATE
                                 : gfx::Arena::TextureCommandType::CREATE,
      &canvas_bitmap_);

  if (!chr_sheet_.empty()) {
    picker_width_ = 128;
    picker_height_ = static_cast<int>(chr_sheet_.size() / picker_width_);
    picker_indexed_ =
        chr_sheet_;  // raw 2bpp values 0-3; palette below maps them
    picker_bitmap_.Create(picker_width_, picker_height_, 8, picker_indexed_);
    std::array<gfx::SnesColor, 32> slice{};
    for (int i = 0; i < 4; ++i) {
      slice[i] = palette_colors_[selected_palette_ * 4 + i];
    }
    picker_bitmap_.SetPalette(BuildSnesPalette(slice));
    gfx::Arena::Get().QueueTextureCommand(
        picker_bitmap_.is_active() ? gfx::Arena::TextureCommandType::UPDATE
                                   : gfx::Arena::TextureCommandType::CREATE,
        &picker_bitmap_);
  }
}

void MenuTilemapEditorUI::DrawToolbar() {
  if (ImGui::RadioButton(tr("Paint"), tool_ == Tool::kPaint)) {
    tool_ = Tool::kPaint;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton(tr("Select"), tool_ == Tool::kSelect)) {
    tool_ = Tool::kSelect;
  }
  ImGui::SameLine();
  ImGui::Checkbox(tr("Grid"), &show_grid_);
  ImGui::SameLine();
  ImGui::Checkbox(tr("Tint Priority"), &show_priority_tint_);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120);
  ImGui::SliderFloat("Zoom", &zoom_, 1.0f, 4.0f, "%.1fx");

  if (tool_ == Tool::kSelect && has_selection_) {
    if (ImGui::Button(tr("Copy"))) {
      clipboard_ = doc_.CopyRect(selection_);
      has_clipboard_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Paste")) && has_clipboard_) {
      BeginStroke("Paste");
      doc_.PasteRect(selection_.row, selection_.col, clipboard_);
      CommitStroke(nullptr);
      RebuildRenderTextures();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Fill"))) {
      BeginStroke("Fill selection");
      gfx::TileInfo info(static_cast<uint16_t>(selected_tile_id_),
                         static_cast<uint8_t>(selected_palette_), v_flip_,
                         h_flip_, priority_);
      doc_.FillRect(selection_, info);
      CommitStroke(nullptr);
      RebuildRenderTextures();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Erase"))) {
      BeginStroke("Erase selection");
      doc_.EraseRect(selection_, erase_word_);
      CommitStroke(nullptr);
      RebuildRenderTextures();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    int erase_val = erase_word_;
    if (ImGui::InputInt("Erase word", &erase_val, 0, 0,
                        ImGuiInputTextFlags_CharsHexadecimal)) {
      erase_word_ = static_cast<uint16_t>(std::clamp(erase_val, 0, 0xFFFF));
    }
  }
}

void MenuTilemapEditorUI::DrawMainCanvas() {
  // Keep the canvas's own scale (used internally for grid lines and for
  // converting screen-space mouse position back to unscaled tile-pixel
  // coordinates in points()/hover_mouse_pos()) in lockstep with the zoom
  // slider passed to DrawBitmap() below -- otherwise the grid/hover/click
  // mapping and the visible bitmap drift apart as zoom_ changes.
  canvas_.SetGlobalScale(zoom_);
  canvas_.SetCanvasSize(
      ImVec2(doc_.render_width() * zoom_, doc_.render_height() * zoom_));
  last_canvas_screen_origin_ = ImGui::GetCursorScreenPos();
  canvas_.DrawBackground();
  canvas_.DrawContextMenu();

  if (canvas_bitmap_.is_active()) {
    canvas_.DrawBitmap(canvas_bitmap_, 0, 0, zoom_, 255);
  }

  if (show_wram_overlay_ && wram_loaded_ && wram_bitmap_.is_active()) {
    canvas_.DrawBitmap(wram_bitmap_, 0, 0, zoom_, 255);
  }

  if (show_ref_overlay_ && ref_loaded_ && ref_texture_ != nullptr) {
    ImVec2 p_min(last_canvas_screen_origin_.x + ref_nudge_.x * zoom_,
                 last_canvas_screen_origin_.y + ref_nudge_.y * zoom_);
    ImVec2 p_max(p_min.x + ref_texture_width_ * zoom_,
                 p_min.y + ref_texture_height_ * zoom_);
    uint8_t alpha =
        static_cast<uint8_t>(std::clamp(ref_opacity_, 0.0f, 1.0f) * 255.0f);
    ImGui::GetWindowDrawList()->AddImage(
        reinterpret_cast<ImTextureID>(ref_texture_), p_min, p_max, ImVec2(0, 0),
        ImVec2(1, 1), IM_COL32(255, 255, 255, alpha));
  }

  has_hover_ = canvas_.IsMouseHovering();
  if (has_hover_) {
    ImVec2 mp = canvas_.hover_mouse_pos();
    hover_col_ = static_cast<int>(mp.x) / 8;
    hover_row_ = static_cast<int>(mp.y) / 8;
  }

  bool alt_click =
      ImGui::GetIO().KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  bool right_click = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  if (has_hover_ && (alt_click || right_click) &&
      doc_.InBounds(hover_row_, hover_col_)) {
    EyedropAt(hover_row_, hover_col_);
  }

  if (tool_ == Tool::kPaint) {
    if (canvas_.DrawTileSelector(8.0f)) {
      if (!canvas_.points().empty()) {
        ImVec2 p = canvas_.points().front();
        int col = static_cast<int>(p.x) / 8;
        int row = static_cast<int>(p.y) / 8;
        if (doc_.InBounds(row, col)) {
          if (!stroke_active_)
            BeginStroke("Paint tile");
          PaintCellAt(row, col);
          RebuildRenderTextures();
        }
      }
    }
    if (stroke_active_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      CommitStroke(nullptr);
    }
  } else if (tool_ == Tool::kSelect) {
    if (has_hover_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      selecting_ = true;
      select_drag_start_ = ImVec2(static_cast<float>(hover_col_),
                                  static_cast<float>(hover_row_));
    }
    if (selecting_ && has_hover_) {
      int c0 = static_cast<int>(select_drag_start_.x);
      int r0 = static_cast<int>(select_drag_start_.y);
      selection_.col = std::min(c0, hover_col_);
      selection_.row = std::min(r0, hover_row_);
      selection_.cols = std::abs(hover_col_ - c0) + 1;
      selection_.rows = std::abs(hover_row_ - r0) + 1;
      has_selection_ = true;
    }
    if (selecting_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      selecting_ = false;
    }
  }

  if (show_grid_)
    canvas_.DrawGrid(8.0f);
  canvas_.DrawOverlay();
}

void MenuTilemapEditorUI::DrawTilePicker() {
  ImGui::TextUnformatted(tr("Tile Picker"));
  picker_canvas_.DrawBackground();
  picker_canvas_.DrawContextMenu();
  if (picker_bitmap_.is_active()) {
    picker_canvas_.DrawBitmap(picker_bitmap_, 0, 0, 1.0f, 255);
  }
  if (picker_canvas_.DrawTileSelector(8.0f)) {
    if (!picker_canvas_.points().empty()) {
      ImVec2 p = picker_canvas_.points().front();
      int tx = static_cast<int>(p.x) / 8;
      int ty = static_cast<int>(p.y) / 8;
      int tiles_per_row = picker_width_ / 8;
      if (tiles_per_row > 0) {
        selected_tile_id_ = tx + ty * tiles_per_row;
      }
    }
  }
  picker_canvas_.DrawGrid(8.0f);
  picker_canvas_.DrawOverlay();

  ImGui::Text("%s %d", tr("Tile:"), selected_tile_id_);
  ImGui::SetNextItemWidth(140);
  ImGui::SliderInt(tr("Palette"), &selected_palette_, 0, 7);
  if (ImGui::IsItemDeactivatedAfterEdit())
    RebuildRenderTextures();
  ImGui::Checkbox(tr("H Flip"), &h_flip_);
  ImGui::SameLine();
  ImGui::Checkbox(tr("V Flip"), &v_flip_);
  ImGui::Checkbox(tr("Priority"), &priority_);
}

void MenuTilemapEditorUI::DrawHoverReadout() {
  if (!has_hover_ || !doc_.InBounds(hover_row_, hover_col_))
    return;
  gfx::TileInfo info = doc_.GetCell(hover_row_, hover_col_);
  uint16_t word = doc_.GetCellWord(hover_row_, hover_col_);
  ImGui::Text("x=%d y=%d  tile=%03X  pal=%d  H=%d V=%d P=%d  word=%04X",
              hover_col_, hover_row_, info.id_, info.palette_,
              info.horizontal_mirror_ ? 1 : 0, info.vertical_mirror_ ? 1 : 0,
              info.over_ ? 1 : 0, word);
}

void MenuTilemapEditorUI::DrawOverlayControls() {
  if (ImGui::CollapsingHeader(tr("Preview Overlays"))) {
    ImGui::Checkbox(tr("Reference image"), &show_ref_overlay_);
    if (show_ref_overlay_) {
      ImGui::SameLine();
      if (ImGui::Button("...##refimg")) {
        util::FileDialogOptions options;
        options.filters.push_back({"PNG", "png"});
        std::string path = util::FileDialogWrapper::ShowOpenFileDialog(options);
        if (!path.empty()) {
          ref_image_path_ = path;
          auto bytes = util::ReadBinaryFile(path);
          if (bytes.ok()) {
            auto png = util::DecodePng(*bytes);
            if (png.ok()) {
              if (png->indexed) {
                std::vector<uint8_t> rgba(png->indices.size() * 4);
                for (size_t i = 0; i < png->indices.size(); ++i) {
                  auto& c = png->palette[png->indices[i]];
                  rgba[i * 4 + 0] = c[0];
                  rgba[i * 4 + 1] = c[1];
                  rgba[i * 4 + 2] = c[2];
                  rgba[i * 4 + 3] = c[3];
                }
                SetReferenceImage(rgba, png->width, png->height);
              } else {
                SetReferenceImage(png->rgba, png->width, png->height);
              }
            }
          }
        }
      }
      ImGui::SliderFloat(tr("Opacity"), &ref_opacity_, 0.0f, 1.0f);
      ImGui::SliderFloat("dx", &ref_nudge_.x, -32.0f, 32.0f);
      ImGui::SameLine();
      ImGui::SliderFloat("dy", &ref_nudge_.y, -32.0f, 32.0f);
    }

    ImGui::Checkbox(tr("WRAM composite dump ($7E1000-17FF)"),
                    &show_wram_overlay_);
    if (show_wram_overlay_) {
      ImGui::SameLine();
      if (ImGui::Button("...##wramdump")) {
        util::FileDialogOptions options;
        options.filters.push_back({"WRAM dump", "bin"});
        std::string path = util::FileDialogWrapper::ShowOpenFileDialog(options);
        if (!path.empty()) {
          wram_dump_path_ = path;
          zelda3::MenuTilemapDocument wram_doc;
          if (wram_doc.LoadFromFile(path).ok() && !chr_sheet_.empty()) {
            auto chr_fn = zelda3::MakeChrPixelFn(chr_sheet_);
            auto indexed = wram_doc.RenderIndexed(chr_fn);
            wram_bitmap_.Create(wram_doc.render_width(),
                                wram_doc.render_height(), 8, indexed);
            wram_bitmap_.SetPalette(BuildSnesPalette(palette_colors_));
            gfx::Arena::Get().QueueTextureCommand(
                gfx::Arena::TextureCommandType::CREATE, &wram_bitmap_);
            wram_loaded_ = true;
          }
        }
      }
    }

    if (ImGui::Checkbox(tr("Dynamic content (page 3 mask icons, read-only)"),
                        &show_dynamic_layer_)) {
      RebuildRenderTextures();
    }
    if (show_dynamic_layer_) {
      ImGui::TextDisabled(
          "%s", tr("Display only -- the five mask icons at their fixed "
                   "menu_offset() cells. Not saved into this file, and not the "
                   "title text or ring slots (see docs)."));
    }
  }
}

void MenuTilemapEditorUI::DrawStatusBar() {
  ImGui::Text("%s", doc_.dirty() ? tr("Modified") : tr("Saved"));
  if (doc_.has_backup()) {
    ImGui::SameLine();
    ImGui::TextDisabled("(backup: %s)", doc_.backup_path().c_str());
  }
}

void MenuTilemapEditorUI::BeginStroke(const std::string& description) {
  stroke_active_ = true;
  stroke_before_bytes_ = doc_.raw_bytes();
  stroke_description_ = description;
}

void MenuTilemapEditorUI::CommitStroke(UndoManager* undo_manager) {
  stroke_active_ = false;
  if (undo_manager == nullptr)
    return;
  ScreenSnapshot before;
  before.edit_type = ScreenEditType::kMenuTilemap;
  before.menu_tilemap.path = doc_.path();
  before.menu_tilemap.bytes = stroke_before_bytes_;

  ScreenSnapshot after;
  after.edit_type = ScreenEditType::kMenuTilemap;
  after.menu_tilemap.path = doc_.path();
  after.menu_tilemap.bytes = doc_.raw_bytes();

  if (before.menu_tilemap.bytes == after.menu_tilemap.bytes)
    return;

  undo_manager->Push(std::make_unique<ScreenEditAction>(
      before, after,
      [this](const ScreenSnapshot& snap) {
        RestoreSnapshot(snap.menu_tilemap.bytes);
      },
      stroke_description_));
}

void MenuTilemapEditorUI::RestoreSnapshot(const std::vector<uint8_t>& bytes) {
  std::string path = doc_.path();
  if (doc_.LoadFromBytes(bytes, path).ok()) {
    RebuildRenderTextures();
  }
}

void MenuTilemapEditorUI::PaintCellAt(int row, int col) {
  gfx::TileInfo info(static_cast<uint16_t>(selected_tile_id_),
                     static_cast<uint8_t>(selected_palette_), v_flip_, h_flip_,
                     priority_);
  doc_.SetCell(row, col, info);
}

void MenuTilemapEditorUI::EyedropAt(int row, int col) {
  gfx::TileInfo info = doc_.GetCell(row, col);
  selected_tile_id_ = info.id_;
  selected_palette_ = info.palette_;
  h_flip_ = info.horizontal_mirror_;
  v_flip_ = info.vertical_mirror_;
  priority_ = info.over_;
}

}  // namespace editor
}  // namespace yaze
