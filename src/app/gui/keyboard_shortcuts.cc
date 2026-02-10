// SPDX-License-Identifier: MIT
// Implementation of keyboard shortcut overlay system for yaze

#include "app/gui/keyboard_shortcuts.h"

#include <algorithm>
#include <cstring>

#include "app/gui/core/color.h"
#include "app/gui/core/platform_keys.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

namespace {

// Key name lookup table (local to this file for Shortcut::GetDisplayString)
// Note: gui::GetKeyName from platform_keys.h is the canonical version
constexpr struct {
  ImGuiKey key;
  const char* name;
} kLocalKeyNames[] = {
    {ImGuiKey_Tab, "Tab"},
    {ImGuiKey_LeftArrow, "Left"},
    {ImGuiKey_RightArrow, "Right"},
    {ImGuiKey_UpArrow, "Up"},
    {ImGuiKey_DownArrow, "Down"},
    {ImGuiKey_PageUp, "PageUp"},
    {ImGuiKey_PageDown, "PageDown"},
    {ImGuiKey_Home, "Home"},
    {ImGuiKey_End, "End"},
    {ImGuiKey_Insert, "Insert"},
    {ImGuiKey_Delete, "Delete"},
    {ImGuiKey_Backspace, "Backspace"},
    {ImGuiKey_Space, "Space"},
    {ImGuiKey_Enter, "Enter"},
    {ImGuiKey_Escape, "Escape"},
    {ImGuiKey_A, "A"},
    {ImGuiKey_B, "B"},
    {ImGuiKey_C, "C"},
    {ImGuiKey_D, "D"},
    {ImGuiKey_E, "E"},
    {ImGuiKey_F, "F"},
    {ImGuiKey_G, "G"},
    {ImGuiKey_H, "H"},
    {ImGuiKey_I, "I"},
    {ImGuiKey_J, "J"},
    {ImGuiKey_K, "K"},
    {ImGuiKey_L, "L"},
    {ImGuiKey_M, "M"},
    {ImGuiKey_N, "N"},
    {ImGuiKey_O, "O"},
    {ImGuiKey_P, "P"},
    {ImGuiKey_Q, "Q"},
    {ImGuiKey_R, "R"},
    {ImGuiKey_S, "S"},
    {ImGuiKey_T, "T"},
    {ImGuiKey_U, "U"},
    {ImGuiKey_V, "V"},
    {ImGuiKey_W, "W"},
    {ImGuiKey_X, "X"},
    {ImGuiKey_Y, "Y"},
    {ImGuiKey_Z, "Z"},
    {ImGuiKey_0, "0"},
    {ImGuiKey_1, "1"},
    {ImGuiKey_2, "2"},
    {ImGuiKey_3, "3"},
    {ImGuiKey_4, "4"},
    {ImGuiKey_5, "5"},
    {ImGuiKey_6, "6"},
    {ImGuiKey_7, "7"},
    {ImGuiKey_8, "8"},
    {ImGuiKey_9, "9"},
    {ImGuiKey_F1, "F1"},
    {ImGuiKey_F2, "F2"},
    {ImGuiKey_F3, "F3"},
    {ImGuiKey_F4, "F4"},
    {ImGuiKey_F5, "F5"},
    {ImGuiKey_F6, "F6"},
    {ImGuiKey_F7, "F7"},
    {ImGuiKey_F8, "F8"},
    {ImGuiKey_F9, "F9"},
    {ImGuiKey_F10, "F10"},
    {ImGuiKey_F11, "F11"},
    {ImGuiKey_F12, "F12"},
    {ImGuiKey_Minus, "-"},
    {ImGuiKey_Equal, "="},
    {ImGuiKey_LeftBracket, "["},
    {ImGuiKey_RightBracket, "]"},
    {ImGuiKey_Backslash, "\\"},
    {ImGuiKey_Semicolon, ";"},
    {ImGuiKey_Apostrophe, "'"},
    {ImGuiKey_Comma, ","},
    {ImGuiKey_Period, "."},
    {ImGuiKey_Slash, "/"},
    {ImGuiKey_GraveAccent, "`"},
};

const char* GetLocalKeyName(ImGuiKey key) {
  for (const auto& entry : kLocalKeyNames) {
    if (entry.key == key) {
      return entry.name;
    }
  }
  return "?";
}

}  // namespace

