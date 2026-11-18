#include "app/editor/agent/panels/agent_configuration_panel.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/time/clock.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "cli/service/ai/ai_config_utils.h"
#include "cli/service/ai/ai_http_client.h"
#include "cli/service/ai/cli_harness_service.h"
#include "cli/service/ai/provider_ids.h"
#include "cli/service/ai/service_factory.h"
#include "cli/service/ai/subprocess_runner.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace yaze {
namespace editor {

namespace {

std::string FormatByteSize(uint64_t bytes) {
  if (bytes < 1024)
    return absl::StrFormat("%d B", bytes);
  if (bytes < 1024 * 1024)
    return absl::StrFormat("%.1f KB", bytes / 1024.0);
  if (bytes < 1024 * 1024 * 1024)
    return absl::StrFormat("%.1f MB", bytes / (1024.0 * 1024.0));
  return absl::StrFormat("%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
}

std::string FormatRelativeTime(absl::Time timestamp) {
  if (timestamp == absl::InfinitePast())
    return "never";
  auto delta = absl::Now() - timestamp;
  if (delta < absl::Seconds(60))
    return "just now";
  if (delta < absl::Minutes(60))
    return absl::StrFormat("%.0fm ago", absl::ToDoubleMinutes(delta));
  if (delta < absl::Hours(24))
    return absl::StrFormat("%.0fh ago", absl::ToDoubleHours(delta));
  return absl::StrFormat("%.0fd ago", absl::ToDoubleHours(delta) / 24.0);
}

bool ContainsText(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

bool StartsWithText(const std::string& text, const std::string& prefix) {
  return text.rfind(prefix, 0) == 0;
}

bool IsLocalEndpoint(const std::string& base_url) {
  if (base_url.empty()) {
    return false;
  }
  std::string lower = absl::AsciiStrToLower(base_url);
  // Check for common local identifiers
  return ContainsText(lower, "localhost") || ContainsText(lower, "127.0.0.1") ||
         ContainsText(lower, "0.0.0.0") || ContainsText(lower, "::1") ||
         // LAN IPs (rudimentary check)
         ContainsText(lower, "192.168.") || StartsWithText(lower, "10.") ||
         // LM Studio default port check just in case domain differs
         ContainsText(lower, ":1234");
}

bool IsHalextEndpoint(const std::string& base_url) {
  if (base_url.empty()) {
    return false;
  }
  const std::string lower = absl::AsciiStrToLower(base_url);
  return ContainsText(lower, "halext.org");
}

struct CachedRowStatus {
  bool checked = false;
  bool checking = false;
  bool ready = false;
  std::string status_label;
  std::string tooltip;
  std::string override_used;
  std::string endpoint_used;
};

struct QueuedToastMessage {
  std::string text;
  ToastType type = ToastType::kInfo;
  float duration_seconds = 4.0f;
};

struct ProviderEntry {
  const char* id;
  const char* label;
  const char* group;
};

std::string SanitizeProbeSnippet(absl::string_view text) {
  std::string clean;
  clean.reserve(80);
  bool last_space = false;
  for (unsigned char c : text) {
    if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
      if (!clean.empty() && !last_space) {
        clean.push_back(' ');
        last_space = true;
      }
      continue;
    }
    if (c < 0x20) {
      continue;
    }
    clean.push_back(static_cast<char>(c));
    last_space = false;
    if (clean.size() >= 72) {
      clean += "...";
      break;
    }
  }
  return clean;
}

std::string ResolveWorkingDirForConfig(const AgentUIContext* context,
                                       const AgentConfigState& config) {
  if (!config.working_directory.empty()) {
    return config.working_directory;
  }
  if (context && context->GetProject() &&
      !context->GetProject()->filepath.empty()) {
    std::error_code ec;
    const std::filesystem::path fp(context->GetProject()->filepath);
    if (std::filesystem::is_directory(fp, ec)) {
      return fp.string();
    }
    const std::filesystem::path parent = fp.parent_path();
    if (!parent.empty() && std::filesystem::is_directory(parent, ec)) {
      return parent.string();
    }
  }
  return {};
}

std::string ResolveEnvFallback(const char* env_var) {
  if (!env_var || !*env_var) {
    return {};
  }
  if (const char* val = std::getenv(env_var); val != nullptr && *val != '\0') {
    return val;
  }
  return {};
}

}  // namespace

struct AgentConfigPanel::ProbeSharedState {
  std::mutex mutex;
  std::atomic<bool> shutting_down{false};
  std::map<std::string, CachedRowStatus> statuses;
  std::map<std::string, bool> probes_in_flight;
  std::deque<QueuedToastMessage> pending_toasts;
  bool initial_local_probe_started = false;
};

AgentConfigPanel::AgentConfigPanel()
    : probe_state_(std::make_shared<ProbeSharedState>()) {}

AgentConfigPanel::~AgentConfigPanel() {
  if (probe_state_) {
    probe_state_->shutting_down.store(true, std::memory_order_relaxed);
  }
}

void AgentConfigPanel::Draw(AgentUIContext* context, const Callbacks& callbacks,
                            ToastManager* toast_manager) {
  const auto& theme = AgentUI::GetTheme();

  gui::StyledChild config_child("AgentConfig", ImVec2(0, 0),
                                {.bg = theme.panel_bg_color}, true);
  AgentUI::RenderSectionHeader(ICON_MD_SETTINGS, "Agent Builder",
                               theme.command_text_color);

  if (ImGui::CollapsingHeader(ICON_MD_SMART_TOY " Connection & Models",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderModelConfigControls(context, callbacks, toast_manager);
    ImGui::Separator();
    RenderModelDeck(context, callbacks, toast_manager);
  }

  if (ImGui::CollapsingHeader(ICON_MD_TUNE " Parameters",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderParameterControls(context->agent_config());
  }

  if (ImGui::CollapsingHeader(ICON_MD_CONSTRUCTION " Tools & Editor Hooks",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderToolingControls(context->agent_config(), callbacks);
  }

  ImGui::Spacing();
  // Note: persist_agent_config_with_history_ logic was local to AgentChatWidget.
  // We might want to move it to AgentConfigState if it needs to be persisted.
  // For now, we'll skip it or add it to AgentConfigState if needed.
  // Assuming it's not critical for this refactor, or we can add it later.

  if (ImGui::Button(ICON_MD_CLOUD_SYNC " Apply Provider Settings",
                    ImVec2(-1, 0))) {
    if (callbacks.update_config) {
      callbacks.update_config(context->agent_config());
    }
  }
}

void AgentConfigPanel::RenderModelConfigControls(AgentUIContext* context,
                                                 const Callbacks& callbacks,
                                                 ToastManager* toast_manager) {
  const auto& theme = AgentUI::GetTheme();
  ImGuiStyle& style = ImGui::GetStyle();
  auto& config = context->agent_config();
  auto& model_cache = context->model_cache();
  static bool filter_by_provider = false;

  // Drain any completed async Test/probe toasts onto the UI thread.
  if (probe_state_) {
    std::deque<QueuedToastMessage> toasts;
    {
      std::lock_guard<std::mutex> lock(probe_state_->mutex);
      toasts.swap(probe_state_->pending_toasts);
    }
    if (toast_manager) {
      for (const auto& msg : toasts) {
        toast_manager->Show(msg.text, msg.type, msg.duration_seconds);
      }
    }
  }

  const std::string active_override =
      config.cli_binary_paths.count(config.ai_provider)
          ? config.cli_binary_paths.at(config.ai_provider)
          : "";

  if (model_cache.last_provider != config.ai_provider ||
      model_cache.last_openai_base != config.openai_base_url ||
      model_cache.last_ollama_host != config.ollama_host ||
      model_cache.last_cli_binary_override != active_override) {
    model_cache.auto_refresh_requested = false;
    model_cache.last_refresh = absl::InfinitePast();
    model_cache.last_provider = config.ai_provider;
    model_cache.last_openai_base = config.openai_base_url;
    model_cache.last_ollama_host = config.ollama_host;
    model_cache.last_cli_binary_override = active_override;
  }

  if (callbacks.refresh_models && !model_cache.loading &&
      !model_cache.auto_refresh_requested) {
    model_cache.auto_refresh_requested = true;
    callbacks.refresh_models(false);
  }

  const float label_width = 124.0f;
  const ImVec2 compact_padding(style.FramePadding.x,
                               std::max(2.0f, style.FramePadding.y * 0.6f));
  const ImVec2 compact_spacing(style.ItemSpacing.x,
                               std::max(3.0f, style.ItemSpacing.y * 0.6f));
  const float env_button_width =
      ImGui::CalcTextSize(ICON_MD_SYNC " Env").x + compact_padding.x * 2.0f;

  auto copy_to_buf = [](const std::string& src, char* dst, size_t dst_size) {
    if (dst_size == 0) {
      return;
    }
    std::snprintf(dst, dst_size, "%s", src.c_str());
  };

  auto set_openai_base = [&](const std::string& base_url) {
    copy_to_buf(base_url, config.openai_base_url_buffer,
                sizeof(config.openai_base_url_buffer));
    config.openai_base_url = config.openai_base_url_buffer;
  };

  auto select_provider = [&](const std::string& provider_id) {
    config.ai_provider = provider_id;
    copy_to_buf(provider_id, config.provider_buffer,
                sizeof(config.provider_buffer));
    if (provider_id == cli::kProviderLmStudio) {
      if (config.openai_base_url.empty() ||
          cli::NormalizeOpenAiBaseUrl(config.openai_base_url) ==
              "https://api.openai.com") {
        set_openai_base("http://localhost:1234");
      }
    } else if (provider_id == cli::kProviderOpenAi) {
      if (IsLocalEndpoint(config.openai_base_url)) {
        set_openai_base("https://api.openai.com");
      }
    }
    std::string override_path;
    if (auto it = config.cli_binary_paths.find(provider_id);
        it != config.cli_binary_paths.end()) {
      override_path = it->second;
    }
    copy_to_buf(override_path, config.cli_binary_override_buffer,
                sizeof(config.cli_binary_override_buffer));
  };

  auto trigger_local_probe = [&](const std::string& provider_id,
                                 const std::string& base_url, const char* path,
                                 bool force) {
    if (!probe_state_) {
      return;
    }
    const std::string normalized_base = cli::NormalizeOpenAiBaseUrl(base_url);
    {
      std::lock_guard<std::mutex> lock(probe_state_->mutex);
      auto& entry = probe_state_->statuses[provider_id];
      if (!force &&
          (entry.checking ||
           (entry.checked && entry.endpoint_used == normalized_base))) {
        return;
      }
      entry.checked = true;
      entry.checking = true;
      entry.ready = false;
      entry.endpoint_used = normalized_base;
      entry.status_label = "Checking...";
      entry.tooltip = normalized_base;
    }
#ifndef YAZE_AI_RUNTIME_AVAILABLE
    // ai_http_client.cc is only built with YAZE_ENABLE_AI_RUNTIME.
    (void)path;
    {
      std::lock_guard<std::mutex> lock(probe_state_->mutex);
      auto& entry = probe_state_->statuses[provider_id];
      entry.checking = false;
      entry.status_label = "AI runtime off";
      entry.tooltip = "This build has no AI runtime (YAZE_ENABLE_AI_RUNTIME)";
    }
#else
    std::thread([state = probe_state_, provider_id, normalized_base,
                 req_path = std::string(path)]() {
      auto resp_or = cli::PerformAiHttpRequest("GET", normalized_base, req_path,
                                               {}, "", 500);
      const bool reachable = resp_or.ok() && resp_or->status_code > 0 &&
                             resp_or->status_code < 500;
      std::lock_guard<std::mutex> lock(state->mutex);
      auto& entry = state->statuses[provider_id];
      if (entry.endpoint_used != normalized_base) {
        return;
      }
      entry.checked = true;
      entry.checking = false;
      entry.ready = reachable;
      entry.status_label = reachable ? "Reachable" : "Offline";
      entry.tooltip =
          reachable
              ? absl::StrCat(normalized_base, " reachable")
              : (resp_or.ok() ? absl::StrFormat("%s HTTP %d", normalized_base,
                                                resp_or->status_code)
                              : absl::StrCat(normalized_base, ": ",
                                             resp_or.status().message()));
    }).detach();
#endif
  };

  auto compute_row_status =
      [&](const std::string& provider_id) -> CachedRowStatus {
    if (provider_id == cli::kProviderMock) {
      return {.checked = true,
              .checking = false,
              .ready = true,
              .status_label = "Ready",
              .tooltip = tr("Offline mock provider for local UI testing.")};
    }

    auto api_key_status = [&](const std::string& configured,
                              const char* env_var,
                              const char* label) -> CachedRowStatus {
      const bool has_configured = !configured.empty();
      const bool has_env = !ResolveEnvFallback(env_var).empty();
      if (has_configured || has_env) {
        return {.checked = true,
                .checking = false,
                .ready = true,
                .status_label = "Key set",
                .tooltip = has_configured
                               ? absl::StrCat(label, " API key configured.")
                               : absl::StrCat("Using ", env_var,
                                              " from environment.")};
      }
      return {.checked = true,
              .checking = false,
              .ready = false,
              .status_label = "No API key",
              .tooltip = absl::StrCat("Set ", label, " API key or ", env_var,
                                      " environment variable.")};
    };

    if (provider_id == cli::kProviderAnthropic) {
      return api_key_status(config.anthropic_api_key, "ANTHROPIC_API_KEY",
                            "Anthropic");
    }
    if (provider_id == cli::kProviderOpenAi) {
      if (!IsLocalEndpoint(config.openai_base_url) ||
          cli::NormalizeOpenAiBaseUrl(config.openai_base_url) ==
              "https://api.openai.com") {
        return api_key_status(config.openai_api_key, "OPENAI_API_KEY",
                              "OpenAI");
      }
      return {.checked = true,
              .checking = false,
              .ready = true,
              .status_label = "Custom base",
              .tooltip = config.openai_base_url};
    }
    if (provider_id == cli::kProviderGemini) {
      return api_key_status(config.gemini_api_key, "GEMINI_API_KEY", "Gemini");
    }

    if (cli::IsCliHarnessProvider(provider_id)) {
      const std::string override_path =
          config.cli_binary_paths.count(provider_id)
              ? config.cli_binary_paths.at(provider_id)
              : "";
      if (probe_state_) {
        std::lock_guard<std::mutex> lock(probe_state_->mutex);
        auto it = probe_state_->statuses.find(provider_id);
        if (it != probe_state_->statuses.end() && it->second.checked &&
            it->second.override_used == override_path) {
          return it->second;
        }
      }
      CachedRowStatus status;
      status.checked = true;
      status.override_used = override_path;
      auto profile_or = cli::GetCliHarnessProfile(provider_id);
      if (!profile_or.ok()) {
        status.ready = false;
        status.status_label = "Invalid";
        status.tooltip = std::string(profile_or.status().message());
      } else {
        auto resolved_or =
            cli::ResolveCliBinary(profile_or->binary_name, override_path);
        if (resolved_or.ok()) {
          status.ready = true;
          status.status_label = "Found";
          status.tooltip = absl::StrCat(*resolved_or, "\n", tr("Mode: "),
                                        profile_or->read_only_summary);
        } else {
          status.ready = false;
          status.status_label = "Not found";
          status.tooltip = absl::StrCat(
              resolved_or.status().message(), "\n",
              tr("Set a custom path under CLI Binary Overrides below."));
        }
      }
      if (probe_state_) {
        std::lock_guard<std::mutex> lock(probe_state_->mutex);
        probe_state_->statuses[provider_id] = status;
      }
      return status;
    }

    if (provider_id == cli::kProviderOllama) {
      trigger_local_probe(cli::kProviderOllama, config.ollama_host,
                          "/api/version", false);
    } else if (provider_id == cli::kProviderLmStudio) {
      const std::string lms_url = IsLocalEndpoint(config.openai_base_url)
                                      ? config.openai_base_url
                                      : "http://localhost:1234";
      trigger_local_probe(cli::kProviderLmStudio, lms_url, "/v1/models", false);
    }

    if (probe_state_) {
      std::lock_guard<std::mutex> lock(probe_state_->mutex);
      if (auto it = probe_state_->statuses.find(provider_id);
          it != probe_state_->statuses.end()) {
        return it->second;
      }
    }
    return {.checked = false,
            .checking = false,
            .ready = false,
            .status_label = "Offline",
            .tooltip = tr("Server not probed yet.")};
  };

  auto launch_test_probe = [&](const ProviderEntry& entry) {
    if (!probe_state_) {
      return;
    }
    {
      std::lock_guard<std::mutex> lock(probe_state_->mutex);
      if (probe_state_->probes_in_flight[entry.id]) {
        return;
      }
      probe_state_->probes_in_flight[entry.id] = true;
    }

    cli::AIServiceConfig probe_cfg;
    probe_cfg.provider = entry.id;
    probe_cfg.ollama_host = config.ollama_host;
    probe_cfg.gemini_api_key = config.gemini_api_key.empty()
                                   ? ResolveEnvFallback("GEMINI_API_KEY")
                                   : config.gemini_api_key;
    probe_cfg.anthropic_api_key = config.anthropic_api_key.empty()
                                      ? ResolveEnvFallback("ANTHROPIC_API_KEY")
                                      : config.anthropic_api_key;
    probe_cfg.openai_api_key = config.openai_api_key.empty()
                                   ? ResolveEnvFallback("OPENAI_API_KEY")
                                   : config.openai_api_key;
    probe_cfg.openai_base_url =
        (std::string(entry.id) == cli::kProviderLmStudio)
            ? (IsLocalEndpoint(config.openai_base_url)
                   ? config.openai_base_url
                   : "http://localhost:1234")
            : (std::string(entry.id) == cli::kProviderOpenAi &&
                       IsLocalEndpoint(config.openai_base_url)
                   ? "https://api.openai.com"
                   : config.openai_base_url);
    probe_cfg.cli_timeout_seconds =
        std::clamp(config.cli_timeout_seconds, 5, 3600);
    probe_cfg.cli_binary_paths = config.cli_binary_paths;
    probe_cfg.working_directory = ResolveWorkingDirForConfig(context, config);

    if (config.ai_provider == entry.id && !config.ai_model.empty()) {
      probe_cfg.model = config.ai_model;
    } else if (std::string(entry.id) == cli::kProviderLmStudio ||
               std::string(entry.id) == cli::kProviderOllama) {
      for (const auto& info : model_cache.available_models) {
        if (absl::StrContains(absl::AsciiStrToLower(info.name), "embedding")) {
          continue;
        }
        if (info.provider == entry.id ||
            (std::string(entry.id) == cli::kProviderLmStudio &&
             info.provider == cli::kProviderOpenAi && info.is_local)) {
          probe_cfg.model = info.name;
          break;
        }
      }
    }

    if (toast_manager) {
      toast_manager->Show(
          absl::StrCat(tr("Testing"), " ", tr(entry.label), "..."),
          ToastType::kInfo, 2.0f);
    }

    std::thread([state = probe_state_, label = std::string(entry.label),
                 provider_id = std::string(entry.id), probe_cfg]() {
      const auto start = std::chrono::steady_clock::now();
      std::string outcome_msg;
      ToastType outcome_type = ToastType::kInfo;

      auto service_or = cli::CreateAIServiceStrict(probe_cfg);
      if (!service_or.ok()) {
        const double secs = std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - start)
                                .count();
        outcome_type = ToastType::kError;
        outcome_msg = absl::StrFormat(
            "%s probe failed (%.2fs): %s", label, secs,
            SanitizeProbeSnippet(service_or.status().message()));
      } else {
        auto resp_or = (*service_or)->GenerateResponse("Reply PROBE OK");
        const double secs = std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - start)
                                .count();
        if (!resp_or.ok()) {
          outcome_type = ToastType::kError;
          outcome_msg =
              absl::StrFormat("%s probe failed (%.2fs): %s", label, secs,
                              SanitizeProbeSnippet(resp_or.status().message()));
        } else {
          std::string reply = SanitizeProbeSnippet(resp_or->text_response);
          if (reply.empty()) {
            reply = "OK";
          }
          outcome_type = ToastType::kSuccess;
          outcome_msg = absl::StrFormat("%s: %s (%.2fs)", label, reply, secs);
        }
      }

      if (!state->shutting_down.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->probes_in_flight[provider_id] = false;
        state->pending_toasts.push_back(
            {std::move(outcome_msg), outcome_type, 5.0f});
      }
    }).detach();
  };

  {
    gui::StyleVarGuard provider_var_guard(
        {{ImGuiStyleVar_FramePadding, compact_padding},
         {ImGuiStyleVar_ItemSpacing, compact_spacing}});

    const ProviderEntry provider_entries[] = {
        {cli::kProviderAnthropic, "Anthropic", "API"},
        {cli::kProviderOpenAi, "OpenAI", "API"},
        {cli::kProviderGemini, "Gemini", "API"},
        {cli::kProviderClaudeCode, "Claude Code", "Harness (CLI)"},
        {cli::kProviderCodex, "Codex", "Harness (CLI)"},
        {cli::kProviderCursorAgent, "Cursor Agent", "Harness (CLI)"},
        {cli::kProviderAgy, "Antigravity (agy)", "Harness (CLI)"},
        {cli::kProviderGeminiCli, "Gemini CLI", "Harness (CLI)"},
        {cli::kProviderOllama, "Ollama", "Local"},
        {cli::kProviderLmStudio, "LM Studio", "Local"},
        {cli::kProviderMock, "Mock", "Local"},
    };

    if (ImGui::BeginTable("AgentGroupedProviderTable", 3,
                          ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_BordersInnerH |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn(tr("Provider"),
                              ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn(tr("Status"), ImGuiTableColumnFlags_WidthFixed,
                              92.0f);
      ImGui::TableSetupColumn(tr("Test"), ImGuiTableColumnFlags_WidthFixed,
                              64.0f);
      ImGui::TableHeadersRow();

      std::string current_group;
      for (const auto& entry : provider_entries) {
        if (current_group != entry.group) {
          current_group = entry.group;
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextColored(theme.text_info, "%s", tr(entry.group));
          ImGui::TableSetColumnIndex(1);
          ImGui::TableSetColumnIndex(2);
        }

        const CachedRowStatus status = compute_row_status(entry.id);
        const bool is_cli_harness = cli::IsCliHarnessProvider(entry.id);
        const bool disable_select = is_cli_harness && !status.ready;
        const bool active = (config.ai_provider == entry.id);
        bool probe_running = false;
        if (probe_state_) {
          std::lock_guard<std::mutex> lock(probe_state_->mutex);
          probe_running = probe_state_->probes_in_flight[entry.id];
        }

        ImGui::PushID(entry.id);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (disable_select) {
          ImGui::BeginDisabled(true);
        }
        std::optional<gui::StyleColorGuard> active_color;
        if (active) {
          active_color.emplace(
              std::initializer_list<gui::StyleColorGuard::Entry>{
                  {ImGuiCol_Header, theme.accent_color},
                  {ImGuiCol_HeaderHovered, theme.accent_color}});
        }
        if (ImGui::Selectable(tr(entry.label), active)) {
          select_provider(entry.id);
        }
        active_color.reset();
        if (disable_select) {
          ImGui::EndDisabled();
          if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) &&
              !status.tooltip.empty()) {
            ImGui::SetTooltip("%s", status.tooltip.c_str());
          }
        } else if (ImGui::IsItemHovered() && !status.tooltip.empty()) {
          ImGui::SetTooltip("%s", status.tooltip.c_str());
        }

        ImGui::TableSetColumnIndex(1);
        const ImVec4 status_color = status.checking ? theme.text_secondary_color
                                    : status.ready  ? theme.status_success
                                                    : theme.status_warning;
        ImGui::TextColored(status_color, "%s", tr(status.status_label));
        if (ImGui::IsItemHovered() && !status.tooltip.empty()) {
          ImGui::SetTooltip("%s", status.tooltip.c_str());
        }

        ImGui::TableSetColumnIndex(2);
        const bool disable_test = !status.ready || probe_running;
        if (disable_test) {
          ImGui::BeginDisabled(true);
        }
        const std::string test_label =
            absl::StrCat(tr("Test"), "##probe_", entry.id);
        if (ImGui::SmallButton(test_label.c_str())) {
          launch_test_probe(entry);
        }
        if (disable_test) {
          ImGui::EndDisabled();
          if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            const std::string reason =
                probe_running ? std::string(tr("Probe already running..."))
                              : status.tooltip;
            if (!reason.empty()) {
              ImGui::SetTooltip("%s", reason.c_str());
            }
          }
        } else if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", tr("Send \"Reply PROBE OK\" test prompt"));
        }

        ImGui::PopID();
      }
      ImGui::EndTable();
    }

