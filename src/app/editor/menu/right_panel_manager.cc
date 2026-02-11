#include "app/editor/menu/right_panel_manager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_chat.h"
#include "app/editor/system/shortcut_manager.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/ui/project_management_panel.h"
#include "app/editor/ui/selection_properties_panel.h"
#include "app/editor/ui/settings_panel.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/platform_keys.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "util/platform_paths.h"

namespace yaze {
namespace editor {

namespace {

std::string ResolveAgentChatHistoryPath() {
  auto agent_dir = util::PlatformPaths::GetAppDataSubdirectory("agent");
  if (agent_dir.ok()) {
    return (*agent_dir / "agent_chat_history.json").string();
  }
  auto temp_dir = util::PlatformPaths::GetTempDirectory();
  if (temp_dir.ok()) {
    return (*temp_dir / "agent_chat_history.json").string();
  }
  return (std::filesystem::current_path() / "agent_chat_history.json").string();
}

std::string BuildSelectionContextSummary(
    const SelectionContext& selection) {
  if (selection.type == SelectionType::kNone) {
    return "";
  }
  std::string context = absl::StrFormat(
      "Selection: %s", GetSelectionTypeName(selection.type));
  if (!selection.display_name.empty()) {
    context += absl::StrFormat("\nName: %s", selection.display_name);
  }
  if (selection.id >= 0) {
    context += absl::StrFormat("\nID: 0x%X", selection.id);
  }
  if (selection.secondary_id >= 0) {
    context += absl::StrFormat("\nSecondary: 0x%X", selection.secondary_id);
  }
  if (selection.read_only) {
    context += "\nRead Only: true";
  }
  return context;
}

}  // namespace

const char* GetPanelTypeName(RightPanelManager::PanelType type) {
  switch (type) {
    case RightPanelManager::PanelType::kNone:
      return "None";
    case RightPanelManager::PanelType::kAgentChat:
      return "AI Agent";
    case RightPanelManager::PanelType::kProposals:
      return "Proposals";
    case RightPanelManager::PanelType::kSettings:
      return "Settings";
    case RightPanelManager::PanelType::kHelp:
      return "Help";
    case RightPanelManager::PanelType::kNotifications:
      return "Notifications";
    case RightPanelManager::PanelType::kProperties:
      return "Properties";
    case RightPanelManager::PanelType::kProject:
      return "Project";
    default:
      return "Unknown";
  }
}

const char* GetPanelTypeIcon(RightPanelManager::PanelType type) {
  switch (type) {
    case RightPanelManager::PanelType::kNone:
      return "";
    case RightPanelManager::PanelType::kAgentChat:
      return ICON_MD_SMART_TOY;
    case RightPanelManager::PanelType::kProposals:
      return ICON_MD_DESCRIPTION;
    case RightPanelManager::PanelType::kSettings:
      return ICON_MD_SETTINGS;
    case RightPanelManager::PanelType::kHelp:
      return ICON_MD_HELP;
    case RightPanelManager::PanelType::kNotifications:
      return ICON_MD_NOTIFICATIONS;
    case RightPanelManager::PanelType::kProperties:
      return ICON_MD_LIST_ALT;
    case RightPanelManager::PanelType::kProject:
      return ICON_MD_FOLDER_SPECIAL;
    default:
      return ICON_MD_HELP;
  }
}

void RightPanelManager::TogglePanel(PanelType type) {
  if (active_panel_ == type) {
    ClosePanel();
  } else {
    // Opens the requested panel (also handles re-opening during close animation)
    OpenPanel(type);
  }
}

bool RightPanelManager::IsPanelExpanded() const {
  return active_panel_ != PanelType::kNone || closing_;
}

void RightPanelManager::OpenPanel(PanelType type) {
  // If we were closing, cancel the close animation
  closing_ = false;
  closing_panel_ = PanelType::kNone;

  active_panel_ = type;
  animating_ = true;
  animation_target_ = 1.0f;

  // Check if animations are enabled
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  if (!theme.enable_animations) {
    panel_animation_ = 1.0f;
    animating_ = false;
  }
  // Otherwise keep current panel_animation_ for smooth transition
}

void RightPanelManager::ClosePanel() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  if (!theme.enable_animations) {
    // Instant close
    active_panel_ = PanelType::kNone;
    closing_ = false;
    closing_panel_ = PanelType::kNone;
    panel_animation_ = 0.0f;
    animating_ = false;
    return;
  }

