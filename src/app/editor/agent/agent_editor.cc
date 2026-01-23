#include "app/editor/agent/agent_editor.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

#include "app/editor/agent/agent_ui_theme.h"
// Centralized UI theme
#include "app/gui/style/theme.h"

#include "app/editor/system/panel_manager.h"
#include "app/gui/app/editor_layout.h"

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_chat.h"
#include "app/editor/agent/agent_collaboration_coordinator.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/user_settings.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/platform/asset_loader.h"
#include "app/service/screenshot_utils.h"
#include "app/test/test_manager.h"
#include "cli/service/agent/tool_dispatcher.h"
#include "cli/service/ai/service_factory.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "rom/rom.h"
#include "util/file_util.h"
#include "util/platform_paths.h"

#ifdef YAZE_WITH_GRPC
#include "app/editor/agent/network_collaboration_coordinator.h"
#include "cli/service/ai/gemini_ai_service.h"
#endif

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace editor {

namespace {

std::string NormalizeOpenAIBaseUrl(std::string base) {
  if (base.empty()) {
    return "https://api.openai.com";
  }
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  if (absl::EndsWith(base, "/v1")) {
    base.resize(base.size() - 3);
    while (!base.empty() && base.back() == '/') {
      base.pop_back();
    }
  }
  return base;
}

void ApplyHostPresetToProfile(AgentEditor::BotProfile* profile,
                              const UserSettings::Preferences::AiHost& host) {
  if (!profile) {
    return;
  }
  profile->host_id = host.id;
  std::string api_type = host.api_type;
  if (api_type == "lmstudio") {
    api_type = "openai";
  }
  if (api_type == "openai" || api_type == "ollama" || api_type == "gemini") {
    profile->provider = api_type;
  }
  if (profile->provider == "openai") {
    if (!host.base_url.empty()) {
      profile->openai_base_url = NormalizeOpenAIBaseUrl(host.base_url);
    }
    if (!host.api_key.empty()) {
      profile->openai_api_key = host.api_key;
    }
  } else if (profile->provider == "ollama") {
    if (!host.base_url.empty()) {
      profile->ollama_host = host.base_url;
    }
  } else if (profile->provider == "gemini") {
    if (!host.api_key.empty()) {
      profile->gemini_api_key = host.api_key;
    }
  }
}

}  // namespace

AgentEditor::AgentEditor() {
  type_ = EditorType::kAgent;
  agent_chat_ = std::make_unique<AgentChat>();
  local_coordinator_ = std::make_unique<AgentCollaborationCoordinator>();
  prompt_editor_ = std::make_unique<TextEditor>();
  common_tiles_editor_ = std::make_unique<TextEditor>();

  // Initialize default configuration (legacy)
  current_config_.provider = "mock";
  current_config_.show_reasoning = true;
  current_config_.max_tool_iterations = 4;

  // Initialize default bot profile
  current_profile_.name = "Default Z3ED Bot";
  current_profile_.description = "Default bot for Zelda 3 ROM editing";
  current_profile_.provider = "mock";
  current_profile_.show_reasoning = true;
  current_profile_.max_tool_iterations = 4;
  current_profile_.max_retry_attempts = 3;
  current_profile_.tags = {"default", "z3ed"};

  // Setup text editors
  prompt_editor_->SetLanguageDefinition(
      TextEditor::LanguageDefinition::CPlusPlus());
  prompt_editor_->SetReadOnly(false);
  prompt_editor_->SetShowWhitespaces(false);

  common_tiles_editor_->SetLanguageDefinition(
      TextEditor::LanguageDefinition::CPlusPlus());
  common_tiles_editor_->SetReadOnly(false);
  common_tiles_editor_->SetShowWhitespaces(false);

  // Ensure profiles directory exists
  EnsureProfilesDirectory();

  builder_state_.stages = {
      {"Persona", "Define persona and goals", false},
      {"Tool Stack", "Select the agent's tools", false},
      {"Automation", "Configure automation hooks", false},
      {"Validation", "Describe E2E validation", false},
      {"E2E Checklist", "Track readiness for end-to-end runs", false}};
  builder_state_.persona_notes =
      "Describe the persona, tone, and constraints for this agent.";
}

AgentEditor::~AgentEditor() = default;

void AgentEditor::Initialize() {
  // Base initialization
  EnsureProfilesDirectory();

  // Register cards with the card registry
  RegisterPanels();

  // Register EditorPanel instances with PanelManager
  if (dependencies_.panel_manager) {
    auto* panel_manager = dependencies_.panel_manager;

    // Register all agent EditorPanels with callbacks
    panel_manager->RegisterEditorPanel(
        std::make_unique<AgentConfigurationPanel>(
            [this]() { DrawConfigurationPanel(); }));
    panel_manager->RegisterEditorPanel(
        std::make_unique<AgentStatusPanel>([this]() { DrawStatusPanel(); }));
    panel_manager->RegisterEditorPanel(std::make_unique<AgentPromptEditorPanel>(
        [this]() { DrawPromptEditorPanel(); }));
    panel_manager->RegisterEditorPanel(std::make_unique<AgentBotProfilesPanel>(
        [this]() { DrawBotProfilesPanel(); }));
    panel_manager->RegisterEditorPanel(std::make_unique<AgentChatHistoryPanel>(
        [this]() { DrawChatHistoryViewer(); }));
    panel_manager->RegisterEditorPanel(
        std::make_unique<AgentMetricsDashboardPanel>(
            [this]() { DrawAdvancedMetricsPanel(); }));
    panel_manager->RegisterEditorPanel(std::make_unique<AgentBuilderPanel>(
        [this]() { DrawAgentBuilderPanel(); }));
    panel_manager->RegisterEditorPanel(
        std::make_unique<AgentChatPanel>(agent_chat_.get()));

    // Knowledge Base panel (callback set by AgentUiController)
    panel_manager->RegisterEditorPanel(
        std::make_unique<AgentKnowledgeBasePanel>([this]() {
          if (knowledge_panel_callback_) {
            knowledge_panel_callback_();
          } else {
            ImGui::TextDisabled("Knowledge service not available");
            ImGui::TextWrapped(
                "Build with Z3ED_AI=ON to enable the knowledge service.");
          }
        }));
  }

  ApplyUserSettingsDefaults();
}

void AgentEditor::ApplyUserSettingsDefaults(bool force) {
  auto* settings = dependencies_.user_settings;
  if (!settings) {
    return;
  }
  const auto& prefs = settings->prefs();
  if (prefs.ai_hosts.empty()) {
    return;
  }
  if (!force) {
    if (!current_profile_.host_id.empty()) {
      return;
    }
    if (current_profile_.provider != "mock") {
      return;
    }
  }
  const std::string& active_id =
      prefs.active_ai_host_id.empty() ? prefs.ai_hosts.front().id
                                      : prefs.active_ai_host_id;
  if (active_id.empty()) {
    return;
  }
  for (const auto& host : prefs.ai_hosts) {
    if (host.id == active_id) {
      ApplyHostPresetToProfile(&current_profile_, host);
      return;
    }
  }
}

void AgentEditor::RegisterPanels() {
  // Panel descriptors are now auto-created by RegisterEditorPanel() calls
  // in Initialize(). No need for duplicate RegisterPanel() calls here.
}

absl::Status AgentEditor::Load() {
  // Load agent configuration from project/settings
  // Try to load all bot profiles
  loaded_profiles_.clear();
  auto profiles_dir = GetProfilesDirectory();
  if (std::filesystem::exists(profiles_dir)) {
    for (const auto& entry :
         std::filesystem::directory_iterator(profiles_dir)) {
      if (entry.path().extension() == ".json") {
        std::ifstream file(entry.path());
        if (file.is_open()) {
          std::string json_content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
          auto profile_or = JsonToProfile(json_content);
          if (profile_or.ok()) {
            loaded_profiles_.push_back(profile_or.value());
          }
        }
      }
    }
  }
  return absl::OkStatus();
}

absl::Status AgentEditor::Save() {
  // Save current profile
  current_profile_.modified_at = absl::Now();
  return SaveBotProfile(current_profile_);
}

absl::Status AgentEditor::Update() {
  if (!active_)
    return absl::OkStatus();

  // Draw configuration dashboard
  DrawDashboard();

  // Chat widget is drawn separately (not here)

  return absl::OkStatus();
}

void AgentEditor::InitializeWithDependencies(ToastManager* toast_manager,
                                             ProposalDrawer* proposal_drawer,
                                             Rom* rom) {
  toast_manager_ = toast_manager;
  proposal_drawer_ = proposal_drawer;
  rom_ = rom;

  // Auto-load API keys from environment
  if (const char* gemini_key = std::getenv("GEMINI_API_KEY")) {
    current_profile_.gemini_api_key = gemini_key;
    current_config_.gemini_api_key = gemini_key;
    // Auto-select gemini provider if key is available and no provider set
    if (current_profile_.provider == "mock") {
      current_profile_.provider = "gemini";
      current_profile_.model = "gemini-2.5-flash";
      current_config_.provider = "gemini";
      current_config_.model = "gemini-2.5-flash";
    }
  }

  if (const char* openai_key = std::getenv("OPENAI_API_KEY")) {
    current_profile_.openai_api_key = openai_key;
    current_config_.openai_api_key = openai_key;
    // Auto-select openai if no gemini key and provider is mock
    if (current_profile_.provider == "mock" &&
        current_profile_.gemini_api_key.empty()) {
      current_profile_.provider = "openai";
      current_profile_.model = "gpt-4o-mini";
      current_config_.provider = "openai";
      current_config_.model = "gpt-4o-mini";
    }
  }

  if (agent_chat_) {
    agent_chat_->Initialize(toast_manager, proposal_drawer);
    if (rom) {
      agent_chat_->SetRomContext(rom);
    }
  }

  SetupMultimodalCallbacks();
  SetupAutomationCallbacks();

#ifdef YAZE_WITH_GRPC
  if (agent_chat_) {
    harness_telemetry_bridge_.SetAgentChat(agent_chat_.get());
    test::TestManager::Get().SetHarnessListener(&harness_telemetry_bridge_);
  }
#endif

  // Push initial configuration to the agent service
  ApplyConfig(current_config_);
}

void AgentEditor::SetRomContext(Rom* rom) {
  rom_ = rom;
  if (agent_chat_) {
    agent_chat_->SetRomContext(rom);
  }
}

void AgentEditor::DrawDashboard() {
  if (!active_) {
    return;
  }

  // Animate retro effects
  ImGuiIO& imgui_io = ImGui::GetIO();
  pulse_animation_ += imgui_io.DeltaTime * 2.0f;
  scanline_offset_ += imgui_io.DeltaTime * 0.4f;
  if (scanline_offset_ > 1.0f) {
    scanline_offset_ -= 1.0f;
  }
  glitch_timer_ += imgui_io.DeltaTime * 5.0f;
  blink_counter_ = static_cast<int>(pulse_animation_ * 2.0f) % 2;
}

void AgentEditor::DrawConfigurationPanel() {
  const auto& theme = AgentUI::GetTheme();

  auto HelpMarker = [](const char* desc) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
      ImGui::TextUnformatted(desc);
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
  };

  AgentUI::RenderSectionHeader(ICON_MD_SETTINGS, "AI Provider",
                               theme.accent_color);

  if (dependencies_.user_settings) {
    const auto& prefs = dependencies_.user_settings->prefs();
    if (!prefs.ai_hosts.empty()) {
      AgentUI::RenderSectionHeader(ICON_MD_STORAGE, "Host Presets",
                                   theme.accent_color);
      const auto& hosts = prefs.ai_hosts;
      std::string active_id =
          current_profile_.host_id.empty() ? prefs.active_ai_host_id
                                           : current_profile_.host_id;
      int active_index = -1;
      for (size_t i = 0; i < hosts.size(); ++i) {
        if (!active_id.empty() && hosts[i].id == active_id) {
          active_index = static_cast<int>(i);
          break;
        }
      }
      const char* preview =
          (active_index >= 0) ? hosts[active_index].label.c_str()
                              : "Select host";
      if (ImGui::BeginCombo("##ai_host_preset", preview)) {
        for (size_t i = 0; i < hosts.size(); ++i) {
          const bool selected = (static_cast<int>(i) == active_index);
          if (ImGui::Selectable(hosts[i].label.c_str(), selected)) {
            ApplyHostPresetToProfile(&current_profile_, hosts[i]);
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      ImGui::TextDisabled(
          "Host presets come from settings.json (Documents/Yaze).");
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
    }
  }

  float avail_width = ImGui::GetContentRegionAvail().x;
  ImVec2 button_size(avail_width / 2 - 8, 46);

  auto ProviderButton = [&](const char* label, const char* provider_id,
                            const ImVec4& color) {
    bool selected = current_profile_.provider == provider_id;
    ImVec4 base_color = selected ? color : theme.panel_bg_darker;
    if (AgentUI::StyledButton(label, base_color, button_size)) {
      current_profile_.provider = provider_id;
      current_profile_.host_id.clear();
    }
  };

  ProviderButton(ICON_MD_SETTINGS " Mock", "mock", theme.provider_mock);
  ImGui::SameLine();
  ProviderButton(ICON_MD_CLOUD " Ollama", "ollama", theme.provider_ollama);
  ProviderButton(ICON_MD_SMART_TOY " Gemini API", "gemini",
                 theme.provider_gemini);
  ImGui::SameLine();
  ProviderButton(ICON_MD_TERMINAL " Local CLI", "gemini-cli",
                 theme.provider_gemini);
  ImGui::Spacing();
  ProviderButton(ICON_MD_AUTO_AWESOME " OpenAI", "openai",
                 theme.provider_openai);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Provider-specific settings
  if (current_profile_.provider == "ollama") {
    AgentUI::RenderSectionHeader(ICON_MD_TUNE, "Ollama Settings",
                                 theme.accent_color);
    ImGui::Text("Model:");
    ImGui::SetNextItemWidth(-1);
    static char model_buf[128] = "qwen2.5-coder:7b";
    if (!current_profile_.model.empty()) {
      strncpy(model_buf, current_profile_.model.c_str(), sizeof(model_buf) - 1);
    }
    if (ImGui::InputTextWithHint("##ollama_model",
                                 "e.g., qwen2.5-coder:7b, llama3.2", model_buf,
                                 sizeof(model_buf))) {
      current_profile_.model = model_buf;
    }

    ImGui::Text("Host URL:");
    ImGui::SetNextItemWidth(-1);
    static char host_buf[256] = "http://localhost:11434";
    strncpy(host_buf, current_profile_.ollama_host.c_str(),
            sizeof(host_buf) - 1);
    if (ImGui::InputText("##ollama_host", host_buf, sizeof(host_buf))) {
      current_profile_.ollama_host = host_buf;
    }
  } else if (current_profile_.provider == "gemini" ||
             current_profile_.provider == "gemini-cli") {
    AgentUI::RenderSectionHeader(ICON_MD_SMART_TOY, "Gemini Settings",
                                 theme.accent_color);

    if (ImGui::Button(ICON_MD_REFRESH " Load from Env (GEMINI_API_KEY)")) {
      const char* gemini_key = std::getenv("GEMINI_API_KEY");
      if (gemini_key) {
        current_profile_.gemini_api_key = gemini_key;
        ApplyConfig(current_config_);
        if (toast_manager_) {
          toast_manager_->Show("Gemini API key loaded", ToastType::kSuccess);
        }
      } else if (toast_manager_) {
        toast_manager_->Show("GEMINI_API_KEY not found", ToastType::kWarning);
      }
    }
    HelpMarker("Loads GEMINI_API_KEY from your environment");

    ImGui::Spacing();
    ImGui::Text("Model:");
    ImGui::SetNextItemWidth(-1);
    static char model_buf[128] = "gemini-2.5-flash";
    if (!current_profile_.model.empty()) {
      strncpy(model_buf, current_profile_.model.c_str(), sizeof(model_buf) - 1);
    }
    if (ImGui::InputTextWithHint("##gemini_model", "e.g., gemini-2.5-flash",
                                 model_buf, sizeof(model_buf))) {
      current_profile_.model = model_buf;
    }

    ImGui::Text("API Key:");
    ImGui::SetNextItemWidth(-1);
    static char key_buf[256] = "";
    if (!current_profile_.gemini_api_key.empty() && key_buf[0] == '\0') {
      strncpy(key_buf, current_profile_.gemini_api_key.c_str(),
              sizeof(key_buf) - 1);
    }
    if (ImGui::InputText("##gemini_key", key_buf, sizeof(key_buf),
                         ImGuiInputTextFlags_Password)) {
      current_profile_.gemini_api_key = key_buf;
    }
    if (!current_profile_.gemini_api_key.empty()) {
      ImGui::TextColored(theme.status_success,
                         ICON_MD_CHECK_CIRCLE " API key configured");
    }
  } else if (current_profile_.provider == "openai") {
    AgentUI::RenderSectionHeader(ICON_MD_AUTO_AWESOME, "OpenAI Settings",
                                 theme.accent_color);

    if (ImGui::Button(ICON_MD_REFRESH " Load from Env (OPENAI_API_KEY)")) {
      const char* openai_key = std::getenv("OPENAI_API_KEY");
      if (openai_key) {
        current_profile_.openai_api_key = openai_key;
        ApplyConfig(current_config_);
        if (toast_manager_) {
          toast_manager_->Show("OpenAI API key loaded", ToastType::kSuccess);
        }
      } else if (toast_manager_) {
        toast_manager_->Show("OPENAI_API_KEY not found", ToastType::kWarning);
      }
    }
    HelpMarker("Loads OPENAI_API_KEY from your environment");

    ImGui::Spacing();
    ImGui::Text("Model:");
    ImGui::SetNextItemWidth(-1);
    static char openai_model_buf[128] = "gpt-4o-mini";
    if (!current_profile_.model.empty()) {
      strncpy(openai_model_buf, current_profile_.model.c_str(),
              sizeof(openai_model_buf) - 1);
    }
    if (ImGui::InputTextWithHint("##openai_model", "e.g., gpt-4o-mini",
                                 openai_model_buf, sizeof(openai_model_buf))) {
      current_profile_.model = openai_model_buf;
    }

    ImGui::Text("Base URL:");
    ImGui::SetNextItemWidth(-1);
    static char openai_base_buf[256] = "https://api.openai.com";
    if (!current_profile_.openai_base_url.empty()) {
      strncpy(openai_base_buf, current_profile_.openai_base_url.c_str(),
              sizeof(openai_base_buf) - 1);
      openai_base_buf[sizeof(openai_base_buf) - 1] = '\0';
    }
    if (ImGui::InputTextWithHint("##openai_base_url",
                                 "e.g., http://localhost:1234",
                                 openai_base_buf,
                                 sizeof(openai_base_buf))) {
      current_profile_.openai_base_url = openai_base_buf;
    }

    ImGui::Text("API Key:");
    ImGui::SetNextItemWidth(-1);
    static char openai_key_buf[256] = "";
    if (!current_profile_.openai_api_key.empty() && openai_key_buf[0] == '\0') {
      strncpy(openai_key_buf, current_profile_.openai_api_key.c_str(),
              sizeof(openai_key_buf) - 1);
    }
    if (ImGui::InputText("##openai_key", openai_key_buf, sizeof(openai_key_buf),
                         ImGuiInputTextFlags_Password)) {
      current_profile_.openai_api_key = openai_key_buf;
    }
    if (!current_profile_.openai_api_key.empty()) {
      ImGui::TextColored(theme.status_success,
                         ICON_MD_CHECK_CIRCLE " API key configured");
    }
  }

  ImGui::Spacing();
  AgentUI::RenderSectionHeader(ICON_MD_TUNE, "Behavior", theme.text_info);

  ImGui::Checkbox("Verbose logging", &current_profile_.verbose);
  HelpMarker("Logs provider requests/responses to console");
  ImGui::Checkbox("Show reasoning traces", &current_profile_.show_reasoning);
  ImGui::Checkbox("Stream responses", &current_profile_.stream_responses);

  ImGui::SliderFloat("Temperature", &current_profile_.temperature, 0.0f, 1.0f);
  ImGui::SliderFloat("Top P", &current_profile_.top_p, 0.0f, 1.0f);
  ImGui::SliderInt("Max output tokens", &current_profile_.max_output_tokens,
                   256, 4096);
  ImGui::SliderInt(ICON_MD_LOOP " Max Tool Iterations",
                   &current_profile_.max_tool_iterations, 1, 10);
  HelpMarker(
      "Maximum number of tool calls the agent can make while solving a single "
      "request.");
  ImGui::SliderInt(ICON_MD_REFRESH " Max Retry Attempts",
                   &current_profile_.max_retry_attempts, 1, 10);
  HelpMarker("Number of times to retry API calls on failure.");

  ImGui::Spacing();
  AgentUI::RenderSectionHeader(ICON_MD_INFO, "Profile",
                               theme.text_secondary_gray);

  static char name_buf[128];
  strncpy(name_buf, current_profile_.name.c_str(), sizeof(name_buf) - 1);
  if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
    current_profile_.name = name_buf;
  }

  static char desc_buf[256];
  strncpy(desc_buf, current_profile_.description.c_str(), sizeof(desc_buf) - 1);
  if (ImGui::InputTextMultiline("Description", desc_buf, sizeof(desc_buf),
                                ImVec2(-1, 64))) {
    current_profile_.description = desc_buf;
  }

  ImGui::Text("Tags (comma-separated)");
  static char tags_buf[256];
  if (tags_buf[0] == '\0' && !current_profile_.tags.empty()) {
    std::string tags_str;
    for (size_t i = 0; i < current_profile_.tags.size(); ++i) {
      if (i > 0)
        tags_str += ", ";
      tags_str += current_profile_.tags[i];
    }
    strncpy(tags_buf, tags_str.c_str(), sizeof(tags_buf) - 1);
  }
  if (ImGui::InputText("##profile_tags", tags_buf, sizeof(tags_buf))) {
    current_profile_.tags.clear();
    std::string tags_str(tags_buf);
    size_t pos = 0;
    while ((pos = tags_str.find(',')) != std::string::npos) {
      std::string tag = tags_str.substr(0, pos);
      tag.erase(0, tag.find_first_not_of(" \t"));
      tag.erase(tag.find_last_not_of(" \t") + 1);
      if (!tag.empty()) {
        current_profile_.tags.push_back(tag);
      }
      tags_str.erase(0, pos + 1);
    }
    tags_str.erase(0, tags_str.find_first_not_of(" \t"));
    tags_str.erase(tags_str.find_last_not_of(" \t") + 1);
    if (!tags_str.empty()) {
      current_profile_.tags.push_back(tags_str);
    }
  }

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Button, theme.status_success);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.status_success);
  if (ImGui::Button(ICON_MD_CHECK " Apply & Save Configuration",
                    ImVec2(-1, 40))) {
    current_config_.provider = current_profile_.provider;
    current_config_.model = current_profile_.model;
    current_config_.ollama_host = current_profile_.ollama_host;
    current_config_.gemini_api_key = current_profile_.gemini_api_key;
    current_config_.openai_api_key = current_profile_.openai_api_key;
    current_config_.openai_base_url = current_profile_.openai_base_url;
    current_config_.verbose = current_profile_.verbose;
    current_config_.show_reasoning = current_profile_.show_reasoning;
    current_config_.max_tool_iterations = current_profile_.max_tool_iterations;
    current_config_.max_retry_attempts = current_profile_.max_retry_attempts;
    current_config_.temperature = current_profile_.temperature;
    current_config_.top_p = current_profile_.top_p;
    current_config_.max_output_tokens = current_profile_.max_output_tokens;
    current_config_.stream_responses = current_profile_.stream_responses;

    ApplyConfig(current_config_);
    Save();

    if (toast_manager_) {
      toast_manager_->Show("Configuration applied and saved",
                           ToastType::kSuccess);
    }
  }
  ImGui::PopStyleColor(2);

  DrawMetricsPanel();
}