    ImGui::Spacing();

    if (cli::IsCliHarnessProvider(config.ai_provider)) {
      if (auto profile_or = cli::GetCliHarnessProfile(config.ai_provider);
          profile_or.ok()) {
        if (ImGui::BeginTable("AgentHarnessDetailTable", 2,
                              ImGuiTableFlags_SizingFixedFit |
                                  ImGuiTableFlags_BordersInnerV)) {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                  label_width);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextDisabled("%s", tr("Binary Override"));
          ImGui::TableSetColumnIndex(1);
          ImGui::SetNextItemWidth(-1);
          const std::string current_override =
              config.cli_binary_paths.count(config.ai_provider)
                  ? config.cli_binary_paths.at(config.ai_provider)
                  : "";
          if (std::string(config.cli_binary_override_buffer) !=
              current_override) {
            copy_to_buf(current_override, config.cli_binary_override_buffer,
                        sizeof(config.cli_binary_override_buffer));
          }
          if (ImGui::InputTextWithHint(
                  "##cli_binary_override", profile_or->binary_name.c_str(),
                  config.cli_binary_override_buffer,
                  IM_ARRAYSIZE(config.cli_binary_override_buffer))) {
            std::string edited = std::string(
                absl::StripAsciiWhitespace(config.cli_binary_override_buffer));
            if (edited.empty()) {
              config.cli_binary_paths.erase(config.ai_provider);
            } else {
              config.cli_binary_paths[config.ai_provider] = edited;
            }
            if (probe_state_) {
              std::lock_guard<std::mutex> lock(probe_state_->mutex);
              probe_state_->statuses.erase(config.ai_provider);
            }
          }

          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextDisabled("%s", tr("CLI Timeout (s)"));
          ImGui::TableSetColumnIndex(1);
          ImGui::SetNextItemWidth(140.0f);
          if (ImGui::InputInt("##cli_timeout_seconds",
                              &config.cli_timeout_seconds, 15, 60)) {
            config.cli_timeout_seconds =
                std::clamp(config.cli_timeout_seconds, 5, 3600);
          }

          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextDisabled("%s", tr("Sandbox Mode"));
          ImGui::TableSetColumnIndex(1);
          ImGui::TextColored(theme.status_success, "%s (%s)", tr("Read-only"),
                             profile_or->read_only_summary.c_str());

          const std::string work_dir =
              ResolveWorkingDirForConfig(context, config);
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextDisabled("%s", tr("Working Dir"));
          ImGui::TableSetColumnIndex(1);
          ImGui::TextDisabled(
              "%s", work_dir.empty() ? tr("(none)") : work_dir.c_str());

          ImGui::EndTable();
        }
      }
    }

    if (ImGui::TreeNode(tr("CLI Binary Overrides"))) {
      static char override_bufs[5][256] = {};
      const char* harness_ids[5] = {cli::kProviderClaudeCode,
                                    cli::kProviderCodex,
                                    cli::kProviderCursorAgent,
                                    cli::kProviderAgy, cli::kProviderGeminiCli};
      const char* harness_names[5] = {"Claude Code", "Codex", "Cursor Agent",
                                      "Antigravity (agy)", "Gemini CLI"};
      if (ImGui::BeginTable(
              "AgentCliOverridesTable", 2,
              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                label_width);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        for (int i = 0; i < 5; ++i) {
          const std::string hid = harness_ids[i];
          std::string cur = config.cli_binary_paths.count(hid)
                                ? config.cli_binary_paths.at(hid)
                                : "";
          if (std::string(override_bufs[i]) != cur) {
            copy_to_buf(cur, override_bufs[i], sizeof(override_bufs[i]));
          }
          ImGui::PushID(hid.c_str());
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextDisabled("%s", tr(harness_names[i]));
          ImGui::TableSetColumnIndex(1);
          ImGui::SetNextItemWidth(-1);
          auto prof_or = cli::GetCliHarnessProfile(hid);
          const char* hint =
              prof_or.ok() ? prof_or->binary_name.c_str() : "path...";
          if (ImGui::InputTextWithHint("##path", hint, override_bufs[i],
                                       IM_ARRAYSIZE(override_bufs[i]))) {
            std::string edited =
                std::string(absl::StripAsciiWhitespace(override_bufs[i]));
            if (edited.empty()) {
              config.cli_binary_paths.erase(hid);
            } else {
              config.cli_binary_paths[hid] = edited;
            }
            if (probe_state_) {
              std::lock_guard<std::mutex> lock(probe_state_->mutex);
              probe_state_->statuses.erase(hid);
            }
          }
          ImGui::PopID();
        }
        ImGui::EndTable();
      }
      ImGui::TreePop();
    }

    const bool open_keys_by_default =
        !cli::IsCliHarnessProvider(config.ai_provider);
    if (ImGui::CollapsingHeader(
            tr("API Keys & Endpoints"),
            open_keys_by_default ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
      if (ImGui::BeginTable(
              "AgentProviderConfigTable", 2,
              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                label_width);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Ollama Host");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##ollama_host", "http://localhost:11434",
                                     config.ollama_host_buffer,
                                     IM_ARRAYSIZE(config.ollama_host_buffer))) {
          config.ollama_host = config.ollama_host_buffer;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
          trigger_local_probe(cli::kProviderOllama, config.ollama_host,
                              "/api/version", true);
        }

        auto key_row = [&](const char* label, const char* hint, char* buffer,
                           size_t buffer_len, std::string* target,
                           const char* env_var, const char* input_id,
                           const char* button_id) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextDisabled("%s", label);
          ImGui::TableSetColumnIndex(1);
          float input_width = ImGui::GetContentRegionAvail().x -
                              env_button_width - style.ItemSpacing.x;
          bool stack = input_width < 140.0f;
          ImGui::SetNextItemWidth(stack ? -1 : input_width);
          if (ImGui::InputTextWithHint(input_id, hint, buffer, buffer_len,
                                       ImGuiInputTextFlags_Password)) {
            if (target) {
              *target = buffer;
            }
          }
          if (!stack) {
            ImGui::SameLine();
          }
          if (ImGui::SmallButton(button_id)) {
            const char* env_key = std::getenv(env_var);
            if (env_key) {
              std::snprintf(buffer, buffer_len, "%s", env_key);
              if (target) {
                *target = env_key;
              }
              if (toast_manager) {
                toast_manager->Show(
                    absl::StrFormat("Loaded %s from environment", env_var),
                    ToastType::kInfo, 2.0f);
              }
            } else if (toast_manager) {
              toast_manager->Show(absl::StrFormat("%s not set", env_var),
                                  ToastType::kWarning, 2.0f);
            }
          }
        };

        key_row("Gemini Key", "API key...", config.gemini_key_buffer,
                IM_ARRAYSIZE(config.gemini_key_buffer), &config.gemini_api_key,
                "GEMINI_API_KEY", "##gemini_key", ICON_MD_SYNC " Env##gemini");
        key_row("Anthropic Key", "API key...", config.anthropic_key_buffer,
                IM_ARRAYSIZE(config.anthropic_key_buffer),
                &config.anthropic_api_key, "ANTHROPIC_API_KEY",
                "##anthropic_key", ICON_MD_SYNC " Env##anthropic");
        key_row("OpenAI Key", "API key...", config.openai_key_buffer,
                IM_ARRAYSIZE(config.openai_key_buffer), &config.openai_api_key,
                "OPENAI_API_KEY", "##openai_key", ICON_MD_SYNC " Env##openai");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("OpenAI Base");
        ImGui::TableSetColumnIndex(1);
        float openai_button_width =
            ImGui::CalcTextSize("OpenAI").x + compact_padding.x * 2.0f;
        float lm_button_width =
            ImGui::CalcTextSize("LM Studio").x + compact_padding.x * 2.0f;
        float afs_button_width =
            ImGui::CalcTextSize("AFS Bridge").x + compact_padding.x * 2.0f;
        float reset_button_width =
            ImGui::CalcTextSize("Reset").x + compact_padding.x * 2.0f;
        float total_buttons = openai_button_width + lm_button_width +
                              afs_button_width + reset_button_width +
                              style.ItemSpacing.x * 3.0f;
        float base_available = ImGui::GetContentRegionAvail().x;
        bool base_stack = base_available < total_buttons + 160.0f;
        ImGui::SetNextItemWidth(base_stack ? -1
                                           : base_available - total_buttons -
                                                 style.ItemSpacing.x);
        if (ImGui::InputTextWithHint(
                "##openai_base", "http://localhost:1234",
                config.openai_base_url_buffer,
                IM_ARRAYSIZE(config.openai_base_url_buffer))) {
          config.openai_base_url = config.openai_base_url_buffer;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() &&
            IsLocalEndpoint(config.openai_base_url)) {
          trigger_local_probe(cli::kProviderLmStudio, config.openai_base_url,
                              "/v1/models", true);
        }
        if (base_stack) {
          ImGui::Spacing();
        } else {
          ImGui::SameLine();
        }
        if (ImGui::SmallButton("OpenAI")) {
          set_openai_base("https://api.openai.com");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("LM Studio")) {
          set_openai_base("http://localhost:1234");
          trigger_local_probe(cli::kProviderLmStudio, "http://localhost:1234",
                              "/v1/models", true);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("AFS Bridge")) {
          set_openai_base("https://halext.org");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset")) {
          set_openai_base("https://api.openai.com");
        }

        ImGui::EndTable();
      }
    }
  }  // provider_var_guard scope

  if (IsHalextEndpoint(config.openai_base_url)) {
    ImGui::TextColored(theme.command_text_color,
                       ICON_MD_HUB " halext.org AFS bridge");
  } else if (IsLocalEndpoint(config.openai_base_url)) {
    ImGui::TextColored(theme.status_success,
                       ICON_MD_COMPUTER " Local OpenAI-compatible server");
  } else {
    ImGui::TextColored(theme.text_secondary_color,
                       ICON_MD_PUBLIC " Remote OpenAI endpoint");
  }

  ImGui::Spacing();

  {
    gui::StyleVarGuard model_var_guard(
        {{ImGuiStyleVar_FramePadding, compact_padding},
         {ImGuiStyleVar_ItemSpacing, compact_spacing}});
    if (ImGui::BeginTable(
            "AgentModelControls", 2,
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
      ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                              label_width);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("Model");
      ImGui::TableSetColumnIndex(1);
      const float suggestions_width = 128.0f;
      const float avail_model_w = ImGui::GetContentRegionAvail().x;
      const bool stack_suggestions = avail_model_w < suggestions_width + 140.0f;
      ImGui::SetNextItemWidth(stack_suggestions
                                  ? -1
                                  : avail_model_w - suggestions_width -
                                        style.ItemSpacing.x);
      if (ImGui::InputTextWithHint("##ai_model", "Model name...",
                                   config.model_buffer,
                                   IM_ARRAYSIZE(config.model_buffer))) {
        config.ai_model = config.model_buffer;
      }
      if (!stack_suggestions) {
        ImGui::SameLine();
      }
      ImGui::SetNextItemWidth(stack_suggestions ? -1 : suggestions_width);
      if (ImGui::BeginCombo("##model_suggestions", tr("Suggestions"))) {
        bool any_suggestion = false;
        for (const auto& info : model_cache.available_models) {
          const bool matches =
              info.provider.empty() || info.provider == config.ai_provider ||
              (config.ai_provider == cli::kProviderLmStudio &&
               info.provider == cli::kProviderOpenAi && info.is_local);
          if (!matches) {
            continue;
          }
          any_suggestion = true;
          const bool selected = config.ai_model == info.name;
          if (ImGui::Selectable(info.name.c_str(), selected)) {
            config.ai_model = info.name;
            std::snprintf(config.model_buffer, sizeof(config.model_buffer),
                          "%s", info.name.c_str());
          }
        }
        if (!any_suggestion) {
          ImGui::TextDisabled("%s", tr("No cached suggestions"));
        }
        ImGui::EndCombo();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("Filter");
      ImGui::TableSetColumnIndex(1);
      ImGui::Checkbox("Provider", &filter_by_provider);
      ImGui::SameLine();
      AgentUI::HorizontalSpacing(6.0f);
      ImGui::SameLine();

      float refresh_width =
          ImGui::CalcTextSize(ICON_MD_REFRESH).x + compact_padding.x * 2.0f;
      float clear_width =
          ImGui::CalcTextSize(ICON_MD_CLEAR).x + compact_padding.x * 2.0f;
      float search_width = ImGui::GetContentRegionAvail().x - refresh_width -
                           style.ItemSpacing.x;
      if (model_cache.search_buffer[0] != '\0') {
        search_width -= clear_width + style.ItemSpacing.x;
      }
      ImGui::SetNextItemWidth(search_width);
      ImGui::InputTextWithHint("##model_search", "Search models...",
                               model_cache.search_buffer,
                               IM_ARRAYSIZE(model_cache.search_buffer));
      ImGui::SameLine();
      if (model_cache.search_buffer[0] != '\0') {
        if (ImGui::SmallButton(ICON_MD_CLEAR)) {
          model_cache.search_buffer[0] = '\0';
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Clear search");
        }
        ImGui::SameLine();
      }
      if (ImGui::SmallButton(model_cache.loading ? ICON_MD_SYNC
                                                 : ICON_MD_REFRESH)) {
        if (probe_state_) {
          {
            std::lock_guard<std::mutex> lock(probe_state_->mutex);
            for (const char* hid :
                 {cli::kProviderClaudeCode, cli::kProviderCodex,
                  cli::kProviderCursorAgent, cli::kProviderAgy,
                  cli::kProviderGeminiCli}) {
              probe_state_->statuses.erase(hid);
            }
          }
          const std::string lms_url = IsLocalEndpoint(config.openai_base_url)
                                          ? config.openai_base_url
                                          : "http://localhost:1234";
          trigger_local_probe(cli::kProviderOllama, config.ollama_host,
                              "/api/version", true);
          trigger_local_probe(cli::kProviderLmStudio, lms_url, "/v1/models",
                              true);
        }
        if (callbacks.refresh_models) {
          callbacks.refresh_models(true);
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tr("Refresh provider models and statuses"));
      }

      ImGui::EndTable();
    }
  }  // model_var_guard scope
  if (!model_cache.available_models.empty() ||
      !model_cache.local_model_names.empty()) {
    const int provider_count =
        static_cast<int>(model_cache.available_models.size());
    const int local_count =
        static_cast<int>(model_cache.local_model_names.size());
    if (provider_count > 0 && local_count > 0) {
      ImGui::TextDisabled("Models: %d provider, %d local", provider_count,
                          local_count);
    } else if (provider_count > 0) {
      ImGui::TextDisabled("Models: %d provider", provider_count);
    } else if (local_count > 0) {
      ImGui::TextDisabled("Models: %d local files", local_count);
    }
  }

  float list_height = std::max(220.0f, ImGui::GetContentRegionAvail().y * 0.6f);
  gui::StyleColorGuard model_list_bg(ImGuiCol_ChildBg, theme.panel_bg_darker);
  ImGui::BeginChild("UnifiedModelList", ImVec2(0, list_height), true);
  std::string filter = absl::AsciiStrToLower(model_cache.search_buffer);

  struct ModelRow {
    std::string name;
    std::string provider;
    std::string param_size;
    std::string quantization;
    std::string family;
    uint64_t size_bytes = 0;
    bool is_local = false;
    bool is_file = false;
  };

  std::vector<ModelRow> rows;
  if (!model_cache.available_models.empty()) {
    rows.reserve(model_cache.available_models.size());
    for (const auto& info : model_cache.available_models) {
      ModelRow row;
      row.name = info.name;
      row.provider = info.provider;
      row.param_size = info.parameter_size;
      row.quantization = info.quantization;
      row.family = info.family;
      row.size_bytes = info.size_bytes;
      row.is_local = info.is_local;
      rows.push_back(std::move(row));
    }
  } else {
    rows.reserve(model_cache.model_names.size());
    for (const auto& model_name : model_cache.model_names) {
      ModelRow row;
      row.name = model_name;
      row.provider = config.ai_provider;
      rows.push_back(std::move(row));
    }
  }

  if (!filter_by_provider && !model_cache.local_model_names.empty()) {
    for (const auto& model_name : model_cache.local_model_names) {
      ModelRow row;
      row.name = model_name;
      row.provider = "local";
      row.is_local = true;
      row.is_file = true;
      rows.push_back(std::move(row));
    }
  }

  auto get_provider_color = [&theme](const std::string& provider) -> ImVec4 {
    if (provider == cli::kProviderOllama || provider == cli::kProviderLmStudio)
      return theme.provider_ollama;
    if (provider == cli::kProviderGemini || provider == cli::kProviderGeminiCli)
      return theme.provider_gemini;
    if (provider == cli::kProviderAnthropic || provider == cli::kProviderOpenAi)
      return theme.provider_openai;
    if (cli::IsCliHarnessProvider(provider))
      return theme.accent_color;
    return theme.provider_mock;
  };

  auto matches_filter = [&](const ModelRow& row) {
    if (filter.empty()) {
      return true;
    }
    std::string lower_name = absl::AsciiStrToLower(row.name);
    std::string lower_provider = absl::AsciiStrToLower(row.provider);
    if (ContainsText(lower_name, filter) ||
        ContainsText(lower_provider, filter)) {
      return true;
    }
    if (!row.param_size.empty() &&
        ContainsText(absl::AsciiStrToLower(row.param_size), filter)) {
      return true;
    }
    if (!row.family.empty() &&
        ContainsText(absl::AsciiStrToLower(row.family), filter)) {
      return true;
    }
    if (!row.quantization.empty() &&
        ContainsText(absl::AsciiStrToLower(row.quantization), filter)) {
      return true;
    }
    return false;
  };

  if (rows.empty()) {
    ImGui::TextDisabled("No cached models. Refresh to discover.");
  } else {
    float list_width = ImGui::GetContentRegionAvail().x;
    bool compact = list_width < 520.0f;
    int column_count = compact ? 3 : 5;
    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_BordersInnerH |
                                  ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("ModelTable", column_count, table_flags)) {
      if (compact) {
        ImGui::TableSetupColumn("Provider", ImGuiTableColumnFlags_WidthFixed,
                                90.0f);
        ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                                70.0f);
      } else {
        ImGui::TableSetupColumn("Provider", ImGuiTableColumnFlags_WidthFixed,
                                90.0f);
        ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed,
                                80.0f);
        ImGui::TableSetupColumn("Meta", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                                80.0f);
      }
      ImGui::TableHeadersRow();

      int row_id = 0;
      for (const auto& row : rows) {
        if (filter_by_provider) {
          if (row.provider == "local") {
            continue;
          }
          const bool matches_provider =
              row.provider.empty() || row.provider == config.ai_provider ||
              (config.ai_provider == cli::kProviderLmStudio &&
               row.provider == cli::kProviderOpenAi && row.is_local);
          if (!matches_provider) {
            continue;
          }
        }

        if (!matches_filter(row)) {
          continue;
        }

        ImGui::PushID(row_id++);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (row.provider.empty()) {
          ImGui::TextDisabled("-");
        } else if (row.provider == "local") {
          ImGui::TextDisabled(ICON_MD_FOLDER " local");
        } else {
          ImVec4 provider_color = get_provider_color(row.provider);
          {
            gui::StyleColorGuard badge_color(ImGuiCol_Button, provider_color);
            gui::StyleVarGuard badge_var(
                {{ImGuiStyleVar_FrameRounding, 6.0f},
                 {ImGuiStyleVar_FramePadding, ImVec2(4, 1)}});
            ImGui::SmallButton(row.provider.c_str());
          }
        }

        ImGui::TableSetColumnIndex(1);
        bool is_selected = config.ai_model == row.name;
        if (ImGui::Selectable(row.name.c_str(), is_selected)) {
          config.ai_model = row.name;
          std::snprintf(config.model_buffer, sizeof(config.model_buffer), "%s",
                        row.name.c_str());
          if (!row.provider.empty() && row.provider != "local") {
            config.ai_provider = row.provider;
            std::snprintf(config.provider_buffer,
                          sizeof(config.provider_buffer), "%s",
                          row.provider.c_str());
          }
        }

        if (row.is_file && ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "Local file detected. Serve this model via LM Studio/Ollama to "
              "use it.");
        }

        std::string size_label = row.param_size;
        if (size_label.empty() && row.size_bytes > 0) {
          size_label = FormatByteSize(row.size_bytes);
        }

        if (!compact) {
          ImGui::TableSetColumnIndex(2);
          ImGui::TextColored(theme.text_secondary_color, "%s",
                             size_label.c_str());

          ImGui::TableSetColumnIndex(3);
          if (!row.quantization.empty()) {
            ImGui::TextColored(theme.text_info, "%s", row.quantization.c_str());
            if (!row.family.empty()) {
              ImGui::SameLine();
            }
          }
          if (!row.family.empty()) {
            ImGui::TextColored(theme.text_secondary_gray, "%s",
                               row.family.c_str());
          }
          if (row.is_local && !row.is_file) {
            ImGui::SameLine();
            ImGui::TextColored(theme.status_success, ICON_MD_COMPUTER);
          }
        } else if (ImGui::IsItemHovered() &&
                   (!size_label.empty() || !row.quantization.empty() ||
                    !row.family.empty())) {
          std::string meta;
          if (!size_label.empty()) {
            meta += size_label;
          }
          if (!row.quantization.empty()) {
            if (!meta.empty()) {
              meta += " • ";
            }
            meta += row.quantization;
          }
          if (!row.family.empty()) {
            if (!meta.empty()) {
              meta += " • ";
            }
            meta += row.family;
          }
          ImGui::SetTooltip("%s", meta.c_str());
        }

        int action_column = compact ? 2 : 4;
        ImGui::TableSetColumnIndex(action_column);
        gui::StyleVarGuard action_var(ImGuiStyleVar_FramePadding, ImVec2(4, 1));
        bool is_favorite = std::find(config.favorite_models.begin(),
                                     config.favorite_models.end(),
                                     row.name) != config.favorite_models.end();
        {
          gui::StyleColorGuard star_color(
              ImGuiCol_Text,
              is_favorite ? theme.status_warning : theme.text_secondary_color);
          if (ImGui::SmallButton(is_favorite ? ICON_MD_STAR
                                             : ICON_MD_STAR_BORDER)) {
            if (is_favorite) {
              config.favorite_models.erase(
                  std::remove(config.favorite_models.begin(),
                              config.favorite_models.end(), row.name),
                  config.favorite_models.end());
              config.model_chain.erase(
                  std::remove(config.model_chain.begin(),
                              config.model_chain.end(), row.name),
                  config.model_chain.end());
            } else {
              config.favorite_models.push_back(row.name);
            }
          }
        }

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(is_favorite ? "Remove from favorites"
                                        : "Favorite model");
        }

        if (!row.provider.empty() && row.provider != "local") {
          ImGui::SameLine();
          if (ImGui::SmallButton(ICON_MD_NOTE_ADD)) {
            ModelPreset preset;
            preset.name = row.name;
            preset.model = row.name;
            preset.provider = row.provider;
            if (row.provider == cli::kProviderOllama) {
              preset.host = config.ollama_host;
            } else if (row.provider == cli::kProviderOpenAi) {
              preset.host = config.openai_base_url;
            }
            preset.tags = {row.provider};
            preset.last_used = absl::Now();
            config.model_presets.push_back(std::move(preset));
            if (toast_manager) {
              toast_manager->Show("Preset captured", ToastType::kSuccess, 2.0f);
            }
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Capture preset from this model");
          }
        }

        ImGui::PopID();
      }
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();

  if (model_cache.last_refresh != absl::InfinitePast()) {
    double seconds =
        absl::ToDoubleSeconds(absl::Now() - model_cache.last_refresh);
    ImGui::TextDisabled("Last refresh %.0fs ago", seconds);
  } else {
    ImGui::TextDisabled("Models not refreshed yet");
  }

  if (config.ai_provider == cli::kProviderOllama) {
    RenderChainModeControls(config);
  }

  if (!config.favorite_models.empty()) {
    ImGui::Separator();
    ImGui::TextColored(theme.status_warning, ICON_MD_STAR " Favorites");
    for (size_t i = 0; i < config.favorite_models.size(); ++i) {
      auto& favorite = config.favorite_models[i];
      ImGui::PushID(static_cast<int>(i));
      bool active = config.ai_model == favorite;

      std::string provider_name;
      for (const auto& info : model_cache.available_models) {
        if (info.name == favorite) {
          provider_name = info.provider;
          break;
        }
      }

      if (!provider_name.empty()) {
        ImVec4 badge_color = theme.provider_mock;
        if (provider_name == cli::kProviderOllama)
          badge_color = theme.provider_ollama;
        else if (provider_name == cli::kProviderGemini)
          badge_color = theme.provider_gemini;
        else if (provider_name == cli::kProviderAnthropic)
          badge_color = theme.provider_openai;
        else if (provider_name == cli::kProviderOpenAi)
          badge_color = theme.provider_openai;
        {
          gui::StyleColorGuard fav_badge_color(ImGuiCol_Button, badge_color);
          gui::StyleVarGuard fav_badge_var(
              {{ImGuiStyleVar_FrameRounding, 6.0f},
               {ImGuiStyleVar_FramePadding, ImVec2(3, 1)}});
          ImGui::SmallButton(provider_name.c_str());
        }
        ImGui::SameLine();
      }

      if (ImGui::Selectable(favorite.c_str(), active)) {
        config.ai_model = favorite;
        std::snprintf(config.model_buffer, sizeof(config.model_buffer), "%s",
                      favorite.c_str());
        if (!provider_name.empty()) {
          config.ai_provider = provider_name;
          std::snprintf(config.provider_buffer, sizeof(config.provider_buffer),
                        "%s", provider_name.c_str());
        }
      }
      ImGui::SameLine();
      {
        gui::StyleColorGuard close_color(ImGuiCol_Text, theme.status_error);
        if (ImGui::SmallButton(ICON_MD_CLOSE)) {
          config.model_chain.erase(
              std::remove(config.model_chain.begin(), config.model_chain.end(),
                          favorite),
              config.model_chain.end());
          config.favorite_models.erase(config.favorite_models.begin() + i);
          ImGui::PopID();
          break;
        }
      }
      ImGui::PopID();
    }
  }
}

