#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_editor.h"
#include "util/platform_paths.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace editor {

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
    builder_state_.tools.memory_inspector =
        tools.value("memory_inspector", false);
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
  (void)profile;
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
  (void)json_str;
  return absl::UnimplementedError("JSON support required");
#endif
}

}  // namespace editor
}  // namespace yaze
