/**
 * @file code_gen_tool_test.cc
 * @brief Unit tests for the CodeGenTool AI agent tools
 *
 * Tests the code generation functionality including ASM templates,
 * placeholder substitution, freespace detection, and hook validation.
 */

#include "cli/service/agent/tools/code_gen_tool.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {
namespace {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

// =============================================================================
// AsmTemplate Structure Tests
// =============================================================================

TEST(AsmTemplateTest, StructureHasExpectedFields) {
  AsmTemplate tmpl;
  tmpl.name = "test";
  tmpl.code_template = "NOP";
  tmpl.required_params = {"PARAM1", "PARAM2"};
  tmpl.description = "Test template";

  EXPECT_EQ(tmpl.name, "test");
  EXPECT_EQ(tmpl.code_template, "NOP");
  EXPECT_THAT(tmpl.required_params, SizeIs(2));
  EXPECT_EQ(tmpl.description, "Test template");
}

// =============================================================================
// FreeSpaceRegion Tests
// =============================================================================

TEST(FreeSpaceRegionTest, StructureHasExpectedFields) {
  FreeSpaceRegion region;
  region.start = 0x1F8000;
  region.end = 0x1FFFFF;
  region.description = "Bank $3F freespace";
  region.free_percent = 95;

  EXPECT_EQ(region.start, 0x1F8000u);
  EXPECT_EQ(region.end, 0x1FFFFFu);
  EXPECT_EQ(region.description, "Bank $3F freespace");
  EXPECT_EQ(region.free_percent, 95);
}

TEST(FreeSpaceRegionTest, SizeCalculation) {
  FreeSpaceRegion region;
  region.start = 0x1F8000;
  region.end = 0x1FFFFF;

  // 0x1FFFFF - 0x1F8000 + 1 = 0x8000 = 32768 bytes
  EXPECT_EQ(region.Size(), 0x8000u);
}

TEST(FreeSpaceRegionTest, SizeCalculationSingleByte) {
  FreeSpaceRegion region;
  region.start = 0x100;
  region.end = 0x100;

  EXPECT_EQ(region.Size(), 1u);
}

TEST(FreeSpaceRegionTest, SizeCalculationLargeRegion) {
  FreeSpaceRegion region;
  region.start = 0x000000;
  region.end = 0x3FFFFF;

  // 4MB region
  EXPECT_EQ(region.Size(), 0x400000u);
}

// =============================================================================
// CodeGenerationDiagnostic Tests
// =============================================================================

TEST(CodeGenerationDiagnosticTest, SeverityStringInfo) {
  CodeGenerationDiagnostic diag;
  diag.severity = CodeGenerationDiagnostic::Severity::kInfo;
  EXPECT_EQ(diag.SeverityString(), "info");
}

TEST(CodeGenerationDiagnosticTest, SeverityStringWarning) {
  CodeGenerationDiagnostic diag;
  diag.severity = CodeGenerationDiagnostic::Severity::kWarning;
  EXPECT_EQ(diag.SeverityString(), "warning");
}

TEST(CodeGenerationDiagnosticTest, SeverityStringError) {
  CodeGenerationDiagnostic diag;
  diag.severity = CodeGenerationDiagnostic::Severity::kError;
  EXPECT_EQ(diag.SeverityString(), "error");
}

TEST(CodeGenerationDiagnosticTest, StructureHasExpectedFields) {
  CodeGenerationDiagnostic diag;
  diag.severity = CodeGenerationDiagnostic::Severity::kWarning;
  diag.message = "Test warning";
  diag.address = 0x008000;

  EXPECT_EQ(diag.message, "Test warning");
  EXPECT_EQ(diag.address, 0x008000u);
}

// =============================================================================
// CodeGenerationResult Tests
// =============================================================================

TEST(CodeGenerationResultTest, DefaultConstruction) {
  CodeGenerationResult result;
  // success is uninitialized by default - not testing its value
  EXPECT_TRUE(result.generated_code.empty());
  EXPECT_TRUE(result.diagnostics.empty());
  EXPECT_TRUE(result.symbols.empty());
}

TEST(CodeGenerationResultTest, AddInfoAddsDiagnostic) {
  CodeGenerationResult result;
  result.success = true;

  result.AddInfo("Test info", 0x008000);

  EXPECT_THAT(result.diagnostics, SizeIs(1));
  EXPECT_EQ(result.diagnostics[0].severity,
            CodeGenerationDiagnostic::Severity::kInfo);
  EXPECT_EQ(result.diagnostics[0].message, "Test info");
  EXPECT_EQ(result.diagnostics[0].address, 0x008000u);
  EXPECT_TRUE(result.success);  // Info doesn't change success
}

TEST(CodeGenerationResultTest, AddWarningAddsDiagnostic) {
  CodeGenerationResult result;
  result.success = true;

  result.AddWarning("Test warning", 0x00A000);

  EXPECT_THAT(result.diagnostics, SizeIs(1));
  EXPECT_EQ(result.diagnostics[0].severity,
            CodeGenerationDiagnostic::Severity::kWarning);
  EXPECT_EQ(result.diagnostics[0].message, "Test warning");
  EXPECT_TRUE(result.success);  // Warning doesn't change success
}

TEST(CodeGenerationResultTest, AddErrorSetsFailure) {
  CodeGenerationResult result;
  result.success = true;

  result.AddError("Test error", 0x00C000);

  EXPECT_THAT(result.diagnostics, SizeIs(1));
  EXPECT_EQ(result.diagnostics[0].severity,
            CodeGenerationDiagnostic::Severity::kError);
  EXPECT_EQ(result.diagnostics[0].message, "Test error");
  EXPECT_FALSE(result.success);  // Error sets success to false
}

TEST(CodeGenerationResultTest, MultipleDiagnostics) {
  CodeGenerationResult result;
  result.success = true;

  result.AddInfo("Info 1");
  result.AddWarning("Warning 1");
  result.AddError("Error 1");
  result.AddInfo("Info 2");

  EXPECT_THAT(result.diagnostics, SizeIs(4));
  EXPECT_FALSE(result.success);  // Because we added an error
}

TEST(CodeGenerationResultTest, SymbolsMap) {
  CodeGenerationResult result;
  result.symbols["MyLabel"] = 0x1F8000;
  result.symbols["OtherLabel"] = 0x00A000;

  EXPECT_EQ(result.symbols["MyLabel"], 0x1F8000u);
  EXPECT_EQ(result.symbols["OtherLabel"], 0x00A000u);
}

// =============================================================================
// Tool Name Tests
// =============================================================================

TEST(CodeGenToolsTest, AsmHookToolName) {
  CodeGenAsmHookTool tool;
  EXPECT_EQ(tool.GetName(), "codegen-asm-hook");
}

TEST(CodeGenToolsTest, FreespacePatchToolName) {
  CodeGenFreespacePatchTool tool;
  EXPECT_EQ(tool.GetName(), "codegen-freespace-patch");
}

TEST(CodeGenToolsTest, SpriteTemplateToolName) {
  CodeGenSpriteTemplateTool tool;
  EXPECT_EQ(tool.GetName(), "codegen-sprite-template");
}

TEST(CodeGenToolsTest, EventHandlerToolName) {
  CodeGenEventHandlerTool tool;
  EXPECT_EQ(tool.GetName(), "codegen-event-handler");
}

TEST(CodeGenToolsTest, AllToolNamesStartWithCodegen) {
  CodeGenAsmHookTool hook;
  CodeGenFreespacePatchTool freespace;
  CodeGenSpriteTemplateTool sprite;
  CodeGenEventHandlerTool event;

  EXPECT_THAT(hook.GetName(), HasSubstr("codegen-"));
  EXPECT_THAT(freespace.GetName(), HasSubstr("codegen-"));
  EXPECT_THAT(sprite.GetName(), HasSubstr("codegen-"));
  EXPECT_THAT(event.GetName(), HasSubstr("codegen-"));
}

TEST(CodeGenToolsTest, AllToolNamesAreUnique) {
  CodeGenAsmHookTool hook;
  CodeGenFreespacePatchTool freespace;
  CodeGenSpriteTemplateTool sprite;
  CodeGenEventHandlerTool event;

  std::vector<std::string> names = {hook.GetName(), freespace.GetName(),
                                     sprite.GetName(), event.GetName()};

  std::set<std::string> unique_names(names.begin(), names.end());
  EXPECT_EQ(unique_names.size(), names.size())
      << "All code gen tool names should be unique";
}

// =============================================================================
// Tool Usage String Tests
// =============================================================================

TEST(CodeGenToolsTest, AsmHookToolUsageFormat) {
  CodeGenAsmHookTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--address"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--label"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--nop-fill"));
}

TEST(CodeGenToolsTest, FreespacePatchToolUsageFormat) {
  CodeGenFreespacePatchTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--label"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--size"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--prefer-bank"));
}

TEST(CodeGenToolsTest, SpriteTemplateToolUsageFormat) {
  CodeGenSpriteTemplateTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--name"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--init-code"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--main-code"));
}

TEST(CodeGenToolsTest, EventHandlerToolUsageFormat) {
  CodeGenEventHandlerTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--type"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--label"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--custom-code"));
}

// =============================================================================
// CodeGenToolBase Method Tests (via concrete class)
// =============================================================================

// Test class to expose protected methods for testing
class TestableCodeGenTool : public CodeGenToolBase {
 public:
  using CodeGenToolBase::DetectFreeSpace;
  using CodeGenToolBase::FormatResultAsJson;
  using CodeGenToolBase::FormatResultAsText;
  using CodeGenToolBase::GetAllTemplates;
  using CodeGenToolBase::GetHookLocationDescription;
  using CodeGenToolBase::GetTemplate;
  using CodeGenToolBase::IsKnownHookLocation;
  using CodeGenToolBase::SubstitutePlaceholders;
  using CodeGenToolBase::ValidateHookAddress;

