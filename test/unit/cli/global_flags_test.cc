#include "cli/global_flags.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/flags/reflection.h"
#include "absl/status/status.h"

ABSL_DECLARE_FLAG(std::string, rom);
ABSL_DECLARE_FLAG(bool, quiet);
ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, openai_base_url);
ABSL_DECLARE_FLAG(bool, use_function_calling);

namespace yaze::cli {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

class GlobalFlagsTest : public ::testing::Test {
 protected:
  static absl::StatusOr<GlobalOptions> Parse(std::vector<std::string> args) {
    return ParseGlobalFlags(args);
  }

 private:
  absl::FlagSaver flag_saver_;
};

TEST_F(GlobalFlagsTest, ForwardsCommandAndCommandSpecificFlags) {
  const std::string rom_before = absl::GetFlag(FLAGS_rom);

  auto options = Parse({"rom-diff", "--rom_a=a.sfc", "--format=json"});

  ASSERT_TRUE(options.ok()) << options.status();
  EXPECT_THAT(options->positional,
              ElementsAre("rom-diff", "--rom_a=a.sfc", "--format=json"));
  EXPECT_EQ(absl::GetFlag(FLAGS_rom), rom_before);
}

TEST_F(GlobalFlagsTest, ConsumesGlobalFlagsAnywhere) {
  auto options =
      Parse({"--rom=base.sfc", "dungeon-list", "--ai-provider", "mock", "-q"});

  ASSERT_TRUE(options.ok()) << options.status();
  EXPECT_THAT(options->positional, ElementsAre("dungeon-list"));
  EXPECT_EQ(absl::GetFlag(FLAGS_rom), "base.sfc");
  EXPECT_EQ(absl::GetFlag(FLAGS_ai_provider), "mock");
  EXPECT_TRUE(absl::GetFlag(FLAGS_quiet));
}

TEST_F(GlobalFlagsTest, DashAndUnderscoreSpellingsAreEquivalent) {
  auto options = Parse({"--ai_provider=gemini", "--gemini-api-key", "key"});

  ASSERT_TRUE(options.ok()) << options.status();
  EXPECT_THAT(options->positional, IsEmpty());
  EXPECT_EQ(absl::GetFlag(FLAGS_ai_provider), "gemini");
  EXPECT_EQ(absl::GetFlag(FLAGS_gemini_api_key), "key");
}

TEST_F(GlobalFlagsTest, DocumentedOpenAiBaseUrlIsGlobal) {
  auto options =
      Parse({"agent", "simple-chat", "--openai-base-url=http://localhost:1"});

  ASSERT_TRUE(options.ok()) << options.status();
  EXPECT_THAT(options->positional, ElementsAre("agent", "simple-chat"));
  EXPECT_EQ(absl::GetFlag(FLAGS_openai_base_url), "http://localhost:1");
}

TEST_F(GlobalFlagsTest, BareBoolFlagTakesOnlyALiteralValue) {
  auto explicit_value = Parse({"--use_function_calling", "false", "agent"});
  ASSERT_TRUE(explicit_value.ok()) << explicit_value.status();
  EXPECT_FALSE(absl::GetFlag(FLAGS_use_function_calling));
  EXPECT_THAT(explicit_value->positional, ElementsAre("agent"));

  auto bare = Parse({"--use-function-calling", "agent"});
  ASSERT_TRUE(bare.ok()) << bare.status();
  EXPECT_TRUE(absl::GetFlag(FLAGS_use_function_calling));
  EXPECT_THAT(bare->positional, ElementsAre("agent"));
}

TEST_F(GlobalFlagsTest, RejectsInvalidValues) {
  auto options = Parse({"--quiet=maybe"});

  EXPECT_EQ(options.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(options.status().message(), HasSubstr("--quiet"));
}

TEST_F(GlobalFlagsTest, RequiresValueForStringFlag) {
  auto options = Parse({"rom-info", "--rom"});

  EXPECT_EQ(options.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(options.status().message(), "--rom flag requires a value");
}

TEST_F(GlobalFlagsTest, DoubleDashForwardsEverythingAfterIt) {
  const std::string rom_before = absl::GetFlag(FLAGS_rom);

  auto options = Parse({"rom-info", "--", "--rom=x.sfc", "--help"});

  ASSERT_TRUE(options.ok()) << options.status();
  EXPECT_THAT(options->positional,
              ElementsAre("rom-info", "--rom=x.sfc", "--help"));
  EXPECT_FALSE(options->show_help);
  EXPECT_EQ(absl::GetFlag(FLAGS_rom), rom_before);
}

TEST_F(GlobalFlagsTest, ParsesHelpForms) {
  auto bare = Parse({"rom-info", "-h"});
  ASSERT_TRUE(bare.ok());
  EXPECT_TRUE(bare->show_help);
  EXPECT_THAT(bare->positional, ElementsAre("rom-info"));

  auto targeted = Parse({"--help", "dungeon"});
  ASSERT_TRUE(targeted.ok());
  EXPECT_EQ(targeted->help_target, "dungeon");
  EXPECT_FALSE(targeted->show_help);

  auto assigned = Parse({"--help=rom-read"});
  ASSERT_TRUE(assigned.ok());
  EXPECT_EQ(assigned->help_target, "rom-read");

  auto followed_by_flag = Parse({"--help", "--rom=x.sfc"});
  ASSERT_TRUE(followed_by_flag.ok());
  EXPECT_TRUE(followed_by_flag->show_help);
  EXPECT_FALSE(followed_by_flag->help_target.has_value());
}

TEST_F(GlobalFlagsTest, ParsesActionFlags) {
  auto options = Parse({"-v", "--list", "--export_schemas", "--selftest"});

  ASSERT_TRUE(options.ok());
  EXPECT_TRUE(options->show_version);
  EXPECT_TRUE(options->list_commands);
  EXPECT_TRUE(options->export_schemas);
  EXPECT_TRUE(options->self_test);
  EXPECT_THAT(options->positional, IsEmpty());
}

TEST_F(GlobalFlagsTest, RejectsRemovedTuiFlag) {
  EXPECT_EQ(Parse({"--tui"}).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(GlobalFlagsTest, ForwardsGlobalFlagThatIsNotLinked) {
  if (absl::FindCommandLineFlag("http_port") != nullptr) {
    GTEST_SKIP() << "HTTP API flags are linked into this binary";
  }

  auto options = Parse({"--http-port=8080", "rom-info"});

  ASSERT_TRUE(options.ok()) << options.status();
  EXPECT_THAT(options->positional, ElementsAre("--http-port=8080", "rom-info"));
}

}  // namespace
}  // namespace yaze::cli
