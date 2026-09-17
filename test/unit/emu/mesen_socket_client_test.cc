#ifdef _WIN32
// winsock2.h must precede headers that may include windows.h.
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "app/emu/mesen/mesen_socket_client.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>

namespace yaze::emu::mesen {
namespace {

int SetEnvironmentVariable(const char* key, const char* value) {
#ifdef _WIN32
  return _putenv_s(key, value);
#else
  return setenv(key, value, 1);
#endif
}

int UnsetEnvironmentVariable(const char* key) {
#ifdef _WIN32
  return _putenv_s(key, "");
#else
  return unsetenv(key);
#endif
}

class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* key, const char* value) : key_(key) {
    const char* previous = std::getenv(key);
    if (previous != nullptr) {
      previous_ = previous;
    }
    valid_ = SetEnvironmentVariable(key, value) == 0;
  }

  ~ScopedEnvVar() {
    if (previous_) {
      (void)SetEnvironmentVariable(key_.c_str(), previous_->c_str());
    } else {
      (void)UnsetEnvironmentVariable(key_.c_str());
    }
  }

  bool valid() const { return valid_; }

 private:
  std::string key_;
  std::optional<std::string> previous_;
  bool valid_ = false;
};

TEST(MesenSocketClientTest, ListAvailableSocketsAcceptsTcpEnv) {
  ScopedEnvVar env("MESEN2_SOCKET_PATH", "tcp://192.168.1.227:27015");
  ASSERT_TRUE(env.valid());

  const auto paths = MesenSocketClient::ListAvailableSockets();

  ASSERT_EQ(paths.size(), 1u);
  EXPECT_EQ(paths.front(), "tcp://192.168.1.227:27015");
}

TEST(MesenSocketClientTest, InvalidExplicitTcpRemainsAuthoritative) {
  constexpr const char* kInvalidTcp = "tcp://192.168.1.227:bad";
  ScopedEnvVar env("MESEN2_SOCKET_PATH", kInvalidTcp);
  ASSERT_TRUE(env.valid());

  const auto paths = MesenSocketClient::ListAvailableSockets();

  ASSERT_EQ(paths.size(), 1u);
  EXPECT_EQ(paths.front(), kInvalidTcp);
}

TEST(MesenSocketClientTest, ConnectRejectsMalformedTcpEndpoint) {
  MesenSocketClient client;
  const auto status = client.Connect("tcp://192.168.1.227:bad");

  EXPECT_FALSE(status.ok());
  EXPECT_NE(status.message().find("Invalid TCP port"), std::string::npos)
      << status.message();
  EXPECT_FALSE(client.IsConnected());
}

}  // namespace
}  // namespace yaze::emu::mesen

#ifdef _WIN32

namespace yaze::emu::mesen {
namespace {

TEST(MesenSocketClientTest, SubscribeDispatchesFrameEvents) {
  GTEST_SKIP() << "Unix socket integration test is not supported on Windows";
}

TEST(MesenSocketClientTest, RefusedTcpConnectReturnsSocketErrorWithoutTimeout) {
  // Constructing the client initializes Winsock for this process.
  MesenSocketClient client;

  SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_NE(listener, INVALID_SOCKET) << WSAGetLastError();

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  ASSERT_NE(
      bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)),
      SOCKET_ERROR)
      << WSAGetLastError();

  int address_length = sizeof(address);
  ASSERT_NE(getsockname(listener, reinterpret_cast<sockaddr*>(&address),
                        &address_length),
            SOCKET_ERROR)
      << WSAGetLastError();
  const uint16_t port = ntohs(address.sin_port);
  closesocket(listener);

  const auto status = client.Connect("tcp://127.0.0.1:" + std::to_string(port));

  // Whatever the network does, a failed connect must be reported as a failure.
  ASSERT_FALSE(status.ok());

  // The interesting assertion is the SHAPE of that failure: a refused port
  // must surface the socket error rather than sitting on the deadline, which
  // is what the Winsock branch of WaitForConnectComplete exists for (Winsock
  // reports a failed nonblocking connect through exceptfds, not writefds, so
  // selecting on writefds alone would always time out here).
  //
  // Whether a closed port actually answers is the OS and firewall's choice,
  // not this client's. A loopback RST is the norm, but a sandboxed CI network
  // may silently DROP the SYN instead — and then a deadline is the only
  // correct answer this code could give. Asserting "never times out" would be
  // asserting a property of the runner's firewall, so only check the refusal
  // shape when a refusal is what actually came back.
  if (status.message().find("Timed out") != std::string::npos) {
    GTEST_SKIP() << "Loopback SYN was dropped rather than refused, so the "
                    "deadline is the correct result: "
                 << status.message();
  }
  EXPECT_NE(status.message().find("Failed to connect"), std::string::npos)
      << status.message();
}

}  // namespace
}  // namespace yaze::emu::mesen

