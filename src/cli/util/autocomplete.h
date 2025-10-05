#ifndef YAZE_CLI_UTIL_AUTOCOMPLETE_H_
#define YAZE_CLI_UTIL_AUTOCOMPLETE_H_

#include <string>
#include <vector>
#include <functional>

namespace yaze {
namespace cli {

struct Suggestion {
  std::string text;
  std::string description;
  std::string example;
  int score;
};

class AutocompleteEngine {
 public:
  void RegisterCommand(const std::string& cmd, const std::string& desc, 
                       const std::vector<std::string>& examples = {});
  
  void RegisterParameter(const std::string& param, const std::string& desc,
                         const std::vector<std::string>& values = {});
  
  std::vector<Suggestion> GetSuggestions(const std::string& input);
  
  std::vector<Suggestion> GetContextualHelp(const std::string& partial_cmd);
  
  void SetRomContext(bool has_rom) { has_rom_ = has_rom; }
  
 private:
  struct CommandDef {
    std::string name;
    std::string description;
    std::vector<std::string> params;
    std::vector<std::string> examples;
  };
  
  std::vector<CommandDef> commands_;
  bool has_rom_ = false;
  
  int FuzzyScore(const std::string& text, const std::string& query);
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_UTIL_AUTOCOMPLETE_H_