std::string Shortcut::GetDisplayString() const {
  std::string result;

  // Use runtime platform detection for correct modifier names
  // This handles native macOS, WASM on Mac browsers, and Windows/Linux
  if (requires_ctrl) {
    result += GetCtrlDisplayName();
    result += "+";
  }
  if (requires_alt) {
    result += GetAltDisplayName();
    result += "+";
  }
  if (requires_shift) {
    result += "Shift+";
  }

  // Use platform_keys.h GetKeyName for consistent key name formatting
  result += gui::GetKeyName(key);
  return result;
}

bool Shortcut::Matches(ImGuiKey pressed_key, bool ctrl, bool shift,
                       bool alt) const {
  if (!enabled) return false;
  if (pressed_key != key) return false;
  if (ctrl != requires_ctrl) return false;
  if (shift != requires_shift) return false;
  if (alt != requires_alt) return false;
  return true;
}

KeyboardShortcuts& KeyboardShortcuts::Get() {
  static KeyboardShortcuts instance;
  return instance;
}

void KeyboardShortcuts::RegisterShortcut(const Shortcut& shortcut) {
  shortcuts_[shortcut.id] = shortcut;
}

void KeyboardShortcuts::RegisterShortcut(const std::string& id,
                                         const std::string& description,
                                         ImGuiKey key, bool ctrl, bool shift,
                                         bool alt, const std::string& category,
                                         ShortcutContext context,
                                         std::function<void()> action) {
  Shortcut shortcut;
  shortcut.id = id;
  shortcut.description = description;
  shortcut.key = key;
  shortcut.requires_ctrl = ctrl;
  shortcut.requires_shift = shift;
  shortcut.requires_alt = alt;
  shortcut.category = category;
  shortcut.context = context;
  shortcut.action = action;
  shortcut.enabled = true;
  RegisterShortcut(shortcut);
}

void KeyboardShortcuts::UnregisterShortcut(const std::string& id) {
  shortcuts_.erase(id);
}

void KeyboardShortcuts::SetShortcutEnabled(const std::string& id,
                                           bool enabled) {
  auto it = shortcuts_.find(id);
  if (it != shortcuts_.end()) {
    it->second.enabled = enabled;
  }
}

bool KeyboardShortcuts::IsTextInputActive() {
  // Check if any ImGui input widget is active (text fields, etc.)
  // This prevents shortcuts from triggering while the user is typing
  return ImGui::GetIO().WantTextInput;
}

void KeyboardShortcuts::ProcessInput() {
  // Don't process shortcuts when text input is active
  if (IsTextInputActive()) {
    toggle_key_was_pressed_ = false;
    return;
  }

  const auto& io = ImGui::GetIO();
  bool ctrl = io.KeyCtrl;
  bool shift = io.KeyShift;
  bool alt = io.KeyAlt;

  // Handle '?' key to toggle overlay (Shift + /)
  // Note: '?' is typically Shift+Slash on US keyboards
  if (ImGui::IsKeyPressed(ImGuiKey_Slash) && shift && !ctrl && !alt) {
    if (!toggle_key_was_pressed_) {
      ToggleOverlay();
      toggle_key_was_pressed_ = true;
    }
  } else if (!ImGui::IsKeyPressed(ImGuiKey_Slash)) {
    toggle_key_was_pressed_ = false;
  }

  // Close overlay with Escape
  if (show_overlay_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    HideOverlay();
    return;
  }

  // Don't process other shortcuts while overlay is shown
  if (show_overlay_) {
    return;
  }

  // Check all registered shortcuts
  for (const auto& [id, shortcut] : shortcuts_) {
    if (!shortcut.enabled) continue;
    if (!IsShortcutActiveInContext(shortcut)) continue;

    // Check if this shortcut's key is pressed
    if (ImGui::IsKeyPressed(shortcut.key, false)) {
      if (shortcut.Matches(shortcut.key, ctrl, shift, alt)) {
        if (shortcut.action) {
          shortcut.action();
        }
      }
    }
  }
}

