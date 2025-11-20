#include "app/editor/system/command_palette.h"

#include <algorithm>
#include <cctype>
#include <chrono>

namespace yaze {
namespace editor {

void CommandPalette::AddCommand(const std::string& name,
                                const std::string& category,
                                const std::string& description,
                                const std::string& shortcut,
                                std::function<void()> callback) {
  CommandEntry entry;
  entry.name = name;
  entry.category = category;
  entry.description = description;
  entry.shortcut = shortcut;
  entry.callback = callback;
  commands_[name] = entry;
}

void CommandPalette::RecordUsage(const std::string& name) {
  auto it = commands_.find(name);
  if (it != commands_.end()) {
    it->second.usage_count++;
    it->second.last_used_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
  }
}

int CommandPalette::FuzzyScore(const std::string& text,
                               const std::string& query) {
  if (query.empty()) return 0;

  int score = 0;
  size_t text_idx = 0;
  size_t query_idx = 0;

  std::string text_lower = text;
  std::string query_lower = query;
  std::transform(text_lower.begin(), text_lower.end(), text_lower.begin(),
                 ::tolower);
  std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                 ::tolower);

  // Exact match bonus
  if (text_lower == query_lower) return 1000;

  // Starts with bonus
  if (text_lower.find(query_lower) == 0) return 500;

  // Contains bonus
  if (text_lower.find(query_lower) != std::string::npos) return 250;

  // Fuzzy match - characters in order
  while (text_idx < text_lower.length() && query_idx < query_lower.length()) {
    if (text_lower[text_idx] == query_lower[query_idx]) {
      score += 10;
      query_idx++;
    }
    text_idx++;
  }

  // Penalty if not all characters matched
  if (query_idx != query_lower.length()) return 0;

  return score;
}

std::vector<CommandEntry> CommandPalette::SearchCommands(
    const std::string& query) {
  std::vector<std::pair<int, CommandEntry>> scored;

  for (const auto& [name, entry] : commands_) {
    int score = FuzzyScore(entry.name, query);

    // Also check category and description
    score += FuzzyScore(entry.category, query) / 2;
    score += FuzzyScore(entry.description, query) / 4;

    // Frecency bonus (frequency + recency)
    score += entry.usage_count * 2;

    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    int64_t age_ms = now_ms - entry.last_used_ms;
    if (age_ms < 60000) {  // Used in last minute
      score += 50;
    } else if (age_ms < 3600000) {  // Last hour
      score += 25;
    }

    if (score > 0) {
      scored.push_back({score, entry});
    }
  }

  // Sort by score descending
  std::sort(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  std::vector<CommandEntry> results;
  for (const auto& [score, entry] : scored) {
    results.push_back(entry);
  }

  return results;
}

std::vector<CommandEntry> CommandPalette::GetRecentCommands(int limit) {
  std::vector<CommandEntry> recent;

  for (const auto& [name, entry] : commands_) {
    if (entry.usage_count > 0) {
      recent.push_back(entry);
    }
  }

  std::sort(recent.begin(), recent.end(),
            [](const CommandEntry& a, const CommandEntry& b) {
              return a.last_used_ms > b.last_used_ms;
            });

  if (recent.size() > static_cast<size_t>(limit)) {
    recent.resize(limit);
  }

  return recent;
}

std::vector<CommandEntry> CommandPalette::GetFrequentCommands(int limit) {
  std::vector<CommandEntry> frequent;

  for (const auto& [name, entry] : commands_) {
    if (entry.usage_count > 0) {
      frequent.push_back(entry);
    }
  }

  std::sort(frequent.begin(), frequent.end(),
            [](const CommandEntry& a, const CommandEntry& b) {
              return a.usage_count > b.usage_count;
            });

  if (frequent.size() > static_cast<size_t>(limit)) {
    frequent.resize(limit);
  }

  return frequent;
}

void CommandPalette::SaveHistory(const std::string& filepath) {
  // TODO: Implement JSON serialization of command history
}

void CommandPalette::LoadHistory(const std::string& filepath) {
  // TODO: Implement JSON deserialization of command history
}

}  // namespace editor
}  // namespace yaze
