#include "app/editor/menu/right_panel_manager.h"

#include "app/editor/agent/agent_chat_widget.h"
#include "app/editor/agent/agent_sidebar.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/ui/toast_manager.h"
#include "app/editor/ui/selection_properties_panel.h"
#include "app/editor/ui/settings_panel.h"
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
    case RightPanelManager::PanelType::kProperties:
      return "Properties";
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
    case RightPanelManager::PanelType::kProperties:
      return ICON_MD_LIST_ALT;
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
    case PanelType::kProperties:
      return properties_width_;
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
    case PanelType::kProperties:
      properties_width_ = width;
      break;
    default:
      break;
  }
}

void RightPanelManager::Draw() {
  if (active_panel_ == PanelType::kNone) {
    return;
  }

  // Handle Escape key to close panel
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    ClosePanel();
    return;
  }

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float viewport_height = viewport->WorkSize.y;
  const float viewport_width = viewport->WorkSize.x;
  const float panel_width = GetPanelWidth();

  // Use SurfaceContainer for slightly elevated panel background
  ImVec4 panel_bg = gui::GetSurfaceContainerVec4();
  ImVec4 panel_border = gui::GetOutlineVec4();

  ImGuiWindowFlags panel_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoNavFocus;

  // Position panel on right edge, full height
  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x + viewport_width - panel_width,
             viewport->WorkPos.y));
  ImGui::SetNextWindowSize(ImVec2(panel_width, viewport_height));

  ImGui::PushStyleColor(ImGuiCol_WindowBg, panel_bg);
  ImGui::PushStyleColor(ImGuiCol_Border, panel_border);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

  if (ImGui::Begin("##RightPanel", nullptr, panel_flags)) {
    // Draw enhanced panel header
    DrawPanelHeader(GetPanelTypeName(active_panel_),
                    GetPanelTypeIcon(active_panel_));

    // Content area with padding
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
    ImGui::BeginChild("##PanelContent", ImVec2(0, 0), false,
                      ImGuiWindowFlags_AlwaysUseWindowPadding);

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
      case PanelType::kProperties:
        DrawPropertiesPanel();
        break;
      default:
        break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();  // WindowPadding for content
  }
  ImGui::End();

  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
}



void RightPanelManager::DrawPanelHeader(const char* title, const char* icon) {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const float header_height = 44.0f;
  const float padding = 12.0f;

  // Header background - slightly elevated surface
  ImVec2 header_min = ImGui::GetCursorScreenPos();
  ImVec2 header_max = ImVec2(header_min.x + ImGui::GetWindowWidth(),
                             header_min.y + header_height);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(header_min, header_max,
                           ImGui::GetColorU32(gui::GetSurfaceContainerHighVec4()));

  // Draw subtle bottom border
  draw_list->AddLine(ImVec2(header_min.x, header_max.y),
                     ImVec2(header_max.x, header_max.y),
                     ImGui::GetColorU32(gui::GetOutlineVec4()), 1.0f);

  // Position content within header
  ImGui::SetCursorPosX(padding);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (header_height - ImGui::GetTextLineHeight()) * 0.5f);

  // Panel icon with primary color
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  ImGui::Text("%s", icon);
  ImGui::PopStyleColor();

  ImGui::SameLine();

  // Panel title (use current style text color)
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
  ImGui::Text("%s", title);
  ImGui::PopStyleColor();

  // Right-aligned buttons
  const float button_size = 28.0f;
  float current_x = ImGui::GetWindowWidth() - button_size - padding;

  // Close button
  ImGui::SameLine(current_x);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);  // Center vertically

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        gui::GetSurfaceContainerHighestVec4());
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(gui::GetPrimaryVec4().x * 0.3f,
                               gui::GetPrimaryVec4().y * 0.3f,
                               gui::GetPrimaryVec4().z * 0.3f, 0.4f));
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

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
    
    // TODO: Hook up to actual lock state in SelectionPropertiesPanel
    static bool is_locked = false; 
    ImVec4 lock_color = is_locked ? gui::GetPrimaryVec4() : gui::GetTextSecondaryVec4();
    ImGui::PushStyleColor(ImGuiCol_Text, lock_color);
    
    if (ImGui::Button(is_locked ? ICON_MD_LOCK : ICON_MD_LOCK_OPEN, ImVec2(button_size, button_size))) {
      is_locked = !is_locked;
    }
    ImGui::PopStyleColor();
    
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(is_locked ? "Unlock Selection" : "Lock Selection");
    }
  }

  ImGui::PopStyleVar();
  ImGui::PopStyleColor(4);

  // Move cursor past the header
  ImGui::SetCursorPosY(header_height + 8.0f);
}

// =============================================================================
// Panel Styling Helpers
// =============================================================================

bool RightPanelManager::BeginPanelSection(const char* label, const char* icon,
                                          bool default_open) {
  ImGui::PushStyleColor(ImGuiCol_Header, gui::GetSurfaceContainerHighVec4());
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                        gui::GetSurfaceContainerHighestVec4());
  ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                        gui::GetSurfaceContainerHighestVec4());
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

  // Build header text with icon if provided
  std::string header_text;
  if (icon) {
    header_text = std::string(icon) + "  " + label;
  } else {
    header_text = label;
  }

  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed |
                             ImGuiTreeNodeFlags_SpanAvailWidth |
                             ImGuiTreeNodeFlags_AllowOverlap |
                             ImGuiTreeNodeFlags_FramePadding;
  if (default_open) {
    flags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  bool is_open = ImGui::TreeNodeEx(header_text.c_str(), flags);

  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(3);

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
  ImGui::PushStyleColor(ImGuiCol_Separator, gui::GetOutlineVec4());
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();
}

void RightPanelManager::DrawPanelLabel(const char* label) {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
}

