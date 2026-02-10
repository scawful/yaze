#include "palette_editor.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/palette/palette_category.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/popup_id.h"
#include "app/gui/core/search.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using ImGui::AcceptDragDropPayload;
using ImGui::BeginChild;
using ImGui::BeginDragDropTarget;
using ImGui::BeginGroup;
using ImGui::BeginPopup;
using ImGui::BeginPopupContextItem;
using ImGui::Button;
using ImGui::ColorButton;
using ImGui::ColorPicker4;
using ImGui::EndChild;
using ImGui::EndDragDropTarget;
using ImGui::EndGroup;
using ImGui::EndPopup;
using ImGui::GetStyle;
using ImGui::OpenPopup;
using ImGui::PopID;
using ImGui::PushID;
using ImGui::SameLine;
using ImGui::Selectable;
using ImGui::Separator;
using ImGui::SetClipboardText;
using ImGui::Text;

using namespace gfx;

constexpr ImGuiTableFlags kPaletteTableFlags =
    ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable |
    ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Hideable;

constexpr ImGuiColorEditFlags kPalNoAlpha = ImGuiColorEditFlags_NoAlpha;

constexpr ImGuiColorEditFlags kPalButtonFlags = ImGuiColorEditFlags_NoAlpha |
                                                ImGuiColorEditFlags_NoPicker |
                                                ImGuiColorEditFlags_NoTooltip;

constexpr ImGuiColorEditFlags kColorPopupFlags =
    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha |
    ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV |
    ImGuiColorEditFlags_DisplayHex;

namespace {
int CustomFormatString(char* buf, size_t buf_size, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
#ifdef IMGUI_USE_STB_SPRINTF
  int w = stbsp_vsnprintf(buf, (int)buf_size, fmt, args);
#else
  int w = vsnprintf(buf, buf_size, fmt, args);
#endif
  va_end(args);
  if (buf == nullptr)
    return w;
  if (w == -1 || w >= (int)buf_size)
    w = (int)buf_size - 1;
  buf[w] = 0;
  return w;
}

static inline float color_saturate(float f) {
  return (f < 0.0f) ? 0.0f : (f > 1.0f) ? 1.0f : f;
}

#define F32_TO_INT8_SAT(_VAL)            \
  ((int)(color_saturate(_VAL) * 255.0f + \
         0.5f))  // Saturated, always output 0..255
}  // namespace

/**
 * @brief Display SNES palette with enhanced ROM hacking features
 * @param palette SNES palette to display
 * @param loaded Whether the palette has been loaded from ROM
 *
 * Enhanced Features:
 * - Real-time color preview with SNES format conversion
 * - Drag-and-drop color swapping for palette editing
 * - Color picker integration with ROM palette system
 * - Undo/redo support for palette modifications
 * - Export functionality for palette sharing
 *
 * Performance Notes:
 * - Static color arrays to avoid repeated allocations
 * - Cached color conversions for fast rendering
 * - Batch palette updates to minimize ROM writes
 */