void AgentEditor::DrawStatusPanel() {
  const auto& theme = AgentUI::GetTheme();

  AgentUI::PushPanelStyle();
  if (ImGui::BeginChild("ChatStatusCard", ImVec2(0, 140), true)) {
    AgentUI::RenderSectionHeader(ICON_MD_CHAT, "Chat Status",
                                 theme.accent_color);

    bool chat_active = agent_chat_ && *agent_chat_->active();
    AgentUI::RenderStatusIndicator(chat_active ? "Active" : "Inactive",
                                   chat_active);
    ImGui::SameLine();
    if (!chat_active && ImGui::SmallButton("Open")) {
      OpenChatWindow();
    }

    ImGui::Spacing();
    ImGui::Text("Provider:");
    ImGui::SameLine();
    AgentUI::RenderProviderBadge(current_profile_.provider.c_str());
    if (!current_profile_.model.empty()) {
      ImGui::TextDisabled("Model: %s", current_profile_.model.c_str());
    }
  }
  ImGui::EndChild();

  ImGui::Spacing();

  if (ImGui::BeginChild("RomStatusCard", ImVec2(0, 110), true)) {
    AgentUI::RenderSectionHeader(ICON_MD_GAMEPAD, "ROM Context",
                                 theme.accent_color);
    if (rom_ && rom_->is_loaded()) {
      ImGui::TextColored(theme.status_success, ICON_MD_CHECK_CIRCLE " Loaded");
      ImGui::TextDisabled("Tools: Ready");
    } else {
      ImGui::TextColored(theme.status_warning, ICON_MD_WARNING " Not Loaded");
      ImGui::TextDisabled("Load a ROM to enable tool calls.");
    }
  }
  ImGui::EndChild();

  ImGui::Spacing();

  if (ImGui::BeginChild("QuickTipsCard", ImVec2(0, 130), true)) {
    AgentUI::RenderSectionHeader(ICON_MD_TIPS_AND_UPDATES, "Quick Tips",
                                 theme.accent_color);
    ImGui::BulletText("Ctrl+H: Toggle chat popup");
    ImGui::BulletText("Ctrl+P: View proposals");
    ImGui::BulletText("Edit prompts in Prompt Editor");
    ImGui::BulletText("Create and save custom bots");
  }
  ImGui::EndChild();
  AgentUI::PopPanelStyle();
}

