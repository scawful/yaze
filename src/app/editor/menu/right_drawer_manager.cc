#include "app/editor/menu/right_drawer_manager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <optional>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_chat.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/hack/workflow/workflow_activity_widgets.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/shortcut_manager.h"
#include "app/editor/ui/project_management_panel.h"
#include "app/editor/ui/selection_properties_panel.h"
#include "app/editor/ui/settings_panel.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/animation/animator.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/platform_keys.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "util/json.h"
#include "util/log.h"
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

std::string JsonValueToDisplayString(const Json& value) {
  if (value.is_string()) {
    return value.get<std::string>();
  }
  if (value.is_boolean()) {
    return value.get<bool>() ? "true" : "false";
  }
  if (value.is_number_integer()) {
    return std::to_string(value.get<long long>());
  }
  if (value.is_number_unsigned()) {
    return std::to_string(value.get<unsigned long long>());
  }
  if (value.is_number_float()) {
    return absl::StrFormat("%.3f", value.get<double>());
  }
  if (value.is_null()) {
    return "null";
  }
  return value.dump();
}

std::string SanitizeToolOutputIdFragment(const std::string& value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (unsigned char ch : value) {
    if (std::isalnum(ch)) {
      sanitized.push_back(static_cast<char>(ch));
    } else {
      sanitized.push_back('_');
    }
  }
  return sanitized.empty() ? std::string("entry") : sanitized;
}

bool TryParseToolOutputJson(const std::string& text, Json* out) {
  if (!out || text.empty()) {
    return false;
  }
  try {
    *out = Json::parse(text);
    return out->is_object();
  } catch (...) {
    return false;
  }
}

