#include "app/editor/agent/agent_editor.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <unordered_set>

#include "app/editor/agent/agent_ui_theme.h"
// Centralized UI theme
#include "app/gui/style/theme.h"

#include "app/editor/system/panel_manager.h"
#include "app/gui/app/editor_layout.h"

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_chat.h"
#include "app/editor/agent/agent_collaboration_coordinator.h"
#include "app/editor/agent/panels/agent_configuration_panel.h"
#include "app/editor/agent/panels/mesen_debug_panel.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/user_settings.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/platform/asset_loader.h"
#include "app/service/screenshot_utils.h"
#include "app/test/test_manager.h"
#include "cli/service/agent/tool_dispatcher.h"
#include "cli/service/ai/ai_config_utils.h"
#include "cli/service/ai/service_factory.h"
#include "httplib.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "implot.h"
#include "rom/rom.h"
#include "util/file_util.h"
#include "util/platform_paths.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif
#endif

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

std::optional<std::string> LoadKeychainValue(const std::string& key);

template <size_t N>
void CopyStringToBuffer(const std::string& src, char (&dest)[N]) {
  std::strncpy(dest, src.c_str(), N - 1);
  dest[N - 1] = '\0';
}

std::filesystem::path ExpandUserPath(const std::string& input) {
  if (input.empty()) {
    return {};
  }
  if (input.front() != '~') {
    return std::filesystem::path(input);
  }
  const auto home_dir = util::PlatformPaths::GetHomeDirectory();
  if (home_dir.empty() || home_dir == ".") {
    return std::filesystem::path(input);
  }
  if (input.size() == 1) {
    return home_dir;
  }
  if (input[1] == '/' || input[1] == '\\') {
    return home_dir / input.substr(2);
  }
  return home_dir / input.substr(1);
}

bool HasModelExtension(const std::filesystem::path& path) {
  const std::string ext =
      absl::AsciiStrToLower(path.extension().string());
  return ext == ".gguf" || ext == ".ggml" || ext == ".bin" ||
         ext == ".safetensors";
}

void AddUniqueModelName(const std::string& name,
                        std::vector<std::string>* output,
                        std::unordered_set<std::string>* seen) {
  if (!output || !seen || name.empty()) {
    return;
  }
  if (output->size() >= 512) {
    return;
  }
  if (seen->insert(name).second) {
    output->push_back(name);
  }
}

bool IsOllamaModelsPath(const std::filesystem::path& path) {
  if (path.filename() != "models") {
    return false;
  }
  return path.parent_path().filename() == ".ollama";
}

void CollectOllamaManifestModels(const std::filesystem::path& models_root,
                                 std::vector<std::string>* output,
                                 std::unordered_set<std::string>* seen) {
  if (!output || !seen) {
    return;
  }
  std::error_code ec;
  const auto library_path =
      models_root / "manifests" / "registry.ollama.ai" / "library";
  if (!std::filesystem::exists(library_path, ec)) {
    return;
  }
  std::filesystem::directory_options options =
      std::filesystem::directory_options::skip_permission_denied;
  for (std::filesystem::recursive_directory_iterator it(library_path, options,
                                                        ec),
       end;
       it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (!it->is_regular_file(ec)) {
      continue;
    }
    const auto rel = it->path().lexically_relative(library_path);
    if (rel.empty()) {
      continue;
    }
    std::vector<std::string> parts;
    for (const auto& part : rel) {
      if (!part.empty()) {
        parts.push_back(part.string());
      }
    }
    if (parts.empty()) {
      continue;
    }
    std::string model = parts.front();
    std::string tag;
    for (size_t i = 1; i < parts.size(); ++i) {
      if (!tag.empty()) {
        tag += "/";
      }
      tag += parts[i];
    }
    const std::string name = tag.empty() ? model : model + ":" + tag;
    AddUniqueModelName(name, output, seen);
    if (output->size() >= 512) {
      return;
    }
  }
}

void CollectModelFiles(const std::filesystem::path& base_path,
                       std::vector<std::string>* output,
                       std::unordered_set<std::string>* seen) {
  if (!output || !seen) {
    return;
  }
  std::error_code ec;
  if (!std::filesystem::exists(base_path, ec)) {
    return;
  }
  std::filesystem::directory_options options =
      std::filesystem::directory_options::skip_permission_denied;
  constexpr int kMaxDepth = 4;
  for (std::filesystem::recursive_directory_iterator it(base_path, options,
                                                        ec),
       end;
       it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (it->is_directory(ec)) {
      if (it.depth() >= kMaxDepth) {
        it.disable_recursion_pending();
      }
      continue;
    }
    if (!it->is_regular_file(ec)) {
      continue;
    }
    if (!HasModelExtension(it->path())) {
      continue;
    }
    std::filesystem::path rel = it->path().lexically_relative(base_path);
    if (rel.empty()) {
      rel = it->path().filename();
    }
    rel.replace_extension();
    std::string name = rel.generic_string();
    AddUniqueModelName(name, output, seen);
    if (output->size() >= 512) {
      return;
    }
  }
}