void AgentEditor::DrawMetricsPanel() {
  if (ImGui::CollapsingHeader(ICON_MD_ANALYTICS " Quick Metrics")) {
    ImGui::TextDisabled("View detailed metrics in the Metrics tab");
  }
}

void AgentEditor::DrawPromptEditorPanel() {
  const auto& theme = AgentUI::GetTheme();
  AgentUI::RenderSectionHeader(ICON_MD_EDIT, "Prompt Editor",
                               theme.accent_color);

  ImGui::Text("File:");
  ImGui::SetNextItemWidth(-45);
  if (ImGui::BeginCombo("##prompt_file", active_prompt_file_.c_str())) {
    const char* options[] = {"system_prompt.txt", "system_prompt_v2.txt",
                             "system_prompt_v3.txt"};
    for (const char* option : options) {
      bool selected = active_prompt_file_ == option;
      if (ImGui::Selectable(option, selected)) {
        active_prompt_file_ = option;
        prompt_editor_initialized_ = false;
      }
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_REFRESH)) {
    prompt_editor_initialized_ = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Reload from disk");
  }

  if (!prompt_editor_initialized_ && prompt_editor_) {
    std::string asset_path = "agent/" + active_prompt_file_;
    auto content_result = AssetLoader::LoadTextFile(asset_path);
    if (content_result.ok()) {
      prompt_editor_->SetText(*content_result);
      current_profile_.system_prompt = *content_result;
      prompt_editor_initialized_ = true;
      if (toast_manager_) {
        toast_manager_->Show(absl::StrFormat(ICON_MD_CHECK_CIRCLE " Loaded %s",
                                             active_prompt_file_),
                             ToastType::kSuccess, 2.0f);
      }
    } else {
      std::string placeholder = absl::StrFormat(
          "# System prompt file not found: %s\n"
          "# Error: %s\n\n"
          "# Ensure the file exists in assets/agent/%s\n",
          active_prompt_file_, content_result.status().message(),
          active_prompt_file_);
      prompt_editor_->SetText(placeholder);
      prompt_editor_initialized_ = true;
    }
  }

  ImGui::Spacing();
  if (prompt_editor_) {
    ImVec2 editor_size(ImGui::GetContentRegionAvail().x,
                       ImGui::GetContentRegionAvail().y - 60);
    prompt_editor_->Render("##prompt_editor", editor_size, true);

    ImGui::Spacing();
    if (ImGui::Button(ICON_MD_SAVE " Save Prompt to Profile", ImVec2(-1, 0))) {
      current_profile_.system_prompt = prompt_editor_->GetText();
      if (toast_manager_) {
        toast_manager_->Show("System prompt saved to profile",
                             ToastType::kSuccess);
      }
    }
  }

  ImGui::Spacing();
  ImGui::TextWrapped(
      "Edit the system prompt that guides the agent's behavior. Changes are "
      "stored on the active bot profile.");
}