  std::string GetName() const override { return "testable-codegen"; }
  std::string GetUsage() const override { return "test"; }

 protected:
  absl::Status ValidateArgs(
      const resources::ArgumentParser& /*parser*/) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* /*rom*/,
                       const resources::ArgumentParser& /*parser*/,
                       resources::OutputFormatter& /*formatter*/) override {
    return absl::OkStatus();
  }
};

TEST(CodeGenToolBaseTest, SubstitutePlaceholdersSimple) {
  TestableCodeGenTool tool;

  std::string tmpl = "Hello {{NAME}}!";
  std::map<std::string, std::string> params = {{"NAME", "World"}};

  std::string result = tool.SubstitutePlaceholders(tmpl, params);
  EXPECT_EQ(result, "Hello World!");
}

TEST(CodeGenToolBaseTest, SubstitutePlaceholdersMultiple) {
  TestableCodeGenTool tool;

  std::string tmpl = "org ${{ADDRESS}}\n{{LABEL}}:\n  JSR {{SUBROUTINE}}\n  RTL";
  std::map<std::string, std::string> params = {
      {"ADDRESS", "1F8000"}, {"LABEL", "MyCode"}, {"SUBROUTINE", "DoStuff"}};

  std::string result = tool.SubstitutePlaceholders(tmpl, params);

  EXPECT_THAT(result, HasSubstr("org $1F8000"));
  EXPECT_THAT(result, HasSubstr("MyCode:"));
  EXPECT_THAT(result, HasSubstr("JSR DoStuff"));
}

