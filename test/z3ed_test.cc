#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/zelda3/overworld.h"
#include "cli/command_handler.h"

using namespace yaze::cli;
using ::testing::_;
using ::testing::Return;
using yaze::app::zelda3::Overworld;

// Mock class for CommandHandler
class MockCommandHandler : public CommandHandler {
 public:
  MOCK_METHOD(absl::Status, handle, (const std::vector<std::string>& arg),
              (override));
};

// Test fixture class
class CommandHandlerTest : public ::testing::Test {
 protected:
  std::shared_ptr<MockCommandHandler> mockHandler =
      std::make_shared<MockCommandHandler>();
};

// TEST_F(CommandHandlerTest, TestApplyPatch) {
//   Commands cmd;
//   cmd.handlers["-a"] = mockHandler;
//   EXPECT_CALL(*mockHandler, handle(_)).WillOnce(Return(absl::OkStatus()));
//   absl::Status result = cmd.handlers["-a"]->handle("apply_patch_args");
//   EXPECT_EQ(result, absl::OkStatus());
// }

// TEST_F(CommandHandlerTest, TestCreatePatch) {
//   Commands cmd;
//   cmd.handlers["-cp"] = mockHandler;
//   EXPECT_CALL(*mockHandler, handle(_)).WillOnce(Return(absl::OkStatus()));
//   absl::Status result = cmd.handlers["-cp"]->handle("create_patch_args");
//   EXPECT_EQ(result, absl::OkStatus());
// }

// TEST_F(CommandHandlerTest, TestOpen) {
//   Commands cmd;
//   cmd.handlers["-o"] = mockHandler;
//   EXPECT_CALL(*mockHandler, handle(_)).WillOnce(Return(absl::OkStatus()));
//   absl::Status result = cmd.handlers["-o"]->handle("open_args");
//   EXPECT_EQ(result, absl::OkStatus());
// }

// TEST_F(CommandHandlerTest, TestBackup) {
//   Commands cmd;
//   cmd.handlers["-b"] = mockHandler;
//   EXPECT_CALL(*mockHandler, handle(_)).WillOnce(Return(absl::OkStatus()));
//   absl::Status result = cmd.handlers["-b"]->handle("backup_args");
//   EXPECT_EQ(result, absl::OkStatus());
// }

// TEST_F(CommandHandlerTest, TestCompress) {
//   Commands cmd;
//   cmd.handlers["-c"] = mockHandler;
//   EXPECT_CALL(*mockHandler, handle(_)).WillOnce(Return(absl::OkStatus()));
//   absl::Status result = cmd.handlers["-c"]->handle("compress_args");
//   EXPECT_EQ(result, absl::OkStatus());
// }

// TEST_F(CommandHandlerTest, TestDecompress) {
//   Commands cmd;
//   cmd.handlers["-d"] = mockHandler;
//   EXPECT_CALL(*mockHandler, handle(_)).WillOnce(Return(absl::OkStatus()));
//   absl::Status result = cmd.handlers["-d"]->handle("decompress_args");
//   EXPECT_EQ(result, absl::OkStatus());
// }

// TEST_F(CommandHandlerTest, TestSnesToPc) {
//   Commands cmd;
//   cmd.handlers["-s"] = mockHandler;
//   EXPECT_CALL(*mockHandler, handle(_)).WillOnce(Return(absl::OkStatus()));
//   absl::Status result = cmd.handlers["-s"]->handle("snes_to_pc_args");
//   EXPECT_EQ(result, absl::OkStatus());
// }

// TEST_F(CommandHandlerTest, TestPcToSnes) {
//   Commands cmd;
//   cmd.handlers["-p"] = mockHandler;
//   EXPECT_CALL(*mockHandler, handle(_)).WillOnce(Return(absl::OkStatus()));
//   absl::Status result = cmd.handlers["-p"]->handle("pc_to_snes_args");
//   EXPECT_EQ(result, absl::OkStatus());
// }
