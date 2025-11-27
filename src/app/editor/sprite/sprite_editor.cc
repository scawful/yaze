#include "sprite_editor.h"

#include <algorithm>

#include "app/editor/sprite/zsprite.h"
#include "app/editor/system/editor_card_registry.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/ui_helpers.h"
#include "util/file_util.h"
#include "util/hex.h"
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

void SpriteEditor::Initialize() {
  if (!dependencies_.card_registry)
    return;
  auto* card_registry = dependencies_.card_registry;

  card_registry->RegisterCard({.card_id = "sprite.vanilla_editor",
                               .display_name = "Vanilla Sprites",
                               .window_title = " Vanilla Sprites",
                               .icon = ICON_MD_SMART_TOY,
                               .category = "Sprite",
                               .shortcut_hint = "Alt+Shift+1",
                               .priority = 10,
                               .enabled_condition = [this]() { return rom_->is_loaded(); },
                               .disabled_tooltip = "Load a ROM first"});
  card_registry->RegisterCard({.card_id = "sprite.custom_editor",
                               .display_name = "Custom Sprites",
                               .window_title = " Custom Sprites",
                               .icon = ICON_MD_ADD_CIRCLE,
                               .category = "Sprite",
                               .shortcut_hint = "Alt+Shift+2",
                               .priority = 20,
                               .enabled_condition = [this]() { return rom_->is_loaded(); },
                               .disabled_tooltip = "Load a ROM first"});

  card_registry->ShowCard("sprite.vanilla_editor");
}

absl::Status SpriteEditor::Load() {
  gfx::ScopedTimer timer("SpriteEditor::Load");
  return absl::OkStatus();
}

absl::Status SpriteEditor::Update() {
  if (rom()->is_loaded() && !sheets_loaded_) {
    sheets_loaded_ = true;
  }

  // Update animation playback
  float current_time = ImGui::GetTime();
  float delta_time = current_time - last_frame_time_;
  last_frame_time_ = current_time;
  UpdateAnimationPlayback(delta_time);

  if (!dependencies_.card_registry)
    return absl::OkStatus();
  auto* card_registry = dependencies_.card_registry;

  static gui::EditorCard vanilla_card("Vanilla Sprites", ICON_MD_SMART_TOY);
  static gui::EditorCard custom_card("Custom Sprites", ICON_MD_ADD_CIRCLE);

  vanilla_card.SetDefaultSize(900, 700);
  custom_card.SetDefaultSize(1000, 700);

  bool* vanilla_visible =
      card_registry->GetVisibilityFlag("sprite.vanilla_editor");
  if (vanilla_visible && *vanilla_visible) {
    if (vanilla_card.Begin(vanilla_visible)) {
      DrawVanillaSpriteEditor();
    }
    vanilla_card.End();
  }

  bool* custom_visible =
      card_registry->GetVisibilityFlag("sprite.custom_editor");
  if (custom_visible && *custom_visible) {
    if (custom_card.Begin(custom_visible)) {
      DrawCustomSprites();
    }
    custom_card.End();
  }

  return status_.ok() ? absl::OkStatus() : status_;
}

absl::Status SpriteEditor::Save() {
  if (current_custom_sprite_index_ >= 0 &&
      current_custom_sprite_index_ < static_cast<int>(custom_sprites_.size())) {
    if (current_zsm_path_.empty()) {
      SaveZsmFileAs();
    } else {
      SaveZsmFile(current_zsm_path_);
    }
  }
  return absl::OkStatus();
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

    if (ImGui::BeginTabBar("SpriteTabBar", kSpriteTabBarFlags)) {
      if (ImGui::TabItemButton(ICON_MD_ADD, kSpriteTabBarFlags)) {
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

      ImGui::EndTabBar();
    }

    TableNextColumn();
    if (sheets_loaded_) {
      DrawCurrentSheets();
    }
    ImGui::EndTable();
  }
}

