#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/agent/tool_dispatcher.h"
#include "cli/service/agent/tool_registry.h"
#include "rom/rom.h"

namespace yaze::cli::agent {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;

constexpr char kInspectTool[] = "dungeon-edit-inspect";
constexpr char kApplyTool[] = "dungeon-edit-apply";
constexpr char kBatchJson[] =
    R"({"session_id":"session-7","expected_objects":[{"object_index":0,)"
    R"("object_id":34,"x_tile":10,"y_tile":12,"size":2,"stored_list":0}],)"
    R"("moves":[{"object_index":0,"x_tile":10,"y_tile":10}]})";

static_assert(
    std::is_same_v<
        decltype(&ConversationalAgentService::SetDungeonEditorToolCallback),
        void (ConversationalAgentService::*)(
            ToolDispatcher::DungeonEditorToolCallback)>);

ToolCall MakeToolCall(const std::string& name,
                      const std::map<std::string, std::string>& args = {}) {
  ToolCall call;
  call.tool_name = name;
  call.args = args;
  return call;
}

struct RecordedCall {
  ToolCall call;
  Rom* rom = nullptr;
};

class FakeDungeonEditorHost {
 public:
  ToolDispatcher::DungeonEditorToolCallback Callback() {
    return
        [this](const ToolCall& call, Rom* rom) -> absl::StatusOr<std::string> {
          calls.push_back({call, rom});
          return response;
        };
  }

  std::vector<RecordedCall> calls;
  absl::StatusOr<std::string> response = std::string(R"({"ok":true})");
};

class DungeonEditorToolDispatchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0x5A)).ok());
    rom_bytes_before_ = rom_.vector();
  }

  void ExpectRomUnchanged() const {
    EXPECT_EQ(rom_.vector(), rom_bytes_before_);
    EXPECT_FALSE(rom_.dirty());
  }

  Rom rom_;
  std::vector<uint8_t> rom_bytes_before_;
  FakeDungeonEditorHost host_;
};

TEST_F(DungeonEditorToolDispatchTest, ToolsAreDiscoverableWithAccessMetadata) {
  const auto inspect = ToolRegistry::Get().GetToolDefinition(kInspectTool);
  ASSERT_TRUE(inspect.has_value());
  EXPECT_EQ(inspect->category, "dungeon");
  EXPECT_EQ(inspect->access, ToolAccess::kReadOnly);
  EXPECT_TRUE(inspect->requires_rom);
  EXPECT_FALSE(inspect->requires_project);
  EXPECT_THAT(inspect->required_args, ElementsAre("room"));
  EXPECT_TRUE(inspect->flag_args.empty());

  const auto apply = ToolRegistry::Get().GetToolDefinition(kApplyTool);
  ASSERT_TRUE(apply.has_value());
  EXPECT_EQ(apply->category, "dungeon");
  EXPECT_EQ(apply->access, ToolAccess::kMutating);
  EXPECT_TRUE(apply->requires_rom);
  EXPECT_FALSE(apply->requires_project);
  EXPECT_THAT(apply->required_args, UnorderedElementsAre("room", "batch"));
  EXPECT_TRUE(apply->flag_args.empty());

  ToolDispatcher dispatcher;
  const auto inspect_info = dispatcher.GetToolInfo(kInspectTool);
  ASSERT_TRUE(inspect_info.has_value());
  EXPECT_FALSE(inspect_info->requires_authorization);
  const auto apply_info = dispatcher.GetToolInfo(kApplyTool);
  ASSERT_TRUE(apply_info.has_value());
  EXPECT_TRUE(apply_info->requires_authorization);

  std::vector<std::string> matched;
  for (const auto& info : dispatcher.SearchTools("dungeon-edit-")) {
    matched.push_back(info.name);
  }
  EXPECT_THAT(matched, Contains(kInspectTool));
  EXPECT_THAT(matched, Contains(kApplyTool));
}

TEST_F(DungeonEditorToolDispatchTest, MissingRomContextFailsBeforeCallback) {
  ToolDispatcher dispatcher;
  dispatcher.SetDungeonEditorToolCallback(host_.Callback());

  auto result =
      dispatcher.Dispatch(MakeToolCall(kInspectTool, {{"room", "37"}}));

  ASSERT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(result.status()));
  EXPECT_THAT(result.status().message(), HasSubstr("ROM context"));
  EXPECT_TRUE(host_.calls.empty());
}

