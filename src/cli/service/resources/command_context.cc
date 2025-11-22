#include "cli/service/resources/command_context.h"

#include <iostream>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/handlers/rom/mock_rom.h"
#include "core/project.h"

ABSL_DECLARE_FLAG(std::string, rom);
ABSL_DECLARE_FLAG(bool, mock_rom);

namespace yaze {
namespace cli {
namespace resources {

// ============================================================================
// CommandContext Implementation
// ============================================================================

CommandContext::CommandContext(const Config& config) : config_(config) {}

absl::Status CommandContext::Initialize() {
  if (initialized_) {
    return absl::OkStatus();
  }

  // If external ROM context is provided, use it
  if (config_.external_rom_context != nullptr &&
      config_.external_rom_context->is_loaded()) {
    active_rom_ = config_.external_rom_context;
    initialized_ = true;
    return absl::OkStatus();
  }

  // Check if mock ROM mode is enabled
  if (config_.use_mock_rom) {
    auto status = cli::InitializeMockRom(rom_storage_);
    if (!status.ok()) {
      return status;
    }
    active_rom_ = &rom_storage_;
    initialized_ = true;
    return absl::OkStatus();
  }

  // Load ROM from file if path is provided
  if (config_.rom_path.has_value() && !config_.rom_path->empty()) {
    auto status = rom_storage_.LoadFromFile(*config_.rom_path);
    if (!status.ok()) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Failed to load ROM from '%s': %s", *config_.rom_path,
                          status.message()));
    }
    active_rom_ = &rom_storage_;
    initialized_ = true;
    return absl::OkStatus();
  }

  // Try loading from flags as fallback
  std::string rom_path = absl::GetFlag(FLAGS_rom);
  if (!rom_path.empty()) {
    auto status = rom_storage_.LoadFromFile(rom_path);
    if (!status.ok()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Failed to load ROM from '%s': %s", rom_path, status.message()));
    }
    active_rom_ = &rom_storage_;
    initialized_ = true;
    return absl::OkStatus();
  }

  return absl::FailedPreconditionError(
      "No ROM loaded. Use --rom=<path> or --mock-rom for testing.");
}

absl::StatusOr<Rom*> CommandContext::GetRom() {
  if (!initialized_) {
    auto status = Initialize();
    if (!status.ok()) {
      return status;
    }
  }

  if (active_rom_ == nullptr) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  return active_rom_;
}

absl::Status CommandContext::EnsureLabelsLoaded(Rom* rom) {
  if (!rom->resource_label()) {
    return absl::FailedPreconditionError("ROM has no resource label manager");
  }

  if (!rom->resource_label()->labels_loaded_ ||
      rom->resource_label()->labels_.empty()) {
    project::YazeProject project;
    project.use_embedded_labels = true;
    auto labels_status = project.InitializeEmbeddedLabels();
    if (labels_status.ok()) {
      rom->resource_label()->labels_ = project.resource_labels;
      rom->resource_label()->labels_loaded_ = true;
    } else {
      return labels_status;
    }
  }

  return absl::OkStatus();
}

// ============================================================================
// ArgumentParser Implementation
// ============================================================================

ArgumentParser::ArgumentParser(const std::vector<std::string>& args)
    : args_(args) {}

std::optional<std::string> ArgumentParser::FindArgValue(
    const std::string& name) const {
  std::string flag = "--" + name;
  std::string equals_form = flag + "=";

  for (size_t i = 0; i < args_.size(); ++i) {
    const std::string& arg = args_[i];

    // Check for --name=value form
    if (absl::StartsWith(arg, equals_form)) {
      return arg.substr(equals_form.length());
    }

    // Check for --name value form
    if (arg == flag && i + 1 < args_.size()) {
      return args_[i + 1];
    }
  }

  return std::nullopt;
}

std::optional<std::string> ArgumentParser::GetString(
    const std::string& name) const {
  return FindArgValue(name);
}

absl::StatusOr<int> ArgumentParser::GetInt(const std::string& name) const {
  auto value = FindArgValue(name);
  if (!value.has_value()) {
    return absl::NotFoundError(
        absl::StrFormat("Argument '--%s' not found", name));
  }

  // Try hex first (with 0x prefix)
  if (absl::StartsWith(*value, "0x") || absl::StartsWith(*value, "0X")) {
    int result;
    if (absl::SimpleHexAtoi(value->substr(2), &result)) {
      return result;
    }
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid hex integer for '--%s': %s", name, *value));
  }

  // Try decimal
  int result;
  if (absl::SimpleAtoi(*value, &result)) {
    return result;
  }

  return absl::InvalidArgumentError(
      absl::StrFormat("Invalid integer for '--%s': %s", name, *value));
}

absl::StatusOr<int> ArgumentParser::GetHex(const std::string& name) const {
  auto value = FindArgValue(name);
  if (!value.has_value()) {
    return absl::NotFoundError(
        absl::StrFormat("Argument '--%s' not found", name));
  }

  // Strip 0x prefix if present
  std::string hex_str = *value;
  if (absl::StartsWith(hex_str, "0x") || absl::StartsWith(hex_str, "0X")) {
    hex_str = hex_str.substr(2);
  }

  int result;
  if (absl::SimpleHexAtoi(hex_str, &result)) {
    return result;
  }

  return absl::InvalidArgumentError(
      absl::StrFormat("Invalid hex value for '--%s': %s", name, *value));
}

