#include "core/message.h"

#include <gtest/gtest.h>

namespace yaze {
namespace test {
namespace message_system {

using app::core::IMessageListener;
using app::core::IMessageProtocol;
using app::core::Message;
using app::core::MessageDispatcher;
using app::core::MessageFilter;

class TestListener : public IMessageListener {
 public:
  ~TestListener() = default;

  absl::Status OnMessageReceived(const Message& message) override {
    last_message_ = message;
    message_count_++;
    return absl::OkStatus();
  }

  const Message& last_message() const { return last_message_; }
  int message_count() const { return message_count_; }

 private:
  Message last_message_;
  int message_count_ = 0;
};

class TestProtocol : public IMessageProtocol {
 public:
  bool CanHandleMessage(const Message& message) const override {
    return message.type == "TestMessage";
  }
};

class TestFilter : public MessageFilter {
 public:
  bool ShouldReceiveMessage(const Message& message) const override {
    return std::any_cast<int>(message.payload) > 10;
  }
};

class MessageDispatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    message_count_ = 0;
    protocol_ = std::make_unique<TestProtocol>();
    filter_ = std::make_unique<TestFilter>();
  }

  void TearDown() override {
    protocol_.reset();
    filter_.reset();
  }

  int message_count_ = 0;
  int message_count1_ = 0;
  int message_count2_ = 0;
  TestListener listener1_;
  TestListener listener2_;
  std::unique_ptr<TestProtocol> protocol_;
  std::unique_ptr<TestFilter> filter_;
};
TEST_F(MessageDispatcherTest, RegisterAndSendMessage) {
  MessageDispatcher dispatcher;

  dispatcher.RegisterListener("TestMessage", &listener1_);

  Message message("TestMessage", nullptr, 42);
  dispatcher.SendMessage(message);

  EXPECT_EQ(listener1_.message_count(), 1);
  EXPECT_EQ(std::any_cast<int>(listener1_.last_message().payload), 42);
}

TEST_F(MessageDispatcherTest, UnregisterListener) {
  MessageDispatcher dispatcher;

  dispatcher.RegisterListener("TestMessage", &listener1_);
  dispatcher.UnregisterListener("TestMessage", &listener1_);

  Message message("TestMessage", nullptr, 42);
  dispatcher.SendMessage(message);

  EXPECT_EQ(listener1_.message_count(), 0);
}

TEST_F(MessageDispatcherTest, MultipleListeners) {
  MessageDispatcher dispatcher;

  dispatcher.RegisterListener("TestMessage", &listener1_);
  dispatcher.RegisterListener("TestMessage", &listener2_);

  Message message("TestMessage", nullptr, 42);
  dispatcher.SendMessage(message);

  EXPECT_EQ(listener1_.message_count(), 1);
  EXPECT_EQ(listener2_.message_count(), 1);
  EXPECT_EQ(std::any_cast<int>(listener1_.last_message().payload), 42);
  EXPECT_EQ(std::any_cast<int>(listener2_.last_message().payload), 42);
}

TEST_F(MessageDispatcherTest, FilteredMessageHandling) {
  MessageDispatcher dispatcher;

  dispatcher.RegisterFilteredListener("TestMessage", &listener1_,
                                      std::move(filter_));

  Message valid_message("TestMessage", nullptr, 15);
  dispatcher.DispatchMessage(valid_message);
  EXPECT_EQ(listener1_.message_count(), 1);

  Message invalid_message("TestMessage", nullptr, 5);
  dispatcher.DispatchMessage(invalid_message);
  EXPECT_EQ(listener1_.message_count(), 1);
}

}  // namespace message_system
}  // namespace test
}  // namespace yaze
