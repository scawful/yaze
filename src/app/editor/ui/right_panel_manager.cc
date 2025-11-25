#include "app/editor/ui/right_panel_manager.h"

#include "app/editor/agent/agent_chat_widget.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/settings_editor.h"
#include "app/editor/system/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

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
    default:
      return ICON_MD_HELP;
  }
}

void RightPanelManager::TogglePanel(PanelType type) {
  if (active_panel_ == type) {
    ClosePanel();
  } else {
    OpenPanel(type);
  }
}

void RightPanelManager::OpenPanel(PanelType type) {
  active_panel_ = type;
  animating_ = true;
  panel_animation_ = 0.0f;
}

void RightPanelManager::ClosePanel() {
  active_panel_ = PanelType::kNone;
  animating_ = false;
  panel_animation_ = 0.0f;
}

float RightPanelManager::GetPanelWidth() const {
  if (active_panel_ == PanelType::kNone) {
    return 0.0f;
  }

  switch (active_panel_) {
    case PanelType::kAgentChat:
      return agent_chat_width_;
    case PanelType::kProposals:
      return proposals_width_;
    case PanelType::kSettings:
      return settings_width_;
    case PanelType::kHelp:
      return help_width_;
    default:
      return 0.0f;
  }
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
    default:
      break;
  }
}

void RightPanelManager::Draw() {
  if (active_panel_ == PanelType::kNone) {
    return;
  }

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  // Use viewport for accurate positioning (panel fills full height)
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float viewport_height = viewport->WorkSize.y;
  const float viewport_width = viewport->WorkSize.x;
  const float panel_width = GetPanelWidth();

  // Use theme colors for panel background
  ImVec4 panel_bg = gui::ConvertColorToImVec4(theme.surface);
  ImVec4 panel_border = gui::ConvertColorToImVec4(theme.text_disabled);

  ImGuiWindowFlags panel_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoNavFocus;

  // Position panel on right edge, full height (menu bar is in dockspace region)
  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x + viewport_width - panel_width, viewport->WorkPos.y));
  ImGui::SetNextWindowSize(ImVec2(panel_width, viewport_height));

  ImGui::PushStyleColor(ImGuiCol_WindowBg, panel_bg);
  ImGui::PushStyleColor(ImGuiCol_Border, panel_border);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

  if (ImGui::Begin("##RightPanel", nullptr, panel_flags)) {
    // Draw panel header with close button
    DrawPanelHeader(GetPanelTypeName(active_panel_),
                    GetPanelTypeIcon(active_panel_));

    ImGui::Separator();
    ImGui::Spacing();

    // Draw panel content based on type
    switch (active_panel_) {
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
      default:
        break;
    }
  }
  ImGui::End();

  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
}

void RightPanelManager::DrawPanelHeader(const char* title, const char* icon) {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  // Panel icon and title
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  ImGui::Text("%s", icon);
  ImGui::PopStyleColor();

  ImGui::SameLine();
  ImGui::Text("%s", title);

  // Close button (right-aligned)
  ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f);
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighVec4());
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, gui::GetSurfaceContainerHighestVec4());

  if (ImGui::SmallButton(ICON_MD_CLOSE)) {
    ClosePanel();
  }

  ImGui::PopStyleColor(3);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Close Panel");
  }
}

void RightPanelManager::DrawAgentChatPanel() {
#ifdef YAZE_WITH_GRPC
  if (agent_chat_widget_) {
    // Set active state and draw the chat widget content
    // The widget will draw its own UI
    agent_chat_widget_->set_active(true);
    agent_chat_widget_->Draw();
  } else {
    ImGui::TextDisabled("AI Agent not available");
    ImGui::TextWrapped(
        "The AI Agent requires gRPC support. "
        "Build with YAZE_WITH_GRPC=ON to enable.");
  }
#else
  ImGui::TextDisabled("AI Agent not available");
  ImGui::TextWrapped(
      "The AI Agent requires gRPC support. "
      "Build with YAZE_WITH_GRPC=ON to enable.");
#endif
}

