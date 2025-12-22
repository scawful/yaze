#ifndef YAZE_APP_CORE_INPUT_H
#define YAZE_APP_CORE_INPUT_H

#define IMGUI_DEFINE_MATH_OPERATORS

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "absl/strings/string_view.h"
#include "app/gfx/types/snes_tile.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

constexpr ImVec2 kDefaultModalSize = ImVec2(200, 0);
constexpr ImVec2 kZeroPos = ImVec2(0, 0);

IMGUI_API bool InputHex(const char* label, uint64_t* data);
IMGUI_API bool InputHex(const char* label, int* data, int num_digits = 4,
                        float input_width = 50.f);

IMGUI_API bool InputHexShort(const char* label, uint32_t* data);
IMGUI_API bool InputHexWord(const char* label, uint16_t* data,
                            float input_width = 50.f, bool no_step = false);
IMGUI_API bool InputHexWord(const char* label, int16_t* data,
                            float input_width = 50.f, bool no_step = false);
IMGUI_API bool InputHexByte(const char* label, uint8_t* data,
                            float input_width = 50.f, bool no_step = false);

IMGUI_API bool InputHexByte(const char* label, uint8_t* data, uint8_t max_value,
                            float input_width = 50.f, bool no_step = false);

// Result type for InputHex functions that need to distinguish between
// immediate changes (button/wheel) and text-edit changes (deferred)
struct InputHexResult {
  bool changed;           // Value was modified (any source)
  bool immediate;         // Change was from button/wheel (apply now)
  bool text_committed;    // Change was from text edit and committed (deactivated)

  // Convenience: true if change should be applied immediately
  // Use this instead of: InputHex(...) && IsItemDeactivatedAfterEdit()
  bool ShouldApply() const { return immediate || text_committed; }

  // Implicit bool conversion for backwards compatibility
  operator bool() const { return changed; }
};

// New API that properly reports change source
IMGUI_API InputHexResult InputHexByteEx(const char* label, uint8_t* data,
                                        float input_width = 50.f,
                                        bool no_step = false);
IMGUI_API InputHexResult InputHexByteEx(const char* label, uint8_t* data,
                                        uint8_t max_value,
                                        float input_width = 50.f,
                                        bool no_step = false);
IMGUI_API InputHexResult InputHexWordEx(const char* label, uint16_t* data,
                                        float input_width = 50.f,
                                        bool no_step = false);

// Custom hex input functions that properly respect width
IMGUI_API bool InputHexByteCustom(const char* label, uint8_t* data,
                                  float input_width = 50.f);
IMGUI_API bool InputHexWordCustom(const char* label, uint16_t* data,
                                  float input_width = 70.f);

IMGUI_API void Paragraph(const std::string& text);

IMGUI_API bool ClickableText(const std::string& text);

IMGUI_API bool ListBox(const char* label, int* current_item,
                       const std::vector<std::string>& items,
                       int height_in_items = -1);

bool InputTileInfo(const char* label, gfx::TileInfo* tile_info);

using ItemLabelFlags = enum ItemLabelFlag {
  Left = 1u << 0u,
  Right = 1u << 1u,
  Default = Left,
};

IMGUI_API void ItemLabel(absl::string_view title, ItemLabelFlags flags);

IMGUI_API ImGuiID GetID(const std::string& id);

ImGuiKey MapKeyToImGuiKey(char key);

using GuiElement = std::variant<std::function<void()>, std::string>;

struct Table {
  const char* id;
  int num_columns;
  ImGuiTableFlags flags;
  ImVec2 size;
  std::vector<std::string> column_labels;
  std::vector<GuiElement> column_contents;
};

void AddTableColumn(Table& table, const std::string& label, GuiElement element);

IMGUI_API bool OpenUrl(const std::string& url);

void MemoryEditorPopup(const std::string& label, std::span<uint8_t> memory);

// Slider with mouse wheel support
IMGUI_API bool SliderFloatWheel(const char* label, float* v, float v_min,
                                float v_max, const char* format = "%.3f",
                                float wheel_step = 0.05f,
                                ImGuiSliderFlags flags = 0);
IMGUI_API bool SliderIntWheel(const char* label, int* v, int v_min, int v_max,
                              const char* format = "%d", int wheel_step = 1,
                              ImGuiSliderFlags flags = 0);

}  // namespace gui
}  // namespace yaze

#endif
