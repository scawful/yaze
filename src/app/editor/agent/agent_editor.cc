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

#include "app/editor/system/workspace_window_manager.h"
#include "app/gui/app/editor_layout.h"

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_chat.h"
#include "app/editor/agent/agent_collaboration_coordinator.h"
#include "app/editor/agent/panels/agent_configuration_panel.h"
#include "app/editor/agent/panels/feature_flag_editor_panel.h"
#include "app/editor/agent/panels/manifest_panel.h"
#include "app/editor/agent/panels/mesen_debug_panel.h"
#include "app/editor/agent/panels/mesen_screenshot_panel.h"
#include "app/editor/agent/panels/oracle_state_library_panel.h"
#include "app/editor/agent/panels/sram_viewer_panel.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/user_settings.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/service/screenshot_utils.h"
#include "cli/service/agent/tool_dispatcher.h"
#include "cli/service/ai/ai_config_utils.h"
#include "cli/service/ai/service_factory.h"
#ifndef __EMSCRIPTEN__
#include "httplib.h"
#endif
#include "rom/rom.h"
#include "util/file_util.h"
#include "util/platform_paths.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <TargetConditionals.h>
#endif

#ifdef YAZE_WITH_GRPC
#include "app/editor/agent/network_collaboration_coordinator.h"
#endif

