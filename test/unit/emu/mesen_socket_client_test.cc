#include "app/emu/mesen/mesen_socket_client.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#ifdef _WIN32

namespace yaze::emu::mesen {
namespace {

TEST(MesenSocketClientTest, SubscribeDispatchesFrameEvents) {
  GTEST_SKIP() << "Unix socket integration test is not supported on Windows";
}

}  // namespace
}  // namespace yaze::emu::mesen

#else

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace yaze::emu::mesen {
namespace {

class FakeMesenSocketServer {
 public:
  FakeMesenSocketServer() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    socket_path_ = (std::filesystem::temp_directory_path() /
                    ("yaze-mesen-socket-test-" + std::to_string(now) + ".sock"))
                       .string();

    listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
      throw std::runtime_error("failed to create listen socket");
    }

    ::sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    const auto bind_result = bind(
        listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (bind_result != 0) {
      throw std::runtime_error("failed to bind fake mesen socket");
    }
    if (listen(listen_fd_, 4) != 0) {
      throw std::runtime_error("failed to listen on fake mesen socket");
    }
  }

  ~FakeMesenSocketServer() { Stop(); }

  void Start() { server_thread_ = std::thread(&FakeMesenSocketServer::Run, this); }

  void Stop() {
    running_ = false;

    ShutdownAndClose(event_client_fd_);
    event_client_fd_ = -1;
    ShutdownAndClose(command_client_fd_);
    command_client_fd_ = -1;
    ShutdownAndClose(listen_fd_);
    listen_fd_ = -1;

    if (server_thread_.joinable()) {
      server_thread_.join();
    }
    std::error_code ec;
    std::filesystem::remove(socket_path_, ec);
  }

  const std::string& socket_path() const { return socket_path_; }

  std::string error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return error_;
  }

 private:
  static void ShutdownAndClose(int fd) {
    if (fd < 0) {
      return;
    }
    shutdown(fd, SHUT_RDWR);
    close(fd);
  }

  static bool ReadLine(int fd, std::string* out) {
    if (fd < 0 || out == nullptr) {
      return false;
    }

    std::string line;
    char c = '\0';
    while (true) {
      const ssize_t n = recv(fd, &c, 1, 0);
      if (n <= 0) {
        return false;
      }
      if (c == '\n') {
        break;
      }
      line.push_back(c);
      if (line.size() > 8192) {
        return false;
      }
    }
    *out = line;
    return true;
  }

  static bool SendLine(int fd, const std::string& line) {
    if (fd < 0) {
      return false;
    }
    const std::string with_newline = line + "\n";
    return send(fd, with_newline.c_str(), with_newline.size(), 0) ==
           static_cast<ssize_t>(with_newline.size());
  }

  void SetError(const std::string& error) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    if (error_.empty()) {
      error_ = error;
    }
  }

  void Run() {
    running_ = true;

    command_client_fd_ = accept(listen_fd_, nullptr, nullptr);
    if (command_client_fd_ < 0) {
      SetError("failed to accept command socket");
      return;
    }

    std::string command_line;
    if (!ReadLine(command_client_fd_, &command_line)) {
      SetError("failed to read ping command");
      return;
    }
    if (command_line.find("\"type\":\"PING\"") == std::string::npos) {
      SetError("first command was not PING");
      return;
    }
    if (!SendLine(command_client_fd_, "{\"success\":true,\"data\":{\"pong\":true}}")) {
      SetError("failed to send ping response");
      return;
    }

    event_client_fd_ = accept(listen_fd_, nullptr, nullptr);
    if (event_client_fd_ < 0) {
      SetError("failed to accept event socket");
      return;
    }

    std::string subscribe_line;
    if (!ReadLine(event_client_fd_, &subscribe_line)) {
      SetError("failed to read subscribe command");
      return;
    }
    if (subscribe_line.find("\"type\":\"SUBSCRIBE\"") == std::string::npos) {
      SetError("second socket did not send SUBSCRIBE");
      return;
    }

    if (!SendLine(event_client_fd_, "{\"success\":true,\"data\":{\"subscribed\":true}}")) {
      SetError("failed to send subscribe ack");
      return;
    }
    if (!SendLine(event_client_fd_,
                  "{\"event\":\"frame_complete\",\"frame\":42,\"address\":\"0x008000\"}")) {
      SetError("failed to send event payload");
      return;
    }

    while (running_) {
      char buf[32];
      const ssize_t n = recv(event_client_fd_, buf, sizeof(buf), 0);
      if (n <= 0) {
        break;
      }
    }
  }

  std::string socket_path_;
  int listen_fd_ = -1;
  int command_client_fd_ = -1;
  int event_client_fd_ = -1;
  std::thread server_thread_;
  std::atomic<bool> running_{false};

  mutable std::mutex error_mutex_;
  std::string error_;
};

TEST(MesenSocketClientTest, SubscribeDispatchesFrameEvents) {
  FakeMesenSocketServer server;
  server.Start();

  MesenSocketClient client;
  ASSERT_TRUE(client.Connect(server.socket_path()).ok());

  std::mutex callback_mutex;
  std::condition_variable callback_cv;
  bool got_event = false;
  MesenEvent captured_event;

  client.SetEventCallback([&](const MesenEvent& event) {
    std::lock_guard<std::mutex> lock(callback_mutex);
    captured_event = event;
    got_event = true;
    callback_cv.notify_one();
  });

  ASSERT_TRUE(client.Subscribe({"frame_complete"}).ok());

  {
    std::unique_lock<std::mutex> lock(callback_mutex);
    ASSERT_TRUE(callback_cv.wait_for(lock, std::chrono::seconds(2),
                                     [&]() { return got_event; }));
  }

  EXPECT_EQ(captured_event.type, "frame_complete");
  EXPECT_EQ(captured_event.frame, 42u);
  EXPECT_EQ(captured_event.address, 0x008000u);

  EXPECT_TRUE(client.Unsubscribe().ok());
  client.Disconnect();
  server.Stop();
  EXPECT_TRUE(server.error().empty()) << server.error();
}

}  // namespace
}  // namespace yaze::emu::mesen

#endif
