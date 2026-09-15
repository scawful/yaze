#include "app/emu/mesen/mesen_socket_client.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32

namespace yaze::emu::mesen {
namespace {

TEST(MesenSocketClientTest, SubscribeDispatchesFrameEvents) {
  GTEST_SKIP() << "Unix socket integration test is not supported on Windows";
}

}  // namespace
}  // namespace yaze::emu::mesen

#else

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>

namespace yaze::emu::mesen {
namespace {

class FakeMesenSocketServer {
 public:
  FakeMesenSocketServer() {
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();
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

    const auto bind_result =
        bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (bind_result != 0) {
      throw std::runtime_error("failed to bind fake mesen socket");
    }
    if (listen(listen_fd_, 4) != 0) {
      throw std::runtime_error("failed to listen on fake mesen socket");
    }
  }

  ~FakeMesenSocketServer() { Stop(); }

  void Start() {
    server_thread_ = std::thread(&FakeMesenSocketServer::Run, this);
  }

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
    if (!SendLine(command_client_fd_,
                  "{\"success\":true,\"data\":{\"pong\":true}}")) {
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

    if (!SendLine(event_client_fd_,
                  "{\"success\":true,\"data\":{\"subscribed\":true}}")) {
      SetError("failed to send subscribe ack");
      return;
    }
    if (!SendLine(event_client_fd_,
                  "{\"event\":\"frame_complete\",\"frame\":42,\"address\":"
                  "\"0x008000\"}")) {
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
  std::atomic<int> legacy_callback_count{0};

  client.SetEventCallback([&](const MesenEvent& event) {
    legacy_callback_count.fetch_add(1);
    (void)event;
  });

  const auto listener_id =
      client.AddEventListener([&](const MesenEvent& event) {
        std::lock_guard<std::mutex> lock(callback_mutex);
        captured_event = event;
        got_event = true;
        callback_cv.notify_one();
      });
  ASSERT_NE(listener_id, 0u);

  ASSERT_TRUE(client.Subscribe({"frame_complete"}).ok());

  {
    std::unique_lock<std::mutex> lock(callback_mutex);
    ASSERT_TRUE(callback_cv.wait_for(lock, std::chrono::seconds(2),
                                     [&]() { return got_event; }));
  }

  EXPECT_EQ(captured_event.type, "frame_complete");
  EXPECT_EQ(captured_event.frame, 42u);
  EXPECT_EQ(captured_event.address, 0x008000u);
  EXPECT_EQ(legacy_callback_count.load(), 1);

  client.RemoveEventListener(listener_id);
  EXPECT_TRUE(client.Unsubscribe().ok());
  client.Disconnect();
  server.Stop();
  EXPECT_TRUE(server.error().empty()) << server.error();
}

class FakeMesenTcpPingServer {
 public:
  FakeMesenTcpPingServer() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
      throw std::runtime_error("failed to create TCP listen socket");
    }
    int reuse = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0);
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) !=
        0) {
      throw std::runtime_error("failed to bind fake mesen TCP socket");
    }
    socklen_t len = sizeof(addr);
    if (getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) !=
        0) {
      throw std::runtime_error("failed to read fake mesen TCP port");
    }
    port_ = ntohs(addr.sin_port);
    if (listen(listen_fd_, 2) != 0) {
      throw std::runtime_error("failed to listen on fake mesen TCP socket");
    }
  }

  ~FakeMesenTcpPingServer() { Stop(); }

  void Start() {
    server_thread_ = std::thread(&FakeMesenTcpPingServer::Run, this);
  }

  void Stop() {
    running_ = false;
    if (listen_fd_ >= 0) {
      shutdown(listen_fd_, SHUT_RDWR);
      close(listen_fd_);
      listen_fd_ = -1;
    }
    if (client_fd_ >= 0) {
      shutdown(client_fd_, SHUT_RDWR);
      close(client_fd_);
      client_fd_ = -1;
    }
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  uint16_t port() const { return port_; }

  std::string error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return error_;
  }

 private:
  static bool ReadLine(int fd, std::string* out) {
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
    }
    *out = line;
    return true;
  }

  void Run() {
    running_ = true;
    client_fd_ = accept(listen_fd_, nullptr, nullptr);
    if (client_fd_ < 0) {
      SetError("failed to accept TCP client");
      return;
    }
    std::string line;
    if (!ReadLine(client_fd_, &line) ||
        line.find("\"type\":\"PING\"") == std::string::npos) {
      SetError("first TCP command was not PING");
      return;
    }
    const char* pong = "{\"success\":true,\"data\":{\"pong\":true}}\n";
    send(client_fd_, pong, std::strlen(pong), 0);
    while (running_) {
      char buf[32];
      if (recv(client_fd_, buf, sizeof(buf), 0) <= 0) {
        break;
      }
    }
  }

  void SetError(const std::string& error) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    if (error_.empty()) {
      error_ = error;
    }
  }

  int listen_fd_ = -1;
  int client_fd_ = -1;
  uint16_t port_ = 0;
  std::thread server_thread_;
  std::atomic<bool> running_{false};
  mutable std::mutex error_mutex_;
  std::string error_;
};