namespace yaze {
namespace editor {

namespace {

constexpr char kDefaultOpenAiBaseUrl[] = "https://api.openai.com";

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
  const std::string ext = absl::AsciiStrToLower(path.extension().string());
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
  for (std::filesystem::recursive_directory_iterator
           it(library_path, options, ec),
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
  for (std::filesystem::recursive_directory_iterator it(base_path, options, ec),
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
  return ContainsText(lower, "localhost") || ContainsText(lower, "127.0.0.1") ||
         ContainsText(lower, "0.0.0.0");
}

bool IsTailscaleEndpoint(const std::string& base_url) {
  if (base_url.empty()) {
    return false;
  }
  std::string lower = absl::AsciiStrToLower(base_url);
  return ContainsText(lower, ".ts.net") || ContainsText(lower, "tailscale");
}

bool IsLocalOrTrustedEndpoint(const std::string& base_url,
                              bool allow_insecure) {
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
  return ContainsText(lower, "localhost") || ContainsText(lower, "127.0.0.1") ||
         ContainsText(lower, "0.0.0.0") || ContainsText(lower, "::1") ||
         ContainsText(lower, "192.168.") || StartsWithText(lower, "10.") ||
         ContainsText(lower, "100.64.");
}

#ifndef __EMSCRIPTEN__
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
#else
bool ProbeOllamaHost(const std::string&) {
  return false;
}
bool ProbeOpenAICompatible(const std::string&) {
  return false;
}
#endif

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

}  // namespace

AgentEditor::AgentEditor() {
  type_ = EditorType::kAgent;
  agent_chat_ = std::make_unique<AgentChat>();
  local_coordinator_ = std::make_unique<AgentCollaborationCoordinator>();
  config_panel_ = std::make_unique<AgentConfigPanel>();
  feature_flag_panel_ = std::make_unique<FeatureFlagEditorPanel>();
  manifest_panel_ = std::make_unique<ManifestPanel>();
  mesen_debug_panel_ = std::make_unique<MesenDebugPanel>();
  mesen_screenshot_panel_ = std::make_unique<MesenScreenshotPanel>();
  oracle_state_panel_ = std::make_unique<OracleStateLibraryPanel>();
  sram_viewer_panel_ = std::make_unique<SramViewerPanel>();
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

  // Register WindowContent instances with WorkspaceWindowManager
  if (dependencies_.window_manager) {
    auto* window_manager = dependencies_.window_manager;

    // Register all agent EditorPanels with callbacks
    window_manager->RegisterWindowContent(
        std::make_unique<AgentConfigurationPanel>(
            [this]() { DrawConfigurationPanel(); }));
    window_manager->RegisterWindowContent(
        std::make_unique<AgentStatusPanel>([this]() { DrawStatusPanel(); }));
    window_manager->RegisterWindowContent(
        std::make_unique<AgentPromptEditorPanel>(
            [this]() { DrawPromptEditorPanel(); }));
    window_manager->RegisterWindowContent(
        std::make_unique<AgentBotProfilesPanel>(
            [this]() { DrawBotProfilesPanel(); }));
    window_manager->RegisterWindowContent(std::make_unique<AgentBuilderPanel>(
        [this]() { DrawAgentBuilderPanel(); }));
    window_manager->RegisterWindowContent(
        std::make_unique<AgentChatPanel>(agent_chat_.get()));

    // Knowledge Base panel (callback set by AgentUiController)
    window_manager->RegisterWindowContent(
        std::make_unique<AgentKnowledgeBasePanel>([this]() {
          if (knowledge_panel_callback_) {
            knowledge_panel_callback_();
          } else {
            ImGui::TextDisabled("Knowledge service not available");
            ImGui::TextWrapped(
                "Build with Z3ED_AI=ON to enable the knowledge service.");
          }
        }));

    window_manager->RegisterWindowContent(
        std::make_unique<AgentMesenDebugPanel>(
            [this]() { DrawMesenDebugPanel(); }));

    window_manager->RegisterWindowContent(
        std::make_unique<MesenScreenshotEditorPanel>(
            [this]() { DrawMesenScreenshotPanel(); }));

    window_manager->RegisterWindowContent(
        std::make_unique<OracleStateLibraryEditorPanel>(
            [this]() { DrawOracleStatePanel(); }));

    window_manager->RegisterWindowContent(
        std::make_unique<FeatureFlagEditorEditorPanel>(
            [this]() { DrawFeatureFlagPanel(); }));

    window_manager->RegisterWindowContent(std::make_unique<ManifestEditorPanel>(
        [this]() { DrawManifestPanel(); }));

    window_manager->RegisterWindowContent(
        std::make_unique<SramViewerEditorPanel>(
            [this]() { DrawSramViewerPanel(); }));

    if (agent_chat_) {
      agent_chat_->SetPanelOpener(
          [window_manager](const std::string& panel_id) {
            if (!panel_id.empty()) {
              window_manager->OpenWindow(panel_id);
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
    const std::string& active_id = prefs.active_ai_host_id.empty()
                                       ? prefs.ai_hosts.front().id
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

void AgentEditor::MarkProfileUiDirty() {
  profile_ui_state_.dirty = true;
}

void AgentEditor::SyncProfileUiState() {
  if (!profile_ui_state_.dirty) {
    return;
  }
  auto& ui = profile_ui_state_;
  CopyStringToBuffer(current_profile_.model, ui.model_buf);
  CopyStringToBuffer(current_profile_.ollama_host.empty()
                         ? "http://localhost:11434"
                         : current_profile_.ollama_host,
                     ui.ollama_host_buf);
  CopyStringToBuffer(current_profile_.gemini_api_key, ui.gemini_key_buf);
  CopyStringToBuffer(current_profile_.anthropic_api_key, ui.anthropic_key_buf);
  CopyStringToBuffer(current_profile_.openai_api_key, ui.openai_key_buf);
  CopyStringToBuffer(current_profile_.openai_base_url.empty()
                         ? "https://api.openai.com"
                         : current_profile_.openai_base_url,
                     ui.openai_base_buf);
  CopyStringToBuffer(current_profile_.name, ui.name_buf);
  CopyStringToBuffer(current_profile_.description, ui.desc_buf);
  CopyStringToBuffer(BuildTagsString(current_profile_.tags), ui.tags_buf);
  ui.dirty = false;
}

void AgentEditor::RegisterPanels() {
  // Panel descriptors are now auto-created by RegisterWindowContent() calls
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

  const bool has_openai_endpoint_hint =
      cli::NormalizeOpenAiBaseUrl(current_profile_.openai_base_url) !=
      kDefaultOpenAiBaseUrl;
  bool provider_is_default =
      current_profile_.provider == "mock" && current_profile_.host_id.empty();

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
    } else if (has_openai_endpoint_hint) {
      current_profile_.provider = "openai";
      current_config_.provider = "openai";
      if (current_profile_.model.empty() && !env_openai_model.empty()) {
        current_profile_.model = env_openai_model;
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
    } else if (!current_profile_.openai_api_key.empty()) {
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
  ctx_config.ai_provider =
      config.ai_provider.empty() ? "mock" : config.ai_provider;
  ctx_config.ai_model = config.ai_model;
  ctx_config.ollama_host = config.ollama_host.empty() ? "http://localhost:11434"
                                                      : config.ollama_host;
  ctx_config.gemini_api_key = config.gemini_api_key;
  ctx_config.anthropic_api_key = config.anthropic_api_key;
  ctx_config.openai_api_key = config.openai_api_key;
  ctx_config.openai_base_url =
      cli::NormalizeOpenAiBaseUrl(config.openai_base_url);
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
                 (absl::Now() - last_local_model_scan_) > absl::Seconds(30)) {
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
    std::string api_type = host.api_type.empty() ? "openai" : host.api_type;
    if (api_type == "lmstudio") {
      api_type = "openai";
    }
    auto resolved = build_host(host);
    bool has_key = !resolved.api_key.empty();

    if (api_type == "ollama") {
      if (resolved.base_url.empty()) {
        return false;
      }
      if (probe_only_local && !IsLocalOrTrustedEndpoint(
                                  resolved.base_url, resolved.allow_insecure)) {
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

}  // namespace editor
}  // namespace yaze
