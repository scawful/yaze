#include "app/editor/agent/agent_chat_history_codec.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace editor {

namespace {

#if defined(YAZE_WITH_JSON)
using Json = nlohmann::json;

absl::Time ParseTimestamp(const Json& value) {
  if (!value.is_string()) {
    return absl::Now();
  }
  absl::Time parsed;
  if (absl::ParseTime(absl::RFC3339_full, value.get<std::string>(), &parsed,
                      nullptr)) {
    return parsed;
  }
  return absl::Now();
}

Json SerializeTableData(const cli::agent::ChatMessage::TableData& table) {
  Json json;
  json["headers"] = table.headers;
  json["rows"] = table.rows;
  return json;
}

std::optional<cli::agent::ChatMessage::TableData> ParseTableData(
    const Json& json) {
  if (!json.is_object()) {
    return std::nullopt;
  }

  cli::agent::ChatMessage::TableData table;
  if (json.contains("headers") && json["headers"].is_array()) {
    for (const auto& header : json["headers"]) {
      if (header.is_string()) {
        table.headers.push_back(header.get<std::string>());
      }
    }
  }

  if (json.contains("rows") && json["rows"].is_array()) {
    for (const auto& row : json["rows"]) {
      if (!row.is_array()) {
        continue;
      }
      std::vector<std::string> row_values;
      for (const auto& value : row) {
        if (value.is_string()) {
          row_values.push_back(value.get<std::string>());
        } else {
          row_values.push_back(value.dump());
        }
      }
      table.rows.push_back(std::move(row_values));
    }
  }

  if (table.headers.empty() && table.rows.empty()) {
    return std::nullopt;
  }

  return table;
}

Json SerializeProposal(
    const cli::agent::ChatMessage::ProposalSummary& proposal) {
  Json json;
  json["id"] = proposal.id;
  json["change_count"] = proposal.change_count;
  json["executed_commands"] = proposal.executed_commands;
  json["sandbox_rom_path"] = proposal.sandbox_rom_path.string();
  json["proposal_json_path"] = proposal.proposal_json_path.string();
  return json;
}

std::optional<cli::agent::ChatMessage::ProposalSummary> ParseProposal(
    const Json& json) {
  if (!json.is_object()) {
    return std::nullopt;
  }

  cli::agent::ChatMessage::ProposalSummary summary;
  summary.id = json.value("id", "");
  summary.change_count = json.value("change_count", 0);
  summary.executed_commands = json.value("executed_commands", 0);
  if (json.contains("sandbox_rom_path") &&
      json["sandbox_rom_path"].is_string()) {
    summary.sandbox_rom_path = json["sandbox_rom_path"].get<std::string>();
  }
  if (json.contains("proposal_json_path") &&
      json["proposal_json_path"].is_string()) {
    summary.proposal_json_path = json["proposal_json_path"].get<std::string>();
  }
  if (summary.id.empty()) {
    return std::nullopt;
  }
  return summary;
}

#endif  // YAZE_WITH_GRPC

}  // namespace

bool AgentChatHistoryCodec::Available() {
#if defined(YAZE_WITH_JSON)
  return true;
#else
  return false;
#endif
}