  // Start close animation — keep the panel type so we can still draw it
  closing_ = true;
  closing_panel_ = active_panel_;
  active_panel_ = PanelType::kNone;
  animating_ = true;
  animation_target_ = 0.0f;
}

float RightPanelManager::GetPanelWidth() const {
  // Determine which panel to measure: active panel, or the one being closed
  PanelType effective_panel = active_panel_;
  if (effective_panel == PanelType::kNone && closing_) {
    effective_panel = closing_panel_;
  }
  if (effective_panel == PanelType::kNone) {
    return 0.0f;
  }

  float width = 0.0f;
  switch (effective_panel) {
    case PanelType::kAgentChat:
      width = agent_chat_width_;
      break;
    case PanelType::kProposals:
      width = proposals_width_;
      break;
    case PanelType::kSettings:
      width = settings_width_;
      break;
    case PanelType::kHelp:
      width = help_width_;
      break;
    case PanelType::kNotifications:
      width = notifications_width_;
      break;
    case PanelType::kProperties:
      width = properties_width_;
      break;
    case PanelType::kProject:
      width = project_width_;
      break;
    default:
      width = 0.0f;
      break;
  }

  ImGuiContext* context = ImGui::GetCurrentContext();
  if (!context) {
    return width * panel_animation_;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (!viewport) {
    return width * panel_animation_;
  }

  const float max_width = viewport->WorkSize.x * gui::UIConfig::kMaxPanelWidthRatio;
  if (max_width > 0.0f && width > max_width) {
    width = max_width;
  }

  // Scale by animation progress for smooth docking space adjustment
  return width * panel_animation_;
}

void RightPanelManager::SetPanelWidth(PanelType type, float width) {
  switch (type) {
    case PanelType::kAgentChat:
      agent_chat_width_ = width;
      break;
    case PanelType::kProposals:
      proposals_width_ = width;
      break;
    case PanelType::kSettings:
      settings_width_ = width;
      break;
    case PanelType::kHelp:
      help_width_ = width;
      break;
    case PanelType::kNotifications:
      notifications_width_ = width;
      break;
    case PanelType::kProperties:
      properties_width_ = width;
      break;
    case PanelType::kProject:
      project_width_ = width;
      break;
    default:
      break;
  }
}

void RightPanelManager::ResetPanelWidths() {
  agent_chat_width_ = GetDefaultPanelWidth(PanelType::kAgentChat, active_editor_type_);
  proposals_width_ = GetDefaultPanelWidth(PanelType::kProposals, active_editor_type_);
  settings_width_ = GetDefaultPanelWidth(PanelType::kSettings, active_editor_type_);
  help_width_ = GetDefaultPanelWidth(PanelType::kHelp, active_editor_type_);
  notifications_width_ = GetDefaultPanelWidth(PanelType::kNotifications, active_editor_type_);
  properties_width_ = GetDefaultPanelWidth(PanelType::kProperties, active_editor_type_);
  project_width_ = GetDefaultPanelWidth(PanelType::kProject, active_editor_type_);
}

float RightPanelManager::GetDefaultPanelWidth(PanelType type, EditorType editor) {
  switch (type) {
    case PanelType::kAgentChat:
      return gui::UIConfig::kPanelWidthAgentChat;
    case PanelType::kProposals:
      return gui::UIConfig::kPanelWidthProposals;
    case PanelType::kSettings:
      return gui::UIConfig::kPanelWidthSettings;
    case PanelType::kHelp:
      return gui::UIConfig::kPanelWidthHelp;
    case PanelType::kNotifications:
      return gui::UIConfig::kPanelWidthNotifications;
    case PanelType::kProperties:
      // Property panel can be wider in certain editors
      if (editor == EditorType::kDungeon) {
        return 380.0f; // More space for complex object properties
      }
      return gui::UIConfig::kPanelWidthProperties;
    case PanelType::kProject:
      return gui::UIConfig::kPanelWidthProject;
    default:
      return gui::UIConfig::kPanelWidthMedium;
  }
}

void RightPanelManager::Draw() {
  // Nothing to draw if no panel is active and no close animation running
  if (active_panel_ == PanelType::kNone && !closing_) {
    return;
  }

  // Handle Escape key to close panel
  if (active_panel_ != PanelType::kNone &&
      ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    ClosePanel();
    // Don't return — we need to start drawing the close animation this frame
    if (!closing_) return;
  }

  // Advance animation
  if (animating_) {
    float delta_time = ImGui::GetIO().DeltaTime;
    float speed = gui::UIConfig::kAnimationSpeed;
    float diff = animation_target_ - panel_animation_;
    panel_animation_ += diff * std::min(1.0f, delta_time * speed);

    // Snap to target when close enough
    if (std::abs(animation_target_ - panel_animation_) <
        gui::UIConfig::kAnimationSnapThreshold) {
      panel_animation_ = animation_target_;
      animating_ = false;

      // Close animation finished — fully clean up
      if (closing_ && animation_target_ == 0.0f) {
        closing_ = false;
        closing_panel_ = PanelType::kNone;
        return;
      }
    }
  }

  // Determine which panel type to draw content for
  PanelType draw_panel = active_panel_;
  if (draw_panel == PanelType::kNone && closing_) {
    draw_panel = closing_panel_;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float viewport_width = viewport->WorkSize.x;
  const float top_inset = gui::LayoutHelpers::GetTopInset();
  const float viewport_height =
      std::max(0.0f, viewport->WorkSize.y - top_inset);

  // GetPanelWidth() already factors in panel_animation_ for docking space.
  // For the window itself, use the full (unanimated) width and slide position.
  const float animated_width = GetPanelWidth();
  // Avoid division by zero
  const float full_width =
      (panel_animation_ > 0.001f) ? animated_width / panel_animation_ : 0.0f;

  // Use SurfaceContainer for slightly elevated panel background
  ImVec4 panel_bg = gui::GetSurfaceContainerVec4();
  ImVec4 panel_border = gui::GetOutlineVec4();

  ImGuiWindowFlags panel_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoNavFocus;

  // Position panel: slides from right edge. At animation=1.0, fully visible.
  // At animation=0.0, fully off-screen to the right.
  float panel_x = viewport->WorkPos.x + viewport_width - animated_width;
  ImGui::SetNextWindowPos(
      ImVec2(panel_x, viewport->WorkPos.y + top_inset));
  ImGui::SetNextWindowSize(ImVec2(full_width, viewport_height));

  gui::StyledWindow panel(
      "##RightPanel",
      {.bg = panel_bg,
       .border = panel_border,
       .padding = ImVec2(0.0f, 0.0f),
       .border_size = 1.0f},
      nullptr, panel_flags);
  if (panel) {
    // Draw enhanced panel header
    DrawPanelHeader(GetPanelTypeName(draw_panel),
                    GetPanelTypeIcon(draw_panel));

    // Content area with padding
    gui::StyleVarGuard content_padding(ImGuiStyleVar_WindowPadding,
                                       ImVec2(12.0f, 8.0f));
    ImGui::BeginChild("##PanelContent", ImVec2(0, 0), false,
                      ImGuiWindowFlags_AlwaysUseWindowPadding);

    // Draw panel content based on type
    switch (draw_panel) {
      case PanelType::kAgentChat:
        DrawAgentChatPanel();
        break;
      case PanelType::kProposals:
        DrawProposalsPanel();
        break;
      case PanelType::kSettings:
        DrawSettingsPanel();
        break;
      case PanelType::kHelp:
        DrawHelpPanel();
        break;
      case PanelType::kNotifications:
        DrawNotificationsPanel();
        break;
      case PanelType::kProperties:
        DrawPropertiesPanel();
        break;
      case PanelType::kProject:
        DrawProjectPanel();
        break;
      default:
        break;
    }

    ImGui::EndChild();
  }
}

void RightPanelManager::DrawPanelHeader(const char* title, const char* icon) {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const float header_height = gui::UIConfig::kPanelHeaderHeight;
  const float padding = 12.0f;

  // Header background - slightly elevated surface
  ImVec2 header_min = ImGui::GetCursorScreenPos();
  ImVec2 header_max = ImVec2(header_min.x + ImGui::GetWindowWidth(),
                             header_min.y + header_height);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(
      header_min, header_max,
      ImGui::GetColorU32(gui::GetSurfaceContainerHighVec4()));

  // Draw subtle bottom border
  draw_list->AddLine(ImVec2(header_min.x, header_max.y),
                     ImVec2(header_max.x, header_max.y),
                     ImGui::GetColorU32(gui::GetOutlineVec4()), 1.0f);

  // Position content within header
  ImGui::SetCursorPosX(padding);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                       (header_height - ImGui::GetTextLineHeight()) * 0.5f);

  // Panel icon with primary color
  gui::ColoredText(icon, gui::GetPrimaryVec4());

  ImGui::SameLine();

  // Panel title (use current style text color)
  gui::ColoredText(title, ImGui::GetStyleColorVec4(ImGuiCol_Text));

  // Right-aligned buttons
  const float button_size = gui::LayoutHelpers::GetStandardWidgetHeight();
  float current_x = ImGui::GetWindowWidth() - button_size - padding;

  // Close button
  ImGui::SameLine(current_x);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);  // Center vertically

  gui::StyleColorGuard button_colors({
      {ImGuiCol_Button, ImVec4(0, 0, 0, 0)},
      {ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighestVec4()},
      {ImGuiCol_ButtonActive,
       ImVec4(gui::GetPrimaryVec4().x * 0.3f, gui::GetPrimaryVec4().y * 0.3f,
              gui::GetPrimaryVec4().z * 0.3f, 0.4f)},
      {ImGuiCol_Text, gui::GetTextSecondaryVec4()},
  });
  gui::StyleVarGuard button_rounding(ImGuiStyleVar_FrameRounding, 4.0f);

  if (ImGui::Button(ICON_MD_CLOSE, ImVec2(button_size, button_size))) {
    ClosePanel();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Close Panel (Esc)");
  }

  // Lock Toggle (Only for Properties Panel)
  if (active_panel_ == PanelType::kProperties) {
    current_x -= (button_size + 4.0f);
    ImGui::SameLine(current_x);

    ImVec4 lock_color = properties_locked_ ? gui::GetPrimaryVec4()
                                           : gui::GetTextSecondaryVec4();
    gui::StyleColorGuard lock_text(ImGuiCol_Text, lock_color);

    if (ImGui::Button(
            properties_locked_ ? ICON_MD_LOCK : ICON_MD_LOCK_OPEN,
            ImVec2(button_size, button_size))) {
      properties_locked_ = !properties_locked_;
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(properties_locked_ ? "Unlock Selection"
                                           : "Lock Selection");
    }
  }

  // Move cursor past the header
  ImGui::SetCursorPosY(header_height + 8.0f);
}

// =============================================================================
// Panel Styling Helpers
// =============================================================================

bool RightPanelManager::BeginPanelSection(const char* label, const char* icon,
                                          bool default_open) {
  gui::StyleColorGuard section_colors({
      {ImGuiCol_Header, gui::GetSurfaceContainerHighVec4()},
      {ImGuiCol_HeaderHovered, gui::GetSurfaceContainerHighestVec4()},
      {ImGuiCol_HeaderActive, gui::GetSurfaceContainerHighestVec4()},
  });
  gui::StyleVarGuard section_vars({
      {ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f)},
      {ImGuiStyleVar_FrameRounding, 4.0f},
  });

