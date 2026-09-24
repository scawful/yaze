#include "sprite_editor.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/editor/sprite/sprite_authoring.h"
#include "app/editor/sprite/sprite_drawer.h"
#include "app/editor/sprite/sprite_editor_internal.h"
#include "app/editor/sprite/sprite_undo_actions.h"
#include "app/editor/sprite/zsprite.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "core/project.h"
#include "util/file_util.h"
#include "util/hex.h"
#include "util/macro.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

using ImGui::BeginTable;
using ImGui::Button;
using ImGui::EndTable;
using ImGui::Selectable;
using ImGui::Separator;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

namespace {
template <size_t N>
void CopyStringToBuffer(const std::string& src, char (&dest)[N]) {
  std::strncpy(dest, src.c_str(), N - 1);
  dest[N - 1] = '\0';
}

int ParseIntOrDefault(const std::string& text, int fallback = 0) {
  if (text.empty()) {
    return fallback;
  }
  errno = 0;
  char* end = nullptr;
  long value = std::strtol(text.c_str(), &end, 0);
  if (end == text.c_str() || errno == ERANGE) {
    return fallback;
  }
  return static_cast<int>(value);
}
}  // namespace

void SpriteEditor::Initialize() {
  if (!dependencies_.window_manager)
    return;
  auto* window_manager = dependencies_.window_manager;

  // Register WindowContent implementations with callbacks
  // EditorPanels provide both metadata (icon, name, priority) and drawing logic
  window_manager->RegisterWindowContent(
      std::make_unique<VanillaSpriteEditorPanel>([this]() {
        if (rom_ && rom_->is_loaded()) {
          DrawVanillaSpriteEditor();
        } else {
          ImGui::TextDisabled(tr("Load a ROM to view vanilla sprites"));
        }
      }));

  window_manager->RegisterWindowContent(
      std::make_unique<CustomSpriteEditorPanel>(
          [this]() { DrawCustomSprites(); }));
  window_manager->RegisterWindowContent(
      std::make_unique<SpriteCatalogPanel>([this]() { DrawSpriteCatalog(); }));
}

absl::Status SpriteEditor::Load() {
  gfx::ScopedTimer timer("SpriteEditor::Load");
  (void)ReloadSpriteCatalog();
  asset_load_status_ = ReloadProjectSpriteAssets();
  return absl::OkStatus();
}

absl::Status SpriteEditor::Update() {
  if (rom() && rom()->is_loaded() && !sheets_loaded_) {
    sheets_loaded_ = true;
  }

  // Update animation playback for custom sprites
  float current_time = ImGui::GetTime();
  float delta_time = current_time - last_frame_time_;
  last_frame_time_ = current_time;
  UpdateAnimationPlayback(delta_time);

  // Handle editor-level shortcuts
  HandleEditorShortcuts();

  // Panel drawing is handled by WorkspaceWindowManager via registered EditorPanels
  // Each panel's Draw() callback invokes the appropriate draw method

  // Commit any pending undo transaction at the end of the frame
  CommitUndoTransaction();

  return status_.ok() ? absl::OkStatus() : status_;
}

void SpriteEditor::HandleEditorShortcuts() {
  // Animation playback shortcuts (when custom sprite panel is active)
  if (ImGui::IsKeyPressed(ImGuiKey_Space, false) &&
      !ImGui::GetIO().WantTextInput) {
    animation_playing_ = !animation_playing_;
  }

  // Frame navigation
  if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket, false)) {
    if (current_frame_ > 0) {
      current_frame_--;
      preview_needs_update_ = true;
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, false)) {
    current_frame_++;
    preview_needs_update_ = true;
  }

  // Sprite navigation
  if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
    if (current_sprite_id_ > 0) {
      current_sprite_id_--;
      vanilla_preview_needs_update_ = true;
    }
  }
  if (ImGui::GetIO().KeyCtrl &&
      ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
    current_sprite_id_++;
    vanilla_preview_needs_update_ = true;
  }
}

absl::Status SpriteEditor::Save() {
  if (current_custom_sprite_index_ >= 0 &&
      current_custom_sprite_index_ < static_cast<int>(custom_sprites_.size())) {
    const std::string& zsm_path = GetCurrentZsmPath();
    if (zsm_path.empty()) {
      SaveZsmFileAs();
    } else {
      SaveZsmFile(zsm_path);
    }
  }
  return status_;
}

void SpriteEditor::DrawToolset() {
  // Sidebar handled by EditorManager for card-based editors
}

// ============================================================
// Vanilla Sprite Editor
// ============================================================

void SpriteEditor::DrawVanillaSpriteEditor() {
  if (ImGui::BeginTable("##SpriteCanvasTable", 3, ImGuiTableFlags_Resizable,
                        ImVec2(0, 0))) {
    TableSetupColumn("Sprites List", ImGuiTableColumnFlags_WidthFixed, 256);
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Tile Selector", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    DrawSpritesList();

    TableNextColumn();
    static int next_tab_id = 0;

    if (gui::BeginThemedTabBar("SpriteTabBar", kSpriteTabBarFlags)) {
      if (ImGui::TabItemButton(ICON_MD_ADD, kSpriteTabFlags)) {
        if (std::find(active_sprites_.begin(), active_sprites_.end(),
                      current_sprite_id_) != active_sprites_.end()) {
          next_tab_id++;
        }
        active_sprites_.push_back(next_tab_id++);
      }

      for (int n = 0; n < active_sprites_.Size;) {
        bool open = true;

        if (active_sprites_[n] > sizeof(zelda3::kSpriteDefaultNames) / 4) {
          active_sprites_.erase(active_sprites_.Data + n);
          continue;
        }

        if (ImGui::BeginTabItem(
                zelda3::kSpriteDefaultNames[active_sprites_[n]].data(), &open,
                ImGuiTabItemFlags_None)) {
          DrawSpriteCanvas();
          ImGui::EndTabItem();
        }

        if (!open)
          active_sprites_.erase(active_sprites_.Data + n);
        else
          n++;
      }

      gui::EndThemedTabBar();
    }

    TableNextColumn();
    if (sheets_loaded_) {
      DrawCurrentSheets();
    }
    ImGui::EndTable();
  }
}