void AgentEditor::DrawBotProfilesPanel() {
  const auto& theme = AgentUI::GetTheme();
  AgentUI::RenderSectionHeader(ICON_MD_FOLDER, "Bot Profile Manager",
                               theme.accent_color);
  ImGui::Spacing();

  ImGui::BeginChild("CurrentProfile", ImVec2(0, 150), true);
  AgentUI::RenderSectionHeader(ICON_MD_STAR, "Current Profile",
                               theme.accent_color);
  ImGui::Text("Name: %s", current_profile_.name.c_str());
  ImGui::Text("Provider: %s", current_profile_.provider.c_str());
  if (!current_profile_.model.empty()) {
    ImGui::Text("Model: %s", current_profile_.model.c_str());
  }
  ImGui::TextWrapped("Description: %s",
                     current_profile_.description.empty()
                         ? "No description"
                         : current_profile_.description.c_str());
  ImGui::EndChild();

  ImGui::Spacing();

  if (ImGui::Button(ICON_MD_ADD " Create New Profile", ImVec2(-1, 0))) {
    BotProfile new_profile = current_profile_;
    new_profile.name = "New Profile";
    new_profile.created_at = absl::Now();
    new_profile.modified_at = absl::Now();
    current_profile_ = new_profile;
    if (toast_manager_) {
      toast_manager_->Show("New profile created. Configure and save it.",
                           ToastType::kInfo);
    }
  }

  ImGui::Spacing();
  AgentUI::RenderSectionHeader(ICON_MD_LIST, "Saved Profiles",
                               theme.accent_color);

  ImGui::BeginChild("ProfilesList", ImVec2(0, 0), true);
  if (loaded_profiles_.empty()) {
    ImGui::TextDisabled(
        "No saved profiles. Create and save a profile to see it here.");
  } else {
    for (size_t i = 0; i < loaded_profiles_.size(); ++i) {
      const auto& profile = loaded_profiles_[i];
      ImGui::PushID(static_cast<int>(i));

      bool is_current = (profile.name == current_profile_.name);
      ImVec2 button_size(ImGui::GetContentRegionAvail().x - 80, 0);
      ImVec4 button_color =
          is_current ? theme.accent_color : theme.panel_bg_darker;
      if (AgentUI::StyledButton(profile.name.c_str(), button_color,
                                button_size)) {
        if (auto status = LoadBotProfile(profile.name); status.ok()) {
          if (toast_manager_) {
            toast_manager_->Show(
                absl::StrFormat("Loaded profile: %s", profile.name),
                ToastType::kSuccess);
          }
        } else if (toast_manager_) {
          toast_manager_->Show(std::string(status.message()),
                               ToastType::kError);
        }
      }

      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, theme.status_warning);
      if (ImGui::SmallButton(ICON_MD_DELETE)) {
        if (auto status = DeleteBotProfile(profile.name); status.ok()) {
          if (toast_manager_) {
            toast_manager_->Show(
                absl::StrFormat("Deleted profile: %s", profile.name),
                ToastType::kInfo);
          }
        } else if (toast_manager_) {
          toast_manager_->Show(std::string(status.message()),
                               ToastType::kError);
        }
      }
      ImGui::PopStyleColor();

      ImGui::TextDisabled("  %s | %s", profile.provider.c_str(),
                          profile.description.empty()
                              ? "No description"
                              : profile.description.c_str());
      ImGui::Spacing();
      ImGui::PopID();
    }
  }
  ImGui::EndChild();
}

void AgentEditor::DrawChatHistoryViewer() {
  const auto& theme = AgentUI::GetTheme();
  AgentUI::RenderSectionHeader(ICON_MD_HISTORY, "Chat History Viewer",
                               theme.accent_color);

  if (ImGui::Button(ICON_MD_REFRESH " Refresh History")) {
    history_needs_refresh_ = true;
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DELETE_FOREVER " Clear History")) {
    if (agent_chat_) {
      agent_chat_->ClearHistory();
      cached_history_.clear();
    }
  }

  if (history_needs_refresh_ && agent_chat_) {
    cached_history_ = agent_chat_->GetAgentService()->GetHistory();
    history_needs_refresh_ = false;
  }

  ImGui::Spacing();
  ImGui::Separator();

  ImGui::BeginChild("HistoryList", ImVec2(0, 0), true);
  if (cached_history_.empty()) {
    ImGui::TextDisabled(
        "No chat history. Start a conversation in the chat window.");
  } else {
    for (const auto& msg : cached_history_) {
      bool from_user = (msg.sender == cli::agent::ChatMessage::Sender::kUser);
      ImVec4 color =
          from_user ? theme.user_message_color : theme.agent_message_color;

      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::Text("%s:", from_user ? "User" : "Agent");
      ImGui::PopStyleColor();

      ImGui::SameLine();
      ImGui::TextDisabled("%s", absl::FormatTime("%H:%M:%S", msg.timestamp,
                                                 absl::LocalTimeZone())
                                    .c_str());

      ImGui::TextWrapped("%s", msg.message.c_str());
      ImGui::Spacing();
      ImGui::Separator();
    }
  }
  ImGui::EndChild();
}

void AgentEditor::DrawAdvancedMetricsPanel() {
  const auto& theme = AgentUI::GetTheme();
  AgentUI::RenderSectionHeader(ICON_MD_ANALYTICS, "Session Metrics",
                               theme.accent_color);
  ImGui::Spacing();

  if (agent_chat_) {
    auto metrics = agent_chat_->GetAgentService()->GetMetrics();
    if (ImGui::BeginTable("MetricsTable", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed,
                              200.0f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      auto Row = [](const char* label, const std::string& value) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%s", label);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("%s", value.c_str());
      };

      Row("Total Messages",
          absl::StrFormat("%d user / %d agent", metrics.total_user_messages,
                          metrics.total_agent_messages));
      Row("Tool Calls", absl::StrFormat("%d", metrics.total_tool_calls));
      Row("Commands", absl::StrFormat("%d", metrics.total_commands));
      Row("Proposals", absl::StrFormat("%d", metrics.total_proposals));
      Row("Average Latency (s)",
          absl::StrFormat("%.2f", metrics.average_latency_seconds));
      Row("Elapsed (s)",
          absl::StrFormat("%.2f", metrics.total_elapsed_seconds));

      ImGui::EndTable();
    }
  } else {
    ImGui::TextDisabled("Initialize the chat system to see metrics.");
  }
}

