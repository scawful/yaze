#ifndef YAZE_CORE_PATCH_ASM_PATCH_H
#define YAZE_CORE_PATCH_ASM_PATCH_H

#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze::core {

/**
 * @brief Parameter types supported by ZScream-compatible ASM patches
 *
 * These correspond to the `;#type=` metadata in patch files.
 */
enum class PatchParameterType {
  kByte,      // 8-bit value (0-255)
  kWord,      // 16-bit value (0-65535)
  kLong,      // 24-bit value (0-16777215)
  kBool,      // Boolean toggle with custom checked/unchecked values
  kChoice,    // Radio button selection from choice0-choice9
  kBitfield,  // Checkbox group for bit flags (bit0-bit7)
  kItem       // Game item selection dropdown
};

/**
 * @brief Represents a configurable parameter within an ASM patch
 *
 * Parameters are defined in the patch file between `;#DEFINE_START` and
 * `;#DEFINE_END` markers. Each parameter has metadata attributes followed
 * by an Asar define line (e.g., `!MY_DEFINE = $10`).
 */
struct PatchParameter {
  std::string define_name;   // Asar define name (e.g., "!MY_DEFINE")
  std::string display_name;  // User-friendly display name
  PatchParameterType type = PatchParameterType::kByte;
  int value = 0;             // Current value
  int min_value = 0;         // Minimum allowed value
  int max_value = 0xFF;      // Maximum allowed value
  int checked_value = 1;     // Value when checkbox is checked (bool type)
  int unchecked_value = 0;   // Value when checkbox is unchecked (bool type)
  bool use_decimal = false;  // Display value as decimal instead of hex
  std::vector<std::string> choices;  // Choice labels (choice/bitfield types)
};

/**
 * @brief Represents a ZScream-compatible ASM patch file
 *
 * ZScream uses comment-based metadata in ASM files to define patch properties
 * and configurable parameters. This class parses that metadata and provides
 * methods to modify parameter values and regenerate the file content.
 *
 * Example patch file format:
 * ```asm
 * ;#PATCH_NAME=My Patch
 * ;#PATCH_AUTHOR=Author Name
 * ;#PATCH_VERSION=1.0
 * ;#PATCH_DESCRIPTION
 * ; Multi-line description
 * ;#ENDPATCH_DESCRIPTION
 * ;#ENABLED=true
 *
 * ;#DEFINE_START
 * ;#name=My Value
 * ;#type=byte
 * ;#range=0,$FF
 * !MY_DEFINE = $10
 * ;#DEFINE_END
 *
 * lorom
 * ; ASM code...
 * ```
 */
class AsmPatch {
 public:
  /**
   * @brief Construct a patch from a file path
   * @param file_path Full path to the .asm file
   * @param folder Category folder name (e.g., "Sprites", "Misc")
   */
  AsmPatch(const std::string& file_path, const std::string& folder);

  // Metadata accessors
  const std::string& name() const { return name_; }
  const std::string& author() const { return author_; }
  const std::string& version() const { return version_; }
  const std::string& description() const { return description_; }
  const std::string& folder() const { return folder_; }
  const std::string& filename() const { return filename_; }
  const std::string& file_path() const { return file_path_; }

  // Enable/disable state
  bool enabled() const { return enabled_; }
  void set_enabled(bool enabled) { enabled_ = enabled; }

  // Parameter access
  const std::vector<PatchParameter>& parameters() const { return parameters_; }
  std::vector<PatchParameter>& mutable_parameters() { return parameters_; }

  /**
   * @brief Set the value of a parameter by its define name
   * @param define_name The Asar define name (e.g., "!MY_DEFINE")
   * @param value The new value to set
   * @return true if the parameter was found and updated
   */
  bool SetParameterValue(const std::string& define_name, int value);

  /**
   * @brief Get a parameter by its define name
   * @param define_name The Asar define name
   * @return Pointer to the parameter, or nullptr if not found
   */
  PatchParameter* GetParameter(const std::string& define_name);
  const PatchParameter* GetParameter(const std::string& define_name) const;

  /**
   * @brief Generate the patch file content with current parameter values
   * @return The complete .asm file content
   */
  std::string GenerateContent() const;

  /**
   * @brief Save the patch to its original file location
   * @return Status indicating success or failure
   */
  absl::Status Save();

  /**
   * @brief Save the patch to a specific path
   * @param output_path The path to save to
   * @return Status indicating success or failure
   */
  absl::Status SaveToPath(const std::string& output_path);

  /**
   * @brief Check if the patch was loaded successfully
   */
  bool is_valid() const { return is_valid_; }

 private:
  /**
   * @brief Parse metadata from the patch file content
   * @param content The full file content
   */
  void ParseMetadata(const std::string& content);

  /**
   * @brief Parse a parameter type from a string
   * @param type_str The type string (e.g., "byte", "bool", "choice")
   * @return The corresponding PatchParameterType
   */
  static PatchParameterType ParseType(const std::string& type_str);

  /**
   * @brief Parse an integer value from a string (handles $hex and decimal)
   * @param value_str The value string (e.g., "$10" or "16")
   * @return The parsed integer value
   */
  static int ParseValue(const std::string& value_str);

  /**
   * @brief Update a line in the content that starts with a prefix
   * @param content The content to modify
   * @param prefix The line prefix to find
   * @param new_value The new value to set after the '='
   */
  static void UpdateLine(std::string& content, const std::string& prefix,
                         const std::string& new_value);

  /**
   * @brief Update an Asar define line with a new value
   * @param content The content to modify
   * @param define_name The define name to find
   * @param value The new value
   */
  static void UpdateDefineLine(std::string& content,
                               const std::string& define_name, int value);

  // Patch metadata
  std::string name_;
  std::string author_;
  std::string version_;
  std::string description_;
  std::string folder_;
  std::string filename_;
  std::string file_path_;
  std::string original_content_;
  bool enabled_ = true;
  bool is_valid_ = false;

  // Configurable parameters
  std::vector<PatchParameter> parameters_;
};

}  // namespace yaze::core

#endif  // YAZE_CORE_PATCH_ASM_PATCH_H
