#include "cli/service/agent/learned_knowledge_service.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "util/platform_paths.h"

#include "nlohmann/json.hpp"

namespace yaze {
namespace cli {
namespace agent {

namespace {

int64_t CurrentTimestamp() {
  return absl::ToUnixMillis(absl::Now());
}

std::string GenerateRandomID() {
  static int counter = 0;
  auto now = std::chrono::system_clock::now().time_since_epoch().count();
  return absl::StrFormat("%lld_%d", now, counter++);
}

bool FileExists(const std::filesystem::path& path) {
  return util::PlatformPaths::Exists(path);
}

}  // namespace

LearnedKnowledgeService::LearnedKnowledgeService() {
  // Get app data directory in a cross-platform way
  auto app_data_result = util::PlatformPaths::GetAppDataSubdirectory("agent");
  if (app_data_result.ok()) {
    data_dir_ = *app_data_result;
  } else {
    // Fallback to current directory
    data_dir_ = std::filesystem::current_path() / ".yaze" / "agent";
  }
  
  prefs_file_ = data_dir_ / "preferences.json";
  patterns_file_ = data_dir_ / "patterns.json";
  projects_file_ = data_dir_ / "projects.json";
  memories_file_ = data_dir_ / "memories.json";
}

LearnedKnowledgeService::LearnedKnowledgeService(
    const std::filesystem::path& data_dir)
    : data_dir_(data_dir),
      prefs_file_(data_dir / "preferences.json"),
      patterns_file_(data_dir / "patterns.json"),
      projects_file_(data_dir / "projects.json"),
      memories_file_(data_dir / "memories.json") {}

absl::Status LearnedKnowledgeService::Initialize() {
  if (initialized_) {
    return absl::OkStatus();
  }
  
  // Ensure data directory exists
  auto status = util::PlatformPaths::EnsureDirectoryExists(data_dir_);
  if (!status.ok()) {
    return status;
  }
  
  // Load existing data
  LoadPreferences();  // Ignore errors for empty files
  LoadPatterns();
  LoadProjects();
  LoadMemories();
  
  initialized_ = true;
  return absl::OkStatus();
}

absl::Status LearnedKnowledgeService::SaveAll() {
  auto status = SavePreferences();
  if (!status.ok()) return status;
  
  status = SavePatterns();
  if (!status.ok()) return status;
  
  status = SaveProjects();
  if (!status.ok()) return status;
  
  status = SaveMemories();
  if (!status.ok()) return status;
  
  return absl::OkStatus();
}

// === Preference Management ===

absl::Status LearnedKnowledgeService::SetPreference(const std::string& key,
                                                    const std::string& value) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Service not initialized");
  }
  
  preferences_[key] = value;
  return SavePreferences();
}

std::optional<std::string> LearnedKnowledgeService::GetPreference(
    const std::string& key) const {
  auto it = preferences_.find(key);
  if (it != preferences_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::map<std::string, std::string> LearnedKnowledgeService::GetAllPreferences() const {
  return preferences_;
}

absl::Status LearnedKnowledgeService::RemovePreference(const std::string& key) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Service not initialized");
  }
  
  preferences_.erase(key);
  return SavePreferences();
}

// === ROM Pattern Learning ===

absl::Status LearnedKnowledgeService::LearnPattern(const std::string& type,
                                                   const std::string& rom_hash,
                                                   const std::string& data,
                                                   float confidence) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Service not initialized");
  }
  
  ROMPattern pattern;
  pattern.pattern_type = type;
  pattern.rom_hash = rom_hash;
  pattern.pattern_data = data;
  pattern.confidence = confidence;
  pattern.learned_at = CurrentTimestamp();
  pattern.access_count = 1;
  
  patterns_.push_back(pattern);
  return SavePatterns();
}

std::vector<LearnedKnowledgeService::ROMPattern>
LearnedKnowledgeService::QueryPatterns(const std::string& type,
                                      const std::string& rom_hash) const {
  std::vector<ROMPattern> results;
  
  for (const auto& pattern : patterns_) {
    bool type_match = type.empty() || pattern.pattern_type == type;
    bool hash_match = rom_hash.empty() || pattern.rom_hash == rom_hash;
    
    if (type_match && hash_match) {
      results.push_back(pattern);
    }
  }
  
  return results;
}

absl::Status LearnedKnowledgeService::UpdatePatternConfidence(
    const std::string& type,
    const std::string& rom_hash,
    float new_confidence) {
  bool found = false;
  
  for (auto& pattern : patterns_) {
    if (pattern.pattern_type == type && pattern.rom_hash == rom_hash) {
      pattern.confidence = new_confidence;
      pattern.access_count++;
      found = true;
    }
  }
  
  if (!found) {
    return absl::NotFoundError("Pattern not found");
  }
  
  return SavePatterns();
}