void SpriteEditor::DrawSpriteCanvas() {
  if (ImGui::BeginChild(gui::GetID("##SpriteCanvas"),
                        ImGui::GetContentRegionAvail(), true)) {
    sprite_canvas_.DrawBackground();
    sprite_canvas_.DrawContextMenu();

    // Render vanilla sprite if layout exists
    if (current_sprite_id_ >= 0) {
      const auto* layout = zelda3::SpriteOamRegistry::GetLayout(
          static_cast<uint8_t>(current_sprite_id_));
      if (layout) {
        // Vanilla preview must not replace the custom editor's sheet selection.
        std::array<uint8_t, 8> custom_sheets;
        std::copy_n(current_sheets_, 8, custom_sheets.begin());
        LoadSheetsForSprite(layout->required_sheets);
        RenderVanillaSprite(*layout);

        // Draw the preview bitmap centered on canvas
        if (vanilla_preview_bitmap_.is_active()) {
          sprite_canvas_.DrawBitmap(vanilla_preview_bitmap_, 64, 64, 2.0f);
        }

        // Show sprite info
        ImGui::SetCursorPos(ImVec2(10, 10));
        Text(tr("Sprite: %s (0x%02X)"), layout->name, layout->sprite_id);
        Text(tr("Tiles: %zu"), layout->tiles.size());
        if (ImGui::Button(tr("Edit preview copy"))) {
          CreateNewZSprite();
          auto& copy = custom_sprites_.back();
          copy.sprName = std::string(layout->name) + " (preview copy)";
          copy.editor.Frames[0] = sprite_authoring::CopyVanillaLayout(*layout);
          std::copy_n(current_sheets_, 8, custom_sheets.begin());
          custom_sprite_bindings_.back().sheets = custom_sheets;
          if (dependencies_.window_manager) {
            auto visible = dependencies_.window_manager->GetVisibleWindowIds(
                dependencies_.session_id);
            visible.push_back("sprite.custom_editor");
            dependencies_.window_manager->SetVisibleWindows(
                dependencies_.session_id, visible);
          }
        }
        ImGui::TextDisabled(
            "Static layout copy; save as ZSM in Custom Sprites.");
        if (!std::equal(custom_sheets.begin(), custom_sheets.end(),
                        current_sheets_)) {
          std::copy(custom_sheets.begin(), custom_sheets.end(),
                    current_sheets_);
          gfx_buffer_loaded_ = false;
          preview_needs_update_ = true;
        }
        gfx_buffer_loaded_ = false;
        preview_needs_update_ = true;
      }
    }

    sprite_canvas_.DrawGrid();
    sprite_canvas_.DrawOverlay();

    ImGui::TextDisabled(
        "Use Edit preview copy to author frames and animations.");
  }
  ImGui::EndChild();
}

void SpriteEditor::DrawCurrentSheets() {
  if (ImGui::BeginChild(gui::GetID("sheet_label"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0), true,
                        ImGuiWindowFlags_NoDecoration)) {
    // Track previous sheet values for change detection
    bool sheets_changed = false;
    if (current_custom_sprite())
      BeginUndoTransaction();

    for (int i = 0; i < 8; i++) {
      std::string sheet_label = absl::StrFormat("Sheet %d", i);
      if (gui::InputHexByte(sheet_label.c_str(), &current_sheets_[i])) {
        sheets_changed = true;
      }
      if (i % 2 == 0)
        ImGui::SameLine();
    }

    // Reload graphics buffer if sheets changed
    if (sheets_changed) {
      gfx_buffer_loaded_ = false;
      preview_needs_update_ = true;
      if (current_custom_sprite()) {
        auto& binding = custom_sprite_bindings_[current_custom_sprite_index_];
        std::copy_n(current_sheets_, 8, binding.sheets.begin());
        MarkSpriteMutated();
      }
    }
    CommitUndoTransaction();

    graphics_sheet_canvas_.GetConfig().role = gui::CanvasRole::kSelectionSource;
    graphics_sheet_canvas_.DrawBackground();
    graphics_sheet_canvas_.DrawContextMenu();
    graphics_sheet_canvas_.DrawTileSelector(32);
    for (int i = 0; i < 8; i++) {
      if (current_sheets_[i] >= gfx::Arena::Get().gfx_sheets().size())
        continue;
      auto& sheet = gfx::Arena::Get().gfx_sheets().at(current_sheets_[i]);
      if (sheet.is_active() && !sheet.texture())
        sheet.CreateTexture();
      graphics_sheet_canvas_.DrawBitmap(sheet, 1, (i * 0x40) + 1, 2);
    }
    graphics_sheet_canvas_.DrawGrid();
    graphics_sheet_canvas_.DrawOverlay();
  }
  ImGui::EndChild();
}

void SpriteEditor::DrawSpritesList() {
  if (ImGui::BeginChild(gui::GetID("##SpritesList"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0), true,
                        ImGuiWindowFlags_NoDecoration)) {
    int i = 0;
    for (const auto each_sprite_name : zelda3::kSpriteDefaultNames) {
      rom()->resource_label()->SelectableLabelWithNameEdit(
          current_sprite_id_ == i, "Sprite Names", util::HexByte(i),
          zelda3::kSpriteDefaultNames[i].data());
      if (ImGui::IsItemClicked()) {
        if (current_sprite_id_ != i) {
          current_sprite_id_ = i;
          vanilla_preview_needs_update_ = true;
        }
        if (!active_sprites_.contains(i)) {
          active_sprites_.push_back(i);
        }
      }
      i++;
    }
  }
  ImGui::EndChild();
}

// ============================================================
// Custom ZSM Sprite Editor
// ============================================================

void SpriteEditor::DrawCustomSprites() {
  if (BeginTable("##CustomSpritesTable", 3,
                 ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders,
                 ImVec2(0, 0))) {
    TableSetupColumn("Sprite Data", ImGuiTableColumnFlags_WidthFixed, 300);
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Tilesheets", ImGuiTableColumnFlags_WidthFixed, 280);

    TableHeadersRow();
    TableNextRow();
    TableNextColumn();

    // Keep the canvas visible while scrolling the longer authoring controls.
    if (ImGui::BeginChild("##SpriteMetadata", ImVec2(0, 0)))
      DrawCustomSpritesMetadata();
    ImGui::EndChild();

    TableNextColumn();
    DrawZSpriteOnCanvas();

    TableNextColumn();
    DrawCurrentSheets();

    EndTable();
  }
}