void AgentEditor::DrawCommonTilesEditor() {
  const auto& theme = AgentUI::GetTheme();
  AgentUI::RenderSectionHeader(ICON_MD_GRID_ON, "Common Tiles Reference",
                               theme.accent_color);
  ImGui::Spacing();

  ImGui::TextWrapped(
      "Customize the tile reference file that AI uses for tile placement. "
      "Organize tiles by category and provide hex IDs with descriptions.");

  ImGui::Spacing();

  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Load", ImVec2(100, 0))) {
    auto content = AssetLoader::LoadTextFile("agent/common_tiles.txt");
    if (content.ok()) {
      common_tiles_editor_->SetText(*content);
      common_tiles_initialized_ = true;
      if (toast_manager_) {
        toast_manager_->Show(ICON_MD_CHECK_CIRCLE " Common tiles loaded",
                             ToastType::kSuccess, 2.0f);
      }
    }
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE " Save", ImVec2(100, 0))) {
    if (toast_manager_) {
      toast_manager_->Show(ICON_MD_INFO
                           " Save to project directory (coming soon)",
                           ToastType::kInfo, 2.0f);
    }
  }

  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_REFRESH)) {
    common_tiles_initialized_ = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Reload from disk");
  }

  if (!common_tiles_initialized_ && common_tiles_editor_) {
    auto content = AssetLoader::LoadTextFile("agent/common_tiles.txt");
    if (content.ok()) {
      common_tiles_editor_->SetText(*content);
    } else {
      std::string default_tiles =
          "# Common Tile16 Reference\n"
          "# Format: 0xHEX = Description\n\n"
          "[grass_tiles]\n"
          "0x020 = Grass (standard)\n\n"
          "[nature_tiles]\n"
          "0x02E = Tree (oak)\n"
          "0x003 = Bush\n\n"
          "[water_tiles]\n"
          "0x14C = Water (top edge)\n"
          "0x14D = Water (middle)\n";
      common_tiles_editor_->SetText(default_tiles);
    }
    common_tiles_initialized_ = true;
  }

  ImGui::Separator();
  ImGui::Spacing();

  if (common_tiles_editor_) {
    ImVec2 editor_size(ImGui::GetContentRegionAvail().x,
                       ImGui::GetContentRegionAvail().y);
    common_tiles_editor_->Render("##tiles_editor", editor_size, true);
  }
}

void AgentEditor::DrawNewPromptCreator() {
  const auto& theme = AgentUI::GetTheme();
  AgentUI::RenderSectionHeader(ICON_MD_ADD, "Create New System Prompt",
                               theme.accent_color);
  ImGui::Spacing();

  ImGui::TextWrapped(
      "Create a custom system prompt from scratch or start from a template.");
  ImGui::Separator();

  ImGui::Text("Prompt Name:");
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##new_prompt_name", "e.g., custom_prompt.txt",
                           new_prompt_name_, sizeof(new_prompt_name_));

  ImGui::Spacing();
  ImGui::Text("Start from template:");

  auto LoadTemplate = [&](const char* path, const char* label) {
    if (ImGui::Button(label, ImVec2(-1, 0))) {
      auto content = AssetLoader::LoadTextFile(path);
      if (content.ok() && prompt_editor_) {
        prompt_editor_->SetText(*content);
        if (toast_manager_) {
          toast_manager_->Show("Template loaded", ToastType::kSuccess, 1.5f);
        }
      }
    }
  };

  LoadTemplate("agent/system_prompt.txt", ICON_MD_FILE_COPY " v1 (Basic)");
  LoadTemplate("agent/system_prompt_v2.txt",
               ICON_MD_FILE_COPY " v2 (Enhanced)");
  LoadTemplate("agent/system_prompt_v3.txt",
               ICON_MD_FILE_COPY " v3 (Proactive)");

  if (ImGui::Button(ICON_MD_NOTE_ADD " Blank Template", ImVec2(-1, 0))) {
    if (prompt_editor_) {
      std::string blank_template =
          "# Custom System Prompt\n\n"
          "You are an AI assistant for ROM hacking.\n\n"
          "## Your Role\n"
          "- Help users understand ROM data\n"
          "- Provide accurate information\n"
          "- Use tools when needed\n\n"
          "## Guidelines\n"
          "1. Always provide text_response after tool calls\n"
          "2. Be helpful and accurate\n"
          "3. Explain your reasoning\n";
      prompt_editor_->SetText(blank_template);
      if (toast_manager_) {
        toast_manager_->Show("Blank template created", ToastType::kSuccess,
                             1.5f);
      }
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  ImGui::PushStyleColor(ImGuiCol_Button, theme.status_success);
  if (ImGui::Button(ICON_MD_SAVE " Save New Prompt", ImVec2(-1, 40))) {
    if (std::strlen(new_prompt_name_) > 0 && prompt_editor_) {
      std::string filename = new_prompt_name_;
      if (!absl::EndsWith(filename, ".txt")) {
        filename += ".txt";
      }
      if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat(ICON_MD_SAVE " Prompt saved as %s", filename),
            ToastType::kSuccess, 3.0f);
      }
      std::memset(new_prompt_name_, 0, sizeof(new_prompt_name_));
    } else if (toast_manager_) {
      toast_manager_->Show(ICON_MD_WARNING " Enter a name for the prompt",
                           ToastType::kWarning, 2.0f);
    }
  }
  ImGui::PopStyleColor();

  ImGui::Spacing();
  ImGui::TextWrapped(
      "Note: New prompts are saved to your project. Use the Prompt Editor to "
      "edit existing prompts.");
}