TEST(CodeGenToolBaseTest, SubstitutePlaceholdersNoMatch) {
  TestableCodeGenTool tool;

  std::string tmpl = "No placeholders here";
  std::map<std::string, std::string> params = {{"UNUSED", "value"}};

  std::string result = tool.SubstitutePlaceholders(tmpl, params);
  EXPECT_EQ(result, "No placeholders here");
}

TEST(CodeGenToolBaseTest, SubstitutePlaceholdersMissingParam) {
  TestableCodeGenTool tool;

  std::string tmpl = "Hello {{NAME}} and {{OTHER}}!";
  std::map<std::string, std::string> params = {{"NAME", "World"}};

  std::string result = tool.SubstitutePlaceholders(tmpl, params);
  // Missing param should remain as placeholder
  EXPECT_THAT(result, HasSubstr("World"));
  EXPECT_THAT(result, HasSubstr("{{OTHER}}"));
}

TEST(CodeGenToolBaseTest, GetAllTemplatesNotEmpty) {
  TestableCodeGenTool tool;
  const auto& templates = tool.GetAllTemplates();

  EXPECT_FALSE(templates.empty());
}

TEST(CodeGenToolBaseTest, GetAllTemplatesContainsExpectedTemplates) {
  TestableCodeGenTool tool;
  const auto& templates = tool.GetAllTemplates();

  std::vector<std::string> names;
  for (const auto& tmpl : templates) {
    names.push_back(tmpl.name);
  }

  EXPECT_THAT(names, Contains("nmi_hook"));
  EXPECT_THAT(names, Contains("sprite"));
  EXPECT_THAT(names, Contains("freespace_alloc"));
  EXPECT_THAT(names, Contains("jsl_hook"));
  EXPECT_THAT(names, Contains("event_handler"));
}