std::vector<std::string> CollectLocalModelNames(
    const UserSettings::Preferences* prefs) {
  std::vector<std::string> results;
  if (!prefs) {
    return results;
  }
  std::unordered_set<std::string> seen;
  for (const auto& raw_path : prefs->ai_model_paths) {
    auto expanded = ExpandUserPath(raw_path);
    if (expanded.empty()) {
      continue;
    }
    std::error_code ec;
    if (!std::filesystem::exists(expanded, ec)) {
      continue;
    }
    if (std::filesystem::is_regular_file(expanded, ec)) {
      std::filesystem::path rel = expanded.filename();
      rel.replace_extension();
      AddUniqueModelName(rel.string(), &results, &seen);
      continue;
    }
    if (!std::filesystem::is_directory(expanded, ec)) {
      continue;
    }
    if (IsOllamaModelsPath(expanded)) {
      CollectOllamaManifestModels(expanded, &results, &seen);
      continue;
    }
    CollectModelFiles(expanded, &results, &seen);
  }
  std::sort(results.begin(), results.end());
  return results;
}

std::string ResolveHostApiKey(
    const UserSettings::Preferences* prefs,
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

std::string BuildTagsString(const std::vector<std::string>& tags) {
  std::string result;
  for (size_t i = 0; i < tags.size(); ++i) {
    if (i > 0) {
      result.append(", ");
    }
    result.append(tags[i]);
  }
  return result;
}

bool ContainsText(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

bool StartsWithText(const std::string& text, const std::string& prefix) {
  return text.rfind(prefix, 0) == 0;
}

bool IsLocalOpenAiBaseUrl(const std::string& base_url) {
  if (base_url.empty()) {
    return false;
  }
  std::string lower = absl::AsciiStrToLower(base_url);
  return ContainsText(lower, "localhost") ||
         ContainsText(lower, "127.0.0.1") ||
         ContainsText(lower, "0.0.0.0");
}

bool IsTailscaleEndpoint(const std::string& base_url) {
  if (base_url.empty()) {
    return false;
  }
  std::string lower = absl::AsciiStrToLower(base_url);
  return ContainsText(lower, ".ts.net") ||
         ContainsText(lower, "tailscale");
}

bool IsLocalOrTrustedEndpoint(const std::string& base_url, bool allow_insecure) {
  if (allow_insecure) {
    return true;
  }
  if (IsTailscaleEndpoint(base_url)) {
    return true;
  }
  if (base_url.empty()) {
    return false;
  }
  std::string lower = absl::AsciiStrToLower(base_url);
  return ContainsText(lower, "localhost") ||
         ContainsText(lower, "127.0.0.1") ||
         ContainsText(lower, "0.0.0.0") ||
         ContainsText(lower, "::1") ||
         ContainsText(lower, "192.168.") ||
         StartsWithText(lower, "10.") ||
         ContainsText(lower, "100.64.");
}

bool ProbeHttpEndpoint(const std::string& base_url, const char* path) {
  if (base_url.empty()) {
    return false;
  }
  httplib::Client client(base_url);
  client.set_connection_timeout(0, 200000);
  client.set_read_timeout(0, 250000);
  client.set_write_timeout(0, 250000);
  client.set_follow_location(true);
  auto response = client.Get(path);
  if (!response) {
    return false;
  }
  return response->status > 0 && response->status < 500;
}

bool ProbeOllamaHost(const std::string& base_url) {
  return ProbeHttpEndpoint(base_url, "/api/version");
}

bool ProbeOpenAICompatible(const std::string& base_url) {
  return ProbeHttpEndpoint(base_url, "/v1/models");
}

std::optional<std::string> LoadKeychainValue(const std::string& key) {
#if defined(__APPLE__) && !TARGET_OS_IPHONE
  if (key.empty()) {
    return std::nullopt;
  }
  CFStringRef key_ref =
      CFStringCreateWithCString(kCFAllocatorDefault, key.c_str(),
                                kCFStringEncodingUTF8);
  const void* keys[] = {kSecClass, kSecAttrAccount, kSecReturnData,
                        kSecMatchLimit};
  const void* values[] = {kSecClassGenericPassword, key_ref, kCFBooleanTrue,
                          kSecMatchLimitOne};
  CFDictionaryRef query =
      CFDictionaryCreate(kCFAllocatorDefault, keys, values,
                         static_cast<CFIndex>(sizeof(keys) / sizeof(keys[0])),
                         &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);
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

}  // namespace

AgentEditor::AgentEditor() {
  type_ = EditorType::kAgent;
  agent_chat_ = std::make_unique<AgentChat>();
  local_coordinator_ = std::make_unique<AgentCollaborationCoordinator>();
  config_panel_ = std::make_unique<AgentConfigPanel>();
  mesen_debug_panel_ = std::make_unique<MesenDebugPanel>();
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

    panel_manager->RegisterEditorPanel(
        std::make_unique<AgentMesenDebugPanel>(
            [this]() { DrawMesenDebugPanel(); }));

    if (agent_chat_) {
      agent_chat_->SetPanelOpener(
          [panel_manager](const std::string& panel_id) {
            if (!panel_id.empty()) {
              panel_manager->ShowPanel(panel_id);
            }
          });
    }
  }

  ApplyUserSettingsDefaults();
}

void AgentEditor::ApplyUserSettingsDefaults(bool force) {
  auto* settings = dependencies_.user_settings;
  if (!settings) {
    return;
  }
  const auto& prefs = settings->prefs();
  if (prefs.ai_hosts.empty() && prefs.ai_profiles.empty()) {
    return;
  }
  bool applied = false;
  if (!force) {
    if (!current_profile_.host_id.empty()) {
      return;
    }
    if (current_profile_.provider != "mock") {
      return;
    }
  }
  if (!prefs.ai_hosts.empty()) {
    const std::string& active_id =
        prefs.active_ai_host_id.empty() ? prefs.ai_hosts.front().id
                                        : prefs.active_ai_host_id;
    if (!active_id.empty()) {
      for (const auto& host : prefs.ai_hosts) {
        if (host.id == active_id) {
          ApplyHostPresetToProfile(&current_profile_, host, &prefs);
          applied = true;
          break;
        }
      }
    }
  }
  if (!prefs.ai_profiles.empty()) {
    const UserSettings::Preferences::AiModelProfile* active_profile = nullptr;
    if (!prefs.active_ai_profile.empty()) {
      for (const auto& profile : prefs.ai_profiles) {
        if (profile.name == prefs.active_ai_profile) {
          active_profile = &profile;
          break;
        }
      }
    }
    if (!active_profile) {
      active_profile = &prefs.ai_profiles.front();
    }
    if (active_profile && (force || current_profile_.model.empty())) {
      if (!active_profile->model.empty()) {
        current_profile_.model = active_profile->model;
        current_profile_.temperature = active_profile->temperature;
        current_profile_.top_p = active_profile->top_p;
        current_profile_.max_output_tokens = active_profile->max_output_tokens;
        applied = true;
      }
    }
  }
  if (current_profile_.openai_api_key.empty() &&
      !prefs.openai_api_key.empty()) {
    current_profile_.openai_api_key = prefs.openai_api_key;
    applied = true;
  }
  if (current_profile_.gemini_api_key.empty() &&
      !prefs.gemini_api_key.empty()) {
    current_profile_.gemini_api_key = prefs.gemini_api_key;
    applied = true;
  }
  if (current_profile_.anthropic_api_key.empty() &&
      !prefs.anthropic_api_key.empty()) {
    current_profile_.anthropic_api_key = prefs.anthropic_api_key;
    applied = true;
  }
  if (applied) {
    MarkProfileUiDirty();
    SyncConfigFromProfile();
  }
}

void AgentEditor::MarkProfileUiDirty() { profile_ui_state_.dirty = true; }

void AgentEditor::SyncProfileUiState() {
  if (!profile_ui_state_.dirty) {
    return;
  }
  auto& ui = profile_ui_state_;
  CopyStringToBuffer(current_profile_.model, ui.model_buf);
  CopyStringToBuffer(
      current_profile_.ollama_host.empty() ? "http://localhost:11434"
                                           : current_profile_.ollama_host,
      ui.ollama_host_buf);
  CopyStringToBuffer(current_profile_.gemini_api_key, ui.gemini_key_buf);
  CopyStringToBuffer(current_profile_.anthropic_api_key, ui.anthropic_key_buf);
  CopyStringToBuffer(current_profile_.openai_api_key, ui.openai_key_buf);
  CopyStringToBuffer(
      current_profile_.openai_base_url.empty() ? "https://api.openai.com"
                                               : current_profile_.openai_base_url,
      ui.openai_base_buf);
  CopyStringToBuffer(current_profile_.name, ui.name_buf);
  CopyStringToBuffer(current_profile_.description, ui.desc_buf);
  CopyStringToBuffer(BuildTagsString(current_profile_.tags), ui.tags_buf);
  ui.dirty = false;
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
  bool profile_updated = false;
  auto env_value = [](const char* key) -> std::string {
    const char* value = std::getenv(key);
    return value ? std::string(value) : std::string();
  };

  std::string env_openai_base = env_value("OPENAI_BASE_URL");
  if (env_openai_base.empty()) {
    env_openai_base = env_value("OPENAI_API_BASE");
  }
  std::string env_openai_model = env_value("OPENAI_MODEL");
  std::string env_ollama_host = env_value("OLLAMA_HOST");
  std::string env_ollama_model = env_value("OLLAMA_MODEL");
  std::string env_gemini_model = env_value("GEMINI_MODEL");
  std::string env_anthropic_model = env_value("ANTHROPIC_MODEL");

  if (!env_ollama_host.empty() &&
      current_profile_.ollama_host != env_ollama_host) {
    current_profile_.ollama_host = env_ollama_host;
    current_config_.ollama_host = env_ollama_host;
    profile_updated = true;
  }
  if (!env_openai_base.empty()) {
    std::string normalized_base = cli::NormalizeOpenAiBaseUrl(env_openai_base);
    if (current_profile_.openai_base_url.empty() ||
        cli::NormalizeOpenAiBaseUrl(current_profile_.openai_base_url) ==
            "https://api.openai.com") {
      current_profile_.openai_base_url = normalized_base;
      current_config_.openai_base_url = normalized_base;
      profile_updated = true;
    }
  }

  if (const char* gemini_key = std::getenv("GEMINI_API_KEY")) {
    current_profile_.gemini_api_key = gemini_key;
    current_config_.gemini_api_key = gemini_key;
    profile_updated = true;
  }

  if (const char* anthropic_key = std::getenv("ANTHROPIC_API_KEY")) {
    current_profile_.anthropic_api_key = anthropic_key;
    current_config_.anthropic_api_key = anthropic_key;
    profile_updated = true;
  }

  if (const char* openai_key = std::getenv("OPENAI_API_KEY")) {
    current_profile_.openai_api_key = openai_key;
    current_config_.openai_api_key = openai_key;
    profile_updated = true;
  }

  bool provider_is_default =
      current_profile_.provider == "mock" &&
      current_profile_.host_id.empty();

  if (provider_is_default) {
    if (!current_profile_.gemini_api_key.empty()) {
      current_profile_.provider = "gemini";
      current_config_.provider = "gemini";
      if (current_profile_.model.empty()) {
        current_profile_.model =
            env_gemini_model.empty() ? "gemini-2.5-flash" : env_gemini_model;
        current_config_.model = current_profile_.model;
      }
      profile_updated = true;
    } else if (!current_profile_.anthropic_api_key.empty()) {
      current_profile_.provider = "anthropic";
      current_config_.provider = "anthropic";
      if (current_profile_.model.empty()) {
        current_profile_.model = env_anthropic_model.empty()
                                     ? "claude-3-5-sonnet-20241022"
                                     : env_anthropic_model;
        current_config_.model = current_profile_.model;
      }
      profile_updated = true;
    } else if (!current_profile_.openai_api_key.empty() ||
               !env_openai_base.empty()) {
      current_profile_.provider = "openai";
      current_config_.provider = "openai";
      if (current_profile_.model.empty()) {
        if (!env_openai_model.empty()) {
          current_profile_.model = env_openai_model;
        } else if (!current_profile_.openai_api_key.empty()) {
          current_profile_.model = "gpt-4o-mini";
        }
        current_config_.model = current_profile_.model;
      }
      profile_updated = true;
    } else if (!env_ollama_host.empty() || !env_ollama_model.empty()) {
      current_profile_.provider = "ollama";
      current_config_.provider = "ollama";
      if (current_profile_.model.empty() && !env_ollama_model.empty()) {
        current_profile_.model = env_ollama_model;
        current_config_.model = current_profile_.model;
      }
      profile_updated = true;
    }
  }

  if (current_profile_.provider == "ollama" && current_profile_.model.empty() &&
      !env_ollama_model.empty()) {
    current_profile_.model = env_ollama_model;
    current_config_.model = env_ollama_model;
    profile_updated = true;
  }
  if (current_profile_.provider == "openai" && current_profile_.model.empty() &&
      !env_openai_model.empty()) {
    current_profile_.model = env_openai_model;
    current_config_.model = env_openai_model;
    profile_updated = true;
  }
  if (current_profile_.provider == "anthropic" &&
      current_profile_.model.empty() && !env_anthropic_model.empty()) {
    current_profile_.model = env_anthropic_model;
    current_config_.model = env_anthropic_model;
    profile_updated = true;
  }
  if (current_profile_.provider == "gemini" && current_profile_.model.empty() &&
      !env_gemini_model.empty()) {
    current_profile_.model = env_gemini_model;
    current_config_.model = env_gemini_model;
    profile_updated = true;
  }
  if (profile_updated) {
    MarkProfileUiDirty();
  }

  if (MaybeAutoDetectLocalProviders()) {
    profile_updated = true;
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

void AgentEditor::SetContext(AgentUIContext* context) {
  context_ = context;
  if (agent_chat_) {
    agent_chat_->SetContext(context_);
  }
  SyncContextFromProfile();
}

void AgentEditor::SyncConfigFromProfile() {
  current_config_.provider = current_profile_.provider;
  current_config_.model = current_profile_.model;
  current_config_.ollama_host = current_profile_.ollama_host;
  current_config_.gemini_api_key = current_profile_.gemini_api_key;
  current_config_.anthropic_api_key = current_profile_.anthropic_api_key;
  current_config_.openai_api_key = current_profile_.openai_api_key;
  current_config_.openai_base_url =
      cli::NormalizeOpenAiBaseUrl(current_profile_.openai_base_url);
  current_config_.verbose = current_profile_.verbose;
  current_config_.show_reasoning = current_profile_.show_reasoning;
  current_config_.max_tool_iterations = current_profile_.max_tool_iterations;
  current_config_.max_retry_attempts = current_profile_.max_retry_attempts;
  current_config_.temperature = current_profile_.temperature;
  current_config_.top_p = current_profile_.top_p;
  current_config_.max_output_tokens = current_profile_.max_output_tokens;
  current_config_.stream_responses = current_profile_.stream_responses;
}

void AgentEditor::SyncContextFromProfile() {
  if (!context_) {
    return;
  }

  auto& ctx_config = context_->agent_config();
  ctx_config.ai_provider =
      current_profile_.provider.empty() ? "mock" : current_profile_.provider;
  ctx_config.ai_model = current_profile_.model;
  ctx_config.ollama_host = current_profile_.ollama_host.empty()
                               ? "http://localhost:11434"
                               : current_profile_.ollama_host;
  ctx_config.gemini_api_key = current_profile_.gemini_api_key;
  ctx_config.anthropic_api_key = current_profile_.anthropic_api_key;
  ctx_config.openai_api_key = current_profile_.openai_api_key;
  ctx_config.openai_base_url =
      cli::NormalizeOpenAiBaseUrl(current_profile_.openai_base_url);
  current_profile_.openai_base_url = ctx_config.openai_base_url;
  ctx_config.host_id = current_profile_.host_id;
  ctx_config.verbose = current_profile_.verbose;
  ctx_config.show_reasoning = current_profile_.show_reasoning;
  ctx_config.max_tool_iterations = current_profile_.max_tool_iterations;
  ctx_config.max_retry_attempts = current_profile_.max_retry_attempts;
  ctx_config.temperature = current_profile_.temperature;
  ctx_config.top_p = current_profile_.top_p;
  ctx_config.max_output_tokens = current_profile_.max_output_tokens;
  ctx_config.stream_responses = current_profile_.stream_responses;

  CopyStringToBuffer(ctx_config.ai_provider, ctx_config.provider_buffer);
  CopyStringToBuffer(ctx_config.ai_model, ctx_config.model_buffer);
  CopyStringToBuffer(ctx_config.ollama_host, ctx_config.ollama_host_buffer);
  CopyStringToBuffer(ctx_config.gemini_api_key, ctx_config.gemini_key_buffer);
  CopyStringToBuffer(ctx_config.anthropic_api_key,
                     ctx_config.anthropic_key_buffer);
  CopyStringToBuffer(ctx_config.openai_api_key, ctx_config.openai_key_buffer);
  CopyStringToBuffer(ctx_config.openai_base_url,
                     ctx_config.openai_base_url_buffer);

  SyncConfigFromProfile();

  context_->NotifyChanged();
}

void AgentEditor::ApplyConfigFromContext(const AgentConfigState& config) {
  if (!context_) {
    return;
  }

  auto& ctx_config = context_->agent_config();
  const std::string prev_provider = ctx_config.ai_provider;
  const std::string prev_openai_base = ctx_config.openai_base_url;
  const std::string prev_ollama_host = ctx_config.ollama_host;
  ctx_config.ai_provider = config.ai_provider.empty() ? "mock" : config.ai_provider;
  ctx_config.ai_model = config.ai_model;
  ctx_config.ollama_host =
      config.ollama_host.empty() ? "http://localhost:11434" : config.ollama_host;
  ctx_config.gemini_api_key = config.gemini_api_key;
  ctx_config.anthropic_api_key = config.anthropic_api_key;
  ctx_config.openai_api_key = config.openai_api_key;
  ctx_config.openai_base_url = cli::NormalizeOpenAiBaseUrl(config.openai_base_url);
  ctx_config.host_id = config.host_id;
  ctx_config.verbose = config.verbose;
  ctx_config.show_reasoning = config.show_reasoning;
  ctx_config.max_tool_iterations = config.max_tool_iterations;
  ctx_config.max_retry_attempts = config.max_retry_attempts;
  ctx_config.temperature = config.temperature;
  ctx_config.top_p = config.top_p;
  ctx_config.max_output_tokens = config.max_output_tokens;
  ctx_config.stream_responses = config.stream_responses;
  ctx_config.favorite_models = config.favorite_models;
  ctx_config.model_chain = config.model_chain;
  ctx_config.model_presets = config.model_presets;
  ctx_config.chain_mode = config.chain_mode;
  ctx_config.tool_config = config.tool_config;

  if (prev_provider != ctx_config.ai_provider ||
      prev_openai_base != ctx_config.openai_base_url ||
      prev_ollama_host != ctx_config.ollama_host) {
    auto& model_cache = context_->model_cache();
    model_cache.available_models.clear();
    model_cache.model_names.clear();
    model_cache.last_refresh = absl::InfinitePast();
    model_cache.auto_refresh_requested = false;
    model_cache.last_provider = ctx_config.ai_provider;
    model_cache.last_openai_base = ctx_config.openai_base_url;
    model_cache.last_ollama_host = ctx_config.ollama_host;
  }

  CopyStringToBuffer(ctx_config.ai_provider, ctx_config.provider_buffer);
  CopyStringToBuffer(ctx_config.ai_model, ctx_config.model_buffer);
  CopyStringToBuffer(ctx_config.ollama_host, ctx_config.ollama_host_buffer);
  CopyStringToBuffer(ctx_config.gemini_api_key, ctx_config.gemini_key_buffer);
  CopyStringToBuffer(ctx_config.anthropic_api_key,
                     ctx_config.anthropic_key_buffer);
  CopyStringToBuffer(ctx_config.openai_api_key, ctx_config.openai_key_buffer);
  CopyStringToBuffer(ctx_config.openai_base_url,
                     ctx_config.openai_base_url_buffer);

  current_profile_.provider = ctx_config.ai_provider;
  current_profile_.model = ctx_config.ai_model;
  current_profile_.ollama_host = ctx_config.ollama_host;
  current_profile_.gemini_api_key = ctx_config.gemini_api_key;
  current_profile_.anthropic_api_key = ctx_config.anthropic_api_key;
  current_profile_.openai_api_key = ctx_config.openai_api_key;
  current_profile_.openai_base_url = ctx_config.openai_base_url;
  current_profile_.host_id = ctx_config.host_id;
  current_profile_.verbose = ctx_config.verbose;
  current_profile_.show_reasoning = ctx_config.show_reasoning;
  current_profile_.max_tool_iterations = ctx_config.max_tool_iterations;
  current_profile_.max_retry_attempts = ctx_config.max_retry_attempts;
  current_profile_.temperature = ctx_config.temperature;
  current_profile_.top_p = ctx_config.top_p;
  current_profile_.max_output_tokens = ctx_config.max_output_tokens;
  current_profile_.stream_responses = ctx_config.stream_responses;
  current_profile_.modified_at = absl::Now();

  MarkProfileUiDirty();

  current_config_.provider = ctx_config.ai_provider;
  current_config_.model = ctx_config.ai_model;
  current_config_.ollama_host = ctx_config.ollama_host;
  current_config_.gemini_api_key = ctx_config.gemini_api_key;
  current_config_.openai_api_key = ctx_config.openai_api_key;
  current_config_.openai_base_url = ctx_config.openai_base_url;
  current_config_.verbose = ctx_config.verbose;
  current_config_.show_reasoning = ctx_config.show_reasoning;
  current_config_.max_tool_iterations = ctx_config.max_tool_iterations;
  current_config_.max_retry_attempts = ctx_config.max_retry_attempts;
  current_config_.temperature = ctx_config.temperature;
  current_config_.top_p = ctx_config.top_p;
  current_config_.max_output_tokens = ctx_config.max_output_tokens;
  current_config_.stream_responses = ctx_config.stream_responses;

  ApplyConfig(current_config_);
  context_->NotifyChanged();
}

void AgentEditor::ApplyToolPreferencesFromContext() {
  if (!context_ || !agent_chat_) {
    return;
  }
  auto* service = agent_chat_->GetAgentService();
  if (!service) {
    return;
  }

  const auto& tool_config = context_->agent_config().tool_config;
  cli::agent::ToolDispatcher::ToolPreferences prefs;
  prefs.resources = tool_config.resources;
  prefs.dungeon = tool_config.dungeon;
  prefs.overworld = tool_config.overworld;
  prefs.messages = tool_config.messages;
  prefs.dialogue = tool_config.dialogue;
  prefs.gui = tool_config.gui;
  prefs.music = tool_config.music;
  prefs.sprite = tool_config.sprite;
#ifdef YAZE_WITH_GRPC
  prefs.emulator = tool_config.emulator;
#else
  prefs.emulator = false;
#endif
  prefs.memory_inspector = tool_config.memory_inspector;
  service->SetToolPreferences(prefs);
}

void AgentEditor::RefreshModelCache(bool force) {
  if (!context_) {
    return;
  }

  auto& model_cache = context_->model_cache();
  if (model_cache.loading) {
    return;
  }
  if (!force && model_cache.last_refresh != absl::InfinitePast()) {
    absl::Duration since_refresh = absl::Now() - model_cache.last_refresh;
    if (since_refresh < absl::Seconds(15)) {
      return;
    }
  }

  model_cache.loading = true;
  model_cache.auto_refresh_requested = true;
  model_cache.available_models.clear();
  model_cache.model_names.clear();
  if (dependencies_.user_settings) {
    const auto& prefs = dependencies_.user_settings->prefs();
    bool needs_local_refresh = force;
    if (!needs_local_refresh) {
      if (prefs.ai_model_paths != last_local_model_paths_) {
        needs_local_refresh = true;
      } else if (last_local_model_scan_ == absl::InfinitePast() ||
                 (absl::Now() - last_local_model_scan_) >
                     absl::Seconds(30)) {
        needs_local_refresh = true;
      }
    }
    if (needs_local_refresh) {
      model_cache.local_model_names = CollectLocalModelNames(&prefs);
      last_local_model_paths_ = prefs.ai_model_paths;
      last_local_model_scan_ = absl::Now();
    }
  } else {
    model_cache.local_model_names.clear();
  }

  const auto& config = context_->agent_config();
  ModelServiceKey next_key;
  next_key.provider = config.ai_provider.empty() ? "mock" : config.ai_provider;
  next_key.model = config.ai_model;
  next_key.ollama_host = config.ollama_host;
  next_key.gemini_api_key = config.gemini_api_key;
  next_key.anthropic_api_key = config.anthropic_api_key;
  next_key.openai_api_key = config.openai_api_key;
  next_key.openai_base_url =
      cli::NormalizeOpenAiBaseUrl(config.openai_base_url);
  next_key.verbose = config.verbose;

  auto same_key = [](const ModelServiceKey& a, const ModelServiceKey& b) {
    return a.provider == b.provider && a.model == b.model &&
           a.ollama_host == b.ollama_host &&
           a.gemini_api_key == b.gemini_api_key &&
           a.anthropic_api_key == b.anthropic_api_key &&
           a.openai_api_key == b.openai_api_key &&
           a.openai_base_url == b.openai_base_url && a.verbose == b.verbose;
  };

  if (next_key.provider == "mock") {
    model_cache.loading = false;
    model_cache.model_names = model_cache.local_model_names;
    model_cache.last_refresh = absl::Now();
    return;
  }

  if (!model_service_ || !same_key(next_key, last_model_service_key_)) {
    cli::AIServiceConfig service_config;
    service_config.provider = next_key.provider;
    service_config.model = next_key.model;
    service_config.ollama_host = next_key.ollama_host;
    service_config.gemini_api_key = next_key.gemini_api_key;
    service_config.anthropic_api_key = next_key.anthropic_api_key;
    service_config.openai_api_key = next_key.openai_api_key;
    service_config.openai_base_url = next_key.openai_base_url;
    service_config.verbose = next_key.verbose;

    auto service_or = cli::CreateAIServiceStrict(service_config);
    if (!service_or.ok()) {
      model_service_.reset();
      model_cache.loading = false;
      model_cache.model_names = model_cache.local_model_names;
      model_cache.last_refresh = absl::Now();
      if (toast_manager_) {
        toast_manager_->Show(std::string(service_or.status().message()),
                             ToastType::kWarning, 2.0f);
      }
      return;
    }
    model_service_ = std::move(service_or.value());
    last_model_service_key_ = next_key;
  }

  auto models_or = model_service_->ListAvailableModels();
  if (!models_or.ok()) {
    model_cache.loading = false;
    model_cache.model_names = model_cache.local_model_names;
    model_cache.last_refresh = absl::Now();
    if (toast_manager_) {
      toast_manager_->Show(std::string(models_or.status().message()),
                           ToastType::kWarning, 2.0f);
    }
    return;
  }

  model_cache.available_models = models_or.value();
  std::unordered_set<std::string> seen;
  for (const auto& info : model_cache.available_models) {
    if (!info.name.empty()) {
      AddUniqueModelName(info.name, &model_cache.model_names, &seen);
    }
  }
  std::sort(model_cache.model_names.begin(), model_cache.model_names.end());
  if (context_->agent_config().ai_model.empty()) {
    auto& ctx_config = context_->agent_config();
    std::string selected;
    for (const auto& info : model_cache.available_models) {
      if (ctx_config.ai_provider.empty() ||
          info.provider == ctx_config.ai_provider) {
        selected = info.name;
        break;
      }
    }
    if (selected.empty() && !model_cache.model_names.empty()) {
      selected = model_cache.model_names.front();
    }
    if (!selected.empty()) {
      ctx_config.ai_model = selected;
      CopyStringToBuffer(ctx_config.ai_model, ctx_config.model_buffer);
    }
  }
  model_cache.last_refresh = absl::Now();
  model_cache.loading = false;
}

void AgentEditor::ApplyModelPreset(const ModelPreset& preset) {
  if (!context_) {
    return;
  }

  auto& config = context_->agent_config();
  if (!preset.provider.empty()) {
    config.ai_provider = preset.provider;
  }
  if (!preset.model.empty()) {
    config.ai_model = preset.model;
  }
  if (!preset.host.empty()) {
    if (config.ai_provider == "ollama") {
      config.ollama_host = preset.host;
    } else if (config.ai_provider == "openai") {
      config.openai_base_url = cli::NormalizeOpenAiBaseUrl(preset.host);
    }
  }

  for (auto& entry : config.model_presets) {
    if (entry.name == preset.name) {
      entry.last_used = absl::Now();
      break;
    }
  }

  CopyStringToBuffer(config.ai_provider, config.provider_buffer);
  CopyStringToBuffer(config.ai_model, config.model_buffer);
  CopyStringToBuffer(config.ollama_host, config.ollama_host_buffer);
  CopyStringToBuffer(config.openai_base_url, config.openai_base_url_buffer);

  ApplyConfigFromContext(config);
}

bool AgentEditor::MaybeAutoDetectLocalProviders() {
  if (auto_probe_done_) {
    return false;
  }
  auto_probe_done_ = true;

  auto* settings = dependencies_.user_settings;
  if (!settings) {
    return false;
  }
  const auto& prefs = settings->prefs();
  if (prefs.ai_hosts.empty()) {
    return false;
  }
  if (!current_profile_.host_id.empty() ||
      current_profile_.provider != "mock") {
    return false;
  }

  auto build_host = [&](const UserSettings::Preferences::AiHost& host) {
    auto resolved = host;
    if (resolved.api_key.empty()) {
      resolved.api_key = ResolveHostApiKey(&prefs, host);
    }
    return resolved;
  };

  auto select_host = [&](const UserSettings::Preferences::AiHost& host) {
    ApplyHostPresetToProfile(&current_profile_, host, &prefs);
    SyncConfigFromProfile();
    ApplyConfig(current_config_);
    MarkProfileUiDirty();
    if (context_) {
      SyncContextFromProfile();
    }
    return true;
  };

  auto try_host = [&](const UserSettings::Preferences::AiHost& host,
                      bool probe_only_local) {
    std::string api_type =
        host.api_type.empty() ? "openai" : host.api_type;
    if (api_type == "lmstudio") {
      api_type = "openai";
    }
    auto resolved = build_host(host);
    bool has_key = !resolved.api_key.empty();

    if (api_type == "ollama") {
      if (resolved.base_url.empty()) {
        return false;
      }
      if (probe_only_local &&
          !IsLocalOrTrustedEndpoint(resolved.base_url,
                                    resolved.allow_insecure)) {
        return false;
      }
      if (!ProbeOllamaHost(resolved.base_url)) {
        return false;
      }
      return select_host(resolved);
    }

    if (api_type == "openai") {
      if (resolved.base_url.empty()) {
        return false;
      }
      bool trusted =
          IsLocalOrTrustedEndpoint(resolved.base_url, resolved.allow_insecure);
      if (probe_only_local && !trusted) {
        return false;
      }
      if (trusted && ProbeOpenAICompatible(resolved.base_url)) {
        return select_host(resolved);
      }
      if (!probe_only_local && has_key) {
        return select_host(resolved);
      }
      return false;
    }

    if (api_type == "gemini") {
      if (!has_key) {
        return false;
      }
      return select_host(resolved);
    }

    if (api_type == "anthropic") {
      if (!has_key) {
        return false;
      }
      return select_host(resolved);
    }

    return false;
  };

  std::vector<const UserSettings::Preferences::AiHost*> candidates;
  if (!prefs.active_ai_host_id.empty()) {
    for (const auto& host : prefs.ai_hosts) {
      if (host.id == prefs.active_ai_host_id) {
        candidates.push_back(&host);
        break;
      }
    }
  }
  for (const auto& host : prefs.ai_hosts) {
    if (!candidates.empty() && candidates.front()->id == host.id) {
      continue;
    }
    candidates.push_back(&host);
  }

  for (const auto* host : candidates) {
    if (try_host(*host, true)) {
      return true;
    }
  }
  for (const auto* host : candidates) {
    if (try_host(*host, false)) {
      return true;
    }
  }

  return false;
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
        ImGui::TextDisabled("API type: %s",
                            host.api_type.empty() ? "openai"
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
  callbacks.refresh_models = [this](bool force) { RefreshModelCache(force); };
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
        ImGui::TextColored(theme.status_success, ICON_MD_CHECK_CIRCLE " Loaded");
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
          double max_latency = *std::max_element(latencies.begin(),
                                                 latencies.end());
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

  const float left_width = std::min(
      260.0f, ImGui::GetContentRegionAvail().x * 0.32f);

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
      ImGui::ProgressBar(
          completion_ratio, ImVec2(-1, 0),
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
  ImGui::PushStyleColor(ImGuiCol_Button, theme.accent_color);
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
  ImGui::PopStyleColor();

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
      {"memory_inspector", builder_state_.tools.memory_inspector},
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
    builder_state_.tools.memory_inspector = tools.value("memory_inspector", false);
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
  MarkProfileUiDirty();

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
  SyncContextFromProfile();
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
  MarkProfileUiDirty();
  SyncContextFromProfile();
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
  json["anthropic_api_key"] = profile.anthropic_api_key;
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
    profile.anthropic_api_key = json.value("anthropic_api_key", "");
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
      provider_config.anthropic_api_key = config.anthropic_api_key;
      provider_config.openai_api_key = config.openai_api_key;
      provider_config.openai_base_url =
          cli::NormalizeOpenAiBaseUrl(config.openai_base_url);
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