void SpriteEditor::DrawCustomSpritesMetadata() {
  if (!asset_load_status_.ok())
    ImGui::TextWrapped("%s", asset_load_status_.ToString().c_str());
  // File operations toolbar
  if (ImGui::Button(ICON_MD_ADD " New")) {
    CreateNewZSprite();
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Open")) {
    std::string file_path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!file_path.empty()) {
      status_ = OpenSpriteAsset(file_path);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE " Save")) {
    if (current_custom_sprite_index_ >= 0) {
      const std::string& zsm_path = GetCurrentZsmPath();
      if (zsm_path.empty()) {
        SaveZsmFileAs();
      } else {
        SaveZsmFile(zsm_path);
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE_AS " Save As")) {
    SaveZsmFileAs();
  }

  Separator();

  // Sprite list
  Text(tr("Loaded Sprites:"));
  if (ImGui::BeginChild("SpriteList", ImVec2(0, 100), true)) {
    for (size_t i = 0; i < custom_sprites_.size(); i++) {
      std::string label = custom_sprites_[i].sprName.empty()
                              ? "Unnamed Sprite"
                              : custom_sprites_[i].sprName;
      ImGui::PushID(static_cast<int>(i));
      if (Selectable(label.c_str(), current_custom_sprite_index_ == (int)i)) {
        current_custom_sprite_index_ = static_cast<int>(i);
        current_frame_ = custom_sprites_[i].editor.Frames.empty() ? -1 : 0;
        current_animation_index_ =
            custom_sprites_[i].animations.empty() ? -1 : 0;
        selected_tile_index_ = -1;
        selected_routine_index_ = -1;
        animation_playing_ = false;
        frame_timer_ = 0.0f;
        preview_needs_update_ = true;
        ApplyCurrentSpriteBinding();
        (void)CheckCurrentSpriteSource();
      }
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  Separator();

  // Show properties for selected sprite
  if (current_custom_sprite_index_ >= 0 &&
      current_custom_sprite_index_ < (int)custom_sprites_.size()) {
    BeginUndoTransaction();
    DrawSpriteAssetBindings();
    if (gui::BeginThemedTabBar("SpriteDataTabs")) {
      if (ImGui::BeginTabItem(tr("Properties"))) {
        DrawSpritePropertiesPanel();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(tr("Animations"))) {
        DrawAnimationPanel();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(tr("Behavior"))) {
        DrawSpriteBehaviorPanel();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(tr("Routines"))) {
        DrawUserRoutinesPanel();
        ImGui::EndTabItem();
      }
      gui::EndThemedTabBar();
    }
  } else {
    Text(tr("No sprite selected"));
  }
  CommitUndoTransaction();
}

absl::Status SpriteEditor::Copy() {
  if (current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >= static_cast<int>(custom_sprites_.size()))
    return absl::FailedPreconditionError(
        "Select a custom sprite frame to copy");
  const auto& frames =
      custom_sprites_[current_custom_sprite_index_].editor.Frames;
  if (current_frame_ < 0 || current_frame_ >= static_cast<int>(frames.size()))
    return absl::FailedPreconditionError("Select a frame to copy");
  frame_clipboard_ = frames[current_frame_];
  return absl::OkStatus();
}

absl::Status SpriteEditor::Cut() {
  RETURN_IF_ERROR(Copy());
  auto& sprite = custom_sprites_[current_custom_sprite_index_];
  if (sprite.editor.Frames.size() <= 1)
    return absl::FailedPreconditionError(
        "Cannot cut the last frame; copied it instead");
  BeginUndoTransaction();
  sprite_authoring::DeleteFrame(sprite, current_frame_);
  current_frame_ = std::min(current_frame_,
                            static_cast<int>(sprite.editor.Frames.size()) - 1);
  selected_tile_index_ = -1;
  MarkSpriteMutated();
  CommitUndoTransaction();
  return absl::OkStatus();
}

absl::Status SpriteEditor::Paste() {
  if (!frame_clipboard_ || current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >= static_cast<int>(custom_sprites_.size()))
    return absl::FailedPreconditionError(
        "Copy a frame and select a destination sprite");
  auto& sprite = custom_sprites_[current_custom_sprite_index_];
  if (sprite.editor.Frames.size() >= sprite_authoring::kMaxFrames)
    return absl::OutOfRangeError("ZSM animations address at most 256 frames");
  BeginUndoTransaction();
  sprite.editor.Frames.push_back(*frame_clipboard_);
  current_frame_ = static_cast<int>(sprite.editor.Frames.size()) - 1;
  selected_tile_index_ = -1;
  MarkSpriteMutated();
  CommitUndoTransaction();
  return absl::OkStatus();
}

void SpriteEditor::CreateNewZSprite() {
  zsprite::ZSprite new_sprite;
  new_sprite.Reset();
  new_sprite.sprName = "New Sprite";

  // Add default frame
  new_sprite.editor.Frames.emplace_back();

  // Add default animation
  new_sprite.animations.emplace_back(0, 0, 1, "Idle");

  custom_sprites_.push_back(std::move(new_sprite));
  custom_sprite_paths_.push_back(std::string());
  project::SpriteAssetBinding binding;
  std::copy_n(current_sheets_, 8, binding.sheets.begin());
  custom_sprite_bindings_.push_back(std::move(binding));
  source_checked_ = false;
  current_custom_sprite_index_ = static_cast<int>(custom_sprites_.size()) - 1;
  current_frame_ = 0;
  current_animation_index_ = 0;
  selected_tile_index_ = -1;
  selected_routine_index_ = -1;
  animation_playing_ = false;
  frame_timer_ = 0.0f;
  zsm_dirty_ = true;
  preview_needs_update_ = true;
}

void SpriteEditor::LoadZsmFile(const std::string& path) {
  zsprite::ZSprite sprite;
  status_ = sprite.Load(path);
  if (status_.ok()) {
    custom_sprites_.push_back(std::move(sprite));
    custom_sprite_paths_.push_back(path);
    project::SpriteAssetBinding binding;
    binding.zsm_path = path;
    std::copy_n(current_sheets_, 8, binding.sheets.begin());
    if (project()) {
      for (const auto& saved : project()->sprite_assets) {
        if (project()->GetAbsolutePath(saved.zsm_path) == path) {
          binding = saved;
          break;
        }
      }
    }
    custom_sprite_bindings_.push_back(std::move(binding));
    current_custom_sprite_index_ = static_cast<int>(custom_sprites_.size()) - 1;
    current_frame_ = custom_sprites_.back().editor.Frames.empty() ? -1 : 0;
    current_animation_index_ =
        custom_sprites_.back().animations.empty() ? -1 : 0;
    selected_tile_index_ = -1;
    selected_routine_index_ = -1;
    animation_playing_ = false;
    frame_timer_ = 0.0f;
    zsm_dirty_ = false;
    preview_needs_update_ = true;
    ApplyCurrentSpriteBinding();
    (void)CheckCurrentSpriteSource();
  }
}

void SpriteEditor::SaveZsmFile(const std::string& path) {
  if (current_custom_sprite_index_ >= 0 &&
      current_custom_sprite_index_ < (int)custom_sprites_.size()) {
    status_ = custom_sprites_[current_custom_sprite_index_].Save(path);
    if (status_.ok()) {
      SetCurrentZsmPath(path);
      auto& binding = custom_sprite_bindings_[current_custom_sprite_index_];
      binding.zsm_path = path;
      if (project()) {
        auto& assets = project()->sprite_assets;
        auto found =
            std::find_if(assets.begin(), assets.end(), [&](const auto& asset) {
              return project()->GetAbsolutePath(asset.zsm_path) == path;
            });
        if (found == assets.end())
          assets.push_back(binding);
        else
          *found = binding;
      }
      zsm_dirty_ = false;
    }
  }
}

void SpriteEditor::SaveZsmFileAs() {
  if (current_custom_sprite_index_ >= 0) {
    std::string path =
        util::FileDialogWrapper::ShowSaveFileDialog("sprite.zsm", "zsm");
    if (!path.empty()) {
      SaveZsmFile(path);
    }
  }
}

// ============================================================
// Properties Panel
// ============================================================

void SpriteEditor::DrawSpritePropertiesPanel() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  // Basic info
  Text(tr("Sprite Info"));
  Separator();

  static char name_buf[256];
  CopyStringToBuffer(sprite.sprName, name_buf);
  if (ImGui::InputText(tr("Name"), name_buf, sizeof(name_buf))) {
    sprite.sprName = name_buf;
    sprite.property_sprname.Text = name_buf;
    MarkSpriteMutated();
  }

  static char id_buf[32];
  CopyStringToBuffer(sprite.property_sprid.Text, id_buf);
  if (ImGui::InputText(tr("Sprite ID"), id_buf, sizeof(id_buf))) {
    sprite.property_sprid.Text = id_buf;
    MarkSpriteMutated();
  }

  Separator();
  DrawStatProperties();

  Separator();
  DrawBooleanProperties();
}

void SpriteEditor::DrawStatProperties() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  Text(tr("Stats"));

  // Use InputInt for numeric values
  int prize = ParseIntOrDefault(sprite.property_prize.Text);
  if (ImGui::InputInt(tr("Prize"), &prize)) {
    sprite.property_prize.Text = std::to_string(std::clamp(prize, 0, 255));
    MarkSpriteMutated();
  }

  int palette = ParseIntOrDefault(sprite.property_palette.Text);
  if (ImGui::InputInt(tr("Palette"), &palette)) {
    sprite.property_palette.Text = std::to_string(std::clamp(palette, 0, 7));
    MarkSpriteMutated();
  }

  int oamnbr = ParseIntOrDefault(sprite.property_oamnbr.Text);
  if (ImGui::InputInt(tr("OAM Count"), &oamnbr)) {
    sprite.property_oamnbr.Text = std::to_string(std::clamp(oamnbr, 0, 255));
    MarkSpriteMutated();
  }

  int hitbox = ParseIntOrDefault(sprite.property_hitbox.Text);
  if (ImGui::InputInt(tr("Hitbox"), &hitbox)) {
    sprite.property_hitbox.Text = std::to_string(std::clamp(hitbox, 0, 255));
    MarkSpriteMutated();
  }

  int health = ParseIntOrDefault(sprite.property_health.Text);
  if (ImGui::InputInt(tr("Health"), &health)) {
    sprite.property_health.Text = std::to_string(std::clamp(health, 0, 255));
    MarkSpriteMutated();
  }

  int damage = ParseIntOrDefault(sprite.property_damage.Text);
  if (ImGui::InputInt(tr("Damage"), &damage)) {
    sprite.property_damage.Text = std::to_string(std::clamp(damage, 0, 255));
    MarkSpriteMutated();
  }
}