TEST(CodeGenToolBaseTest, GetTemplateFound) {
  TestableCodeGenTool tool;

  auto result = tool.GetTemplate("sprite");
  ASSERT_TRUE(result.ok()) << result.status().message();

  EXPECT_EQ(result->name, "sprite");
  EXPECT_FALSE(result->code_template.empty());
  EXPECT_FALSE(result->required_params.empty());
}

TEST(CodeGenToolBaseTest, GetTemplateNotFound) {
  TestableCodeGenTool tool;

  auto result = tool.GetTemplate("nonexistent");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kNotFound);
}

TEST(CodeGenToolBaseTest, IsKnownHookLocationTrue) {
  TestableCodeGenTool tool;

  // Known hook: EnableForceBlank at 0x00893D
  EXPECT_TRUE(tool.IsKnownHookLocation(0x00893D));
}

TEST(CodeGenToolBaseTest, IsKnownHookLocationFalse) {
  TestableCodeGenTool tool;

  EXPECT_FALSE(tool.IsKnownHookLocation(0x000000));
  EXPECT_FALSE(tool.IsKnownHookLocation(0xFFFFFF));
}

TEST(CodeGenToolBaseTest, GetHookLocationDescriptionKnown) {
  TestableCodeGenTool tool;

  std::string desc = tool.GetHookLocationDescription(0x00893D);
  EXPECT_EQ(desc, "EnableForceBlank");
}

TEST(CodeGenToolBaseTest, GetHookLocationDescriptionUnknown) {
  TestableCodeGenTool tool;

  std::string desc = tool.GetHookLocationDescription(0x000000);
  EXPECT_EQ(desc, "Unknown");
}

TEST(CodeGenToolBaseTest, FormatResultAsJsonContainsSuccess) {
  TestableCodeGenTool tool;
  CodeGenerationResult result;
  result.success = true;
  result.generated_code = "NOP";

  std::string json = tool.FormatResultAsJson(result);
  EXPECT_THAT(json, HasSubstr("\"success\": true"));
}

TEST(CodeGenToolBaseTest, FormatResultAsJsonContainsCode) {
  TestableCodeGenTool tool;
  CodeGenerationResult result;
  result.success = true;
  result.generated_code = "LDA #$00";

  std::string json = tool.FormatResultAsJson(result);
  EXPECT_THAT(json, HasSubstr("\"code\":"));
  EXPECT_THAT(json, HasSubstr("LDA"));
}

TEST(CodeGenToolBaseTest, FormatResultAsJsonContainsDiagnostics) {
  TestableCodeGenTool tool;
  CodeGenerationResult result;
  result.success = true;
  result.AddInfo("Test info");

  std::string json = tool.FormatResultAsJson(result);
  EXPECT_THAT(json, HasSubstr("\"diagnostics\":"));
  EXPECT_THAT(json, HasSubstr("Test info"));
}

TEST(CodeGenToolBaseTest, FormatResultAsTextContainsStatus) {
  TestableCodeGenTool tool;
  CodeGenerationResult result;
  result.success = true;

  std::string text = tool.FormatResultAsText(result);
  EXPECT_THAT(text, HasSubstr("SUCCESS"));
}

TEST(CodeGenToolBaseTest, FormatResultAsTextContainsFailedStatus) {
  TestableCodeGenTool tool;
  CodeGenerationResult result;
  result.success = false;
  result.AddError("Something failed");

  std::string text = tool.FormatResultAsText(result);
  EXPECT_THAT(text, HasSubstr("FAILED"));
}

TEST(CodeGenToolBaseTest, FormatResultAsTextContainsGeneratedCode) {
  TestableCodeGenTool tool;
  CodeGenerationResult result;
  result.success = true;
  result.generated_code = "JSL MyRoutine\nRTL";

  std::string text = tool.FormatResultAsText(result);
  EXPECT_THAT(text, HasSubstr("Generated Code:"));
  EXPECT_THAT(text, HasSubstr("JSL MyRoutine"));
}

TEST(CodeGenToolBaseTest, FormatResultAsTextContainsSymbols) {
  TestableCodeGenTool tool;
  CodeGenerationResult result;
  result.success = true;
  result.symbols["MyLabel"] = 0x1F8000;

  std::string text = tool.FormatResultAsText(result);
  EXPECT_THAT(text, HasSubstr("Symbols:"));
  EXPECT_THAT(text, HasSubstr("MyLabel"));
}

// =============================================================================
// Template Content Tests
// =============================================================================

