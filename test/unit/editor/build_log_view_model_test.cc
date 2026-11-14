#include "app/editor/system/session/build_log_view_model.h"

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

constexpr char kOracleLog[] =
    "Using project ROM\n"
    "[*] Validating custom collision source contract...\n"
    "[*] Generating water-gate runtime tables from: /tmp/proj/rom\n"
    "  wrote 12 tables\r\n"
    "---  Music  ---\n"
    "Core/credits_text.asm:59: warning: (Wfeature_deprecated): warnpc\n"
    "---  Overworld  ---\n"
    "[*] Checking ZScream overlap...\n"
    "[+] No unowned mapped emission starts in ZScream reserved regions.\n"
    "[-] analysis: unavailable (Analyzer missing; required=0).\n"
    "\n"
    "Build receipt: /tmp/proj/Roms/oos168x.build.json\n";

TEST(BuildLogViewModelTest, GroupsOracleBuildLogIntoSteps) {
  const BuildLogSummary summary = ParseBuildLog(kOracleLog);

  ASSERT_EQ(summary.steps.size(), 6u);
  EXPECT_EQ(summary.steps[0].title, "Output");
  ASSERT_EQ(summary.steps[0].lines.size(), 1u);
  EXPECT_EQ(summary.steps[0].lines[0].text, "Using project ROM");

  EXPECT_EQ(summary.steps[1].title,
            "Validating custom collision source contract...");
  EXPECT_TRUE(summary.steps[1].lines.empty());

  ASSERT_EQ(summary.steps[2].lines.size(), 1u);
  EXPECT_EQ(summary.steps[2].lines[0].text, "  wrote 12 tables");
  EXPECT_EQ(summary.steps[2].severity, BuildLogSeverity::kInfo);

  EXPECT_EQ(summary.steps[3].title, "Music");
  EXPECT_EQ(summary.steps[3].severity, BuildLogSeverity::kWarning);
  ASSERT_EQ(summary.steps[3].lines.size(), 1u);
  EXPECT_EQ(summary.steps[3].lines[0].location, "Core/credits_text.asm:59");
  EXPECT_EQ(summary.steps[3].lines[0].text, "(Wfeature_deprecated): warnpc");

  EXPECT_EQ(summary.steps[4].title, "Overworld");
  EXPECT_TRUE(summary.steps[4].lines.empty());

  const BuildLogStep& overlap = summary.steps[5];
  EXPECT_EQ(overlap.title, "Checking ZScream overlap...");
  ASSERT_EQ(overlap.lines.size(), 2u);
  EXPECT_EQ(overlap.lines[0].severity, BuildLogSeverity::kSuccess);
  EXPECT_EQ(overlap.lines[1].severity, BuildLogSeverity::kSkipped);
  EXPECT_EQ(overlap.lines[1].text,
            "analysis: unavailable (Analyzer missing; required=0).");
  EXPECT_EQ(overlap.severity, BuildLogSeverity::kSkipped);

  EXPECT_EQ(summary.passed, 1);
  EXPECT_EQ(summary.warnings, 1);
  EXPECT_EQ(summary.errors, 0);
  EXPECT_EQ(summary.skipped, 1);
  EXPECT_EQ(summary.receipt_path, "/tmp/proj/Roms/oos168x.build.json");
}

TEST(BuildLogViewModelTest, ClassifiesErrorsAndKeepsPlainColonsAsDetail) {
  const BuildLogSummary summary = ParseBuildLog(
      "[*] Assembling\n"
      "Oracle_main.asm:12:4: error: label not found\n"
      "error: assembler exited 1\n"
      "[!] assembly failed\n"
      "Warning: legacy flag file\n"
      "time: 12:30: done\n");

  ASSERT_EQ(summary.steps.size(), 1u);
  const BuildLogStep& step = summary.steps[0];
  EXPECT_EQ(step.severity, BuildLogSeverity::kError);
  ASSERT_EQ(step.lines.size(), 5u);
  EXPECT_EQ(step.lines[0].location, "Oracle_main.asm:12:4");
  EXPECT_EQ(step.lines[0].text, "label not found");
  EXPECT_EQ(step.lines[1].severity, BuildLogSeverity::kError);
  EXPECT_EQ(step.lines[2].severity, BuildLogSeverity::kError);
  EXPECT_EQ(step.lines[2].text, "assembly failed");
  EXPECT_EQ(step.lines[3].severity, BuildLogSeverity::kWarning);
  EXPECT_EQ(step.lines[4].severity, BuildLogSeverity::kInfo);
  EXPECT_EQ(summary.errors, 3);
  EXPECT_EQ(summary.warnings, 1);
  EXPECT_TRUE(summary.receipt_path.empty());
}

