#include <algorithm>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_chat.h"
#include "app/editor/agent/agent_editor.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/agent/panels/agent_configuration_panel.h"
#include "app/editor/agent/panels/feature_flag_editor_panel.h"
#include "app/editor/agent/panels/manifest_panel.h"
#include "app/editor/agent/panels/mesen_debug_panel.h"
#include "app/editor/agent/panels/mesen_screenshot_panel.h"
#include "app/editor/agent/panels/oracle_state_library_panel.h"
#include "app/editor/agent/panels/sram_viewer_panel.h"
#include "app/editor/system/user_settings.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/text_editor.h"
#include "app/platform/asset_loader.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/agent/tool_dispatcher.h"
#include "cli/service/ai/ai_config_utils.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "implot.h"
#include "rom/rom.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <TargetConditionals.h>
#endif

namespace yaze {
namespace editor {

namespace {

// TODO(B.2d): these three helpers are duplicated from the anonymous namespace
// in agent_editor.cc. When the model-service slice (RefreshModelCache,
// MaybeAutoDetectLocalProviders, ApplyUserSettingsDefaults + their helper
// dependencies) is extracted in a follow-up commit, consolidate these into a
// shared internal header so both TUs reach the same implementation.
std::optional<std::string> LoadKeychainValue(const std::string& key) {
#if defined(__APPLE__)
  if (key.empty()) {
    return std::nullopt;
  }
  CFStringRef key_ref = CFStringCreateWithCString(
      kCFAllocatorDefault, key.c_str(), kCFStringEncodingUTF8);
  const void* keys[] = {kSecClass, kSecAttrAccount, kSecReturnData,
                        kSecMatchLimit};
  const void* values[] = {kSecClassGenericPassword, key_ref, kCFBooleanTrue,
                          kSecMatchLimitOne};
  CFDictionaryRef query = CFDictionaryCreate(
      kCFAllocatorDefault, keys, values,
      static_cast<CFIndex>(sizeof(keys) / sizeof(keys[0])),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFTypeRef item = nullptr;
  OSStatus status = SecItemCopyMatching(query, &item);
  if (query) {
    CFRelease(query);
  }
  if (key_ref) {
    CFRelease(key_ref);
  }
  if (status == errSecItemNotFound) {
    return std::nullopt;
  }
  if (status != errSecSuccess || !item) {
    if (item) {
      CFRelease(item);
    }
    return std::nullopt;
  }
  CFDataRef data_ref = static_cast<CFDataRef>(item);
  const UInt8* data_ptr = CFDataGetBytePtr(data_ref);
  CFIndex data_len = CFDataGetLength(data_ref);
  std::string value(reinterpret_cast<const char*>(data_ptr),
                    static_cast<size_t>(data_len));
  CFRelease(item);
  return value;
#else
  (void)key;
  return std::nullopt;
#endif
}

std::string ResolveHostApiKey(const UserSettings::Preferences* prefs,
                              const UserSettings::Preferences::AiHost& host) {
  if (!host.api_key.empty()) {
    return host.api_key;
  }
  if (!host.credential_id.empty()) {
    if (auto key = LoadKeychainValue(host.credential_id)) {
      return *key;
    }
  }
  if (!prefs) {
    return {};
  }
  std::string api_type = host.api_type.empty() ? "openai" : host.api_type;
  if (api_type == "lmstudio") {
    api_type = "openai";
  }
  if (api_type == "openai") {
    return prefs->openai_api_key;
  }
  if (api_type == "gemini") {
    return prefs->gemini_api_key;
  }
  if (api_type == "anthropic") {
    return prefs->anthropic_api_key;
  }
  return {};
}

void ApplyHostPresetToProfile(AgentEditor::BotProfile* profile,
                              const UserSettings::Preferences::AiHost& host,
                              const UserSettings::Preferences* prefs) {
  if (!profile) {
    return;
  }
  std::string api_key = ResolveHostApiKey(prefs, host);
  profile->host_id = host.id;
  std::string api_type = host.api_type;
  if (api_type == "lmstudio") {
    api_type = "openai";
  }
  if (api_type == "openai" || api_type == "ollama" || api_type == "gemini" ||
      api_type == "anthropic") {
    profile->provider = api_type;
  }
  if (profile->provider == "openai") {
    if (!host.base_url.empty()) {
      profile->openai_base_url = cli::NormalizeOpenAiBaseUrl(host.base_url);
    }
    if (!api_key.empty()) {
      profile->openai_api_key = api_key;
    }
  } else if (profile->provider == "ollama") {
    if (!host.base_url.empty()) {
      profile->ollama_host = host.base_url;
    }
  } else if (profile->provider == "gemini") {
    if (!api_key.empty()) {
      profile->gemini_api_key = api_key;
    }
  } else if (profile->provider == "anthropic") {
    if (!api_key.empty()) {
      profile->anthropic_api_key = api_key;
    }
  }
}

}  // namespace

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
  if (!context_) {
    ImGui::TextDisabled("Agent configuration unavailable.");
    ImGui::TextWrapped("Initialize the Agent UI to edit provider settings.");
    return;
  }

