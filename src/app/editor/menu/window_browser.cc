#include "app/editor/menu/window_browser.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/system/workspace_window_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace {

std::string LowercaseCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(::tolower(c)); });
  return value;
}

bool MatchesWindowSearch(const std::string& query,
                         const WindowDescriptor& window) {
  if (query.empty()) {
    return true;
  }

  const std::string search_str = LowercaseCopy(query);
  return LowercaseCopy(window.display_name).find(search_str) !=
             std::string::npos ||
         LowercaseCopy(window.card_id).find(search_str) != std::string::npos ||
         LowercaseCopy(window.shortcut_hint).find(search_str) !=
             std::string::npos;
}

}  // namespace

WindowBrowser::WindowBrowser(WorkspaceWindowManager& window_manager)
    : window_manager_(window_manager) {}

void WindowBrowser::Draw(size_t session_id, bool* p_open) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 max_window_size(1600.0f, 1000.0f);
  ImVec2 default_window_size(1080.0f, 700.0f);
  if (viewport) {
    max_window_size = ImVec2(std::max(760.0f, viewport->WorkSize.x * 0.95f),
                             std::max(520.0f, viewport->WorkSize.y * 0.95f));
    default_window_size = ImVec2(
        std::clamp(viewport->WorkSize.x * 0.72f, 880.0f, max_window_size.x),
        std::clamp(viewport->WorkSize.y * 0.76f, 600.0f, max_window_size.y));
    const ImVec2 center(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  }

  ImGui::SetNextWindowSize(default_window_size, ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(ImVec2(780, 520), max_window_size);

  if (ImGui::Begin(
          absl::StrFormat("%s Window Browser", ICON_MD_DASHBOARD).c_str(),
          p_open, ImGuiWindowFlags_NoDocking)) {
    std::vector<std::string> categories =
        window_manager_.GetAllWindowCategories(session_id);
    std::sort(categories.begin(), categories.end());
    if (category_filter_ != "All" &&
        std::find(categories.begin(), categories.end(), category_filter_) ==
            categories.end()) {
      category_filter_ = "All";
    }

    const auto all_windows = window_manager_.GetWindowsInSession(session_id);
    auto count_windows = [&](const std::string& category,
                             bool visible_only) -> int {
      int count = 0;
      for (const auto& window_id : all_windows) {
        const auto* window =
            window_manager_.GetWindowDescriptor(session_id, window_id);
        if (!window) {
          continue;
        }
        if (category != "All" && window->category != category) {
          continue;
        }
        const bool visible =
            window->visibility_flag && *window->visibility_flag;
        if (!visible_only || visible) {
          ++count;
        }
      }
      return count;
    };

    ImGui::SetNextItemWidth(
        std::max(280.0f, ImGui::GetContentRegionAvail().x * 0.45f));
    ImGui::InputTextWithHint(
        "##Search",
        absl::StrFormat("%s Search windows...", ICON_MD_SEARCH).c_str(),
        search_filter_, sizeof(search_filter_));
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CLEAR " Clear")) {
      search_filter_[0] = '\0';
    }
    ImGui::SameLine();
    if (category_filter_ == "All") {
      if (ImGui::Button(ICON_MD_VISIBILITY " Show All")) {
        window_manager_.ShowAllWindowsInSession(session_id);
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_VISIBILITY_OFF " Hide All")) {
        window_manager_.HideAllWindowsInSession(session_id);
      }
    } else {
      if (ImGui::Button(ICON_MD_VISIBILITY " Show Category")) {
        window_manager_.ShowAllWindowsInCategory(session_id, category_filter_);
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_VISIBILITY_OFF " Hide Category")) {
        window_manager_.HideAllWindowsInCategory(session_id, category_filter_);
      }
    }

    ImGui::Separator();

    const float content_height = ImGui::GetContentRegionAvail().y;
    const float max_sidebar_width =
        std::max(220.0f, ImGui::GetContentRegionAvail().x * 0.50f);
    float category_sidebar_width =
        std::clamp(window_manager_.GetWindowBrowserCategoryWidth(), 220.0f,
                   max_sidebar_width);

    if (ImGui::BeginChild("##WindowBrowserCategories",
                          ImVec2(category_sidebar_width, content_height),
                          true)) {
      const int visible_total = count_windows("All", true);
      std::string all_label =
          absl::StrFormat("%s All Windows (%d/%d)", ICON_MD_DASHBOARD,
                          visible_total, static_cast<int>(all_windows.size()));
      if (ImGui::Selectable(all_label.c_str(), category_filter_ == "All")) {
        category_filter_ = "All";
      }
      ImGui::Separator();

      for (const auto& category : categories) {
        const int category_total = count_windows(category, false);
        if (category_total <= 0) {
          continue;
        }
        const int visible_in_category = count_windows(category, true);
        const std::string icon = WorkspaceWindowManager::GetCategoryIcon(category);
        std::string label = absl::StrFormat(
            "%s %s (%d/%d)", icon.c_str(), category.c_str(),
            visible_in_category, category_total);
        if (ImGui::Selectable(label.c_str(), category_filter_ == category)) {
          category_filter_ = category;
        }
      }
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);

    const float splitter_width = 6.0f;
    const ImVec2 splitter_pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##WindowBrowserSplitter",
                           ImVec2(splitter_width, content_height));
    const bool splitter_hovered = ImGui::IsItemHovered();
    const bool splitter_active = ImGui::IsItemActive();
    if (splitter_hovered || splitter_active) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (splitter_hovered &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      window_manager_.SetWindowBrowserCategoryWidth(
          WorkspaceWindowManager::GetDefaultWindowBrowserCategoryWidth());
      category_sidebar_width =
          std::clamp(window_manager_.GetWindowBrowserCategoryWidth(), 220.0f,
                     max_sidebar_width);
    }
    if (splitter_active) {
      category_sidebar_width =
          std::clamp(category_sidebar_width + ImGui::GetIO().MouseDelta.x,
                     220.0f, max_sidebar_width);
      window_manager_.SetWindowBrowserCategoryWidth(category_sidebar_width);
      ImGui::SetTooltip("Width: %.0f px", category_sidebar_width);
    }

    ImVec4 splitter_color = gui::GetOutlineVec4();
    splitter_color.w =
        splitter_active ? 0.95f : (splitter_hovered ? 0.72f : 0.35f);
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(splitter_pos.x + splitter_width * 0.5f, splitter_pos.y),
        ImVec2(splitter_pos.x + splitter_width * 0.5f,
               splitter_pos.y + content_height),
        ImGui::GetColorU32(splitter_color), splitter_active ? 2.0f : 1.0f);

    ImGui::SameLine(0.0f, 0.0f);

    if (ImGui::BeginChild("##WindowBrowserTable", ImVec2(0, content_height),
                          false)) {
      if (ImGui::BeginTable("##WindowTable", 5,
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Borders |
                                ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed,
                                60);
        ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 36);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed,
                                130);
        ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed,
                                110);
        ImGui::TableHeadersRow();

        auto windows = (category_filter_ == "All")
                           ? all_windows
                           : std::vector<std::string>{};
        if (category_filter_ != "All") {
          auto category_windows =
              window_manager_.GetWindowsInCategory(session_id, category_filter_);
          windows.reserve(category_windows.size());
          for (const auto& window : category_windows) {
            windows.push_back(window.card_id);
          }
        }

        for (const auto& window_id : windows) {
          const auto* window =
              window_manager_.GetWindowDescriptor(session_id, window_id);
          if (!window) {
            continue;
          }

          if (!MatchesWindowSearch(search_filter_, *window)) {
            continue;
          }

          ImGui::TableNextRow();

          ImGui::TableNextColumn();
          if (window->visibility_flag) {
            bool visible = *window->visibility_flag;
            if (ImGui::Checkbox(absl::StrFormat("##vis_%s", window->card_id)
                                    .c_str(),
                                &visible)) {
              window_manager_.ToggleWindow(session_id, window->card_id);
            }
          }

          ImGui::TableNextColumn();
          const bool is_pinned = window_manager_.IsWindowPinned(window->card_id);
          const ImVec4 pin_color =
              is_pinned ? gui::GetPrimaryVec4() : gui::GetTextDisabledVec4();
          const float pin_side =
              std::max(20.0f, gui::LayoutHelpers::GetStandardWidgetHeight());
          ImGui::PushID(
              absl::StrFormat("browser_pin_%s", window->card_id).c_str());
          if (gui::TransparentIconButton(
                  is_pinned ? ICON_MD_PUSH_PIN : ICON_MD_PIN,
                  ImVec2(pin_side, pin_side),
                  is_pinned ? "Unpin window" : "Pin window", is_pinned,
                  pin_color, "window_browser", window->card_id.c_str())) {
            window_manager_.SetWindowPinned(session_id, window->card_id,
                                           !is_pinned);
          }
          ImGui::PopID();

          ImGui::TableNextColumn();
          ImGui::Text("%s %s", window->icon.c_str(),
                      window->display_name.c_str());
          if (ImGui::IsItemHovered() &&
              ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            window_manager_.OpenWindow(session_id, window->card_id);
            const std::string window_name =
                window_manager_.GetWorkspaceWindowName(*window);
            if (!window_name.empty()) {
              ImGui::SetWindowFocus(window_name.c_str());
            }
          }

          ImGui::TableNextColumn();
          ImGui::TextUnformatted(window->category.c_str());

          ImGui::TableNextColumn();
          if (window->shortcut_hint.empty()) {
            ImGui::TextDisabled("-");
          } else {
            ImGui::TextDisabled("%s", window->shortcut_hint.c_str());
          }
        }

        ImGui::EndTable();
      }
    }
    ImGui::EndChild();
  }
  ImGui::End();
}

}  // namespace editor
}  // namespace yaze