void SpriteEditor::DrawBooleanProperties() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  Text(tr("Behavior Flags"));

  // Two columns for boolean properties
  if (ImGui::BeginTable("BoolProps", 2, ImGuiTableFlags_None)) {
    // Column 1
    ImGui::TableNextColumn();
    if (ImGui::Checkbox(tr("Blockable"), &sprite.property_blockable.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Can Fall"), &sprite.property_canfall.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Collision Layer"),
                        &sprite.property_collisionlayer.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Custom Death"),
                        &sprite.property_customdeath.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Damage Sound"),
                        &sprite.property_damagesound.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Deflect Arrows"),
                        &sprite.property_deflectarrows.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Deflect Projectiles"),
                        &sprite.property_deflectprojectiles.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Fast"), &sprite.property_fast.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Harmless"), &sprite.property_harmless.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Impervious"),
                        &sprite.property_impervious.IsChecked))
      MarkSpriteMutated();

    // Column 2
    ImGui::TableNextColumn();
    if (ImGui::Checkbox(tr("Impervious Arrow"),
                        &sprite.property_imperviousarrow.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Impervious Melee"),
                        &sprite.property_imperviousmelee.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Interaction"),
                        &sprite.property_interaction.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Is Boss"), &sprite.property_isboss.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Persist"), &sprite.property_persist.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Shadow"), &sprite.property_shadow.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Small Shadow"),
                        &sprite.property_smallshadow.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Stasis"), &sprite.property_statis.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Statue"), &sprite.property_statue.IsChecked))
      MarkSpriteMutated();
    if (ImGui::Checkbox(tr("Water Sprite"),
                        &sprite.property_watersprite.IsChecked))
      MarkSpriteMutated();

    ImGui::EndTable();
  }
}

// ============================================================
// Animation Panel
// ============================================================

