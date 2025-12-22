#include "app/gui/core/platform_keys.h"

#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace yaze {
namespace gui {

namespace {

// Cache the platform detection result
Platform g_cached_platform = Platform::kWindows;
bool g_platform_initialized = false;

#ifdef __EMSCRIPTEN__
// JavaScript function to detect macOS in browser
EM_JS(bool, js_is_mac_platform, (), {
  return navigator.platform.toUpperCase().indexOf('MAC') >= 0;
});
#endif

void InitializePlatform() {
  if (g_platform_initialized) return;

#ifdef __EMSCRIPTEN__
  g_cached_platform =
      js_is_mac_platform() ? Platform::kWebMac : Platform::kWebOther;
#elif defined(__APPLE__)
  g_cached_platform = Platform::kMacOS;
#elif defined(_WIN32)
  g_cached_platform = Platform::kWindows;
#else
  g_cached_platform = Platform::kLinux;
#endif

  g_platform_initialized = true;
}

}  // namespace

Platform GetCurrentPlatform() {
  InitializePlatform();
  return g_cached_platform;
}

bool IsMacPlatform() {
  Platform p = GetCurrentPlatform();
  return p == Platform::kMacOS || p == Platform::kWebMac;
}

const char* GetCtrlDisplayName() {
  return IsMacPlatform() ? "Cmd" : "Ctrl";
}

const char* GetAltDisplayName() { return IsMacPlatform() ? "Opt" : "Alt"; }

const char* GetKeyName(ImGuiKey key) {
  // Handle special keys
  switch (key) {
    case ImGuiKey_Space:
      return "Space";
    case ImGuiKey_Tab:
      return "Tab";
    case ImGuiKey_Enter:
      return "Enter";
    case ImGuiKey_Escape:
      return "Esc";
    case ImGuiKey_Backspace:
      return "Backspace";
    case ImGuiKey_Delete:
      return "Delete";
    case ImGuiKey_Insert:
      return "Insert";
    case ImGuiKey_Home:
      return "Home";
    case ImGuiKey_End:
      return "End";
    case ImGuiKey_PageUp:
      return "PageUp";
    case ImGuiKey_PageDown:
      return "PageDown";
    case ImGuiKey_LeftArrow:
      return "Left";
    case ImGuiKey_RightArrow:
      return "Right";
    case ImGuiKey_UpArrow:
      return "Up";
    case ImGuiKey_DownArrow:
      return "Down";

    // Function keys
    case ImGuiKey_F1:
      return "F1";
    case ImGuiKey_F2:
      return "F2";
    case ImGuiKey_F3:
      return "F3";
    case ImGuiKey_F4:
      return "F4";
    case ImGuiKey_F5:
      return "F5";
    case ImGuiKey_F6:
      return "F6";
    case ImGuiKey_F7:
      return "F7";
    case ImGuiKey_F8:
      return "F8";
    case ImGuiKey_F9:
      return "F9";
    case ImGuiKey_F10:
      return "F10";
    case ImGuiKey_F11:
      return "F11";
    case ImGuiKey_F12:
      return "F12";

    // Letter keys
    case ImGuiKey_A:
      return "A";
    case ImGuiKey_B:
      return "B";
    case ImGuiKey_C:
      return "C";
    case ImGuiKey_D:
      return "D";
    case ImGuiKey_E:
      return "E";
    case ImGuiKey_F:
      return "F";
    case ImGuiKey_G:
      return "G";
    case ImGuiKey_H:
      return "H";
    case ImGuiKey_I:
      return "I";
    case ImGuiKey_J:
      return "J";
    case ImGuiKey_K:
      return "K";
    case ImGuiKey_L:
      return "L";
    case ImGuiKey_M:
      return "M";
    case ImGuiKey_N:
      return "N";
    case ImGuiKey_O:
      return "O";
    case ImGuiKey_P:
      return "P";
    case ImGuiKey_Q:
      return "Q";
    case ImGuiKey_R:
      return "R";
    case ImGuiKey_S:
      return "S";
    case ImGuiKey_T:
      return "T";
    case ImGuiKey_U:
      return "U";
    case ImGuiKey_V:
      return "V";
    case ImGuiKey_W:
      return "W";
    case ImGuiKey_X:
      return "X";
    case ImGuiKey_Y:
      return "Y";
    case ImGuiKey_Z:
      return "Z";

    // Number keys
    case ImGuiKey_0:
      return "0";
    case ImGuiKey_1:
      return "1";
    case ImGuiKey_2:
      return "2";
    case ImGuiKey_3:
      return "3";
    case ImGuiKey_4:
      return "4";
    case ImGuiKey_5:
      return "5";
    case ImGuiKey_6:
      return "6";
    case ImGuiKey_7:
      return "7";
    case ImGuiKey_8:
      return "8";
    case ImGuiKey_9:
      return "9";

    // Punctuation
    case ImGuiKey_Comma:
      return ",";
    case ImGuiKey_Period:
      return ".";
    case ImGuiKey_Slash:
      return "/";
    case ImGuiKey_Semicolon:
      return ";";
    case ImGuiKey_Apostrophe:
      return "'";
    case ImGuiKey_LeftBracket:
      return "[";
    case ImGuiKey_RightBracket:
      return "]";
    case ImGuiKey_Backslash:
      return "\\";
    case ImGuiKey_Minus:
      return "-";
    case ImGuiKey_Equal:
      return "=";
    case ImGuiKey_GraveAccent:
      return "`";

    default:
      return "?";
  }
}

std::string FormatShortcut(const std::vector<ImGuiKey>& keys) {
  if (keys.empty()) return "";

  std::string result;
  bool has_primary = false;
  bool has_shift = false;
  bool has_alt = false;
  bool has_super = false;
  ImGuiKey main_key = ImGuiKey_None;

  // First pass: identify modifiers and main key
  for (ImGuiKey key : keys) {
    int key_value = static_cast<int>(key);
    if (key_value & ImGuiMod_Mask_) {
      if (key_value & ImGuiMod_Ctrl) has_primary = true;
      if (key_value & ImGuiMod_Shift) has_shift = true;
      if (key_value & ImGuiMod_Alt) has_alt = true;
      if (key_value & ImGuiMod_Super) has_super = true;
      continue;
    }
    if (key == ImGuiMod_Shortcut) {
      has_primary = true;
      continue;
    }
    if (key == ImGuiMod_Shift) {
      has_shift = true;
      continue;
    }
    if (key == ImGuiMod_Alt) {
      has_alt = true;
      continue;
    }
    if (key == ImGuiMod_Super) {
      has_super = true;
      continue;
    }

    main_key = key;
  }

  // Build display string with modifiers in consistent order
  // On macOS: primary modifier displays as "Cmd"
  // On other platforms: primary modifier displays as "Ctrl"
  if (has_primary) {
    result += GetCtrlDisplayName();
    result += "+";
  }
  if (has_super) {
    // Super key (Cmd on macOS, Win/Super elsewhere)
    result += IsMacPlatform() ? "Cmd" : "Win";
    result += "+";
  }
  if (has_alt) {
    result += GetAltDisplayName();
    result += "+";
  }
  if (has_shift) {
    result += "Shift+";
  }

  // Add the main key
  if (main_key != ImGuiKey_None) {
    result += GetKeyName(main_key);
  }

  return result;
}

std::string FormatCtrlShortcut(ImGuiKey key) {
  return FormatShortcut({ImGuiMod_Ctrl, key});
}

std::string FormatCtrlShiftShortcut(ImGuiKey key) {
  return FormatShortcut({ImGuiMod_Ctrl, ImGuiMod_Shift, key});
}

}  // namespace gui
}  // namespace yaze