void SpriteEditor::DrawSpriteCanvas() {
  static bool flip_x = false;
  static bool flip_y = false;
  if (ImGui::BeginChild(gui::GetID("##SpriteCanvas"),
                        ImGui::GetContentRegionAvail(), true)) {
    sprite_canvas_.DrawBackground();
    sprite_canvas_.DrawContextMenu();
    sprite_canvas_.DrawGrid();
    sprite_canvas_.DrawOverlay();

    if (ImGui::BeginTable("##OAMTable", 7, ImGuiTableFlags_Resizable,
                          ImVec2(0, 0))) {
      TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Tile", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Palette", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Flip X", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Flip Y", ImGuiTableColumnFlags_WidthStretch);
      TableHeadersRow();
      TableNextRow();

      TableNextColumn();
      gui::InputHexWord("", &oam_config_.x);

      TableNextColumn();
      gui::InputHexWord("", &oam_config_.y);

      TableNextColumn();
      gui::InputHexByte("", &oam_config_.tile);

      TableNextColumn();
      gui::InputHexByte("", &oam_config_.palette);

      TableNextColumn();
      gui::InputHexByte("", &oam_config_.priority);

      TableNextColumn();
      if (ImGui::Checkbox("##XFlip", &flip_x)) {
        oam_config_.flip_x = flip_x;
      }

      TableNextColumn();
      if (ImGui::Checkbox("##YFlip", &flip_y)) {
        oam_config_.flip_y = flip_y;
      }

      ImGui::EndTable();
    }

    DrawAnimationFrames();
  }
  ImGui::EndChild();
}

void SpriteEditor::DrawCurrentSheets() {
  if (ImGui::BeginChild(gui::GetID("sheet_label"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0), true,
                        ImGuiWindowFlags_NoDecoration)) {
    for (int i = 0; i < 8; i++) {
      std::string sheet_label = absl::StrFormat("Sheet %d", i);
      gui::InputHexByte(sheet_label.c_str(), &current_sheets_[i]);
      if (i % 2 == 0)
        ImGui::SameLine();
    }

    graphics_sheet_canvas_.DrawBackground();
    graphics_sheet_canvas_.DrawContextMenu();
    graphics_sheet_canvas_.DrawTileSelector(32);
    for (int i = 0; i < 8; i++) {
      graphics_sheet_canvas_.DrawBitmap(
          gfx::Arena::Get().gfx_sheets().at(current_sheets_[i]), 1,
          (i * 0x40) + 1, 2);
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
        current_sprite_id_ = i;
        if (!active_sprites_.contains(i)) {
          active_sprites_.push_back(i);
        }
      }
      i++;
    }
  }
  ImGui::EndChild();
}

void SpriteEditor::DrawAnimationFrames() {
  if (ImGui::Button("Add Frame")) {
    // Add a new frame
  }
  if (ImGui::Button("Remove Frame")) {
    // Remove the current frame
  }
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

    DrawCustomSpritesMetadata();

    TableNextColumn();
    DrawZSpriteOnCanvas();

    TableNextColumn();
    DrawCurrentSheets();

    EndTable();
  }
}

