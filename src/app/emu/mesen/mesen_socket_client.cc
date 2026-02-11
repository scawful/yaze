#include "app/emu/mesen/mesen_socket_client.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <filesystem>

#ifdef _WIN32
// clang-format off
// winsock2.h must precede afunix.h (defines ADDRESS_FAMILY)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#include <io.h>
// clang-format on
#define close closesocket
typedef int ssize_t;
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include <cstdlib>

#include <cerrno>
#include <cstring>
#include <regex>
#include <sstream>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"

#include <charconv>

namespace yaze {
namespace emu {
namespace mesen {

namespace {

// Simple JSON value extraction (avoids pulling in a full JSON library)
std::string ExtractJsonString(const std::string& json, const std::string& key) {
  std::string search = "\"" + key + "\":";
  size_t pos = json.find(search);
  if (pos == std::string::npos)
    return "";

  pos += search.length();
  while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t'))
    pos++;

  if (pos >= json.length())
    return "";

  if (json[pos] == '"') {
    // String value
    size_t start = pos + 1;
    size_t end = json.find('"', start);
    while (end != std::string::npos && end > 0 && json[end - 1] == '\\') {
      end = json.find('"', end + 1);
    }
    if (end == std::string::npos)
      return "";
    return json.substr(start, end - start);
  } else if (json[pos] == '{') {
    // Object value - find matching brace
    int depth = 1;
    size_t start = pos;
    pos++;
    while (pos < json.length() && depth > 0) {
      if (json[pos] == '{')
        depth++;
      else if (json[pos] == '}')
        depth--;
      pos++;
    }
    return json.substr(start, pos - start);
  } else if (json[pos] == '[') {
    // Array value - find matching bracket
    int depth = 1;
    size_t start = pos;
    pos++;
    while (pos < json.length() && depth > 0) {
      if (json[pos] == '[')
        depth++;
      else if (json[pos] == ']')
        depth--;
      pos++;
    }
    return json.substr(start, pos - start);
  } else {
    // Number or boolean
    size_t start = pos;
    while (pos < json.length() && json[pos] != ',' && json[pos] != '}' &&
           json[pos] != ']') {
      pos++;
    }
    return json.substr(start, pos - start);
  }
}

int64_t ExtractJsonInt(const std::string& json, const std::string& key,
                       int64_t default_value = 0) {
  std::string value = ExtractJsonString(json, key);
  if (value.empty())
    return default_value;

  // Handle hex strings like "0x7E0000"
  if (value.length() > 2 && value[0] == '0' &&
      (value[1] == 'x' || value[1] == 'X')) {
    int64_t result;
    std::string hex_str = value.substr(2);
    auto [ptr, ec] = std::from_chars(
        hex_str.data(), hex_str.data() + hex_str.size(), result, 16);
    if (ec == std::errc()) {
      return result;
    }
  }

  int64_t result;
  if (absl::SimpleAtoi(value, &result)) {
    return result;
  }
  return default_value;
}

double ExtractJsonDouble(const std::string& json, const std::string& key,
                         double default_value = 0.0) {
  std::string value = ExtractJsonString(json, key);
  if (value.empty())
    return default_value;

  double result;
  if (absl::SimpleAtod(value, &result)) {
    return result;
  }
  return default_value;
}

bool ExtractJsonBool(const std::string& json, const std::string& key,
                     bool default_value = false) {
  std::string value = ExtractJsonString(json, key);
  if (value.empty())
    return default_value;
  return value == "true";
}

std::string BuildJsonCommand(const std::string& type) {
  return absl::StrFormat("{\"type\":\"%s\"}\n", type);
}

std::string EscapeJsonValue(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (char c : value) {
    switch (c) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += c;
        break;
    }
  }
  return escaped;
}

std::string BuildJsonCommand(
    const std::string& type,
    const std::vector<std::pair<std::string, std::string>>& params) {
  std::stringstream ss;
  ss << "{\"type\":\"" << type << "\"";
  for (const auto& [key, value] : params) {
    ss << ",\"" << key << "\":\"" << EscapeJsonValue(value) << "\"";
  }
  ss << "}\n";
  return ss.str();
}

constexpr size_t kMaxResponseSize = 4 * 1024 * 1024;

absl::Status ConnectSocketToPath(const std::string& socket_path,
                                 int* socket_fd) {
  if (socket_fd == nullptr) {
    return absl::InvalidArgumentError("socket_fd output pointer is null");
  }

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return absl::InternalError(
        absl::StrCat("Failed to create socket: ", strerror(errno)));
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

  if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(fd);
    return absl::UnavailableError(absl::StrCat(
        "Failed to connect to ", socket_path, ": ", strerror(errno)));
  }

