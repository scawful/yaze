#include "app/editor/agent/panels/agent_configuration_panel.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/time/clock.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

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
  return ContainsText(lower, "localhost") ||
         ContainsText(lower, "127.0.0.1") ||
         ContainsText(lower, "0.0.0.0") ||
         ContainsText(lower, "::1") ||
         // LAN IPs (rudimentary check)
         ContainsText(lower, "192.168.") || StartsWithText(lower, "10.") ||
         // LM Studio default port check just in case domain differs
         ContainsText(lower, ":1234");
}

}  // namespace

void AgentConfigPanel::Draw(AgentUIContext* context,
                            const Callbacks& callbacks,
                            ToastManager* toast_manager) {
  const auto& theme = AgentUI::GetTheme();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_color);
  ImGui::BeginChild("AgentConfig", ImVec2(0, 0), true);
  AgentUI::RenderSectionHeader(ICON_MD_SETTINGS, "Agent Builder",
                               theme.command_text_color);

  if (ImGui::BeginTabBar("AgentConfigTabs",
                         ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
    if (ImGui::BeginTabItem(ICON_MD_SMART_TOY " Models")) {
      RenderModelConfigControls(context, callbacks, toast_manager);
      ImGui::Separator();
      RenderModelDeck(context, callbacks, toast_manager);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(ICON_MD_TUNE " Parameters")) {
      RenderParameterControls(context->agent_config());
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(ICON_MD_CONSTRUCTION " Tools")) {
      RenderToolingControls(context->agent_config(), callbacks);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
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

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void AgentConfigPanel::RenderModelConfigControls(
    AgentUIContext* context, const Callbacks& callbacks,
    ToastManager* toast_manager) {
  const auto& theme = AgentUI::GetTheme();
  ImGuiStyle& style = ImGui::GetStyle();
  auto& config = context->agent_config();
  auto& model_cache = context->model_cache();

  if (model_cache.last_provider != config.ai_provider ||
      model_cache.last_openai_base != config.openai_base_url ||
      model_cache.last_ollama_host != config.ollama_host) {
    model_cache.auto_refresh_requested = false;
    model_cache.last_provider = config.ai_provider;
    model_cache.last_openai_base = config.openai_base_url;
    model_cache.last_ollama_host = config.ollama_host;
  }

  if (callbacks.refresh_models && !model_cache.loading &&
      !model_cache.auto_refresh_requested) {
    model_cache.auto_refresh_requested = true;
    callbacks.refresh_models(false);
  }

  ImGui::Text("Provider");
  float provider_width = ImGui::GetContentRegionAvail().x;
  int provider_columns = provider_width > 560.0f ? 3
                         : provider_width > 360.0f ? 2
                                                   : 1;

  // Provider selection buttons using theme colors
  auto provider_button = [&](const char* label, const char* value,
                             const ImVec4& color) {
    bool active = config.ai_provider == value;
    ImGui::TableNextColumn();
    if (active) {
      ImGui::PushStyleColor(ImGuiCol_Button, color);
      ImGui::PushStyleColor(
          ImGuiCol_ButtonHovered,
          ImVec4(color.x * 1.15f, color.y * 1.15f, color.z * 1.15f, color.w));
    }
    if (ImGui::Button(label, ImVec2(-FLT_MIN, 30))) {
      config.ai_provider = value;
      std::snprintf(config.provider_buffer, sizeof(config.provider_buffer),
                    "%s", value);
    }
    if (active) {
      ImGui::PopStyleColor(2);
    }
  };

  if (ImGui::BeginTable("AgentProviderButtons", provider_columns,
                        ImGuiTableFlags_SizingStretchSame)) {
    provider_button(ICON_MD_SETTINGS " Mock", "mock", theme.provider_mock);
    provider_button(ICON_MD_CLOUD " Ollama", "ollama", theme.provider_ollama);
    provider_button(ICON_MD_SMART_TOY " Gemini", "gemini",
                    theme.provider_gemini);
    provider_button(ICON_MD_PSYCHOLOGY " Anthropic", "anthropic",
                    theme.provider_openai);
    provider_button(ICON_MD_AUTO_AWESOME " OpenAI", "openai",
                    theme.provider_openai);
    ImGui::EndTable();
  }
  ImGui::NewLine();

  // Provider-specific configuration
  ImGui::Text("Ollama Host");
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputTextWithHint("##ollama_host", "http://localhost:11434",
                               config.ollama_host_buffer,
                               IM_ARRAYSIZE(config.ollama_host_buffer))) {
    config.ollama_host = config.ollama_host_buffer;
  }

  ImGui::Spacing();
  ImGui::Text("Gemini Key");
  float env_button_width =
      ImGui::CalcTextSize(ICON_MD_SYNC " Env").x + style.FramePadding.x * 2.0f;
  float gemini_input_width =
      ImGui::GetContentRegionAvail().x - env_button_width -
      style.ItemSpacing.x;
  bool gemini_stack = gemini_input_width < 160.0f;
  if (!gemini_stack) {
    ImGui::SetNextItemWidth(gemini_input_width);
  } else {
    ImGui::SetNextItemWidth(-1);
  }
  if (ImGui::InputTextWithHint("##gemini_key", "API key...",
                               config.gemini_key_buffer,
                               IM_ARRAYSIZE(config.gemini_key_buffer),
                               ImGuiInputTextFlags_Password)) {
    config.gemini_api_key = config.gemini_key_buffer;
  }
  if (!gemini_stack) {
    ImGui::SameLine();
  }
  if (ImGui::SmallButton(ICON_MD_SYNC " Env##gemini")) {
    const char* env_key = std::getenv("GEMINI_API_KEY");
    if (env_key) {
      std::snprintf(config.gemini_key_buffer, sizeof(config.gemini_key_buffer),
                    "%s", env_key);
      config.gemini_api_key = env_key;
      if (toast_manager) {
        toast_manager->Show("Loaded GEMINI_API_KEY from environment",
                            ToastType::kInfo, 2.0f);
      }
    } else if (toast_manager) {
      toast_manager->Show("GEMINI_API_KEY not set", ToastType::kWarning, 2.0f);
    }
  }

  ImGui::Spacing();
  ImGui::Text("Anthropic Key");
  float anthropic_input_width =
      ImGui::GetContentRegionAvail().x - env_button_width -
      style.ItemSpacing.x;
  bool anthropic_stack = anthropic_input_width < 160.0f;
  if (!anthropic_stack) {
    ImGui::SetNextItemWidth(anthropic_input_width);
  } else {
    ImGui::SetNextItemWidth(-1);
  }
  if (ImGui::InputTextWithHint("##anthropic_key", "API key...",
                               config.anthropic_key_buffer,
                               IM_ARRAYSIZE(config.anthropic_key_buffer),
                               ImGuiInputTextFlags_Password)) {
    config.anthropic_api_key = config.anthropic_key_buffer;
  }
  if (!anthropic_stack) {
    ImGui::SameLine();
  }
  if (ImGui::SmallButton(ICON_MD_SYNC " Env##anthropic")) {
    const char* env_key = std::getenv("ANTHROPIC_API_KEY");
    if (env_key) {
      std::snprintf(config.anthropic_key_buffer,
                    sizeof(config.anthropic_key_buffer), "%s", env_key);
      config.anthropic_api_key = env_key;
      if (toast_manager) {
        toast_manager->Show("Loaded ANTHROPIC_API_KEY from environment",
                            ToastType::kInfo, 2.0f);
      }
    } else if (toast_manager) {
      toast_manager->Show("ANTHROPIC_API_KEY not set", ToastType::kWarning,
                          2.0f);
    }
  }

  ImGui::Spacing();
  ImGui::Text("OpenAI Key");
  float openai_input_width =
      ImGui::GetContentRegionAvail().x - env_button_width -
      style.ItemSpacing.x;
  bool openai_stack = openai_input_width < 160.0f;
  if (!openai_stack) {
    ImGui::SetNextItemWidth(openai_input_width);
  } else {
    ImGui::SetNextItemWidth(-1);
  }
  if (ImGui::InputTextWithHint("##openai_key", "API key...",
                               config.openai_key_buffer,
                               IM_ARRAYSIZE(config.openai_key_buffer),
                               ImGuiInputTextFlags_Password)) {
    config.openai_api_key = config.openai_key_buffer;
  }
  if (!openai_stack) {
    ImGui::SameLine();
  }
  if (ImGui::SmallButton(ICON_MD_SYNC " Env##openai")) {
    const char* env_key = std::getenv("OPENAI_API_KEY");
    if (env_key) {
      std::snprintf(config.openai_key_buffer, sizeof(config.openai_key_buffer),
                    "%s", env_key);
      config.openai_api_key = env_key;
      if (toast_manager) {
        toast_manager->Show("Loaded OPENAI_API_KEY from environment",
                            ToastType::kInfo, 2.0f);
      }
    } else if (toast_manager) {
      toast_manager->Show("OPENAI_API_KEY not set", ToastType::kWarning, 2.0f);
    }
  }

  auto set_openai_base = [&](const std::string& base_url) {
    std::snprintf(config.openai_base_url_buffer,
                  sizeof(config.openai_base_url_buffer), "%s",
                  base_url.c_str());
    config.openai_base_url = config.openai_base_url_buffer;
  };

  ImGui::Spacing();
  ImGui::Text("OpenAI Base URL");
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputTextWithHint("##openai_base", "http://localhost:1234",
                               config.openai_base_url_buffer,
                               IM_ARRAYSIZE(config.openai_base_url_buffer))) {
    config.openai_base_url = config.openai_base_url_buffer;
  }
  if (IsLocalEndpoint(config.openai_base_url)) {
    ImGui::TextColored(theme.status_success,
                       ICON_MD_COMPUTER " Local OpenAI-compatible server");
  } else {
    ImGui::TextColored(theme.text_secondary_color,
                       ICON_MD_PUBLIC " Remote OpenAI endpoint");
  }
  if (ImGui::SmallButton("OpenAI")) {
    set_openai_base("https://api.openai.com");
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("LM Studio")) {
    set_openai_base("http://localhost:1234");
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Reset Base")) {
    set_openai_base("https://api.openai.com");
  }

  ImGui::Spacing();

  // Unified Model Selection
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputTextWithHint("##ai_model", "Model name...",
                               config.model_buffer,
                               IM_ARRAYSIZE(config.model_buffer))) {
    config.ai_model = config.model_buffer;
  }

  // Provider filter checkbox for unified model list
  static bool filter_by_provider = false;
  ImGui::Checkbox("Filter by selected provider", &filter_by_provider);
  ImGui::SameLine();
  AgentUI::HorizontalSpacing(8.0f);
  ImGui::SameLine();

  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
  ImGui::InputTextWithHint("##model_search", "Search all models...",
                           model_cache.search_buffer,
                           IM_ARRAYSIZE(model_cache.search_buffer));
  ImGui::SameLine();
  if (ImGui::Button(model_cache.loading ? ICON_MD_SYNC : ICON_MD_REFRESH)) {
    if (callbacks.refresh_models) {
      callbacks.refresh_models(true);
    }
  }

  // Use theme color for model list background
  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_darker);
  float list_height =
      std::max(160.0f, ImGui::GetContentRegionAvail().y * 0.45f);
  ImGui::BeginChild("UnifiedModelList", ImVec2(0, list_height), true);
  std::string filter = absl::AsciiStrToLower(model_cache.search_buffer);

  if (model_cache.available_models.empty() && model_cache.model_names.empty()) {
    ImGui::TextDisabled("No cached models. Refresh to discover.");
  } else {
    auto get_provider_color = [&theme](const std::string& provider) -> ImVec4 {
      if (provider == "ollama")
        return theme.provider_ollama;
      if (provider == "gemini")
        return theme.provider_gemini;
      if (provider == "anthropic")
        return theme.provider_openai;
      if (provider == "openai")
        return theme.provider_openai;
      return theme.provider_mock;
    };

    if (!model_cache.available_models.empty()) {
      int model_index = 0;
      for (const auto& info : model_cache.available_models) {
        std::string lower_name = absl::AsciiStrToLower(info.name);
        std::string lower_provider = absl::AsciiStrToLower(info.provider);

        if (filter_by_provider && info.provider != config.ai_provider) {
          continue;
        }

        if (!filter.empty()) {
          bool match = ContainsText(lower_name, filter) ||
                       ContainsText(lower_provider, filter);
          if (!match && !info.parameter_size.empty()) {
            match = ContainsText(absl::AsciiStrToLower(info.parameter_size),
                                 filter);
          }
          if (!match && !info.family.empty()) {
            match = ContainsText(absl::AsciiStrToLower(info.family), filter);
          }
          if (!match) {
            continue;
          }
        }

        ImGui::PushID(model_index++);

        bool is_selected = config.ai_model == info.name;

        ImVec4 provider_color = get_provider_color(info.provider);
        ImGui::PushStyleColor(ImGuiCol_Button, provider_color);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
        ImGui::SmallButton(info.provider.c_str());
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        ImGui::SameLine();

        if (ImGui::Selectable(
                info.name.c_str(), is_selected, ImGuiSelectableFlags_None,
                ImVec2(ImGui::GetContentRegionAvail().x - 60, 0))) {
          config.ai_model = info.name;
          config.ai_provider = info.provider;
          std::snprintf(config.model_buffer, sizeof(config.model_buffer), "%s",
                        info.name.c_str());
          std::snprintf(config.provider_buffer, sizeof(config.provider_buffer),
                        "%s", info.provider.c_str());
        }

        ImGui::SameLine();
        bool is_favorite = std::find(config.favorite_models.begin(),
                                     config.favorite_models.end(),
                                     info.name) != config.favorite_models.end();
        ImGui::PushStyleColor(ImGuiCol_Text, is_favorite
                                                 ? theme.status_warning
                                                 : theme.text_secondary_color);
        if (ImGui::SmallButton(is_favorite ? ICON_MD_STAR
                                           : ICON_MD_STAR_BORDER)) {
          if (is_favorite) {
            config.favorite_models.erase(
                std::remove(config.favorite_models.begin(),
                            config.favorite_models.end(), info.name),
                config.favorite_models.end());
            config.model_chain.erase(
                std::remove(config.model_chain.begin(),
                            config.model_chain.end(), info.name),
                config.model_chain.end());
          } else {
            config.favorite_models.push_back(info.name);
          }
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(is_favorite ? "Remove from favorites"
                                        : "Favorite model");
        }

        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MD_NOTE_ADD)) {
          ModelPreset preset;
          preset.name = info.name;
          preset.model = info.name;
          preset.provider = info.provider;
          if (info.provider == "ollama") {
            preset.host = config.ollama_host;
          } else if (info.provider == "openai") {
            preset.host = config.openai_base_url;
          }
          preset.tags = {info.provider};
          preset.last_used = absl::Now();
          config.model_presets.push_back(std::move(preset));
          if (toast_manager) {
            toast_manager->Show("Preset captured", ToastType::kSuccess, 2.0f);
          }
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Capture preset from this model");
        }

        std::string size_label = info.parameter_size.empty()
                                     ? FormatByteSize(info.size_bytes)
                                     : info.parameter_size;
        ImGui::TextColored(theme.text_secondary_color, "  %s",
                           size_label.c_str());
        if (!info.quantization.empty()) {
          ImGui::SameLine();
          ImGui::TextColored(theme.text_info, "  %s",
                             info.quantization.c_str());
        }
        if (!info.family.empty()) {
          ImGui::SameLine();
          ImGui::TextColored(theme.text_secondary_gray, "  Family: %s",
                             info.family.c_str());
        }
        if (info.is_local) {
          ImGui::SameLine();
          ImGui::TextColored(theme.status_success, "  " ICON_MD_COMPUTER);
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Running locally");
          }
        }
        ImGui::Separator();
        ImGui::PopID();
      }
    } else {
      // Fallback to just names
      int model_index = 0;
      for (const auto& model_name : model_cache.model_names) {
        std::string lower = absl::AsciiStrToLower(model_name);
        if (!filter.empty() && !ContainsText(lower, filter)) {
          continue;
        }

        ImGui::PushID(model_index++);

        bool is_selected = config.ai_model == model_name;
        if (ImGui::Selectable(model_name.c_str(), is_selected)) {
          config.ai_model = model_name;
          std::snprintf(config.model_buffer, sizeof(config.model_buffer), "%s",
                        model_name.c_str());
        }

        ImGui::SameLine();
        bool is_favorite =
            std::find(config.favorite_models.begin(),
                      config.favorite_models.end(),
                      model_name) != config.favorite_models.end();
        ImGui::PushStyleColor(ImGuiCol_Text, is_favorite
                                                 ? theme.status_warning
                                                 : theme.text_secondary_color);
        if (ImGui::SmallButton(is_favorite ? ICON_MD_STAR
                                           : ICON_MD_STAR_BORDER)) {
          if (is_favorite) {
            config.favorite_models.erase(
                std::remove(config.favorite_models.begin(),
                            config.favorite_models.end(), model_name),
                config.favorite_models.end());
          } else {
            config.favorite_models.push_back(model_name);
          }
        }
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::PopID();
      }
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();

  if (model_cache.last_refresh != absl::InfinitePast()) {
    double seconds =
        absl::ToDoubleSeconds(absl::Now() - model_cache.last_refresh);
    ImGui::TextDisabled("Last refresh %.0fs ago", seconds);
  } else {
    ImGui::TextDisabled("Models not refreshed yet");
  }

  if (config.ai_provider == "ollama") {
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
        if (provider_name == "ollama")
          badge_color = theme.provider_ollama;
        else if (provider_name == "gemini")
          badge_color = theme.provider_gemini;
        else if (provider_name == "anthropic")
          badge_color = theme.provider_openai;
        else if (provider_name == "openai")
          badge_color = theme.provider_openai;
        ImGui::PushStyleColor(ImGuiCol_Button, badge_color);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 1));
        ImGui::SmallButton(provider_name.c_str());
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
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
      ImGui::PushStyleColor(ImGuiCol_Text, theme.status_error);
      if (ImGui::SmallButton(ICON_MD_CLOSE)) {
        config.model_chain.erase(
            std::remove(config.model_chain.begin(), config.model_chain.end(),
                        favorite),
            config.model_chain.end());
        config.favorite_models.erase(config.favorite_models.begin() + i);
        ImGui::PopStyleColor();
        ImGui::PopID();
        break;
      }
      ImGui::PopStyleColor();
      ImGui::PopID();
    }
  }
}