absl::StatusOr<AgentChatHistoryCodec::Snapshot> AgentChatHistoryCodec::Load(
    const std::filesystem::path& path) {
#if defined(YAZE_WITH_JSON)
  Snapshot snapshot;

  std::ifstream file(path);
  if (!file.good()) {
    return snapshot;  // Treat missing file as empty history.
  }

  Json json;
  try {
    file >> json;
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to parse chat history: %s", e.what()));
  }

  if (!json.contains("messages") || !json["messages"].is_array()) {
    return snapshot;
  }

  for (const auto& item : json["messages"]) {
    if (!item.is_object()) {
      continue;
    }

    cli::agent::ChatMessage message;
    std::string sender = item.value("sender", "agent");
    message.sender = sender == "user" ? cli::agent::ChatMessage::Sender::kUser
                                      : cli::agent::ChatMessage::Sender::kAgent;
    message.message = item.value("message", "");
    message.timestamp = ParseTimestamp(item["timestamp"]);
    message.is_internal = item.value("is_internal", false);

    if (item.contains("json_pretty") && item["json_pretty"].is_string()) {
      message.json_pretty = item["json_pretty"].get<std::string>();
    }
    if (item.contains("table_data")) {
      message.table_data = ParseTableData(item["table_data"]);
    }
    if (item.contains("metrics") && item["metrics"].is_object()) {
      cli::agent::ChatMessage::SessionMetrics metrics;
      const auto& metrics_json = item["metrics"];
      metrics.turn_index = metrics_json.value("turn_index", 0);
      metrics.total_user_messages =
          metrics_json.value("total_user_messages", 0);
      metrics.total_agent_messages =
          metrics_json.value("total_agent_messages", 0);
      metrics.total_tool_calls = metrics_json.value("total_tool_calls", 0);
      metrics.total_commands = metrics_json.value("total_commands", 0);
      metrics.total_proposals = metrics_json.value("total_proposals", 0);
      metrics.total_elapsed_seconds =
          metrics_json.value("total_elapsed_seconds", 0.0);
      metrics.average_latency_seconds =
          metrics_json.value("average_latency_seconds", 0.0);
      message.metrics = metrics;
    }
    if (item.contains("proposal")) {
      message.proposal = ParseProposal(item["proposal"]);
    }
    if (item.contains("warnings") && item["warnings"].is_array()) {
      for (const auto& warning : item["warnings"]) {
        if (warning.is_string()) {
          message.warnings.push_back(warning.get<std::string>());
        }
      }
    }
    if (item.contains("model_metadata") && item["model_metadata"].is_object()) {
      const auto& meta_json = item["model_metadata"];
      cli::agent::ChatMessage::ModelMetadata meta;
      meta.provider = meta_json.value("provider", "");
      meta.model = meta_json.value("model", "");
      meta.latency_seconds = meta_json.value("latency_seconds", 0.0);
      meta.tool_iterations = meta_json.value("tool_iterations", 0);
      if (meta_json.contains("tool_names") &&
          meta_json["tool_names"].is_array()) {
        for (const auto& name : meta_json["tool_names"]) {
          if (name.is_string()) {
            meta.tool_names.push_back(name.get<std::string>());
          }
        }
      }
      if (meta_json.contains("parameters") &&
          meta_json["parameters"].is_object()) {
        for (const auto& [key, value] : meta_json["parameters"].items()) {
          if (value.is_string()) {
            meta.parameters[key] = value.get<std::string>();
          }
        }
      }
      message.model_metadata = meta;
    }

    snapshot.history.push_back(std::move(message));
  }

  if (json.contains("collaboration") && json["collaboration"].is_object()) {
    const auto& collab_json = json["collaboration"];
    snapshot.collaboration.active = collab_json.value("active", false);
    snapshot.collaboration.session_id = collab_json.value("session_id", "");
    snapshot.collaboration.session_name = collab_json.value("session_name", "");
    snapshot.collaboration.participants.clear();
    if (collab_json.contains("participants") &&
        collab_json["participants"].is_array()) {
      for (const auto& participant : collab_json["participants"]) {
        if (participant.is_string()) {
          snapshot.collaboration.participants.push_back(
              participant.get<std::string>());
        }
      }
    }
    if (collab_json.contains("last_synced")) {
      snapshot.collaboration.last_synced =
          ParseTimestamp(collab_json["last_synced"]);
    }
    if (snapshot.collaboration.session_name.empty() &&
        !snapshot.collaboration.session_id.empty()) {
      snapshot.collaboration.session_name = snapshot.collaboration.session_id;
    }
  }

  if (json.contains("multimodal") && json["multimodal"].is_object()) {
    const auto& multimodal_json = json["multimodal"];
    if (multimodal_json.contains("last_capture_path") &&
        multimodal_json["last_capture_path"].is_string()) {
      std::string path_value =
          multimodal_json["last_capture_path"].get<std::string>();
      if (!path_value.empty()) {
        snapshot.multimodal.last_capture_path =
            std::filesystem::path(path_value);
      }
    }
    snapshot.multimodal.status_message =
        multimodal_json.value("status_message", "");
    if (multimodal_json.contains("last_updated")) {
      snapshot.multimodal.last_updated =
          ParseTimestamp(multimodal_json["last_updated"]);
    }
  }

  if (json.contains("agent_config") && json["agent_config"].is_object()) {
    const auto& config_json = json["agent_config"];
    AgentConfigSnapshot config;
    config.provider = config_json.value("provider", "");
    config.model = config_json.value("model", "");
    config.ollama_host =
        config_json.value("ollama_host", "http://localhost:11434");
    config.gemini_api_key = config_json.value("gemini_api_key", "");
    config.verbose = config_json.value("verbose", false);
    config.show_reasoning = config_json.value("show_reasoning", true);
    config.max_tool_iterations = config_json.value("max_tool_iterations", 4);
    config.max_retry_attempts = config_json.value("max_retry_attempts", 3);
    config.temperature = config_json.value("temperature", 0.25f);
    config.top_p = config_json.value("top_p", 0.95f);
    config.max_output_tokens = config_json.value("max_output_tokens", 2048);
    config.stream_responses = config_json.value("stream_responses", false);
    config.chain_mode = config_json.value("chain_mode", 0);
    if (config_json.contains("favorite_models") &&
        config_json["favorite_models"].is_array()) {
      for (const auto& fav : config_json["favorite_models"]) {
        if (fav.is_string()) {
          config.favorite_models.push_back(fav.get<std::string>());
        }
      }
    }
    if (config_json.contains("model_chain") &&
        config_json["model_chain"].is_array()) {
      for (const auto& chain : config_json["model_chain"]) {
        if (chain.is_string()) {
          config.model_chain.push_back(chain.get<std::string>());
        }
      }
    }
    if (config_json.contains("goals") && config_json["goals"].is_array()) {
      for (const auto& goal : config_json["goals"]) {
        if (goal.is_string()) {
          config.goals.push_back(goal.get<std::string>());
        }
      }
    }
    if (config_json.contains("model_presets") &&
        config_json["model_presets"].is_array()) {
      for (const auto& preset_json : config_json["model_presets"]) {
        if (!preset_json.is_object())
          continue;
        AgentConfigSnapshot::ModelPreset preset;
        preset.name = preset_json.value("name", "");
        preset.model = preset_json.value("model", "");
        preset.host = preset_json.value("host", "");
        preset.pinned = preset_json.value("pinned", false);
        if (preset_json.contains("tags") && preset_json["tags"].is_array()) {
          for (const auto& tag : preset_json["tags"]) {
            if (tag.is_string()) {
              preset.tags.push_back(tag.get<std::string>());
            }
          }
        }
        config.model_presets.push_back(std::move(preset));
      }
    }
    if (config_json.contains("tools") && config_json["tools"].is_object()) {
      const auto& tools_json = config_json["tools"];
      config.tools.resources = tools_json.value("resources", true);
      config.tools.dungeon = tools_json.value("dungeon", true);
      config.tools.overworld = tools_json.value("overworld", true);
      config.tools.dialogue = tools_json.value("dialogue", true);
      config.tools.messages = tools_json.value("messages", true);
      config.tools.gui = tools_json.value("gui", true);
      config.tools.music = tools_json.value("music", true);
      config.tools.sprite = tools_json.value("sprite", true);
      config.tools.emulator = tools_json.value("emulator", true);
    }
    config.persona_notes = config_json.value("persona_notes", "");
    snapshot.agent_config = config;
  }

  return snapshot;
#else
  (void)path;
  return absl::UnimplementedError(
      "Chat history persistence requires YAZE_WITH_GRPC=ON");
#endif
}