  *socket_fd = fd;
  return absl::OkStatus();
}

void ShutdownSocketFd(int fd) {
  if (fd < 0) {
    return;
  }
#ifdef _WIN32
  shutdown(fd, SD_BOTH);
#else
  shutdown(fd, SHUT_RDWR);
#endif
}

}  // namespace

MesenSocketClient::MesenSocketClient() = default;

MesenSocketClient::~MesenSocketClient() {
  Disconnect();
}

absl::Status MesenSocketClient::Connect() {
  auto paths = FindSocketPaths();
  if (paths.empty()) {
    return absl::NotFoundError(
        "No Mesen2 socket found. Is Mesen2-OoS running?");
  }
  return Connect(paths[0]);
}

absl::Status MesenSocketClient::Connect(const std::string& socket_path) {
  if (IsConnected()) {
    Disconnect();
  }

  int fd = -1;
  auto connect_status = ConnectSocketToPath(socket_path, &fd);
  if (!connect_status.ok()) {
    return connect_status;
  }

  socket_fd_ = fd;
  socket_path_ = socket_path;
  connected_ = true;

  // Verify connection with a ping
  auto status = Ping();
  if (!status.ok()) {
    Disconnect();
    return status;
  }

  return absl::OkStatus();
}

void MesenSocketClient::Disconnect() {
  // Best-effort: stop event stream first so background thread exits cleanly.
  (void)Unsubscribe();

  if (socket_fd_ >= 0) {
    close(socket_fd_);
    socket_fd_ = -1;
  }
  socket_path_.clear();
  connected_ = false;
}

bool MesenSocketClient::IsConnected() const {
  return connected_;
}

std::vector<std::string> MesenSocketClient::FindSocketPaths() {
  const char* env_path = std::getenv("MESEN2_SOCKET_PATH");
  if (env_path && env_path[0] != '\0') {
#ifdef _WIN32
    // Windows AF_UNIX sockets don't report S_IFSOCK via stat; trust the env var
    if (std::filesystem::exists(env_path)) {
      return {std::string(env_path)};
    }
#else
    struct stat st;
    if (stat(env_path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFSOCK) {
      return {std::string(env_path)};
    }
#endif
  }

  std::vector<std::string> paths;
  namespace fs = std::filesystem;
  std::vector<fs::path> search_paths;

#ifdef _WIN32
  search_paths.push_back(fs::temp_directory_path());
#else
  search_paths.push_back("/tmp");
#endif

  std::regex socket_pattern("mesen2-\\d+\\.sock");

  for (const auto& search_path : search_paths) {
    std::error_code ec;
    if (!fs::exists(search_path, ec))
      continue;

    for (const auto& entry : fs::directory_iterator(search_path, ec)) {
      if (ec)
        break;
      // On Windows, checking is_socket might be unreliable or not supported for AF_UNIX files,
      // so we mainly rely on the filename pattern.
      std::string filename = entry.path().filename().string();
      if (std::regex_match(filename, socket_pattern)) {
        paths.push_back(entry.path().string());
      }
    }
  }

  return paths;
}

std::vector<std::string> MesenSocketClient::ListAvailableSockets() {
  return FindSocketPaths();
}

absl::StatusOr<std::string> MesenSocketClient::SendCommandOnSocket(
    int fd, const std::string& json, bool update_connection_state) {
  if (fd < 0) {
    return absl::FailedPreconditionError("Socket is not connected");
  }

  // Send command
  ssize_t sent = send(fd, json.c_str(), json.length(), 0);
  if (sent < 0) {
    if (update_connection_state) {
      connected_ = false;
    }
    return absl::InternalError(
        absl::StrCat("Failed to send command: ", strerror(errno)));
  }

  // Receive response (with timeout)
  struct timeval tv;
  tv.tv_sec = 5;
  tv.tv_usec = 0;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv),
             sizeof(tv));