void KeyboardShortcuts::ToggleOverlay() {
  show_overlay_ = !show_overlay_;
  if (show_overlay_) {
    // Clear search filter when opening
    search_filter_[0] = '\0';
  }
}

void KeyboardShortcuts::DrawOverlay() {
  if (!show_overlay_) return;

  // Semi-transparent fullscreen background
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGuiWindowFlags overlay_flags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

  {
    StyledWindow overlay_bg(
        "##ShortcutOverlayBg",
        {.bg = ImVec4(0.0f, 0.0f, 0.0f, 0.7f),
         .padding = ImVec2(0, 0),
         .border_size = 0.0f},
        nullptr, overlay_flags);
    if (overlay_bg) {
      // Close on click outside modal
      if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
        ImVec2 mouse = ImGui::GetMousePos();
        ImVec2 modal_pos = ImVec2((io.DisplaySize.x - 600) * 0.5f,
                                  (io.DisplaySize.y - 500) * 0.5f);
        ImVec2 modal_size = ImVec2(600, 500);

        if (mouse.x < modal_pos.x || mouse.x > modal_pos.x + modal_size.x ||
            mouse.y < modal_pos.y || mouse.y > modal_pos.y + modal_size.y) {
          HideOverlay();
        }
      }
    }
  }

  // Draw the centered modal window
  DrawOverlayContent();
}

void KeyboardShortcuts::DrawOverlayContent() {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  ImGuiIO& io = ImGui::GetIO();

  // Calculate centered position
  float modal_width = 600.0f;
  float modal_height = 500.0f;
  ImVec2 modal_pos((io.DisplaySize.x - modal_width) * 0.5f,
                   (io.DisplaySize.y - modal_height) * 0.5f);

  ImGui::SetNextWindowPos(modal_pos);
  ImGui::SetNextWindowSize(ImVec2(modal_width, modal_height));

  // Style the modal window
  ImGuiWindowFlags modal_flags = ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoSavedSettings;

  StyledWindow modal(
      "##ShortcutOverlay",
      {.bg = ConvertColorToImVec4(theme.popup_bg),
       .border = ConvertColorToImVec4(theme.border),
       .padding = ImVec2(20, 16),
       .border_size = 1.0f,
       .rounding = 8.0f},
      nullptr, modal_flags);

  if (modal) {
    // Header
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);  // Use default font
    ImGui::TextColored(ConvertColorToImVec4(theme.accent), "Keyboard Shortcuts");
    ImGui::PopFont();

    ImGui::SameLine(modal_width - 60);
    if (ImGui::SmallButton("X##CloseOverlay")) {
      HideOverlay();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Close (Escape)");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Search filter
    ImGui::SetNextItemWidth(modal_width - 40);
    {
      StyleVarGuard frame_rounding(ImGuiStyleVar_FrameRounding, 4.0f);
      ImGui::InputTextWithHint("##ShortcutSearch", "Search shortcuts...",
                               search_filter_, sizeof(search_filter_));
    }

    // Context indicator
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", ShortcutContextToString(current_context_));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Scrollable content area
    ImGui::BeginChild("##ShortcutList", ImVec2(0, -30), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);

    // Collect shortcuts by category
    std::map<std::string, std::vector<const Shortcut*>> shortcuts_by_category;
    std::string filter_lower;
    for (char c : std::string(search_filter_)) {
      filter_lower += static_cast<char>(std::tolower(c));
    }

    for (const auto& [id, shortcut] : shortcuts_) {
      // Apply search filter
      if (!filter_lower.empty()) {
        std::string desc_lower;
        for (char c : shortcut.description) {
          desc_lower += static_cast<char>(std::tolower(c));
        }
        std::string cat_lower;
        for (char c : shortcut.category) {
          cat_lower += static_cast<char>(std::tolower(c));
        }
        std::string key_lower;
        for (char c : shortcut.GetDisplayString()) {
          key_lower += static_cast<char>(std::tolower(c));
        }

        if (desc_lower.find(filter_lower) == std::string::npos &&
            cat_lower.find(filter_lower) == std::string::npos &&
            key_lower.find(filter_lower) == std::string::npos) {
          continue;
        }
      }

      shortcuts_by_category[shortcut.category].push_back(&shortcut);
    }

    // Display shortcuts by category in order
    for (const auto& category : category_order_) {
      auto it = shortcuts_by_category.find(category);
      if (it != shortcuts_by_category.end() && !it->second.empty()) {
        DrawCategorySection(category, it->second);
      }
    }

    // Display any categories not in the order list
    for (const auto& [category, shortcuts] : shortcuts_by_category) {
      bool in_order = false;
      for (const auto& ordered : category_order_) {
        if (ordered == category) {
          in_order = true;
          break;
        }
      }
      if (!in_order && !shortcuts.empty()) {
        DrawCategorySection(category, shortcuts);
      }
    }

    ImGui::EndChild();

    // Footer
    ImGui::Separator();
    ImGui::TextDisabled("Press ? to toggle | Escape to close");
  }
}

