#include "cli/cli.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "cli/service/command_registry.h"

namespace yaze::cli {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;

constexpr char kTestCommand[] = "cli-dispatch-test";
constexpr char kTestCategory[] = "cli-dispatch-category";

std::vector<std::string>& LastPositionals() {
  static std::vector<std::string> positionals;
  return positionals;
}

class RecordingHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return kTestCommand; }
  std::string GetUsage() const override {
    return "cli-dispatch-test <words...>";
  }
  bool RequiresRom() const override { return false; }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser&) override {
    return absl::OkStatus();
  }
  absl::Status Execute(Rom*, const resources::ArgumentParser& parser,
                       resources::OutputFormatter&) override {
    LastPositionals() = parser.GetPositional();
    return absl::OkStatus();
  }
};

class CliTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    auto& registry = CommandRegistry::Instance();
    if (registry.HasCommand(kTestCommand)) {
      return;
    }
    CommandRegistry::CommandMetadata metadata;
    metadata.name = kTestCommand;
    metadata.category = kTestCategory;
    metadata.description = "Records dispatched arguments";
    metadata.usage = "cli-dispatch-test <words...>";
    metadata.available_to_agent = false;
    metadata.requires_rom = false;
    metadata.requires_grpc = false;
    registry.Register(std::make_unique<RecordingHandler>(), metadata);
  }

  Cli MakeCli(Cli::Options options = {}) {
    return Cli(CommandRegistry::Instance(), std::move(options), out_);
  }

  std::ostringstream out_;
};

TEST_F(CliTest, DispatchesRegisteredCommand) {
  LastPositionals().clear();

  absl::Status status = MakeCli().Run({kTestCommand, "alpha", "beta"});

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_THAT(LastPositionals(), ElementsAre("alpha", "beta"));
}

TEST_F(CliTest, UnknownCommandIsNotFound) {
  absl::Status status = MakeCli().Run({"definitely-not-a-command"});

  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
}

TEST_F(CliTest, CategoryNamePrintsCategoryHelp) {
  ASSERT_TRUE(MakeCli().Run({kTestCategory}).ok());

  EXPECT_THAT(out_.str(), HasSubstr(kTestCommand));
  EXPECT_THAT(out_.str(), HasSubstr("Records dispatched arguments"));
}

TEST_F(CliTest, EmptyCommandLinePrintsOverview) {
  ASSERT_TRUE(MakeCli().Run({}).ok());

  EXPECT_THAT(out_.str(), HasSubstr("USAGE:"));
  EXPECT_THAT(out_.str(), HasSubstr(kTestCategory));
}

TEST_F(CliTest, OverviewOmitsAnsiCodesWhenColorDisabled) {
  MakeCli().PrintOverview();

  EXPECT_THAT(out_.str(), Not(HasSubstr("\033[")));
}

TEST_F(CliTest, OverviewUsesAnsiCodesWhenColorEnabled) {
  Cli::Options options;
  options.style = util::AnsiStyle(true);

  MakeCli(std::move(options)).PrintOverview();

  EXPECT_THAT(out_.str(), HasSubstr("\033["));
}

TEST_F(CliTest, AliasGroupWithoutSubcommandPrintsUsage) {
  ASSERT_TRUE(MakeCli().Run({"rom"}).ok());

  EXPECT_THAT(out_.str(), HasSubstr("z3ed rom read"));
}

TEST_F(CliTest, UnknownAliasSubcommandPrintsUsageAndFails) {
  absl::Status status = MakeCli().Run({"debug", "teleport"});

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(out_.str(), HasSubstr("z3ed debug state"));
}

TEST_F(CliTest, AgentWithoutHandlerFails) {
  absl::Status status = MakeCli().Run({"agent", "simple-chat"});

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(CliTest, AgentForwardsArgumentsToHandler) {
  std::vector<std::string> received;
  Cli::Options options;
  options.agent_handler = [&received](const std::vector<std::string>& args) {
    received = args;
    return absl::OkStatus();
  };

  ASSERT_TRUE(
      MakeCli(std::move(options)).Run({"agent", "plan", "--prompt=x"}).ok());

  EXPECT_THAT(received, ElementsAre("plan", "--prompt=x"));
}

TEST_F(CliTest, HelpCommandMatchesScopedHelp) {
  ASSERT_TRUE(MakeCli().Run({"help", kTestCommand}).ok());

  EXPECT_THAT(out_.str(), HasSubstr("cli-dispatch-test <words...>"));
}

TEST_F(CliTest, ScopedHelpForUnknownTargetIsNotFound) {
  absl::Status status = MakeCli().PrintScopedHelp({"no-such-topic"});

  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
}

TEST_F(CliTest, ScopedHelpResolvesAliasSubcommand) {
  if (!CommandRegistry::Instance().HasCommand("mesen-memory-read")) {
    GTEST_SKIP() << "mesen-memory-read is not registered in this build";
  }

  ASSERT_TRUE(MakeCli().PrintScopedHelp({"debug", "mem", "read"}).ok());

  EXPECT_THAT(out_.str(), HasSubstr("mesen-memory-read"));
  EXPECT_THAT(out_.str(), Not(HasSubstr("z3ed debug state")));
}

TEST_F(CliTest, ScopedHelpForAliasGroupPrintsUsage) {
  ASSERT_TRUE(MakeCli().PrintScopedHelp({"debug"}).ok());

  EXPECT_THAT(out_.str(), HasSubstr("z3ed debug mem read"));
}

}  // namespace
}  // namespace yaze::cli