  std::string response;
  char buffer[4096];
  while (true) {
    ssize_t received = recv(fd, buffer, sizeof(buffer), 0);
    if (received < 0) {
#ifdef _WIN32
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT) {
#else
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
        if (response.empty()) {
          return absl::DeadlineExceededError("Timeout waiting for response");
        }
        break;
      }
      if (update_connection_state) {
        connected_ = false;
      }
      return absl::InternalError(
          absl::StrCat("Failed to receive response: ", strerror(errno)));
    }
    if (received == 0) {
      break;
    }
    response.append(buffer, static_cast<size_t>(received));
    if (response.size() > kMaxResponseSize) {
      return absl::ResourceExhaustedError("Mesen2 response too large");
    }
    if (response.find('\n') != std::string::npos) {
      break;
    }
  }
  if (response.empty()) {
    return absl::DeadlineExceededError("Empty response from Mesen2");
  }
  auto newline_pos = response.find('\n');
  if (newline_pos != std::string::npos) {
    response = response.substr(0, newline_pos);
  }

  return ParseResponse(response);
}

absl::StatusOr<std::string> MesenSocketClient::SendCommand(
    const std::string& json) {
  if (!IsConnected()) {
    return absl::FailedPreconditionError("Not connected to Mesen2");
  }

  std::lock_guard<std::mutex> lock(command_mutex_);
  return SendCommandOnSocket(socket_fd_, json, /*update_connection_state=*/true);
}

absl::StatusOr<std::string> MesenSocketClient::ParseResponse(
    const std::string& response) {
  bool success = ExtractJsonBool(response, "success");
  if (!success) {
    std::string error = ExtractJsonString(response, "error");
    if (error.empty())
      error = "Unknown Mesen2 error";
    return absl::InternalError(error);
  }

  std::string data = ExtractJsonString(response, "data");
  return data.empty() ? response : data;
}

// ─────────────────────────────────────────────────────────────────────────────
// Control Commands
// ─────────────────────────────────────────────────────────────────────────────

absl::Status MesenSocketClient::Ping() {
  auto result = SendCommand(BuildJsonCommand("PING"));
  if (!result.ok())
    return result.status();
  return absl::OkStatus();
}

absl::StatusOr<MesenState> MesenSocketClient::GetState() {
  auto result = SendCommand(BuildJsonCommand("STATE"));
  if (!result.ok())
    return result.status();

  MesenState state;
  state.running = ExtractJsonBool(*result, "running");
  state.paused = ExtractJsonBool(*result, "paused");
  state.debugging = ExtractJsonBool(*result, "debugging");
  state.frame = ExtractJsonInt(*result, "frame");
  state.fps = ExtractJsonDouble(*result, "fps");
  state.console_type = ExtractJsonInt(*result, "consoleType");
  return state;
}

absl::Status MesenSocketClient::Pause() {
  auto result = SendCommand(BuildJsonCommand("PAUSE"));
  return result.status();
}

absl::Status MesenSocketClient::Resume() {
  auto result = SendCommand(BuildJsonCommand("RESUME"));
  return result.status();
}

absl::Status MesenSocketClient::Reset() {
  auto result = SendCommand(BuildJsonCommand("RESET"));
  return result.status();
}

absl::Status MesenSocketClient::Frame() {
  auto result = SendCommand(BuildJsonCommand("FRAME"));
  return result.status();
}

absl::Status MesenSocketClient::Step(int count) {
  auto result =
      SendCommand(BuildJsonCommand("STEP", {{"count", std::to_string(count)}}));
  return result.status();
}

// ─────────────────────────────────────────────────────────────────────────────
// Memory Commands
// ─────────────────────────────────────────────────────────────────────────────

absl::StatusOr<uint8_t> MesenSocketClient::ReadByte(uint32_t addr) {
  auto result = SendCommand(
      BuildJsonCommand("READ", {{"addr", absl::StrFormat("0x%06X", addr)}}));
  if (!result.ok())
    return result.status();
  return static_cast<uint8_t>(ExtractJsonInt(*result, "data", 0));
}

