#include "app/editor/menu/status_bar.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "app/editor/events/core_events.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "rom/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

float StatusBar::GetHeight() const {
  if (!enabled_) {
    return 0.0f;
  }
  if (gui::LayoutHelpers::IsTouchDevice()) {
    return std::max(kStatusBarHeight, kStatusBarTouchHeight);
  }
  return kStatusBarHeight;
}

void StatusBar::Initialize(GlobalEditorContext* context) {
  context_ = context;
  if (context_) {
    // Subscribe to UI status updates
    context_->GetEventBus().Subscribe<StatusUpdateEvent>(
        [this](const StatusUpdateEvent& e) { HandleStatusUpdate(e); });

    // Subscribe to selection changes from any editor
    context_->GetEventBus().Subscribe<SelectionChangedEvent>(
        [this](const SelectionChangedEvent& e) {
          if (e.IsEmpty()) {
            ClearSelection();
          } else {
            SetSelection(static_cast<int>(e.Count()));
          }
        });

    // Subscribe to zoom changes from any canvas
    context_->GetEventBus().Subscribe<ZoomChangedEvent>(
        [this](const ZoomChangedEvent& e) { SetZoom(e.new_zoom); });
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

void StatusBar::SetAgentInfo(const std::string& provider,
                             const std::string& model, bool active) {
  has_agent_ = true;
  agent_provider_ = provider;
  agent_model_ = model;
  agent_active_ = active;
}

void StatusBar::ClearAgentInfo() {
  has_agent_ = false;
  agent_provider_.clear();
  agent_model_.clear();
  agent_active_ = false;
}

void StatusBar::Draw() {
  if (!enabled_) {
    return;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();

  // Position at very bottom of viewport, above the safe area (home indicator).
  // The dockspace is already reduced by GetBottomLayoutOffset() in controller.cc.
  const float bar_height = GetHeight();
  const float bottom_safe = gui::LayoutHelpers::GetSafeAreaInsets().bottom;
  const float bar_y =
      viewport->WorkPos.y + viewport->WorkSize.y - bar_height - bottom_safe;
  const bool touch_mode = gui::LayoutHelpers::IsTouchDevice();
  const ImVec2 panel_padding = touch_mode ? ImVec2(14.0f, 7.0f)
                                          : ImVec2(8.0f, 4.0f);
  const ImVec2 panel_spacing = touch_mode ? ImVec2(12.0f, 0.0f)
                                          : ImVec2(8.0f, 0.0f);

  // Status bar background - slightly elevated surface
  ImVec4 bar_bg = gui::GetSurfaceContainerVec4();
  ImVec4 bar_border = gui::GetOutlineVec4();

  ImGuiWindowFlags extra_flags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_NoBringToFrontOnFocus;

  gui::FixedPanel bar(
      "##StatusBar",
      ImVec2(viewport->WorkPos.x, bar_y),
      ImVec2(viewport->WorkSize.x, bar_height),
      {.bg = bar_bg,
       .border = bar_border,
       .padding = panel_padding,
       .spacing = panel_spacing,
       .border_size = 1.0f},
      extra_flags);

  if (bar) {
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
    std::string agent_label;
    if (has_agent_) {
      agent_label = agent_model_.empty() ? agent_provider_ : agent_model_;
      if (agent_label.empty()) {
        agent_label = "Agent";
      }
      const size_t max_len = 20;
      if (agent_label.size() > max_len) {
        agent_label = agent_label.substr(0, max_len - 3) + "...";
      }
      std::string label = std::string(ICON_MD_SMART_TOY " ") + agent_label;
      right_section_width +=
          ImGui::CalcTextSize(label.c_str()).x +
          ImGui::GetStyle().FramePadding.x * 2.0f + 10.0f;
    }
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

        if (has_agent_) {
          DrawAgentSegment();
          if (has_zoom_ || has_mode_) {
            DrawSeparator();
          }
        }

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
}

void StatusBar::DrawAgentSegment() {
  std::string label = agent_model_.empty() ? agent_provider_ : agent_model_;
  if (label.empty()) {
    label = "Agent";
  }
  const size_t max_len = 20;
  if (label.size() > max_len) {
    label = label.substr(0, max_len - 3) + "...";
  }
  std::string button_label = std::string(ICON_MD_SMART_TOY " ") + label;

  ImVec4 text_color = agent_active_ ? gui::GetPrimaryVec4()
                                    : gui::GetTextSecondaryVec4();
  gui::StyleColorGuard agent_colors({
      {ImGuiCol_Text, text_color},
      {ImGuiCol_Button, gui::GetSurfaceContainerHighVec4()},
      {ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighestVec4()},
  });
  if (gui::ThemedButton(button_label.c_str())) {
    if (agent_toggle_callback_) {
      agent_toggle_callback_();
    }
  }

  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("%s Agent", ICON_MD_SMART_TOY);
    ImGui::TextDisabled("Provider: %s",
                        agent_provider_.empty() ? "mock"
                                                : agent_provider_.c_str());
    ImGui::TextDisabled("Model: %s",
                        agent_model_.empty() ? "not set"
                                             : agent_model_.c_str());
    ImGui::TextDisabled("Toggle chat panel");
    ImGui::EndTooltip();
  }
}

void StatusBar::DrawRomSegment() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  if (rom_ && rom_->is_loaded()) {
    // ROM name
    gui::ColoredTextF(ImGui::GetStyleColorVec4(ImGuiCol_Text),
                      "%s %s", ICON_MD_DESCRIPTION,
                      rom_->short_name().c_str());

    // Dirty indicator
    if (rom_->dirty()) {
      ImGui::SameLine();
      gui::ColoredText(ICON_MD_FIBER_MANUAL_RECORD,
                       gui::ConvertColorToImVec4(theme.warning));

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Unsaved changes");
      }
    }
  } else {
    gui::ColoredTextF(gui::GetTextSecondaryVec4(),
                      "%s No ROM loaded", ICON_MD_DESCRIPTION);
  }
}

void StatusBar::DrawSessionSegment() {
  gui::ColoredTextF(gui::GetTextSecondaryVec4(),
                    "%s S%zu/%zu", ICON_MD_LAYERS,
                    session_id_ + 1, total_sessions_);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Session %zu of %zu", session_id_ + 1, total_sessions_);
  }
}

