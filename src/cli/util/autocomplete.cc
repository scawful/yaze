#include "cli/util/autocomplete.h"

#include <algorithm>
#include <cctype>

namespace yaze {
namespace cli {

void AutocompleteEngine::RegisterCommand(
    const std::string& cmd, const std::string& desc,
    const std::vector<std::string>& examples) {
  CommandDef def;
  def.name = cmd;
  def.description = desc;
  def.examples = examples;
  commands_.push_back(def);
}

void AutocompleteEngine::RegisterParameter(
    const std::string& param, const std::string& desc,
    const std::vector<std::string>& values) {
  // TODO: Store parameter definitions
}

int AutocompleteEngine::FuzzyScore(const std::string& text,
                                   const std::string& query) {
  if (query.empty()) return 0;

  std::string t = text, q = query;
  std::transform(t.begin(), t.end(), t.begin(), ::tolower);
  std::transform(q.begin(), q.end(), q.begin(), ::tolower);

  if (t == q) return 1000;
  if (t.find(q) == 0) return 500;
  if (t.find(q) != std::string::npos) return 250;

  // Fuzzy char matching
  size_t ti = 0, qi = 0;
  int score = 0;
  while (ti < t.length() && qi < q.length()) {
    if (t[ti] == q[qi]) {
      score += 10;
      qi++;
    }
    ti++;
  }

  return (qi == q.length()) ? score : 0;
}

std::vector<Suggestion> AutocompleteEngine::GetSuggestions(
    const std::string& input) {
  std::vector<Suggestion> results;

  for (const auto& cmd : commands_) {
    int score = FuzzyScore(cmd.name, input);
    if (score > 0) {
      Suggestion s;
      s.text = cmd.name;
      s.description = cmd.description;
      s.example = cmd.examples.empty() ? "" : cmd.examples[0];
      s.score = score;
      results.push_back(s);
    }
  }

  std::sort(results.begin(), results.end(),
            [](const Suggestion& a, const Suggestion& b) {
              return a.score > b.score;
            });

  return results;
}

std::vector<Suggestion> AutocompleteEngine::GetContextualHelp(
    const std::string& partial_cmd) {
  std::vector<Suggestion> help;

  // Parse command and suggest parameters
  if (partial_cmd.find("hex-") == 0) {
    help.push_back({"--address=0xXXXXXX", "ROM address in hex", ""});
    help.push_back({"--length=16", "Number of bytes", ""});
    help.push_back({"--format=both", "Output format (hex/ascii/both)", ""});
  } else if (partial_cmd.find("palette-") == 0) {
    help.push_back({"--group=0", "Palette group (0-7)", ""});
    help.push_back({"--palette=0", "Palette index (0-7)", ""});
    help.push_back({"--format=hex", "Color format (snes/rgb/hex)", ""});
  } else if (partial_cmd.find("dungeon-") == 0) {
    help.push_back({"--room_id=5", "Room ID (0-296)", ""});
    help.push_back({"--include_sprites=true", "Include sprite data", ""});
  }

  return help;
}

}  // namespace cli
}  // namespace yaze