void AgentEditor::DrawAgentBuilderPanel() {
  const auto& theme = AgentUI::GetTheme();
  AgentUI::RenderSectionHeader(ICON_MD_AUTO_FIX_HIGH, "Agent Builder",
                               theme.accent_color);

  if (!agent_chat_) {
    ImGui::TextDisabled("Chat system not initialized.");
    return;
  }

  ImGui::BeginChild("AgentBuilderPanel", ImVec2(0, 0), false);
  ImGui::Columns(2, nullptr, false);
  ImGui::TextColored(theme.accent_color, "Stages");
  ImGui::Separator();

  for (size_t i = 0; i < builder_state_.stages.size(); ++i) {
    auto& stage = builder_state_.stages[i];
    ImGui::PushID(static_cast<int>(i));
    bool selected = builder_state_.active_stage == static_cast<int>(i);
    if (ImGui::Selectable(stage.name.c_str(), selected)) {
      builder_state_.active_stage = static_cast<int>(i);
    }
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f);
    ImGui::Checkbox("##stage_done", &stage.completed);
    ImGui::PopID();
  }

  ImGui::NextColumn();
  ImGui::TextColored(theme.text_info, "Stage Details");
  ImGui::Separator();

  int stage_index =
      std::clamp(builder_state_.active_stage, 0,
                 static_cast<int>(builder_state_.stages.size()) - 1);
  int completed_stages = 0;
  for (const auto& stage : builder_state_.stages) {
    if (stage.completed) {
      ++completed_stages;
    }
  }

  switch (stage_index) {
    case 0: {
      static std::string new_goal;
      ImGui::Text("Persona + Goals");
      ImGui::InputTextMultiline("##persona_notes",
                                &builder_state_.persona_notes, ImVec2(-1, 120));
      ImGui::Spacing();
      ImGui::TextDisabled("Add Goal");
      ImGui::InputTextWithHint("##goal_input", "e.g. Document dungeon plan",
                               &new_goal);
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_ADD) && !new_goal.empty()) {
        builder_state_.goals.push_back(new_goal);
        new_goal.clear();
      }
      for (size_t i = 0; i < builder_state_.goals.size(); ++i) {
        ImGui::BulletText("%s", builder_state_.goals[i].c_str());
        ImGui::SameLine();
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton(ICON_MD_CLOSE)) {
          builder_state_.goals.erase(builder_state_.goals.begin() + i);
          ImGui::PopID();
          break;
        }
        ImGui::PopID();
      }
      break;
    }
    case 1: {
      ImGui::Text("Tool Stack");
      auto tool_checkbox = [&](const char* label, bool* value) {
        ImGui::Checkbox(label, value);
      };
      tool_checkbox("Resources", &builder_state_.tools.resources);
      tool_checkbox("Dungeon", &builder_state_.tools.dungeon);
      tool_checkbox("Overworld", &builder_state_.tools.overworld);
      tool_checkbox("Dialogue", &builder_state_.tools.dialogue);
      tool_checkbox("GUI Automation", &builder_state_.tools.gui);
      tool_checkbox("Music", &builder_state_.tools.music);
      tool_checkbox("Sprite", &builder_state_.tools.sprite);
      tool_checkbox("Emulator", &builder_state_.tools.emulator);
      break;
    }
    case 2: {
      ImGui::Text("Automation");
      ImGui::Checkbox("Auto-run harness plan", &builder_state_.auto_run_tests);
      ImGui::Checkbox("Auto-sync ROM context", &builder_state_.auto_sync_rom);
      ImGui::Checkbox("Auto-focus proposal drawer",
                      &builder_state_.auto_focus_proposals);
      ImGui::TextWrapped(
          "Enable these options to push harness dashboards/test plans when "
          "executing plans.");
      break;
    }
    case 3: {
      ImGui::Text("Validation Criteria");
      ImGui::InputTextMultiline("##validation_notes",
                                &builder_state_.stages[stage_index].summary,
                                ImVec2(-1, 120));
      break;
    }
    case 4: {
      ImGui::Text("E2E Checklist");
      float progress =
          builder_state_.stages.empty()
              ? 0.0f
              : static_cast<float>(completed_stages) /
                    static_cast<float>(builder_state_.stages.size());
      ImGui::ProgressBar(progress, ImVec2(-1, 0),
                         absl::StrFormat("%d/%zu complete", completed_stages,
                                         builder_state_.stages.size())
                             .c_str());
      ImGui::Checkbox("Ready for automation handoff",
                      &builder_state_.ready_for_e2e);
      ImGui::TextDisabled("Auto-sync ROM: %s",
                          builder_state_.auto_sync_rom ? "ON" : "OFF");
      ImGui::TextDisabled("Auto-focus proposals: %s",
                          builder_state_.auto_focus_proposals ? "ON" : "OFF");
      break;
    }
  }

  ImGui::Columns(1);
  ImGui::Separator();

  float completion_ratio =
      builder_state_.stages.empty()
          ? 0.0f
          : static_cast<float>(completed_stages) /
                static_cast<float>(builder_state_.stages.size());
  ImGui::TextDisabled("Overall Progress");
  ImGui::ProgressBar(completion_ratio, ImVec2(-1, 0));
  ImGui::TextDisabled("E2E Ready: %s",
                      builder_state_.ready_for_e2e ? "Yes" : "No");

  if (ImGui::Button(ICON_MD_LINK " Apply to Chat")) {
    auto* service = agent_chat_->GetAgentService();
    if (service) {
      cli::agent::ToolDispatcher::ToolPreferences prefs;
      prefs.resources = builder_state_.tools.resources;
      prefs.dungeon = builder_state_.tools.dungeon;
      prefs.overworld = builder_state_.tools.overworld;
      prefs.dialogue = builder_state_.tools.dialogue;
      prefs.gui = builder_state_.tools.gui;
      prefs.music = builder_state_.tools.music;
      prefs.sprite = builder_state_.tools.sprite;
#ifdef YAZE_WITH_GRPC
      prefs.emulator = builder_state_.tools.emulator;
#endif
      service->SetToolPreferences(prefs);

      auto agent_cfg = service->GetConfig();
      agent_cfg.max_tool_iterations = current_profile_.max_tool_iterations;
      agent_cfg.max_retry_attempts = current_profile_.max_retry_attempts;
      agent_cfg.verbose = current_profile_.verbose;
      agent_cfg.show_reasoning = current_profile_.show_reasoning;
      service->SetConfig(agent_cfg);
    }

    agent_chat_->SetLastPlanSummary(builder_state_.persona_notes);

    if (toast_manager_) {
      toast_manager_->Show("Builder tool plan synced to chat",
                           ToastType::kSuccess, 2.0f);
    }
  }
  ImGui::SameLine();

  ImGui::InputTextWithHint("##blueprint_path", "Path to blueprint...",
                           &builder_state_.blueprint_path);
  std::filesystem::path blueprint_path =
      builder_state_.blueprint_path.empty()
          ? (std::filesystem::temp_directory_path() / "agent_builder.json")
          : std::filesystem::path(builder_state_.blueprint_path);

  if (ImGui::Button(ICON_MD_SAVE " Save Blueprint")) {
    auto status = SaveBuilderBlueprint(blueprint_path);
    if (toast_manager_) {
      if (status.ok()) {
        toast_manager_->Show("Builder blueprint saved", ToastType::kSuccess,
                             2.0f);
      } else {
        toast_manager_->Show(std::string(status.message()), ToastType::kError,
                             3.5f);
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Load Blueprint")) {
    auto status = LoadBuilderBlueprint(blueprint_path);
    if (toast_manager_) {
      if (status.ok()) {
        toast_manager_->Show("Builder blueprint loaded", ToastType::kSuccess,
                             2.0f);
      } else {
        toast_manager_->Show(std::string(status.message()), ToastType::kError,
                             3.5f);
      }
    }
  }

  ImGui::EndChild();
}

absl::Status AgentEditor::SaveBuilderBlueprint(
    const std::filesystem::path& path) {
#if defined(YAZE_WITH_JSON)
  nlohmann::json json;
  json["persona_notes"] = builder_state_.persona_notes;
  json["goals"] = builder_state_.goals;
  json["auto_run_tests"] = builder_state_.auto_run_tests;
  json["auto_sync_rom"] = builder_state_.auto_sync_rom;
  json["auto_focus_proposals"] = builder_state_.auto_focus_proposals;
  json["ready_for_e2e"] = builder_state_.ready_for_e2e;
  json["tools"] = {
      {"resources", builder_state_.tools.resources},
      {"dungeon", builder_state_.tools.dungeon},
      {"overworld", builder_state_.tools.overworld},
      {"dialogue", builder_state_.tools.dialogue},
      {"gui", builder_state_.tools.gui},
      {"music", builder_state_.tools.music},
      {"sprite", builder_state_.tools.sprite},
      {"emulator", builder_state_.tools.emulator},
  };
  json["stages"] = nlohmann::json::array();
  for (const auto& stage : builder_state_.stages) {
    json["stages"].push_back({{"name", stage.name},
                              {"summary", stage.summary},
                              {"completed", stage.completed}});
  }

  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream file(path);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open blueprint: %s", path.string()));
  }
  file << json.dump(2);
  builder_state_.blueprint_path = path.string();
  return absl::OkStatus();
#else
  (void)path;
  return absl::UnimplementedError("Blueprint export requires JSON support");
#endif
}

absl::Status AgentEditor::LoadBuilderBlueprint(
    const std::filesystem::path& path) {
#if defined(YAZE_WITH_JSON)
  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Blueprint not found: %s", path.string()));
  }

  nlohmann::json json;
  file >> json;

  builder_state_.persona_notes = json.value("persona_notes", "");
  builder_state_.goals.clear();
  if (json.contains("goals") && json["goals"].is_array()) {
    for (const auto& goal : json["goals"]) {
      if (goal.is_string()) {
        builder_state_.goals.push_back(goal.get<std::string>());
      }
    }
  }
  if (json.contains("tools") && json["tools"].is_object()) {
    auto tools = json["tools"];
    builder_state_.tools.resources = tools.value("resources", true);
    builder_state_.tools.dungeon = tools.value("dungeon", true);
    builder_state_.tools.overworld = tools.value("overworld", true);
    builder_state_.tools.dialogue = tools.value("dialogue", true);
    builder_state_.tools.gui = tools.value("gui", false);
    builder_state_.tools.music = tools.value("music", false);
    builder_state_.tools.sprite = tools.value("sprite", false);
    builder_state_.tools.emulator = tools.value("emulator", false);
  }
  builder_state_.auto_run_tests = json.value("auto_run_tests", false);
  builder_state_.auto_sync_rom = json.value("auto_sync_rom", true);
  builder_state_.auto_focus_proposals =
      json.value("auto_focus_proposals", true);
  builder_state_.ready_for_e2e = json.value("ready_for_e2e", false);
  if (json.contains("stages") && json["stages"].is_array()) {
    builder_state_.stages.clear();
    for (const auto& stage : json["stages"]) {
      AgentBuilderState::Stage builder_stage;
      builder_stage.name = stage.value("name", std::string{});
      builder_stage.summary = stage.value("summary", std::string{});
      builder_stage.completed = stage.value("completed", false);
      builder_state_.stages.push_back(builder_stage);
    }
  }
  builder_state_.blueprint_path = path.string();
  return absl::OkStatus();
#else
  (void)path;
  return absl::UnimplementedError("Blueprint import requires JSON support");
#endif
}

absl::Status AgentEditor::SaveBotProfile(const BotProfile& profile) {
#if defined(YAZE_WITH_JSON)
  auto dir_status = EnsureProfilesDirectory();
  if (!dir_status.ok())
    return dir_status;

  std::filesystem::path profile_path =
      GetProfilesDirectory() / (profile.name + ".json");
  std::ofstream file(profile_path);
  if (!file.is_open()) {
    return absl::InternalError("Failed to open profile file for writing");
  }

  file << ProfileToJson(profile);
  file.close();
  return Load();
#else
  return absl::UnimplementedError(
      "JSON support required for profile management");
#endif
}

absl::Status AgentEditor::LoadBotProfile(const std::string& name) {
#if defined(YAZE_WITH_JSON)
  std::filesystem::path profile_path =
      GetProfilesDirectory() / (name + ".json");
  if (!std::filesystem::exists(profile_path)) {
    return absl::NotFoundError(absl::StrFormat("Profile '%s' not found", name));
  }

  std::ifstream file(profile_path);
  if (!file.is_open()) {
    return absl::InternalError("Failed to open profile file");
  }

  std::string json_content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

  auto profile_or = JsonToProfile(json_content);
  if (!profile_or.ok()) {
    return profile_or.status();
  }
  current_profile_ = *profile_or;

  current_config_.provider = current_profile_.provider;
  current_config_.model = current_profile_.model;
  current_config_.ollama_host = current_profile_.ollama_host;
  current_config_.gemini_api_key = current_profile_.gemini_api_key;
  current_config_.openai_api_key = current_profile_.openai_api_key;
  current_config_.openai_base_url = current_profile_.openai_base_url;
  current_config_.verbose = current_profile_.verbose;
  current_config_.show_reasoning = current_profile_.show_reasoning;
  current_config_.max_tool_iterations = current_profile_.max_tool_iterations;
  current_config_.max_retry_attempts = current_profile_.max_retry_attempts;
  current_config_.temperature = current_profile_.temperature;
  current_config_.top_p = current_profile_.top_p;
  current_config_.max_output_tokens = current_profile_.max_output_tokens;
  current_config_.stream_responses = current_profile_.stream_responses;

  ApplyConfig(current_config_);
  return absl::OkStatus();
#else
  return absl::UnimplementedError(
      "JSON support required for profile management");
#endif
}

