#ifndef YAZE_SRC_CLI_SERVICE_RESOURCES_COMMAND_CONTEXT_H_
#define YAZE_SRC_CLI_SERVICE_RESOURCES_COMMAND_CONTEXT_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/rom.h"

namespace yaze {
namespace cli {
namespace resources {

/**
 * @class CommandContext
 * @brief Encapsulates common context for CLI command execution
 * 
 * Provides unified ROM loading, argument parsing, and common options
 * management to reduce duplication across command handlers.
 */
class CommandContext {
 public:
  /**
   * @brief Configuration for command context
   */
  struct Config {
    std::optional<std::string> rom_path;
    bool use_mock_rom = false;
    std::string format = "json";  // "json" or "text"
    bool verbose = false;

    // ROM context can be provided externally (e.g., from Agent class)
    Rom* external_rom_context = nullptr;
  };

  explicit CommandContext(const Config& config);
  ~CommandContext() = default;

  /**
   * @brief Initialize the context and load ROM if needed
   */
  absl::Status Initialize();

  /**
   * @brief Get the ROM instance (loads if not already loaded)
   */
  absl::StatusOr<Rom*> GetRom();

  /**
   * @brief Get the output format ("json" or "text")
   */
  const std::string& GetFormat() const { return config_.format; }

  /**
   * @brief Check if verbose mode is enabled
   */
  bool IsVerbose() const { return config_.verbose; }

  /**
   * @brief Ensure resource labels are loaded
   */
  absl::Status EnsureLabelsLoaded(Rom* rom);

 private:
  Config config_;
  Rom rom_storage_;  // Owned ROM if loaded from file
  Rom* active_rom_ =
      nullptr;  // Points to either rom_storage_ or external_rom_context
  bool initialized_ = false;
};

/**
 * @class ArgumentParser
 * @brief Utility for parsing common CLI argument patterns
 */
class ArgumentParser {
 public:
  explicit ArgumentParser(const std::vector<std::string>& args);

  /**
   * @brief Parse a named argument (e.g., --format=json or --format json)
   */
  std::optional<std::string> GetString(const std::string& name) const;

  /**
   * @brief Parse an integer argument (supports hex with 0x prefix)
   */
  absl::StatusOr<int> GetInt(const std::string& name) const;

  /**
   * @brief Parse a hex integer argument
   */
  absl::StatusOr<int> GetHex(const std::string& name) const;

  /**
   * @brief Check if a flag is present
   */
  bool HasFlag(const std::string& name) const;

  /**
   * @brief Get all remaining positional arguments
   */
  std::vector<std::string> GetPositional() const;

  /**
   * @brief Validate that required arguments are present
   */
  absl::Status RequireArgs(const std::vector<std::string>& required) const;

 private:
  std::vector<std::string> args_;

  std::optional<std::string> FindArgValue(const std::string& name) const;
};

/**
 * @class OutputFormatter
 * @brief Utility for consistent output formatting across commands
 */
class OutputFormatter {
 public:
  enum class Format { kJson, kText };

  explicit OutputFormatter(Format format) : format_(format) {}

  /**
   * @brief Create formatter from string ("json" or "text")
   */
  static absl::StatusOr<OutputFormatter> FromString(const std::string& format);

  /**
   * @brief Start a JSON object or text section
   */
  void BeginObject(const std::string& title = "");

  /**
   * @brief End a JSON object or text section
   */
  void EndObject();

  /**
   * @brief Add a key-value pair
   */
  void AddField(const std::string& key, const std::string& value);
  void AddField(const std::string& key, int value);
  void AddField(const std::string& key, uint64_t value);
  void AddField(const std::string& key, bool value);

  /**
   * @brief Add a hex-formatted field
   */
  void AddHexField(const std::string& key, uint64_t value, int width = 2);

  /**
   * @brief Begin an array
   */
  void BeginArray(const std::string& key);

  /**
   * @brief End an array
   */
  void EndArray();

  /**
   * @brief Add an item to current array
   */
  void AddArrayItem(const std::string& item);

  /**
   * @brief Get the formatted output
   */
  std::string GetOutput() const;

  /**
   * @brief Print the formatted output to stdout
   */
  void Print() const;

  /**
   * @brief Check if using JSON format
   */
  bool IsJson() const { return format_ == Format::kJson; }

  /**
   * @brief Check if using text format
   */
  bool IsText() const { return format_ == Format::kText; }

 private:
  Format format_;
  std::string buffer_;
  int indent_level_ = 0;
  bool first_field_ = true;
  bool in_array_ = false;
  int array_item_count_ = 0;

  void AddIndent();
  std::string EscapeJson(const std::string& str) const;
};

}  // namespace resources
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_RESOURCES_COMMAND_CONTEXT_H_