absl::StatusOr<uint16_t> MesenSocketClient::ReadWord(uint32_t addr) {
  auto result = SendCommand(
      BuildJsonCommand("READ16", {{"addr", absl::StrFormat("0x%06X", addr)}}));
  if (!result.ok())
    return result.status();
  return static_cast<uint16_t>(ExtractJsonInt(*result, "data", 0));
}

absl::StatusOr<std::vector<uint8_t>> MesenSocketClient::ReadBlock(uint32_t addr,
                                                                  size_t len) {
  auto result = SendCommand(
      BuildJsonCommand("READBLOCK", {{"addr", absl::StrFormat("0x%06X", addr)},
                                     {"len", std::to_string(len)}}));
  if (!result.ok())
    return result.status();

  // Response is hex string
  std::string hex = ExtractJsonString(*result, "data");
  if (hex.empty()) {
    // Try raw data field
    hex = *result;
  }

  std::vector<uint8_t> data;
  data.reserve(len);

  for (size_t i = 0; i + 1 < hex.length(); i += 2) {
    int byte;
    std::string byte_hex = hex.substr(i, 2);
    auto [ptr, ec] = std::from_chars(
        byte_hex.data(), byte_hex.data() + byte_hex.size(), byte, 16);
    if (ec == std::errc()) {
      data.push_back(static_cast<uint8_t>(byte));
    }
  }
  return data;
}

absl::Status MesenSocketClient::WriteByte(uint32_t addr, uint8_t value) {
  auto result = SendCommand(
      BuildJsonCommand("WRITE", {{"addr", absl::StrFormat("0x%06X", addr)},
                                 {"value", absl::StrFormat("0x%02X", value)}}));
  return result.status();
}

absl::Status MesenSocketClient::WriteWord(uint32_t addr, uint16_t value) {
  auto result = SendCommand(BuildJsonCommand(
      "WRITE16", {{"addr", absl::StrFormat("0x%06X", addr)},
                  {"value", absl::StrFormat("0x%04X", value)}}));
  return result.status();
}

absl::Status MesenSocketClient::WriteBlock(uint32_t addr,
                                           const std::vector<uint8_t>& data) {
  std::stringstream hex;
  for (uint8_t byte : data) {
    hex << absl::StrFormat("%02X", byte);
  }
  auto result = SendCommand(BuildJsonCommand(
      "WRITEBLOCK",
      {{"addr", absl::StrFormat("0x%06X", addr)}, {"hex", hex.str()}}));
  return result.status();
}

// ─────────────────────────────────────────────────────────────────────────────
// Debugging Commands
// ─────────────────────────────────────────────────────────────────────────────

absl::StatusOr<CpuState> MesenSocketClient::GetCpuState() {
  auto result = SendCommand(BuildJsonCommand("CPU"));
  if (!result.ok())
    return result.status();

  CpuState state;
  state.A = static_cast<uint16_t>(ExtractJsonInt(*result, "A"));
  state.X = static_cast<uint16_t>(ExtractJsonInt(*result, "X"));
  state.Y = static_cast<uint16_t>(ExtractJsonInt(*result, "Y"));
  state.SP = static_cast<uint16_t>(ExtractJsonInt(*result, "SP"));
  state.D = static_cast<uint16_t>(ExtractJsonInt(*result, "D"));
  state.PC = static_cast<uint32_t>(ExtractJsonInt(*result, "PC"));
  state.K = static_cast<uint8_t>(ExtractJsonInt(*result, "K"));
  state.DBR = static_cast<uint8_t>(ExtractJsonInt(*result, "DBR"));
  state.P = static_cast<uint8_t>(ExtractJsonInt(*result, "P"));
  state.emulation_mode = ExtractJsonBool(*result, "emulationMode");
  return state;
}

absl::StatusOr<std::string> MesenSocketClient::Disassemble(uint32_t addr,
                                                           int count) {
  auto result = SendCommand(
      BuildJsonCommand("DISASM", {{"addr", absl::StrFormat("0x%06X", addr)},
                                  {"count", std::to_string(count)}}));
  if (!result.ok())
    return result.status();
  return *result;
}