absl::Status AgentEditor::DeleteBotProfile(const std::string& name) {
  std::filesystem::path profile_path =
      GetProfilesDirectory() / (name + ".json");
  if (!std::filesystem::exists(profile_path)) {
    return absl::NotFoundError(absl::StrFormat("Profile '%s' not found", name));
  }

  std::filesystem::remove(profile_path);
  return Load();
}

std::vector<AgentEditor::BotProfile> AgentEditor::GetAllProfiles() const {
  return loaded_profiles_;
}

void AgentEditor::SetCurrentProfile(const BotProfile& profile) {
  current_profile_ = profile;
  // Sync to legacy config
  current_config_.provider = profile.provider;
  current_config_.model = profile.model;
  current_config_.ollama_host = profile.ollama_host;
  current_config_.gemini_api_key = profile.gemini_api_key;
  current_config_.openai_api_key = profile.openai_api_key;
  current_config_.openai_base_url = profile.openai_base_url;
  current_config_.verbose = profile.verbose;
  current_config_.show_reasoning = profile.show_reasoning;
  current_config_.max_tool_iterations = profile.max_tool_iterations;
  current_config_.max_retry_attempts = profile.max_retry_attempts;
  current_config_.temperature = profile.temperature;
  current_config_.top_p = profile.top_p;
  current_config_.max_output_tokens = profile.max_output_tokens;
  current_config_.stream_responses = profile.stream_responses;
  ApplyConfig(current_config_);
}

absl::Status AgentEditor::ExportProfile(const BotProfile& profile,
                                        const std::filesystem::path& path) {
#if defined(YAZE_WITH_JSON)
  auto status = SaveBotProfile(profile);
  if (!status.ok())
    return status;

  std::ofstream file(path);
  if (!file.is_open()) {
    return absl::InternalError("Failed to open file for export");
  }
  file << ProfileToJson(profile);
  return absl::OkStatus();
#else
  (void)profile;
  (void)path;
  return absl::UnimplementedError("JSON support required");
#endif
}

absl::Status AgentEditor::ImportProfile(const std::filesystem::path& path) {
#if defined(YAZE_WITH_JSON)
  if (!std::filesystem::exists(path)) {
    return absl::NotFoundError("Import file not found");
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::InternalError("Failed to open import file");
  }

  std::string json_content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

  auto profile_or = JsonToProfile(json_content);
  if (!profile_or.ok()) {
    return profile_or.status();
  }

  return SaveBotProfile(*profile_or);
#else
  (void)path;
  return absl::UnimplementedError("JSON support required");
#endif
}

std::filesystem::path AgentEditor::GetProfilesDirectory() const {
  auto agent_dir = yaze::util::PlatformPaths::GetAppDataSubdirectory("agent");
  if (agent_dir.ok()) {
    return *agent_dir / "profiles";
  }
  auto temp_dir = yaze::util::PlatformPaths::GetTempDirectory();
  if (temp_dir.ok()) {
    return *temp_dir / "agent" / "profiles";
  }
  return std::filesystem::current_path() / "agent" / "profiles";
}

absl::Status AgentEditor::EnsureProfilesDirectory() {
  auto dir = GetProfilesDirectory();
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    return absl::InternalError(absl::StrFormat(
        "Failed to create profiles directory: %s", ec.message()));
  }
  return absl::OkStatus();
}

std::string AgentEditor::ProfileToJson(const BotProfile& profile) const {
#if defined(YAZE_WITH_JSON)
  nlohmann::json json;
  json["name"] = profile.name;
  json["description"] = profile.description;
  json["provider"] = profile.provider;
  json["host_id"] = profile.host_id;
  json["model"] = profile.model;
  json["ollama_host"] = profile.ollama_host;
  json["gemini_api_key"] = profile.gemini_api_key;
  json["openai_api_key"] = profile.openai_api_key;
  json["openai_base_url"] = profile.openai_base_url;
  json["system_prompt"] = profile.system_prompt;
  json["verbose"] = profile.verbose;
  json["show_reasoning"] = profile.show_reasoning;
  json["max_tool_iterations"] = profile.max_tool_iterations;
  json["max_retry_attempts"] = profile.max_retry_attempts;
  json["temperature"] = profile.temperature;
  json["top_p"] = profile.top_p;
  json["max_output_tokens"] = profile.max_output_tokens;
  json["stream_responses"] = profile.stream_responses;
  json["tags"] = profile.tags;
  json["created_at"] = absl::FormatTime(absl::RFC3339_full, profile.created_at,
                                        absl::UTCTimeZone());
  json["modified_at"] = absl::FormatTime(
      absl::RFC3339_full, profile.modified_at, absl::UTCTimeZone());

  return json.dump(2);
#else
  return "{}";
#endif
}

absl::StatusOr<AgentEditor::BotProfile> AgentEditor::JsonToProfile(
    const std::string& json_str) const {
#if defined(YAZE_WITH_JSON)
  try {
    nlohmann::json json = nlohmann::json::parse(json_str);

    BotProfile profile;
    profile.name = json.value("name", "Unnamed Profile");
    profile.description = json.value("description", "");
    profile.provider = json.value("provider", "mock");
    profile.host_id = json.value("host_id", "");
    profile.model = json.value("model", "");
    profile.ollama_host = json.value("ollama_host", "http://localhost:11434");
    profile.gemini_api_key = json.value("gemini_api_key", "");
    profile.openai_api_key = json.value("openai_api_key", "");
    profile.openai_base_url =
        json.value("openai_base_url", "https://api.openai.com");
    profile.system_prompt = json.value("system_prompt", "");
    profile.verbose = json.value("verbose", false);
    profile.show_reasoning = json.value("show_reasoning", true);
    profile.max_tool_iterations = json.value("max_tool_iterations", 4);
    profile.max_retry_attempts = json.value("max_retry_attempts", 3);
    profile.temperature = json.value("temperature", 0.25f);
    profile.top_p = json.value("top_p", 0.95f);
    profile.max_output_tokens = json.value("max_output_tokens", 2048);
    profile.stream_responses = json.value("stream_responses", false);

    if (json.contains("tags") && json["tags"].is_array()) {
      for (const auto& tag : json["tags"]) {
        profile.tags.push_back(tag.get<std::string>());
      }
    }

    if (json.contains("created_at")) {
      absl::Time created;
      if (absl::ParseTime(absl::RFC3339_full,
                          json["created_at"].get<std::string>(), &created,
                          nullptr)) {
        profile.created_at = created;
      }
    }

    if (json.contains("modified_at")) {
      absl::Time modified;
      if (absl::ParseTime(absl::RFC3339_full,
                          json["modified_at"].get<std::string>(), &modified,
                          nullptr)) {
        profile.modified_at = modified;
      }
    }

    return profile;
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to parse profile JSON: %s", e.what()));
  }
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

AgentEditor::AgentConfig AgentEditor::GetCurrentConfig() const {
  return current_config_;
}

void AgentEditor::ApplyConfig(const AgentConfig& config) {
  current_config_ = config;

  if (agent_chat_) {
    auto* service = agent_chat_->GetAgentService();
    if (service) {
      cli::AIServiceConfig provider_config;
      provider_config.provider =
          config.provider.empty() ? "auto" : config.provider;
      provider_config.model = config.model;
      provider_config.ollama_host = config.ollama_host;
      provider_config.gemini_api_key = config.gemini_api_key;
      provider_config.openai_api_key = config.openai_api_key;
      provider_config.openai_base_url =
          NormalizeOpenAIBaseUrl(config.openai_base_url);
      provider_config.verbose = config.verbose;

      auto status = service->ConfigureProvider(provider_config);
      if (!status.ok() && toast_manager_) {
        toast_manager_->Show(std::string(status.message()), ToastType::kError);
      }

      auto agent_cfg = service->GetConfig();
      agent_cfg.max_tool_iterations = config.max_tool_iterations;
      agent_cfg.max_retry_attempts = config.max_retry_attempts;
      agent_cfg.verbose = config.verbose;
      agent_cfg.show_reasoning = config.show_reasoning;
      service->SetConfig(agent_cfg);
    }
  }
}

bool AgentEditor::IsChatActive() const {
  return agent_chat_ && *agent_chat_->active();
}

void AgentEditor::SetChatActive(bool active) {
  if (agent_chat_) {
    agent_chat_->set_active(active);
  }
}

void AgentEditor::ToggleChat() {
  SetChatActive(!IsChatActive());
}

void AgentEditor::OpenChatWindow() {
  if (agent_chat_) {
    agent_chat_->set_active(true);
  }
}

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::HostSession(
    const std::string& session_name, CollaborationMode mode) {
  current_mode_ = mode;

  if (mode == CollaborationMode::kLocal) {
    auto session_or = local_coordinator_->HostSession(session_name);
    if (!session_or.ok())
      return session_or.status();

    SessionInfo info;
    info.session_id = session_or->session_id;
    info.session_name = session_or->session_name;
    info.participants = session_or->participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Hosting local session: %s", session_name),
          ToastType::kSuccess, 3.0f);
    }
    return info;
  }