void KeyboardShortcuts::DrawCategorySection(
    const std::string& category,
    const std::vector<const Shortcut*>& shortcuts) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  auto header_bg = ConvertColorToImVec4(theme.header);

  // Category header with collapsible behavior
  StyleColorGuard header_colors({
      {ImGuiCol_Header, header_bg},
      {ImGuiCol_HeaderHovered,
       ImVec4(header_bg.x + 0.05f, header_bg.y + 0.05f,
              header_bg.z + 0.05f, 1.0f)},
  });

  bool is_open = ImGui::CollapsingHeader(
      category.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

  if (is_open) {
    ImGui::Indent(10.0f);

    // Table for shortcuts
    if (ImGui::BeginTable("##ShortcutTable", 3,
                          ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Context", ImGuiTableColumnFlags_WidthFixed, 80.0f);

      for (const auto* shortcut : shortcuts) {
        DrawShortcutRow(*shortcut);
      }

      ImGui::EndTable();
    }

    ImGui::Unindent(10.0f);
    ImGui::Spacing();
  }
}

void KeyboardShortcuts::DrawShortcutRow(const Shortcut& shortcut) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  ImGui::TableNextRow();

  // Shortcut key combination
  ImGui::TableNextColumn();
  bool is_active = IsShortcutActiveInContext(shortcut);

  // Draw keyboard shortcut badge
  StyleColorGuard badge_colors({
      {ImGuiCol_Button, is_active ? ImVec4(0.2f, 0.3f, 0.4f, 0.8f)
                                  : ImVec4(0.15f, 0.15f, 0.15f, 0.6f)},
      {ImGuiCol_ButtonHovered, is_active ? ImVec4(0.25f, 0.35f, 0.45f, 0.9f)
                                         : ImVec4(0.2f, 0.2f, 0.2f, 0.7f)},
  });
  StyleVarGuard badge_vars({
      {ImGuiStyleVar_FrameRounding, 4.0f},
      {ImGuiStyleVar_FramePadding, ImVec2(6, 2)},
  });

  std::string display = shortcut.GetDisplayString();
  ImGui::SmallButton(display.c_str());

  // Description
  ImGui::TableNextColumn();
  ImVec4 text_color = is_active ? ConvertColorToImVec4(theme.text_primary)
                                : ConvertColorToImVec4(theme.text_secondary);
  ImGui::TextColored(text_color, "%s", shortcut.description.c_str());

  // Context indicator
  ImGui::TableNextColumn();
  if (shortcut.context != ShortcutContext::kGlobal) {
    ImGui::TextDisabled("%s", ShortcutContextToString(shortcut.context));
  }
}

bool KeyboardShortcuts::IsShortcutActiveInContext(
    const Shortcut& shortcut) const {
  if (shortcut.context == ShortcutContext::kGlobal) {
    return true;
  }
  return shortcut.context == current_context_;
}

std::vector<const Shortcut*> KeyboardShortcuts::GetShortcutsInCategory(
    const std::string& category) const {
  std::vector<const Shortcut*> result;
  for (const auto& [id, shortcut] : shortcuts_) {
    if (shortcut.category == category) {
      result.push_back(&shortcut);
    }
  }
  return result;
}

std::vector<const Shortcut*> KeyboardShortcuts::GetContextShortcuts() const {
  std::vector<const Shortcut*> result;
  for (const auto& [id, shortcut] : shortcuts_) {
    if (IsShortcutActiveInContext(shortcut)) {
      result.push_back(&shortcut);
    }
  }
  return result;
}