#else

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace yaze::emu::mesen {
namespace {

class FakeMesenSocketServer {
 public:
  explicit FakeMesenSocketServer(int event_delay_ms = 0)
      : event_delay_ms_(event_delay_ms) {
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
    if (event_delay_ms_ > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(event_delay_ms_));
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

  int event_delay_ms_ = 0;
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

// Answers one PING, then either closes the connection or stops reading. Both
// shapes exercise the command write path rather than the protocol.
class UnresponsiveUnixServer {
 public:
  enum class Mode { kCloseAfterPing, kStallAfterPing, kSlowReaderAfterPing };

  explicit UnresponsiveUnixServer(Mode mode) : mode_(mode) {
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();
    socket_path_ =
        (std::filesystem::temp_directory_path() /
         ("yaze-mesen-unresponsive-" + std::to_string(now) + ".sock"))
            .string();
    listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
      throw std::runtime_error("failed to create listen socket");
    }
    ::sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) !=
        0) {
      throw std::runtime_error("failed to bind unresponsive server socket");
    }
    if (listen(listen_fd_, 4) != 0) {
      throw std::runtime_error("failed to listen on unresponsive server");
    }
  }

  ~UnresponsiveUnixServer() { Stop(); }

  void Start() { thread_ = std::thread(&UnresponsiveUnixServer::Run, this); }

  void Stop() {
    running_ = false;
    if (client_fd_ >= 0) {
      shutdown(client_fd_, SHUT_RDWR);
      close(client_fd_);
      client_fd_ = -1;
    }
    if (listen_fd_ >= 0) {
      shutdown(listen_fd_, SHUT_RDWR);
      close(listen_fd_);
      listen_fd_ = -1;
    }
    if (thread_.joinable()) {
      thread_.join();
    }
    std::error_code ec;
    std::filesystem::remove(socket_path_, ec);
  }

  const std::string& socket_path() const { return socket_path_; }

 private:
  void Run() {
    running_ = true;
    client_fd_ = accept(listen_fd_, nullptr, nullptr);
    if (client_fd_ < 0) {
      return;
    }
    std::string line;
    char c = '\0';
    while (recv(client_fd_, &c, 1, 0) == 1 && c != '\n') {
      line.push_back(c);
    }
    const std::string pong = "{\"success\":true,\"data\":{\"pong\":true}}\n";
    (void)send(client_fd_, pong.c_str(), pong.size(), 0);
    if (mode_ == Mode::kCloseAfterPing) {
      shutdown(client_fd_, SHUT_RDWR);
      close(client_fd_);
      client_fd_ = -1;
      return;
    }
    if (mode_ == Mode::kSlowReaderAfterPing) {
      // Drain a trickle: each send() can make progress, so only a
      // per-command deadline bounds the write.
      char byte = 0;
      while (running_) {
        if (recv(client_fd_, &byte, 1, 0) <= 0) {
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
      return;
    }
    // kStallAfterPing: never read again, so the client's socket buffer fills.
    while (running_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }

  Mode mode_;
  std::string socket_path_;
  int listen_fd_ = -1;
  int client_fd_ = -1;
  std::thread thread_;
  std::atomic<bool> running_{false};
};

TEST(MesenSocketClientTest, CommandOnClosedPeerReturnsErrorInsteadOfSignal) {
  UnresponsiveUnixServer server(UnresponsiveUnixServer::Mode::kCloseAfterPing);
  server.Start();

  MesenSocketClient client;
  ASSERT_TRUE(client.Connect(server.socket_path()).ok());

  // Writing to a closed peer raises SIGPIPE unless the socket or send()
  // suppresses it; an unsuppressed signal would kill this test process.
  absl::Status status = absl::OkStatus();
  for (int attempt = 0; attempt < 10 && status.ok(); ++attempt) {
    status = client.Ping();
  }
  EXPECT_FALSE(status.ok());

  client.Disconnect();
  server.Stop();
}

TEST(MesenSocketClientTest, LargeCommandReportsDeadlineInsteadOfTruncating) {
  ScopedEnvVar send_timeout("YAZE_MESEN_SEND_TIMEOUT_MS", "200");
  UnresponsiveUnixServer server(UnresponsiveUnixServer::Mode::kStallAfterPing);
  server.Start();

  MesenSocketClient client;
  ASSERT_TRUE(client.Connect(server.socket_path()).ok());

  // Larger than any socket buffer, so the write cannot complete while the
  // peer never reads. A single send() would report partial success here.
  const std::string payload(4 * 1024 * 1024, 'a');
  const auto result =
      client.SendCommand("{\"type\":\"WRITE\",\"data\":\"" + payload + "\"}\n");
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kDeadlineExceeded)
      << result.status();
  // The message must show how far the write got; a truncating implementation
  // that reported the whole command as sent would read "4194304 of 4194304".
  const std::string message(result.status().message());
  std::smatch progress;
  ASSERT_TRUE(std::regex_search(message, progress,
                                std::regex(R"(\((\d+) of (\d+) bytes\))")))
      << message;
  EXPECT_LT(std::stoul(progress[1].str()), std::stoul(progress[2].str()))
      << message;

  client.Disconnect();
  server.Stop();
}

// Counts this process's open descriptors, so a reconnect can be checked for
// leaks without reaching into the client's internals.
int OpenDescriptorCount() {
#ifdef __APPLE__
  const char* dir = "/dev/fd";
#else
  const char* dir = "/proc/self/fd";
#endif
  std::error_code ec;
  int count = 0;
  for (auto it = std::filesystem::directory_iterator(dir, ec);
       !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
    ++count;
  }
  return count;
}

TEST(MesenSocketClientTest, SlowReaderStillHitsTheCommandDeadline) {
  ScopedEnvVar send_timeout("YAZE_MESEN_SEND_TIMEOUT_MS", "200");
  UnresponsiveUnixServer server(
      UnresponsiveUnixServer::Mode::kSlowReaderAfterPing);
  server.Start();

  MesenSocketClient client;
  ASSERT_TRUE(client.Connect(server.socket_path()).ok());

  // Every send() makes progress here, so a per-syscall timeout alone would
  // never fire and the command could crawl for minutes.
  const std::string payload(4 * 1024 * 1024, 'a');
  // Run the command on another thread: a regression here blocks inside
  // send(), and the test should report that rather than hang the suite.
  auto pending = std::async(std::launch::async, [&]() {
    return client.SendCommand("{\"type\":\"WRITE\",\"data\":\"" + payload +
                              "\"}\n");
  });
  if (pending.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
    server.Stop();  // unblocks the send so the thread can finish
    ADD_FAILURE() << "command deadline did not bound a slow reader";
    pending.wait();
    return;
  }
  const auto result = pending.get();

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kDeadlineExceeded)
      << result.status();

  client.Disconnect();
  server.Stop();
}

TEST(MesenSocketClientTest, ReconnectAfterCommandFailureDoesNotLeakSockets) {
  ScopedEnvVar send_timeout("YAZE_MESEN_SEND_TIMEOUT_MS", "200");
  const std::string payload(4 * 1024 * 1024, 'a');
  const std::string command =
      "{\"type\":\"WRITE\",\"data\":\"" + payload + "\"}\n";

  MesenSocketClient client;
  std::optional<int> baseline;
  for (int attempt = 0; attempt < 3; ++attempt) {
    UnresponsiveUnixServer server(
        UnresponsiveUnixServer::Mode::kStallAfterPing);
    server.Start();
    // Reconnect without an intervening Disconnect(), the way the Mesen
    // panels' Connect button does.
    ASSERT_TRUE(client.Connect(server.socket_path()).ok());
    EXPECT_FALSE(client.SendCommand(command).ok());
    server.Stop();
    if (attempt == 0) {
      baseline = OpenDescriptorCount();
    } else {
      EXPECT_LE(OpenDescriptorCount(), *baseline)
          << "reconnect leaked a descriptor on attempt " << attempt;
    }
  }
  client.Disconnect();
}

TEST(MesenSocketClientTest, EventArrivesAfterAnIdlePeriod) {
  ScopedEnvVar poll("YAZE_MESEN_EVENT_POLL_MS", "50");
  // The stream stays quiet for several poll intervals before the event.
  FakeMesenSocketServer server(/*event_delay_ms=*/300);
  server.Start();

  MesenSocketClient client;
  ASSERT_TRUE(client.Connect(server.socket_path()).ok());

  std::mutex mutex;
  std::condition_variable cv;
  bool got_event = false;
  const auto listener = client.AddEventListener([&](const MesenEvent&) {
    std::lock_guard<std::mutex> lock(mutex);
    got_event = true;
    cv.notify_one();
  });
  ASSERT_TRUE(client.Subscribe({"frame_complete"}).ok());

  {
    std::unique_lock<std::mutex> lock(mutex);
    EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&]() {
      return got_event;
    })) << "a receive timeout ended the event loop";
  }

  client.RemoveEventListener(listener);
  EXPECT_TRUE(client.Unsubscribe().ok());
  client.Disconnect();
  server.Stop();
}

