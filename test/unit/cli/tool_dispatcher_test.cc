#include "cli/service/agent/tool_dispatcher.h"

#include <map>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "cli/service/agent/tool_registry.h"
#include "cli/service/resources/command_handler.h"
#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze::cli::agent {
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;

class FailingIfReachedHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "failing-if-reached"; }
  std::string GetUsage() const override {
    return "failing-if-reached --name <value> [--verbose]";
  }
  bool RequiresRom() const override { return false; }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser&) override {
    return absl::InternalError("dispatcher should reject call before handler");
  }

  absl::Status Execute(Rom*, const resources::ArgumentParser&,
                       resources::OutputFormatter&) override {
    return absl::InternalError("dispatcher should reject call before execute");
  }
};

class FlagEchoHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "flag-echo"; }
  std::string GetUsage() const override {
    return "flag-echo --name <value> [--verbose] [--dry-run]";
  }
  bool RequiresRom() const override { return false; }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser&) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom*, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override {
    formatter.BeginObject("FlagEcho");
    formatter.AddField("name", parser.GetString("name").value_or(""));
    formatter.AddField("verbose", parser.HasFlag("verbose"));
    formatter.AddField("dry_run", parser.HasFlag("dry-run"));
    formatter.EndObject();
    return absl::OkStatus();
  }
};

class MutatingEchoHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "mutating-echo"; }
  std::string GetUsage() const override { return "mutating-echo"; }
  bool RequiresRom() const override { return false; }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser&) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom*, const resources::ArgumentParser&,
                       resources::OutputFormatter& formatter) override {
    formatter.BeginObject("MutatingEcho");
    formatter.AddField("executed", true);
    formatter.EndObject();
    return absl::OkStatus();
  }
};

ToolCall MakeToolCall(const std::string& name,
                      const std::map<std::string, std::string>& args = {}) {
  ToolCall call;
  call.tool_name = name;
  call.args = args;
  return call;
}

TEST(ToolDispatcherUnitTest, RegistryInitializesBuiltinToolsOnDemand) {
  EXPECT_TRUE(ToolRegistry::Get().GetToolDefinition("tools-list").has_value());
  EXPECT_TRUE(
      ToolRegistry::Get().GetToolDefinition("build-status").has_value());
}

TEST(ToolDispatcherUnitTest, SharedValidationRejectsMissingRequiredArgs) {
  ToolRegistry::Get().RegisterTool(
      {"test-required-arg",
       "test",
       "Checks required arg validation",
       "test-required-arg --name <value> [--verbose]",
       {},
       false,
       false,
       ToolAccess::kReadOnly,
       {},
       {}},
      []() { return std::make_unique<FailingIfReachedHandler>(); });

  ToolDispatcher dispatcher;
  auto result = dispatcher.Dispatch(MakeToolCall("test-required-arg"));

  ASSERT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsInvalidArgument(result.status()));
  EXPECT_THAT(result.status().message(), HasSubstr("--name"));
}

#ifdef YAZE_WITH_JSON
TEST(ToolDispatcherUnitTest, BooleanToolArgsAreConvertedIntoFlags) {
  ToolRegistry::Get().RegisterTool(
      {"test-flag-echo",
       "test",
       "Checks bool flag conversion",
       "test-flag-echo --name <value> [--verbose] [--dry-run]",
       {},
       false,
       false,
       ToolAccess::kReadOnly,
       {},
       {}},
      []() { return std::make_unique<FlagEchoHandler>(); });

  ToolDispatcher dispatcher;
  auto result = dispatcher.Dispatch(MakeToolCall(
      "test-flag-echo",
      {{"name", "demo"}, {"verbose", "true"}, {"dry-run", "false"}}));

  ASSERT_TRUE(result.ok()) << result.status().message();
  const auto parsed = nlohmann::json::parse(*result);
  ASSERT_TRUE(parsed.contains("FlagEcho"));
  const auto& payload = parsed["FlagEcho"];
  EXPECT_EQ(payload["name"], "demo");
  EXPECT_EQ(payload["verbose"], true);
  EXPECT_EQ(payload["dry_run"], false);
}
#endif

TEST(ToolDispatcherUnitTest, MutatingToolsRequireAuthorizationByDefault) {
  ToolRegistry::Get().RegisterTool(
      {"test-mutating-echo",
       "test",
       "Checks auth gating",
       "test-mutating-echo",
       {},
       false,
       false,
       ToolAccess::kMutating,
       {},
       {}},
      []() { return std::make_unique<MutatingEchoHandler>(); });

  ToolDispatcher dispatcher;
  auto result = dispatcher.Dispatch(MakeToolCall("test-mutating-echo"));

  ASSERT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsPermissionDenied(result.status()));
  EXPECT_THAT(result.status().message(), HasSubstr("not authorized"));
}

TEST(ToolDispatcherUnitTest, MutatingToolsRunWhenExplicitlyAuthorized) {
  ToolRegistry::Get().RegisterTool(
      {"test-mutating-echo-allowed",
       "test",
       "Checks auth gating",
       "test-mutating-echo-allowed",
       {},
       false,
       false,
       ToolAccess::kMutating,
       {},
       {}},
      []() { return std::make_unique<MutatingEchoHandler>(); });

  ToolDispatcher dispatcher;
  ToolDispatcher::ToolPreferences prefs;
  prefs.allow_mutating_tools = true;
  dispatcher.SetToolPreferences(prefs);

  auto result = dispatcher.Dispatch(MakeToolCall("test-mutating-echo-allowed"));

  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_THAT(*result, HasSubstr("executed"));
}

#ifdef YAZE_WITH_JSON
TEST(ToolDispatcherUnitTest,
     MetaToolsAreHandledByDispatcherAndExposeBuildTools) {
  ToolDispatcher dispatcher;

  auto list_result = dispatcher.Dispatch(MakeToolCall("tools-list"));
  ASSERT_TRUE(list_result.ok()) << list_result.status().message();

  const auto listed = nlohmann::json::parse(*list_result);
  ASSERT_TRUE(listed.contains("tools"));
  ASSERT_TRUE(listed["tools"].is_array());
  EXPECT_GE(listed["count"].get<size_t>(), 1u);

  bool saw_build_status = false;
  for (const auto& tool : listed["tools"]) {
    if (tool["name"] == "build-status") {
      saw_build_status = true;
      break;
    }
  }
  EXPECT_TRUE(saw_build_status);

  auto describe_result = dispatcher.Dispatch(
      MakeToolCall("tools-describe", {{"name", "build-configure"}}));
  ASSERT_TRUE(describe_result.ok()) << describe_result.status().message();

  const auto described = nlohmann::json::parse(*describe_result);
  EXPECT_EQ(described["name"], "build-configure");
  EXPECT_EQ(described["requires_authorization"], true);
  EXPECT_THAT(described["required_args"].get<std::vector<std::string>>(),
              Contains("preset"));
}
#endif

TEST(ToolDispatcherUnitTest, DefaultPreferencesDisallowMutatingTools) {
  ToolDispatcher dispatcher;
  EXPECT_FALSE(dispatcher.preferences().allow_mutating_tools);
}

}  // namespace
}  // namespace yaze::cli::agent