  const auto& theme = AgentUI::GetTheme();

  if (dependencies_.user_settings) {
    const auto& prefs = dependencies_.user_settings->prefs();
    if (!prefs.ai_hosts.empty()) {
      AgentUI::RenderSectionHeader(ICON_MD_STORAGE, "Host Presets",
                                   theme.accent_color);
      const auto& hosts = prefs.ai_hosts;
      std::string active_id = current_profile_.host_id.empty()
                                  ? prefs.active_ai_host_id
                                  : current_profile_.host_id;
      int active_index = -1;
      for (size_t i = 0; i < hosts.size(); ++i) {
        if (!active_id.empty() && hosts[i].id == active_id) {
          active_index = static_cast<int>(i);
          break;
        }
      }
      const char* preview = (active_index >= 0)
                                ? hosts[active_index].label.c_str()
                                : "Select host";
      if (ImGui::BeginCombo("##ai_host_preset", preview)) {
        for (size_t i = 0; i < hosts.size(); ++i) {
          const bool selected = (static_cast<int>(i) == active_index);
          if (ImGui::Selectable(hosts[i].label.c_str(), selected)) {
            ApplyHostPresetToProfile(&current_profile_, hosts[i], &prefs);
            MarkProfileUiDirty();
            SyncContextFromProfile();
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      if (active_index >= 0) {
        const auto& host = hosts[active_index];
        ImGui::TextDisabled("Active host: %s", host.label.c_str());
        ImGui::TextDisabled("Endpoint: %s", host.base_url.c_str());
        ImGui::TextDisabled("API type: %s", host.api_type.empty()
                                                ? "openai"
                                                : host.api_type.c_str());
      } else {
        ImGui::TextDisabled(
            "Host presets come from settings.json (Documents/Yaze).");
      }
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
    }
  }

  AgentConfigPanel::Callbacks callbacks;
  callbacks.update_config = [this](const AgentConfigState& config) {
    ApplyConfigFromContext(config);
  };
  callbacks.refresh_models = [this](bool force) {
    RefreshModelCache(force);
  };
  callbacks.apply_preset = [this](const ModelPreset& preset) {
    ApplyModelPreset(preset);
  };
  callbacks.apply_tool_preferences = [this]() {
    ApplyToolPreferencesFromContext();
  };

  if (config_panel_) {
    config_panel_->Draw(context_, callbacks, toast_manager_);
  }
}

void AgentEditor::DrawStatusPanel() {
  const auto& theme = AgentUI::GetTheme();

  AgentUI::PushPanelStyle();
  if (ImGui::BeginChild("AgentStatusCard", ImVec2(0, 150), true)) {
    AgentUI::RenderSectionHeader(ICON_MD_CHAT, "Agent Status",
                                 theme.accent_color);

    bool chat_active = agent_chat_ && *agent_chat_->active();
    if (ImGui::BeginTable("AgentStatusTable", 2,
                          ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("Chat");
      ImGui::TableSetColumnIndex(1);
      AgentUI::RenderStatusIndicator(chat_active ? "Active" : "Inactive",
                                     chat_active);
      if (!chat_active) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Open")) {
          OpenChatWindow();
        }
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("Provider");
      ImGui::TableSetColumnIndex(1);
      AgentUI::RenderProviderBadge(current_profile_.provider.c_str());
      if (!current_profile_.model.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", current_profile_.model.c_str());
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("ROM");
      ImGui::TableSetColumnIndex(1);
      if (rom_ && rom_->is_loaded()) {
        ImGui::TextColored(theme.status_success,
                           ICON_MD_CHECK_CIRCLE " Loaded");
        ImGui::SameLine();
        ImGui::TextDisabled("Tools ready");
      } else {
        ImGui::TextColored(theme.status_warning, ICON_MD_WARNING " Not Loaded");
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();

  ImGui::Spacing();

  if (ImGui::BeginChild("AgentMetricsCard", ImVec2(0, 170), true)) {
    AgentUI::RenderSectionHeader(ICON_MD_ANALYTICS, "Session Metrics",
                                 theme.accent_color);
    if (agent_chat_) {
      auto metrics = agent_chat_->GetAgentService()->GetMetrics();
      ImGui::TextDisabled("Messages: %d user / %d agent",
                          metrics.total_user_messages,
                          metrics.total_agent_messages);
      ImGui::TextDisabled("Tool calls: %d  Proposals: %d  Commands: %d",
                          metrics.total_tool_calls, metrics.total_proposals,
                          metrics.total_commands);
      ImGui::TextDisabled("Avg latency: %.2fs  Elapsed: %.2fs",
                          metrics.average_latency_seconds,
                          metrics.total_elapsed_seconds);

      std::vector<double> latencies;
      for (const auto& msg : agent_chat_->GetAgentService()->GetHistory()) {
        if (msg.sender == cli::agent::ChatMessage::Sender::kAgent &&
            msg.model_metadata.has_value() &&
            msg.model_metadata->latency_seconds > 0.0) {
          latencies.push_back(msg.model_metadata->latency_seconds);
        }
      }
      if (latencies.size() > 30) {
        latencies.erase(latencies.begin(),
                        latencies.end() - static_cast<long>(30));
      }
      if (!latencies.empty()) {
        std::vector<double> xs(latencies.size());
        for (size_t i = 0; i < xs.size(); ++i) {
          xs[i] = static_cast<double>(i);
        }
        ImPlotFlags plot_flags = ImPlotFlags_NoLegend | ImPlotFlags_NoMenus |
                                 ImPlotFlags_NoBoxSelect;
        if (ImPlot::BeginPlot("##LatencyPlot", ImVec2(-1, 90), plot_flags)) {
          ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations,
                            ImPlotAxisFlags_NoDecorations);
          ImPlot::SetupAxisLimits(ImAxis_X1, 0, xs.back(), ImGuiCond_Always);
          double max_latency =
              *std::max_element(latencies.begin(), latencies.end());
          ImPlot::SetupAxisLimits(ImAxis_Y1, 0, max_latency * 1.2,
                                  ImGuiCond_Always);
          ImPlot::PlotLine("Latency", xs.data(), latencies.data(),
                           static_cast<int>(latencies.size()));
          ImPlot::EndPlot();
        }
      }
    } else {
      ImGui::TextDisabled("Initialize the chat system to see metrics.");
    }
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
    MarkProfileUiDirty();
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
      {
        gui::StyleColorGuard del_guard(ImGuiCol_Button, theme.status_warning);
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
      }

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

      gui::ColoredTextF(color, "%s:", from_user ? "User" : "Agent");

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

  {
    gui::StyleColorGuard save_guard(ImGuiCol_Button, theme.status_success);
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
  }

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

  int stage_index =
      std::clamp(builder_state_.active_stage, 0,
                 static_cast<int>(builder_state_.stages.size()) - 1);
  int completed_stages = 0;
  for (const auto& stage : builder_state_.stages) {
    if (stage.completed) {
      ++completed_stages;
    }
  }
  float completion_ratio =
      builder_state_.stages.empty()
          ? 0.0f
          : static_cast<float>(completed_stages) /
                static_cast<float>(builder_state_.stages.size());
  auto truncate_summary = [](const std::string& text) {
    constexpr size_t kMaxLen = 64;
    if (text.size() <= kMaxLen) {
      return text;
    }
    return text.substr(0, kMaxLen - 3) + "...";
  };

  const float left_width =
      std::min(260.0f, ImGui::GetContentRegionAvail().x * 0.32f);

  ImGui::BeginChild("BuilderStages", ImVec2(left_width, 0), true);
  AgentUI::RenderSectionHeader(ICON_MD_LIST, "Stages", theme.accent_color);
  ImGui::TextDisabled("%d/%zu complete", completed_stages,
                      builder_state_.stages.size());
  ImGui::ProgressBar(completion_ratio, ImVec2(-1, 0));
  ImGui::Spacing();

  for (size_t i = 0; i < builder_state_.stages.size(); ++i) {
    auto& stage = builder_state_.stages[i];
    ImGui::PushID(static_cast<int>(i));
    bool selected = builder_state_.active_stage == static_cast<int>(i);
    if (ImGui::Selectable(stage.name.c_str(), selected)) {
      builder_state_.active_stage = static_cast<int>(i);
      stage_index = static_cast<int>(i);
    }
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f);
    ImGui::Checkbox("##stage_done", &stage.completed);
    ImGui::TextDisabled("%s", truncate_summary(stage.summary).c_str());
    ImGui::Separator();
    ImGui::PopID();
  }
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("BuilderDetails", ImVec2(0, 0), false);
  AgentUI::RenderSectionHeader(ICON_MD_AUTO_FIX_HIGH, "Stage Details",
                               theme.accent_color);
  if (stage_index >= 0 &&
      stage_index < static_cast<int>(builder_state_.stages.size())) {
    ImGui::TextColored(theme.text_secondary_color, "%s",
                       builder_state_.stages[stage_index].summary.c_str());
  }
  ImGui::Spacing();

  switch (stage_index) {
    case 0: {
      static std::string new_goal;
      ImGui::Text("Persona + Goals");
      ImGui::TextWrapped(
          "Define the agent's voice, boundaries, and success criteria. Keep "
          "goals short and action-focused.");
      ImGui::InputTextMultiline("##persona_notes",
                                &builder_state_.persona_notes, ImVec2(-1, 140));
      ImGui::Spacing();
      ImGui::TextDisabled("Add Goal");
      ImGui::InputTextWithHint("##goal_input",
                               "e.g. Review collision edge cases", &new_goal);
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
      ImGui::TextWrapped(
          "Enable only what the plan needs. Fewer tools = clearer responses.");
      auto tool_checkbox = [&](const char* label, bool* value,
                               const char* hint) {
        ImGui::Checkbox(label, value);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", hint);
      };
      tool_checkbox("Resources", &builder_state_.tools.resources,
                    "Project files, docs, refs");
      tool_checkbox("Dungeon", &builder_state_.tools.dungeon,
                    "Rooms, objects, entrances");
      tool_checkbox("Overworld", &builder_state_.tools.overworld,
                    "Maps, tile16, entities");
      tool_checkbox("Dialogue", &builder_state_.tools.dialogue,
                    "NPC text + scripts");
      tool_checkbox("GUI Automation", &builder_state_.tools.gui,
                    "Test harness + screenshots");
      tool_checkbox("Music", &builder_state_.tools.music, "Trackers + SPC");
      tool_checkbox("Sprite", &builder_state_.tools.sprite,
                    "Sprites + palettes");
      tool_checkbox("Emulator", &builder_state_.tools.emulator,
                    "Runtime probes");
      tool_checkbox("Memory Inspector", &builder_state_.tools.memory_inspector,
                    "RAM/SRAM watch + inspection");
      break;
    }
    case 2: {
      ImGui::Text("Automation");
      ImGui::TextWrapped(
          "Use automation to validate fixes quickly. Pair with gRPC harness "
          "for repeatable checks.");
      ImGui::Checkbox("Auto-run harness plan", &builder_state_.auto_run_tests);
      ImGui::Checkbox("Auto-sync ROM context", &builder_state_.auto_sync_rom);
      ImGui::Checkbox("Auto-focus proposal drawer",
                      &builder_state_.auto_focus_proposals);
      break;
    }
    case 3: {
      ImGui::Text("Validation Criteria");
      ImGui::TextWrapped(
          "Capture the acceptance criteria and what a passing run looks like.");
      ImGui::InputTextMultiline("##validation_notes",
                                &builder_state_.stages[stage_index].summary,
                                ImVec2(-1, 140));
      break;
    }
    case 4: {
      ImGui::Text("E2E Checklist");
      ImGui::ProgressBar(completion_ratio, ImVec2(-1, 0),
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

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextDisabled("Builder Output");
  ImGui::BulletText("Persona notes sync to the chat summary");
  ImGui::BulletText("Tool stack applies to the agent tool preferences");
  ImGui::BulletText("E2E readiness gates automation handoff");

  ImGui::Spacing();
  gui::StyleColorGuard apply_guard(ImGuiCol_Button, theme.accent_color);
  if (ImGui::Button(ICON_MD_LINK " Apply to Chat", ImVec2(-1, 0))) {
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
      prefs.memory_inspector = builder_state_.tools.memory_inspector;
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

  ImGui::Spacing();
  ImGui::InputTextWithHint("##blueprint_path",
                           "Path to blueprint (optional)...",
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
  ImGui::EndChild();
}

void AgentEditor::DrawMesenDebugPanel() {
  if (mesen_debug_panel_) {
    mesen_debug_panel_->Draw();
  } else {
    ImGui::TextDisabled("Mesen2 debug panel unavailable.");
  }
}

void AgentEditor::DrawMesenScreenshotPanel() {
  if (mesen_screenshot_panel_) {
    mesen_screenshot_panel_->Draw();
  } else {
    ImGui::TextDisabled("Mesen2 screenshot panel unavailable.");
  }
}

void AgentEditor::DrawOracleStatePanel() {
  if (oracle_state_panel_) {
    // Share the Mesen client if available
    if (mesen_debug_panel_ && mesen_debug_panel_->IsConnected()) {
      // The panels can share the client from the registry
    }
    oracle_state_panel_->Draw();
  } else {
    ImGui::TextDisabled("Oracle state panel unavailable.");
  }
}

void AgentEditor::DrawFeatureFlagPanel() {
  if (feature_flag_panel_) {
    // Wire up the project pointer so the panel can access the manifest
    feature_flag_panel_->SetProject(dependencies_.project);
    feature_flag_panel_->Draw();
  } else {
    ImGui::TextDisabled("Feature flag panel unavailable.");
  }
}

void AgentEditor::DrawManifestPanel() {
  if (manifest_panel_) {
    manifest_panel_->SetProject(dependencies_.project);
    manifest_panel_->Draw();
  } else {
    ImGui::TextDisabled("Manifest panel unavailable.");
  }
}

void AgentEditor::DrawSramViewerPanel() {
  if (sram_viewer_panel_) {
    sram_viewer_panel_->SetProject(dependencies_.project);
    sram_viewer_panel_->Draw();
  } else {
    ImGui::TextDisabled("SRAM viewer panel unavailable.");
  }
}

}  // namespace editor
}  // namespace yaze