void RightPanelManager::DrawProposalsPanel() {
  if (proposal_drawer_) {
    // Set ROM and draw
    if (rom_) {
      proposal_drawer_->SetRom(rom_);
    }
    proposal_drawer_->Show();
    proposal_drawer_->Draw();
  } else {
    ImGui::TextDisabled("Proposal system not initialized");
  }
}

void RightPanelManager::DrawSettingsPanel() {
  if (settings_editor_) {
    // Draw settings directly in the panel
    auto status = settings_editor_->Update();
    if (!status.ok()) {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Settings error: %s",
                         status.message().data());
    }
  } else {
    ImGui::TextDisabled("Settings not initialized");
  }
}

void RightPanelManager::DrawHelpPanel() {
  ImGui::Text(ICON_MD_HELP " Help & Documentation");
  ImGui::Separator();
  ImGui::Spacing();

  // Quick links section
  if (ImGui::CollapsingHeader("Quick Start", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BulletText("Open a ROM file via File > Open ROM");
    ImGui::BulletText("Select an editor from the editor selection dialog");
    ImGui::BulletText("Use the sidebar to toggle editor cards");
    ImGui::BulletText("Save your work via File > Save ROM");
  }

  if (ImGui::CollapsingHeader("Keyboard Shortcuts")) {
    ImGui::BulletText("Ctrl+O: Open ROM");
    ImGui::BulletText("Ctrl+S: Save ROM");
    ImGui::BulletText("Ctrl+B: Toggle Sidebar");
    ImGui::BulletText("Ctrl+1-9: Switch Editors");
    ImGui::BulletText("Ctrl+Shift+P: Command Palette");
    ImGui::BulletText("Ctrl+Shift+F: Global Search");
  }

  if (ImGui::CollapsingHeader("About")) {
    ImGui::Text("YAZE - Yet Another Zelda3 Editor");
    ImGui::TextWrapped(
        "A comprehensive editor for The Legend of Zelda: "
        "A Link to the Past ROM files.");
    ImGui::Spacing();
    ImGui::TextDisabled("Visit github.com/scawful/yaze for more info");
  }
}

bool RightPanelManager::DrawPanelToggleButtons() {
  bool clicked = false;
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  // Style for panel toggle buttons
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        gui::GetSurfaceContainerHighVec4());
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        gui::GetSurfaceContainerHighestVec4());

#ifdef YAZE_WITH_GRPC
  // Agent Chat button
  bool agent_active = IsPanelActive(PanelType::kAgentChat);
  if (agent_active) {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  }

  if (ImGui::SmallButton(ICON_MD_SMART_TOY)) {
    TogglePanel(PanelType::kAgentChat);
    clicked = true;
  }

  if (agent_active) {
    ImGui::PopStyleColor();
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("AI Agent Panel");
  }

  ImGui::SameLine();
#endif

  // Proposals button
  bool proposals_active = IsPanelActive(PanelType::kProposals);
  if (proposals_active) {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  }

  if (ImGui::SmallButton(ICON_MD_DESCRIPTION)) {
    TogglePanel(PanelType::kProposals);
    clicked = true;
  }

  if (proposals_active) {
    ImGui::PopStyleColor();
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Proposals Panel");
  }

  ImGui::SameLine();

  // Settings button
  bool settings_active = IsPanelActive(PanelType::kSettings);
  if (settings_active) {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  }

  if (ImGui::SmallButton(ICON_MD_SETTINGS)) {
    TogglePanel(PanelType::kSettings);
    clicked = true;
  }

  if (settings_active) {
    ImGui::PopStyleColor();
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Settings Panel");
  }

  ImGui::PopStyleColor(3);

  return clicked;
}

}  // namespace editor
}  // namespace yaze