TEST_F(DungeonEditorToolDispatchTest, UnloadedRomContextFailsBeforeCallback) {
  Rom unloaded;
  ToolDispatcher dispatcher;
  dispatcher.SetRomContext(&unloaded);
  dispatcher.SetDungeonEditorToolCallback(host_.Callback());

  auto result =
      dispatcher.Dispatch(MakeToolCall(kInspectTool, {{"room", "37"}}));

  ASSERT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(result.status()));
  EXPECT_TRUE(host_.calls.empty());
}

TEST_F(DungeonEditorToolDispatchTest,
       MissingHostCallbackFailsWithoutCliFallback) {
  ToolDispatcher dispatcher;
  dispatcher.SetRomContext(&rom_);
  ToolDispatcher::ToolPreferences prefs;
  prefs.allow_mutating_tools = true;
  dispatcher.SetToolPreferences(prefs);

  auto inspect =
      dispatcher.Dispatch(MakeToolCall(kInspectTool, {{"room", "37"}}));
  ASSERT_FALSE(inspect.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(inspect.status()));
  EXPECT_THAT(inspect.status().message(), HasSubstr("native yaze dungeon"));

  auto apply = dispatcher.Dispatch(
      MakeToolCall(kApplyTool, {{"room", "37"}, {"batch", kBatchJson}}));
  ASSERT_FALSE(apply.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(apply.status()));
  EXPECT_THAT(apply.status().message(), HasSubstr("native yaze dungeon"));

  ExpectRomUnchanged();
}

TEST_F(DungeonEditorToolDispatchTest, DisabledDungeonCategoryBlocksBothTools) {
  ToolDispatcher dispatcher;
  dispatcher.SetRomContext(&rom_);
  dispatcher.SetDungeonEditorToolCallback(host_.Callback());
  ToolDispatcher::ToolPreferences prefs;
  prefs.dungeon = false;
  prefs.allow_mutating_tools = true;
  dispatcher.SetToolPreferences(prefs);

  auto inspect =
      dispatcher.Dispatch(MakeToolCall(kInspectTool, {{"room", "37"}}));
  ASSERT_FALSE(inspect.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(inspect.status()));
  EXPECT_THAT(inspect.status().message(), HasSubstr("disabled"));

  auto apply = dispatcher.Dispatch(
      MakeToolCall(kApplyTool, {{"room", "37"}, {"batch", kBatchJson}}));
  ASSERT_FALSE(apply.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(apply.status()));
  EXPECT_THAT(apply.status().message(), HasSubstr("disabled"));

  EXPECT_TRUE(dispatcher.SearchTools("dungeon-edit-").empty());
  EXPECT_TRUE(host_.calls.empty());
}

TEST_F(DungeonEditorToolDispatchTest, UnauthorizedApplyNeverReachesCallback) {
  ToolDispatcher dispatcher;
  dispatcher.SetRomContext(&rom_);
  dispatcher.SetDungeonEditorToolCallback(host_.Callback());

  auto result = dispatcher.Dispatch(
      MakeToolCall(kApplyTool, {{"room", "37"}, {"batch", kBatchJson}}));

  ASSERT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsPermissionDenied(result.status()));
  EXPECT_THAT(result.status().message(), HasSubstr("not authorized"));
  EXPECT_TRUE(host_.calls.empty());
  ExpectRomUnchanged();
}

TEST_F(DungeonEditorToolDispatchTest, ReadOnlyInspectRunsWithoutAuthorization) {
  ToolDispatcher dispatcher;
  dispatcher.SetRomContext(&rom_);
  dispatcher.SetDungeonEditorToolCallback(host_.Callback());
  host_.response =
      std::string(R"({"session_id":"session-7","room_id":37,"objects":[],)"
                  R"("rom_saved":false})");

  auto result =
      dispatcher.Dispatch(MakeToolCall(kInspectTool, {{"room", "37"}}));

  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_EQ(*result, *host_.response);
  ASSERT_EQ(host_.calls.size(), 1u);
  EXPECT_EQ(host_.calls[0].call.tool_name, kInspectTool);
  EXPECT_EQ(host_.calls[0].call.args,
            (std::map<std::string, std::string>{{"room", "37"}}));
  EXPECT_EQ(host_.calls[0].rom, &rom_);
  ExpectRomUnchanged();
}

