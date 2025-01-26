#include "flag.h"

namespace yaze {
namespace util {

void FlagParser::Parse(std::vector<std::string>* tokens) {
  std::vector<std::string> leftover;
  leftover.reserve(tokens->size());

  for (size_t i = 0; i < tokens->size(); i++) {
    const std::string& token = (*tokens)[i];
    if (token.rfind("--", 0) == 0) {
      // Found a token that starts with "--".
      std::string flag_name;
      std::string value_string;
      if (!ExtractFlagAndValue(token, &flag_name, &value_string)) {
        // If no value found after '=', see if next token is a value.
        if ((i + 1) < tokens->size()) {
          const std::string& next_token = (*tokens)[i + 1];
          // If next token is NOT another flag, treat it as the value.
          if (next_token.rfind("--", 0) != 0) {
            value_string = next_token;
            i++;
          } else {
            // If no explicit value, treat it as boolean 'true'.
            value_string = "true";
          }
        } else {
          value_string = "true";
        }
        flag_name = token;
      }

      // Attempt to parse the flag (strip leading dashes in the registry).
      IFlag* flag_ptr = registry_->GetFlag(flag_name);
      if (!flag_ptr) {
        throw std::runtime_error("Unrecognized flag: " + flag_name);
      }

      // Set the parsed value on the matching flag.
      flag_ptr->ParseValue(value_string);
    } else {
      leftover.push_back(token);
    }
  }
  *tokens = leftover;
}

}  // namespace util
}  // namespace yaze