// === Project Context ===

absl::Status LearnedKnowledgeService::SaveProjectContext(
    const std::string& project_name,
    const std::string& rom_hash,
    const std::string& context) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Service not initialized");
  }
  
  // Update existing or create new
  bool found = false;
  for (auto& project : projects_) {
    if (project.project_name == project_name) {
      project.rom_hash = rom_hash;
      project.context_data = context;
      project.last_accessed = CurrentTimestamp();
      found = true;
      break;
    }
  }
  
  if (!found) {
    ProjectContext project;
    project.project_name = project_name;
    project.rom_hash = rom_hash;
    project.context_data = context;
    project.last_accessed = CurrentTimestamp();
    projects_.push_back(project);
  }
  
  return SaveProjects();
}

std::optional<LearnedKnowledgeService::ProjectContext>
LearnedKnowledgeService::GetProjectContext(const std::string& project_name) const {
  for (const auto& project : projects_) {
    if (project.project_name == project_name) {
      return project;
    }
  }
  return std::nullopt;
}

std::vector<LearnedKnowledgeService::ProjectContext>
LearnedKnowledgeService::GetAllProjects() const {
  return projects_;
}

// === Conversation Memory ===

absl::Status LearnedKnowledgeService::StoreConversationSummary(
    const std::string& topic,
    const std::string& summary,
    const std::vector<std::string>& key_facts) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Service not initialized");
  }
  
  ConversationMemory memory;
  memory.id = GenerateRandomID();
  memory.topic = topic;
  memory.summary = summary;
  memory.key_facts = key_facts;
  memory.created_at = CurrentTimestamp();
  memory.access_count = 1;
  
  memories_.push_back(memory);
  
  // Keep only last 100 memories
  if (memories_.size() > 100) {
    memories_.erase(memories_.begin());
  }
  
  return SaveMemories();
}

std::vector<LearnedKnowledgeService::ConversationMemory>
LearnedKnowledgeService::SearchMemories(const std::string& query) const {
  std::vector<ConversationMemory> results;
  
  std::string query_lower = query;
  std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
  
  for (const auto& memory : memories_) {
    std::string topic_lower = memory.topic;
    std::string summary_lower = memory.summary;
    std::transform(topic_lower.begin(), topic_lower.end(), topic_lower.begin(), ::tolower);
    std::transform(summary_lower.begin(), summary_lower.end(), summary_lower.begin(), ::tolower);
    
    if (topic_lower.find(query_lower) != std::string::npos ||
        summary_lower.find(query_lower) != std::string::npos) {
      results.push_back(memory);
    }
  }
  
  return results;
}

std::vector<LearnedKnowledgeService::ConversationMemory>
LearnedKnowledgeService::GetRecentMemories(int limit) const {
  std::vector<ConversationMemory> recent = memories_;
  
  // Sort by created_at descending
  std::sort(recent.begin(), recent.end(),
           [](const ConversationMemory& a, const ConversationMemory& b) {
             return a.created_at > b.created_at;
           });
  
  if (recent.size() > static_cast<size_t>(limit)) {
    recent.resize(limit);
  }
  
  return recent;
}

// === Import/Export ===

#ifdef YAZE_WITH_JSON
absl::StatusOr<std::string> LearnedKnowledgeService::ExportToJSON() const {
  nlohmann::json export_data;
  
  // Export preferences
  export_data["preferences"] = preferences_;
  
  // Export patterns
  export_data["patterns"] = nlohmann::json::array();
  for (const auto& pattern : patterns_) {
    nlohmann::json p;
    p["type"] = pattern.pattern_type;
    p["rom_hash"] = pattern.rom_hash;
    p["data"] = pattern.pattern_data;
    p["confidence"] = pattern.confidence;
    p["learned_at"] = pattern.learned_at;
    p["access_count"] = pattern.access_count;
    export_data["patterns"].push_back(p);
  }
  
  // Export projects
  export_data["projects"] = nlohmann::json::array();
  for (const auto& project : projects_) {
    nlohmann::json p;
    p["name"] = project.project_name;
    p["rom_hash"] = project.rom_hash;
    p["context"] = project.context_data;
    p["last_accessed"] = project.last_accessed;
    export_data["projects"].push_back(p);
  }
  
  // Export memories
  export_data["memories"] = nlohmann::json::array();
  for (const auto& memory : memories_) {
    nlohmann::json m;
    m["id"] = memory.id;
    m["topic"] = memory.topic;
    m["summary"] = memory.summary;
    m["key_facts"] = memory.key_facts;
    m["created_at"] = memory.created_at;
    m["access_count"] = memory.access_count;
    export_data["memories"].push_back(m);
  }
  
  return export_data.dump(2);
}

