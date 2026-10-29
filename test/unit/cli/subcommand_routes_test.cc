#include "cli/subcommand_routes.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace yaze::cli {
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;

absl::StatusOr<CommandInvocation> Rom(std::vector<std::string> args) {
  return ResolveRomSubcommand(args);
}

absl::StatusOr<CommandInvocation> Debug(std::vector<std::string> args) {
  return ResolveDebugSubcommand(args);
}

TEST(RomSubcommandTest, MapsSubcommandToRomCommand) {
  auto invocation = Rom({"read", "--address=0x10", "--length=4"});

  ASSERT_TRUE(invocation.ok()) << invocation.status();
  EXPECT_EQ(*invocation,
            (CommandInvocation{"rom-read", {"--address=0x10", "--length=4"}}));
}

TEST(RomSubcommandTest, RejectsUnknownOrMissingSubcommand) {
  EXPECT_EQ(Rom({"format"}).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(Rom({}).status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(RomSubcommandTest, UsageListsEverySubcommand) {
  const std::string usage = RomSubcommandUsage();
  for (absl::string_view name : RomSubcommandNames()) {
    EXPECT_THAT(usage, HasSubstr(absl::StrCat("z3ed rom ", name)));
  }
  EXPECT_THAT(RomSubcommandNames(), Contains("doctor"));
}

TEST(DebugSubcommandTest, DefaultsToMesenBackend) {
  auto invocation = Debug({"state"});

  ASSERT_TRUE(invocation.ok()) << invocation.status();
  EXPECT_EQ(*invocation, (CommandInvocation{"mesen-gamestate", {}}));
}

TEST(DebugSubcommandTest, BackendFlagMayAppearAnywhere) {
  auto trailing = Debug({"gamestate", "--backend=grpc"});
  ASSERT_TRUE(trailing.ok()) << trailing.status();
  EXPECT_EQ(*trailing, (CommandInvocation{"emulator-get-state", {}}));

  auto leading = Debug({"--backend", "grpc", "mem", "read", "--address=1"});
  ASSERT_TRUE(leading.ok()) << leading.status();
  EXPECT_EQ(*leading,
            (CommandInvocation{"emulator-read-memory", {"--address=1"}}));
}

TEST(DebugSubcommandTest, MemoryRequiresAction) {
  auto write = Debug({"memory", "write", "--value=0xFF"});
  ASSERT_TRUE(write.ok()) << write.status();
  EXPECT_EQ(*write,
            (CommandInvocation{"mesen-memory-write", {"--value=0xFF"}}));

  auto missing = Debug({"mem"});
  EXPECT_EQ(missing.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(missing.status().message(), HasSubstr("read, write"));

  EXPECT_EQ(Debug({"mem", "peek"}).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(DebugSubcommandTest, ResetAddsMesenActionOnlyForMesen) {
  auto mesen = Debug({"reset"});
  ASSERT_TRUE(mesen.ok()) << mesen.status();
  EXPECT_EQ(*mesen, (CommandInvocation{"mesen-control", {"--action=reset"}}));

  auto grpc = Debug({"reset", "--debug-backend=grpc"});
  ASSERT_TRUE(grpc.ok()) << grpc.status();
  EXPECT_EQ(*grpc, (CommandInvocation{"emulator-reset", {}}));
}

TEST(DebugSubcommandTest, AliasesResolve) {
  auto breakpoint = Debug({"bp", "--address=0x8000"});
  ASSERT_TRUE(breakpoint.ok()) << breakpoint.status();
  EXPECT_EQ(*breakpoint,
            (CommandInvocation{"mesen-breakpoint", {"--address=0x8000"}}));
}

TEST(DebugSubcommandTest, MesenOnlyTopicRejectsGrpc) {
  EXPECT_EQ(Debug({"sprites", "--backend=grpc"}).status().code(),
            absl::StatusCode::kFailedPrecondition);
}

TEST(DebugSubcommandTest, RejectsMalformedInput) {
  EXPECT_EQ(Debug({"--backend=grpc"}).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(Debug({"state", "--backend=snes9x"}).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(Debug({"state", "--backend"}).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(Debug({"teleport"}).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(DebugSubcommandTest, UsageCoversEveryTopic) {
  const std::string usage = DebugSubcommandUsage();
  for (const char* topic :
       {"state", "sprites", "cpu", "mem read", "mem write", "disasm", "trace",
        "breakpoint", "control", "reset"}) {
    EXPECT_THAT(usage, HasSubstr(absl::StrCat("z3ed debug ", topic)));
  }
}

}  // namespace
}  // namespace yaze::cli