void StatusBar::DrawCursorSegment() {
  gui::ColoredTextF(gui::GetTextSecondaryVec4(),
                    "%s: %d, %d", cursor_label_.c_str(), cursor_x_, cursor_y_);
}

void StatusBar::DrawSelectionSegment() {
  gui::StyleColorGuard selection_color(ImGuiCol_Text,
                                       gui::GetTextSecondaryVec4());

  if (selection_width_ > 0 && selection_height_ > 0) {
    ImGui::Text("%s %d (%dx%d)", ICON_MD_SELECT_ALL, selection_count_,
                selection_width_, selection_height_);
  } else if (selection_count_ > 0) {
    ImGui::Text("%s %d selected", ICON_MD_SELECT_ALL, selection_count_);
  }
}

void StatusBar::DrawZoomSegment() {
  int zoom_percent = static_cast<int>(zoom_level_ * 100.0f);
  gui::ColoredTextF(gui::GetTextSecondaryVec4(),
                    "%s %d%%", ICON_MD_ZOOM_IN, zoom_percent);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Zoom: %d%%", zoom_percent);
  }
}

void StatusBar::DrawModeSegment() {
  gui::ColoredTextF(gui::GetTextSecondaryVec4(), "%s", editor_mode_.c_str());
}

void StatusBar::DrawCustomSegments() {
  for (const auto& [key, value] : custom_segments_) {
    DrawSeparator();
    gui::ColoredTextF(gui::GetTextSecondaryVec4(),
                      "%s: %s", key.c_str(), value.c_str());
  }
}

void StatusBar::DrawSeparator() {
  ImGui::SameLine();
  gui::ColoredText("|", gui::GetOutlineVec4());
  ImGui::SameLine();
}

}  // namespace editor
}  // namespace yaze