std::vector<std::string> KeyboardShortcuts::GetCategories() const {
  std::vector<std::string> result;
  for (const auto& [id, shortcut] : shortcuts_) {
    bool found = false;
    for (const auto& cat : result) {
      if (cat == shortcut.category) {
        found = true;
        break;
      }
    }
    if (!found) {
      result.push_back(shortcut.category);
    }
  }
  return result;
}

void KeyboardShortcuts::RegisterDefaultShortcuts(
    std::function<void()> open_callback,
    std::function<void()> save_callback,
    std::function<void()> save_as_callback,
    std::function<void()> close_callback,
    std::function<void()> undo_callback,
    std::function<void()> redo_callback,
    std::function<void()> copy_callback,
    std::function<void()> paste_callback,
    std::function<void()> cut_callback,
    std::function<void()> find_callback) {

  // === File Shortcuts ===
  if (open_callback) {
    RegisterShortcut("file.open", "Open ROM/Project", ImGuiKey_O,
                     true, false, false, "File", ShortcutContext::kGlobal,
                     open_callback);
  }

  if (save_callback) {
    RegisterShortcut("file.save", "Save", ImGuiKey_S,
                     true, false, false, "File", ShortcutContext::kGlobal,
                     save_callback);
  }

  if (save_as_callback) {
    RegisterShortcut("file.save_as", "Save As...", ImGuiKey_S,
                     true, true, false, "File", ShortcutContext::kGlobal,
                     save_as_callback);
  }

  if (close_callback) {
    RegisterShortcut("file.close", "Close", ImGuiKey_W,
                     true, false, false, "File", ShortcutContext::kGlobal,
                     close_callback);
  }

  // === Edit Shortcuts ===
  if (undo_callback) {
    RegisterShortcut("edit.undo", "Undo", ImGuiKey_Z,
                     true, false, false, "Edit", ShortcutContext::kGlobal,
                     undo_callback);
  }

  if (redo_callback) {
    RegisterShortcut("edit.redo", "Redo", ImGuiKey_Y,
                     true, false, false, "Edit", ShortcutContext::kGlobal,
                     redo_callback);

    // Also register Ctrl+Shift+Z for redo (common alternative)
    RegisterShortcut("edit.redo_alt", "Redo", ImGuiKey_Z,
                     true, true, false, "Edit", ShortcutContext::kGlobal,
                     redo_callback);
  }

  if (copy_callback) {
    RegisterShortcut("edit.copy", "Copy", ImGuiKey_C,
                     true, false, false, "Edit", ShortcutContext::kGlobal,
                     copy_callback);
  }

  if (paste_callback) {
    RegisterShortcut("edit.paste", "Paste", ImGuiKey_V,
                     true, false, false, "Edit", ShortcutContext::kGlobal,
                     paste_callback);
  }

  if (cut_callback) {
    RegisterShortcut("edit.cut", "Cut", ImGuiKey_X,
                     true, false, false, "Edit", ShortcutContext::kGlobal,
                     cut_callback);
  }

  if (find_callback) {
    RegisterShortcut("edit.find", "Find", ImGuiKey_F,
                     true, false, false, "Edit", ShortcutContext::kGlobal,
                     find_callback);
  }

  // === View Shortcuts ===
  RegisterShortcut("view.fullscreen", "Toggle Fullscreen", ImGuiKey_F11,
                   false, false, false, "View", ShortcutContext::kGlobal,
                   nullptr);  // Placeholder - implement in EditorManager

  RegisterShortcut("view.grid", "Toggle Grid", ImGuiKey_G,
                   true, false, false, "View", ShortcutContext::kGlobal,
                   nullptr);  // Placeholder

  RegisterShortcut("view.zoom_in", "Zoom In", ImGuiKey_Equal,
                   true, false, false, "View", ShortcutContext::kGlobal,
                   nullptr);  // Placeholder

  RegisterShortcut("view.zoom_out", "Zoom Out", ImGuiKey_Minus,
                   true, false, false, "View", ShortcutContext::kGlobal,
                   nullptr);  // Placeholder

  RegisterShortcut("view.zoom_reset", "Reset Zoom", ImGuiKey_0,
                   true, false, false, "View", ShortcutContext::kGlobal,
                   nullptr);  // Placeholder

  // === Navigation Shortcuts ===
  RegisterShortcut("nav.first", "Go to First", ImGuiKey_Home,
                   false, false, false, "Navigation", ShortcutContext::kGlobal,
                   nullptr);  // Placeholder

  RegisterShortcut("nav.last", "Go to Last", ImGuiKey_End,
                   false, false, false, "Navigation", ShortcutContext::kGlobal,
                   nullptr);  // Placeholder

  RegisterShortcut("nav.prev", "Previous", ImGuiKey_PageUp,
                   false, false, false, "Navigation", ShortcutContext::kGlobal,
                   nullptr);  // Placeholder

  RegisterShortcut("nav.next", "Next", ImGuiKey_PageDown,
                   false, false, false, "Navigation", ShortcutContext::kGlobal,
                   nullptr);  // Placeholder

  // === Emulator Shortcuts ===
  RegisterShortcut("emu.play_pause", "Play/Pause Emulator", ImGuiKey_Space,
                   false, false, false, "Editor", ShortcutContext::kEmulator,
                   nullptr);  // Placeholder

  RegisterShortcut("emu.reset", "Reset Emulator", ImGuiKey_R,
                   true, false, false, "Editor", ShortcutContext::kEmulator,
                   nullptr);  // Placeholder

  RegisterShortcut("emu.step", "Step Frame", ImGuiKey_F,
                   false, false, false, "Editor", ShortcutContext::kEmulator,
                   nullptr);  // Placeholder

  // === Overworld Editor Shortcuts ===
  RegisterShortcut("ow.select_all", "Select All Tiles", ImGuiKey_A,
                   true, false, false, "Editor", ShortcutContext::kOverworld,
                   nullptr);  // Placeholder

  RegisterShortcut("ow.deselect", "Deselect", ImGuiKey_D,
                   true, false, false, "Editor", ShortcutContext::kOverworld,
                   nullptr);  // Placeholder

  // === Dungeon Editor Shortcuts ===
  RegisterShortcut("dg.select_all", "Select All Objects", ImGuiKey_A,
                   true, false, false, "Editor", ShortcutContext::kDungeon,
                   nullptr);  // Placeholder

  RegisterShortcut("dg.delete", "Delete Selected", ImGuiKey_Delete,
                   false, false, false, "Editor", ShortcutContext::kDungeon,
                   nullptr);  // Placeholder

  RegisterShortcut("dg.duplicate", "Duplicate Selected", ImGuiKey_D,
                   true, false, false, "Editor", ShortcutContext::kDungeon,
                   nullptr);  // Placeholder
}

