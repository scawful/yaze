#include "app/gui/widget_auto_register.h"

#include "imgui/imgui_internal.h"
#include "absl/strings/string_view.h"

namespace yaze {
namespace gui {

// Thread-local storage for the current auto-registration scope
thread_local std::vector<std::string> g_auto_scope_stack_;

AutoWidgetScope::AutoWidgetScope(const std::string& name)
    : scope_(name), name_(name) {
  g_auto_scope_stack_.push_back(name);
}

void AutoRegisterLastItem(const std::string& widget_type,
                          const std::string& explicit_label,
                          const std::string& description) {
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (!ctx || !ctx->CurrentWindow) {
    return;
  }

  // Get the last item's ID
  ImGuiID imgui_id = ctx->LastItemData.ID;
  if (imgui_id == 0) {
    return;  // No valid item to register
  }

  // Extract label
  std::string label = explicit_label;
  if (label.empty()) {
    // Try to get label from ImGui
    const char* imgui_label = ImGui::GetItemLabel();
    if (imgui_label && imgui_label[0] != '\0') {
      label = imgui_label;
    } else {
      // Fallback to widget type + ID
      label = absl::StrCat(widget_type, "_", imgui_id);
    }
  }

  // Build full hierarchical path
  std::string full_path;
  if (!g_auto_scope_stack_.empty()) {
    full_path = absl::StrJoin(g_auto_scope_stack_, "/");
    full_path += "/";
  }
  
  // Add widget type and normalized label
  std::string normalized_label = WidgetIdRegistry::NormalizeLabel(label);
  full_path += absl::StrCat(widget_type, ":", normalized_label);

  // Capture metadata from ImGui's last item
  WidgetIdRegistry::WidgetMetadata metadata;
  metadata.label = label;
  
  // Get window name
  if (ctx->CurrentWindow) {
    metadata.window_name = std::string(ctx->CurrentWindow->Name);
  }

  // Capture visibility and enabled state
  const ImGuiLastItemData& last = ctx->LastItemData;
  metadata.visible = (last.StatusFlags & ImGuiItemStatusFlags_Visible) != 0;
  metadata.enabled = (last.ItemFlags & ImGuiItemFlags_Disabled) == 0;

  // Capture bounding rectangle
  WidgetIdRegistry::WidgetBounds bounds;
  bounds.min_x = last.Rect.Min.x;
  bounds.min_y = last.Rect.Min.y;
  bounds.max_x = last.Rect.Max.x;
  bounds.max_y = last.Rect.Max.y;
  bounds.valid = true;
  metadata.bounds = bounds;

  // Register with the global registry
  WidgetIdRegistry::Instance().RegisterWidget(
      full_path, widget_type, imgui_id, description, metadata);
}

}  // namespace gui
}  // namespace yaze

