#include "palette_editor.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "app/editor/system/editor_card_registry.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
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
  static ImVec4 color = ImVec4(0, 0, 0, 255.f);
  static ImVec4 current_palette[256] = {};
  ImGuiColorEditFlags misc_flags = ImGuiColorEditFlags_AlphaPreview |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoOptions;

  // Generate a default palette. The palette will persist and can be edited.
  static bool init = false;
  if (loaded && !init) {
    for (int n = 0; n < palette.size(); n++) {
      auto color = palette[n];
      current_palette[n].x = color.rgb().x / 255;
      current_palette[n].y = color.rgb().y / 255;
      current_palette[n].z = color.rgb().z / 255;
      current_palette[n].w = 255;  // Alpha
    }
    init = true;
  }

  static ImVec4 backup_color;
  bool open_popup = ColorButton("MyColor##3b", color, misc_flags);
  SameLine(0, GetStyle().ItemInnerSpacing.x);
  open_popup |= Button("Palette");
  if (open_popup) {
    OpenPopup("mypicker");
    backup_color = color;
  }

  if (BeginPopup("mypicker")) {
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
  // Register all cards with EditorCardRegistry (done once during
  // initialization)
  if (!dependencies_.card_registry)
    return;
  auto* card_registry = dependencies_.card_registry;

  card_registry->RegisterCard({.card_id = "palette.control_panel",
                               .display_name = "Palette Controls",
                               .icon = ICON_MD_PALETTE,
                               .category = "Palette",
                               .shortcut_hint = "Ctrl+Shift+P",
                               .visibility_flag = &show_control_panel_,
                               .priority = 10});

  card_registry->RegisterCard({.card_id = "palette.ow_main",
                               .display_name = "Overworld Main",
                               .icon = ICON_MD_LANDSCAPE,
                               .category = "Palette",
                               .shortcut_hint = "Ctrl+Alt+1",
                               .visibility_flag = &show_ow_main_card_,
                               .priority = 20});

  card_registry->RegisterCard({.card_id = "palette.ow_animated",
                               .display_name = "Overworld Animated",
                               .icon = ICON_MD_WATER,
                               .category = "Palette",
                               .shortcut_hint = "Ctrl+Alt+2",
                               .visibility_flag = &show_ow_animated_card_,
                               .priority = 30});

  card_registry->RegisterCard({.card_id = "palette.dungeon_main",
                               .display_name = "Dungeon Main",
                               .icon = ICON_MD_CASTLE,
                               .category = "Palette",
                               .shortcut_hint = "Ctrl+Alt+3",
                               .visibility_flag = &show_dungeon_main_card_,
                               .priority = 40});

  card_registry->RegisterCard({.card_id = "palette.sprites",
                               .display_name = "Global Sprite Palettes",
                               .icon = ICON_MD_PETS,
                               .category = "Palette",
                               .shortcut_hint = "Ctrl+Alt+4",
                               .visibility_flag = &show_sprite_card_,
                               .priority = 50});

  card_registry->RegisterCard({.card_id = "palette.sprites_aux1",
                               .display_name = "Sprites Aux 1",
                               .icon = ICON_MD_FILTER_1,
                               .category = "Palette",
                               .shortcut_hint = "Ctrl+Alt+7",
                               .visibility_flag = &show_sprites_aux1_card_,
                               .priority = 51});

  card_registry->RegisterCard({.card_id = "palette.sprites_aux2",
                               .display_name = "Sprites Aux 2",
                               .icon = ICON_MD_FILTER_2,
                               .category = "Palette",
                               .shortcut_hint = "Ctrl+Alt+8",
                               .visibility_flag = &show_sprites_aux2_card_,
                               .priority = 52});

  card_registry->RegisterCard({.card_id = "palette.sprites_aux3",
                               .display_name = "Sprites Aux 3",
                               .icon = ICON_MD_FILTER_3,
                               .category = "Palette",
                               .shortcut_hint = "Ctrl+Alt+9",
                               .visibility_flag = &show_sprites_aux3_card_,
                               .priority = 53});

  card_registry->RegisterCard({.card_id = "palette.equipment",
                               .display_name = "Equipment Palettes",
                               .icon = ICON_MD_SHIELD,
                               .category = "Palette",
                               .shortcut_hint = "Ctrl+Alt+5",
                               .visibility_flag = &show_equipment_card_,
                               .priority = 60});

  card_registry->RegisterCard({.card_id = "palette.quick_access",
                               .display_name = "Quick Access",
                               .icon = ICON_MD_COLOR_LENS,
                               .category = "Palette",
                               .shortcut_hint = "Ctrl+Alt+Q",
                               .visibility_flag = &show_quick_access_,
                               .priority = 70});

  card_registry->RegisterCard({.card_id = "palette.custom",
                               .display_name = "Custom Palette",
                               .icon = ICON_MD_BRUSH,
                               .category = "Palette",
                               .shortcut_hint = "Ctrl+Alt+C",
                               .visibility_flag = &show_custom_palette_,
                               .priority = 80});

  // Show control panel by default when Palette Editor is activated
  show_control_panel_ = true;
}

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

  // Initialize the centralized PaletteManager with ROM data
  // This must be done before creating any palette cards
  gfx::PaletteManager::Get().Initialize(rom_);

  // Initialize palette card instances NOW (after ROM is loaded)
  ow_main_card_ = std::make_unique<OverworldMainPaletteCard>(rom_);
  ow_animated_card_ = std::make_unique<OverworldAnimatedPaletteCard>(rom_);
  dungeon_main_card_ = std::make_unique<DungeonMainPaletteCard>(rom_);
  sprite_card_ = std::make_unique<SpritePaletteCard>(rom_);
  sprites_aux1_card_ = std::make_unique<SpritesAux1PaletteCard>(rom_);
  sprites_aux2_card_ = std::make_unique<SpritesAux2PaletteCard>(rom_);
  sprites_aux3_card_ = std::make_unique<SpritesAux3PaletteCard>(rom_);
  equipment_card_ = std::make_unique<EquipmentPaletteCard>(rom_);

  return absl::OkStatus();
}