std::optional<uint32_t> ParseToolOutputAddress(const Json& value) {
  if (value.is_number_unsigned()) {
    return static_cast<uint32_t>(value.get<uint64_t>());
  }
  if (value.is_number_integer()) {
    return static_cast<uint32_t>(value.get<int64_t>());
  }
  if (!value.is_string()) {
    return std::nullopt;
  }

  std::string token = value.get<std::string>();
  if (token.empty()) {
    return std::nullopt;
  }
  if (token[0] == '$') {
    token = token.substr(1);
  } else if (token.size() > 2 && token[0] == '0' &&
             (token[1] == 'x' || token[1] == 'X')) {
    token = token.substr(2);
  }
  try {
    return static_cast<uint32_t>(std::stoul(token, nullptr, 16));
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<uint32_t> ExtractToolOutputAddress(const Json& object) {
  if (!object.is_object()) {
    return std::nullopt;
  }
  for (const char* key : {"address", "entry_address"}) {
    if (object.contains(key)) {
      auto parsed = ParseToolOutputAddress(object[key]);
      if (parsed.has_value()) {
        return parsed;
      }
    }
  }
  return std::nullopt;
}

std::string ExtractToolOutputReference(const Json& object) {
  if (!object.is_object()) {
    return {};
  }
  if (object.contains("source") && object["source"].is_string()) {
    return object["source"].get<std::string>();
  }
  if (object.contains("file") && object["file"].is_string()) {
    std::string reference = object["file"].get<std::string>();
    if (object.contains("line") && object["line"].is_number_integer()) {
      reference = absl::StrCat(reference, ":", object["line"].get<int>());
    }
    return reference;
  }
  return {};
}

std::string BuildToolOutputEntryTitle(const Json& object) {
  if (!object.is_object()) {
    return {};
  }

  const std::string address = object.contains("address")
                                  ? JsonValueToDisplayString(object["address"])
                                  : "";
  const std::string bank =
      object.contains("bank") ? JsonValueToDisplayString(object["bank"]) : "";
  const std::string name =
      object.contains("name") ? JsonValueToDisplayString(object["name"]) : "";
  const std::string source = ExtractToolOutputReference(object);

  if (!source.empty() && !address.empty()) {
    return absl::StrFormat("%s  (%s)", source.c_str(), address.c_str());
  }
  if (!name.empty() && !address.empty()) {
    return absl::StrFormat("%s  %s", name.c_str(), address.c_str());
  }
  if (!name.empty() && !bank.empty()) {
    return absl::StrFormat("%s  %s", name.c_str(), bank.c_str());
  }
  if (!source.empty()) {
    return source;
  }
  if (!address.empty()) {
    return address;
  }
  if (!bank.empty()) {
    return bank;
  }
  if (!name.empty()) {
    return name;
  }
  return {};
}

std::string BuildToolOutputActionLabel(const char* visible_label,
                                       const char* action_key,
                                       const Json& object) {
  const auto address = ExtractToolOutputAddress(object);
  if (address.has_value()) {
    return absl::StrFormat("%s##tool_output_%s_%06X", visible_label, action_key,
                           *address);
  }

  const std::string reference = ExtractToolOutputReference(object);
  if (!reference.empty()) {
    return absl::StrFormat("%s##tool_output_%s_%s", visible_label, action_key,
                           SanitizeToolOutputIdFragment(reference).c_str());
  }

  return absl::StrFormat("%s##tool_output_%s_entry", visible_label, action_key);
}

void DrawToolOutputEntryActions(
    const Json& object, const RightDrawerManager::ToolOutputActions& actions) {
  if (!object.is_object()) {
    return;
  }

  const std::string reference = ExtractToolOutputReference(object);
  const auto address = ExtractToolOutputAddress(object);
  bool drew_any = false;

  if (!reference.empty() && actions.on_open_reference) {
    const std::string label =
        BuildToolOutputActionLabel("Open", "open", object);
    if (ImGui::SmallButton(label.c_str())) {
      actions.on_open_reference(reference);
    }
    drew_any = true;
  }
  if (address.has_value() && actions.on_open_address) {
    if (drew_any) {
      ImGui::SameLine();
    }
    const std::string label =
        BuildToolOutputActionLabel("Addr", "addr", object);
    if (ImGui::SmallButton(label.c_str())) {
      actions.on_open_address(*address);
    }
    drew_any = true;
  }
  if (address.has_value() && actions.on_open_lookup) {
    if (drew_any) {
      ImGui::SameLine();
    }
    const std::string label =
        BuildToolOutputActionLabel("Lookup", "lookup", object);
    if (ImGui::SmallButton(label.c_str())) {
      actions.on_open_lookup(*address);
    }
  }
}

void DrawJsonObjectFields(const Json& object) {
  for (auto it = object.begin(); it != object.end(); ++it) {
    if (it.value().is_array() || it.value().is_object()) {
      continue;
    }
    ImGui::BulletText("%s: %s", it.key().c_str(),
                      JsonValueToDisplayString(it.value()).c_str());
  }
}

void DrawJsonObjectArraySection(
    const char* label, const Json& array,
    const RightDrawerManager::ToolOutputActions& actions) {
  if (!array.is_array() || array.empty()) {
    return;
  }
  if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  ImGui::PushID(label);
  for (size_t i = 0; i < array.size(); ++i) {
    const auto& entry = array[i];
    ImGui::PushID(static_cast<int>(i));
    if (entry.is_object()) {
      const std::string title = BuildToolOutputEntryTitle(entry);
      if (!title.empty()) {
        gui::ColoredText(title.c_str(), gui::GetOnSurfaceVec4());
      }
      DrawToolOutputEntryActions(entry, actions);
      if (!title.empty() || !entry.empty()) {
        ImGui::Spacing();
      }
      DrawJsonObjectFields(entry);
    } else {
      ImGui::BulletText("%s", JsonValueToDisplayString(entry).c_str());
    }
    if (i + 1 < array.size()) {
      ImGui::Separator();
    }
    ImGui::PopID();
  }
  ImGui::PopID();
}

std::string BuildSelectionContextSummary(const SelectionContext& selection) {
  if (selection.type == SelectionType::kNone) {
    return "";
  }
  std::string context =
      absl::StrFormat("Selection: %s", GetSelectionTypeName(selection.type));
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

void DrawWorkflowSummaryCard(
    const char* title, const char* fallback_icon,
    const ProjectWorkflowStatus& status,
    const std::function<void()>& cancel_callback = {}) {
  if (!status.visible) {
    return;
  }

  gui::ColoredTextF(workflow::WorkflowColor(status.state), "%s %s",
                    workflow::WorkflowIcon(status, fallback_icon), title);
  ImGui::TextWrapped("%s", status.summary.empty() ? status.label.c_str()
                                                  : status.summary.c_str());
  if (!status.detail.empty()) {
    ImGui::TextWrapped("%s", status.detail.c_str());
  }
  if (!status.output_tail.empty()) {
    ImGui::TextWrapped("%s", status.output_tail.c_str());
  }
  if (status.can_cancel && cancel_callback) {
    if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel Build")) {
      cancel_callback();
    }
  }
}

void DrawWorkflowPreviewEntry(
    const ProjectWorkflowHistoryEntry& entry,
    const workflow::WorkflowActionCallbacks& callbacks) {
  gui::ColoredTextF(workflow::WorkflowColor(entry.status.state), "%s %s",
                    workflow::WorkflowIcon(
                        entry.status, entry.kind == "Run" ? ICON_MD_PLAY_ARROW
                                                          : ICON_MD_BUILD),
                    entry.kind.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("%s",
                      workflow::FormatHistoryTime(entry.timestamp).c_str());
  ImGui::TextWrapped("%s", entry.status.summary.empty()
                               ? entry.status.label.c_str()
                               : entry.status.summary.c_str());
  if (!entry.status.output_tail.empty()) {
    ImGui::TextWrapped("%s", entry.status.output_tail.c_str());
  }
  workflow::DrawHistoryActionRow(
      entry, callbacks, {.show_open_output = true, .show_copy_log = true});
}

const std::array<RightDrawerManager::PanelType, 7> kRightPanelSwitchOrder = {
    RightDrawerManager::PanelType::kProject,
    RightDrawerManager::PanelType::kProperties,
    RightDrawerManager::PanelType::kAgentChat,
    RightDrawerManager::PanelType::kProposals,
    RightDrawerManager::PanelType::kNotifications,
    RightDrawerManager::PanelType::kHelp,
    RightDrawerManager::PanelType::kSettings,
};

int FindRightPanelIndex(RightDrawerManager::PanelType type) {
  for (size_t i = 0; i < kRightPanelSwitchOrder.size(); ++i) {
    if (kRightPanelSwitchOrder[i] == type) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

RightDrawerManager::PanelType StepRightPanel(
    RightDrawerManager::PanelType current, int direction) {
  if (kRightPanelSwitchOrder.empty()) {
    return RightDrawerManager::PanelType::kNone;
  }
  int index = FindRightPanelIndex(current);
  if (index < 0) {
    index = 0;
  }
  const int size = static_cast<int>(kRightPanelSwitchOrder.size());
  const int next = (index + direction + size) % size;
  return kRightPanelSwitchOrder[static_cast<size_t>(next)];
}

const char* GetPanelShortcutAction(RightDrawerManager::PanelType type) {
  switch (type) {
    case RightDrawerManager::PanelType::kProject:
      return "View: Toggle Project Panel";
    case RightDrawerManager::PanelType::kAgentChat:
      return "View: Toggle AI Agent Panel";
    case RightDrawerManager::PanelType::kProposals:
      return "View: Toggle Proposals Panel";
    case RightDrawerManager::PanelType::kSettings:
      return "View: Toggle Settings Panel";
    case RightDrawerManager::PanelType::kHelp:
      return "View: Toggle Help Panel";
    case RightDrawerManager::PanelType::kNotifications:
      return "View: Toggle Notifications Panel";
    case RightDrawerManager::PanelType::kProperties:
      return "View: Toggle Properties Panel";
    case RightDrawerManager::PanelType::kNone:
    default:
      return "";
  }
}

}  // namespace

const char* GetPanelTypeName(RightDrawerManager::PanelType type) {
  switch (type) {
    case RightDrawerManager::PanelType::kNone:
      return "None";
    case RightDrawerManager::PanelType::kAgentChat:
      return "AI Agent";
    case RightDrawerManager::PanelType::kProposals:
      return "Proposals";
    case RightDrawerManager::PanelType::kSettings:
      return "Settings";
    case RightDrawerManager::PanelType::kHelp:
      return "Help";
    case RightDrawerManager::PanelType::kNotifications:
      return "Notifications";
    case RightDrawerManager::PanelType::kProperties:
      return "Properties";
    case RightDrawerManager::PanelType::kProject:
      return "Project";
    case RightDrawerManager::PanelType::kToolOutput:
      return "Tool Output";
    default:
      return "Unknown";
  }
}

const char* GetPanelTypeIcon(RightDrawerManager::PanelType type) {
  switch (type) {
    case RightDrawerManager::PanelType::kNone:
      return "";
    case RightDrawerManager::PanelType::kAgentChat:
      return ICON_MD_SMART_TOY;
    case RightDrawerManager::PanelType::kProposals:
      return ICON_MD_DESCRIPTION;
    case RightDrawerManager::PanelType::kSettings:
      return ICON_MD_SETTINGS;
    case RightDrawerManager::PanelType::kHelp:
      return ICON_MD_HELP;
    case RightDrawerManager::PanelType::kNotifications:
      return ICON_MD_NOTIFICATIONS;
    case RightDrawerManager::PanelType::kProperties:
      return ICON_MD_LIST_ALT;
    case RightDrawerManager::PanelType::kProject:
      return ICON_MD_FOLDER_SPECIAL;
    case RightDrawerManager::PanelType::kToolOutput:
      return ICON_MD_ACCOUNT_TREE;
    default:
      return ICON_MD_HELP;
  }
}

std::string RightDrawerManager::PanelTypeKey(PanelType type) {
  switch (type) {
    case PanelType::kAgentChat:
      return "agent_chat";
    case PanelType::kProposals:
      return "proposals";
    case PanelType::kSettings:
      return "settings";
    case PanelType::kHelp:
      return "help";
    case PanelType::kNotifications:
      return "notifications";
    case PanelType::kProperties:
      return "properties";
    case PanelType::kProject:
      return "project";
    case PanelType::kToolOutput:
      return "tool_output";
    case PanelType::kNone:
    default:
      return "none";
  }
}

void RightDrawerManager::ToggleDrawer(PanelType type) {
  if (active_panel_ == type) {
    CloseDrawer();
  } else {
    // Opens the requested panel (also handles re-opening during close animation)
    OpenDrawer(type);
  }
}

void RightDrawerManager::SetToolOutput(std::string title, std::string query,
                                       std::string content,
                                       ToolOutputActions actions) {
  tool_output_title_ = std::move(title);
  tool_output_query_ = std::move(query);
  tool_output_content_ = std::move(content);
  tool_output_actions_ = std::move(actions);
}

bool RightDrawerManager::IsDrawerExpanded() const {
  return active_panel_ != PanelType::kNone || closing_;
}

void RightDrawerManager::OpenDrawer(PanelType type) {
  // If we were closing, cancel the close animation
  closing_ = false;
  closing_panel_ = PanelType::kNone;

  active_panel_ = type;
  animating_ = true;
  animation_target_ = 1.0f;

  // Check if animations are enabled
  if (!gui::GetAnimator().IsEnabled()) {
    panel_animation_ = 1.0f;
    animating_ = false;
  }
  // Otherwise keep current panel_animation_ for smooth transition
}

void RightDrawerManager::CloseDrawer() {
  if (!gui::GetAnimator().IsEnabled()) {
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

void RightDrawerManager::CycleDrawer(int direction) {
  if (direction == 0) {
    return;
  }

  const PanelType current_panel =
      (active_panel_ != PanelType::kNone) ? active_panel_ : closing_panel_;
  if (current_panel == PanelType::kNone) {
    return;
  }

  const int step = direction > 0 ? 1 : -1;
  OpenDrawer(StepRightPanel(current_panel, step));
}

void RightDrawerManager::OnHostVisibilityChanged(bool visible) {
  // Snap transition state to a stable endpoint. This avoids stale intermediate
  // frames being composited when the OS moves the app across spaces.
  (void)visible;
  closing_ = false;
  closing_panel_ = PanelType::kNone;
  animating_ = false;

  panel_animation_ = (active_panel_ == PanelType::kNone) ? 0.0f : 1.0f;
  animation_target_ = panel_animation_;
}

float RightDrawerManager::GetDrawerWidth() const {
  // Determine which panel to measure: active panel, or the one being closed
  PanelType effective_panel = active_panel_;
  if (effective_panel == PanelType::kNone && closing_) {
    effective_panel = closing_panel_;
  }
  if (effective_panel == PanelType::kNone) {
    return 0.0f;
  }

  ImGuiContext* context = ImGui::GetCurrentContext();
  if (!context) {
    return GetConfiguredPanelWidth(effective_panel) * panel_animation_;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (!viewport) {
    return GetConfiguredPanelWidth(effective_panel) * panel_animation_;
  }

  const float vp_width = viewport->WorkSize.x;
  const float width = GetClampedPanelWidth(effective_panel, vp_width);

  // Scale by animation progress for smooth docking space adjustment
  return width * panel_animation_;
}

void RightDrawerManager::SetDrawerWidth(PanelType type, float width) {
  if (type == PanelType::kNone) {
    return;
  }
  float viewport_width = 0.0f;
  if (const ImGuiViewport* viewport = ImGui::GetMainViewport()) {
    viewport_width = viewport->WorkSize.x;
  }
  if (viewport_width <= 0.0f && ImGui::GetCurrentContext()) {
    viewport_width = ImGui::GetIO().DisplaySize.x;
  }
  const auto limits = GetPanelSizeLimits(type);
  float clamped = std::max(limits.min_width, width);
  if (viewport_width > 0.0f) {
    const float ratio = viewport_width < 768.0f
                            ? std::max(0.88f, limits.max_width_ratio)
                            : limits.max_width_ratio;
    const float max_width = std::max(limits.min_width, viewport_width * ratio);
    clamped = std::clamp(clamped, limits.min_width, max_width);
  }

  float* target = nullptr;
  switch (type) {
    case PanelType::kAgentChat:
      target = &agent_chat_width_;
      break;
    case PanelType::kProposals:
      target = &proposals_width_;
      break;
    case PanelType::kSettings:
      target = &settings_width_;
      break;
    case PanelType::kHelp:
      target = &help_width_;
      break;
    case PanelType::kNotifications:
      target = &notifications_width_;
      break;
    case PanelType::kProperties:
      target = &properties_width_;
      break;
    case PanelType::kProject:
      target = &project_width_;
      break;
    case PanelType::kToolOutput:
      target = &tool_output_width_;
      break;
    default:
      break;
  }
  if (!target) {
    return;
  }
  if (std::abs(*target - clamped) < 0.5f) {
    return;
  }
  *target = clamped;
#if !defined(NDEBUG)
  LOG_INFO("RightDrawerManager",
           "SetDrawerWidth type=%d requested=%.1f clamped=%.1f",
           static_cast<int>(type), width, clamped);
#endif
  NotifyPanelWidthChanged(type, *target);
}

void RightDrawerManager::ResetDrawerWidths() {
  SetDrawerWidth(
      PanelType::kAgentChat,
      GetDefaultDrawerWidth(PanelType::kAgentChat, active_editor_type_));
  SetDrawerWidth(
      PanelType::kProposals,
      GetDefaultDrawerWidth(PanelType::kProposals, active_editor_type_));
  SetDrawerWidth(
      PanelType::kSettings,
      GetDefaultDrawerWidth(PanelType::kSettings, active_editor_type_));
  SetDrawerWidth(PanelType::kHelp,
                 GetDefaultDrawerWidth(PanelType::kHelp, active_editor_type_));
  SetDrawerWidth(
      PanelType::kNotifications,
      GetDefaultDrawerWidth(PanelType::kNotifications, active_editor_type_));
  SetDrawerWidth(
      PanelType::kProperties,
      GetDefaultDrawerWidth(PanelType::kProperties, active_editor_type_));
  SetDrawerWidth(
      PanelType::kProject,
      GetDefaultDrawerWidth(PanelType::kProject, active_editor_type_));
  SetDrawerWidth(
      PanelType::kToolOutput,
      GetDefaultDrawerWidth(PanelType::kToolOutput, active_editor_type_));
}

float RightDrawerManager::GetDefaultDrawerWidth(PanelType type,
                                                EditorType editor) {
  switch (type) {
    case PanelType::kAgentChat:
      return std::max(gui::UIConfig::kPanelWidthAgentChat, 480.0f);
    case PanelType::kProposals:
      return std::max(gui::UIConfig::kPanelWidthProposals, 440.0f);
    case PanelType::kSettings:
      return std::max(gui::UIConfig::kPanelWidthSettings, 380.0f);
    case PanelType::kHelp:
      return std::max(gui::UIConfig::kPanelWidthHelp, 380.0f);
    case PanelType::kNotifications:
      return std::max(gui::UIConfig::kPanelWidthNotifications, 380.0f);
    case PanelType::kProperties:
      // Property panel can be wider in certain editors.
      if (editor == EditorType::kDungeon) {
        return 440.0f;
      }
      return std::max(gui::UIConfig::kPanelWidthProperties, 400.0f);
    case PanelType::kProject:
      return std::max(gui::UIConfig::kPanelWidthProject, 420.0f);
    case PanelType::kToolOutput:
      return 460.0f;
    default:
      return std::max(gui::UIConfig::kPanelWidthMedium, 380.0f);
  }
}

void RightDrawerManager::SetPanelSizeLimits(PanelType type,
                                            const PanelSizeLimits& limits) {
  if (type == PanelType::kNone) {
    return;
  }
  PanelSizeLimits normalized = limits;
  normalized.min_width =
      std::max(gui::UIConfig::kPanelMinWidthAbsolute, normalized.min_width);
  normalized.max_width_ratio =
      std::clamp(normalized.max_width_ratio, 0.25f, 0.95f);
  panel_size_limits_[PanelTypeKey(type)] = normalized;
}

RightDrawerManager::PanelSizeLimits RightDrawerManager::GetPanelSizeLimits(
    PanelType type) const {
  auto it = panel_size_limits_.find(PanelTypeKey(type));
  if (it != panel_size_limits_.end()) {
    return it->second;
  }

  PanelSizeLimits defaults;
  switch (type) {
    case PanelType::kAgentChat:
      defaults.min_width = gui::UIConfig::kPanelMinWidthAgentChat;
      defaults.max_width_ratio = 0.90f;
      break;
    case PanelType::kProposals:
      defaults.min_width = gui::UIConfig::kPanelMinWidthProposals;
      defaults.max_width_ratio = 0.86f;
      break;
    case PanelType::kSettings:
      defaults.min_width = gui::UIConfig::kPanelMinWidthSettings;
      defaults.max_width_ratio = 0.80f;
      break;
    case PanelType::kHelp:
      defaults.min_width = gui::UIConfig::kPanelMinWidthHelp;
      defaults.max_width_ratio = 0.80f;
      break;
    case PanelType::kNotifications:
      defaults.min_width = gui::UIConfig::kPanelMinWidthNotifications;
      defaults.max_width_ratio = 0.82f;
      break;
    case PanelType::kProperties:
      defaults.min_width = gui::UIConfig::kPanelMinWidthProperties;
      defaults.max_width_ratio = 0.90f;
      break;
    case PanelType::kProject:
      defaults.min_width = gui::UIConfig::kPanelMinWidthProject;
      defaults.max_width_ratio = 0.86f;
      break;
    case PanelType::kToolOutput:
      defaults.min_width = 360.0f;
      defaults.max_width_ratio = 0.88f;
      break;
    case PanelType::kNone:
    default:
      break;
  }
  return defaults;
}

float RightDrawerManager::GetConfiguredPanelWidth(PanelType type) const {
  switch (type) {
    case PanelType::kAgentChat:
      return agent_chat_width_;
    case PanelType::kProposals:
      return proposals_width_;
    case PanelType::kSettings:
      return settings_width_;
    case PanelType::kHelp:
      return help_width_;
    case PanelType::kNotifications:
      return notifications_width_;
    case PanelType::kProperties:
      return properties_width_;
    case PanelType::kProject:
      return project_width_;
    case PanelType::kToolOutput:
      return tool_output_width_;
    case PanelType::kNone:
    default:
      return 0.0f;
  }
}

float RightDrawerManager::GetClampedPanelWidth(PanelType type,
                                               float viewport_width) const {
  float width = GetConfiguredPanelWidth(type);
  if (width <= 0.0f) {
    return width;
  }
  const auto limits = GetPanelSizeLimits(type);
  const float ratio = viewport_width < 768.0f
                          ? std::max(0.88f, limits.max_width_ratio)
                          : limits.max_width_ratio;
  const float max_width = std::max(limits.min_width, viewport_width * ratio);
  return std::clamp(width, limits.min_width, max_width);
}

void RightDrawerManager::NotifyPanelWidthChanged(PanelType type, float width) {
  if (on_panel_width_changed_) {
    on_panel_width_changed_(type, width);
  }
}

std::unordered_map<std::string, float>
RightDrawerManager::SerializeDrawerWidths() const {
  return {
      {PanelTypeKey(PanelType::kAgentChat), agent_chat_width_},
      {PanelTypeKey(PanelType::kProposals), proposals_width_},
      {PanelTypeKey(PanelType::kSettings), settings_width_},
      {PanelTypeKey(PanelType::kHelp), help_width_},
      {PanelTypeKey(PanelType::kNotifications), notifications_width_},
      {PanelTypeKey(PanelType::kProperties), properties_width_},
      {PanelTypeKey(PanelType::kProject), project_width_},
      {PanelTypeKey(PanelType::kToolOutput), tool_output_width_},
  };
}

void RightDrawerManager::RestoreDrawerWidths(
    const std::unordered_map<std::string, float>& widths) {
#if !defined(NDEBUG)
  LOG_INFO("RightDrawerManager",
           "RestoreDrawerWidths: %zu entries from settings", widths.size());
#endif
  auto apply = [&](PanelType type) {
    auto it = widths.find(PanelTypeKey(type));
    if (it != widths.end()) {
      SetDrawerWidth(type, it->second);
    }
  };
  apply(PanelType::kAgentChat);
  apply(PanelType::kProposals);
  apply(PanelType::kSettings);
  apply(PanelType::kHelp);
  apply(PanelType::kNotifications);
  apply(PanelType::kProperties);
  apply(PanelType::kProject);
  apply(PanelType::kToolOutput);
}

void RightDrawerManager::Draw() {
  // Nothing to draw if no panel is active and no close animation running
  if (active_panel_ == PanelType::kNone && !closing_) {
    return;
  }

  // Handle Escape key to close panel
  if (active_panel_ != PanelType::kNone &&
      ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    CloseDrawer();
    // Don't return — we need to start drawing the close animation this frame
    if (!closing_)
      return;
  }

  const bool animations_enabled = gui::GetAnimator().IsEnabled();
  if (!animations_enabled && animating_) {
    panel_animation_ = animation_target_;
    animating_ = false;
    if (closing_ && animation_target_ == 0.0f) {
      closing_ = false;
      closing_panel_ = PanelType::kNone;
      return;
    }
  }

  // Advance animation
  if (animating_ && animations_enabled) {
    // Clamp dt to avoid giant interpolation jumps after focus/space changes.
    float delta_time = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 1.0f / 20.0f);
    float speed = gui::UIConfig::kAnimationSpeed;
    switch (gui::GetAnimator().motion_profile()) {
      case gui::MotionProfile::kSnappy:
        speed *= 1.20f;
        break;
      case gui::MotionProfile::kRelaxed:
        speed *= 0.75f;
        break;
      case gui::MotionProfile::kStandard:
      default:
        break;
    }
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
  const float bottom_safe = gui::LayoutHelpers::GetSafeAreaInsets().bottom;
  const float viewport_height =
      std::max(0.0f, viewport->WorkSize.y - top_inset - bottom_safe);

  // Keep full-width state explicit so drag-resize and animation remain stable.
  const float full_width =
      (draw_panel == PanelType::kNone)
          ? 0.0f
          : GetClampedPanelWidth(draw_panel, viewport_width);
  const float animated_width = full_width * panel_animation_;

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
  ImGui::SetNextWindowPos(ImVec2(panel_x, viewport->WorkPos.y + top_inset));
  ImGui::SetNextWindowSize(ImVec2(full_width, viewport_height));

  gui::StyledWindow panel("##RightPanel",
                          {.bg = panel_bg,
                           .border = panel_border,
                           .padding = ImVec2(0.0f, 0.0f),
                           .border_size = 1.0f},
                          nullptr, panel_flags);
  if (panel) {
    const char* panel_title = GetPanelTypeName(draw_panel);
    const char* panel_icon = GetPanelTypeIcon(draw_panel);
    if (draw_panel == PanelType::kToolOutput && !tool_output_title_.empty()) {
      panel_title = tool_output_title_.c_str();
    }
    // Draw enhanced panel header
    DrawPanelHeader(panel_title, panel_icon);

    // Content area with padding and minimum height so content never collapses
    gui::StyleVarGuard content_padding(
        ImGuiStyleVar_WindowPadding,
        ImVec2(gui::UIConfig::kPanelPaddingLarge,
               gui::UIConfig::kPanelPaddingMedium));
    const bool panel_content_open = gui::LayoutHelpers::BeginContentChild(
        "##PanelContent", ImVec2(0.0f, gui::UIConfig::kContentMinHeightList),
        ImGuiChildFlags_AlwaysUseWindowPadding);
    if (panel_content_open) {
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
        case PanelType::kToolOutput:
          DrawToolOutputPanel();
          break;
        default:
          break;
      }
    }
    gui::LayoutHelpers::EndContentChild();

    // VSCode-style splitter: drag from the left edge to resize.
    if (!closing_ && active_panel_ != PanelType::kNone) {
      const float handle_width = gui::UIConfig::kSplitterWidth;
      const ImVec2 win_pos = ImGui::GetWindowPos();
      const float win_height = ImGui::GetWindowHeight();
      ImGui::SetCursorScreenPos(
          ImVec2(win_pos.x - handle_width * 0.5f, win_pos.y));
      ImGui::InvisibleButton("##RightPanelResizeHandle",
                             ImVec2(handle_width, win_height));
      const bool handle_hovered = ImGui::IsItemHovered();
      const bool handle_active = ImGui::IsItemActive();
      if (handle_hovered || handle_active) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
      }
      if (handle_hovered &&
          ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        SetDrawerWidth(active_panel_, GetDefaultDrawerWidth(
                                          active_panel_, active_editor_type_));
      }
      if (handle_active) {
        const float new_width = GetConfiguredPanelWidth(active_panel_) -
                                ImGui::GetIO().MouseDelta.x;
        SetDrawerWidth(active_panel_, new_width);
        ImGui::SetTooltip("Width: %.0f px",
                          GetConfiguredPanelWidth(active_panel_));
      }

      ImVec4 handle_color = gui::GetOutlineVec4();
      handle_color.w = handle_active ? 0.95f : (handle_hovered ? 0.72f : 0.35f);
      ImGui::GetWindowDrawList()->AddLine(
          ImVec2(win_pos.x, win_pos.y),
          ImVec2(win_pos.x, win_pos.y + win_height),
          ImGui::GetColorU32(handle_color), handle_active ? 2.0f : 1.0f);
    }
  }
}

void RightDrawerManager::DrawPanelHeader(const char* title, const char* icon) {
  const float header_height = gui::UIConfig::kPanelHeaderHeight;
  const float padding = gui::UIConfig::kPanelPaddingLarge;

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

  const PanelType current_panel =
      (active_panel_ != PanelType::kNone) ? active_panel_ : closing_panel_;
  const std::string previous_shortcut =
      GetShortcutLabel("View: Previous Right Panel", "");
  const std::string next_shortcut =
      GetShortcutLabel("View: Next Right Panel", "");
  const std::string previous_tooltip =
      previous_shortcut.empty() ? "Previous right panel"
                                : absl::StrFormat("Previous right panel (%s)",
                                                  previous_shortcut.c_str());
  const std::string next_tooltip =
      next_shortcut.empty()
          ? "Next right panel"
          : absl::StrFormat("Next right panel (%s)", next_shortcut.c_str());

  ImGui::SameLine(0.0f, gui::UIConfig::kHeaderButtonSpacing);
  if (gui::TransparentIconButton(ICON_MD_CHEVRON_LEFT, gui::IconSize::Small(),
                                 previous_tooltip.c_str(), false,
                                 gui::GetTextSecondaryVec4(), "right_sidebar",
                                 "switch_panel_prev")) {
    CycleToPreviousDrawer();
  }

  ImGui::SameLine(0.0f, gui::UIConfig::kHeaderButtonGap);
  if (gui::TransparentIconButton(
          ICON_MD_SWAP_HORIZ, gui::IconSize::Small(), "Panel switcher", false,
          gui::GetTextSecondaryVec4(), "right_sidebar", "switch_panel_menu")) {
    ImGui::OpenPopup("##RightPanelSwitcher");
  }

  ImGui::SameLine(0.0f, gui::UIConfig::kHeaderButtonGap);
  if (gui::TransparentIconButton(ICON_MD_CHEVRON_RIGHT, gui::IconSize::Small(),
                                 next_tooltip.c_str(), false,
                                 gui::GetTextSecondaryVec4(), "right_sidebar",
                                 "switch_panel_next")) {
    CycleToNextDrawer();
  }

  if (ImGui::BeginPopup("##RightPanelSwitcher")) {
    for (PanelType panel_type : kRightPanelSwitchOrder) {
      std::string label = absl::StrFormat("%s %s", GetPanelTypeIcon(panel_type),
                                          GetPanelTypeName(panel_type));
      const char* shortcut_action = GetPanelShortcutAction(panel_type);
      std::string shortcut;
      if (shortcut_action[0] != '\0') {
        shortcut = GetShortcutLabel(shortcut_action, "");
        if (shortcut == "Unassigned") {
          shortcut.clear();
        }
      }
      if (ImGui::MenuItem(label.c_str(),
                          shortcut.empty() ? nullptr : shortcut.c_str(),
                          current_panel == panel_type)) {
        OpenDrawer(panel_type);
      }
    }
    ImGui::EndPopup();
  }

  // Right-aligned buttons
  const ImVec2 chrome_button_size = gui::IconSize::Toolbar();
  const float button_size = chrome_button_size.x;
  const float button_y =
      header_min.y + (header_height - chrome_button_size.y) * 0.5f;
  float current_x = ImGui::GetWindowWidth() - button_size - padding;

  // Close button
  ImGui::SetCursorScreenPos(ImVec2(header_min.x + current_x, button_y));
  if (gui::TransparentIconButton(
          ICON_MD_CANCEL, chrome_button_size, "Close Drawer (Esc)", false,
          ImVec4(0, 0, 0, 0), "right_sidebar", "close_panel")) {
    CloseDrawer();
  }

  // Lock Toggle (Only for Properties Panel)
  if (active_panel_ == PanelType::kProperties) {
    current_x -= (button_size + 4.0f);
    ImGui::SetCursorScreenPos(ImVec2(header_min.x + current_x, button_y));

    if (gui::TransparentIconButton(
            properties_locked_ ? ICON_MD_LOCK : ICON_MD_LOCK_OPEN,
            chrome_button_size,
            properties_locked_ ? "Unlock Selection" : "Lock Selection",
            properties_locked_, ImVec4(0, 0, 0, 0), "right_sidebar",
            "lock_selection")) {
      properties_locked_ = !properties_locked_;
    }
  }

  // Move cursor past the header
  ImGui::SetCursorPosY(header_height + 8.0f);
}

// =============================================================================
// Panel Styling Helpers
// =============================================================================

bool RightDrawerManager::BeginPanelSection(const char* label, const char* icon,
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

void RightDrawerManager::EndPanelSection() {
  ImGui::Unindent(4.0f);
  ImGui::TreePop();
  ImGui::Spacing();
}

void RightDrawerManager::DrawPanelDivider() {
  ImGui::Spacing();
  {
    gui::StyleColorGuard sep_color(ImGuiCol_Separator, gui::GetOutlineVec4());
    ImGui::Separator();
  }
  ImGui::Spacing();
}

void RightDrawerManager::DrawPanelLabel(const char* label) {
  gui::ColoredText(label, gui::GetTextSecondaryVec4());
}

void RightDrawerManager::DrawPanelValue(const char* label, const char* value) {
  gui::ColoredTextF(gui::GetTextSecondaryVec4(), "%s:", label);
  ImGui::SameLine();
  ImGui::TextUnformatted(value);
}

void RightDrawerManager::DrawPanelDescription(const char* text) {
  gui::StyleColorGuard desc_color(ImGuiCol_Text, gui::GetTextDisabledVec4());
  ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
  ImGui::TextWrapped("%s", text);
  ImGui::PopTextWrapPos();
}

std::string RightDrawerManager::GetShortcutLabel(
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

void RightDrawerManager::DrawShortcutRow(const std::string& action,
                                         const char* description,
                                         const std::string& fallback) {
  std::string label = GetShortcutLabel(action, fallback);
  DrawPanelValue(label.c_str(), description);
}

// =============================================================================
// Panel Content Drawing
// =============================================================================

void RightDrawerManager::DrawAgentChatPanel() {
#ifdef YAZE_BUILD_AGENT_UI
  if (!agent_chat_) {
    gui::ColoredText(ICON_MD_SMART_TOY " AI Agent Not Available",
                     gui::GetTextSecondaryVec4());
    ImGui::Spacing();
    DrawPanelDescription(
        "The AI Agent is not initialized. "
        "Open the AI Agent from View menu or use Ctrl+Shift+A.");
    return;
  }

  agent_chat_->set_active(true);

  const float action_bar_height = ImGui::GetFrameHeightWithSpacing() + 8.0f;
  const float content_height =
      std::max(gui::UIConfig::kContentMinHeightChat,
               ImGui::GetContentRegionAvail().y - action_bar_height);

  if (ImGui::BeginChild("AgentChatBody", ImVec2(0, content_height), true)) {
    agent_chat_->Draw(0.0f);
  }
  ImGui::EndChild();

  gui::StyleVarGuard action_spacing(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));
  const ImVec2 action_size = gui::IconSize::Toolbar();
  const ImVec4 transparent_bg(0, 0, 0, 0);

  if (proposal_drawer_) {
    if (gui::TransparentIconButton(ICON_MD_DESCRIPTION, action_size,
                                   "Open Proposals", false, transparent_bg,
                                   "agent_sidebar", "open_proposals")) {
      OpenDrawer(PanelType::kProposals);
    }
    ImGui::SameLine();
  }

  if (gui::TransparentIconButton(ICON_MD_DELETE_FOREVER, action_size,
                                 "Clear Chat History", false, transparent_bg,
                                 "agent_sidebar", "clear_history")) {
    agent_chat_->ClearHistory();
  }
  ImGui::SameLine();

  if (gui::TransparentIconButton(ICON_MD_FILE_DOWNLOAD, action_size,
                                 "Save Chat History", false, transparent_bg,
                                 "agent_sidebar", "save_history")) {
    agent_chat_->SaveHistory(ResolveAgentChatHistoryPath());
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

bool RightDrawerManager::DrawAgentQuickActions() {
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
    actions.push_back(
        {"Suggest fixes",
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

void RightDrawerManager::DrawProposalsPanel() {
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

void RightDrawerManager::DrawSettingsPanel() {
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

void RightDrawerManager::DrawHelpPanel() {
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

void RightDrawerManager::DrawEditorContextHeader() {
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

void RightDrawerManager::DrawGlobalShortcuts() {
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
  DrawPanelValue("Esc", "Close Drawer");
  ImGui::Unindent(8.0f);
  ImGui::Spacing();
}

void RightDrawerManager::DrawEditorSpecificShortcuts() {
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

void RightDrawerManager::DrawEditorSpecificHelp() {
  switch (active_editor_type_) {
    case EditorType::kOverworld: {
      gui::StyleColorGuard text_color(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(ImGuiCol_Text));
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
      gui::StyleColorGuard text_color(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(ImGuiCol_Text));
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
      gui::StyleColorGuard text_color(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(ImGuiCol_Text));
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
      gui::StyleColorGuard text_color(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(ImGuiCol_Text));
      ImGui::Bullet();
      ImGui::TextWrapped("Edit overworld, dungeon, and sprite palettes");
      ImGui::Bullet();
      ImGui::TextWrapped("Use Quick Access for color harmony tools");
      ImGui::Bullet();
      ImGui::TextWrapped("Changes update in real-time across all editors");
    } break;

    case EditorType::kMusic: {
      gui::StyleColorGuard text_color(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(ImGuiCol_Text));
      ImGui::Bullet();
      ImGui::TextWrapped("Browse songs in the Song Browser");
      ImGui::Bullet();
      ImGui::TextWrapped("Use the tracker for playback control");
      ImGui::Bullet();
      ImGui::TextWrapped("Edit instruments and BRR samples");
    } break;

    case EditorType::kMessage: {
      gui::StyleColorGuard text_color(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(ImGuiCol_Text));
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

void RightDrawerManager::DrawQuickActionButtons() {
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
    if (ImGui::Button(ICON_MD_FORUM " Join Discord", ImVec2(button_width, 0))) {
      gui::OpenUrl("https://discord.gg/zU5qDm8MZg");
    }
  }
}

void RightDrawerManager::DrawAboutSection() {
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

void RightDrawerManager::DrawNotificationsPanel() {
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

  const auto build_status = ContentRegistry::Context::build_workflow_status();
  const auto run_status = ContentRegistry::Context::run_workflow_status();
  const auto workflow_history = ContentRegistry::Context::workflow_history();
  workflow::WorkflowActionCallbacks workflow_callbacks;
  workflow_callbacks.start_build =
      ContentRegistry::Context::start_build_workflow_callback();
  workflow_callbacks.run_project =
      ContentRegistry::Context::run_project_workflow_callback();
  workflow_callbacks.show_output =
      ContentRegistry::Context::show_workflow_output_callback();
  const auto cancel_build =
      ContentRegistry::Context::cancel_build_workflow_callback();

  if (build_status.visible || run_status.visible || !workflow_history.empty()) {
    DrawPanelLabel("Workflow Activity");
    if (build_status.visible) {
      DrawWorkflowSummaryCard("Build", ICON_MD_BUILD, build_status,
                              cancel_build);
      ImGui::Spacing();
    }
    if (run_status.visible) {
      DrawWorkflowSummaryCard("Run", ICON_MD_PLAY_ARROW, run_status);
      ImGui::Spacing();
    }
    if (!workflow_history.empty()) {
      DrawPanelLabel("Recent Workflow History");
      const auto preview_entries =
          workflow::SelectWorkflowPreviewEntries(workflow_history, 3);
      for (size_t i = 0; i < preview_entries.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        DrawWorkflowPreviewEntry(preview_entries[i], workflow_callbacks);
        ImGui::PopID();
        if (i + 1 < preview_entries.size()) {
          ImGui::Separator();
        }
      }
      if (workflow_history.size() > preview_entries.size()) {
        ImGui::Spacing();
        ImGui::TextDisabled("+%zu more entries available in Workflow Output",
                            workflow_history.size() - preview_entries.size());
        if (workflow_callbacks.show_output) {
          if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW
                                 " View Full History##workflow_view_full")) {
            workflow_callbacks.show_output();
          }
        }
      }
    }
    DrawPanelDivider();
  }

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

  // Scrollable notification list (minimum height so list never collapses)
  const bool notification_list_open = gui::LayoutHelpers::BeginContentChild(
      "##NotificationList", ImVec2(0.0f, gui::UIConfig::kContentMinHeightList),
      ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
  if (notification_list_open) {
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
      auto diff_sec = std::chrono::duration_cast<std::chrono::seconds>(
                          now - entry.timestamp)
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
  }
  gui::LayoutHelpers::EndContentChild();
}

void RightDrawerManager::DrawPropertiesPanel() {
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

void RightDrawerManager::DrawProjectPanel() {
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

void RightDrawerManager::DrawToolOutputPanel() {
  if (!tool_output_query_.empty()) {
    gui::ColoredText(ICON_MD_TERMINAL " Query", gui::GetPrimaryVec4());
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CONTENT_COPY " Copy")) {
      ImGui::SetClipboardText(tool_output_query_.c_str());
    }
    ImGui::TextWrapped("%s", tool_output_query_.c_str());
    DrawPanelDivider();
  }

  if (tool_output_content_.empty()) {
    gui::ColoredText(ICON_MD_INFO " No tool output",
                     gui::GetTextSecondaryVec4());
    DrawPanelDescription(
        "Run a project-graph query from the editor to inspect its output "
        "here.");
    return;
  }

  Json parsed;
  const bool has_json = TryParseToolOutputJson(tool_output_content_, &parsed);

  gui::ColoredText(ICON_MD_ACCOUNT_TREE " Result", gui::GetPrimaryVec4());
  if (ImGui::SmallButton(ICON_MD_CONTENT_COPY " Copy Result")) {
    ImGui::SetClipboardText(tool_output_content_.c_str());
  }

  if (has_json) {
    if (BeginPanelSection("Summary", ICON_MD_INFO, true)) {
      ImGui::PushID("summary");
      DrawToolOutputEntryActions(parsed, tool_output_actions_);
      if (!parsed.empty()) {
        ImGui::Spacing();
      }
      DrawJsonObjectFields(parsed);
      ImGui::PopID();
      EndPanelSection();
    }
    if (parsed.contains("source") && parsed["source"].is_object() &&
        BeginPanelSection("Resolved Source", ICON_MD_CODE, true)) {
      ImGui::PushID("resolved_source");
      DrawToolOutputEntryActions(parsed["source"], tool_output_actions_);
      if (!parsed["source"].empty()) {
        ImGui::Spacing();
      }
      DrawJsonObjectFields(parsed["source"]);
      ImGui::PopID();
      EndPanelSection();
    }
    DrawJsonObjectArraySection("Matching Symbols", parsed["matching_symbols"],
                               tool_output_actions_);
    DrawJsonObjectArraySection("Sources", parsed["sources"],
                               tool_output_actions_);
    DrawJsonObjectArraySection("Hooks", parsed["hooks"], tool_output_actions_);
    DrawJsonObjectArraySection("Writes", parsed["writes"],
                               tool_output_actions_);
    DrawJsonObjectArraySection("Banks", parsed["banks"], tool_output_actions_);
    DrawJsonObjectArraySection("Symbols", parsed["symbols"],
                               tool_output_actions_);
  }

  if (ImGui::CollapsingHeader("Raw Output",
                              has_json ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::BeginChild("##tool_output_result", ImVec2(0.0f, 220.0f), true)) {
      ImGui::TextUnformatted(tool_output_content_.c_str());
    }
    ImGui::EndChild();
  }
}

bool RightDrawerManager::DrawDrawerToggleButtons() {
  bool clicked = false;

  // Keep menu-bar controls on SmallButton metrics so baseline/spacing stays
  // consistent with the session + notification controls.
  auto DrawPanelButton = [&](const char* icon, const char* base_tooltip,
                             const char* shortcut_action, PanelType type) {
    const bool is_active = IsDrawerActive(type);
    gui::StyleColorGuard button_colors({
        {ImGuiCol_Button, ImVec4(0, 0, 0, 0)},
        {ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighVec4()},
        {ImGuiCol_ButtonActive, gui::GetSurfaceContainerHighestVec4()},
        {ImGuiCol_Text,
         is_active ? gui::GetPrimaryVec4() : gui::GetTextSecondaryVec4()},
    });

    if (ImGui::SmallButton(icon)) {
      ToggleDrawer(type);
      clicked = true;
    }

    if (ImGui::IsItemHovered()) {
      const std::string shortcut = GetShortcutLabel(shortcut_action, "");
      if (shortcut.empty() || shortcut == "Unassigned") {
        ImGui::SetTooltip("%s", base_tooltip);
      } else {
        ImGui::SetTooltip("%s (%s)", base_tooltip, shortcut.c_str());
      }
    }
  };

  DrawPanelButton(ICON_MD_FOLDER_SPECIAL, "Project Drawer",
                  "View: Toggle Project Panel", PanelType::kProject);
  ImGui::SameLine();

  DrawPanelButton(ICON_MD_SMART_TOY, "AI Agent Drawer",
                  "View: Toggle AI Agent Panel", PanelType::kAgentChat);
  ImGui::SameLine();

  DrawPanelButton(ICON_MD_HELP_OUTLINE, "Help Drawer",
                  "View: Toggle Help Panel", PanelType::kHelp);
  ImGui::SameLine();

  DrawPanelButton(ICON_MD_SETTINGS, "Settings Drawer",
                  "View: Toggle Settings Panel", PanelType::kSettings);
  ImGui::SameLine();

  DrawPanelButton(ICON_MD_LIST_ALT, "Properties Drawer",
                  "View: Toggle Properties Panel", PanelType::kProperties);

  return clicked;
}

}  // namespace editor
}  // namespace yaze