absl::Status LearnedKnowledgeService::ImportFromJSON(const std::string& json_data) {
  try {
    auto data = nlohmann::json::parse(json_data);
    
    // Import preferences
    if (data.contains("preferences")) {
      for (const auto& [key, value] : data["preferences"].items()) {
        preferences_[key] = value.get<std::string>();
      }
    }
    
    // Import patterns
    if (data.contains("patterns")) {
      for (const auto& p : data["patterns"]) {
        ROMPattern pattern;
        pattern.pattern_type = p["type"];
        pattern.rom_hash = p["rom_hash"];
        pattern.pattern_data = p["data"];
        pattern.confidence = p["confidence"];
        pattern.learned_at = p["learned_at"];
        pattern.access_count = p["access_count"];
        patterns_.push_back(pattern);
      }
    }
    
    // Import projects
    if (data.contains("projects")) {
      for (const auto& p : data["projects"]) {
        ProjectContext project;
        project.project_name = p["name"];
        project.rom_hash = p["rom_hash"];
        project.context_data = p["context"];
        project.last_accessed = p["last_accessed"];
        projects_.push_back(project);
      }
    }
    
    // Import memories
    if (data.contains("memories")) {
      for (const auto& m : data["memories"]) {
        ConversationMemory memory;
        memory.id = m["id"];
        memory.topic = m["topic"];
        memory.summary = m["summary"];
        memory.key_facts = m["key_facts"].get<std::vector<std::string>>();
        memory.created_at = m["created_at"];
        memory.access_count = m["access_count"];
        memories_.push_back(memory);
      }
    }
    
    return SaveAll();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("JSON parse error: ", e.what()));
  }
}
#else
absl::StatusOr<std::string> LearnedKnowledgeService::ExportToJSON() const {
  return absl::UnimplementedError("JSON support not enabled. Build with -DYAZE_WITH_JSON=ON");
}

absl::Status LearnedKnowledgeService::ImportFromJSON(const std::string&) {
  return absl::UnimplementedError("JSON support not enabled. Build with -DYAZE_WITH_JSON=ON");
}
#endif

absl::Status LearnedKnowledgeService::ClearAll() {
  preferences_.clear();
  patterns_.clear();
  projects_.clear();
  memories_.clear();
  return SaveAll();
}

// === Statistics ===

LearnedKnowledgeService::Stats LearnedKnowledgeService::GetStats() const {
  Stats stats;
  stats.preference_count = preferences_.size();
  stats.pattern_count = patterns_.size();
  stats.project_count = projects_.size();
  stats.memory_count = memories_.size();
  
  // Find earliest learned_at
  int64_t earliest = CurrentTimestamp();
  for (const auto& pattern : patterns_) {
    if (pattern.learned_at < earliest) {
      earliest = pattern.learned_at;
    }
  }
  for (const auto& memory : memories_) {
    if (memory.created_at < earliest) {
      earliest = memory.created_at;
    }
  }
  stats.first_learned_at = earliest;
  
  // Last updated is now
  stats.last_updated_at = CurrentTimestamp();
  
  return stats;
}

// === Internal Helpers ===

#ifdef YAZE_WITH_JSON
absl::Status LearnedKnowledgeService::LoadPreferences() {
  if (!FileExists(prefs_file_)) {
    return absl::OkStatus();  // No file yet, empty preferences
  }
  
  try {
    std::ifstream file(prefs_file_);
    if (!file.is_open()) {
      return absl::InternalError("Failed to open preferences file");
    }
    
    nlohmann::json data;
    file >> data;
    
    for (const auto& [key, value] : data.items()) {
      preferences_[key] = value.get<std::string>();
    }
    
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("Failed to load preferences: ", e.what()));
  }
}

absl::Status LearnedKnowledgeService::SavePreferences() {
  try {
    nlohmann::json data = preferences_;
    
    std::ofstream file(prefs_file_);
    if (!file.is_open()) {
      return absl::InternalError("Failed to open preferences file for writing");
    }
    
    file << data.dump(2);
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("Failed to save preferences: ", e.what()));
  }
}