absl::StatusOr<int> MesenSocketClient::AddBreakpoint(
    uint32_t addr, BreakpointType type, const std::string& condition) {
  std::string type_str;
  switch (type) {
    case BreakpointType::kExecute:
      type_str = "exec";
      break;
    case BreakpointType::kRead:
      type_str = "read";
      break;
    case BreakpointType::kWrite:
      type_str = "write";
      break;
    case BreakpointType::kReadWrite:
      type_str = "rw";
      break;
  }

  std::vector<std::pair<std::string, std::string>> params = {
      {"action", "add"},
      {"addr", absl::StrFormat("0x%06X", addr)},
      {"bptype", type_str}};

  if (!condition.empty()) {
    params.push_back({"condition", condition});
  }

  auto result = SendCommand(BuildJsonCommand("BREAKPOINT", params));
  if (!result.ok())
    return result.status();

  return static_cast<int>(ExtractJsonInt(*result, "id", -1));
}

absl::Status MesenSocketClient::RemoveBreakpoint(int id) {
  auto result = SendCommand(BuildJsonCommand(
      "BREAKPOINT", {{"action", "remove"}, {"id", std::to_string(id)}}));
  return result.status();
}

absl::Status MesenSocketClient::ClearBreakpoints() {
  auto result =
      SendCommand(BuildJsonCommand("BREAKPOINT", {{"action", "clear"}}));
  return result.status();
}

absl::StatusOr<std::string> MesenSocketClient::GetTrace(int count) {
  auto result = SendCommand(
      BuildJsonCommand("TRACE", {{"count", std::to_string(count)}}));
  if (!result.ok())
    return result.status();
  return *result;
}

// ─────────────────────────────────────────────────────────────────────────────
// ALTTP Commands
// ─────────────────────────────────────────────────────────────────────────────

absl::StatusOr<GameState> MesenSocketClient::GetGameState() {
  auto result = SendCommand(BuildJsonCommand("GAMESTATE"));
  if (!result.ok())
    return result.status();

  GameState state;

  // Parse link state
  std::string link_json = ExtractJsonString(*result, "link");
  state.link.x = static_cast<uint16_t>(ExtractJsonInt(link_json, "x"));
  state.link.y = static_cast<uint16_t>(ExtractJsonInt(link_json, "y"));
  state.link.layer = static_cast<uint8_t>(ExtractJsonInt(link_json, "layer"));
  state.link.direction =
      static_cast<uint8_t>(ExtractJsonInt(link_json, "direction"));
  state.link.state = static_cast<uint8_t>(ExtractJsonInt(link_json, "state"));
  state.link.pose = static_cast<uint8_t>(ExtractJsonInt(link_json, "pose"));

  // Parse health
  std::string health_json = ExtractJsonString(*result, "health");
  state.items.current_health =
      static_cast<uint8_t>(ExtractJsonInt(health_json, "current"));
  state.items.max_health =
      static_cast<uint8_t>(ExtractJsonInt(health_json, "max"));

  // Parse items
  std::string items_json = ExtractJsonString(*result, "items");
  state.items.magic = static_cast<uint8_t>(ExtractJsonInt(items_json, "magic"));
  state.items.rupees =
      static_cast<uint16_t>(ExtractJsonInt(items_json, "rupees"));
  state.items.bombs = static_cast<uint8_t>(ExtractJsonInt(items_json, "bombs"));
  state.items.arrows =
      static_cast<uint8_t>(ExtractJsonInt(items_json, "arrows"));

  // Parse game mode
  std::string game_json = ExtractJsonString(*result, "game");
  state.game.mode = static_cast<uint8_t>(ExtractJsonInt(game_json, "mode"));
  state.game.submode =
      static_cast<uint8_t>(ExtractJsonInt(game_json, "submode"));
  state.game.indoors = ExtractJsonBool(game_json, "indoors");
  state.game.room_id =
      static_cast<uint16_t>(ExtractJsonInt(game_json, "room_id"));
  state.game.overworld_area =
      static_cast<uint8_t>(ExtractJsonInt(game_json, "overworld_area"));

  return state;
}