#ifdef YAZE_WITH_GRPC
  if (mode == CollaborationMode::kNetwork) {
    if (!network_coordinator_) {
      return absl::FailedPreconditionError(
          "Network coordinator not initialized. Connect to a server first.");
    }

    const char* username = std::getenv("USER");
    if (!username) {
      username = std::getenv("USERNAME");
    }
    if (!username) {
      username = "unknown";
    }

    auto session_or = network_coordinator_->HostSession(session_name, username);
    if (!session_or.ok())
      return session_or.status();

    SessionInfo info;
    info.session_id = session_or->session_id;
    info.session_name = session_or->session_name;
    info.participants = session_or->participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Hosting network session: %s", session_name),
          ToastType::kSuccess, 3.0f);
    }

    return info;
  }
#endif

  return absl::InvalidArgumentError("Unsupported collaboration mode");
}

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::JoinSession(
    const std::string& session_code, CollaborationMode mode) {
  current_mode_ = mode;

  if (mode == CollaborationMode::kLocal) {
    auto session_or = local_coordinator_->JoinSession(session_code);
    if (!session_or.ok())
      return session_or.status();

    SessionInfo info;
    info.session_id = session_or->session_id;
    info.session_name = session_or->session_name;
    info.participants = session_or->participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Joined local session: %s", session_code),
          ToastType::kSuccess, 3.0f);
    }

    return info;
  }

#ifdef YAZE_WITH_GRPC
  if (mode == CollaborationMode::kNetwork) {
    if (!network_coordinator_) {
      return absl::FailedPreconditionError(
          "Network coordinator not initialized. Connect to a server first.");
    }

    const char* username = std::getenv("USER");
    if (!username) {
      username = std::getenv("USERNAME");
    }
    if (!username) {
      username = "unknown";
    }

    auto session_or = network_coordinator_->JoinSession(session_code, username);
    if (!session_or.ok())
      return session_or.status();

    SessionInfo info;
    info.session_id = session_or->session_id;
    info.session_name = session_or->session_name;
    info.participants = session_or->participants;

    in_session_ = true;
    current_session_id_ = info.session_id;
    current_session_name_ = info.session_name;
    current_participants_ = info.participants;

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Joined network session: %s", session_code),
          ToastType::kSuccess, 3.0f);
    }

    return info;
  }
#endif

  return absl::InvalidArgumentError("Unsupported collaboration mode");
}

absl::Status AgentEditor::LeaveSession() {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  if (current_mode_ == CollaborationMode::kLocal) {
    auto status = local_coordinator_->LeaveSession();
    if (!status.ok())
      return status;
  }
#ifdef YAZE_WITH_GRPC
  else if (current_mode_ == CollaborationMode::kNetwork) {
    if (network_coordinator_) {
      auto status = network_coordinator_->LeaveSession();
      if (!status.ok())
        return status;
    }
  }
#endif

  in_session_ = false;
  current_session_id_.clear();
  current_session_name_.clear();
  current_participants_.clear();

  if (toast_manager_) {
    toast_manager_->Show("Left collaboration session", ToastType::kInfo, 3.0f);
  }

  return absl::OkStatus();
}

absl::StatusOr<AgentEditor::SessionInfo> AgentEditor::RefreshSession() {
  if (!in_session_) {
    return absl::FailedPreconditionError("Not in a session");
  }

  if (current_mode_ == CollaborationMode::kLocal) {
    auto session_or = local_coordinator_->RefreshSession();
    if (!session_or.ok())
      return session_or.status();

    SessionInfo info;
    info.session_id = session_or->session_id;
    info.session_name = session_or->session_name;
    info.participants = session_or->participants;
    current_participants_ = info.participants;
    return info;
  }

  SessionInfo info;
  info.session_id = current_session_id_;
  info.session_name = current_session_name_;
  info.participants = current_participants_;
  return info;
}

absl::Status AgentEditor::CaptureSnapshot(std::filesystem::path* output_path,
                                          const CaptureConfig& config) {
#ifdef YAZE_WITH_GRPC
  using yaze::test::CaptureActiveWindow;
  using yaze::test::CaptureHarnessScreenshot;
  using yaze::test::CaptureWindowByName;

  absl::StatusOr<yaze::test::ScreenshotArtifact> result;
  switch (config.mode) {
    case CaptureConfig::CaptureMode::kFullWindow:
      result = CaptureHarnessScreenshot("");
      break;
    case CaptureConfig::CaptureMode::kActiveEditor:
      result = CaptureActiveWindow("");
      if (!result.ok()) {
        result = CaptureHarnessScreenshot("");
      }
      break;
    case CaptureConfig::CaptureMode::kSpecificWindow: {
      if (!config.specific_window_name.empty()) {
        result = CaptureWindowByName(config.specific_window_name, "");
      } else {
        result = CaptureActiveWindow("");
      }
      if (!result.ok()) {
        result = CaptureHarnessScreenshot("");
      }
      break;
    }
  }

  if (!result.ok()) {
    return result.status();
  }
  *output_path = result->file_path;
  return absl::OkStatus();
#else
  (void)output_path;
  (void)config;
  return absl::UnimplementedError("Screenshot capture requires YAZE_WITH_GRPC");
#endif
}

absl::Status AgentEditor::SendToGemini(const std::filesystem::path& image_path,
                                       const std::string& prompt) {
#ifdef YAZE_WITH_GRPC
  const char* api_key = current_profile_.gemini_api_key.empty()
                            ? std::getenv("GEMINI_API_KEY")
                            : current_profile_.gemini_api_key.c_str();
  if (!api_key || std::strlen(api_key) == 0) {
    return absl::FailedPreconditionError(
        "Gemini API key not configured (set GEMINI_API_KEY)");
  }

  cli::GeminiConfig config;
  config.api_key = api_key;
  config.model = current_profile_.model.empty() ? "gemini-2.5-flash"
                                                : current_profile_.model;
  config.verbose = current_profile_.verbose;

  cli::GeminiAIService gemini_service(config);
  auto response =
      gemini_service.GenerateMultimodalResponse(image_path.string(), prompt);
  if (!response.ok()) {
    return response.status();
  }

  if (agent_chat_) {
    auto* service = agent_chat_->GetAgentService();
    if (service) {
      auto history = service->GetHistory();
      cli::agent::ChatMessage agent_msg;
      agent_msg.sender = cli::agent::ChatMessage::Sender::kAgent;
      agent_msg.message = response->text_response;
      agent_msg.timestamp = absl::Now();
      history.push_back(agent_msg);
      service->ReplaceHistory(history);
    }
  }

  if (toast_manager_) {
    toast_manager_->Show("Gemini vision response added to chat",
                         ToastType::kSuccess, 2.5f);
  }
  return absl::OkStatus();
#else
  (void)image_path;
  (void)prompt;
  return absl::UnimplementedError("Gemini integration requires YAZE_WITH_GRPC");
#endif
}

#ifdef YAZE_WITH_GRPC
absl::Status AgentEditor::ConnectToServer(const std::string& server_url) {
  try {
    network_coordinator_ =
        std::make_unique<NetworkCollaborationCoordinator>(server_url);

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Connected to server: %s", server_url),
          ToastType::kSuccess, 3.0f);
    }

    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to connect to server: %s", e.what()));
  }
}

void AgentEditor::DisconnectFromServer() {
  if (in_session_ && current_mode_ == CollaborationMode::kNetwork) {
    LeaveSession();
  }
  network_coordinator_.reset();

  if (toast_manager_) {
    toast_manager_->Show("Disconnected from server", ToastType::kInfo, 2.5f);
  }
}

bool AgentEditor::IsConnectedToServer() const {
  return network_coordinator_ && network_coordinator_->IsConnected();
}
#endif

bool AgentEditor::IsInSession() const {
  return in_session_;
}

AgentEditor::CollaborationMode AgentEditor::GetCurrentMode() const {
  return current_mode_;
}

std::optional<AgentEditor::SessionInfo> AgentEditor::GetCurrentSession() const {
  if (!in_session_)
    return std::nullopt;
  return SessionInfo{current_session_id_, current_session_name_,
                     current_participants_};
}

void AgentEditor::SetupMultimodalCallbacks() {}

void AgentEditor::SetupAutomationCallbacks() {}

}  // namespace editor
}  // namespace yaze