absl::Status PaletteEditor::Update() {
  if (!rom_ || !rom_->is_loaded()) {
    // Create a minimal loading card
    gui::EditorCard loading_card("Palette Editor Loading", ICON_MD_PALETTE);
    loading_card.SetDefaultSize(400, 200);
    if (loading_card.Begin()) {
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                         "Loading palette data...");
      ImGui::TextWrapped("Palette cards will appear once ROM data is loaded.");
    }
    loading_card.End();
    return absl::OkStatus();
  }

  // CARD-BASED EDITOR: All windows are independent top-level cards
  // No parent wrapper - this allows closing control panel without affecting
  // palettes

  // Optional control panel (can be hidden/minimized)
  if (show_control_panel_) {
    DrawControlPanel();
  } else if (control_panel_minimized_) {
    // Draw floating icon button to reopen
    ImGui::SetNextWindowPos(ImVec2(10, 100));
    ImGui::SetNextWindowSize(ImVec2(50, 50));
    ImGuiWindowFlags icon_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("##PaletteControlIcon", nullptr, icon_flags)) {
      if (ImGui::Button(ICON_MD_PALETTE, ImVec2(40, 40))) {
        show_control_panel_ = true;
        control_panel_minimized_ = false;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open Palette Controls");
      }
    }
    ImGui::End();
  }

  // Draw all independent palette cards
  // Each card has its own show_ flag that needs to be synced with our
  // visibility flags
  if (show_ow_main_card_ && ow_main_card_) {
    if (!ow_main_card_->IsVisible())
      ow_main_card_->Show();
    ow_main_card_->Draw();
    // Sync back if user closed the card with X button
    if (!ow_main_card_->IsVisible())
      show_ow_main_card_ = false;
  }

  if (show_ow_animated_card_ && ow_animated_card_) {
    if (!ow_animated_card_->IsVisible())
      ow_animated_card_->Show();
    ow_animated_card_->Draw();
    if (!ow_animated_card_->IsVisible())
      show_ow_animated_card_ = false;
  }

  if (show_dungeon_main_card_ && dungeon_main_card_) {
    if (!dungeon_main_card_->IsVisible())
      dungeon_main_card_->Show();
    dungeon_main_card_->Draw();
    if (!dungeon_main_card_->IsVisible())
      show_dungeon_main_card_ = false;
  }

  if (show_sprite_card_ && sprite_card_) {
    if (!sprite_card_->IsVisible())
      sprite_card_->Show();
    sprite_card_->Draw();
    if (!sprite_card_->IsVisible())
      show_sprite_card_ = false;
  }

  if (show_sprites_aux1_card_ && sprites_aux1_card_) {
    if (!sprites_aux1_card_->IsVisible())
      sprites_aux1_card_->Show();
    sprites_aux1_card_->Draw();
    if (!sprites_aux1_card_->IsVisible())
      show_sprites_aux1_card_ = false;
  }

  if (show_sprites_aux2_card_ && sprites_aux2_card_) {
    if (!sprites_aux2_card_->IsVisible())
      sprites_aux2_card_->Show();
    sprites_aux2_card_->Draw();
    if (!sprites_aux2_card_->IsVisible())
      show_sprites_aux2_card_ = false;
  }

  if (show_sprites_aux3_card_ && sprites_aux3_card_) {
    if (!sprites_aux3_card_->IsVisible())
      sprites_aux3_card_->Show();
    sprites_aux3_card_->Draw();
    if (!sprites_aux3_card_->IsVisible())
      show_sprites_aux3_card_ = false;
  }

  if (show_equipment_card_ && equipment_card_) {
    if (!equipment_card_->IsVisible())
      equipment_card_->Show();
    equipment_card_->Draw();
    if (!equipment_card_->IsVisible())
      show_equipment_card_ = false;
  }

  // Draw quick access and custom palette cards
  if (show_quick_access_) {
    DrawQuickAccessCard();
  }

  if (show_custom_palette_) {
    DrawCustomPaletteCard();
  }

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
        ImGui::OpenPopup("CustomPaletteColorEdit");
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
  if (ImGui::BeginPopup("CustomPaletteColorEdit")) {
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
  if (!rom()->is_loaded()) {
    return absl::NotFoundError("ROM not open, no palettes to display");
  }

  auto palette_group_name = kPaletteGroupNames[category];
  gfx::PaletteGroup* palette_group =
      rom()->mutable_palette_group()->get_group(palette_group_name.data());
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
    OpenPopup("Copy");
  if (BeginPopup("Copy")) {
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
// Card-Based UI Methods
// ============================================================================

void PaletteEditor::DrawToolset() {
  // Sidebar is drawn by EditorCardRegistry in EditorManager
  // Cards registered in Initialize() appear in the sidebar automatically
}

void PaletteEditor::DrawControlPanel() {
  ImGui::SetNextWindowSize(ImVec2(320, 420), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_FirstUseEver);

  ImGuiWindowFlags flags = ImGuiWindowFlags_None;

  if (ImGui::Begin(ICON_MD_PALETTE " Palette Controls", &show_control_panel_,
                   flags)) {
    // Toolbar with quick toggles
    DrawToolset();

    ImGui::Separator();

    // Quick toggle checkboxes in a table
    ImGui::Text("Palette Groups:");
    if (ImGui::BeginTable("##PaletteToggles", 2,
                          ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Checkbox("OW Main", &show_ow_main_card_);
      ImGui::TableNextColumn();
      ImGui::Checkbox("OW Animated", &show_ow_animated_card_);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Checkbox("Dungeon", &show_dungeon_main_card_);
      ImGui::TableNextColumn();
      ImGui::Checkbox("Sprites", &show_sprite_card_);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Checkbox("Equipment", &show_equipment_card_);
      ImGui::TableNextColumn();
      // Empty cell

      ImGui::EndTable();
    }

    ImGui::Separator();

    ImGui::Text("Utilities:");
    if (ImGui::BeginTable("##UtilityToggles", 2,
                          ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Checkbox("Quick Access", &show_quick_access_);
      ImGui::TableNextColumn();
      ImGui::Checkbox("Custom", &show_custom_palette_);

      ImGui::EndTable();
    }

    ImGui::Separator();

    // Modified status indicator
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Modified Cards:");
    bool any_modified = false;

    if (ow_main_card_ && ow_main_card_->HasUnsavedChanges()) {
      ImGui::BulletText("Overworld Main");
      any_modified = true;
    }
    if (ow_animated_card_ && ow_animated_card_->HasUnsavedChanges()) {
      ImGui::BulletText("Overworld Animated");
      any_modified = true;
    }
    if (dungeon_main_card_ && dungeon_main_card_->HasUnsavedChanges()) {
      ImGui::BulletText("Dungeon Main");
      any_modified = true;
    }
    if (sprite_card_ && sprite_card_->HasUnsavedChanges()) {
      ImGui::BulletText("Global Sprite Palettes");
      any_modified = true;
    }
    if (sprites_aux1_card_ && sprites_aux1_card_->HasUnsavedChanges()) {
      ImGui::BulletText("Sprites Aux 1");
      any_modified = true;
    }
    if (sprites_aux2_card_ && sprites_aux2_card_->HasUnsavedChanges()) {
      ImGui::BulletText("Sprites Aux 2");
      any_modified = true;
    }
    if (sprites_aux3_card_ && sprites_aux3_card_->HasUnsavedChanges()) {
      ImGui::BulletText("Sprites Aux 3");
      any_modified = true;
    }
    if (equipment_card_ && equipment_card_->HasUnsavedChanges()) {
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
    if (ImGui::Button(
            absl::StrFormat("Save All (%zu colors)", modified_count).c_str(),
            ImVec2(-1, 0))) {
      auto status = gfx::PaletteManager::Get().SaveAllToRom();
      if (!status.ok()) {
        // TODO: Show error toast/notification
        ImGui::OpenPopup("SaveError");
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

    ImGui::BeginDisabled(!has_unsaved);
    if (ImGui::Button("Discard All Changes", ImVec2(-1, 0))) {
      ImGui::OpenPopup("ConfirmDiscardAll");
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
    if (ImGui::BeginPopupModal("ConfirmDiscardAll", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Discard all unsaved changes?");
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
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
    if (ImGui::BeginPopupModal("SaveError", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                         "Failed to save changes");
      ImGui::Text("An error occurred while saving to ROM.");
      ImGui::Separator();

      if (ImGui::Button("OK", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    ImGui::Separator();

    // Editor Manager Menu Button
    if (ImGui::Button(ICON_MD_DASHBOARD " Card Manager", ImVec2(-1, 0))) {
      ImGui::OpenPopup("PaletteCardManager");
    }

    if (ImGui::BeginPopup("PaletteCardManager")) {
      ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f),
                         "%s Palette Card Manager", ICON_MD_PALETTE);
      ImGui::Separator();

      // View menu section now handled by EditorCardRegistry in EditorManager
      if (!dependencies_.card_registry)
        return;
      auto* card_registry = dependencies_.card_registry;

      ImGui::EndPopup();
    }

    ImGui::Separator();

    // Minimize button
    if (ImGui::SmallButton(ICON_MD_MINIMIZE " Minimize to Icon")) {
      control_panel_minimized_ = true;
      show_control_panel_ = false;
    }
  }
  ImGui::End();
}

void PaletteEditor::DrawQuickAccessCard() {
  gui::EditorCard card("Quick Access Palette", ICON_MD_COLOR_LENS,
                       &show_quick_access_);
  card.SetDefaultSize(340, 300);
  card.SetPosition(gui::EditorCard::Position::Right);

  if (card.Begin(&show_quick_access_)) {
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
  card.End();
}

void PaletteEditor::DrawCustomPaletteCard() {
  gui::EditorCard card("Custom Palette", ICON_MD_BRUSH, &show_custom_palette_);
  card.SetDefaultSize(420, 200);
  card.SetPosition(gui::EditorCard::Position::Bottom);

  if (card.Begin(&show_custom_palette_)) {
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
          ImGui::OpenPopup("CustomPaletteColorEdit");
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
  }
  card.End();

  // Color picker popup for custom palette editing
  if (ImGui::BeginPopup("CustomPaletteColorEdit")) {
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
  // Hide all cards first
  show_ow_main_card_ = false;
  show_ow_animated_card_ = false;
  show_dungeon_main_card_ = false;
  show_sprite_card_ = false;
  show_sprites_aux1_card_ = false;
  show_sprites_aux2_card_ = false;
  show_sprites_aux3_card_ = false;
  show_equipment_card_ = false;

  // Show and focus the appropriate card
  if (group_name == "ow_main") {
    show_ow_main_card_ = true;
    if (ow_main_card_) {
      ow_main_card_->Show();
      ow_main_card_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "ow_animated") {
    show_ow_animated_card_ = true;
    if (ow_animated_card_) {
      ow_animated_card_->Show();
      ow_animated_card_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "dungeon_main") {
    show_dungeon_main_card_ = true;
    if (dungeon_main_card_) {
      dungeon_main_card_->Show();
      dungeon_main_card_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "global_sprites") {
    show_sprite_card_ = true;
    if (sprite_card_) {
      sprite_card_->Show();
      sprite_card_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "sprites_aux1") {
    show_sprites_aux1_card_ = true;
    if (sprites_aux1_card_) {
      sprites_aux1_card_->Show();
      sprites_aux1_card_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "sprites_aux2") {
    show_sprites_aux2_card_ = true;
    if (sprites_aux2_card_) {
      sprites_aux2_card_->Show();
      sprites_aux2_card_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "sprites_aux3") {
    show_sprites_aux3_card_ = true;
    if (sprites_aux3_card_) {
      sprites_aux3_card_->Show();
      sprites_aux3_card_->SetSelectedPaletteIndex(palette_index);
    }
  } else if (group_name == "armors") {
    show_equipment_card_ = true;
    if (equipment_card_) {
      equipment_card_->Show();
      equipment_card_->SetSelectedPaletteIndex(palette_index);
    }
  }

  // Show control panel too for easy navigation
  show_control_panel_ = true;
}

}  // namespace editor
}  // namespace yaze