absl::Status AgentChatHistoryCodec::Save(const std::filesystem::path& path,
                                         const Snapshot& snapshot) {
#if defined(YAZE_WITH_JSON)
  Json json;
  json["version"] = 4;
  json["messages"] = Json::array();

  for (const auto& message : snapshot.history) {
    Json entry;
    entry["sender"] = message.sender == cli::agent::ChatMessage::Sender::kUser
                          ? "user"
                          : "agent";
    entry["message"] = message.message;
    entry["timestamp"] = absl::FormatTime(absl::RFC3339_full, message.timestamp,
                                          absl::UTCTimeZone());
    entry["is_internal"] = message.is_internal;

    if (message.json_pretty.has_value()) {
      entry["json_pretty"] = *message.json_pretty;
    }
    if (message.table_data.has_value()) {
      entry["table_data"] = SerializeTableData(*message.table_data);
    }
    if (message.metrics.has_value()) {
      const auto& metrics = *message.metrics;
      Json metrics_json;
      metrics_json["turn_index"] = metrics.turn_index;
      metrics_json["total_user_messages"] = metrics.total_user_messages;
      metrics_json["total_agent_messages"] = metrics.total_agent_messages;
      metrics_json["total_tool_calls"] = metrics.total_tool_calls;
      metrics_json["total_commands"] = metrics.total_commands;
      metrics_json["total_proposals"] = metrics.total_proposals;
      metrics_json["total_elapsed_seconds"] = metrics.total_elapsed_seconds;
      metrics_json["average_latency_seconds"] = metrics.average_latency_seconds;
      entry["metrics"] = metrics_json;
    }
    if (message.proposal.has_value()) {
      entry["proposal"] = SerializeProposal(*message.proposal);
    }
    if (!message.warnings.empty()) {
      entry["warnings"] = message.warnings;
    }
    if (message.model_metadata.has_value()) {
      const auto& meta = *message.model_metadata;
      Json meta_json;
      meta_json["provider"] = meta.provider;
      meta_json["model"] = meta.model;
      meta_json["latency_seconds"] = meta.latency_seconds;
      meta_json["tool_iterations"] = meta.tool_iterations;
      meta_json["tool_names"] = meta.tool_names;
      Json params_json;
      for (const auto& [key, value] : meta.parameters) {
        params_json[key] = value;
      }
      meta_json["parameters"] = std::move(params_json);
      entry["model_metadata"] = std::move(meta_json);
    }

    json["messages"].push_back(std::move(entry));
  }

  Json collab_json;
  collab_json["active"] = snapshot.collaboration.active;
  collab_json["session_id"] = snapshot.collaboration.session_id;
  collab_json["session_name"] = snapshot.collaboration.session_name;
  collab_json["participants"] = snapshot.collaboration.participants;
  if (snapshot.collaboration.last_synced != absl::InfinitePast()) {
    collab_json["last_synced"] =
        absl::FormatTime(absl::RFC3339_full, snapshot.collaboration.last_synced,
                         absl::UTCTimeZone());
  }
  json["collaboration"] = std::move(collab_json);

  Json multimodal_json;
  if (snapshot.multimodal.last_capture_path.has_value()) {
    multimodal_json["last_capture_path"] =
        snapshot.multimodal.last_capture_path->string();
  } else {
    multimodal_json["last_capture_path"] = "";
  }
  multimodal_json["status_message"] = snapshot.multimodal.status_message;
  if (snapshot.multimodal.last_updated != absl::InfinitePast()) {
    multimodal_json["last_updated"] =
        absl::FormatTime(absl::RFC3339_full, snapshot.multimodal.last_updated,
                         absl::UTCTimeZone());
  }
  json["multimodal"] = std::move(multimodal_json);

  if (snapshot.agent_config.has_value()) {
    const auto& config = *snapshot.agent_config;
    Json config_json;
    config_json["provider"] = config.provider;
    config_json["model"] = config.model;
    config_json["ollama_host"] = config.ollama_host;
    config_json["gemini_api_key"] = config.gemini_api_key;
    config_json["verbose"] = config.verbose;
    config_json["show_reasoning"] = config.show_reasoning;
    config_json["max_tool_iterations"] = config.max_tool_iterations;
    config_json["max_retry_attempts"] = config.max_retry_attempts;
    config_json["temperature"] = config.temperature;
    config_json["top_p"] = config.top_p;
    config_json["max_output_tokens"] = config.max_output_tokens;
    config_json["stream_responses"] = config.stream_responses;
    config_json["chain_mode"] = config.chain_mode;
    config_json["favorite_models"] = config.favorite_models;
    config_json["model_chain"] = config.model_chain;
    config_json["persona_notes"] = config.persona_notes;
    config_json["goals"] = config.goals;

    Json tools_json;
    tools_json["resources"] = config.tools.resources;
    tools_json["dungeon"] = config.tools.dungeon;
    tools_json["overworld"] = config.tools.overworld;
    tools_json["dialogue"] = config.tools.dialogue;
    tools_json["messages"] = config.tools.messages;
    tools_json["gui"] = config.tools.gui;
    tools_json["music"] = config.tools.music;
    tools_json["sprite"] = config.tools.sprite;
    tools_json["emulator"] = config.tools.emulator;
    config_json["tools"] = std::move(tools_json);

    Json presets_json = Json::array();
    for (const auto& preset : config.model_presets) {
      Json preset_json;
      preset_json["name"] = preset.name;
      preset_json["model"] = preset.model;
      preset_json["host"] = preset.host;
      preset_json["tags"] = preset.tags;
      preset_json["pinned"] = preset.pinned;
      presets_json.push_back(std::move(preset_json));
    }
    config_json["model_presets"] = std::move(presets_json);

    json["agent_config"] = std::move(config_json);
  }

  std::error_code ec;
  auto directory = path.parent_path();
  if (!directory.empty()) {
    std::filesystem::create_directories(directory, ec);
    if (ec) {
      return absl::InternalError(absl::StrFormat(
          "Unable to create chat history directory: %s", ec.message()));
    }
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    return absl::InternalError("Cannot write chat history file");
  }

  file << json.dump(2);
  return absl::OkStatus();
#else
  (void)path;
  (void)snapshot;
  return absl::UnimplementedError(
      "Chat history persistence requires YAZE_WITH_GRPC=ON");
#endif
}

}  // namespace editor
}  // namespace yaze