bool ArgumentParser::HasFlag(const std::string& name) const {
  std::string flag = "--" + name;
  for (const auto& arg : args_) {
    if (arg == flag) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> ArgumentParser::GetPositional() const {
  std::vector<std::string> positional;
  for (size_t i = 0; i < args_.size(); ++i) {
    const std::string& arg = args_[i];
    if (!absl::StartsWith(arg, "--")) {
      positional.push_back(arg);
    } else if (arg.find('=') == std::string::npos && i + 1 < args_.size()) {
      // Skip the next argument as it's the value for this flag
      ++i;
    }
  }
  return positional;
}

absl::Status ArgumentParser::RequireArgs(
    const std::vector<std::string>& required) const {
  std::vector<std::string> missing;
  for (const auto& arg : required) {
    if (!FindArgValue(arg).has_value()) {
      missing.push_back("--" + arg);
    }
  }

  if (!missing.empty()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Missing required arguments: %s", absl::StrJoin(missing, ", ")));
  }

  return absl::OkStatus();
}

// ============================================================================
// OutputFormatter Implementation
// ============================================================================

absl::StatusOr<OutputFormatter> OutputFormatter::FromString(
    const std::string& format) {
  std::string lower = absl::AsciiStrToLower(format);
  if (lower == "json") {
    return OutputFormatter(Format::kJson);
  } else if (lower == "text") {
    return OutputFormatter(Format::kText);
  } else {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Unknown format: %s (expected 'json' or 'text')", format));
  }
}

void OutputFormatter::BeginObject(const std::string& title) {
  if (IsJson()) {
    buffer_ += "{\n";
    indent_level_++;
    first_field_ = true;
  } else if (IsText() && !title.empty()) {
    buffer_ += absl::StrFormat("=== %s ===\n", title);
  }
}

void OutputFormatter::EndObject() {
  if (IsJson()) {
    buffer_ += "\n";
    indent_level_--;
    AddIndent();
    buffer_ += "}";
  }
}

void OutputFormatter::AddField(const std::string& key,
                               const std::string& value) {
  if (IsJson()) {
    if (!first_field_) {
      buffer_ += ",\n";
    }
    first_field_ = false;
    AddIndent();
    buffer_ +=
        absl::StrFormat("\"%s\": \"%s\"", EscapeJson(key), EscapeJson(value));
  } else {
    buffer_ += absl::StrFormat("  %-20s : %s\n", key, value);
  }
}

void OutputFormatter::AddField(const std::string& key, int value) {
  if (IsJson()) {
    if (!first_field_) {
      buffer_ += ",\n";
    }
    first_field_ = false;
    AddIndent();
    buffer_ += absl::StrFormat("\"%s\": %d", EscapeJson(key), value);
  } else {
    buffer_ += absl::StrFormat("  %-20s : %d\n", key, value);
  }
}

void OutputFormatter::AddField(const std::string& key, uint64_t value) {
  if (IsJson()) {
    if (!first_field_) {
      buffer_ += ",\n";
    }
    first_field_ = false;
    AddIndent();
    buffer_ += absl::StrFormat("\"%s\": %llu", EscapeJson(key), value);
  } else {
    buffer_ += absl::StrFormat("  %-20s : %llu\n", key, value);
  }
}

void OutputFormatter::AddField(const std::string& key, bool value) {
  if (IsJson()) {
    if (!first_field_) {
      buffer_ += ",\n";
    }
    first_field_ = false;
    AddIndent();
    buffer_ += absl::StrFormat("\"%s\": %s", EscapeJson(key),
                               value ? "true" : "false");
  } else {
    buffer_ += absl::StrFormat("  %-20s : %s\n", key, value ? "yes" : "no");
  }
}

void OutputFormatter::AddHexField(const std::string& key, uint64_t value,
                                  int width) {
  if (IsJson()) {
    if (!first_field_) {
      buffer_ += ",\n";
    }
    first_field_ = false;
    AddIndent();
    buffer_ +=
        absl::StrFormat("\"%s\": \"0x%0*X\"", EscapeJson(key), width, value);
  } else {
    buffer_ += absl::StrFormat("  %-20s : 0x%0*X\n", key, width, value);
  }
}

void OutputFormatter::BeginArray(const std::string& key) {
  in_array_ = true;
  array_item_count_ = 0;

  if (IsJson()) {
    if (!first_field_) {
      buffer_ += ",\n";
    }
    first_field_ = false;
    AddIndent();
    buffer_ += absl::StrFormat("\"%s\": [\n", EscapeJson(key));
    indent_level_++;
  } else {
    buffer_ += absl::StrFormat("  %s:\n", key);
  }
}

void OutputFormatter::EndArray() {
  in_array_ = false;

  if (IsJson()) {
    buffer_ += "\n";
    indent_level_--;
    AddIndent();
    buffer_ += "]";
  }
}

void OutputFormatter::AddArrayItem(const std::string& item) {
  if (IsJson()) {
    if (array_item_count_ > 0) {
      buffer_ += ",\n";
    }
    AddIndent();
    buffer_ += absl::StrFormat("\"%s\"", EscapeJson(item));
  } else {
    buffer_ += absl::StrFormat("    - %s\n", item);
  }
  array_item_count_++;
}

std::string OutputFormatter::GetOutput() const {
  return buffer_;
}

void OutputFormatter::Print() const {
  std::cout << buffer_;
  if (IsJson()) {
    std::cout << "\n";
  }
}

void OutputFormatter::AddIndent() {
  for (int i = 0; i < indent_level_; ++i) {
    buffer_ += "  ";
  }
}

std::string OutputFormatter::EscapeJson(const std::string& str) const {
  std::string result;
  result.reserve(str.size() + 10);

  for (char c : str) {
    switch (c) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\b':
        result += "\\b";
        break;
      case '\f':
        result += "\\f";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        if (c < 0x20) {
          result += absl::StrFormat("\\u%04x", static_cast<int>(c));
        } else {
          result += c;
        }
    }
  }

  return result;
}

}  // namespace resources
}  // namespace cli
}  // namespace yaze
