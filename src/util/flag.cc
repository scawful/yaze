#include "flag.h"

#include <iostream>

#include "yaze_config.h"

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
    } else if (token.rfind("-", 0) == 0) {
      if (token == "-v" || token == "-version") {
        std::cout << "Version: " << YAZE_VERSION_MAJOR << "."
                  << YAZE_VERSION_MINOR << "." << YAZE_VERSION_PATCH << "\n";
        exit(0);
      }

      // Check for -h or -help
      if (token == "-h" || token == "-help") {
        std::cout << "Available flags:\n";
        for (const auto& flag :
             yaze::util::global_flag_registry()->AllFlags()) {
          std::cout << flag->name() << ": " << flag->help() << "\n";
        }
        exit(0);
      }

      std::string flag_name;
      if (!ExtractFlag(token, &flag_name)) {
        throw std::runtime_error("Unrecognized flag: " + token);
      }

    } else {
      leftover.push_back(token);
    }
  }
  *tokens = leftover;
}

bool FlagParser::ExtractFlagAndValue(const std::string& token,
                                     std::string* flag_name,
                                     std::string* value_string) {
  const size_t eq_pos = token.find('=');
  if (eq_pos == std::string::npos) {
    return false;
  }
  *flag_name = token.substr(0, eq_pos);
  *value_string = token.substr(eq_pos + 1);
  return true;
}

bool FlagParser::ExtractFlag(const std::string& token, std::string* flag_name) {
  if (token.rfind("-", 0) == 0) {
    *flag_name = token;
    return true;
  }
  return false;
}

}  // namespace util
}  // namespace yaze