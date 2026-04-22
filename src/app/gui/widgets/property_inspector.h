#ifndef YAZE_APP_GUI_WIDGETS_PROPERTY_INSPECTOR_H_
#define YAZE_APP_GUI_WIDGETS_PROPERTY_INSPECTOR_H_

#include <cstdint>
#include <string>
#include <type_traits>

#include "app/gui/core/color.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// Typed property editor widgets. Each DrawProperty/DrawPropertyHex overload
// returns true on commit (matching the ImGui::Input* contract). When called
// inside a BeginPropertyTable(...) / EndPropertyTable() block, the label is
// placed in the "Property" column and the editor in the "Value" column
// automatically. When called outside a property table, the widget renders
// inline with ImGui's default label-on-right placement.
//
// Undo integration: these functions do NOT post undo commands directly (no
// dependency from the widget library into editor code). Wrap the return
// value at the call site to bind into your editor's command manager:
//
//   if (gui::DrawProperty("Retention", &settings.retention_count,
//                         {.min = 1, .max = 365})) {
//     undo_manager_.Post(std::make_unique<SetRetentionCommand>(...));
//   }

struct PropertyOptions {
  // Numeric range. When max > min, the committed value is clamped. Otherwise
  // no clamp is applied (still used as slider bounds if a slider is selected).
  double min = 0.0;
  double max = 0.0;

  // Step controls for InputInt / InputFloat. Defaults produce ImGui standard
  // step of 1 (step) and 10 (step_fast).
  double step = 1.0;
  double step_fast = 10.0;

  // Printf format override for numeric types. If null, the widget uses
  // ImGui's default ("%d" for int, "%.3f" for float, etc.).
  const char* format = nullptr;

  // Optional tooltip shown when hovering the label cell.
  const char* tooltip = nullptr;

  // Disable interaction; the widget still renders but greyed out.
  bool read_only = false;

  // Extra ImGui input flags (e.g., ImGuiInputTextFlags_EnterReturnsTrue).
  ImGuiInputTextFlags text_flags = 0;
};

// Numeric / boolean / string / color property editors.
bool DrawProperty(const char* label, bool* value,
                  const PropertyOptions& opts = {});
bool DrawProperty(const char* label, int* value,
                  const PropertyOptions& opts = {});
bool DrawProperty(const char* label, std::uint8_t* value,
                  const PropertyOptions& opts = {});
bool DrawProperty(const char* label, std::uint16_t* value,
                  const PropertyOptions& opts = {});
bool DrawProperty(const char* label, std::uint32_t* value,
                  const PropertyOptions& opts = {});
bool DrawProperty(const char* label, float* value,
                  const PropertyOptions& opts = {});
bool DrawProperty(const char* label, double* value,
                  const PropertyOptions& opts = {});
bool DrawProperty(const char* label, std::string* value,
                  const PropertyOptions& opts = {});
bool DrawProperty(const char* label, ImVec4* value,
                  const PropertyOptions& opts = {});
bool DrawProperty(const char* label, Color* value,
                  const PropertyOptions& opts = {});

// Hex-formatted numeric editors. Hex input restricts character set to
// [0-9A-F] and formats output via %02X/%04X/%08X by default.
bool DrawPropertyHex(const char* label, std::uint8_t* value,
                     const PropertyOptions& opts = {});
bool DrawPropertyHex(const char* label, std::uint16_t* value,
                     const PropertyOptions& opts = {});
bool DrawPropertyHex(const char* label, std::uint32_t* value,
                     const PropertyOptions& opts = {});

// Enum combo. `items` is a null-terminated array of display labels indexed
// by the enum's underlying integer value. Example:
//
//   enum class Role { kReader, kWriter, kAdmin };
//   static constexpr const char* kRoles[] = {"Reader", "Writer", "Admin",
//                                             nullptr};
//   Role role = Role::kReader;
//   if (gui::DrawPropertyCombo("Role", &role, kRoles)) { ... }
template <typename EnumT>
bool DrawPropertyCombo(const char* label, EnumT* value,
                       const char* const* items,
                       const PropertyOptions& opts = {});

// ---------------------------------------------------------------------------
// Internal helpers exposed for tests and for template instantiations below.
// ---------------------------------------------------------------------------
namespace property_inspector_internal {

// True if a BeginPropertyTable table is currently active on the ImGui stack.
bool IsInPropertyTable();

// Emit the label cell (when in a table) or no-op (when inline). Sets the
// next item width to fill the value column when in a table.
void BeginRow(const char* label, const char* tooltip);

// Construct the ImGui widget ID. Inside a table we hide the label ("##") so
// the label cell is the only visible label; otherwise we pass it through.
std::string ResolveWidgetId(const char* label);

}  // namespace property_inspector_internal

// ---------------------------------------------------------------------------
// DrawPropertyCombo<EnumT> definition (header-only template).
// ---------------------------------------------------------------------------
template <typename EnumT>
bool DrawPropertyCombo(const char* label, EnumT* value,
                       const char* const* items, const PropertyOptions& opts) {
  static_assert(std::is_enum_v<EnumT>,
                "DrawPropertyCombo requires an enum type");
  int count = 0;
  while (items[count] != nullptr)
    ++count;
  int current = static_cast<int>(*value);
  if (current < 0 || current >= count)
    current = 0;

  property_inspector_internal::BeginRow(label, opts.tooltip);
  const std::string id = property_inspector_internal::ResolveWidgetId(label);

  ImGui::BeginDisabled(opts.read_only);
  const bool changed = ImGui::Combo(id.c_str(), &current, items, count);
  ImGui::EndDisabled();

  if (changed && !opts.read_only) {
    *value = static_cast<EnumT>(current);
  }
  return changed && !opts.read_only;
}

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_PROPERTY_INSPECTOR_H_
