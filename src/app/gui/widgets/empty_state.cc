#include "app/gui/widgets/empty_state.h"

#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

namespace {

void CenterNextItem(float width) {
  const float avail = ImGui::GetContentRegionAvail().x;
  if (avail > width) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
  }
}

}  // namespace

bool DrawEmptyState(const EmptyStateOptions& options) {
  if (!options.title && !options.detail && !options.icon) {
    return false;
  }

  const float top_pad = options.compact ? 8.0f : 24.0f;
  const float mid_pad = options.compact ? 6.0f : 12.0f;
  ImGui::Dummy(ImVec2(0.0f, top_pad));

  if (options.icon && options.icon[0] != '\0') {
    const ImVec2 icon_size = ImGui::CalcTextSize(options.icon);
    CenterNextItem(icon_size.x);
    ColoredText(options.icon, GetDisabledColor());
  }

  if (options.title && options.title[0] != '\0') {
    ImGui::Spacing();
    const ImVec2 title_size = ImGui::CalcTextSize(options.title);
    CenterNextItem(title_size.x);
    ColoredText(options.title,
                ResolveSemanticColor(SemanticColor::OnSurfaceVariant));
  }

  if (options.detail && options.detail[0] != '\0') {
    ImGui::Dummy(ImVec2(0.0f, mid_pad * 0.5f));
    const float wrap_width =
        ImGui::GetContentRegionAvail().x * (options.compact ? 0.95f : 0.85f);
    const float indent = (ImGui::GetContentRegionAvail().x - wrap_width) * 0.5f;
    if (indent > 0.0f) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_width);
    ImGui::TextDisabled("%s", options.detail);
    ImGui::PopTextWrapPos();
  }

  bool clicked = false;
  if (options.action_label && options.action_label[0] != '\0') {
    ImGui::Dummy(ImVec2(0.0f, mid_pad));
    const ImVec2 label_size = ImGui::CalcTextSize(options.action_label);
    const float button_width =
        label_size.x + ImGui::GetStyle().FramePadding.x * 4.0f;
    CenterNextItem(button_width);
    if (PrimaryButton(options.action_label, ImVec2(button_width, 0.0f))) {
      clicked = true;
      if (options.on_action) {
        options.on_action();
      }
    }
  }

  ImGui::Dummy(ImVec2(0.0f, top_pad * 0.5f));
  return clicked;
}

EmptyStateOptions EmptyNoRom(bool compact) {
  EmptyStateOptions opts;
  opts.icon = ICON_MD_FOLDER_OPEN;
  opts.title = "Open a ROM";
  opts.detail =
      "Use File > Open ROM / Project, or drop a .sfc / .smc file onto the "
      "window.";
  opts.compact = compact;
  return opts;
}

EmptyStateOptions EmptyNoSelection(bool compact) {
  EmptyStateOptions opts;
  opts.icon = ICON_MD_TOUCH_APP;
  opts.title = "Select something";
  opts.detail =
      "Click an object in the active editor to view and edit its properties. "
      "Dungeon: rooms, objects, sprites. Overworld: maps, tiles, entities. "
      "Graphics: sheets, palettes.";
  opts.compact = compact;
  return opts;
}

EmptyStateOptions EmptySelectInCanvas(bool compact) {
  EmptyStateOptions opts;
  opts.icon = ICON_MD_MOUSE;
  opts.title = "Select in the canvas";
  opts.detail =
      "Click a room object, door, sprite, or item to inspect it here. "
      "Shift-click and drag to multi-select. Use placement panels to add new "
      "entities.";
  opts.compact = compact;
  return opts;
}

EmptyStateOptions EmptyNoProject(bool compact) {
  EmptyStateOptions opts;
  opts.icon = ICON_MD_FOLDER_SPECIAL;
  opts.title = "Open a project";
  opts.detail =
      "Create or open a .yaze project via File > New Project / Open to manage "
      "ROM versioning, snapshots, and project settings.";
  opts.compact = compact;
  return opts;
}

EmptyStateOptions EmptyLoading(const char* what, bool compact) {
  EmptyStateOptions opts;
  opts.icon = ICON_MD_HOURGLASS_EMPTY;
  opts.title = "Loading…";
  opts.detail = what;
  opts.compact = compact;
  return opts;
}

}  // namespace gui
}  // namespace yaze