absl::StatusOr<std::vector<SpriteInfo>> MesenSocketClient::GetSprites(
    bool all) {
  std::vector<std::pair<std::string, std::string>> params;
  if (all) {
    params.push_back({"all", "true"});
  }

  auto result =
      SendCommand(params.empty() ? BuildJsonCommand("SPRITES")
                                 : BuildJsonCommand("SPRITES", params));
  if (!result.ok())
    return result.status();

  std::vector<SpriteInfo> sprites;

  // Find sprites array in response
  size_t array_start = result->find("[");
  size_t array_end = result->rfind("]");
  if (array_start == std::string::npos || array_end == std::string::npos) {
    return sprites;
  }

  std::string array_content =
      result->substr(array_start + 1, array_end - array_start - 1);

  // Parse each sprite object
  size_t pos = 0;
  while ((pos = array_content.find("{", pos)) != std::string::npos) {
    size_t end = array_content.find("}", pos);
    if (end == std::string::npos)
      break;

    std::string sprite_json = array_content.substr(pos, end - pos + 1);

    SpriteInfo sprite;
    sprite.slot = static_cast<int>(ExtractJsonInt(sprite_json, "slot"));
    sprite.type = static_cast<uint8_t>(ExtractJsonInt(sprite_json, "type"));
    sprite.state = static_cast<uint8_t>(ExtractJsonInt(sprite_json, "state"));
    sprite.x = static_cast<uint16_t>(ExtractJsonInt(sprite_json, "x"));
    sprite.y = static_cast<uint16_t>(ExtractJsonInt(sprite_json, "y"));
    sprite.health = static_cast<uint8_t>(ExtractJsonInt(sprite_json, "health"));
    sprite.subtype =
        static_cast<uint8_t>(ExtractJsonInt(sprite_json, "subtype"));

    sprites.push_back(sprite);
    pos = end + 1;
  }

  return sprites;
}

absl::Status MesenSocketClient::SetCollisionOverlay(bool enable,
                                                    const std::string& colmap) {
  auto result = SendCommand(BuildJsonCommand(
      "COLLISION_OVERLAY",
      {{"action", enable ? "enable" : "disable"}, {"colmap", colmap}}));
  return result.status();
}

// ─────────────────────────────────────────────────────────────────────────────
// Save State Commands
// ─────────────────────────────────────────────────────────────────────────────

absl::Status MesenSocketClient::SaveState(int slot) {
  auto result = SendCommand(
      BuildJsonCommand("SAVESTATE", {{"slot", std::to_string(slot)}}));
  return result.status();
}

absl::Status MesenSocketClient::LoadState(int slot) {
  auto result = SendCommand(
      BuildJsonCommand("LOADSTATE", {{"slot", std::to_string(slot)}}));
  return result.status();
}

absl::StatusOr<std::string> MesenSocketClient::Screenshot() {
  auto result = SendCommand(BuildJsonCommand("SCREENSHOT"));
  if (!result.ok())
    return result.status();
  return *result;  // Base64 PNG data
}

// ─────────────────────────────────────────────────────────────────────────────
// Event Subscription
// ─────────────────────────────────────────────────────────────────────────────