absl::Status LearnedKnowledgeService::LoadPatterns() {
  if (!FileExists(patterns_file_)) {
    return absl::OkStatus();
  }
  
  try {
    std::ifstream file(patterns_file_);
    nlohmann::json data;
    file >> data;
    
    for (const auto& p : data) {
      ROMPattern pattern;
      pattern.pattern_type = p["type"];
      pattern.rom_hash = p["rom_hash"];
      pattern.pattern_data = p["data"];
      pattern.confidence = p["confidence"];
      pattern.learned_at = p["learned_at"];
      pattern.access_count = p.value("access_count", 0);
      patterns_.push_back(pattern);
    }
    
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("Failed to load patterns: ", e.what()));
  }
}

absl::Status LearnedKnowledgeService::SavePatterns() {
  try {
    nlohmann::json data = nlohmann::json::array();
    
    for (const auto& pattern : patterns_) {
      nlohmann::json p;
      p["type"] = pattern.pattern_type;
      p["rom_hash"] = pattern.rom_hash;
      p["data"] = pattern.pattern_data;
      p["confidence"] = pattern.confidence;
      p["learned_at"] = pattern.learned_at;
      p["access_count"] = pattern.access_count;
      data.push_back(p);
    }
    
    std::ofstream file(patterns_file_);
    file << data.dump(2);
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("Failed to save patterns: ", e.what()));
  }
}

absl::Status LearnedKnowledgeService::LoadProjects() {
  if (!FileExists(projects_file_)) {
    return absl::OkStatus();
  }
  
  try {
    std::ifstream file(projects_file_);
    nlohmann::json data;
    file >> data;
    
    for (const auto& p : data) {
      ProjectContext project;
      project.project_name = p["name"];
      project.rom_hash = p["rom_hash"];
      project.context_data = p["context"];
      project.last_accessed = p["last_accessed"];
      projects_.push_back(project);
    }
    
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("Failed to load projects: ", e.what()));
  }
}

absl::Status LearnedKnowledgeService::SaveProjects() {
  try {
    nlohmann::json data = nlohmann::json::array();
    
    for (const auto& project : projects_) {
      nlohmann::json p;
      p["name"] = project.project_name;
      p["rom_hash"] = project.rom_hash;
      p["context"] = project.context_data;
      p["last_accessed"] = project.last_accessed;
      data.push_back(p);
    }
    
    std::ofstream file(projects_file_);
    file << data.dump(2);
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("Failed to save projects: ", e.what()));
  }
}

absl::Status LearnedKnowledgeService::SaveMemories() {
  try {
    nlohmann::json data = nlohmann::json::array();
    
    for (const auto& memory : memories_) {
      nlohmann::json m;
      m["id"] = memory.id;
      m["topic"] = memory.topic;
      m["summary"] = memory.summary;
      m["key_facts"] = memory.key_facts;
      m["created_at"] = memory.created_at;
      m["access_count"] = memory.access_count;
      data.push_back(m);
    }
    
    std::ofstream file(memories_file_);
    file << data.dump(2);
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("Failed to save memories: ", e.what()));
  }
}

absl::Status LearnedKnowledgeService::LoadMemories() {
  if (!FileExists(memories_file_)) {
    return absl::OkStatus();
  }
  
  try {
    std::ifstream file(memories_file_);
    nlohmann::json data;
    file >> data;
    
    for (const auto& m : data) {
      ConversationMemory memory;
      memory.id = m["id"];
      memory.topic = m["topic"];
      memory.summary = m["summary"];
      memory.key_facts = m["key_facts"].get<std::vector<std::string>>();
      memory.created_at = m["created_at"];
      memory.access_count = m.value("access_count", 0);
      memories_.push_back(memory);
    }
    
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("Failed to load memories: ", e.what()));
  }
}

#else
// Stub implementations when JSON is not available
absl::Status LearnedKnowledgeService::LoadPreferences() { return absl::OkStatus(); }
absl::Status LearnedKnowledgeService::SavePreferences() { return absl::UnimplementedError("JSON support required"); }
absl::Status LearnedKnowledgeService::LoadPatterns() { return absl::OkStatus(); }
absl::Status LearnedKnowledgeService::SavePatterns() { return absl::UnimplementedError("JSON support required"); }
absl::Status LearnedKnowledgeService::LoadProjects() { return absl::OkStatus(); }
absl::Status LearnedKnowledgeService::SaveProjects() { return absl::UnimplementedError("JSON support required"); }
absl::Status LearnedKnowledgeService::LoadMemories() { return absl::OkStatus(); }
absl::Status LearnedKnowledgeService::SaveMemories() { return absl::UnimplementedError("JSON support required"); }
#endif

std::string LearnedKnowledgeService::GenerateID() const {
  return GenerateRandomID();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