void SpriteEditor::DrawCustomSpritesMetadata() {
  // File operations toolbar
  if (ImGui::Button(ICON_MD_ADD " New")) {
    CreateNewZSprite();
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Open")) {
    std::string file_path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!file_path.empty()) {
      LoadZsmFile(file_path);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE " Save")) {
    if (current_custom_sprite_index_ >= 0) {
      if (current_zsm_path_.empty()) {
        SaveZsmFileAs();
      } else {
        SaveZsmFile(current_zsm_path_);
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE_AS " Save As")) {
    SaveZsmFileAs();
  }

  Separator();

  // Sprite list
  Text("Loaded Sprites:");
  if (ImGui::BeginChild("SpriteList", ImVec2(0, 100), true)) {
    for (size_t i = 0; i < custom_sprites_.size(); i++) {
      std::string label = custom_sprites_[i].sprName.empty()
                              ? "Unnamed Sprite"
                              : custom_sprites_[i].sprName;
      if (Selectable(label.c_str(), current_custom_sprite_index_ == (int)i)) {
        current_custom_sprite_index_ = static_cast<int>(i);
        preview_needs_update_ = true;
      }
    }
  }
  ImGui::EndChild();

  Separator();

  // Show properties for selected sprite
  if (current_custom_sprite_index_ >= 0 &&
      current_custom_sprite_index_ < (int)custom_sprites_.size()) {
    if (ImGui::BeginTabBar("SpriteDataTabs")) {
      if (ImGui::BeginTabItem("Properties")) {
        DrawSpritePropertiesPanel();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Animations")) {
        DrawAnimationPanel();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Routines")) {
        DrawUserRoutinesPanel();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  } else {
    Text("No sprite selected");
  }
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
  current_custom_sprite_index_ = static_cast<int>(custom_sprites_.size()) - 1;
  current_zsm_path_.clear();
  zsm_dirty_ = true;
  preview_needs_update_ = true;
}

void SpriteEditor::LoadZsmFile(const std::string& path) {
  zsprite::ZSprite sprite;
  status_ = sprite.Load(path);
  if (status_.ok()) {
    custom_sprites_.push_back(std::move(sprite));
    current_custom_sprite_index_ = static_cast<int>(custom_sprites_.size()) - 1;
    current_zsm_path_ = path;
    zsm_dirty_ = false;
    preview_needs_update_ = true;
  }
}

void SpriteEditor::SaveZsmFile(const std::string& path) {
  if (current_custom_sprite_index_ >= 0 &&
      current_custom_sprite_index_ < (int)custom_sprites_.size()) {
    status_ = custom_sprites_[current_custom_sprite_index_].Save(path);
    if (status_.ok()) {
      current_zsm_path_ = path;
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
  Text("Sprite Info");
  Separator();

  static char name_buf[256];
  strncpy(name_buf, sprite.sprName.c_str(), sizeof(name_buf) - 1);
  if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
    sprite.sprName = name_buf;
    sprite.property_sprname.Text = name_buf;
    zsm_dirty_ = true;
  }

  static char id_buf[32];
  strncpy(id_buf, sprite.property_sprid.Text.c_str(), sizeof(id_buf) - 1);
  if (ImGui::InputText("Sprite ID", id_buf, sizeof(id_buf))) {
    sprite.property_sprid.Text = id_buf;
    zsm_dirty_ = true;
  }

  Separator();
  DrawStatProperties();

  Separator();
  DrawBooleanProperties();
}

void SpriteEditor::DrawStatProperties() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  Text("Stats");

  // Use InputInt for numeric values
  int prize = sprite.property_prize.Text.empty()
                  ? 0
                  : std::stoi(sprite.property_prize.Text);
  if (ImGui::InputInt("Prize", &prize)) {
    sprite.property_prize.Text = std::to_string(std::clamp(prize, 0, 255));
    zsm_dirty_ = true;
  }

  int palette = sprite.property_palette.Text.empty()
                    ? 0
                    : std::stoi(sprite.property_palette.Text);
  if (ImGui::InputInt("Palette", &palette)) {
    sprite.property_palette.Text = std::to_string(std::clamp(palette, 0, 7));
    zsm_dirty_ = true;
  }

  int oamnbr = sprite.property_oamnbr.Text.empty()
                   ? 0
                   : std::stoi(sprite.property_oamnbr.Text);
  if (ImGui::InputInt("OAM Count", &oamnbr)) {
    sprite.property_oamnbr.Text = std::to_string(std::clamp(oamnbr, 0, 255));
    zsm_dirty_ = true;
  }

  int hitbox = sprite.property_hitbox.Text.empty()
                   ? 0
                   : std::stoi(sprite.property_hitbox.Text);
  if (ImGui::InputInt("Hitbox", &hitbox)) {
    sprite.property_hitbox.Text = std::to_string(std::clamp(hitbox, 0, 255));
    zsm_dirty_ = true;
  }

  int health = sprite.property_health.Text.empty()
                   ? 0
                   : std::stoi(sprite.property_health.Text);
  if (ImGui::InputInt("Health", &health)) {
    sprite.property_health.Text = std::to_string(std::clamp(health, 0, 255));
    zsm_dirty_ = true;
  }

  int damage = sprite.property_damage.Text.empty()
                   ? 0
                   : std::stoi(sprite.property_damage.Text);
  if (ImGui::InputInt("Damage", &damage)) {
    sprite.property_damage.Text = std::to_string(std::clamp(damage, 0, 255));
    zsm_dirty_ = true;
  }
}

void SpriteEditor::DrawBooleanProperties() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  Text("Behavior Flags");

  // Two columns for boolean properties
  if (ImGui::BeginTable("BoolProps", 2, ImGuiTableFlags_None)) {
    // Column 1
    ImGui::TableNextColumn();
    if (ImGui::Checkbox("Blockable", &sprite.property_blockable.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Can Fall", &sprite.property_canfall.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Collision Layer",
                        &sprite.property_collisionlayer.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Custom Death", &sprite.property_customdeath.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Damage Sound", &sprite.property_damagesound.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Deflect Arrows",
                        &sprite.property_deflectarrows.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Deflect Projectiles",
                        &sprite.property_deflectprojectiles.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Fast", &sprite.property_fast.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Harmless", &sprite.property_harmless.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Impervious", &sprite.property_impervious.IsChecked))
      zsm_dirty_ = true;

    // Column 2
    ImGui::TableNextColumn();
    if (ImGui::Checkbox("Impervious Arrow",
                        &sprite.property_imperviousarrow.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Impervious Melee",
                        &sprite.property_imperviousmelee.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Interaction", &sprite.property_interaction.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Is Boss", &sprite.property_isboss.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Persist", &sprite.property_persist.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Shadow", &sprite.property_shadow.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Small Shadow",
                        &sprite.property_smallshadow.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Stasis", &sprite.property_statis.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Statue", &sprite.property_statue.IsChecked))
      zsm_dirty_ = true;
    if (ImGui::Checkbox("Water Sprite",
                        &sprite.property_watersprite.IsChecked))
      zsm_dirty_ = true;

    ImGui::EndTable();
  }
}

// ============================================================
// Animation Panel
// ============================================================

void SpriteEditor::DrawAnimationPanel() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

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
    if (current_frame_ > 0) current_frame_--;
    preview_needs_update_ = true;
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SKIP_NEXT)) {
    if (current_frame_ < (int)sprite.editor.Frames.size() - 1) current_frame_++;
    preview_needs_update_ = true;
  }
  ImGui::SameLine();
  Text("Frame: %d / %d", current_frame_,
       (int)sprite.editor.Frames.size() - 1);

  Separator();

  // Animation list
  Text("Animations");
  if (ImGui::Button(ICON_MD_ADD " Add Animation")) {
    int frame_count = static_cast<int>(sprite.editor.Frames.size());
    sprite.animations.emplace_back(0, frame_count > 0 ? frame_count - 1 : 0, 1,
                                   "New Animation");
    zsm_dirty_ = true;
  }

  if (ImGui::BeginChild("AnimList", ImVec2(0, 120), true)) {
    for (size_t i = 0; i < sprite.animations.size(); i++) {
      auto& anim = sprite.animations[i];
      std::string label =
          anim.frame_name.empty() ? "Unnamed" : anim.frame_name;
      if (Selectable(label.c_str(), current_animation_index_ == (int)i)) {
        current_animation_index_ = static_cast<int>(i);
        current_frame_ = anim.frame_start;
        preview_needs_update_ = true;
      }
    }
  }
  ImGui::EndChild();

  // Edit selected animation
  if (current_animation_index_ >= 0 &&
      current_animation_index_ < (int)sprite.animations.size()) {
    auto& anim = sprite.animations[current_animation_index_];

    Separator();
    Text("Animation Properties");

    static char anim_name[128];
    strncpy(anim_name, anim.frame_name.c_str(), sizeof(anim_name) - 1);
    if (ImGui::InputText("Name##Anim", anim_name, sizeof(anim_name))) {
      anim.frame_name = anim_name;
      zsm_dirty_ = true;
    }

    int start = anim.frame_start;
    int end = anim.frame_end;
    int speed = anim.frame_speed;

    if (ImGui::SliderInt("Start Frame", &start, 0,
                         std::max(0, (int)sprite.editor.Frames.size() - 1))) {
      anim.frame_start = static_cast<uint8_t>(start);
      zsm_dirty_ = true;
    }
    if (ImGui::SliderInt("End Frame", &end, 0,
                         std::max(0, (int)sprite.editor.Frames.size() - 1))) {
      anim.frame_end = static_cast<uint8_t>(end);
      zsm_dirty_ = true;
    }
    if (ImGui::SliderInt("Speed", &speed, 1, 16)) {
      anim.frame_speed = static_cast<uint8_t>(speed);
      zsm_dirty_ = true;
    }

    if (ImGui::Button("Delete Animation") && sprite.animations.size() > 1) {
      sprite.animations.erase(sprite.animations.begin() +
                              current_animation_index_);
      current_animation_index_ =
          std::min(current_animation_index_,
                   (int)sprite.animations.size() - 1);
      zsm_dirty_ = true;
    }
  }

  Separator();
  DrawFrameEditor();
}