void SpriteEditor::DrawAnimationPanel() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  if (ImGui::Button(tr("Copy draw tables (ASM)"))) {
    auto tables = ExportCurrentSpriteDraw();
    draw_export_status_ = tables.status();
    if (tables.ok())
      ImGui::SetClipboardText(tables->c_str());
  }
  HOVER_HINT(
      "Copy a draw-table candidate. Does not register or patch a sprite.");
  if (!draw_export_status_.ok())
    ImGui::TextWrapped("%s", draw_export_status_.ToString().c_str());

  // Playback controls
  if (animation_playing_) {
    if (ImGui::Button(ICON_MD_STOP " Stop")) {
      animation_playing_ = false;
    }
  } else {
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Play")) {
      animation_playing_ = true;
      frame_timer_ = 0.0f;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SKIP_PREVIOUS)) {
    if (current_frame_ > 0)
      current_frame_--;
    preview_needs_update_ = true;
  }
  HOVER_HINT("Previous Frame");
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SKIP_NEXT)) {
    if (current_frame_ < (int)sprite.editor.Frames.size() - 1)
      current_frame_++;
    preview_needs_update_ = true;
  }
  HOVER_HINT("Next Frame");
  ImGui::SameLine();
  Text(tr("Frame: %d / %d"), current_frame_,
       (int)sprite.editor.Frames.size() - 1);

  Separator();

  // Animation list
  Text(tr("Animations"));
  if (ImGui::Button(ICON_MD_ADD " Add Animation")) {
    int frame_count = static_cast<int>(sprite.editor.Frames.size());
    sprite.animations.emplace_back(0, std::clamp(frame_count - 1, 0, 255), 1,
                                   "New Animation");
    MarkSpriteMutated();
  }
  HOVER_HINT("Add a new animation sequence");

  if (ImGui::BeginChild("AnimList", ImVec2(0, 120), true)) {
    for (size_t i = 0; i < sprite.animations.size(); i++) {
      auto& anim = sprite.animations[i];
      std::string label = anim.frame_name.empty() ? "Unnamed" : anim.frame_name;
      ImGui::PushID(static_cast<int>(i));
      if (Selectable(label.c_str(), current_animation_index_ == (int)i)) {
        current_animation_index_ = static_cast<int>(i);
        current_frame_ = anim.frame_start;
        frame_timer_ = 0;
        preview_needs_update_ = true;
      }
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  // Edit selected animation
  if (current_animation_index_ >= 0 &&
      current_animation_index_ < (int)sprite.animations.size()) {
    auto& anim = sprite.animations[current_animation_index_];

    Separator();
    Text(tr("Animation Properties"));

    static char anim_name[128];
    CopyStringToBuffer(anim.frame_name, anim_name);
    if (ImGui::InputText(tr("Name##Anim"), anim_name, sizeof(anim_name))) {
      anim.frame_name = anim_name;
      MarkSpriteMutated();
    }

    int start = anim.frame_start;
    int end = anim.frame_end;
    int speed = anim.frame_speed;

    if (ImGui::SliderInt(
            tr("Start Frame"), &start, 0,
            std::clamp((int)sprite.editor.Frames.size() - 1, 0, 255))) {
      anim.frame_start = static_cast<uint8_t>(start);
      anim.frame_end = std::max(anim.frame_start, anim.frame_end);
      end = anim.frame_end;
      MarkSpriteMutated();
    }
    if (ImGui::SliderInt(
            tr("End Frame"), &end, 0,
            std::clamp((int)sprite.editor.Frames.size() - 1, 0, 255))) {
      anim.frame_end = static_cast<uint8_t>(end);
      anim.frame_start = std::min(anim.frame_start, anim.frame_end);
      MarkSpriteMutated();
    }
    if (ImGui::SliderInt(tr("Ticks per frame (60 Hz)"), &speed, 1, 255)) {
      anim.frame_speed = static_cast<uint8_t>(speed);
      MarkSpriteMutated();
    }

    auto& actions =
        custom_sprite_bindings_[current_custom_sprite_index_].behavior.actions;
    bool used_by_action =
        std::any_of(actions.begin(), actions.end(), [&](const auto& action) {
          return action.animation == current_animation_index_;
        });
    ImGui::BeginDisabled(used_by_action);
    if (ImGui::Button(tr("Delete Animation")) && sprite.animations.size() > 1) {
      for (auto& action : actions)
        if (action.animation > current_animation_index_)
          --action.animation;
      sprite.animations.erase(sprite.animations.begin() +
                              current_animation_index_);
      current_animation_index_ =
          std::min(current_animation_index_, (int)sprite.animations.size() - 1);
      MarkSpriteMutated();
    }
    ImGui::EndDisabled();
    if (used_by_action)
      ImGui::TextDisabled(
          "Assign another animation to referencing actions before deleting.");
    HOVER_HINT("Delete the selected animation");
  }

  Separator();
  DrawFrameEditor();
}

void SpriteEditor::DrawFrameEditor() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  Text(tr("Frames"));
  if (ImGui::Button(ICON_MD_ADD " Add Frame")) {
    if (sprite_authoring::AppendFrame(sprite)) {
      current_frame_ = static_cast<int>(sprite.editor.Frames.size()) - 1;
      selected_tile_index_ = -1;
      MarkSpriteMutated();
    }
  }
  HOVER_HINT("Add a new animation frame");
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DELETE " Delete Frame") &&
      sprite.editor.Frames.size() > 1 && current_frame_ >= 0) {
    if (sprite_authoring::DeleteFrame(sprite, current_frame_)) {
      current_frame_ =
          std::min(current_frame_, (int)sprite.editor.Frames.size() - 1);
      selected_tile_index_ = -1;
      frame_timer_ = 0;
      MarkSpriteMutated();
    }
  }
  HOVER_HINT("Delete the current frame and repair animation ranges");
  if (ImGui::Button(tr("Duplicate Frame")) && current_frame_ >= 0 &&
      sprite_authoring::AppendFrame(sprite, current_frame_)) {
    current_frame_ = static_cast<int>(sprite.editor.Frames.size()) - 1;
    selected_tile_index_ = -1;
    MarkSpriteMutated();
  }
  ImGui::TextDisabled("Up to 256 frames; duplicate appends a frame.");

  // Frame selector
  if (ImGui::BeginChild("FrameList", ImVec2(0, 80), true,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
    for (size_t i = 0; i < sprite.editor.Frames.size(); i++) {
      ImGui::PushID(static_cast<int>(i));
      std::string label = absl::StrFormat("F%d", i);
      if (Selectable(label.c_str(), current_frame_ == (int)i,
                     ImGuiSelectableFlags_None, ImVec2(40, 40))) {
        current_frame_ = static_cast<int>(i);
        preview_needs_update_ = true;
      }
      ImGui::SameLine();
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  // Edit tiles in current frame
  if (current_frame_ >= 0 &&
      current_frame_ < (int)sprite.editor.Frames.size()) {
    auto& frame = sprite.editor.Frames[current_frame_];

    Separator();
    Text(tr("Tiles in Frame %d"), current_frame_);

    if (ImGui::Button(ICON_MD_ADD " Add Tile")) {
      frame.Tiles.emplace_back();
      frame.Tiles.back().x = sprite_authoring::kOriginX;
      frame.Tiles.back().y = sprite_authoring::kOriginY;
      selected_tile_index_ = static_cast<int>(frame.Tiles.size()) - 1;
      MarkSpriteMutated();
      preview_needs_update_ = true;
    }
    HOVER_HINT("Add a new tile to this frame");

    if (ImGui::BeginChild("TileList", ImVec2(0, 100), true)) {
      for (size_t i = 0; i < frame.Tiles.size(); i++) {
        auto& tile = frame.Tiles[i];
        std::string label = absl::StrFormat("Tile %d (ID: %d)", i, tile.id);
        if (Selectable(label.c_str(), selected_tile_index_ == (int)i)) {
          selected_tile_index_ = static_cast<int>(i);
        }
      }
    }
    ImGui::EndChild();

    // Edit selected tile
    if (selected_tile_index_ >= 0 &&
        selected_tile_index_ < (int)frame.Tiles.size()) {
      auto& tile = frame.Tiles[selected_tile_index_];

      int tile_id = tile.id;
      if (ImGui::InputInt(tr("Tile ID"), &tile_id)) {
        tile.id = static_cast<uint16_t>(std::clamp(tile_id, 0, 511));
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }

      int x = sprite_authoring::OffsetX(tile),
          y = sprite_authoring::OffsetY(tile);
      if (ImGui::InputInt(tr("X offset"), &x)) {
        tile.x = static_cast<uint8_t>(std::clamp(x, -128, 127) +
                                      sprite_authoring::kOriginX);
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }
      if (ImGui::InputInt(tr("Y offset"), &y)) {
        tile.y = static_cast<uint8_t>(std::clamp(y, -112, 143) +
                                      sprite_authoring::kOriginY);
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }

      int pal = tile.palette;
      if (ImGui::SliderInt(tr("Palette##Tile"), &pal, 0, 7)) {
        tile.palette = static_cast<uint8_t>(pal);
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }

      int priority = tile.priority;
      if (ImGui::SliderInt(tr("BG priority"), &priority, 0, 3)) {
        tile.priority = static_cast<uint8_t>(priority);
        MarkSpriteMutated();
      }
      ImGui::TextDisabled(
          "BG priority is stored; preview has no background layers.");
      if (ImGui::Checkbox(tr("16x16"), &tile.size)) {
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }
      ImGui::SameLine();
      if (ImGui::Checkbox(tr("Flip X"), &tile.mirror_x)) {
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }
      ImGui::SameLine();
      if (ImGui::Checkbox(tr("Flip Y"), &tile.mirror_y)) {
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }

      if (ImGui::Button(tr("Duplicate Tile"))) {
        const auto copy = tile;
        frame.Tiles.push_back(copy);
        selected_tile_index_ = static_cast<int>(frame.Tiles.size()) - 1;
        MarkSpriteMutated();
      }
      if (ImGui::Button(tr("Delete Tile"))) {
        frame.Tiles.erase(frame.Tiles.begin() + selected_tile_index_);
        selected_tile_index_ = -1;
        MarkSpriteMutated();
        preview_needs_update_ = true;
      }
      HOVER_HINT("Delete the selected tile");
    }
  }
}

void SpriteEditor::UpdateAnimationPlayback(float delta_time) {
  if (!animation_playing_ || current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >= (int)custom_sprites_.size()) {
    return;
  }

  auto& sprite = custom_sprites_[current_custom_sprite_index_];
  if (current_animation_index_ < 0 ||
      current_animation_index_ >= (int)sprite.animations.size()) {
    return;
  }

  auto& anim = sprite.animations[current_animation_index_];

  if (sprite_authoring::Advance(anim, sprite.editor.Frames.size(), delta_time,
                                current_frame_, frame_timer_)) {
    preview_needs_update_ = true;
  }
}

// ============================================================
// User Routines Panel
// ============================================================

void SpriteEditor::DrawUserRoutinesPanel() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  if (ImGui::Button(ICON_MD_ADD " Add Routine")) {
    sprite.userRoutines.emplace_back("New Routine", "; ASM code here\n");
    MarkSpriteMutated();
  }
  HOVER_HINT("Add a new ASM routine");

  // Routine list
  if (ImGui::BeginChild("RoutineList", ImVec2(0, 100), true)) {
    for (size_t i = 0; i < sprite.userRoutines.size(); i++) {
      auto& routine = sprite.userRoutines[i];
      if (Selectable(routine.name.c_str(), selected_routine_index_ == (int)i)) {
        selected_routine_index_ = static_cast<int>(i);
      }
    }
  }
  ImGui::EndChild();

  // Edit selected routine
  if (selected_routine_index_ >= 0 &&
      selected_routine_index_ < (int)sprite.userRoutines.size()) {
    auto& routine = sprite.userRoutines[selected_routine_index_];

    Separator();

    static char routine_name[128];
    CopyStringToBuffer(routine.name, routine_name);
    if (ImGui::InputText(tr("Routine Name"), routine_name,
                         sizeof(routine_name))) {
      routine.name = routine_name;
      MarkSpriteMutated();
    }

    Text(tr("ASM Code:"));

    // Multiline text input for code
    static char code_buffer[16384];
    CopyStringToBuffer(routine.code, code_buffer);
    if (ImGui::InputTextMultiline("##RoutineCode", code_buffer,
                                  sizeof(code_buffer), ImVec2(-1, 200))) {
      routine.code = code_buffer;
      MarkSpriteMutated();
    }

    if (ImGui::Button(tr("Delete Routine"))) {
      sprite.userRoutines.erase(sprite.userRoutines.begin() +
                                selected_routine_index_);
      selected_routine_index_ = -1;
      MarkSpriteMutated();
    }
    HOVER_HINT("Delete the selected routine");
  }
}

// ============================================================
// Graphics Pipeline
// ============================================================

void SpriteEditor::LoadSpriteGraphicsBuffer() {
  // Combine selected sheets (current_sheets_[0-7]) into single 8BPP buffer
  // Layout: 16 tiles per row, 8 rows per sheet, 8 sheets total = 64 tile rows
  // Buffer size: 0x10000 bytes (65536)

  sprite_gfx_buffer_.assign(0x10000, 0);
  graphics_binding_status_ = absl::OkStatus();

  // Each sheet is 128x32 pixels (128 bytes per row, 32 rows) = 4096 bytes
  // We combine 8 sheets vertically: 128x256 pixels total
  constexpr int kSheetWidth = 128;
  constexpr int kSheetHeight = 32;
  constexpr int kRowStride = 128;

  for (int sheet_idx = 0; sheet_idx < 8; sheet_idx++) {
    uint8_t sheet_id = current_sheets_[sheet_idx];
    if (sheet_id >= gfx::Arena::Get().gfx_sheets().size()) {
      graphics_binding_status_ = absl::OutOfRangeError(
          "Sprite graphics sheet unavailable in loaded ROM");
      continue;
    }

    auto& sheet = gfx::Arena::Get().gfx_sheets().at(sheet_id);
    if (!sheet.is_active() || sheet.size() == 0) {
      graphics_binding_status_ = absl::FailedPreconditionError(
          "Sprite graphics sheet has not been loaded");
      continue;
    }

    // Copy sheet data to buffer at appropriate offset
    // Each sheet occupies 8 tile rows (8 * 8 scanlines = 64 scanlines)
    // Offset = sheet_idx * (8 tile rows * 1024 bytes per tile row)
    // But sheets are 32 pixels tall (4 tile rows), so:
    // Offset = sheet_idx * 4 * 1024 = sheet_idx * 4096
    int dest_offset = sheet_idx * (kSheetHeight * kRowStride);

    const uint8_t* src_data = sheet.data();
    size_t copy_size =
        std::min(sheet.size(), static_cast<size_t>(kSheetWidth * kSheetHeight));

    if (dest_offset + copy_size <= sprite_gfx_buffer_.size()) {
      std::memcpy(sprite_gfx_buffer_.data() + dest_offset, src_data, copy_size);
    }
  }

  // Update drawer with new buffer
  sprite_drawer_.SetGraphicsBuffer(sprite_gfx_buffer_.data());
  gfx_buffer_loaded_ = true;
}

void SpriteEditor::LoadSpritePalettes(bool use_asset_binding) {
  // Load sprite palettes from ROM palette groups
  // ALTTP sprites use a combination of palette groups:
  // - Rows 0-1: Global sprite palettes (shared by all sprites)
  // - Rows 2-7: Aux palettes (vary by sprite type)
  //
  // For simplicity, we load global_sprites which contains the main
  // sprite palettes. More accurate rendering would require looking up
  // which aux palette group each sprite type uses.

  if (!rom_ || !rom_->is_loaded()) {
    return;
  }

  // Build combined sprite palette from global + aux groups
  sprite_palettes_.clear();

  // Add global sprite palettes (typically 2 palettes, 16 colors each)
  if (!game_data())
    return;
  const auto& global = game_data()->palette_groups.global_sprites;
  for (size_t i = 0; i < global.size() && i < 8; i++) {
    sprite_palettes_.AddPalette(global.palette(i));
  }

  // If we don't have 8 palettes yet, fill with aux palettes
  const auto& aux1 = game_data()->palette_groups.sprites_aux1;
  const auto& aux2 = game_data()->palette_groups.sprites_aux2;
  const auto& aux3 = game_data()->palette_groups.sprites_aux3;

  // Pad to 8 palettes total for proper OAM palette mapping
  while (sprite_palettes_.size() < 8) {
    if (sprite_palettes_.size() < 4 && aux1.size() > 0) {
      sprite_palettes_.AddPalette(
          aux1.palette(sprite_palettes_.size() % aux1.size()));
    } else if (sprite_palettes_.size() < 6 && aux2.size() > 0) {
      sprite_palettes_.AddPalette(
          aux2.palette((sprite_palettes_.size() - 4) % aux2.size()));
    } else if (aux3.size() > 0) {
      sprite_palettes_.AddPalette(
          aux3.palette((sprite_palettes_.size() - 6) % aux3.size()));
    } else {
      // Fallback: add empty palette
      sprite_palettes_.AddPalette(gfx::SnesPalette());
    }
  }

  palette_binding_status_ = absl::OkStatus();
  if (const auto* binding =
          use_asset_binding ? current_sprite_binding() : nullptr) {
    auto palettes = sprite_authoring::BindPaletteRows(
        *binding, sprite_palettes_, global, aux1, aux2, aux3);
    palette_binding_status_ = palettes.status();
    if (palettes.ok())
      sprite_palettes_ = std::move(*palettes);
    else
      sprite_palettes_.clear();
  }
  sprite_drawer_.SetPalettes(&sprite_palettes_);
}

void SpriteEditor::LoadSheetsForSprite(const std::array<uint8_t, 4>& sheets) {
  // Load the required sheets for a vanilla sprite
  bool changed = false;
  for (int i = 0; i < 4; i++) {
    if (sheets[i] != 0 && current_sheets_[i] != sheets[i]) {
      current_sheets_[i] = sheets[i];
      changed = true;
    }
  }

  if (changed) {
    gfx_buffer_loaded_ = false;
    vanilla_preview_needs_update_ = true;
  }
}

void SpriteEditor::RenderVanillaSprite(const zelda3::SpriteOamLayout& layout) {
  // Ensure graphics buffer is loaded
  if (!gfx_buffer_loaded_ && sheets_loaded_) {
    LoadSpriteGraphicsBuffer();
    LoadSpritePalettes(false);
  }

  // Initialize vanilla preview bitmap if needed. The helper also queues a
  // CREATE texture command so canvas_rendering doesn't silently skip the
  // draw, and stamps the bitmap as kCompositeOutput.
  internal::EnsureSpritePreviewBitmapReady(vanilla_preview_bitmap_, 128, 128, 8,
                                           sprite_gfx_buffer_);

  if (!sprite_drawer_.IsReady() || !vanilla_preview_needs_update_) {
    return;
  }

  // Clear and render
  sprite_drawer_.ClearBitmap(vanilla_preview_bitmap_);

  // Origin is center of bitmap
  int origin_x = 64;
  int origin_y = 64;

  sprite_drawer_.DrawFrame(vanilla_preview_bitmap_,
                           sprite_authoring::CopyVanillaLayout(layout),
                           origin_x, origin_y);

  // Build combined 128-color palette (8 sub-palettes × 16 colors)
  // and apply to bitmap for proper color rendering
  if (sprite_palettes_.size() > 0) {
    gfx::SnesPalette combined_palette;
    for (size_t pal_idx = 0; pal_idx < 8 && pal_idx < sprite_palettes_.size();
         pal_idx++) {
      const auto& sub_pal = sprite_palettes_.palette(pal_idx);
      for (size_t col_idx = 0; col_idx < 16 && col_idx < sub_pal.size();
           col_idx++) {
        combined_palette.AddColor(sub_pal[col_idx]);
      }
      // Pad to 16 if sub-palette is smaller
      while (combined_palette.size() < (pal_idx + 1) * 16) {
        combined_palette.AddColor(gfx::SnesColor(0));
      }
    }
    vanilla_preview_bitmap_.SetPalette(combined_palette);
  }

  // Surface pixels and palette were just mutated above; queue an UPDATE so
  // the GPU texture reflects the new state on the next frame. Without this,
  // the texture stays frozen at first-CREATE state forever.
  internal::PublishSpritePreviewPixels(vanilla_preview_bitmap_);

  vanilla_preview_needs_update_ = false;
}

// ============================================================
// Canvas Rendering
// ============================================================

void SpriteEditor::RenderZSpriteFrame(int frame_index) {
  if (current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >= (int)custom_sprites_.size()) {
    return;
  }

  auto& sprite = custom_sprites_[current_custom_sprite_index_];
  if (frame_index < 0 || frame_index >= (int)sprite.editor.Frames.size()) {
    return;
  }

  auto& frame = sprite.editor.Frames[frame_index];

  // Ensure graphics buffer is loaded
  if (!gfx_buffer_loaded_ && sheets_loaded_) {
    LoadSpriteGraphicsBuffer();
    LoadSpritePalettes();
  }

  // The sprite_canvas_ displays a post-render composite (pixels come from
  // SpriteDrawer, not direct user paint). Declaring the role lets future
  // canvas surfaces (cursor hints, context-menu items) default appropriately.
  sprite_canvas_.GetConfig().role = gui::CanvasRole::kCompositeOutput;

  // Initialize preview bitmap if needed. The helper also queues a CREATE
  // texture command so canvas_rendering doesn't silently skip the draw, and
  // stamps the bitmap as kCompositeOutput.
  internal::EnsureSpritePreviewBitmapReady(sprite_preview_bitmap_, 256, 256, 8,
                                           sprite_gfx_buffer_);

  // Only render if drawer is ready
  if (sprite_drawer_.IsReady() && preview_needs_update_) {
    // Clear and render to preview bitmap
    sprite_drawer_.ClearBitmap(sprite_preview_bitmap_);

    // Origin is center of canvas (128, 128 for 256x256 bitmap).
    if (palette_binding_status_.ok() && graphics_binding_status_.ok())
      sprite_drawer_.DrawFrame(sprite_preview_bitmap_, frame, 128, 128);

    // Build combined 128-color palette and apply to bitmap
    if (sprite_palettes_.size() > 0) {
      gfx::SnesPalette combined_palette;
      for (size_t pal_idx = 0; pal_idx < 8 && pal_idx < sprite_palettes_.size();
           pal_idx++) {
        const auto& sub_pal = sprite_palettes_.palette(pal_idx);
        for (size_t col_idx = 0; col_idx < 16 && col_idx < sub_pal.size();
             col_idx++) {
          combined_palette.AddColor(sub_pal[col_idx]);
        }
        // Pad to 16 if sub-palette is smaller
        while (combined_palette.size() < (pal_idx + 1) * 16) {
          combined_palette.AddColor(gfx::SnesColor(0));
        }
      }
      sprite_preview_bitmap_.SetPalette(combined_palette);
    }

    // Surface pixels and palette were just mutated above; queue an UPDATE so
    // the GPU texture reflects the new frame on the next paint.
    internal::PublishSpritePreviewPixels(sprite_preview_bitmap_);

    // Mark as updated
    preview_needs_update_ = false;
  }

  // Draw the preview bitmap on canvas
  if (sprite_preview_bitmap_.is_active()) {
    sprite_canvas_.DrawBitmap(sprite_preview_bitmap_, 0, 0, 2.0f);
  }

  // Draw tile outlines for selection (over the bitmap)
  if (show_tile_grid_) {
    for (size_t i = 0; i < frame.Tiles.size(); i++) {
      const auto& tile = frame.Tiles[i];
      int tile_size = tile.size ? 16 : 8;

      // Convert signed tile position to canvas position
      int signed_x = sprite_authoring::OffsetX(tile);
      int signed_y = sprite_authoring::OffsetY(tile);

      int canvas_x = 128 + signed_x;
      int canvas_y = 128 + signed_y;

      // Highlight selected tile
      ImVec4 color = (selected_tile_index_ == static_cast<int>(i))
                         ? ImVec4(0.0f, 1.0f, 0.0f, 0.8f)  // Green for selected
                         : ImVec4(1.0f, 1.0f, 0.0f, 0.3f);  // Yellow for others

      // Match the 2x bitmap scale used above.
      sprite_canvas_.DrawRect(canvas_x * 2, canvas_y * 2, tile_size * 2,
                              tile_size * 2, color);
    }
  }
}

void SpriteEditor::DrawZSpriteOnCanvas() {
  if (ImGui::BeginChild(gui::GetID("##ZSpriteCanvas"),
                        ImGui::GetContentRegionAvail(), true)) {
    sprite_canvas_.DrawBackground();
    sprite_canvas_.DrawContextMenu();

    // Render current frame if we have a sprite selected
    if (current_custom_sprite_index_ >= 0 &&
        current_custom_sprite_index_ < (int)custom_sprites_.size()) {
      RenderZSpriteFrame(current_frame_);
    }

    sprite_canvas_.DrawGrid();
    sprite_canvas_.DrawOverlay();

    // Display current frame info
    if (current_custom_sprite_index_ >= 0) {
      auto& sprite = custom_sprites_[current_custom_sprite_index_];
      ImGui::SetCursorPos(ImVec2(10, 10));
      char label[256];
      std::snprintf(label, sizeof(label), tr("Frame: %d | Tiles: %d"),
                    current_frame_,
                    current_frame_ >= 0 &&
                            current_frame_ < (int)sprite.editor.Frames.size()
                        ? (int)sprite.editor.Frames[current_frame_].Tiles.size()
                        : 0);
      const ImVec2 position = ImGui::GetCursorScreenPos();
      const ImVec2 size = ImGui::CalcTextSize(label);
      // Preview colors come from the ROM; keep status legible on any palette.
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(position.x - 3, position.y - 2),
          ImVec2(position.x + size.x + 3, position.y + size.y + 2),
          IM_COL32(24, 24, 24, 255), 3.0f);
      ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", label);
    }
  }
  ImGui::EndChild();
}

