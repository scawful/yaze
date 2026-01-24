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
  static bool filter_by_provider = false;

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
  const float label_width = 120.0f;
  const ImVec2 compact_padding(style.FramePadding.x,
                               std::max(2.0f, style.FramePadding.y * 0.6f));
  const ImVec2 compact_spacing(style.ItemSpacing.x,
                               std::max(3.0f, style.ItemSpacing.y * 0.6f));
  const float env_button_width =
      ImGui::CalcTextSize(ICON_MD_SYNC " Env").x + compact_padding.x * 2.0f;

  auto set_openai_base = [&](const std::string& base_url) {
    std::snprintf(config.openai_base_url_buffer,
                  sizeof(config.openai_base_url_buffer), "%s",
                  base_url.c_str());
    config.openai_base_url = config.openai_base_url_buffer;
  };

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
    if (ImGui::Button(label, ImVec2(-FLT_MIN, 28))) {
      config.ai_provider = value;
      std::snprintf(config.provider_buffer, sizeof(config.provider_buffer),
                    "%s", value);
    }
    if (active) {
      ImGui::PopStyleColor(2);
    }
  };

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, compact_padding);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, compact_spacing);
  if (ImGui::BeginTable("AgentProviderConfigTable", 2,
                        ImGuiTableFlags_SizingFixedFit |
                            ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                            label_width);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Provider");
    ImGui::TableSetColumnIndex(1);
    float provider_width = ImGui::GetContentRegionAvail().x;
    int provider_columns = provider_width > 560.0f ? 3
                           : provider_width > 360.0f ? 2
                                                     : 1;
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

    auto key_row = [&](const char* label, const char* hint, char* buffer,
                       size_t buffer_len, std::string* target,
                       const char* env_var, const char* input_id,
                       const char* button_id) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("%s", label);
      ImGui::TableSetColumnIndex(1);
      float input_width =
          ImGui::GetContentRegionAvail().x - env_button_width -
          style.ItemSpacing.x;
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
          toast_manager->Show(
              absl::StrFormat("%s not set", env_var), ToastType::kWarning,
              2.0f);
        }
      }
    };

    key_row("Gemini Key", "API key...", config.gemini_key_buffer,
            IM_ARRAYSIZE(config.gemini_key_buffer), &config.gemini_api_key,
            "GEMINI_API_KEY", "##gemini_key",
            ICON_MD_SYNC " Env##gemini");
    key_row("Anthropic Key", "API key...", config.anthropic_key_buffer,
            IM_ARRAYSIZE(config.anthropic_key_buffer),
            &config.anthropic_api_key, "ANTHROPIC_API_KEY", "##anthropic_key",
            ICON_MD_SYNC " Env##anthropic");
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
    float reset_button_width =
        ImGui::CalcTextSize("Reset").x + compact_padding.x * 2.0f;
    float total_buttons =
        openai_button_width + lm_button_width + reset_button_width +
        style.ItemSpacing.x * 2.0f;
    float base_available = ImGui::GetContentRegionAvail().x;
    bool base_stack = base_available < total_buttons + 160.0f;
    ImGui::SetNextItemWidth(
        base_stack ? -1 : base_available - total_buttons - style.ItemSpacing.x);
    if (ImGui::InputTextWithHint("##openai_base", "http://localhost:1234",
                                 config.openai_base_url_buffer,
                                 IM_ARRAYSIZE(config.openai_base_url_buffer))) {
      config.openai_base_url = config.openai_base_url_buffer;
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
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
      set_openai_base("https://api.openai.com");
    }

    ImGui::EndTable();
  }
  ImGui::PopStyleVar(2);

  if (IsLocalEndpoint(config.openai_base_url)) {
    ImGui::TextColored(theme.status_success,
                       ICON_MD_COMPUTER " Local OpenAI-compatible server");
  } else {
    ImGui::TextColored(theme.text_secondary_color,
                       ICON_MD_PUBLIC " Remote OpenAI endpoint");
  }

  ImGui::Spacing();

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, compact_padding);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, compact_spacing);
  if (ImGui::BeginTable("AgentModelControls", 2,
                        ImGuiTableFlags_SizingFixedFit |
                            ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                            label_width);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Model");
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##ai_model", "Model name...",
                                 config.model_buffer,
                                 IM_ARRAYSIZE(config.model_buffer))) {
      config.ai_model = config.model_buffer;
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
      if (callbacks.refresh_models) {
        callbacks.refresh_models(true);
      }
    }

    ImGui::EndTable();
  }
  ImGui::PopStyleVar(2);
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

  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_darker);
  float list_height =
      std::max(220.0f, ImGui::GetContentRegionAvail().y * 0.6f);
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
    ImGuiTableFlags table_flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
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
          if (!row.provider.empty() && row.provider != config.ai_provider) {
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
          ImGui::PushStyleColor(ImGuiCol_Button, provider_color);
          ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
          ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 1));
          ImGui::SmallButton(row.provider.c_str());
          ImGui::PopStyleVar(2);
          ImGui::PopStyleColor();
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
            ImGui::TextColored(theme.text_info, "%s",
                               row.quantization.c_str());
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
        } else if (ImGui::IsItemHovered() && (!size_label.empty() ||
                                             !row.quantization.empty() ||
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
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 1));
        bool is_favorite =
            std::find(config.favorite_models.begin(),
                      config.favorite_models.end(),
                      row.name) != config.favorite_models.end();
        ImGui::PushStyleColor(ImGuiCol_Text, is_favorite
                                                 ? theme.status_warning
                                                 : theme.text_secondary_color);
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
        ImGui::PopStyleColor();

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
            if (row.provider == "ollama") {
              preset.host = config.ollama_host;
            } else if (row.provider == "openai") {
              preset.host = config.openai_base_url;
            }
            preset.tags = {row.provider};
            preset.last_used = absl::Now();
            config.model_presets.push_back(std::move(preset));
            if (toast_manager) {
              toast_manager->Show("Preset captured", ToastType::kSuccess,
                                  2.0f);
            }
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Capture preset from this model");
          }
        }
        ImGui::PopStyleVar();

        ImGui::PopID();
      }
      ImGui::EndTable();
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
  ImGuiStyle& style = ImGui::GetStyle();
  auto& config = context->agent_config();
  auto& model_cache = context->model_cache();

  ImGui::TextDisabled("Presets");
  if (config.model_presets.empty()) {
    ImGui::TextDisabled("Capture a preset to swap models quickly.");
  }

  float capture_width =
      ImGui::CalcTextSize(ICON_MD_NOTE_ADD " Capture").x +
      style.FramePadding.x * 2.0f;
  float capture_input_width = ImGui::GetContentRegionAvail().x -
                              capture_width - style.ItemSpacing.x;
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
      std::max(90.0f, ImGui::GetContentRegionAvail().y * 0.32f);
  ImGui::BeginChild("PresetList", ImVec2(0, deck_height), true);
  if (config.model_presets.empty()) {
    ImGui::TextDisabled("No presets yet");
  } else {
    ImGuiTableFlags table_flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
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
            tooltip += absl::StrFormat("\nTags: %s",
                                       absl::StrJoin(preset.tags, ", "));
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
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 1));
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
          ImGui::PopStyleVar();
          ImGui::PopID();
          break;
        }
        ImGui::PopStyleVar();
        ImGui::PopID();
      }
      ImGui::EndTable();
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
          {"Emulator", &config.tool_config.emulator, "Emulator controls"},
          {"Memory", &config.tool_config.memory_inspector, "RAM inspection & watch"}};
      
  ImGui::TextDisabled(
      "Expose tools in the agent sidebar and editor panels.");
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
