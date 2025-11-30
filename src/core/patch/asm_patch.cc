#include "core/patch/asm_patch.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/strip.h"

namespace yaze::core {

namespace {

// Trim whitespace from both ends of a string
std::string Trim(const std::string& str) {
  size_t start = str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

// Extract filename from path
std::string GetFilename(const std::string& path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) return path;
  return path.substr(pos + 1);
}

}  // namespace

AsmPatch::AsmPatch(const std::string& file_path, const std::string& folder)
    : folder_(folder), file_path_(file_path) {
  filename_ = GetFilename(file_path);

  // Read file content
  std::ifstream file(file_path);
  if (!file.is_open()) {
    is_valid_ = false;
    return;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  original_content_ = buffer.str();
  file.close();

  ParseMetadata(original_content_);
  is_valid_ = true;

  // If no ;#ENABLED= line was found, default to enabled and prepend it
  if (original_content_.find(";#ENABLED=") == std::string::npos) {
    original_content_ = ";#ENABLED=true\n" + original_content_;
  }
}

void AsmPatch::ParseMetadata(const std::string& content) {
  std::istringstream stream(content);
  std::string line;

  bool in_define_block = false;
  bool reading_description = false;
  PatchParameter current_param;
  bool has_param_attributes = false;

  while (std::getline(stream, line)) {
    // Handle patch name
    if (line.find(";#PATCH_NAME=") != std::string::npos) {
      size_t pos = line.find('=');
      if (pos != std::string::npos) {
        name_ = Trim(line.substr(pos + 1));
      }
      continue;
    }

    // Handle patch author
    if (line.find(";#PATCH_AUTHOR=") != std::string::npos) {
      size_t pos = line.find('=');
      if (pos != std::string::npos) {
        author_ = Trim(line.substr(pos + 1));
      }
      continue;
    }

    // Handle patch version
    if (line.find(";#PATCH_VERSION=") != std::string::npos) {
      size_t pos = line.find('=');
      if (pos != std::string::npos) {
        version_ = Trim(line.substr(pos + 1));
      }
      continue;
    }

    // Handle description start
    if (line.find(";#PATCH_DESCRIPTION") != std::string::npos) {
      reading_description = true;
      description_.clear();
      continue;
    }

    // Handle description end
    if (line.find(";#ENDPATCH_DESCRIPTION") != std::string::npos) {
      reading_description = false;
      continue;
    }

    // Accumulate description lines
    if (reading_description) {
      std::string desc_line = line;
      // Strip leading semicolons
      while (!desc_line.empty() && desc_line[0] == ';') {
        desc_line = desc_line.substr(1);
      }
      description_ += Trim(desc_line) + "\n";
      continue;
    }

    // Handle enabled flag
    if (line.find(";#ENABLED=") != std::string::npos) {
      size_t pos = line.find('=');
      if (pos != std::string::npos) {
        std::string value = absl::AsciiStrToLower(Trim(line.substr(pos + 1)));
        enabled_ = (value == "true" || value == "1");
      }
      continue;
    }

    // Handle define block start
    if (line.find(";#DEFINE_START") != std::string::npos) {
      in_define_block = true;
      current_param = PatchParameter{};
      has_param_attributes = false;
      continue;
    }

    // Handle define block end
    if (line.find(";#DEFINE_END") != std::string::npos) {
      in_define_block = false;
      break;  // Stop parsing after define block
    }

    // Parse define block content
    if (in_define_block) {
      // Parse attribute comments (;#key=value)
      if (line.find(";#") != std::string::npos) {
        size_t hash_pos = line.find(";#");
        std::string attr_line = line.substr(hash_pos + 2);
        size_t eq_pos = attr_line.find('=');

        if (eq_pos != std::string::npos) {
          std::string key = absl::AsciiStrToLower(Trim(attr_line.substr(0, eq_pos)));
          std::string value = Trim(attr_line.substr(eq_pos + 1));

          if (key == "name") {
            current_param.display_name = value;
            has_param_attributes = true;
          } else if (key == "type") {
            current_param.type = ParseType(value);
            has_param_attributes = true;
          } else if (key == "range") {
            // Parse "min,max" format
            size_t comma_pos = value.find(',');
            if (comma_pos != std::string::npos) {
              current_param.min_value = ParseValue(Trim(value.substr(0, comma_pos)));
              current_param.max_value = ParseValue(Trim(value.substr(comma_pos + 1)));
            }
          } else if (key == "checkedvalue") {
            current_param.checked_value = ParseValue(value);
          } else if (key == "uncheckedvalue") {
            current_param.unchecked_value = ParseValue(value);
          } else if (key == "decimal") {
            current_param.use_decimal = true;
          } else if (key.starts_with("choice")) {
            // Handle choice0, choice1, ..., choice9
            current_param.choices.push_back(value);
          } else if (key.starts_with("bit")) {
            // Handle bit0, bit1, ..., bit7
            // Extract bit index
            int bit_index = 0;
            if (key.size() > 3) {
              bit_index = std::stoi(key.substr(3));
            }
            // Ensure choices vector is large enough
            while (current_param.choices.size() <= static_cast<size_t>(bit_index)) {
              current_param.choices.push_back("");
            }
            current_param.choices[bit_index] = value;
          }
        }
        continue;
      }

      // Parse Asar define line: !DEFINE_NAME = $VALUE
      std::string trimmed = Trim(line);
      if (!trimmed.empty() && trimmed[0] == '!') {
        size_t eq_pos = trimmed.find('=');
        if (eq_pos != std::string::npos) {
          current_param.define_name = Trim(trimmed.substr(0, eq_pos));
          std::string value_str = Trim(trimmed.substr(eq_pos + 1));
          current_param.value = ParseValue(value_str);

          // Set default max value based on type if not specified
          if (current_param.max_value == 0xFF) {
            switch (current_param.type) {
              case PatchParameterType::kWord:
                current_param.max_value = 0xFFFF;
                break;
              case PatchParameterType::kLong:
                current_param.max_value = 0xFFFFFF;
                break;
              default:
                break;
            }
          }

          // Use define name as display name if none specified
          if (current_param.display_name.empty()) {
            current_param.display_name = current_param.define_name;
          }

          parameters_.push_back(std::move(current_param));
          current_param = PatchParameter{};
          has_param_attributes = false;
        }
      }
    }
  }

  // Trim trailing newline from description
  while (!description_.empty() && description_.back() == '\n') {
    description_.pop_back();
  }

  // If no name was found, use filename
  if (name_.empty()) {
    name_ = filename_;
    // Remove .asm extension
    if (name_.size() > 4 && name_.substr(name_.size() - 4) == ".asm") {
      name_ = name_.substr(0, name_.size() - 4);
    }
  }
}

PatchParameterType AsmPatch::ParseType(const std::string& type_str) {
  std::string lower = absl::AsciiStrToLower(type_str);

  if (lower.find("byte") != std::string::npos) {
    return PatchParameterType::kByte;
  } else if (lower.find("word") != std::string::npos) {
    return PatchParameterType::kWord;
  } else if (lower.find("long") != std::string::npos) {
    return PatchParameterType::kLong;
  } else if (lower.find("bool") != std::string::npos) {
    return PatchParameterType::kBool;
  } else if (lower.find("choice") != std::string::npos) {
    return PatchParameterType::kChoice;
  } else if (lower.find("bitfield") != std::string::npos) {
    return PatchParameterType::kBitfield;
  } else if (lower.find("item") != std::string::npos) {
    return PatchParameterType::kItem;
  }

  return PatchParameterType::kByte;  // Default to byte
}

int AsmPatch::ParseValue(const std::string& value_str) {
  std::string trimmed = Trim(value_str);
  if (trimmed.empty()) return 0;

  // Handle hex values (prefixed with $)
  if (trimmed[0] == '$') {
    try {
      return std::stoi(trimmed.substr(1), nullptr, 16);
    } catch (...) {
      return 0;
    }
  }

  // Handle hex values (prefixed with 0x)
  if (trimmed.size() > 2 && trimmed[0] == '0' &&
      (trimmed[1] == 'x' || trimmed[1] == 'X')) {
    try {
      return std::stoi(trimmed.substr(2), nullptr, 16);
    } catch (...) {
      return 0;
    }
  }

  // Handle decimal values
  try {
    return std::stoi(trimmed);
  } catch (...) {
    return 0;
  }
}

bool AsmPatch::SetParameterValue(const std::string& define_name, int value) {
  for (auto& param : parameters_) {
    if (param.define_name == define_name) {
      param.value = std::clamp(value, param.min_value, param.max_value);
      return true;
    }
  }
  return false;
}

PatchParameter* AsmPatch::GetParameter(const std::string& define_name) {
  for (auto& param : parameters_) {
    if (param.define_name == define_name) {
      return &param;
    }
  }
  return nullptr;
}

const PatchParameter* AsmPatch::GetParameter(
    const std::string& define_name) const {
  for (const auto& param : parameters_) {
    if (param.define_name == define_name) {
      return &param;
    }
  }
  return nullptr;
}

std::string AsmPatch::GenerateContent() const {
  std::string result = original_content_;

  // Update ;#ENABLED= line
  UpdateLine(result, ";#ENABLED=", enabled_ ? "true" : "false");

  // Update each define value
  for (const auto& param : parameters_) {
    UpdateDefineLine(result, param.define_name, param.value);
  }

  return result;
}

void AsmPatch::UpdateLine(std::string& content, const std::string& prefix,
                          const std::string& new_value) {
  size_t pos = content.find(prefix);
  if (pos == std::string::npos) return;

  size_t line_end = content.find('\n', pos);
  if (line_end == std::string::npos) {
    line_end = content.size();
  }

  // Replace the line content after the prefix
  std::string new_line = prefix + new_value;
  content.replace(pos, line_end - pos, new_line);
}

void AsmPatch::UpdateDefineLine(std::string& content,
                                const std::string& define_name, int value) {
  // Find the define line (starts with the define name)
  size_t pos = content.find(define_name);
  if (pos == std::string::npos) return;

  // Find the end of the line
  size_t line_end = content.find('\n', pos);
  if (line_end == std::string::npos) {
    line_end = content.size();
  }

  // Create new line with updated value
  std::string new_line = absl::StrFormat("%s = $%02X", define_name, value);
  content.replace(pos, line_end - pos, new_line);
}

absl::Status AsmPatch::Save() {
  return SaveToPath(file_path_);
}

absl::Status AsmPatch::SaveToPath(const std::string& output_path) {
  std::string content = GenerateContent();

  std::ofstream file(output_path);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrCat("Failed to open file for writing: ", output_path));
  }

  file << content;
  file.close();

  if (file.fail()) {
    return absl::InternalError(
        absl::StrCat("Failed to write patch file: ", output_path));
  }

  return absl::OkStatus();
}

}  // namespace yaze::core