  // Build header text with icon if provided
  std::string header_text;
  if (icon) {
    header_text = std::string(icon) + "  " + label;
  } else {
    header_text = label;
  }

  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth |
      ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;
  if (default_open) {
    flags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  bool is_open = ImGui::TreeNodeEx(header_text.c_str(), flags);

  if (is_open) {
    ImGui::Spacing();
    ImGui::Indent(4.0f);
  }

  return is_open;
}

void RightPanelManager::EndPanelSection() {
  ImGui::Unindent(4.0f);
  ImGui::TreePop();
  ImGui::Spacing();
}

void RightPanelManager::DrawPanelDivider() {
  ImGui::Spacing();
  {
    gui::StyleColorGuard sep_color(ImGuiCol_Separator, gui::GetOutlineVec4());
    ImGui::Separator();
  }
  ImGui::Spacing();
}

void RightPanelManager::DrawPanelLabel(const char* label) {
  gui::ColoredText(label, gui::GetTextSecondaryVec4());
}

void RightPanelManager::DrawPanelValue(const char* label, const char* value) {
  gui::ColoredTextF(gui::GetTextSecondaryVec4(), "%s:", label);
  ImGui::SameLine();
  ImGui::TextUnformatted(value);
}

void RightPanelManager::DrawPanelDescription(const char* text) {
  gui::StyleColorGuard desc_color(ImGuiCol_Text, gui::GetTextDisabledVec4());
  ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
  ImGui::TextWrapped("%s", text);
  ImGui::PopTextWrapPos();
}

std::string RightPanelManager::GetShortcutLabel(
    const std::string& action, const std::string& fallback) const {
  if (!shortcut_manager_) {
    return fallback;
  }

  const Shortcut* shortcut = shortcut_manager_->FindShortcut(action);
  if (!shortcut) {
    return fallback;
  }
  if (shortcut->keys.empty()) {
    return "Unassigned";
  }

  return PrintShortcut(shortcut->keys);
}

void RightPanelManager::DrawShortcutRow(const std::string& action,
                                        const char* description,
                                        const std::string& fallback) {
  std::string label = GetShortcutLabel(action, fallback);
  DrawPanelValue(label.c_str(), description);
}

// =============================================================================
// Panel Content Drawing
// =============================================================================

void RightPanelManager::DrawAgentChatPanel() {
#ifdef YAZE_BUILD_AGENT_UI
  const ImVec4 header_bg = gui::GetSurfaceContainerHighVec4();
  const ImVec4 hero_text = gui::GetOnSurfaceVec4();
  const ImVec4 accent = gui::GetPrimaryVec4();

  if (!agent_chat_) {
    gui::ColoredText(ICON_MD_SMART_TOY " AI Agent Not Available",
                     gui::GetTextSecondaryVec4());
    ImGui::Spacing();
    DrawPanelDescription(
        "The AI Agent is not initialized. "
        "Open the AI Agent from View menu or use Ctrl+Shift+A.");
    return;
  }

  bool chat_active = *agent_chat_->active();

  {
    gui::StyledChild hero_child("AgentHero", ImVec2(0, 110),
                                {.bg = header_bg, .rounding = 8.0f}, true);
    if (hero_child) {
      // TextColored sets its own color, no Push/Pop needed
      ImGui::TextColored(accent, "%s AI Agent", ICON_MD_SMART_TOY);
      ImGui::SameLine();
      gui::ColoredText("Right Sidebar", gui::GetTextSecondaryVec4());

      ImGui::Spacing();
      DrawPanelValue("Status", chat_active ? "Active" : "Inactive");
      DrawPanelValue("Provider", "Configured via Agent Editor");
    }
  }

  ImGui::Spacing();
  agent_chat_->set_active(true);

  const float footer_height = ImGui::GetFrameHeightWithSpacing() * 3.5f;
  float content_height =
      std::max(120.0f, ImGui::GetContentRegionAvail().y - footer_height);

  static int active_tab = 0;  // 0 = Chat, 1 = Quick Config
  if (ImGui::BeginTabBar("AgentSidebarTabs")) {
    if (ImGui::BeginTabItem(ICON_MD_CHAT " Chat")) {
      active_tab = 0;
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(ICON_MD_SETTINGS " Quick Config")) {
      active_tab = 1;
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  if (active_tab == 0) {
    if (ImGui::BeginChild("AgentChatBody", ImVec2(0, content_height), true)) {
      bool drew_quick_actions = DrawAgentQuickActions();
      if (drew_quick_actions) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
      }
      agent_chat_->Draw(0.0f);
    }
    ImGui::EndChild();
  } else {
    if (ImGui::BeginChild("AgentQuickConfig", ImVec2(0, content_height),
                          true)) {
      bool auto_scroll = agent_chat_->auto_scroll();
      bool show_ts = agent_chat_->show_timestamps();
      bool show_reasoning = agent_chat_->show_reasoning();

      ImGui::TextColored(accent, "%s Display", ICON_MD_TUNE);
      if (ImGui::Checkbox("Auto-scroll", &auto_scroll)) {
        agent_chat_->set_auto_scroll(auto_scroll);
      }
      if (ImGui::Checkbox("Show timestamps", &show_ts)) {
        agent_chat_->set_show_timestamps(show_ts);
      }
      if (ImGui::Checkbox("Show reasoning traces", &show_reasoning)) {
        agent_chat_->set_show_reasoning(show_reasoning);
      }

      ImGui::Separator();
      ImGui::TextColored(accent, "%s Provider", ICON_MD_SMART_TOY);
      DrawPanelDescription(
          "Change provider/model in the main Agent Editor. This sidebar shows "
          "active chat controls.");
    }
    ImGui::EndChild();
  }

  // Footer actions (always visible, not clipped)
  gui::StyleVarGuard footer_spacing(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));
  {
    gui::StyleColorGuard focus_btn_colors({
        {ImGuiCol_Button, gui::GetPrimaryVec4()},
        {ImGuiCol_ButtonHovered, gui::GetPrimaryHoverVec4()},
        {ImGuiCol_ButtonActive, gui::GetPrimaryActiveVec4()},
    });
    if (ImGui::Button(ICON_MD_OPEN_IN_NEW " Focus Agent Chat",
                      ImVec2(-1, 0))) {
      agent_chat_->set_active(true);
      agent_chat_->ScrollToBottom();
    }
  }

  ImVec2 half_width(ImGui::GetContentRegionAvail().x / 2 - 4, 0);
  {
    gui::StyleColorGuard secondary_btn_colors({
        {ImGuiCol_Button, gui::GetSurfaceContainerVec4()},
        {ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighVec4()},
        {ImGuiCol_ButtonActive, gui::GetSurfaceContainerHighestVec4()},
    });
    if (ImGui::Button(ICON_MD_DELETE_FOREVER " Clear", half_width)) {
      agent_chat_->ClearHistory();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_FILE_DOWNLOAD " Save", half_width)) {
      agent_chat_->SaveHistory(ResolveAgentChatHistoryPath());
    }
  }
#else
  gui::ColoredText(ICON_MD_SMART_TOY " AI Agent Not Available",
                   gui::GetTextSecondaryVec4());