const char* ShortcutContextToString(ShortcutContext context) {
  switch (context) {
    case ShortcutContext::kGlobal:
      return "Global";
    case ShortcutContext::kOverworld:
      return "Overworld";
    case ShortcutContext::kDungeon:
      return "Dungeon";
    case ShortcutContext::kGraphics:
      return "Graphics";
    case ShortcutContext::kPalette:
      return "Palette";
    case ShortcutContext::kSprite:
      return "Sprite";
    case ShortcutContext::kMusic:
      return "Music";
    case ShortcutContext::kMessage:
      return "Message";
    case ShortcutContext::kEmulator:
      return "Emulator";
    case ShortcutContext::kCode:
      return "Code";
    default:
      return "Unknown";
  }
}

ShortcutContext EditorNameToContext(const std::string& editor_name) {
  if (editor_name == "Overworld") return ShortcutContext::kOverworld;
  if (editor_name == "Dungeon") return ShortcutContext::kDungeon;
  if (editor_name == "Graphics") return ShortcutContext::kGraphics;
  if (editor_name == "Palette") return ShortcutContext::kPalette;
  if (editor_name == "Sprite") return ShortcutContext::kSprite;
  if (editor_name == "Music") return ShortcutContext::kMusic;
  if (editor_name == "Message") return ShortcutContext::kMessage;
  if (editor_name == "Emulator") return ShortcutContext::kEmulator;
  if (editor_name == "Assembly" || editor_name == "Code")
    return ShortcutContext::kCode;
  return ShortcutContext::kGlobal;
}

}  // namespace gui
}  // namespace yaze