// ============================================================
// Undo/Redo Helpers
// ============================================================

SpriteSnapshot SpriteEditor::CaptureCurrentSpriteSnapshot() const {
  SpriteSnapshot snapshot;
  snapshot.sprite_index = current_custom_sprite_index_;
  snapshot.current_frame = current_frame_;
  snapshot.current_animation_index = current_animation_index_;
  if (current_custom_sprite_index_ >= 0 &&
      current_custom_sprite_index_ < static_cast<int>(custom_sprites_.size())) {
    snapshot.sprite_data = custom_sprites_[current_custom_sprite_index_];
    snapshot.binding = custom_sprite_bindings_[current_custom_sprite_index_];
  }
  return snapshot;
}

void SpriteEditor::RestoreFromSnapshot(const SpriteSnapshot& snapshot) {
  if (snapshot.sprite_index < 0 ||
      snapshot.sprite_index >= static_cast<int>(custom_sprites_.size())) {
    return;
  }
  current_custom_sprite_index_ = snapshot.sprite_index;
  current_frame_ = snapshot.current_frame;
  current_animation_index_ = snapshot.current_animation_index;
  custom_sprites_[snapshot.sprite_index] = snapshot.sprite_data;
  custom_sprite_bindings_[snapshot.sprite_index] = snapshot.binding;
  ApplyCurrentSpriteBinding();
  preview_needs_update_ = true;
  zsm_dirty_ = true;
}