void AgentConfigPanel::RenderModelDeck(AgentUIContext* context,
                                       const Callbacks& callbacks,
                                       ToastManager* toast_manager) {
  const auto& theme = AgentUI::GetTheme();
  ImGuiStyle& style = ImGui::GetStyle();
  auto& config = context->agent_config();
  auto& model_cache = context->model_cache();

  ImGui::TextDisabled("Presets");
  if (config.model_presets.empty()) {
    ImGui::TextDisabled("Capture a preset to swap models quickly.");
  }

  float capture_width = ImGui::CalcTextSize(ICON_MD_NOTE_ADD " Capture").x +
                        style.FramePadding.x * 2.0f;
  float capture_input_width =
      ImGui::GetContentRegionAvail().x - capture_width - style.ItemSpacing.x;
  if (capture_input_width > 120.0f) {
    ImGui::SetNextItemWidth(capture_input_width);
  }
  ImGui::InputTextWithHint("##new_preset_name", "Preset name...",
                           model_cache.new_preset_name,
                           IM_ARRAYSIZE(model_cache.new_preset_name));
  if (capture_input_width > 120.0f) {
    ImGui::SameLine();
  }
  if (ImGui::SmallButton(ICON_MD_NOTE_ADD " Capture")) {
    ModelPreset preset;
    preset.name = model_cache.new_preset_name[0]
                      ? std::string(model_cache.new_preset_name)
                      : config.ai_model;
    preset.model = config.ai_model;
    preset.provider = config.ai_provider;
    if (config.ai_provider == cli::kProviderOllama) {
      preset.host = config.ollama_host;
    } else if (config.ai_provider == cli::kProviderOpenAi) {
      preset.host = config.openai_base_url;
    }
    preset.tags = {config.ai_provider};
    preset.last_used = absl::Now();
    config.model_presets.push_back(std::move(preset));
    model_cache.new_preset_name[0] = '\0';
    if (toast_manager) {
      toast_manager->Show("Captured chat preset", ToastType::kSuccess, 2.0f);
    }
  }

  float deck_height = std::max(90.0f, ImGui::GetContentRegionAvail().y * 0.32f);
  gui::StyledChild preset_child("PresetList", ImVec2(0, deck_height),
                                {.bg = theme.panel_bg_darker}, true);
  if (config.model_presets.empty()) {
    ImGui::TextDisabled("No presets yet");
  } else {
    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_BordersInnerH |
                                  ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("PresetTable", 3, table_flags)) {
      ImGui::TableSetupColumn("Preset", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Host/Provider",
                              ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                              90.0f);
      ImGui::TableHeadersRow();

      for (int i = 0; i < static_cast<int>(config.model_presets.size()); ++i) {
        auto& preset = config.model_presets[i];
        ImGui::PushID(i);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        bool selected = model_cache.active_preset_index == i;
        if (ImGui::Selectable(preset.name.c_str(), selected)) {
          model_cache.active_preset_index = i;
          if (callbacks.apply_preset) {
            callbacks.apply_preset(preset);
          }
        }
        if (ImGui::IsItemHovered()) {
          std::string tooltip = absl::StrFormat("Model: %s", preset.model);
          if (!preset.tags.empty()) {
            tooltip +=
                absl::StrFormat("\nTags: %s", absl::StrJoin(preset.tags, ", "));
          }
          if (preset.last_used != absl::InfinitePast()) {
            tooltip += absl::StrFormat("\nLast used %s",
                                       FormatRelativeTime(preset.last_used));
          }
          ImGui::SetTooltip("%s", tooltip.c_str());
        }

        ImGui::TableSetColumnIndex(1);
        if (!preset.host.empty()) {
          ImGui::TextDisabled("%s", preset.host.c_str());
        } else if (!preset.provider.empty()) {
          ImGui::TextDisabled("%s", preset.provider.c_str());
        } else {
          ImGui::TextDisabled("-");
        }

        ImGui::TableSetColumnIndex(2);
        {
          gui::StyleVarGuard preset_action_var(ImGuiStyleVar_FramePadding,
                                               ImVec2(4, 1));
          if (ImGui::SmallButton(ICON_MD_PLAY_ARROW "##apply")) {
            model_cache.active_preset_index = i;
            if (callbacks.apply_preset) {
              callbacks.apply_preset(preset);
            }
          }
          ImGui::SameLine();
          if (ImGui::SmallButton(preset.pinned ? ICON_MD_STAR
                                               : ICON_MD_STAR_BORDER)) {
            preset.pinned = !preset.pinned;
          }
          ImGui::SameLine();
          if (ImGui::SmallButton(ICON_MD_DELETE)) {
            config.model_presets.erase(config.model_presets.begin() + i);
            if (model_cache.active_preset_index == i) {
              model_cache.active_preset_index = -1;
            }
            ImGui::PopID();
            break;
          }
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
  }
}

void AgentConfigPanel::RenderParameterControls(AgentConfigState& config) {
  ImGui::SliderFloat("Temperature", &config.temperature, 0.0f, 1.5f);
  ImGui::SliderFloat("Top P", &config.top_p, 0.0f, 1.0f);
  ImGui::SliderInt("Max Output Tokens", &config.max_output_tokens, 256, 8192);
  ImGui::SliderInt("Max Tool Iterations", &config.max_tool_iterations, 1, 10);
  ImGui::SliderInt("Max Retry Attempts", &config.max_retry_attempts, 0, 5);
  ImGui::Checkbox("Stream responses", &config.stream_responses);
  ImGui::SameLine();
  ImGui::Checkbox("Show reasoning", &config.show_reasoning);
  ImGui::SameLine();
  ImGui::Checkbox("Verbose logs", &config.verbose);
}

void AgentConfigPanel::RenderToolingControls(AgentConfigState& config,
                                             const Callbacks& callbacks) {
  struct ToolToggleEntry {
    const char* label;
    bool* flag;
    const char* hint;
  } entries[] = {
      {"Resources", &config.tool_config.resources, "resource-list/search"},
      {"Dungeon", &config.tool_config.dungeon, "Room + sprite inspection"},
      {"Overworld", &config.tool_config.overworld, "Map + entrance analysis"},
      {"Dialogue", &config.tool_config.dialogue, "Dialogue list/search"},
      {"Messages", &config.tool_config.messages, "Message table + ROM text"},
      {"GUI Automation", &config.tool_config.gui, "GUI automation tools"},
      {"Music", &config.tool_config.music, "Music info & tracks"},
      {"Sprite", &config.tool_config.sprite, "Sprite palette/properties"},
      {"Emulator", &config.tool_config.emulator, "Emulator controls"},
      {"Memory", &config.tool_config.memory_inspector,
       "RAM inspection & watch"}};

  ImGui::TextDisabled("Expose tools in the agent sidebar and editor panels.");
  int columns = ImGui::GetContentRegionAvail().x > 360.0f ? 2 : 1;
  ImGuiTableFlags table_flags =
      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg;
  if (ImGui::BeginTable("AgentToolTable", columns, table_flags)) {
    for (size_t i = 0; i < std::size(entries); ++i) {
      ImGui::TableNextColumn();
      if (ImGui::Checkbox(entries[i].label, entries[i].flag)) {
        if (callbacks.apply_tool_preferences) {
          callbacks.apply_tool_preferences();
        }
      }
      if (ImGui::IsItemHovered() && entries[i].hint) {
        ImGui::SetTooltip("%s", entries[i].hint);
      }
    }
    ImGui::EndTable();
  }
}

void AgentConfigPanel::RenderChainModeControls(AgentConfigState& config) {
  ImGui::Spacing();
  ImGui::TextDisabled("Chain Mode (Experimental)");

  bool round_robin = config.chain_mode == ChainMode::kRoundRobin;
  if (ImGui::Checkbox("Round Robin", &round_robin)) {
    config.chain_mode =
        round_robin ? ChainMode::kRoundRobin : ChainMode::kDisabled;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Rotate through favorite models for each response");
  }

  ImGui::SameLine();
  bool consensus = config.chain_mode == ChainMode::kConsensus;
  if (ImGui::Checkbox("Consensus", &consensus)) {
    config.chain_mode =
        consensus ? ChainMode::kConsensus : ChainMode::kDisabled;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Ask multiple models and synthesize a response");
  }
}

}  // namespace editor
}  // namespace yaze
