/**
 * @file agent_context.h
 * @brief Agent context for state preservation across tool calls
 *
 * Provides session state management including ROM context, pending edits,
 * call history, and cached data for efficient multi-turn conversations.
 */

#ifndef YAZE_SRC_CLI_SERVICE_AGENT_AGENT_CONTEXT_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_AGENT_CONTEXT_H_

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "rom/rom.h"
#include "cli/service/ai/common.h"

namespace yaze {
namespace cli {
namespace agent {

/**
 * @brief Record of a ROM edit operation
 */
struct RomEdit {
  uint32_t address;
  std::vector<uint8_t> old_value;
  std::vector<uint8_t> new_value;
  std::string description;
  std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Record of a tool call and its result
 */
struct ToolCallRecord {
  ToolCall call;
  std::string result;
  std::chrono::system_clock::time_point timestamp;
  double execution_time_ms;
  bool success;
};

/**
 * @brief Cached dungeon data for efficient access
 */
struct DungeonCache {
  int current_room_id = -1;
  std::vector<int> visited_rooms;
  std::map<int, std::string> room_descriptions;
  std::chrono::system_clock::time_point last_updated;

  bool IsValid() const {
    auto age = std::chrono::system_clock::now() - last_updated;
    return std::chrono::duration_cast<std::chrono::minutes>(age).count() < 5;
  }

  void Invalidate() {
    current_room_id = -1;
    visited_rooms.clear();
    room_descriptions.clear();
  }
};

/**
 * @brief Cached overworld data for efficient access
 */
struct OverworldCache {
  int current_map_id = -1;
  std::vector<int> visited_maps;
  std::map<int, std::string> map_descriptions;
  std::chrono::system_clock::time_point last_updated;

  bool IsValid() const {
    auto age = std::chrono::system_clock::now() - last_updated;
    return std::chrono::duration_cast<std::chrono::minutes>(age).count() < 5;
  }

  void Invalidate() {
    current_map_id = -1;
    visited_maps.clear();
    map_descriptions.clear();
  }
};

/**
 * @brief Agent context for maintaining state across tool calls
 *
 * Provides:
 * - ROM state management
 * - Pending edit tracking
 * - Tool call history
 * - Session variables
 * - Caching for frequently accessed data
 */
class AgentContext {
 public:
  AgentContext() = default;

  // =========================================================================
  // ROM State Management
  // =========================================================================

  /**
   * @brief Set the current ROM context
   */
  void SetRom(Rom* rom) {
    current_rom_ = rom;
    InvalidateCaches();
  }

  /**
   * @brief Get the current ROM context
   */
  Rom* GetRom() const { return current_rom_; }

  /**
   * @brief Check if a ROM is loaded
   */
  bool HasRom() const { return current_rom_ != nullptr; }

  /**
   * @brief Get the current ROM path
   */
  const std::string& GetRomPath() const { return rom_path_; }

  /**
   * @brief Set the current ROM path
   */
  void SetRomPath(const std::string& path) { rom_path_ = path; }

  // =========================================================================
  // Edit Management
  // =========================================================================

  /**
   * @brief Record a pending ROM edit
   */
  void AddPendingEdit(const RomEdit& edit) { pending_edits_.push_back(edit); }

  /**
   * @brief Get all pending edits
   */
  const std::vector<RomEdit>& GetPendingEdits() const { return pending_edits_; }

  /**
   * @brief Check if there are pending edits
   */
  bool HasPendingEdits() const { return !pending_edits_.empty(); }

  /**
   * @brief Clear all pending edits
   */
  void ClearPendingEdits() { pending_edits_.clear(); }

  /**
   * @brief Commit all pending edits to the ROM
   */
  absl::Status CommitPendingEdits() {
    if (!current_rom_) {
      return absl::FailedPreconditionError("No ROM loaded");
    }

    for (const auto& edit : pending_edits_) {
      for (size_t i = 0; i < edit.new_value.size(); ++i) {
        current_rom_->WriteByte(edit.address + i, edit.new_value[i]);
      }
    }

    pending_edits_.clear();
    InvalidateCaches();
    return absl::OkStatus();
  }

  /**
   * @brief Rollback all pending edits
   */
  void RollbackPendingEdits() {
    // Pending edits haven't been applied yet, just clear them
    pending_edits_.clear();
  }

  // =========================================================================
  // Tool Call History
  // =========================================================================