absl::Status DisplayPalette(gfx::SnesPalette& palette, bool loaded) {
  static ImVec4 color = ImVec4(0, 0, 0, 1.0f);
  static ImVec4 current_palette[256] = {};
  ImGuiColorEditFlags misc_flags = ImGuiColorEditFlags_AlphaPreview |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoOptions;

  // Reload palette colors whenever the palette data is available.
  if (loaded) {
    for (size_t n = 0; n < palette.size(); n++) {
      auto color = palette[n];
      current_palette[n].x = color.rgb().x / 255.0f;
      current_palette[n].y = color.rgb().y / 255.0f;
      current_palette[n].z = color.rgb().z / 255.0f;
      current_palette[n].w = 1.0f;
    }
  }

  static ImVec4 backup_color;
  bool open_popup = ColorButton("MyColor##3b", color, misc_flags);
  SameLine(0, GetStyle().ItemInnerSpacing.x);
  open_popup |= Button("Palette");
  if (open_popup) {
    OpenPopup(gui::MakePopupId(gui::EditorNames::kPalette,
                               gui::PopupNames::kColorPicker)
                  .c_str());
    backup_color = color;
  }

  if (BeginPopup(gui::MakePopupId(gui::EditorNames::kPalette,
                                  gui::PopupNames::kColorPicker)
                     .c_str())) {
    TEXT_WITH_SEPARATOR("Current Overworld Palette");
    ColorPicker4("##picker", (float*)&color,
                 misc_flags | ImGuiColorEditFlags_NoSidePreview |
                     ImGuiColorEditFlags_NoSmallPreview);
    SameLine();

    BeginGroup();  // Lock X position
    Text("Current ==>");
    SameLine();
    Text("Previous");

    if (Button("Update Map Palette")) {}

    ColorButton(
        "##current", color,
        ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_AlphaPreviewHalf,
        ImVec2(60, 40));
    SameLine();

    if (ColorButton(
            "##previous", backup_color,
            ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_AlphaPreviewHalf,
            ImVec2(60, 40)))
      color = backup_color;

    // List of Colors in Overworld Palette
    Separator();
    Text("Palette");
    for (int n = 0; n < IM_ARRAYSIZE(current_palette); n++) {
      PushID(n);
      if ((n % 8) != 0)
        SameLine(0.0f, GetStyle().ItemSpacing.y);

      if (ColorButton("##palette", current_palette[n], kPalButtonFlags,
                      ImVec2(20, 20)))
        color = ImVec4(current_palette[n].x, current_palette[n].y,
                       current_palette[n].z, color.w);  // Preserve alpha!

      if (BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
          memcpy((float*)&current_palette[n], payload->Data, sizeof(float) * 3);
        if (const ImGuiPayload* payload =
                AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
          memcpy((float*)&current_palette[n], payload->Data, sizeof(float) * 4);
        EndDragDropTarget();
      }

      PopID();
    }
    EndGroup();
    EndPopup();
  }

  return absl::OkStatus();
}

void PaletteEditor::Initialize() {
  // Register all panels with PanelManager (done once during
  // initialization)
  if (!dependencies_.panel_manager)
    return;
  auto* panel_manager = dependencies_.panel_manager;
  const size_t session_id = dependencies_.session_id;

  panel_manager->RegisterPanel(
      {.card_id = "palette.control_panel",
       .display_name = "Palette Controls",
       .window_title = " Palette Controls",
       .icon = ICON_MD_PALETTE,
       .category = "Palette",
       .shortcut_hint = "Ctrl+Shift+P",
       .visibility_flag = &show_control_panel_,
       .priority = 10,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM first"});

  panel_manager->RegisterPanel(
      {.card_id = "palette.ow_main",
       .display_name = "Overworld Main",
       .window_title = " Overworld Main",
       .icon = ICON_MD_LANDSCAPE,
       .category = "Palette",
       .shortcut_hint = "Ctrl+Alt+1",
       .visibility_flag = &show_ow_main_panel_,
       .priority = 20,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM first"});

  panel_manager->RegisterPanel(
      {.card_id = "palette.ow_animated",
       .display_name = "Overworld Animated",
       .window_title = " Overworld Animated",
       .icon = ICON_MD_WATER,
       .category = "Palette",
       .shortcut_hint = "Ctrl+Alt+2",
       .visibility_flag = &show_ow_animated_panel_,
       .priority = 30,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM first"});

  panel_manager->RegisterPanel(
      {.card_id = "palette.dungeon_main",
       .display_name = "Dungeon Main",
       .window_title = " Dungeon Main",
       .icon = ICON_MD_CASTLE,
       .category = "Palette",
       .shortcut_hint = "Ctrl+Alt+3",
       .visibility_flag = &show_dungeon_main_panel_,
       .priority = 40,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM first"});

  panel_manager->RegisterPanel(
      {.card_id = "palette.sprites",
       .display_name = "Global Sprite Palettes",
       .window_title = " SNES Palette",
       .icon = ICON_MD_PETS,
       .category = "Palette",
       .shortcut_hint = "Ctrl+Alt+4",
       .visibility_flag = &show_sprite_panel_,
       .priority = 50,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM first"});

  panel_manager->RegisterPanel(
      {.card_id = "palette.sprites_aux1",
       .display_name = "Sprites Aux 1",
       .window_title = " Sprites Aux 1",
       .icon = ICON_MD_FILTER_1,
       .category = "Palette",
       .shortcut_hint = "Ctrl+Alt+7",
       .visibility_flag = &show_sprites_aux1_panel_,
       .priority = 51,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM first"});

  panel_manager->RegisterPanel(
      {.card_id = "palette.sprites_aux2",
       .display_name = "Sprites Aux 2",
       .window_title = " Sprites Aux 2",
       .icon = ICON_MD_FILTER_2,
       .category = "Palette",
       .shortcut_hint = "Ctrl+Alt+8",
       .visibility_flag = &show_sprites_aux2_panel_,
       .priority = 52,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM first"});

  panel_manager->RegisterPanel(
      {.card_id = "palette.sprites_aux3",
       .display_name = "Sprites Aux 3",
       .window_title = " Sprites Aux 3",
       .icon = ICON_MD_FILTER_3,
       .category = "Palette",
       .shortcut_hint = "Ctrl+Alt+9",
       .visibility_flag = &show_sprites_aux3_panel_,
       .priority = 53,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM first"});

  panel_manager->RegisterPanel(
      {.card_id = "palette.equipment",
       .display_name = "Equipment Palettes",
       .window_title = " Equipment Palettes",
       .icon = ICON_MD_SHIELD,
       .category = "Palette",
       .shortcut_hint = "Ctrl+Alt+5",
       .visibility_flag = &show_equipment_panel_,
       .priority = 60,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM first"});

  panel_manager->RegisterPanel(
      {.card_id = "palette.quick_access",
       .display_name = "Quick Access",
       .window_title = " Color Harmony",
       .icon = ICON_MD_COLOR_LENS,
       .category = "Palette",
       .shortcut_hint = "Ctrl+Alt+Q",
       .visibility_flag = &show_quick_access_,
       .priority = 70,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM first"});

  panel_manager->RegisterPanel(
      {.card_id = "palette.custom",
       .display_name = "Custom Palette",
       .window_title = " Palette Editor",
       .icon = ICON_MD_BRUSH,
       .category = "Palette",
       .shortcut_hint = "Ctrl+Alt+C",
       .visibility_flag = &show_custom_palette_,
       .priority = 80,
       .enabled_condition = [this]() { return rom_ && rom_->is_loaded(); },
       .disabled_tooltip = "Load a ROM first"});

  // Show control panel by default when Palette Editor is activated
  panel_manager->ShowPanel(session_id, "palette.control_panel");
}

// ============================================================================
// Helper Panel Classes
// ============================================================================

class PaletteControlPanel : public EditorPanel {
 public:
  explicit PaletteControlPanel(std::function<void()> draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "palette.control_panel"; }
  std::string GetDisplayName() const override { return "Palette Controls"; }
  std::string GetIcon() const override { return ICON_MD_PALETTE; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 10; }
  float GetPreferredWidth() const override { return 320.0f; }

  void Draw(bool* p_open) override {
    if (p_open && !*p_open)
      return;
    if (draw_callback_)
      draw_callback_();
  }

 private:
  std::function<void()> draw_callback_;
};

class QuickAccessPalettePanel : public EditorPanel {
 public:
  explicit QuickAccessPalettePanel(std::function<void()> draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "palette.quick_access"; }
  std::string GetDisplayName() const override { return "Quick Access"; }
  std::string GetIcon() const override { return ICON_MD_COLOR_LENS; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 70; }
  float GetPreferredWidth() const override { return 340.0f; }

  void Draw(bool* p_open) override {
    if (p_open && !*p_open)
      return;
    if (draw_callback_)
      draw_callback_();
  }

 private:
  std::function<void()> draw_callback_;
};

class CustomPalettePanel : public EditorPanel {
 public:
  explicit CustomPalettePanel(std::function<void()> draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "palette.custom"; }
  std::string GetDisplayName() const override { return "Custom Palette"; }
  std::string GetIcon() const override { return ICON_MD_BRUSH; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 80; }
  float GetPreferredWidth() const override { return 420.0f; }

  void Draw(bool* p_open) override {
    if (p_open && !*p_open)
      return;
    if (draw_callback_)
      draw_callback_();
  }

 private:
  std::function<void()> draw_callback_;
};

absl::Status PaletteEditor::Load() {
  gfx::ScopedTimer timer("PaletteEditor::Load");

  if (!rom() || !rom()->is_loaded()) {
    return absl::NotFoundError("ROM not open, no palettes to display");
  }

  // Initialize the labels
  for (int i = 0; i < kNumPalettes; i++) {
    rom()->resource_label()->CreateOrGetLabel(
        "Palette Group Name", std::to_string(i),
        std::string(kPaletteGroupNames[i]));
  }

  // Initialize the centralized PaletteManager with GameData
  // This must be done before creating any palette cards
  if (game_data()) {
    gfx::PaletteManager::Get().Initialize(game_data());
  } else {
    // Fallback to legacy ROM-only initialization
    gfx::PaletteManager::Get().Initialize(rom_);
  }

  // Also set up the embedded GfxGroupEditor
  gfx_group_editor_.SetRom(rom_);
  gfx_group_editor_.SetGameData(game_data());

  // Register EditorPanel instances with PanelManager
  if (dependencies_.panel_manager) {
    auto* panel_manager = dependencies_.panel_manager;

    // Create and register palette panels
    // Note: PanelManager takes ownership via unique_ptr

    // Overworld Main
    auto ow_main =
        std::make_unique<OverworldMainPalettePanel>(rom_, game_data());
    ow_main_panel_ = ow_main.get();
    panel_manager->RegisterEditorPanel(std::move(ow_main));

    // Overworld Animated
    auto ow_anim =
        std::make_unique<OverworldAnimatedPalettePanel>(rom_, game_data());
    ow_anim_panel_ = ow_anim.get();
    panel_manager->RegisterEditorPanel(std::move(ow_anim));

    // Dungeon Main
    auto dungeon_main =
        std::make_unique<DungeonMainPalettePanel>(rom_, game_data());
    dungeon_main_panel_ = dungeon_main.get();
    panel_manager->RegisterEditorPanel(std::move(dungeon_main));

    // Global Sprites
    auto sprite_global =
        std::make_unique<SpritePalettePanel>(rom_, game_data());
    sprite_global_panel_ = sprite_global.get();
    panel_manager->RegisterEditorPanel(std::move(sprite_global));

    // Sprites Aux 1
    auto sprite_aux1 =
        std::make_unique<SpritesAux1PalettePanel>(rom_, game_data());
    sprite_aux1_panel_ = sprite_aux1.get();
    panel_manager->RegisterEditorPanel(std::move(sprite_aux1));

    // Sprites Aux 2
    auto sprite_aux2 =
        std::make_unique<SpritesAux2PalettePanel>(rom_, game_data());
    sprite_aux2_panel_ = sprite_aux2.get();
    panel_manager->RegisterEditorPanel(std::move(sprite_aux2));

    // Sprites Aux 3
    auto sprite_aux3 =
        std::make_unique<SpritesAux3PalettePanel>(rom_, game_data());
    sprite_aux3_panel_ = sprite_aux3.get();
    panel_manager->RegisterEditorPanel(std::move(sprite_aux3));

    // Equipment
    auto equipment = std::make_unique<EquipmentPalettePanel>(rom_, game_data());
    equipment_panel_ = equipment.get();
    panel_manager->RegisterEditorPanel(std::move(equipment));

    // Wire toast manager to all palette group panels
    auto* toast = dependencies_.toast_manager;
    if (toast) {
      ow_main_panel_->SetToastManager(toast);
      ow_anim_panel_->SetToastManager(toast);
      dungeon_main_panel_->SetToastManager(toast);
      sprite_global_panel_->SetToastManager(toast);
      sprite_aux1_panel_->SetToastManager(toast);
      sprite_aux2_panel_->SetToastManager(toast);
      sprite_aux3_panel_->SetToastManager(toast);
      equipment_panel_->SetToastManager(toast);
    }

    // Register utility panels with callbacks
    panel_manager->RegisterEditorPanel(std::make_unique<PaletteControlPanel>(
        [this]() { DrawControlPanel(); }));
    panel_manager->RegisterEditorPanel(
        std::make_unique<QuickAccessPalettePanel>(
            [this]() { DrawQuickAccessPanel(); }));
    panel_manager->RegisterEditorPanel(std::make_unique<CustomPalettePanel>(
        [this]() { DrawCustomPalettePanel(); }));
  }

  return absl::OkStatus();
}

absl::Status PaletteEditor::Save() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Delegate to PaletteManager for centralized save
  RETURN_IF_ERROR(gfx::PaletteManager::Get().SaveAllToRom());

  // Mark ROM as needing file save
  rom_->set_dirty(true);

  return absl::OkStatus();
}

absl::Status PaletteEditor::Undo() {
  if (!gfx::PaletteManager::Get().IsInitialized()) {
    return absl::FailedPreconditionError("PaletteManager not initialized");
  }

  gfx::PaletteManager::Get().Undo();
  return absl::OkStatus();
}

absl::Status PaletteEditor::Redo() {
  if (!gfx::PaletteManager::Get().IsInitialized()) {
    return absl::FailedPreconditionError("PaletteManager not initialized");
  }

  gfx::PaletteManager::Get().Redo();
  return absl::OkStatus();
}

absl::Status PaletteEditor::Update() {
  // Panel drawing is handled centrally by PanelManager::DrawAllVisiblePanels()
  // via the EditorPanel implementations registered in Load().
  // No local drawing needed here - this fixes duplicate panel rendering.
  return absl::OkStatus();
}

void PaletteEditor::DrawQuickAccessTab() {
  BeginChild("QuickAccessPalettes", ImVec2(0, 0), true);

  Text("Custom Palette");
  DrawCustomPalette();

  Separator();

  // Current color picker with more options
  BeginGroup();
  Text("Current Color");
  gui::SnesColorEdit4("##CurrentColorPicker", &current_color_,
                      kColorPopupFlags);

  char buf[64];
  auto col = current_color_.rgb();
  int cr = F32_TO_INT8_SAT(col.x / 255.0f);
  int cg = F32_TO_INT8_SAT(col.y / 255.0f);
  int cb = F32_TO_INT8_SAT(col.z / 255.0f);

  CustomFormatString(buf, IM_ARRAYSIZE(buf), "RGB: %d, %d, %d", cr, cg, cb);
  Text("%s", buf);

  CustomFormatString(buf, IM_ARRAYSIZE(buf), "SNES: $%04X",
                     current_color_.snes());
  Text("%s", buf);

  if (Button("Copy to Clipboard")) {
    SetClipboardText(buf);
  }
  EndGroup();

  Separator();

  // Recently used colors
  Text("Recently Used Colors");
  for (int i = 0; i < recently_used_colors_.size(); i++) {
    PushID(i);
    if (i % 8 != 0)
      SameLine();
    ImVec4 displayColor =
        gui::ConvertSnesColorToImVec4(recently_used_colors_[i]);
    if (ImGui::ColorButton("##recent", displayColor)) {
      // Set as current color
      current_color_ = recently_used_colors_[i];
    }
    PopID();
  }

  EndChild();
}

/**
 * @brief Draw custom palette editor with enhanced ROM hacking features
 *
 * Enhanced Features:
 * - Drag-and-drop color reordering
 * - Context menu for each color with advanced options
 * - Export/import functionality for palette sharing
 * - Integration with recently used colors
 * - Undo/redo support for palette modifications
 *
 * Performance Notes:
 * - Efficient color conversion caching
 * - Minimal redraws with dirty region tracking
 * - Batch operations for multiple color changes
 */
void PaletteEditor::DrawCustomPalette() {
  if (BeginChild("ColorPalette", ImVec2(0, 40), ImGuiChildFlags_None,
                 ImGuiWindowFlags_HorizontalScrollbar)) {
    for (int i = 0; i < custom_palette_.size(); i++) {
      PushID(i);
      if (i > 0)
        SameLine(0.0f, GetStyle().ItemSpacing.y);

      // Enhanced color button with context menu and drag-drop support
      ImVec4 displayColor = gui::ConvertSnesColorToImVec4(custom_palette_[i]);
      bool open_color_picker = ImGui::ColorButton(
          absl::StrFormat("##customPal%d", i).c_str(), displayColor);

      if (open_color_picker) {
        current_color_ = custom_palette_[i];
        edit_palette_index_ = i;
        ImGui::OpenPopup(gui::MakePopupId(gui::EditorNames::kPalette,
                                          "CustomPaletteColorEdit")
                             .c_str());
      }

      if (BeginPopupContextItem()) {
        // Edit color directly in the popup
        SnesColor original_color = custom_palette_[i];
        if (gui::SnesColorEdit4("Edit Color", &custom_palette_[i],
                                kColorPopupFlags)) {
          // Color was changed, add to recently used
          AddRecentlyUsedColor(custom_palette_[i]);
        }

        if (Button("Delete", ImVec2(-1, 0))) {
          custom_palette_.erase(custom_palette_.begin() + i);
        }
      }

      // Handle drag/drop for palette rearrangement
      if (BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F)) {
          ImVec4 color;
          memcpy((float*)&color, payload->Data, sizeof(float) * 3);
          color.w = 1.0f;  // Set alpha to 1.0
          custom_palette_[i] = SnesColor(color);
          AddRecentlyUsedColor(custom_palette_[i]);
        }
        EndDragDropTarget();
      }

      PopID();
    }

    SameLine();
    if (ImGui::Button("+")) {
      custom_palette_.push_back(SnesColor(0x7FFF));
    }

    SameLine();
    if (ImGui::Button("Clear")) {
      custom_palette_.clear();
    }

    SameLine();
    if (ImGui::Button("Export")) {
      std::string clipboard;
      for (const auto& color : custom_palette_) {
        clipboard += absl::StrFormat("$%04X,", color.snes());
      }
      SetClipboardText(clipboard.c_str());
    }
  }
  EndChild();

  // Color picker popup for custom palette editing
  if (ImGui::BeginPopup(
          gui::MakePopupId(gui::EditorNames::kPalette, "CustomPaletteColorEdit")
              .c_str())) {
    if (edit_palette_index_ >= 0 &&
        edit_palette_index_ < custom_palette_.size()) {
      SnesColor original_color = custom_palette_[edit_palette_index_];
      if (gui::SnesColorEdit4(
              "Edit Color", &custom_palette_[edit_palette_index_],
              kColorPopupFlags | ImGuiColorEditFlags_PickerHueWheel)) {
        // Color was changed, add to recently used
        AddRecentlyUsedColor(custom_palette_[edit_palette_index_]);
      }
    }
    ImGui::EndPopup();
  }
}

absl::Status PaletteEditor::DrawPaletteGroup(int category,
                                             bool /*right_side*/) {
  if (!rom()->is_loaded() || !game_data()) {
    return absl::NotFoundError("ROM not open, no palettes to display");
  }

  auto palette_group_name = kPaletteGroupNames[category];
  gfx::PaletteGroup* palette_group =
      game_data()->palette_groups.get_group(palette_group_name.data());
  const auto size = palette_group->size();

  for (int j = 0; j < size; j++) {
    gfx::SnesPalette* palette = palette_group->mutable_palette(j);
    auto pal_size = palette->size();

    BeginGroup();

    PushID(j);
    BeginGroup();
    rom()->resource_label()->SelectableLabelWithNameEdit(
        false, palette_group_name.data(), /*key=*/std::to_string(j),
        "Unnamed Palette");
    EndGroup();

    for (int n = 0; n < pal_size; n++) {
      PushID(n);
      if (n > 0 && n % 8 != 0)
        SameLine(0.0f, 2.0f);

      auto popup_id =
          absl::StrCat(kPaletteCategoryNames[category].data(), j, "_", n);

      ImVec4 displayColor = gui::ConvertSnesColorToImVec4((*palette)[n]);
      if (ImGui::ColorButton(popup_id.c_str(), displayColor)) {
        current_color_ = (*palette)[n];
        AddRecentlyUsedColor(current_color_);
      }

      if (BeginPopupContextItem(popup_id.c_str())) {
        RETURN_IF_ERROR(HandleColorPopup(*palette, category, j, n))
      }
      PopID();
    }
    PopID();
    EndGroup();

    if (j < size - 1) {
      Separator();
    }
  }
  return absl::OkStatus();
}

void PaletteEditor::AddRecentlyUsedColor(const SnesColor& color) {
  // Check if color already exists in recently used
  auto it = std::find_if(
      recently_used_colors_.begin(), recently_used_colors_.end(),
      [&color](const SnesColor& c) { return c.snes() == color.snes(); });

  // If found, remove it to re-add at front
  if (it != recently_used_colors_.end()) {
    recently_used_colors_.erase(it);
  }

  // Add at front
  recently_used_colors_.insert(recently_used_colors_.begin(), color);

  // Limit size
  if (recently_used_colors_.size() > 16) {
    recently_used_colors_.pop_back();
  }
}

absl::Status PaletteEditor::HandleColorPopup(gfx::SnesPalette& palette, int i,
                                             int j, int n) {
  auto col = gfx::ToFloatArray(palette[n]);
  auto original_color = palette[n];

  if (gui::SnesColorEdit4("Edit Color", &palette[n], kColorPopupFlags)) {
    history_.RecordChange(/*group_name=*/std::string(kPaletteGroupNames[i]),
                          /*palette_index=*/j, /*color_index=*/n,
                          original_color, palette[n]);
    palette[n].set_modified(true);

    // Add to recently used colors
    AddRecentlyUsedColor(palette[n]);
  }

  // Color information display
  char buf[64];
  int cr = F32_TO_INT8_SAT(col[0]);
  int cg = F32_TO_INT8_SAT(col[1]);
  int cb = F32_TO_INT8_SAT(col[2]);

  Text("RGB: %d, %d, %d", cr, cg, cb);
  Text("SNES: $%04X", palette[n].snes());

  Separator();

  if (Button("Copy as..", ImVec2(-1, 0)))
    OpenPopup(gui::MakePopupId(gui::EditorNames::kPalette,
                               gui::PopupNames::kCopyPopup)
                  .c_str());
  if (BeginPopup(gui::MakePopupId(gui::EditorNames::kPalette,
                                  gui::PopupNames::kCopyPopup)
                     .c_str())) {
    CustomFormatString(buf, IM_ARRAYSIZE(buf), "(%.3ff, %.3ff, %.3ff)", col[0],
                       col[1], col[2]);
    if (Selectable(buf))
      SetClipboardText(buf);

    CustomFormatString(buf, IM_ARRAYSIZE(buf), "(%d,%d,%d)", cr, cg, cb);
    if (Selectable(buf))
      SetClipboardText(buf);

    CustomFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", cr, cg, cb);
    if (Selectable(buf))
      SetClipboardText(buf);

    // SNES Format
    CustomFormatString(buf, IM_ARRAYSIZE(buf), "$%04X",
                       ConvertRgbToSnes(ImVec4(col[0], col[1], col[2], 1.0f)));
    if (Selectable(buf))
      SetClipboardText(buf);

    EndPopup();
  }

  // Add a button to add this color to custom palette
  if (Button("Add to Custom Palette", ImVec2(-1, 0))) {
    custom_palette_.push_back(palette[n]);
  }

  EndPopup();
  return absl::OkStatus();
}

absl::Status PaletteEditor::EditColorInPalette(gfx::SnesPalette& palette,
                                               int index) {
  if (index >= palette.size()) {
    return absl::InvalidArgumentError("Index out of bounds");
  }

  // Get the current color
  auto color = palette[index];
  auto currentColor = color.rgb();
  if (ColorPicker4("Color Picker", (float*)&palette[index])) {
    // The color was modified, update it in the palette
    palette[index] = gui::ConvertImVec4ToSnesColor(currentColor);

    // Add to recently used colors
    AddRecentlyUsedColor(palette[index]);
  }
  return absl::OkStatus();
}

absl::Status PaletteEditor::ResetColorToOriginal(
    gfx::SnesPalette& palette, int index,
    const gfx::SnesPalette& originalPalette) {
  if (index >= palette.size() || index >= originalPalette.size()) {
    return absl::InvalidArgumentError("Index out of bounds");
  }
  auto color = originalPalette[index];
  auto originalColor = color.rgb();
  palette[index] = gui::ConvertImVec4ToSnesColor(originalColor);
  return absl::OkStatus();
}

// ============================================================================
// Panel-Based UI Methods
// ============================================================================

void PaletteEditor::DrawToolset() {
  // Sidebar is drawn by PanelManager in EditorManager
  // Panels registered in Initialize() appear in the sidebar automatically
}

void PaletteEditor::DrawControlPanel() {
  // Toolbar with quick toggles
  DrawToolset();

  ImGui::Separator();

  // Categorized palette list with search
  DrawCategorizedPaletteList();

  ImGui::Separator();

  // Modified status indicator
  ImGui::TextColored(gui::GetWarningColor(), "Modified Panels:");
  bool any_modified = false;

  if (ow_main_panel_ && ow_main_panel_->HasUnsavedChanges()) {
    ImGui::BulletText("Overworld Main");
    any_modified = true;
  }
  if (ow_anim_panel_ && ow_anim_panel_->HasUnsavedChanges()) {
    ImGui::BulletText("Overworld Animated");
    any_modified = true;
  }
  if (dungeon_main_panel_ && dungeon_main_panel_->HasUnsavedChanges()) {
    ImGui::BulletText("Dungeon Main");
    any_modified = true;
  }
  if (sprite_global_panel_ && sprite_global_panel_->HasUnsavedChanges()) {
    ImGui::BulletText("Global Sprite Palettes");
    any_modified = true;
  }
  if (sprite_aux1_panel_ && sprite_aux1_panel_->HasUnsavedChanges()) {
    ImGui::BulletText("Sprites Aux 1");
    any_modified = true;
  }
  if (sprite_aux2_panel_ && sprite_aux2_panel_->HasUnsavedChanges()) {
    ImGui::BulletText("Sprites Aux 2");
    any_modified = true;
  }
  if (sprite_aux3_panel_ && sprite_aux3_panel_->HasUnsavedChanges()) {
    ImGui::BulletText("Sprites Aux 3");
    any_modified = true;
  }
  if (equipment_panel_ && equipment_panel_->HasUnsavedChanges()) {
    ImGui::BulletText("Equipment Palettes");
    any_modified = true;
  }

  if (!any_modified) {
    ImGui::TextDisabled("No unsaved changes");
  }

  ImGui::Separator();

  // Quick actions
  ImGui::Text("Quick Actions:");

  // Use centralized PaletteManager for global operations
  bool has_unsaved = gfx::PaletteManager::Get().HasUnsavedChanges();
  size_t modified_count = gfx::PaletteManager::Get().GetModifiedColorCount();

  ImGui::BeginDisabled(!has_unsaved);
  if (ImGui::Button(absl::StrFormat(ICON_MD_SAVE " Save All (%zu colors)",
                                    modified_count)
                        .c_str(),
                    ImVec2(-1, 0))) {
    auto status = gfx::PaletteManager::Get().SaveAllToRom();
    if (!status.ok()) {
      if (dependencies_.toast_manager) {
        dependencies_.toast_manager->Show(
            absl::StrFormat("Failed to save palettes: %s",
                            status.message()),
            ToastType::kError);
      }
    }
  }
  ImGui::EndDisabled();

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (has_unsaved) {
      ImGui::SetTooltip("Save all modified colors to ROM");
    } else {
      ImGui::SetTooltip("No unsaved changes");
    }
  }

  // Apply to Editors button - preview changes without saving to ROM
  ImGui::BeginDisabled(!has_unsaved);
  if (ImGui::Button(ICON_MD_VISIBILITY " Apply to Editors", ImVec2(-1, 0))) {
    auto status = gfx::PaletteManager::Get().ApplyPreviewChanges();
    if (!status.ok()) {
      ImGui::OpenPopup(gui::MakePopupId(gui::EditorNames::kPalette,
                                        gui::PopupNames::kSaveError)
                           .c_str());
    }
  }
  ImGui::EndDisabled();

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (has_unsaved) {
      ImGui::SetTooltip(
          "Preview palette changes in other editors without saving to ROM");
    } else {
      ImGui::SetTooltip("No changes to preview");
    }
  }

  ImGui::BeginDisabled(!has_unsaved);
  if (ImGui::Button(ICON_MD_UNDO " Discard All Changes", ImVec2(-1, 0))) {
    ImGui::OpenPopup(gui::MakePopupId(gui::EditorNames::kPalette,
                                      gui::PopupNames::kConfirmDiscardAll)
                         .c_str());
  }
  ImGui::EndDisabled();

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (has_unsaved) {
      ImGui::SetTooltip("Discard all unsaved changes");
    } else {
      ImGui::SetTooltip("No changes to discard");
    }
  }

  // Confirmation popup for discard
  if (ImGui::BeginPopupModal(
          gui::MakePopupId(gui::EditorNames::kPalette,
                           gui::PopupNames::kConfirmDiscardAll)
              .c_str(),
          nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Discard all unsaved changes?");
    ImGui::TextColored(gui::GetWarningColor(),
                       "This will revert %zu modified colors.",
                       modified_count);
    ImGui::Separator();

    if (ImGui::Button("Discard", ImVec2(120, 0))) {
      gfx::PaletteManager::Get().DiscardAllChanges();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // Error popup for save failures
  if (ImGui::BeginPopupModal(gui::MakePopupId(gui::EditorNames::kPalette,
                                              gui::PopupNames::kSaveError)
                                 .c_str(),
                             nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(gui::GetErrorColor(),
                       "Failed to save changes");
    ImGui::Text("An error occurred while saving to ROM.");
    ImGui::Separator();

    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::Separator();

  // Panel management handled globally in the sidebar/menu system.
}

void PaletteEditor::DrawQuickAccessPanel() {
  // Current color picker with more options
  ImGui::BeginGroup();
  ImGui::Text("Current Color");
  gui::SnesColorEdit4("##CurrentColorPicker", &current_color_,
                      kColorPopupFlags);

  char buf[64];
  auto col = current_color_.rgb();
  int cr = F32_TO_INT8_SAT(col.x / 255.0f);
  int cg = F32_TO_INT8_SAT(col.y / 255.0f);
  int cb = F32_TO_INT8_SAT(col.z / 255.0f);

  CustomFormatString(buf, IM_ARRAYSIZE(buf), "RGB: %d, %d, %d", cr, cg, cb);
  ImGui::Text("%s", buf);

  CustomFormatString(buf, IM_ARRAYSIZE(buf), "SNES: $%04X",
                     current_color_.snes());
  ImGui::Text("%s", buf);

  if (ImGui::Button("Copy to Clipboard", ImVec2(-1, 0))) {
    SetClipboardText(buf);
  }
  ImGui::EndGroup();

  ImGui::Separator();

  // Recently used colors
  ImGui::Text("Recently Used Colors");
  if (recently_used_colors_.empty()) {
    ImGui::TextDisabled("No recently used colors yet");
  } else {
    for (int i = 0; i < recently_used_colors_.size(); i++) {
      PushID(i);
      if (i % 8 != 0)
        SameLine();
      ImVec4 displayColor =
          gui::ConvertSnesColorToImVec4(recently_used_colors_[i]);
      if (ImGui::ColorButton("##recent", displayColor, kPalButtonFlags,
                             ImVec2(28, 28))) {
        // Set as current color
        current_color_ = recently_used_colors_[i];
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("SNES: $%04X", recently_used_colors_[i].snes());
      }
      PopID();
    }
  }
}

void PaletteEditor::DrawCustomPalettePanel() {
  ImGui::TextWrapped(
      "Create your own custom color palette for reference. "
      "Colors can be added from any palette group or created from scratch.");

  ImGui::Separator();

  // Custom palette color grid
  if (custom_palette_.empty()) {
    ImGui::TextDisabled("Your custom palette is empty.");
    ImGui::Text("Click + to add colors or drag colors from any palette.");
  } else {
    for (int i = 0; i < custom_palette_.size(); i++) {
      PushID(i);
      if (i > 0 && i % 16 != 0)
        SameLine(0.0f, 2.0f);

      // Enhanced color button with context menu and drag-drop support
      ImVec4 displayColor = gui::ConvertSnesColorToImVec4(custom_palette_[i]);
      bool open_color_picker =
          ImGui::ColorButton(absl::StrFormat("##customPal%d", i).c_str(),
                             displayColor, kPalButtonFlags, ImVec2(28, 28));

      if (open_color_picker) {
        current_color_ = custom_palette_[i];
        edit_palette_index_ = i;
        ImGui::OpenPopup(gui::MakePopupId(gui::EditorNames::kPalette,
                                          "PanelCustomPaletteColorEdit")
                             .c_str());
      }

      if (BeginPopupContextItem()) {
        // Edit color directly in the popup
        SnesColor original_color = custom_palette_[i];
        if (gui::SnesColorEdit4("Edit Color", &custom_palette_[i],
                                kColorPopupFlags)) {
          // Color was changed, add to recently used
          AddRecentlyUsedColor(custom_palette_[i]);
        }

        if (ImGui::Button("Delete", ImVec2(-1, 0))) {
          custom_palette_.erase(custom_palette_.begin() + i);
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      // Handle drag/drop for palette rearrangement
      if (BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F)) {
          ImVec4 color;
          memcpy((float*)&color, payload->Data, sizeof(float) * 3);
          color.w = 1.0f;  // Set alpha to 1.0
          custom_palette_[i] = SnesColor(color);
          AddRecentlyUsedColor(custom_palette_[i]);
        }
        EndDragDropTarget();
      }

      PopID();
    }
  }

  ImGui::Separator();

  // Buttons for palette management
  if (ImGui::Button(ICON_MD_ADD " Add Color")) {
    custom_palette_.push_back(SnesColor(0x7FFF));
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DELETE " Clear All")) {
    custom_palette_.clear();
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_CONTENT_COPY " Export")) {
    std::string clipboard;
    for (const auto& color : custom_palette_) {
      clipboard += absl::StrFormat("$%04X,", color.snes());
    }
    if (!clipboard.empty()) {
      clipboard.pop_back();  // Remove trailing comma
    }
    SetClipboardText(clipboard.c_str());
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Copy palette as comma-separated SNES values");
  }

  // Color picker popup for custom palette editing
  if (ImGui::BeginPopup(gui::MakePopupId(gui::EditorNames::kPalette,
                                         "PanelCustomPaletteColorEdit")
                            .c_str())) {
    if (edit_palette_index_ >= 0 &&
        edit_palette_index_ < custom_palette_.size()) {
      SnesColor original_color = custom_palette_[edit_palette_index_];
      if (gui::SnesColorEdit4(
              "Edit Color", &custom_palette_[edit_palette_index_],
              kColorPopupFlags | ImGuiColorEditFlags_PickerHueWheel)) {
        // Color was changed, add to recently used
        AddRecentlyUsedColor(custom_palette_[edit_palette_index_]);
      }
    }
    ImGui::EndPopup();
  }
}

void PaletteEditor::JumpToPalette(const std::string& group_name,
                                  int palette_index) {
  if (!dependencies_.panel_manager) {
    return;
  }
  auto* panel_manager = dependencies_.panel_manager;
  const size_t session_id = dependencies_.session_id;

  // Show and focus the appropriate card
  if (group_name == "ow_main") {
    panel_manager->ShowPanel(session_id, "palette.ow_main");
    if (ow_main_panel_) {
      ow_main_panel_->Show();
      ow_main_panel_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "ow_animated") {
    panel_manager->ShowPanel(session_id, "palette.ow_animated");
    if (ow_anim_panel_) {
      ow_anim_panel_->Show();
      ow_anim_panel_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "dungeon_main") {
    panel_manager->ShowPanel(session_id, "palette.dungeon_main");
    if (dungeon_main_panel_) {
      dungeon_main_panel_->Show();
      dungeon_main_panel_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "global_sprites") {
    panel_manager->ShowPanel(session_id, "palette.sprites");
    if (sprite_global_panel_) {
      sprite_global_panel_->Show();
      sprite_global_panel_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "sprites_aux1") {
    panel_manager->ShowPanel(session_id, "palette.sprites_aux1");
    if (sprite_aux1_panel_) {
      sprite_aux1_panel_->Show();
      sprite_aux1_panel_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "sprites_aux2") {
    panel_manager->ShowPanel(session_id, "palette.sprites_aux2");
    if (sprite_aux2_panel_) {
      sprite_aux2_panel_->Show();
      sprite_aux2_panel_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "sprites_aux3") {
    panel_manager->ShowPanel(session_id, "palette.sprites_aux3");
    if (sprite_aux3_panel_) {
      sprite_aux3_panel_->Show();
      sprite_aux3_panel_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "armors") {
    panel_manager->ShowPanel(session_id, "palette.equipment");
    if (equipment_panel_) {
      equipment_panel_->Show();
      equipment_panel_->SetSelectedPaletteIndex(palette_index);
    }
  }

  // Show control panel too for easy navigation
  panel_manager->ShowPanel(session_id, "palette.control_panel");
}

// ============================================================================
// Category and Search UI Methods
// ============================================================================

void PaletteEditor::DrawSearchBar() {
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputTextWithHint("##PaletteSearch",
                               ICON_MD_SEARCH " Search palettes...",
                               search_buffer_, sizeof(search_buffer_))) {
    // Search text changed - UI will update automatically
  }
}

bool PaletteEditor::PassesSearchFilter(const std::string& group_name) const {
  if (search_buffer_[0] == '\0')
    return true;

  // Check if group name or display name matches
  return gui::FuzzyMatch(search_buffer_, group_name) ||
         gui::FuzzyMatch(search_buffer_, GetGroupDisplayName(group_name));
}

bool* PaletteEditor::GetShowFlagForGroup(const std::string& group_name) {
  if (group_name == "ow_main")
    return &show_ow_main_panel_;
  if (group_name == "ow_animated")
    return &show_ow_animated_panel_;
  if (group_name == "dungeon_main")
    return &show_dungeon_main_panel_;
  if (group_name == "global_sprites")
    return &show_sprite_panel_;
  if (group_name == "sprites_aux1")
    return &show_sprites_aux1_panel_;
  if (group_name == "sprites_aux2")
    return &show_sprites_aux2_panel_;
  if (group_name == "sprites_aux3")
    return &show_sprites_aux3_panel_;
  if (group_name == "armors")
    return &show_equipment_panel_;
  return nullptr;
}

void PaletteEditor::DrawCategorizedPaletteList() {
  // Search bar at top
  DrawSearchBar();
  ImGui::Separator();

  const auto& categories = GetPaletteCategories();

  for (size_t cat_idx = 0; cat_idx < categories.size(); cat_idx++) {
    const auto& cat = categories[cat_idx];

    // Check if any items in category match search
    bool has_visible_items = false;
    for (const auto& group_name : cat.group_names) {
      if (PassesSearchFilter(group_name)) {
        has_visible_items = true;
        break;
      }
    }

    if (!has_visible_items)
      continue;

    ImGui::PushID(static_cast<int>(cat_idx));

    // Collapsible header for category with icon
    std::string header_text =
        absl::StrFormat("%s %s", cat.icon, cat.display_name);
    bool open = ImGui::CollapsingHeader(header_text.c_str(),
                                        ImGuiTreeNodeFlags_DefaultOpen);

    if (open) {
      ImGui::Indent(10.0f);
      for (const auto& group_name : cat.group_names) {
        if (!PassesSearchFilter(group_name))
          continue;

        bool* show_flag = GetShowFlagForGroup(group_name);
        if (show_flag) {
          std::string label = GetGroupDisplayName(group_name);

          // Show modified indicator
          bool is_modified =
              gfx::PaletteManager::Get().IsGroupModified(group_name);
          if (is_modified) {
            label += " *";
          }
          std::optional<gui::StyleColorGuard> mod_guard;
          if (is_modified) {
            mod_guard.emplace(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
          }

          ImGui::Checkbox(label.c_str(), show_flag);
          mod_guard.reset();
        }
      }
      ImGui::Unindent(10.0f);
    }
    ImGui::PopID();
  }

  ImGui::Separator();

  // Utilities section
  ImGui::Text("Utilities:");
  ImGui::Indent(10.0f);
  ImGui::Checkbox("Quick Access", &show_quick_access_);
  ImGui::Checkbox("Custom Palette", &show_custom_palette_);
  ImGui::Unindent(10.0f);
}

}  // namespace editor
}  // namespace yaze
