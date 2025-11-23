#include "app/editor/message/message_data.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace yaze {
namespace editor {
namespace {

class MessageDataTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create some dummy message data
    messages_.push_back(MessageData(0, 0x100, "Test Message 1", {}, "Test Message 1", {}));
    messages_.push_back(MessageData(1, 0x200, "Test Message 2", {}, "Test Message 2", {}));
  }

  std::vector<MessageData> messages_;
};

TEST_F(MessageDataTest, SerializeMessagesToJson) {
  nlohmann::json j = SerializeMessagesToJson(messages_);

  ASSERT_TRUE(j.is_array());
  ASSERT_EQ(j.size(), 2);

  EXPECT_EQ(j[0]["id"], 0);
  EXPECT_EQ(j[0]["address"], 0x100);
  EXPECT_EQ(j[0]["raw_string"], "Test Message 1");
  EXPECT_EQ(j[0]["parsed_string"], "Test Message 1");

  EXPECT_EQ(j[1]["id"], 1);
  EXPECT_EQ(j[1]["address"], 0x200);
  EXPECT_EQ(j[1]["raw_string"], "Test Message 2");
  EXPECT_EQ(j[1]["parsed_string"], "Test Message 2");
}

TEST_F(MessageDataTest, ExportMessagesToJson) {
  std::string test_file = "test_messages.json";
  
  // Ensure file doesn't exist
  if (std::filesystem::exists(test_file)) {
    std::filesystem::remove(test_file);
  }

  absl::Status status = ExportMessagesToJson(test_file, messages_);
  ASSERT_TRUE(status.ok());

  ASSERT_TRUE(std::filesystem::exists(test_file));

  // Read back and verify
  std::ifstream file(test_file);
  nlohmann::json j;
  file >> j;

  ASSERT_TRUE(j.is_array());
  ASSERT_EQ(j.size(), 2);
  EXPECT_EQ(j[0]["raw_string"], "Test Message 1");

  // Cleanup
  std::filesystem::remove(test_file);
}

}  // namespace
}  // namespace editor
}  // namespace yaze