  ImGui::Spacing();
  DrawPanelDescription(
      "The AI Agent requires agent UI support. "
      "Build with YAZE_BUILD_AGENT_UI=ON to enable.");
#endif
}

bool RightPanelManager::DrawAgentQuickActions() {
#ifdef YAZE_BUILD_AGENT_UI
  if (!agent_chat_) {
    return false;
  }
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImVec4 accent = gui::GetPrimaryVec4();

  std::string selection_context;
  if (properties_panel_ && properties_panel_->HasSelection()) {
    selection_context =
        BuildSelectionContextSummary(properties_panel_->GetSelection());
  }

  struct QuickAction {
    const char* label;
    std::string prompt;
  };

  std::vector<QuickAction> actions;
  if (!selection_context.empty()) {
    actions.push_back({"Explain selection",
                       "Explain this selection and how to edit it safely.\n\n" +
                           selection_context});
    actions.push_back({"Suggest fixes",
                       "Suggest improvements or checks for this selection.\n\n" +
                           selection_context});
  }

  switch (active_editor_type_) {
    case EditorType::kOverworld:
      actions.push_back({"Summarize map",
                         "Summarize the current overworld map and its key "
                         "features. Use overworld tools if available."});
      actions.push_back({"List sprites/items",
                         "List notable sprites or items on the current "
                         "overworld map."});
      break;
    case EditorType::kDungeon:
      actions.push_back({"Audit room",
                         "Summarize the current dungeon room layout, doors, "
                         "and object density."});
      actions.push_back({"List sprites",
                         "List sprites in the current dungeon room and any "
                         "potential conflicts."});
      break;
    case EditorType::kGraphics:
      actions.push_back({"Review tiles",
                         "Review the current tileset usage and point out any "
                         "obvious issues."});
      actions.push_back({"Palette check",
                         "Check palette usage for contrast/readability "
                         "problems."});
      break;
    case EditorType::kPalette:
      actions.push_back({"Palette audit",
                         "Audit the active palette for hue/contrast balance "
                         "and note risks."});
      actions.push_back({"Theme ideas",
                         "Suggest a palette variation that fits the current "
                         "scene style."});
      break;
    case EditorType::kSprite:
      actions.push_back({"Sprite review",
                         "Review the selected sprite properties and suggest "
                         "tuning."});
      break;
    case EditorType::kMessage:
      actions.push_back({"Copy edit",
                         "Review the current message text for clarity and "
                         "style improvements."});
      break;
    case EditorType::kAssembly:
      actions.push_back({"ASM review",
                         "Review the current ASM changes for risks and style "
                         "issues."});
      break;
    case EditorType::kHex:
      actions.push_back({"Hex context",
                         "Explain what the current hex selection likely "
                         "represents."});
      break;
    case EditorType::kEmulator:
      actions.push_back({"Test suggestion",
                         "Propose a short emulator test to validate the "
                         "current feature."});
      break;
    case EditorType::kAgent:
      actions.push_back({"Agent config review",
                         "Review current agent configuration for practical "
                         "improvements."});
      break;
    default:
      actions.push_back({"Agent overview",
                         "Suggest the next best agent-assisted action for the "
                         "current editor context."});
      break;
  }

  if (actions.empty()) {
    return false;
  }

  ImGui::TextColored(accent, "%s Editor Actions", ICON_MD_BOLT);
  gui::ColoredText("Send a context-aware prompt to the agent.",
                   gui::GetTextSecondaryVec4());

  int columns = ImGui::GetContentRegionAvail().x > 420.0f ? 2 : 1;
  if (ImGui::BeginTable("AgentQuickActionsTable", columns,
                        ImGuiTableFlags_SizingStretchSame)) {
    for (const auto& action : actions) {
      ImGui::TableNextColumn();
      if (ImGui::Button(action.label, ImVec2(-1, 0))) {
        agent_chat_->SendMessage(action.prompt);
      }
    }
    ImGui::EndTable();
  }
  return true;
#else
  return false;
#endif
}

