#include "app/gui/widgets/property_inspector.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "app/gui/core/color.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {
namespace property_inspector_internal {

bool IsInPropertyTable() {
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  return ctx != nullptr && ctx->CurrentTable != nullptr;
}

void BeginRow(const char* label, const char* tooltip) {
  if (!IsInPropertyTable()) {
    return;
  }
  ImGui::TableNextColumn();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  if (tooltip && tooltip[0] != '\0' && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  ImGui::TableNextColumn();
  ImGui::SetNextItemWidth(-FLT_MIN);
}

std::string ResolveWidgetId(const char* label) {
  std::string result;
  if (IsInPropertyTable()) {
    result.reserve(std::strlen(label) + 2);
    result.append("##").append(label);
  } else {
    result.assign(label);
  }
  return result;
}

}  // namespace property_inspector_internal

namespace {

namespace pi = property_inspector_internal;

template <typename T>
void ClampIfBounded(T* value, const PropertyOptions& opts) {
  if (opts.max > opts.min) {
    const T lo = static_cast<T>(opts.min);
    const T hi = static_cast<T>(opts.max);
    if (*value < lo)
      *value = lo;
    if (*value > hi)
      *value = hi;
  }
}

const char* IntFormat(const PropertyOptions& opts) {
  return opts.format ? opts.format : "%d";
}

const char* FloatFormat(const PropertyOptions& opts, const char* fallback) {
  return opts.format ? opts.format : fallback;
}

}  // namespace

bool DrawProperty(const char* label, bool* value, const PropertyOptions& opts) {
  pi::BeginRow(label, opts.tooltip);
  const std::string id = pi::ResolveWidgetId(label);
  ImGui::BeginDisabled(opts.read_only);
  const bool changed = ImGui::Checkbox(id.c_str(), value);
  ImGui::EndDisabled();
  return changed && !opts.read_only;
}

bool DrawProperty(const char* label, int* value, const PropertyOptions& opts) {
  pi::BeginRow(label, opts.tooltip);
  const std::string id = pi::ResolveWidgetId(label);
  const int step = static_cast<int>(opts.step);
  const int step_fast = static_cast<int>(opts.step_fast);
  ImGui::BeginDisabled(opts.read_only);
  const bool changed =
      ImGui::InputInt(id.c_str(), value, step, step_fast, opts.text_flags);
  ImGui::EndDisabled();
  if (changed && !opts.read_only)
    ClampIfBounded(value, opts);
  return changed && !opts.read_only;
}

bool DrawProperty(const char* label, std::uint8_t* value,
                  const PropertyOptions& opts) {
  pi::BeginRow(label, opts.tooltip);
  const std::string id = pi::ResolveWidgetId(label);
  const std::uint8_t step = static_cast<std::uint8_t>(opts.step);
  const std::uint8_t step_fast = static_cast<std::uint8_t>(opts.step_fast);
  ImGui::BeginDisabled(opts.read_only);
  const bool changed = ImGui::InputScalar(
      id.c_str(), ImGuiDataType_U8, value, step ? &step : nullptr,
      step_fast ? &step_fast : nullptr, IntFormat(opts), opts.text_flags);
  ImGui::EndDisabled();
  if (changed && !opts.read_only)
    ClampIfBounded(value, opts);
  return changed && !opts.read_only;
}

bool DrawProperty(const char* label, std::uint16_t* value,
                  const PropertyOptions& opts) {
  pi::BeginRow(label, opts.tooltip);
  const std::string id = pi::ResolveWidgetId(label);
  const std::uint16_t step = static_cast<std::uint16_t>(opts.step);
  const std::uint16_t step_fast = static_cast<std::uint16_t>(opts.step_fast);
  ImGui::BeginDisabled(opts.read_only);
  const bool changed = ImGui::InputScalar(
      id.c_str(), ImGuiDataType_U16, value, step ? &step : nullptr,
      step_fast ? &step_fast : nullptr, IntFormat(opts), opts.text_flags);
  ImGui::EndDisabled();
  if (changed && !opts.read_only)
    ClampIfBounded(value, opts);
  return changed && !opts.read_only;
}

bool DrawProperty(const char* label, std::uint32_t* value,
                  const PropertyOptions& opts) {
  pi::BeginRow(label, opts.tooltip);
  const std::string id = pi::ResolveWidgetId(label);
  const std::uint32_t step = static_cast<std::uint32_t>(opts.step);
  const std::uint32_t step_fast = static_cast<std::uint32_t>(opts.step_fast);
  ImGui::BeginDisabled(opts.read_only);
  const bool changed = ImGui::InputScalar(
      id.c_str(), ImGuiDataType_U32, value, step ? &step : nullptr,
      step_fast ? &step_fast : nullptr, IntFormat(opts), opts.text_flags);
  ImGui::EndDisabled();
  if (changed && !opts.read_only)
    ClampIfBounded(value, opts);
  return changed && !opts.read_only;
}

bool DrawProperty(const char* label, float* value,
                  const PropertyOptions& opts) {
  pi::BeginRow(label, opts.tooltip);
  const std::string id = pi::ResolveWidgetId(label);
  const float step = static_cast<float>(opts.step > 0 ? opts.step : 0.0);
  const float step_fast =
      static_cast<float>(opts.step_fast > 0 ? opts.step_fast : 0.0);
  ImGui::BeginDisabled(opts.read_only);
  const bool changed =
      ImGui::InputFloat(id.c_str(), value, step, step_fast,
                        FloatFormat(opts, "%.3f"), opts.text_flags);
  ImGui::EndDisabled();
  if (changed && !opts.read_only)
    ClampIfBounded(value, opts);
  return changed && !opts.read_only;
}

bool DrawProperty(const char* label, double* value,
                  const PropertyOptions& opts) {
  pi::BeginRow(label, opts.tooltip);
  const std::string id = pi::ResolveWidgetId(label);
  const double step = opts.step > 0 ? opts.step : 0.0;
  const double step_fast = opts.step_fast > 0 ? opts.step_fast : 0.0;
  ImGui::BeginDisabled(opts.read_only);
  const bool changed =
      ImGui::InputDouble(id.c_str(), value, step, step_fast,
                         FloatFormat(opts, "%.6f"), opts.text_flags);
  ImGui::EndDisabled();
  if (changed && !opts.read_only)
    ClampIfBounded(value, opts);
  return changed && !opts.read_only;
}

namespace {

int StringInputCallback(ImGuiInputTextCallbackData* data) {
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
    auto* str = static_cast<std::string*>(data->UserData);
    str->resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = str->data();
  }
  return 0;
}

}  // namespace

