#ifndef YAZE_APP_CORE_INPUT_H
#define YAZE_APP_CORE_INPUT_H

#define IMGUI_DEFINE_MATH_OPERATORS

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "app/gfx/snes_tile.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

constexpr ImVec2 kDefaultModalSize = ImVec2(200, 0);
constexpr ImVec2 kZeroPos = ImVec2(0, 0);

IMGUI_API bool InputHex(const char *label, uint64_t *data);
IMGUI_API bool InputHex(const char *label, int *data, int num_digits = 4,
                        float input_width = 50.f);

IMGUI_API bool InputHexShort(const char *label, uint32_t *data);
IMGUI_API bool InputHexWord(const char *label, uint16_t *data,
                            float input_width = 50.f, bool no_step = false);
IMGUI_API bool InputHexWord(const char *label, int16_t *data,
                            float input_width = 50.f, bool no_step = false);
IMGUI_API bool InputHexByte(const char *label, uint8_t *data,
                            float input_width = 50.f, bool no_step = false);

IMGUI_API bool InputHexByte(const char *label, uint8_t *data, uint8_t max_value,
                            float input_width = 50.f, bool no_step = false);

IMGUI_API bool ListBox(const char *label, int *current_item,
                       const std::vector<std::string> &items,
                       int height_in_items = -1);

bool InputTileInfo(const char *label, gfx::TileInfo *tile_info);

using ItemLabelFlags = enum ItemLabelFlag {
  Left = 1u << 0u,
  Right = 1u << 1u,
  Default = Left,
};

IMGUI_API void ItemLabel(absl::string_view title, ItemLabelFlags flags);

IMGUI_API ImGuiID GetID(const std::string &id);

using GuiElement = std::variant<std::function<void()>, std::string>;

struct Table {
  const char *id;
  int num_columns;
  ImGuiTableFlags flags;
  ImVec2 size;
  std::vector<std::string> column_labels;
  std::vector<GuiElement> column_contents;
};

void AddTableColumn(Table &table, const std::string &label, GuiElement element);

void DrawTable(Table &params);

static std::function<bool()> kDefaultEnabledCondition = []() { return false; };

struct MenuItem {
  std::string name;
  std::string shortcut;
  std::function<void()> callback;
  std::function<bool()> enabled_condition = kDefaultEnabledCondition;
  std::vector<MenuItem> subitems;
};
using Menu = std::array<MenuItem, 5>;

void DrawMenu(Menu &params);

enum MenuType {
  kFile,
  kEdit,
  kView,
  kTools,
  kHelp,
};

static Menu kMainMenu;

inline void AddToMenu(const std::string &label, const char *icon,
                      MenuType type) {
  if (icon) {
    kMainMenu[type].subitems.emplace_back(absl::StrCat(icon, " ", label), "",
                                          nullptr);
  } else {
    kMainMenu[type].subitems.emplace_back(label, "", nullptr);
  }
}

inline void AddToFileMenu(const std::string &label, const std::string &shortcut,
                          std::function<void()> callback) {
  kMainMenu[MenuType::kFile].subitems.emplace_back(label, shortcut, callback);
}

inline void AddToFileMenu(const std::string &label, const std::string &shortcut,
                          std::function<void()> callback,
                          std::function<bool()> enabled_condition,
                          std::vector<MenuItem> subitems) {
  kMainMenu[MenuType::kFile].subitems.emplace_back(label, shortcut, callback,
                                                   enabled_condition, subitems);
}

inline void AddToEditMenu(const std::string &label, const std::string &shortcut,
                          std::function<void()> callback) {
  kMainMenu[MenuType::kEdit].subitems.emplace_back(label, shortcut, callback);
}

inline void AddToViewMenu(const std::string &label, const std::string &shortcut,
                          std::function<void()> callback) {
  kMainMenu[MenuType::kView].subitems.emplace_back(label, shortcut, callback);
}

inline void AddToHelpMenu(const std::string &label, const std::string &shortcut,
                          std::function<void()> callback) {
  kMainMenu[MenuType::kHelp].subitems.emplace_back(label, shortcut, callback);
}

}  // namespace gui
}  // namespace yaze

#endif