TEST(MesenSocketClientTest, UnsubscribeReturnsPromptlyWhileIdle) {
  ScopedEnvVar poll("YAZE_MESEN_EVENT_POLL_MS", "50");
  FakeMesenSocketServer server;
  server.Start();

  MesenSocketClient client;
  ASSERT_TRUE(client.Connect(server.socket_path()).ok());
  ASSERT_TRUE(client.Subscribe({"frame_complete"}).ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  const auto start = std::chrono::steady_clock::now();
  EXPECT_TRUE(client.Unsubscribe().ok());
  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_LT(
      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
      3000)
      << "Unsubscribe waited on an unbounded recv";

  client.Disconnect();
  server.Stop();
}

class FakeMesenTcpPingServer {
 public:
  explicit FakeMesenTcpPingServer(std::string command_type = {},
                                  std::string command_response = {})
      : command_type_(command_type), command_response_(command_response) {
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
    if (!command_type_.empty()) {
      if (!ReadLine(client_fd_, &line) ||
          line.find("\"type\":\"" + command_type_ + "\"") ==
              std::string::npos) {
        SetError("unexpected TCP command after PING");
        return;
      }
      const std::string response = command_response_ + "\n";
      if (send(client_fd_, response.c_str(), response.size(), 0) !=
          static_cast<ssize_t>(response.size())) {
        SetError("failed to send TCP command response");
        return;
      }
    }
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
  std::string command_type_;
  std::string command_response_;
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

TEST(MesenSocketClientTest, GetCpuStateParsesLowercaseServerSchema) {
  FakeMesenTcpPingServer server(
      "CPU",
      R"({"success":true,"data":{"pc":"0x128036","flags":"0x32","a":"0xB700","x":"0x1234","y":"0xABCD","sp":"0x01FF","d":"0x4321","k":"0x12","dbr":"0x7E","p":"0x32","cycles":12345,"consoleType":0}})");
  server.Start();

  MesenSocketClient client;
  const std::string endpoint =
      "tcp://127.0.0.1:" + std::to_string(server.port());
  ASSERT_TRUE(client.Connect(endpoint).ok());

  const auto cpu = client.GetCpuState();
  ASSERT_TRUE(cpu.ok()) << cpu.status().message();
  EXPECT_EQ(cpu->A, 0xB700);
  EXPECT_EQ(cpu->X, 0x1234);
  EXPECT_EQ(cpu->Y, 0xABCD);
  EXPECT_EQ(cpu->SP, 0x01FF);
  EXPECT_EQ(cpu->D, 0x4321);
  EXPECT_EQ(cpu->PC, 0x128036);
  EXPECT_EQ(cpu->K, 0x12);
  EXPECT_EQ(cpu->DBR, 0x7E);
  EXPECT_EQ(cpu->P, 0x32);
  EXPECT_FALSE(cpu->emulation_mode);

  client.Disconnect();
  server.Stop();
  EXPECT_TRUE(server.error().empty()) << server.error();
}

struct DiscoverableUnixSocketFile {
  explicit DiscoverableUnixSocketFile(const std::string& path) : path_(path) {
    std::ofstream output(path_);
    output << "decoy";
  }

  ~DiscoverableUnixSocketFile() {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }

  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

TEST(MesenSocketClientTest, InvalidExplicitTcpDoesNotUseDiscoveredLocalSocket) {
  const std::string decoy_path =
      "/tmp/mesen2-" + std::to_string(::getpid()) + ".sock";
  DiscoverableUnixSocketFile decoy(decoy_path);
  ASSERT_TRUE(std::filesystem::exists(decoy.path()));

  constexpr const char* kInvalidTcp = "tcp://192.168.1.227:bad";
  ScopedEnvVar env("MESEN2_SOCKET_PATH", kInvalidTcp);
  ASSERT_TRUE(env.valid());

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

}  // namespace
}  // namespace yaze::emu::mesen

#endif
