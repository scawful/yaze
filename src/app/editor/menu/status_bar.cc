#include "app/editor/menu/status_bar.h"

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "rom/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void StatusBar::Initialize(GlobalEditorContext* context) {
  context_ = context;
  if (context_) {
    context_->GetEventBus().Subscribe<StatusUpdateEvent>(
        [this](const StatusUpdateEvent& e) { HandleStatusUpdate(e); });
  }
}

void StatusBar::HandleStatusUpdate(const StatusUpdateEvent& event) {
  switch (event.type) {
    case StatusUpdateEvent::Type::Cursor:
      SetCursorPosition(event.x, event.y, event.text.empty() ? nullptr : event.text.c_str());
      break;
    case StatusUpdateEvent::Type::Selection:
      SetSelection(event.count, event.width, event.height);
      break;
    case StatusUpdateEvent::Type::Zoom:
      SetZoom(event.zoom);
      break;
    case StatusUpdateEvent::Type::Mode:
      SetEditorMode(event.text);
      break;
    case StatusUpdateEvent::Type::Clear:
      ClearAllContext();
      break;
    default:
      break;
  }
}

void StatusBar::SetSessionInfo(size_t session_id, size_t total_sessions) {
  session_id_ = session_id;
  total_sessions_ = total_sessions;
}

void StatusBar::SetCursorPosition(int x, int y, const char* label) {
  has_cursor_ = true;
  cursor_x_ = x;
  cursor_y_ = y;
  cursor_label_ = label ? label : "Pos";
}

void StatusBar::ClearCursorPosition() {
  has_cursor_ = false;
}

void StatusBar::SetSelection(int count, int width, int height) {
  has_selection_ = true;
  selection_count_ = count;
  selection_width_ = width;
  selection_height_ = height;
}

void StatusBar::ClearSelection() {
  has_selection_ = false;
}

void StatusBar::SetZoom(float level) {
  has_zoom_ = true;
  zoom_level_ = level;
}

void StatusBar::ClearZoom() {
  has_zoom_ = false;
}

void StatusBar::SetEditorMode(const std::string& mode) {
  has_mode_ = true;
  editor_mode_ = mode;
}

void StatusBar::ClearEditorMode() {
  has_mode_ = false;
  editor_mode_.clear();
}

void StatusBar::SetCustomSegment(const std::string& key,
                                  const std::string& value) {
  custom_segments_[key] = value;
}

void StatusBar::ClearCustomSegment(const std::string& key) {
  custom_segments_.erase(key);
}

void StatusBar::ClearAllContext() {
  ClearCursorPosition();
  ClearSelection();
  ClearZoom();
  ClearEditorMode();
  custom_segments_.clear();
}

void StatusBar::Draw() {
  if (!enabled_) {
    return;
  }

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();

  // Position at very bottom of viewport, outside the dockspace
  // The dockspace is already reduced by GetBottomLayoutOffset() in controller.cc
  const float bar_height = kStatusBarHeight;
  const float bar_y = viewport->WorkPos.y + viewport->WorkSize.y - bar_height;

  // Use full viewport width (status bar spans under sidebars for visual continuity)
  ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, bar_y));
  ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, bar_height));

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;

  // Status bar background - slightly elevated surface
  ImVec4 bar_bg = gui::GetSurfaceContainerVec4();
  ImVec4 bar_border = gui::GetOutlineVec4();

  ImGui::PushStyleColor(ImGuiCol_WindowBg, bar_bg);
  ImGui::PushStyleColor(ImGuiCol_Border, bar_border);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));

  if (ImGui::Begin("##StatusBar", nullptr, flags)) {
    // Left section: ROM info, Session, Dirty status
    DrawRomSegment();

    if (total_sessions_ > 1) {
      DrawSeparator();
      DrawSessionSegment();
    }

    // Middle section: Editor context (cursor, selection)
    if (has_cursor_) {
      DrawSeparator();
      DrawCursorSegment();
    }

    if (has_selection_) {
      DrawSeparator();
      DrawSelectionSegment();
    }

    // Custom segments
    DrawCustomSegments();

    // Right section: Zoom, Mode (right-aligned)
    float right_section_width = 0.0f;
    if (has_zoom_) {
      right_section_width += ImGui::CalcTextSize("100%").x + 20.0f;
    }
    if (has_mode_) {
      right_section_width += ImGui::CalcTextSize(editor_mode_.c_str()).x + 30.0f;
    }

    if (right_section_width > 0.0f) {
      float available = ImGui::GetContentRegionAvail().x;
      if (available > right_section_width + 20.0f) {
        ImGui::SameLine(ImGui::GetWindowWidth() - right_section_width - 16.0f);

        if (has_zoom_) {
          DrawZoomSegment();
          if (has_mode_) {
            DrawSeparator();
          }
        }

        if (has_mode_) {
          DrawModeSegment();
        }
      }
    }
  }
  ImGui::End();

  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(2);
}

void StatusBar::DrawRomSegment() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  if (rom_ && rom_->is_loaded()) {
    // ROM name
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
    ImGui::Text("%s %s", ICON_MD_DESCRIPTION, rom_->short_name().c_str());
    ImGui::PopStyleColor();

    // Dirty indicator
    if (rom_->dirty()) {
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Text, gui::ConvertColorToImVec4(theme.warning));
      ImGui::Text(ICON_MD_FIBER_MANUAL_RECORD);
      ImGui::PopStyleColor();

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Unsaved changes");
      }
    }
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
    ImGui::Text("%s No ROM loaded", ICON_MD_DESCRIPTION);
    ImGui::PopStyleColor();
  }
}

void StatusBar::DrawSessionSegment() {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
  ImGui::Text("%s S%zu/%zu", ICON_MD_LAYERS, session_id_ + 1, total_sessions_);
  ImGui::PopStyleColor();

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Session %zu of %zu", session_id_ + 1, total_sessions_);
  }
}

void StatusBar::DrawCursorSegment() {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
  ImGui::Text("%s: %d, %d", cursor_label_.c_str(), cursor_x_, cursor_y_);
  ImGui::PopStyleColor();
}

void StatusBar::DrawSelectionSegment() {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());

  if (selection_width_ > 0 && selection_height_ > 0) {
    ImGui::Text("%s %d (%dx%d)", ICON_MD_SELECT_ALL, selection_count_,
                selection_width_, selection_height_);
  } else if (selection_count_ > 0) {
    ImGui::Text("%s %d selected", ICON_MD_SELECT_ALL, selection_count_);
  }

  ImGui::PopStyleColor();
}

void StatusBar::DrawZoomSegment() {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());

  int zoom_percent = static_cast<int>(zoom_level_ * 100.0f);
  ImGui::Text("%s %d%%", ICON_MD_ZOOM_IN, zoom_percent);

  ImGui::PopStyleColor();

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Zoom: %d%%", zoom_percent);
  }
}

void StatusBar::DrawModeSegment() {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
  ImGui::Text("%s", editor_mode_.c_str());
  ImGui::PopStyleColor();
}

void StatusBar::DrawCustomSegments() {
  for (const auto& [key, value] : custom_segments_) {
    DrawSeparator();
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
    ImGui::Text("%s: %s", key.c_str(), value.c_str());
    ImGui::PopStyleColor();
  }
}

void StatusBar::DrawSeparator() {
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetOutlineVec4());
  ImGui::Text("|");
  ImGui::PopStyleColor();
  ImGui::SameLine();
}

}  // namespace editor
}  // namespace yaze