void RightPanelManager::DrawProposalsPanel() {
  if (proposal_drawer_) {
    // Set ROM and draw content inside the panel (not a separate window)
    if (rom_) {
      proposal_drawer_->SetRom(rom_);
    }
    proposal_drawer_->DrawContent();
  } else {
    gui::ColoredText(ICON_MD_DESCRIPTION " Proposals Not Available",
                     gui::GetTextSecondaryVec4());

    ImGui::Spacing();
    DrawPanelDescription(
        "The proposal system is not initialized. "
        "Proposals will appear here when the AI Agent creates them.");
  }
}

void RightPanelManager::DrawSettingsPanel() {
  if (settings_panel_) {
    // Draw settings inline (no card windows)
    settings_panel_->Draw();
  } else {
    gui::ColoredText(ICON_MD_SETTINGS " Settings Not Available",
                     gui::GetTextSecondaryVec4());

    ImGui::Spacing();
    DrawPanelDescription(
        "Settings will be available once initialized. "
        "This panel provides quick access to application settings.");
  }
}

void RightPanelManager::DrawHelpPanel() {
  // Context-aware editor header
  DrawEditorContextHeader();

  // Keyboard Shortcuts section (default open)
  if (BeginPanelSection("Keyboard Shortcuts", ICON_MD_KEYBOARD, true)) {
    DrawGlobalShortcuts();
    DrawEditorSpecificShortcuts();
    EndPanelSection();
  }

  // Editor-specific help (default open)
  if (BeginPanelSection("Editor Guide", ICON_MD_HELP, true)) {
    DrawEditorSpecificHelp();
    EndPanelSection();
  }

  // Quick Actions (collapsed by default)
  if (BeginPanelSection("Quick Actions", ICON_MD_BOLT, false)) {
    DrawQuickActionButtons();
    EndPanelSection();
  }

  // About section (collapsed by default)
  if (BeginPanelSection("About", ICON_MD_INFO, false)) {
    DrawAboutSection();
    EndPanelSection();
  }
}

void RightPanelManager::DrawEditorContextHeader() {
  const char* editor_name = "No Editor Selected";
  const char* editor_icon = ICON_MD_HELP;

  switch (active_editor_type_) {
    case EditorType::kOverworld:
      editor_name = "Overworld Editor";
      editor_icon = ICON_MD_LANDSCAPE;
      break;
    case EditorType::kDungeon:
      editor_name = "Dungeon Editor";
      editor_icon = ICON_MD_CASTLE;
      break;
    case EditorType::kGraphics:
      editor_name = "Graphics Editor";
      editor_icon = ICON_MD_IMAGE;
      break;
    case EditorType::kPalette:
      editor_name = "Palette Editor";
      editor_icon = ICON_MD_PALETTE;
      break;
    case EditorType::kMusic:
      editor_name = "Music Editor";
      editor_icon = ICON_MD_MUSIC_NOTE;
      break;
    case EditorType::kScreen:
      editor_name = "Screen Editor";
      editor_icon = ICON_MD_TV;
      break;
    case EditorType::kSprite:
      editor_name = "Sprite Editor";
      editor_icon = ICON_MD_SMART_TOY;
      break;
    case EditorType::kMessage:
      editor_name = "Message Editor";
      editor_icon = ICON_MD_CHAT;
      break;
    case EditorType::kEmulator:
      editor_name = "Emulator";
      editor_icon = ICON_MD_VIDEOGAME_ASSET;
      break;
    default:
      break;
  }

  // Draw context header with editor info
  gui::ColoredTextF(gui::GetPrimaryVec4(), "%s %s Help", editor_icon,
                    editor_name);

  DrawPanelDivider();
}

void RightPanelManager::DrawGlobalShortcuts() {
  const char* ctrl = gui::GetCtrlDisplayName();
  DrawPanelLabel("Global");
  ImGui::Indent(8.0f);
  DrawShortcutRow("Open", "Open ROM", absl::StrFormat("%s+O", ctrl));
  DrawShortcutRow("Save", "Save ROM", absl::StrFormat("%s+S", ctrl));
  DrawShortcutRow("Save As", "Save ROM As",
                  absl::StrFormat("%s+Shift+S", ctrl));
  DrawShortcutRow("Undo", "Undo", absl::StrFormat("%s+Z", ctrl));
  DrawShortcutRow("Redo", "Redo", absl::StrFormat("%s+Shift+Z", ctrl));
  DrawShortcutRow("Command Palette", "Command Palette",
                  absl::StrFormat("%s+Shift+P", ctrl));
  DrawShortcutRow("Global Search", "Global Search",
                  absl::StrFormat("%s+Shift+K", ctrl));
  DrawShortcutRow("view.toggle_activity_bar", "Toggle Sidebar",
                  absl::StrFormat("%s+B", ctrl));
  DrawShortcutRow("Show About", "About / Help", "F1");
  DrawPanelValue("Esc", "Close Panel");
  ImGui::Unindent(8.0f);
  ImGui::Spacing();
}