TEST(MesenSocketClientTest, ConnectsToTcpEndpoint) {
  FakeMesenTcpPingServer server;
  server.Start();
  MesenSocketClient client;
  const std::string endpoint =
      "tcp://127.0.0.1:" + std::to_string(server.port());
  auto status = client.Connect(endpoint);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(client.GetSocketPath(), endpoint);
  client.Disconnect();
  server.Stop();
  EXPECT_TRUE(server.error().empty()) << server.error();
}

class EnvVarGuard {
 public:
  EnvVarGuard(const char* key, const char* value) : key_(key) {
    const char* previous = std::getenv(key);
    if (previous) {
      previous_ = previous;
    }
    setenv(key, value, 1);
  }

  ~EnvVarGuard() {
    if (previous_) {
      setenv(key_.c_str(), previous_->c_str(), 1);
    } else {
      unsetenv(key_.c_str());
    }
  }

 private:
  std::string key_;
  std::optional<std::string> previous_;
};

struct DiscoverableUnixSocketFile {
  explicit DiscoverableUnixSocketFile(const std::string& path) : path_(path) {
    std::ofstream out(path_);
    out << "decoy";
  }

  ~DiscoverableUnixSocketFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

TEST(MesenSocketClientTest, ListAvailableSocketsAcceptsTcpEnv) {
  EnvVarGuard env("MESEN2_SOCKET_PATH", "tcp://192.168.1.227:27015");
  const auto paths = MesenSocketClient::ListAvailableSockets();
  ASSERT_FALSE(paths.empty());
  EXPECT_EQ(paths.front(), "tcp://192.168.1.227:27015");
}

TEST(MesenSocketClientTest, InvalidExplicitTcpDoesNotFallBackToLocalSocket) {
  const std::string decoy_path =
      "/tmp/mesen2-" + std::to_string(::getpid()) + ".sock";
  DiscoverableUnixSocketFile decoy(decoy_path);
  ASSERT_TRUE(std::filesystem::exists(decoy.path()));

  constexpr const char* kInvalidTcp = "tcp://192.168.1.227:bad";
  EnvVarGuard env("MESEN2_SOCKET_PATH", kInvalidTcp);

  const auto paths = MesenSocketClient::ListAvailableSockets();
  ASSERT_EQ(paths.size(), 1u);
  EXPECT_EQ(paths.front(), kInvalidTcp);
  EXPECT_EQ(std::find(paths.begin(), paths.end(), decoy.path()), paths.end());

  MesenSocketClient client;
  const auto status = client.Connect();
  EXPECT_FALSE(status.ok());
  EXPECT_NE(status.message().find("Invalid TCP port"), std::string::npos)
      << status.message();
  EXPECT_FALSE(client.IsConnected());
}

TEST(MesenSocketClientTest, ConnectRejectsMalformedTcpEndpoint) {
  MesenSocketClient client;
  const auto status = client.Connect("tcp://192.168.1.227:bad");
  EXPECT_FALSE(status.ok());
  EXPECT_NE(status.message().find("Invalid TCP port"), std::string::npos)
      << status.message();
  EXPECT_FALSE(client.IsConnected());
}

TEST(MesenSocketClientTest, ConnectTcpFailureReturnsPromptly) {
  MesenSocketClient client;
  const auto started = std::chrono::steady_clock::now();
  const auto status = client.Connect("tcp://192.0.2.1:27015");
  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - started)
                              .count();
  EXPECT_FALSE(status.ok()) << status.message();
  EXPECT_FALSE(client.IsConnected());
  EXPECT_LT(elapsed_ms, 8000) << status.message();
}

}  // namespace
}  // namespace yaze::emu::mesen

#endif