void AgentConfigPanel::RenderModelDeck(AgentUIContext* context,
                                       const Callbacks& callbacks,
                                       ToastManager* toast_manager) {
  const auto& theme = AgentUI::GetTheme();
  auto& config = context->agent_config();
  auto& model_cache = context->model_cache();

  ImGui::TextDisabled("Model Deck");
  if (config.model_presets.empty()) {
    ImGui::TextWrapped(
        "Capture a preset to quickly swap between hosts/models with consistent "
        "tool stacks.");
  }
  ImGui::InputTextWithHint("##new_preset_name", "Preset name...",
                           model_cache.new_preset_name,
                           IM_ARRAYSIZE(model_cache.new_preset_name));
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_NOTE_ADD " Capture Current")) {
    ModelPreset preset;
    preset.name = model_cache.new_preset_name[0]
                      ? std::string(model_cache.new_preset_name)
                      : config.ai_model;
    preset.model = config.ai_model;
    preset.provider = config.ai_provider;
    if (config.ai_provider == "ollama") {
      preset.host = config.ollama_host;
    } else if (config.ai_provider == "openai") {
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

  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_darker);
  float deck_height =
      std::max(120.0f, ImGui::GetContentRegionAvail().y * 0.5f);
  ImGui::BeginChild("PresetList", ImVec2(0, deck_height), true);
  if (config.model_presets.empty()) {
    ImGui::TextDisabled("No presets yet");
  } else {
    for (int i = 0; i < static_cast<int>(config.model_presets.size()); ++i) {
      auto& preset = config.model_presets[i];
      ImGui::PushID(i);
      bool selected = model_cache.active_preset_index == i;
      if (ImGui::Selectable(preset.name.c_str(), selected)) {
        model_cache.active_preset_index = i;
        if (callbacks.apply_preset) {
          callbacks.apply_preset(preset);
        }
      }
      ImGui::SameLine();
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
      if (!preset.host.empty()) {
        ImGui::TextDisabled("%s", preset.host.c_str());
      }
      if (!preset.provider.empty()) {
        ImGui::TextDisabled("Provider: %s", preset.provider.c_str());
      }
      if (!preset.tags.empty()) {
        ImGui::TextDisabled("Tags: %s",
                            absl::StrJoin(preset.tags, ", ").c_str());
      }
      if (preset.last_used != absl::InfinitePast()) {
        ImGui::TextDisabled("Last used %s",
                            FormatRelativeTime(preset.last_used).c_str());
      }
      ImGui::Separator();
      ImGui::PopID();
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void AgentConfigPanel::RenderParameterControls(
    AgentConfigState& config) {
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

void AgentConfigPanel::RenderToolingControls(
    AgentConfigState& config, const Callbacks& callbacks) {
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
      {"Emulator", &config.tool_config.emulator, "Emulator controls"}};

  int columns = ImGui::GetContentRegionAvail().x > 360.0f ? 2 : 1;
  ImGui::Columns(columns, nullptr, false);
  for (size_t i = 0; i < std::size(entries); ++i) {
    if (ImGui::Checkbox(entries[i].label, entries[i].flag)) {
      if (callbacks.apply_tool_preferences) {
        callbacks.apply_tool_preferences();
      }
    }
    if (ImGui::IsItemHovered() && entries[i].hint) {
      ImGui::SetTooltip("%s", entries[i].hint);
    }
    ImGui::NextColumn();
  }
  ImGui::Columns(1);
}

void AgentConfigPanel::RenderChainModeControls(
    AgentConfigState& config) {
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