void SpriteEditor::BeginUndoTransaction() {
  if (undo_snapshot_pending_) {
    return;  // Already in a transaction
  }
  undo_before_snapshot_ = CaptureCurrentSpriteSnapshot();
  undo_snapshot_pending_ = true;
}

void SpriteEditor::CommitUndoTransaction() {
  if (!undo_snapshot_pending_) {
    return;
  }

  if (!sprite_mutated_this_frame_) {
    // No mutation happened; discard the pending snapshot so we can
    // capture a fresh one next frame.
    undo_snapshot_pending_ = false;
    return;
  }

  undo_snapshot_pending_ = false;
  sprite_mutated_this_frame_ = false;

  auto after = CaptureCurrentSpriteSnapshot();
  auto restore_fn = [this](const SpriteSnapshot& snapshot) {
    RestoreFromSnapshot(snapshot);
  };
  undo_manager_.Push(std::make_unique<SpriteEditAction>(
      std::move(undo_before_snapshot_), std::move(after),
      std::move(restore_fn)));
}

void SpriteEditor::MarkSpriteMutated() {
  behavior_candidate_.clear();
  preview_needs_update_ = true;
  zsm_dirty_ = true;
  sprite_mutated_this_frame_ = true;
}

void SpriteEditor::EnsureCustomSpritePaths() {
  if (custom_sprite_paths_.size() < custom_sprites_.size()) {
    custom_sprite_paths_.resize(custom_sprites_.size());
  }
}

const std::string& SpriteEditor::GetCurrentZsmPath() const {
  static const std::string kEmptyPath;
  if (current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >=
          static_cast<int>(custom_sprite_paths_.size())) {
    return kEmptyPath;
  }
  return custom_sprite_paths_[current_custom_sprite_index_];
}

void SpriteEditor::SetCurrentZsmPath(const std::string& path) {
  EnsureCustomSpritePaths();
  if (current_custom_sprite_index_ < 0 ||
      current_custom_sprite_index_ >=
          static_cast<int>(custom_sprite_paths_.size())) {
    return;
  }
  custom_sprite_paths_[current_custom_sprite_index_] = path;
}

}  // namespace editor
}  // namespace yaze