TEST_F(DungeonEditorToolDispatchTest,
       AuthorizedApplyPassesExactBatchAndActiveRom) {
  ToolDispatcher dispatcher;
  dispatcher.SetRomContext(&rom_);
  dispatcher.SetDungeonEditorToolCallback(host_.Callback());
  ToolDispatcher::ToolPreferences prefs;
  prefs.allow_mutating_tools = true;
  dispatcher.SetToolPreferences(prefs);
  host_.response = std::string(
      R"({"session_id":"session-7","room_id":37,"changed_object_count":1,)"
      R"("rendered":true,"undo_recorded":true,"rom_saved":false})");

  auto result = dispatcher.Dispatch(
      MakeToolCall(kApplyTool, {{"room", "37"}, {"batch", kBatchJson}}));

  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_EQ(*result, *host_.response);
  ASSERT_EQ(host_.calls.size(), 1u);
  EXPECT_EQ(host_.calls[0].call.tool_name, kApplyTool);
  EXPECT_EQ(host_.calls[0].call.args,
            (std::map<std::string, std::string>{{"room", "37"},
                                                {"batch", kBatchJson}}));
  EXPECT_EQ(host_.calls[0].rom, &rom_);
  ExpectRomUnchanged();
}

TEST_F(DungeonEditorToolDispatchTest, MissingBatchArgumentIsRejected) {
  ToolDispatcher dispatcher;
  dispatcher.SetRomContext(&rom_);
  dispatcher.SetDungeonEditorToolCallback(host_.Callback());
  ToolDispatcher::ToolPreferences prefs;
  prefs.allow_mutating_tools = true;
  dispatcher.SetToolPreferences(prefs);

  auto result = dispatcher.Dispatch(MakeToolCall(kApplyTool, {{"room", "37"}}));

  ASSERT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsInvalidArgument(result.status()));
  EXPECT_THAT(result.status().message(), HasSubstr("--batch"));
  EXPECT_TRUE(host_.calls.empty());
}

TEST_F(DungeonEditorToolDispatchTest, UnsupportedArgumentsAreRejected) {
  ToolDispatcher dispatcher;
  dispatcher.SetRomContext(&rom_);
  dispatcher.SetDungeonEditorToolCallback(host_.Callback());
  ToolDispatcher::ToolPreferences prefs;
  prefs.allow_mutating_tools = true;
  dispatcher.SetToolPreferences(prefs);

  auto inspect = dispatcher.Dispatch(
      MakeToolCall(kInspectTool, {{"room", "37"}, {"batch", kBatchJson}}));
  ASSERT_FALSE(inspect.ok());
  EXPECT_TRUE(absl::IsInvalidArgument(inspect.status()));
  EXPECT_THAT(inspect.status().message(), HasSubstr("--batch"));

  auto apply = dispatcher.Dispatch(MakeToolCall(
      kApplyTool, {{"room", "37"}, {"batch", kBatchJson}, {"write", "true"}}));
  ASSERT_FALSE(apply.ok());
  EXPECT_TRUE(absl::IsInvalidArgument(apply.status()));
  EXPECT_THAT(apply.status().message(), HasSubstr("--write"));

  EXPECT_TRUE(host_.calls.empty());
  ExpectRomUnchanged();
}

TEST_F(DungeonEditorToolDispatchTest, HostCallbackErrorsPropagate) {
  ToolDispatcher dispatcher;
  dispatcher.SetRomContext(&rom_);
  dispatcher.SetDungeonEditorToolCallback(host_.Callback());
  ToolDispatcher::ToolPreferences prefs;
  prefs.allow_mutating_tools = true;
  dispatcher.SetToolPreferences(prefs);
  host_.response = absl::AbortedError("stale object guard at index 0");

  auto result = dispatcher.Dispatch(
      MakeToolCall(kApplyTool, {{"room", "37"}, {"batch", kBatchJson}}));

  ASSERT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsAborted(result.status()));
  EXPECT_THAT(result.status().message(), HasSubstr("stale object guard"));
  EXPECT_EQ(host_.calls.size(), 1u);
  ExpectRomUnchanged();
}

}  // namespace
}  // namespace yaze::cli::agent