absl::Status MesenSocketClient::Subscribe(
    const std::vector<std::string>& events) {
  if (!IsConnected()) {
    return absl::FailedPreconditionError("Not connected to Mesen2");
  }
  if (events.empty()) {
    return absl::InvalidArgumentError("At least one event must be requested");
  }

  // Ensure we don't leak a previous subscription socket/thread.
  (void)Unsubscribe();

  std::stringstream ss;
  for (size_t i = 0; i < events.size(); ++i) {
    if (i > 0)
      ss << ",";
    ss << events[i];
  }

  int event_fd = -1;
  auto connect_status = ConnectSocketToPath(socket_path_, &event_fd);
  if (!connect_status.ok()) {
    return connect_status;
  }

  const std::string subscribe_command =
      BuildJsonCommand("SUBSCRIBE", {{"events", ss.str()}});
  const ssize_t sent =
      send(event_fd, subscribe_command.c_str(), subscribe_command.length(), 0);
  if (sent < 0) {
    close(event_fd);
    return absl::InternalError(
        absl::StrCat("Failed to send command: ", strerror(errno)));
  }

  struct timeval tv;
  tv.tv_sec = 5;
  tv.tv_usec = 0;
  setsockopt(event_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv),
             sizeof(tv));

  std::string response;
  char buffer[4096];
  while (true) {
    const ssize_t received = recv(event_fd, buffer, sizeof(buffer), 0);
    if (received < 0) {
#ifdef _WIN32
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT) {
#else
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
        close(event_fd);
        return absl::DeadlineExceededError(
            "Timeout waiting for subscribe response");
      }
      close(event_fd);
      return absl::InternalError(
          absl::StrCat("Failed to receive response: ", strerror(errno)));
    }
    if (received == 0) {
      break;
    }
    response.append(buffer, static_cast<size_t>(received));
    if (response.size() > kMaxResponseSize) {
      close(event_fd);
      return absl::ResourceExhaustedError("Mesen2 response too large");
    }
    if (response.find('\n') != std::string::npos) {
      break;
    }
  }

  const size_t newline_pos = response.find('\n');
  if (newline_pos == std::string::npos) {
    close(event_fd);
    return absl::DeadlineExceededError("Malformed subscribe response");
  }

  auto parse_status = ParseResponse(response.substr(0, newline_pos));
  if (!parse_status.ok()) {
    close(event_fd);
    return parse_status.status();
  }

  pending_event_payload_ = response.substr(newline_pos + 1);
  event_socket_fd_ = event_fd;
  event_thread_running_ = true;
  event_thread_ = std::thread(&MesenSocketClient::EventLoop, this);
  return absl::OkStatus();
}

absl::Status MesenSocketClient::Unsubscribe() {
  event_thread_running_ = false;
  ShutdownSocketFd(event_socket_fd_);
  if (event_thread_.joinable()) {
    event_thread_.join();
  }

  if (event_socket_fd_ >= 0) {
    close(event_socket_fd_);
    event_socket_fd_ = -1;
  }
  pending_event_payload_.clear();

  return absl::OkStatus();
}

void MesenSocketClient::SetEventCallback(EventCallback callback) {
  std::lock_guard<std::mutex> lock(event_callback_mutex_);
  event_callback_ = std::move(callback);
}

void MesenSocketClient::EventLoop() {
  const int event_fd = event_socket_fd_;
  if (event_fd < 0) {
    return;
  }

  std::string pending = pending_event_payload_;
  pending_event_payload_.clear();
  char buffer[4096];

  auto dispatch_pending_lines = [&]() {
    size_t newline_pos = pending.find('\n');
    while (newline_pos != std::string::npos) {
      std::string line = pending.substr(0, newline_pos);
      pending.erase(0, newline_pos + 1);

      if (!line.empty() && line.find("\"success\"") == std::string::npos) {
        MesenEvent event;
        event.raw_json = line;
        event.type = ExtractJsonString(line, "event");
        if (event.type.empty()) {
          event.type = ExtractJsonString(line, "type");
        }
        event.address = static_cast<uint32_t>(
            ExtractJsonInt(line, "address", ExtractJsonInt(line, "addr", 0)));
        event.frame = static_cast<uint64_t>(ExtractJsonInt(line, "frame", 0));

        EventCallback callback;
        {
          std::lock_guard<std::mutex> lock(event_callback_mutex_);
          callback = event_callback_;
        }
        if (callback && !event.type.empty()) {
          callback(event);
        }
      }

      newline_pos = pending.find('\n');
    }
  };

  while (event_thread_running_) {
    dispatch_pending_lines();

    const ssize_t received = recv(event_fd, buffer, sizeof(buffer), 0);
    if (received < 0) {
#ifdef _WIN32
      const int err = WSAGetLastError();
      if (err == WSAEINTR || err == WSAEWOULDBLOCK) {
        continue;
      }
#else
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
#endif
      break;
    }

    if (received == 0) {
      break;
    }

    pending.append(buffer, static_cast<size_t>(received));
    dispatch_pending_lines();
  }
}

}  // namespace mesen
}  // namespace emu
}  // namespace yaze
