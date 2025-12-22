/**
 * @file code_gen_tool.h
 * @brief Code generation tools for AI agents
 *
 * Provides tools for:
 * - ASM patch generation with templates
 * - Hook code generation
 * - Freespace allocation and management
 * - Sprite and event handler templates
 */

#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_CODE_GEN_TOOL_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_CODE_GEN_TOOL_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

/**
 * @brief ASM code template with placeholder substitution
 */
struct AsmTemplate {
  std::string name;
  std::string code_template;  // Uses {{PLACEHOLDER}} syntax
  std::vector<std::string> required_params;
  std::string description;
};

/**
 * @brief Free space region in ROM
 */
struct FreeSpaceRegion {
  uint32_t start;
  uint32_t end;
  std::string description;
  int free_percent;  // Percentage of region that is 0x00/0xFF

  size_t Size() const { return end - start + 1; }
};

/**
 * @brief Diagnostic message from code generation
 */
struct CodeGenerationDiagnostic {
  enum class Severity { kInfo, kWarning, kError };

  Severity severity;
  std::string message;
  uint32_t address = 0;

  std::string SeverityString() const {
    switch (severity) {
      case Severity::kInfo:
        return "info";
      case Severity::kWarning:
        return "warning";
      case Severity::kError:
        return "error";
    }
    return "unknown";
  }
};

/**
 * @brief Result of code generation operation
 */
struct CodeGenerationResult {
  bool success;
  std::string generated_code;
  std::vector<CodeGenerationDiagnostic> diagnostics;
  std::map<std::string, uint32_t> symbols;  // Label -> address

  void AddInfo(const std::string& message, uint32_t address = 0) {
    diagnostics.push_back({CodeGenerationDiagnostic::Severity::kInfo, message, address});
  }

  void AddWarning(const std::string& message, uint32_t address = 0) {
    diagnostics.push_back({CodeGenerationDiagnostic::Severity::kWarning, message, address});
  }

  void AddError(const std::string& message, uint32_t address = 0) {
    success = false;
    diagnostics.push_back({CodeGenerationDiagnostic::Severity::kError, message, address});
  }
};

/**
 * @brief Base class for code generation tools
 *
 * Provides common functionality for ASM code generation:
 * - Template management and substitution
 * - Freespace detection and validation
 * - Address validation
 * - Hook location checking
 */
class CodeGenToolBase : public resources::CommandHandler {
 protected:
  /**
   * @brief Validate that an address is safe for hooking
   */
  absl::Status ValidateHookAddress(Rom* rom, uint32_t address) const;

  /**
   * @brief Detect available free space regions in ROM
   */
  std::vector<FreeSpaceRegion> DetectFreeSpace(Rom* rom, size_t min_size = 0x100) const;

  /**
   * @brief Substitute placeholders in template with parameter values
   *
   * Replaces {{PLACEHOLDER}} with corresponding values from params map.
   */
  std::string SubstitutePlaceholders(
      const std::string& template_code,
      const std::map<std::string, std::string>& params) const;

  /**
   * @brief Get a built-in ASM template by name
   */
  absl::StatusOr<AsmTemplate> GetTemplate(const std::string& name) const;

  /**
   * @brief Get all available built-in templates
   */
  const std::vector<AsmTemplate>& GetAllTemplates() const;

  /**
   * @brief Format code generation result as JSON
   */
  std::string FormatResultAsJson(const CodeGenerationResult& result) const;

  /**
   * @brief Format code generation result as text
   */
  std::string FormatResultAsText(const CodeGenerationResult& result) const;

  /**
   * @brief Check if an address is within a known safe hook location
   */
  bool IsKnownHookLocation(uint32_t address) const;

  /**
   * @brief Get description of a known hook location
   */
  std::string GetHookLocationDescription(uint32_t address) const;

 private:
  static std::vector<AsmTemplate> InitializeTemplates();
  static const std::vector<AsmTemplate> kTemplates;

  // Known safe hook locations (routine_name -> address)
  static const std::map<std::string, uint32_t> kKnownHooks;
};

/**
 * @brief Generate ASM hook at a specific ROM address
 *
 * Creates a JSL hook at the specified address that jumps to new code
 * in freespace. Validates that the address is safe for hooking.
 *
 * Usage: codegen-asm-hook --address=0x008040 --label=MyHook [--nop-fill=N] [--format=<json|text>]
 */
class CodeGenAsmHookTool : public CodeGenToolBase {
 public:
  std::string GetName() const override { return "codegen-asm-hook"; }

  std::string GetDescription() const {
    return "Generate ASM hook at ROM address";
  }

  std::string GetUsage() const override {
    return "codegen-asm-hook --address=0x008040 --label=MyHook [--nop-fill=N] [--format=<json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"address", "label"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresRom() const override { return true; }
};

/**
 * @brief Generate patch using detected freespace regions
 *
 * Analyzes ROM for available freespace and generates a patch that
 * allocates code in the best available region.
 *
 * Usage: codegen-freespace-patch --label=MyCode --size=0x100 [--prefer-bank=N] [--format=<json|text>]
 */
class CodeGenFreespacePatchTool : public CodeGenToolBase {
 public:
  std::string GetName() const override { return "codegen-freespace-patch"; }

  std::string GetDescription() const {
    return "Generate patch using detected free regions";
  }

  std::string GetUsage() const override {
    return "codegen-freespace-patch --label=MyCode --size=0x100 [--prefer-bank=N] [--format=<json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"label"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresRom() const override { return true; }
};

/**
 * @brief Generate sprite ASM from template
 *
 * Creates a complete sprite implementation using the built-in
 * sprite template with customizable init and main code.
 *
 * Usage: codegen-sprite-template --name=MySprite [--init-code="..."] [--main-code="..."] [--format=<json|text>]
 */
class CodeGenSpriteTemplateTool : public CodeGenToolBase {
 public:
  std::string GetName() const override { return "codegen-sprite-template"; }

  std::string GetDescription() const {
    return "Generate sprite ASM from template";
  }

  std::string GetUsage() const override {
    return "codegen-sprite-template --name=MySprite [--init-code=\"...\"] [--main-code=\"...\"] [--format=<json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"name"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresRom() const override { return false; }
};

/**
 * @brief Generate event handler code
 *
 * Creates an event handler (NMI, IRQ, etc.) using the appropriate
 * template with hook installation code.
 *
 * Usage: codegen-event-handler --type=<nmi|irq|reset> --label=MyHandler [--custom-code="..."] [--format=<json|text>]
 */
class CodeGenEventHandlerTool : public CodeGenToolBase {
 public:
  std::string GetName() const override { return "codegen-event-handler"; }

  std::string GetDescription() const {
    return "Generate event handler code";
  }

  std::string GetUsage() const override {
    return "codegen-event-handler --type=<nmi|irq|reset> --label=MyHandler [--custom-code=\"...\"] [--format=<json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"type", "label"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresRom() const override { return false; }
};

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_CODE_GEN_TOOL_H_