void SpriteEditor::DrawFrameEditor() {
  auto& sprite = custom_sprites_[current_custom_sprite_index_];

  Text("Frames");
  if (ImGui::Button(ICON_MD_ADD " Add Frame")) {
    sprite.editor.Frames.emplace_back();
    zsm_dirty_ = true;
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DELETE " Delete Frame") &&
      sprite.editor.Frames.size() > 1 && current_frame_ >= 0) {
    sprite.editor.Frames.erase(sprite.editor.Frames.begin() + current_frame_);
    current_frame_ =
        std::min(current_frame_, (int)sprite.editor.Frames.size() - 1);
    zsm_dirty_ = true;
    preview_needs_update_ = true;
  }

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
  if (current_frame_ >= 0 && current_frame_ < (int)sprite.editor.Frames.size()) {
    auto& frame = sprite.editor.Frames[current_frame_];

    Separator();
    Text("Tiles in Frame %d", current_frame_);

    if (ImGui::Button(ICON_MD_ADD " Add Tile")) {
      frame.Tiles.emplace_back();
      zsm_dirty_ = true;
      preview_needs_update_ = true;
    }

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
      if (ImGui::InputInt("Tile ID", &tile_id)) {
        tile.id = static_cast<uint16_t>(std::clamp(tile_id, 0, 511));
        zsm_dirty_ = true;
        preview_needs_update_ = true;
      }

      int x = tile.x, y = tile.y;
      if (ImGui::InputInt("X", &x)) {
        tile.x = static_cast<uint8_t>(std::clamp(x, 0, 251));
        zsm_dirty_ = true;
        preview_needs_update_ = true;
      }
      if (ImGui::InputInt("Y", &y)) {
        tile.y = static_cast<uint8_t>(std::clamp(y, 0, 219));
        zsm_dirty_ = true;
        preview_needs_update_ = true;
      }

      int pal = tile.palette;
      if (ImGui::SliderInt("Palette##Tile", &pal, 0, 7)) {
        tile.palette = static_cast<uint8_t>(pal);
        zsm_dirty_ = true;
        preview_needs_update_ = true;
      }

      if (ImGui::Checkbox("16x16", &tile.size)) {
        zsm_dirty_ = true;
        preview_needs_update_ = true;
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("Flip X", &tile.mirror_x)) {
        zsm_dirty_ = true;
        preview_needs_update_ = true;
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("Flip Y", &tile.mirror_y)) {
        zsm_dirty_ = true;
        preview_needs_update_ = true;
      }

      if (ImGui::Button("Delete Tile")) {
        frame.Tiles.erase(frame.Tiles.begin() + selected_tile_index_);
        selected_tile_index_ = -1;
        zsm_dirty_ = true;
        preview_needs_update_ = true;
      }
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

  frame_timer_ += delta_time;
  float frame_duration = anim.frame_speed / 60.0f;

  if (frame_timer_ >= frame_duration) {
    frame_timer_ = 0;
    current_frame_++;
    if (current_frame_ > anim.frame_end) {
      current_frame_ = anim.frame_start;
    }
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
    zsm_dirty_ = true;
  }

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
    strncpy(routine_name, routine.name.c_str(), sizeof(routine_name) - 1);
    if (ImGui::InputText("Routine Name", routine_name, sizeof(routine_name))) {
      routine.name = routine_name;
      zsm_dirty_ = true;
    }

    Text("ASM Code:");

    // Multiline text input for code
    static char code_buffer[16384];
    strncpy(code_buffer, routine.code.c_str(), sizeof(code_buffer) - 1);
    code_buffer[sizeof(code_buffer) - 1] = '\0';
    if (ImGui::InputTextMultiline("##RoutineCode", code_buffer,
                                  sizeof(code_buffer), ImVec2(-1, 200))) {
      routine.code = code_buffer;
      zsm_dirty_ = true;
    }

    if (ImGui::Button("Delete Routine")) {
      sprite.userRoutines.erase(sprite.userRoutines.begin() +
                                selected_routine_index_);
      selected_routine_index_ = -1;
      zsm_dirty_ = true;
    }
  }
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

  // Draw each tile in the frame
  for (const auto& tile : frame.Tiles) {
    // Calculate source position in sheet
    // sheet_x = (tile.id % 16) * 8
    // sheet_y = (tile.id / 16) * 8
    int tile_size = tile.size ? 16 : 8;

    // Get the graphics sheet
    uint8_t sheet_index = current_sheets_[tile.palette % 8];
    if (sheet_index >= gfx::Arena::Get().gfx_sheets().size()) {
      continue;
    }

    // Draw tile on canvas at tile.x, tile.y with flip settings
    // The actual bitmap drawing would need to extract tile data
    // and apply palette/flip transformations

    // For now, draw a placeholder rectangle
    sprite_canvas_.DrawRect(tile.x, tile.y, tile_size, tile_size,
                            ImVec4(1.0f, 1.0f, 0.0f, 0.5f));
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
      Text("Frame: %d | Tiles: %d", current_frame_,
           current_frame_ < (int)sprite.editor.Frames.size()
               ? (int)sprite.editor.Frames[current_frame_].Tiles.size()
               : 0);
    }
  }
  ImGui::EndChild();
}

}  // namespace editor
}  // namespace yaze