bool DrawProperty(const char* label, std::string* value,
                  const PropertyOptions& opts) {
  pi::BeginRow(label, opts.tooltip);
  const std::string id = pi::ResolveWidgetId(label);
  ImGui::BeginDisabled(opts.read_only);
  // Ensure capacity for in-place edits; callback grows the string as needed.
  if (value->capacity() < value->size() + 1) {
    value->reserve(value->size() + 32);
  }
  const bool changed =
      ImGui::InputText(id.c_str(), value->data(), value->capacity() + 1,
                       opts.text_flags | ImGuiInputTextFlags_CallbackResize,
                       &StringInputCallback, value);
  ImGui::EndDisabled();
  return changed && !opts.read_only;
}

bool DrawProperty(const char* label, ImVec4* value,
                  const PropertyOptions& opts) {
  pi::BeginRow(label, opts.tooltip);
  const std::string id = pi::ResolveWidgetId(label);
  ImGui::BeginDisabled(opts.read_only);
  const bool changed = ImGui::ColorEdit4(id.c_str(), &value->x);
  ImGui::EndDisabled();
  return changed && !opts.read_only;
}

bool DrawProperty(const char* label, Color* value,
                  const PropertyOptions& opts) {
  pi::BeginRow(label, opts.tooltip);
  const std::string id = pi::ResolveWidgetId(label);
  float rgba[4] = {value->red, value->green, value->blue, value->alpha};
  ImGui::BeginDisabled(opts.read_only);
  const bool changed = ImGui::ColorEdit4(id.c_str(), rgba);
  ImGui::EndDisabled();
  if (changed && !opts.read_only) {
    value->red = rgba[0];
    value->green = rgba[1];
    value->blue = rgba[2];
    value->alpha = rgba[3];
  }
  return changed && !opts.read_only;
}

namespace {

template <typename T>
bool DrawHexScalar(const char* label, T* value, ImGuiDataType type,
                   const char* default_fmt, const PropertyOptions& opts) {
  pi::BeginRow(label, opts.tooltip);
  const std::string id = pi::ResolveWidgetId(label);
  const char* fmt = opts.format ? opts.format : default_fmt;
  const T step = static_cast<T>(opts.step);
  const T step_fast = static_cast<T>(opts.step_fast);
  ImGui::BeginDisabled(opts.read_only);
  const bool changed = ImGui::InputScalar(
      id.c_str(), type, value, step ? &step : nullptr,
      step_fast ? &step_fast : nullptr, fmt,
      opts.text_flags | ImGuiInputTextFlags_CharsHexadecimal |
          ImGuiInputTextFlags_CharsUppercase);
  ImGui::EndDisabled();
  if (changed && !opts.read_only)
    ClampIfBounded(value, opts);
  return changed && !opts.read_only;
}

}  // namespace

bool DrawPropertyHex(const char* label, std::uint8_t* value,
                     const PropertyOptions& opts) {
  return DrawHexScalar(label, value, ImGuiDataType_U8, "%02X", opts);
}

bool DrawPropertyHex(const char* label, std::uint16_t* value,
                     const PropertyOptions& opts) {
  return DrawHexScalar(label, value, ImGuiDataType_U16, "%04X", opts);
}

bool DrawPropertyHex(const char* label, std::uint32_t* value,
                     const PropertyOptions& opts) {
  return DrawHexScalar(label, value, ImGuiDataType_U32, "%08X", opts);
}

}  // namespace gui
}  // namespace yaze