void RightPanelManager::DrawEditorSpecificShortcuts() {
  const char* ctrl = gui::GetCtrlDisplayName();
  switch (active_editor_type_) {
    case EditorType::kOverworld:
      DrawPanelLabel("Overworld");
      ImGui::Indent(8.0f);
      DrawPanelValue("1-3", "Switch World (LW/DW/SP)");
      DrawPanelValue("Arrow Keys", "Navigate Maps");
      DrawPanelValue("E", "Entity Mode");
      DrawPanelValue("T", "Tile Mode");
      DrawShortcutRow("overworld.brush_toggle", "Toggle brush", "B");
      DrawShortcutRow("overworld.fill", "Fill tool", "F");
      DrawShortcutRow("overworld.next_tile", "Next tile", "]");
      DrawShortcutRow("overworld.prev_tile", "Previous tile", "[");
      DrawPanelValue("Right Click", "Pick Tile");
      ImGui::Unindent(8.0f);
      break;

    case EditorType::kDungeon:
      DrawPanelLabel("Dungeon");
      ImGui::Indent(8.0f);
      DrawShortcutRow("dungeon.object.select_tool", "Select tool", "S");
      DrawShortcutRow("dungeon.object.place_tool", "Place tool", "P");
      DrawShortcutRow("dungeon.object.delete_tool", "Delete tool", "D");
      DrawShortcutRow("dungeon.object.copy", "Copy selection",
                      absl::StrFormat("%s+C", ctrl));
      DrawShortcutRow("dungeon.object.paste", "Paste selection",
                      absl::StrFormat("%s+V", ctrl));
      DrawShortcutRow("dungeon.object.delete", "Delete selection", "Delete");
      DrawPanelValue("Arrow Keys", "Move Object");
      DrawPanelValue("G", "Toggle Grid");
      DrawPanelValue("L", "Cycle Layers");
      ImGui::Unindent(8.0f);
      break;

    case EditorType::kGraphics:
      DrawPanelLabel("Graphics");
      ImGui::Indent(8.0f);
      DrawShortcutRow("graphics.prev_sheet", "Previous sheet", "PageUp");
      DrawShortcutRow("graphics.next_sheet", "Next sheet", "PageDown");
      DrawShortcutRow("graphics.tool.pencil", "Pencil tool", "B");
      DrawShortcutRow("graphics.tool.fill", "Fill tool", "G");
      DrawShortcutRow("graphics.zoom_in", "Zoom in", "+");
      DrawShortcutRow("graphics.zoom_out", "Zoom out", "-");
      DrawShortcutRow("graphics.toggle_grid", "Toggle grid",
                      absl::StrFormat("%s+G", ctrl));
      ImGui::Unindent(8.0f);
      break;

    case EditorType::kPalette:
      DrawPanelLabel("Palette");
      ImGui::Indent(8.0f);
      DrawPanelValue("Click", "Select Color");
      DrawPanelValue("Double Click", "Edit Color");
      DrawPanelValue("Drag", "Copy Color");
      ImGui::Unindent(8.0f);
      break;

    case EditorType::kMusic:
      DrawPanelLabel("Music");
      ImGui::Indent(8.0f);
      DrawShortcutRow("music.play_pause", "Play/Pause", "Space");
      DrawShortcutRow("music.stop", "Stop", "Esc");
      DrawShortcutRow("music.speed_up", "Speed up", "+");
      DrawShortcutRow("music.speed_down", "Slow down", "-");
      DrawPanelValue("Left/Right", "Seek");
      ImGui::Unindent(8.0f);
      break;

    case EditorType::kMessage:
      DrawPanelLabel("Message");
      ImGui::Indent(8.0f);
      DrawPanelValue(absl::StrFormat("%s+Enter", ctrl).c_str(),
                     "Insert Line Break");
      DrawPanelValue("Up/Down", "Navigate Messages");
      ImGui::Unindent(8.0f);
      break;

    default:
      DrawPanelLabel("Editor Shortcuts");
      ImGui::Indent(8.0f);
      {
        gui::StyleColorGuard text_color(ImGuiCol_Text,
                                        gui::GetTextSecondaryVec4());
        ImGui::TextWrapped("Select an editor to see specific shortcuts.");
      }
      ImGui::Unindent(8.0f);
      break;
  }
}