  /**
   * @brief Record a tool call
   */
  void RecordToolCall(const ToolCallRecord& record) {
    call_history_.push_back(record);

    // Keep history bounded
    if (call_history_.size() > max_history_size_) {
      call_history_.erase(call_history_.begin());
    }
  }

  /**
   * @brief Get tool call history
   */
  const std::vector<ToolCallRecord>& GetCallHistory() const {
    return call_history_;
  }

  /**
   * @brief Get the last N tool calls
   */
  std::vector<ToolCallRecord> GetRecentCalls(size_t n) const {
    if (n >= call_history_.size()) {
      return call_history_;
    }
    return std::vector<ToolCallRecord>(call_history_.end() - n,
                                       call_history_.end());
  }

  /**
   * @brief Clear tool call history
   */
  void ClearCallHistory() { call_history_.clear(); }

  // =========================================================================
  // Session Variables
  // =========================================================================

  /**
   * @brief Set a session variable
   */
  void SetVariable(const std::string& key, const std::string& value) {
    session_vars_[key] = value;
  }

  /**
   * @brief Get a session variable
   */
  std::optional<std::string> GetVariable(const std::string& key) const {
    auto it = session_vars_.find(key);
    if (it != session_vars_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  /**
   * @brief Check if a session variable exists
   */
  bool HasVariable(const std::string& key) const {
    return session_vars_.find(key) != session_vars_.end();
  }

  /**
   * @brief Clear all session variables
   */
  void ClearVariables() { session_vars_.clear(); }

  /**
   * @brief Get all session variables
   */
  const std::map<std::string, std::string>& GetAllVariables() const {
    return session_vars_;
  }

  // =========================================================================
  // Caching
  // =========================================================================

  /**
   * @brief Get dungeon cache
   */
  DungeonCache& GetDungeonCache() { return dungeon_cache_; }
  const DungeonCache& GetDungeonCache() const { return dungeon_cache_; }

  /**
   * @brief Get overworld cache
   */
  OverworldCache& GetOverworldCache() { return overworld_cache_; }
  const OverworldCache& GetOverworldCache() const { return overworld_cache_; }

  /**
   * @brief Invalidate all caches
   */
  void InvalidateCaches() {
    dungeon_cache_.Invalidate();
    overworld_cache_.Invalidate();
  }

  // =========================================================================
  // Session Management
  // =========================================================================

  /**
   * @brief Get session ID
   */
  const std::string& GetSessionId() const { return session_id_; }

  /**
   * @brief Set session ID
   */
  void SetSessionId(const std::string& id) { session_id_ = id; }

  /**
   * @brief Get session start time
   */
  std::chrono::system_clock::time_point GetSessionStartTime() const {
    return session_start_time_;
  }

  /**
   * @brief Reset the session (clear all state except ROM)
   */
  void ResetSession() {
    pending_edits_.clear();
    call_history_.clear();
    session_vars_.clear();
    InvalidateCaches();
    session_start_time_ = std::chrono::system_clock::now();
  }

  /**
   * @brief Generate a summary of the current context
   */
  std::string GenerateSummary() const {
    std::ostringstream ss;
    ss << "Agent Context Summary:\n";
    ss << "  ROM loaded: " << (current_rom_ ? "Yes" : "No") << "\n";
    if (current_rom_) {
      ss << "  ROM path: " << rom_path_ << "\n";
    }
    ss << "  Pending edits: " << pending_edits_.size() << "\n";
    ss << "  Tool calls in history: " << call_history_.size() << "\n";
    ss << "  Session variables: " << session_vars_.size() << "\n";
    ss << "  Dungeon cache valid: "
       << (dungeon_cache_.IsValid() ? "Yes" : "No") << "\n";
    ss << "  Overworld cache valid: "
       << (overworld_cache_.IsValid() ? "Yes" : "No") << "\n";
    return ss.str();
  }

 private:
  // ROM state
  Rom* current_rom_ = nullptr;
  std::string rom_path_;
  std::vector<RomEdit> pending_edits_;

  // Session state
  std::vector<ToolCallRecord> call_history_;
  std::map<std::string, std::string> session_vars_;
  std::string session_id_;
  std::chrono::system_clock::time_point session_start_time_ =
      std::chrono::system_clock::now();

  // Caches
  DungeonCache dungeon_cache_;
  OverworldCache overworld_cache_;

  // Configuration
  size_t max_history_size_ = 100;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_AGENT_CONTEXT_H_