TEST(BuildLogViewModelTest, KeepsDriveLetterPathsInDiagnostics) {
  const BuildLogSummary summary = ParseBuildLog(
      "[*] Assembling\n"
      "C:\\project\\file.asm:59: warning: warnpc\n"
      "C:\\project\\file.asm:12:4: error: label not found\n"
      "D:/Oracle of Secrets/Core/main.asm:7: fatal error: missing include\n"
      "C:\\project\\file.asm: warning: no line number\n");

  ASSERT_EQ(summary.steps.size(), 1u);
  const BuildLogStep& step = summary.steps[0];
  EXPECT_EQ(step.severity, BuildLogSeverity::kError);
  ASSERT_EQ(step.lines.size(), 4u);
  EXPECT_EQ(step.lines[0].severity, BuildLogSeverity::kWarning);
  EXPECT_EQ(step.lines[0].location, "C:\\project\\file.asm:59");
  EXPECT_EQ(step.lines[0].text, "warnpc");
  EXPECT_EQ(step.lines[1].severity, BuildLogSeverity::kError);
  EXPECT_EQ(step.lines[1].location, "C:\\project\\file.asm:12:4");
  EXPECT_EQ(step.lines[1].text, "label not found");
  EXPECT_EQ(step.lines[2].severity, BuildLogSeverity::kError);
  EXPECT_EQ(step.lines[2].location, "D:/Oracle of Secrets/Core/main.asm:7");
  EXPECT_EQ(step.lines[3].severity, BuildLogSeverity::kInfo);
  EXPECT_TRUE(step.lines[3].location.empty());
  EXPECT_EQ(summary.warnings, 1);
  EXPECT_EQ(summary.errors, 2);
}

TEST(BuildLogViewModelTest, EmptyLogHasNoSteps) {
  const BuildLogSummary summary = ParseBuildLog("\n\n");
  EXPECT_TRUE(summary.steps.empty());
  EXPECT_EQ(summary.passed + summary.warnings + summary.errors, 0);
}

#if defined(YAZE_WITH_JSON)
TEST(BuildLogViewModelTest, ReadsBuildReceiptChecks) {
  const auto receipt = ParseBuildReceipt(R"({
    "state": "completed_with_gaps",
    "finish": {"exit_code": 0, "required_gaps": [],
               "optional_gaps": ["analysis"]},
    "checks": [
      {"name": "assembly", "status": "passed", "required": true,
       "detail": "Command completed successfully"},
      {"name": "analysis", "status": "unavailable", "required": false,
       "detail": "Analyzer missing"},
      {"name": 7, "status": null, "required": "yes"}
    ]
  })");

  ASSERT_TRUE(receipt.ok()) << receipt.status();
  EXPECT_EQ(receipt->state, "completed_with_gaps");
  EXPECT_EQ(receipt->exit_code, 0);
  EXPECT_TRUE(receipt->required_gaps.empty());
  ASSERT_EQ(receipt->optional_gaps.size(), 1u);
  ASSERT_EQ(receipt->checks.size(), 3u);
  EXPECT_EQ(receipt->checks[0].name, "assembly");
  EXPECT_TRUE(receipt->checks[0].required);
  EXPECT_EQ(receipt->checks[1].status, "unavailable");
  EXPECT_FALSE(receipt->checks[1].required);
  EXPECT_EQ(receipt->checks[2].name, "");
  EXPECT_FALSE(receipt->checks[2].required);
}

TEST(BuildLogViewModelTest, RejectsNonReceiptJson) {
  EXPECT_FALSE(ParseBuildReceipt("{\"state\": \"done\"}").ok());
  EXPECT_FALSE(ParseBuildReceipt("not json").ok());
}
#endif

}  // namespace
}  // namespace yaze::editor