void RightPanelManager::DrawEditorSpecificHelp() {
  switch (active_editor_type_) {
    case EditorType::kOverworld: {
      gui::StyleColorGuard text_color(
          ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
      ImGui::Bullet();
      ImGui::TextWrapped("Paint tiles by selecting from Tile16 Selector");
      ImGui::Bullet();
      ImGui::TextWrapped(
          "Switch between Light World, Dark World, and Special Areas");
      ImGui::Bullet();
      ImGui::TextWrapped(
          "Use Entity Mode to place entrances, exits, items, and sprites");
      ImGui::Bullet();
      ImGui::TextWrapped("Right-click on the map to pick a tile for painting");
    } break;

    case EditorType::kDungeon: {
      gui::StyleColorGuard text_color(
          ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
      ImGui::Bullet();
      ImGui::TextWrapped("Select rooms from the Room Selector or Room Matrix");
      ImGui::Bullet();
      ImGui::TextWrapped("Place objects using the Object Editor panel");
      ImGui::Bullet();
      ImGui::TextWrapped(
          "Edit room headers for palette, GFX, and floor settings");
      ImGui::Bullet();
      ImGui::TextWrapped("Multiple rooms can be opened in separate tabs");
    } break;

    case EditorType::kGraphics: {
      gui::StyleColorGuard text_color(
          ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
      ImGui::Bullet();
      ImGui::TextWrapped("Browse graphics sheets using the Sheet Browser");
      ImGui::Bullet();
      ImGui::TextWrapped("Edit pixels directly with the Pixel Editor");
      ImGui::Bullet();
      ImGui::TextWrapped("Choose palettes from Palette Controls");
      ImGui::Bullet();
      ImGui::TextWrapped("View 3D objects like rupees and crystals");
    } break;

    case EditorType::kPalette: {
      gui::StyleColorGuard text_color(
          ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
      ImGui::Bullet();
      ImGui::TextWrapped("Edit overworld, dungeon, and sprite palettes");
      ImGui::Bullet();
      ImGui::TextWrapped("Use Quick Access for color harmony tools");
      ImGui::Bullet();
      ImGui::TextWrapped("Changes update in real-time across all editors");
    } break;

    case EditorType::kMusic: {
      gui::StyleColorGuard text_color(
          ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
      ImGui::Bullet();
      ImGui::TextWrapped("Browse songs in the Song Browser");
      ImGui::Bullet();
      ImGui::TextWrapped("Use the tracker for playback control");
      ImGui::Bullet();
      ImGui::TextWrapped("Edit instruments and BRR samples");
    } break;

    case EditorType::kMessage: {
      gui::StyleColorGuard text_color(
          ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
      ImGui::Bullet();
      ImGui::TextWrapped("Edit all in-game dialog messages");
      ImGui::Bullet();
      ImGui::TextWrapped("Preview text rendering with the font atlas");
      ImGui::Bullet();
      ImGui::TextWrapped("Manage the compression dictionary");
    } break;

    default:
      ImGui::Bullet();
      ImGui::TextWrapped("Open a ROM file via File > Open ROM");
      ImGui::Bullet();
      ImGui::TextWrapped("Select an editor from the sidebar");
      ImGui::Bullet();
      ImGui::TextWrapped("Use panels to access tools and settings");
      ImGui::Bullet();
      ImGui::TextWrapped("Save your work via File > Save ROM");
      break;
  }
}

void RightPanelManager::DrawQuickActionButtons() {
  const float button_width = ImGui::GetContentRegionAvail().x;

  gui::StyleVarGuard button_vars({
      {ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f)},
      {ImGuiStyleVar_FrameRounding, 4.0f},
  });

  // Documentation button
  {
    gui::StyleColorGuard btn_colors({
        {ImGuiCol_Button, gui::GetSurfaceContainerHighVec4()},
        {ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighestVec4()},
    });
    if (ImGui::Button(ICON_MD_DESCRIPTION " Open Documentation",
                      ImVec2(button_width, 0))) {
      gui::OpenUrl("https://github.com/scawful/yaze/wiki");
    }
  }

  ImGui::Spacing();

  // GitHub Issues button
  {
    gui::StyleColorGuard btn_colors({
        {ImGuiCol_Button, gui::GetSurfaceContainerHighVec4()},
        {ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighestVec4()},
    });
    if (ImGui::Button(ICON_MD_BUG_REPORT " Report Issue",
                      ImVec2(button_width, 0))) {
      gui::OpenUrl("https://github.com/scawful/yaze/issues/new");
    }
  }

  ImGui::Spacing();

  // Discord button
  {
    gui::StyleColorGuard btn_colors({
        {ImGuiCol_Button, gui::GetSurfaceContainerHighVec4()},
        {ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighestVec4()},
    });
    if (ImGui::Button(ICON_MD_FORUM " Join Discord",
                      ImVec2(button_width, 0))) {
      gui::OpenUrl("https://discord.gg/zU5qDm8MZg");
    }
  }
}

void RightPanelManager::DrawAboutSection() {
  gui::ColoredText("YAZE - Yet Another Zelda3 Editor", gui::GetPrimaryVec4());

  ImGui::Spacing();
  DrawPanelDescription(
      "A comprehensive editor for The Legend of Zelda: "
      "A Link to the Past ROM files.");

  DrawPanelDivider();

  DrawPanelLabel("Credits");
  ImGui::Spacing();
  ImGui::Text("Written by: scawful");
  ImGui::Text("Special Thanks: Zarby89, JaredBrian");

  DrawPanelDivider();

  DrawPanelLabel("Links");
  ImGui::Spacing();
  gui::ColoredText(ICON_MD_LINK " github.com/scawful/yaze",
                   gui::GetTextSecondaryVec4());
}

void RightPanelManager::DrawNotificationsPanel() {
  if (!toast_manager_) {
    gui::ColoredText(ICON_MD_NOTIFICATIONS_OFF " Notifications Unavailable",
                     gui::GetTextSecondaryVec4());
    return;
  }

  // Header actions
  float avail = ImGui::GetContentRegionAvail().x;

  // Mark all read / Clear all buttons
  {
    gui::StyleColorGuard btn_colors({
        {ImGuiCol_Button, gui::GetSurfaceContainerHighVec4()},
        {ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighestVec4()},
    });

    if (ImGui::Button(ICON_MD_DONE_ALL " Mark All Read",
                      ImVec2(avail * 0.5f - 4.0f, 0))) {
      toast_manager_->MarkAllRead();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_DELETE_SWEEP " Clear All",
                      ImVec2(avail * 0.5f - 4.0f, 0))) {
      toast_manager_->ClearHistory();
    }
  }

  DrawPanelDivider();

  // Notification history
  const auto& history = toast_manager_->GetHistory();

  if (history.empty()) {
    ImGui::Spacing();
    gui::ColoredText(ICON_MD_INBOX " No notifications",
                     gui::GetTextSecondaryVec4());
    ImGui::Spacing();
    DrawPanelDescription(
        "Notifications will appear here when actions complete.");
    return;
  }

  // Stats
  size_t unread_count = toast_manager_->GetUnreadCount();
  if (unread_count > 0) {
    gui::ColoredTextF(gui::GetPrimaryVec4(), "%zu unread", unread_count);
  } else {
    gui::ColoredText("All caught up", gui::GetTextSecondaryVec4());
  }

  ImGui::Spacing();

  // Scrollable notification list
  ImGui::BeginChild("##NotificationList", ImVec2(0, 0), false,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  auto now = std::chrono::system_clock::now();

  // Group by time (Today, Yesterday, Older)
  bool shown_today = false;
  bool shown_yesterday = false;
  bool shown_older = false;

  for (const auto& entry : history) {
    auto diff =
        std::chrono::duration_cast<std::chrono::hours>(now - entry.timestamp)
            .count();

    // Time grouping headers
    if (diff < 24 && !shown_today) {
      DrawPanelLabel("Today");
      shown_today = true;
    } else if (diff >= 24 && diff < 48 && !shown_yesterday) {
      ImGui::Spacing();
      DrawPanelLabel("Yesterday");
      shown_yesterday = true;
    } else if (diff >= 48 && !shown_older) {
      ImGui::Spacing();
      DrawPanelLabel("Older");
      shown_older = true;
    }

    // Notification item
    ImGui::PushID(&entry);

    // Icon and color based on type
    const char* icon;
    ImVec4 color;
    switch (entry.type) {
      case ToastType::kSuccess:
        icon = ICON_MD_CHECK_CIRCLE;
        color = gui::ConvertColorToImVec4(theme.success);
        break;
      case ToastType::kWarning:
        icon = ICON_MD_WARNING;
        color = gui::ConvertColorToImVec4(theme.warning);
        break;
      case ToastType::kError:
        icon = ICON_MD_ERROR;
        color = gui::ConvertColorToImVec4(theme.error);
        break;
      default:
        icon = ICON_MD_INFO;
        color = gui::ConvertColorToImVec4(theme.info);
        break;
    }

    // Unread indicator
    if (!entry.read) {
      gui::ColoredText(ICON_MD_FIBER_MANUAL_RECORD, gui::GetPrimaryVec4());
      ImGui::SameLine();
    }

    // Icon
    gui::ColoredTextF(color, "%s", icon);
    ImGui::SameLine();

    // Message
    ImGui::TextWrapped("%s", entry.message.c_str());

    // Timestamp
    auto diff_sec =
        std::chrono::duration_cast<std::chrono::seconds>(now - entry.timestamp)
            .count();
    std::string time_str;
    if (diff_sec < 60) {
      time_str = "just now";
    } else if (diff_sec < 3600) {
      time_str = absl::StrFormat("%dm ago", diff_sec / 60);
    } else if (diff_sec < 86400) {
      time_str = absl::StrFormat("%dh ago", diff_sec / 3600);
    } else {
      time_str = absl::StrFormat("%dd ago", diff_sec / 86400);
    }

    gui::ColoredTextF(gui::GetTextDisabledVec4(), "  %s", time_str.c_str());

    ImGui::PopID();
    ImGui::Spacing();
  }

  ImGui::EndChild();
}

void RightPanelManager::DrawPropertiesPanel() {
  if (properties_panel_) {
    properties_panel_->Draw();
  } else {
    // Placeholder when no properties panel is set
    gui::ColoredText(ICON_MD_SELECT_ALL " No Selection",
                     gui::GetTextSecondaryVec4());

    ImGui::Spacing();
    DrawPanelDescription(
        "Select an item in the editor to view and edit its properties here.");

    DrawPanelDivider();

    // Show placeholder sections for what properties would look like
    if (BeginPanelSection("Position & Size", ICON_MD_STRAIGHTEN, true)) {
      DrawPanelValue("X", "--");
      DrawPanelValue("Y", "--");
      DrawPanelValue("Width", "--");
      DrawPanelValue("Height", "--");
      EndPanelSection();
    }

    if (BeginPanelSection("Appearance", ICON_MD_PALETTE, false)) {
      DrawPanelValue("Tile ID", "--");
      DrawPanelValue("Palette", "--");
      DrawPanelValue("Layer", "--");
      EndPanelSection();
    }

    if (BeginPanelSection("Behavior", ICON_MD_SETTINGS, false)) {
      DrawPanelValue("Type", "--");
      DrawPanelValue("Subtype", "--");
      DrawPanelValue("Properties", "--");
      EndPanelSection();
    }
  }
}

void RightPanelManager::DrawProjectPanel() {
  if (project_panel_) {
    project_panel_->Draw();
  } else {
    gui::ColoredText(ICON_MD_FOLDER_SPECIAL " No Project Loaded",
                     gui::GetTextSecondaryVec4());

    ImGui::Spacing();
    DrawPanelDescription(
        "Open a .yaze project file to access project management features "
        "including ROM versioning, snapshots, and configuration.");

    DrawPanelDivider();

    // Placeholder for project features
    if (BeginPanelSection("Quick Start", ICON_MD_ROCKET_LAUNCH, true)) {
      ImGui::Bullet();
      ImGui::TextWrapped("Create a new project via File > New Project");
      ImGui::Bullet();
      ImGui::TextWrapped("Open existing .yaze project files");
      ImGui::Bullet();
      ImGui::TextWrapped("Projects track ROM versions and settings");
      EndPanelSection();
    }

    if (BeginPanelSection("Features", ICON_MD_CHECKLIST, false)) {
      ImGui::Bullet();
      ImGui::TextWrapped("Version snapshots with Git integration");
      ImGui::Bullet();
      ImGui::TextWrapped("ROM backup and restore");
      ImGui::Bullet();
      ImGui::TextWrapped("Project-specific settings");
      ImGui::Bullet();
      ImGui::TextWrapped("Assembly code folder integration");
      EndPanelSection();
    }
  }
}

bool RightPanelManager::DrawPanelToggleButtons() {
  bool clicked = false;

  // Helper lambda for drawing panel toggle buttons with consistent styling
  auto DrawPanelButton = [&](const char* icon, const char* base_tooltip,
                             const char* shortcut_action, PanelType type) {
    bool is_active = IsPanelActive(type);

    // Consistent button styling - transparent background with hover states
    gui::StyleColorGuard btn_colors({
        {ImGuiCol_Button, ImVec4(0, 0, 0, 0)},
        {ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighVec4()},
        {ImGuiCol_ButtonActive, gui::GetSurfaceContainerHighestVec4()},
        // Active = primary color, inactive = secondary text color
        {ImGuiCol_Text, is_active ? gui::GetPrimaryVec4()
                                  : gui::GetTextSecondaryVec4()},
    });

    if (ImGui::SmallButton(icon)) {
      TogglePanel(type);
      clicked = true;
    }

    if (ImGui::IsItemHovered()) {
      std::string shortcut = GetShortcutLabel(shortcut_action, "");
      if (shortcut.empty() || shortcut == "Unassigned") {
        ImGui::SetTooltip("%s", base_tooltip);
      } else {
        ImGui::SetTooltip("%s (%s)", base_tooltip, shortcut.c_str());
      }
    }
  };

  // Project button
  DrawPanelButton(ICON_MD_FOLDER_SPECIAL, "Project Panel",
                  "View: Toggle Project Panel", PanelType::kProject);
  ImGui::SameLine();

  // Agent Chat button
  DrawPanelButton(ICON_MD_SMART_TOY, "AI Agent Panel",
                  "View: Toggle AI Agent Panel", PanelType::kAgentChat);
  ImGui::SameLine();

  // Help button
  DrawPanelButton(ICON_MD_HELP_OUTLINE, "Help Panel",
                  "View: Toggle Help Panel", PanelType::kHelp);
  ImGui::SameLine();

  // Settings button
  DrawPanelButton(ICON_MD_SETTINGS, "Settings Panel",
                  "View: Toggle Settings Panel", PanelType::kSettings);
  ImGui::SameLine();

  // Properties button (last button - no SameLine after)
  DrawPanelButton(ICON_MD_LIST_ALT, "Properties Panel",
                  "View: Toggle Properties Panel", PanelType::kProperties);

  return clicked;
}

}  // namespace editor
}  // namespace yaze