void RightPanelManager::DrawPanelValue(const char* label, const char* value) {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
  ImGui::Text("%s:", label);
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::TextUnformatted(value);
}

void RightPanelManager::DrawPanelDescription(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextDisabledVec4());
  ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
  ImGui::TextWrapped("%s", text);
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
}

// =============================================================================
// Panel Content Drawing
// =============================================================================

void RightPanelManager::DrawAgentChatPanel() {
#ifdef YAZE_BUILD_AGENT_UI
  // Prefer AgentSidebar if available (cleaner UI for sidebar)
  if (agent_sidebar_) {
    agent_sidebar_->Draw();
  } else if (agent_chat_widget_) {
    // Fallback to full AgentChatWidget
    agent_chat_widget_->set_active(true);
    agent_chat_widget_->Draw();
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
    ImGui::Text(ICON_MD_SMART_TOY " AI Agent Not Available");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    DrawPanelDescription(
        "The AI Agent is not initialized. "
        "Open the AI Agent from View menu or use Ctrl+Shift+A.");
  }
#else
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
  ImGui::Text(ICON_MD_SMART_TOY " AI Agent Not Available");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  DrawPanelDescription(
      "The AI Agent requires agent UI support. "
      "Build with YAZE_BUILD_AGENT_UI=ON to enable.");
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
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
    ImGui::Text(ICON_MD_DESCRIPTION " Proposals Not Available");
    ImGui::PopStyleColor();

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
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
    ImGui::Text(ICON_MD_SETTINGS " Settings Not Available");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    DrawPanelDescription(
        "Settings will be available once initialized. "
        "This panel provides quick access to application settings.");
  }
}

void RightPanelManager::DrawHelpPanel() {
  // Quick Start section
  if (BeginPanelSection("Quick Start", ICON_MD_ROCKET_LAUNCH, true)) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));

    ImGui::Bullet();
    ImGui::TextWrapped("Open a ROM file via File > Open ROM");

    ImGui::Bullet();
    ImGui::TextWrapped("Select an editor from the editor selection dialog");

    ImGui::Bullet();
    ImGui::TextWrapped("Use the sidebar to toggle editor cards");

    ImGui::Bullet();
    ImGui::TextWrapped("Save your work via File > Save ROM");

    ImGui::PopStyleColor();
    EndPanelSection();
  }

  // Keyboard Shortcuts section
  if (BeginPanelSection("Keyboard Shortcuts", ICON_MD_KEYBOARD, false)) {
    // File operations
    DrawPanelLabel("File Operations");
    ImGui::Indent(8.0f);
    DrawPanelValue("Ctrl+O", "Open ROM");
    DrawPanelValue("Ctrl+S", "Save ROM");
    ImGui::Unindent(8.0f);
    ImGui::Spacing();

    // Navigation
    DrawPanelLabel("Navigation");
    ImGui::Indent(8.0f);
    DrawPanelValue("Ctrl+B", "Toggle Sidebar");
    DrawPanelValue("Ctrl+1-9", "Switch Editors");
    ImGui::Unindent(8.0f);
    ImGui::Spacing();

    // Tools
    DrawPanelLabel("Tools");
    ImGui::Indent(8.0f);
    DrawPanelValue("Ctrl+Shift+P", "Command Palette");
    DrawPanelValue("Ctrl+Shift+F", "Global Search");
    DrawPanelValue("Esc", "Close Panel");
    ImGui::Unindent(8.0f);

    EndPanelSection();
  }

  // About section
  if (BeginPanelSection("About", ICON_MD_INFO, false)) {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
    ImGui::Text("YAZE - Yet Another Zelda3 Editor");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    DrawPanelDescription(
        "A comprehensive editor for The Legend of Zelda: "
        "A Link to the Past ROM files.");

    DrawPanelDivider();

    DrawPanelLabel("Links");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
    ImGui::Text(ICON_MD_LINK " github.com/scawful/yaze");
    ImGui::PopStyleColor();

    EndPanelSection();
  }
}

void RightPanelManager::DrawPropertiesPanel() {
  if (properties_panel_) {
    properties_panel_->Draw();
  } else {
    // Placeholder when no properties panel is set
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
    ImGui::Text(ICON_MD_SELECT_ALL " No Selection");
    ImGui::PopStyleColor();

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

bool RightPanelManager::DrawPanelToggleButtons() {
  bool clicked = false;

  // Helper lambda for drawing panel toggle buttons with consistent styling
  auto DrawPanelButton = [&](const char* icon, const char* tooltip,
                             PanelType type) {
    bool is_active = IsPanelActive(type);

    // Consistent button styling - transparent background with hover states
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          gui::GetSurfaceContainerHighVec4());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          gui::GetSurfaceContainerHighestVec4());
    // Active = primary color, inactive = secondary text color
    ImGui::PushStyleColor(
        ImGuiCol_Text,
        is_active ? gui::GetPrimaryVec4() : gui::GetTextSecondaryVec4());

    if (ImGui::SmallButton(icon)) {
      TogglePanel(type);
      clicked = true;
    }

    ImGui::PopStyleColor(4);

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tooltip);
    }
  };

#ifdef YAZE_WITH_GRPC
  // Agent Chat button
  DrawPanelButton(ICON_MD_SMART_TOY, "AI Agent Panel", PanelType::kAgentChat);
  ImGui::SameLine();
#endif



  // Settings button
  DrawPanelButton(ICON_MD_SETTINGS, "Settings Panel", PanelType::kSettings);
  ImGui::SameLine();

  // Properties button (last button - no SameLine after)
  DrawPanelButton(ICON_MD_LIST_ALT, "Properties Panel", PanelType::kProperties);

  return clicked;
}

}  // namespace editor
}  // namespace yaze