TEST(AsmTemplateTest, NmiHookTemplateHasRequiredParams) {
  TestableCodeGenTool tool;
  auto result = tool.GetTemplate("nmi_hook");
  ASSERT_TRUE(result.ok());

  EXPECT_THAT(result->required_params, Contains("LABEL"));
  EXPECT_THAT(result->required_params, Contains("NMI_HOOK_ADDRESS"));
  EXPECT_THAT(result->required_params, Contains("CUSTOM_CODE"));
}

TEST(AsmTemplateTest, SpriteTemplateHasRequiredParams) {
  TestableCodeGenTool tool;
  auto result = tool.GetTemplate("sprite");
  ASSERT_TRUE(result.ok());

  EXPECT_THAT(result->required_params, Contains("SPRITE_NAME"));
  EXPECT_THAT(result->required_params, Contains("INIT_CODE"));
  EXPECT_THAT(result->required_params, Contains("MAIN_CODE"));
}

TEST(AsmTemplateTest, JslHookTemplateHasRequiredParams) {
  TestableCodeGenTool tool;
  auto result = tool.GetTemplate("jsl_hook");
  ASSERT_TRUE(result.ok());

  EXPECT_THAT(result->required_params, Contains("HOOK_ADDRESS"));
  EXPECT_THAT(result->required_params, Contains("LABEL"));
}

TEST(AsmTemplateTest, EventHandlerTemplateHasRequiredParams) {
  TestableCodeGenTool tool;
  auto result = tool.GetTemplate("event_handler");
  ASSERT_TRUE(result.ok());

  EXPECT_THAT(result->required_params, Contains("EVENT_TYPE"));
  EXPECT_THAT(result->required_params, Contains("HOOK_ADDRESS"));
  EXPECT_THAT(result->required_params, Contains("LABEL"));
  EXPECT_THAT(result->required_params, Contains("CUSTOM_CODE"));
}

TEST(AsmTemplateTest, AllTemplatesHaveDescriptions) {
  TestableCodeGenTool tool;
  const auto& templates = tool.GetAllTemplates();

  for (const auto& tmpl : templates) {
    EXPECT_FALSE(tmpl.description.empty())
        << "Template '" << tmpl.name << "' has no description";
  }
}

TEST(AsmTemplateTest, AllTemplatesHaveCode) {
  TestableCodeGenTool tool;
  const auto& templates = tool.GetAllTemplates();

  for (const auto& tmpl : templates) {
    EXPECT_FALSE(tmpl.code_template.empty())
        << "Template '" << tmpl.name << "' has no code template";
  }
}

// =============================================================================
// Template Substitution Integration Tests
// =============================================================================

TEST(AsmTemplateTest, SpriteTemplateSubstitution) {
  TestableCodeGenTool tool;
  auto tmpl_result = tool.GetTemplate("sprite");
  ASSERT_TRUE(tmpl_result.ok());

  std::map<std::string, std::string> params = {
      {"SPRITE_NAME", "MyCustomSprite"},
      {"INIT_CODE", "LDA #$00 : STA $0F50, X"},
      {"MAIN_CODE", "JSR Sprite_Move"},
  };

  std::string code =
      tool.SubstitutePlaceholders(tmpl_result->code_template, params);

  EXPECT_THAT(code, HasSubstr("MyCustomSprite:"));
  EXPECT_THAT(code, HasSubstr("LDA #$00 : STA $0F50, X"));
  EXPECT_THAT(code, HasSubstr("JSR Sprite_Move"));
  // Should not have unsubstituted placeholders
  EXPECT_THAT(code, Not(HasSubstr("{{SPRITE_NAME}}")));
  EXPECT_THAT(code, Not(HasSubstr("{{INIT_CODE}}")));
  EXPECT_THAT(code, Not(HasSubstr("{{MAIN_CODE}}")));
}

TEST(AsmTemplateTest, JslHookTemplateSubstitution) {
  TestableCodeGenTool tool;
  auto tmpl_result = tool.GetTemplate("jsl_hook");
  ASSERT_TRUE(tmpl_result.ok());

  std::map<std::string, std::string> params = {
      {"HOOK_ADDRESS", "008040"},
      {"LABEL", "MyHook"},
      {"NOP_FILL", "NOP\n  NOP"},
  };

  std::string code =
      tool.SubstitutePlaceholders(tmpl_result->code_template, params);

  EXPECT_THAT(code, HasSubstr("org $008040"));
  EXPECT_THAT(code, HasSubstr("JSL MyHook"));
}

}  // namespace
}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
