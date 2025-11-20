#ifndef YAZE_CLI_SERVICE_AGENT_LEARNED_KNOWLEDGE_SERVICE_H_
#define YAZE_CLI_SERVICE_AGENT_LEARNED_KNOWLEDGE_SERVICE_H_

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

// Note: This header is JSON-independent for broader compatibility
// JSON is only used in the .cc implementation file

namespace yaze {
namespace cli {
namespace agent {

/**
 * @class LearnedKnowledgeService
 * @brief Manages persistent learned information across agent sessions
 * 
 * Stores:
 * - User preferences (default palettes, favorite tools, etc.)
 * - ROM patterns (frequently accessed rooms, sprite placements, etc.)
 * - Project context (ROM-specific notes, goals, custom names)
 * - Conversation memory (summaries of past discussions)
 */
class LearnedKnowledgeService {
 public:
  LearnedKnowledgeService();
  explicit LearnedKnowledgeService(const std::filesystem::path& data_dir);

  // Initialize the service and load existing data
  absl::Status Initialize();

  // Save all data to disk
  absl::Status SaveAll();

  // === Preference Management ===

  /**
   * Set a user preference
   * @param key Preference key (e.g., "default_palette", "preferred_tool")
   * @param value Preference value
   */
  absl::Status SetPreference(const std::string& key, const std::string& value);

  /**
   * Get a user preference
   * @param key Preference key
   * @return Value if exists, nullopt otherwise
   */
  std::optional<std::string> GetPreference(const std::string& key) const;

  /**
   * List all preferences
   */
  std::map<std::string, std::string> GetAllPreferences() const;

  /**
   * Remove a preference
   */
  absl::Status RemovePreference(const std::string& key);

  // === ROM Pattern Learning ===

  struct ROMPattern {
    std::string pattern_type;  // e.g., "sprite_location", "tile_usage"
    std::string rom_hash;
    std::string pattern_data;  // JSON encoded
    float confidence = 1.0f;   // 0.0 to 1.0
    int64_t learned_at = 0;
    int access_count = 0;
  };

  /**
   * Learn a pattern from the current ROM
   * @param type Pattern type (e.g., "sprite_distribution", "room_access_frequency")
   * @param rom_hash SHA256 hash of the ROM
   * @param data Pattern-specific data (JSON)
   */
  absl::Status LearnPattern(const std::string& type,
                            const std::string& rom_hash,
                            const std::string& data, float confidence = 1.0f);

  /**
   * Query patterns for a specific ROM
   * @param type Pattern type to query
   * @param rom_hash ROM hash to filter by (empty = all ROMs)
   * @return Vector of matching patterns
   */
  std::vector<ROMPattern> QueryPatterns(const std::string& type,
                                        const std::string& rom_hash = "") const;

  /**
   * Update pattern confidence/access count
   */
  absl::Status UpdatePatternConfidence(const std::string& type,
                                       const std::string& rom_hash,
                                       float new_confidence);

  // === Project Context ===

  struct ProjectContext {
    std::string project_name;
    std::string rom_hash;
    std::string
        context_data;  // JSON encoded: description, goals, custom labels
    int64_t last_accessed = 0;
  };

  /**
   * Save context for a project/ROM
   */
  absl::Status SaveProjectContext(const std::string& project_name,
                                  const std::string& rom_hash,
                                  const std::string& context);

  /**
   * Get project context
   */
  std::optional<ProjectContext> GetProjectContext(
      const std::string& project_name) const;

  /**
   * List all projects
   */
  std::vector<ProjectContext> GetAllProjects() const;

  // === Conversation Memory ===

  struct ConversationMemory {
    std::string id;
    std::string topic;
    std::string summary;
    std::vector<std::string> key_facts;
    int64_t created_at = 0;
    int access_count = 0;
  };

  /**
   * Store a conversation summary
   * @param topic Topic/theme of the conversation
   * @param summary Brief summary
   * @param key_facts Important facts extracted
   */
  absl::Status StoreConversationSummary(
      const std::string& topic, const std::string& summary,
      const std::vector<std::string>& key_facts);

  /**
   * Search conversation memories by topic/keyword
   */
  std::vector<ConversationMemory> SearchMemories(
      const std::string& query) const;

  /**
   * Get most recent conversation memories
   */
  std::vector<ConversationMemory> GetRecentMemories(int limit = 10) const;

  // === Import/Export ===

  /**
   * Export all learned data to JSON
   */
  absl::StatusOr<std::string> ExportToJSON() const;

  /**
   * Import learned data from JSON
   */
  absl::Status ImportFromJSON(const std::string& json_data);

  /**
   * Clear all learned data
   */
  absl::Status ClearAll();

  // === Statistics ===

  struct Stats {
    int preference_count = 0;
    int pattern_count = 0;
    int project_count = 0;
    int memory_count = 0;
    int64_t first_learned_at = 0;
    int64_t last_updated_at = 0;
  };

  Stats GetStats() const;

 private:
  std::filesystem::path data_dir_;
  std::filesystem::path prefs_file_;
  std::filesystem::path patterns_file_;
  std::filesystem::path projects_file_;
  std::filesystem::path memories_file_;

  std::map<std::string, std::string> preferences_;
  std::vector<ROMPattern> patterns_;
  std::vector<ProjectContext> projects_;
  std::vector<ConversationMemory> memories_;

  bool initialized_ = false;

  // Internal helpers
  absl::Status LoadPreferences();
  absl::Status LoadPatterns();
  absl::Status LoadProjects();
  absl::Status LoadMemories();

  absl::Status SavePreferences();
  absl::Status SavePatterns();
  absl::Status SaveProjects();
  absl::Status SaveMemories();

  std::string GenerateID() const;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AGENT_LEARNED_KNOWLEDGE_SERVICE_H_
