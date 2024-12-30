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

void FileDialogPipeline(absl::string_view display_key,
                        absl::string_view file_extensions,
                        std::optional<absl::string_view> button_text,
                        std::function<void()> callback);

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

} // namespace gui
} // namespace yaze

#endif