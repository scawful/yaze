#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_collaboration.h"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <chrono>
#include <cmath>
#include <random>
#include <sstream>

#include "absl/strings/str_format.h"
#include "app/platform/wasm/wasm_config.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// clang-format off
EM_JS(double, GetCurrentTime, (), {
  return Date.now() / 1000.0;
});

EM_JS(void, ConsoleLog, (const char* message), {
  console.log('[WasmCollaboration] ' + UTF8ToString(message));
});

EM_JS(void, ConsoleError, (const char* message), {
  console.error('[WasmCollaboration] ' + UTF8ToString(message));
});

EM_JS(void, UpdateCollaborationUI, (const char* type, const char* data), {
  if (typeof window.updateCollaborationUI === 'function') {
    window.updateCollaborationUI(UTF8ToString(type), UTF8ToString(data));
  }
});

EM_JS(char*, GenerateRandomRoomCode, (), {
  var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
  var result = '';
  for (var i = 0; i < 6; i++) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  var lengthBytes = lengthBytesUTF8(result) + 1;
  var stringOnWasmHeap = _malloc(lengthBytes);
  stringToUTF8(result, stringOnWasmHeap, lengthBytes);
  return stringOnWasmHeap;
});

EM_JS(char*, GetCollaborationServerUrl, (), {
  // Check for configuration in order of precedence:
  // 1. window.YAZE_CONFIG.collaborationServerUrl
  // 2. Environment variable via meta tag
  // 3. Default empty (disabled)
  var url = '';

  if (typeof window !== 'undefined') {
    if (window.YAZE_CONFIG && window.YAZE_CONFIG.collaborationServerUrl) {
      url = window.YAZE_CONFIG.collaborationServerUrl;
    } else {
      // Check for meta tag configuration
      var meta = document.querySelector('meta[name="yaze-collab-server"]');
      if (meta && meta.content) {
        url = meta.content;
      }
    }
  }

  if (url.length === 0) {
    return null;
  }

  var lengthBytes = lengthBytesUTF8(url) + 1;
  var stringOnWasmHeap = _malloc(lengthBytes);
  stringToUTF8(url, stringOnWasmHeap, lengthBytes);
  return stringOnWasmHeap;
});
// clang-format on

namespace yaze {
namespace app {
namespace platform {

namespace {

// Color palette for user cursors
const std::vector<std::string> kUserColors = {
    "#FF6B6B",  // Red
    "#4ECDC4",  // Teal
    "#45B7D1",  // Blue
    "#96CEB4",  // Green
    "#FFEAA7",  // Yellow
    "#DDA0DD",  // Plum
    "#98D8C8",  // Mint
    "#F7DC6F",  // Gold
};

WasmCollaboration& GetInstance() {
  static WasmCollaboration instance;
  return instance;
}

}  // namespace

WasmCollaboration& GetWasmCollaborationInstance() {
  return GetInstance();
}

WasmCollaboration::WasmCollaboration() {
  user_id_ = GenerateUserId();
  user_color_ = GenerateUserColor();
  websocket_ = std::make_unique<net::EmscriptenWebSocket>();

  // Try to initialize from config automatically
  InitializeFromConfig();
}

WasmCollaboration::~WasmCollaboration() {
  if (is_connected_) {
    LeaveSession();
  }
}

void WasmCollaboration::InitializeFromConfig() {
  char* url = GetCollaborationServerUrl();
  if (url != nullptr) {
    websocket_url_ = std::string(url);
    free(url);
    ConsoleLog(("Collaboration server configured: " + websocket_url_).c_str());
  } else {
    ConsoleLog(
        "Collaboration server not configured. Set "
        "window.YAZE_CONFIG.collaborationServerUrl or add <meta "
        "name=\"yaze-collab-server\" content=\"wss://...\"> to enable.");
  }
}

absl::StatusOr<std::string> WasmCollaboration::CreateSession(
    const std::string& session_name, const std::string& username,
    const std::string& password) {
  if (is_connected_ || connection_state_ == ConnectionState::Connecting) {
    return absl::FailedPreconditionError(
        "Already connected or connecting to a session");
  }

  if (!IsConfigured()) {
    return absl::FailedPreconditionError(
        "Collaboration server not configured. Set "
        "window.YAZE_CONFIG.collaborationServerUrl "
        "or call SetWebSocketUrl() before creating a session.");
  }

  // Generate room code
  char* room_code_ptr = GenerateRandomRoomCode();
  room_code_ = std::string(room_code_ptr);
  free(room_code_ptr);

  session_name_ = session_name;
  username_ = username;
  stored_password_ = password;
  should_reconnect_ = true;  // Enable auto-reconnect for this session

  UpdateConnectionState(ConnectionState::Connecting, "Creating session...");

  // Connect to WebSocket server
  auto status = websocket_->Connect(websocket_url_);
  if (!status.ok()) {
    return status;
  }

  // Set up WebSocket callbacks
  websocket_->OnOpen([this, password]() {
    ConsoleLog("WebSocket connected, creating session");
    is_connected_ = true;
    UpdateConnectionState(ConnectionState::Connected, "Connected");

    // Add self to users list
    User self_user;
    self_user.id = user_id_;
    self_user.name = username_;
    self_user.color = user_color_;
    self_user.is_active = true;
    self_user.last_activity = GetCurrentTime();

    {
      std::lock_guard<std::mutex> lock(users_mutex_);
      users_[user_id_] = self_user;
    }

    // Send create session message
    json msg;
    msg["type"] = "create";
    msg["room"] = room_code_;
    msg["name"] = session_name_;
    msg["user"] = username_;
    msg["user_id"] = user_id_;
    msg["color"] = user_color_;
    if (!password.empty()) {
      msg["password"] = password;
    }

    auto send_status = websocket_->Send(msg.dump());
    if (!send_status.ok()) {
      ConsoleError("Failed to send create message");
    }

    if (status_callback_) {
      status_callback_(true, "Session created");
    }
    UpdateCollaborationUI("session_created", room_code_.c_str());
  });

  websocket_->OnMessage(
      [this](const std::string& message) { HandleMessage(message); });

  websocket_->OnClose([this](int code, const std::string& reason) {
    is_connected_ = false;
    ConsoleLog(absl::StrFormat("WebSocket closed: %s (code: %d)", reason, code)
                   .c_str());

    // Initiate reconnection if enabled
    if (should_reconnect_) {
      InitiateReconnection();
    } else {
      UpdateConnectionState(ConnectionState::Disconnected,
                            absl::StrFormat("Disconnected: %s", reason));
    }

    if (status_callback_) {
      status_callback_(false, absl::StrFormat("Disconnected: %s", reason));
    }
    UpdateCollaborationUI("disconnected", "");
  });

  websocket_->OnError([this](const std::string& error) {
    ConsoleError(error.c_str());
    is_connected_ = false;

    // Initiate reconnection on error
    if (should_reconnect_) {
      InitiateReconnection();
    } else {
      UpdateConnectionState(ConnectionState::Disconnected, error);
    }

    if (status_callback_) {
      status_callback_(false, error);
    }
  });

  // Note: is_connected_ will be set to true in OnOpen callback when connection is established
  // For now, mark as "connecting" state by returning the room code
  // The actual connected state is confirmed in HandleMessage when create_response is received

  UpdateCollaborationUI("session_creating", room_code_.c_str());
  return room_code_;
}

absl::Status WasmCollaboration::JoinSession(const std::string& room_code,
                                            const std::string& username,
                                            const std::string& password) {
  if (is_connected_ || connection_state_ == ConnectionState::Connecting) {
    return absl::FailedPreconditionError(
        "Already connected or connecting to a session");
  }

  if (!IsConfigured()) {
    return absl::FailedPreconditionError(
        "Collaboration server not configured. Set "
        "window.YAZE_CONFIG.collaborationServerUrl "
        "or call SetWebSocketUrl() before joining a session.");
  }

  room_code_ = room_code;
  username_ = username;
  stored_password_ = password;
  should_reconnect_ = true;  // Enable auto-reconnect for this session

  UpdateConnectionState(ConnectionState::Connecting, "Joining session...");

  // Connect to WebSocket server
  auto status = websocket_->Connect(websocket_url_);
  if (!status.ok()) {
    return status;
  }

  // Set up WebSocket callbacks
  websocket_->OnOpen([this, password]() {
    ConsoleLog("WebSocket connected, joining session");
    is_connected_ = true;
    UpdateConnectionState(ConnectionState::Connected, "Connected");

    // Send join session message
    json msg;
    msg["type"] = "join";
    msg["room"] = room_code_;
    msg["user"] = username_;
    msg["user_id"] = user_id_;
    msg["color"] = user_color_;
    if (!password.empty()) {
      msg["password"] = password;
    }

    auto send_status = websocket_->Send(msg.dump());
    if (!send_status.ok()) {
      ConsoleError("Failed to send join message");
    }

    if (status_callback_) {
      status_callback_(true, "Joined session");
    }
    UpdateCollaborationUI("session_joined", room_code_.c_str());
  });

  websocket_->OnMessage(
      [this](const std::string& message) { HandleMessage(message); });

  websocket_->OnClose([this](int code, const std::string& reason) {
    is_connected_ = false;
    ConsoleLog(absl::StrFormat("WebSocket closed: %s (code: %d)", reason, code)
                   .c_str());

    // Initiate reconnection if enabled
    if (should_reconnect_) {
      InitiateReconnection();
    } else {
      UpdateConnectionState(ConnectionState::Disconnected,
                            absl::StrFormat("Disconnected: %s", reason));
    }

    if (status_callback_) {
      status_callback_(false, absl::StrFormat("Disconnected: %s", reason));
    }
    UpdateCollaborationUI("disconnected", "");
  });

  websocket_->OnError([this](const std::string& error) {
    ConsoleError(error.c_str());
    is_connected_ = false;

    // Initiate reconnection on error
    if (should_reconnect_) {
      InitiateReconnection();
    } else {
      UpdateConnectionState(ConnectionState::Disconnected, error);
    }

    if (status_callback_) {
      status_callback_(false, error);
    }
  });

  // Note: is_connected_ will be set in OnOpen callback
  UpdateCollaborationUI("session_joining", room_code_.c_str());
  return absl::OkStatus();
}

absl::Status WasmCollaboration::LeaveSession() {
  if (!is_connected_ && connection_state_ != ConnectionState::Connecting &&
      connection_state_ != ConnectionState::Reconnecting) {
    return absl::FailedPreconditionError("Not connected to a session");
  }

  // Disable auto-reconnect when explicitly leaving
  should_reconnect_ = false;

  // Send leave message if connected
  if (is_connected_) {
    json msg;
    msg["type"] = "leave";
    msg["room"] = room_code_;
    msg["user_id"] = user_id_;

    auto status = websocket_->Send(msg.dump());
    if (!status.ok()) {
      ConsoleError("Failed to send leave message");
    }
  }

  // Close WebSocket connection
  if (websocket_) {
    websocket_->Close();
  }
  is_connected_ = false;
  UpdateConnectionState(ConnectionState::Disconnected, "Left session");

  // Clear state
  room_code_.clear();
  session_name_.clear();
  stored_password_.clear();
  ResetReconnectionState();

  {
    std::lock_guard<std::mutex> lock(users_mutex_);
    users_.clear();
  }

  {
    std::lock_guard<std::mutex> lock(cursors_mutex_);
    cursors_.clear();
  }

  if (status_callback_) {
    status_callback_(false, "Left session");
  }

  UpdateCollaborationUI("session_left", "");
  return absl::OkStatus();
}

absl::Status WasmCollaboration::BroadcastChange(
    uint32_t offset, const std::vector<uint8_t>& old_data,
    const std::vector<uint8_t>& new_data) {
  size_t max_size = WasmConfig::Get().collaboration.max_change_size_bytes;
  if (old_data.size() > max_size || new_data.size() > max_size) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Change size exceeds maximum of %d bytes", max_size));
  }

  // Create change message
  json msg;
  msg["type"] = "change";
  msg["room"] = room_code_;
  msg["user_id"] = user_id_;
  msg["offset"] = offset;
  msg["old_data"] = old_data;
  msg["new_data"] = new_data;
  msg["timestamp"] = GetCurrentTime();

  std::string message = msg.dump();

  // If disconnected, queue the message for later
  if (!is_connected_) {
    if (connection_state_ == ConnectionState::Reconnecting) {
      QueueMessageWhileDisconnected(message);
      return absl::OkStatus();  // Queued successfully
    } else {
      return absl::FailedPreconditionError("Not connected to a session");
    }
  }

  auto status = websocket_->Send(message);
  if (!status.ok()) {
    // Try to queue on send failure
    if (connection_state_ == ConnectionState::Reconnecting) {
      QueueMessageWhileDisconnected(message);
      return absl::OkStatus();
    }
    return absl::InternalError("Failed to send change");
  }

  UpdateUserActivity(user_id_);
  return absl::OkStatus();
}

absl::Status WasmCollaboration::SendCursorPosition(
    const std::string& editor_type, int x, int y, int map_id) {
  // Don't queue cursor updates during reconnection - they're transient
  if (!is_connected_) {
    if (connection_state_ == ConnectionState::Reconnecting) {
      return absl::OkStatus();  // Silently drop during reconnection
    }
    return absl::FailedPreconditionError("Not connected to a session");
  }

  // Rate limit cursor updates
  double now = GetCurrentTime();
  double cursor_interval =
      WasmConfig::Get().collaboration.cursor_send_interval_seconds;
  if (now - last_cursor_send_ < cursor_interval) {
    return absl::OkStatus();  // Silently skip
  }
  last_cursor_send_ = now;

  // Send cursor update
  json msg;
  msg["type"] = "cursor";
  msg["room"] = room_code_;
  msg["user_id"] = user_id_;
  msg["editor"] = editor_type;
  msg["x"] = x;
  msg["y"] = y;
  if (map_id >= 0) {
    msg["map_id"] = map_id;
  }

  auto status = websocket_->Send(msg.dump());
  if (!status.ok()) {
    // Don't fail on cursor send errors during reconnection
    if (connection_state_ == ConnectionState::Reconnecting) {
      return absl::OkStatus();
    }
    return absl::InternalError("Failed to send cursor position");
  }

  UpdateUserActivity(user_id_);
  return absl::OkStatus();
}

std::vector<WasmCollaboration::User> WasmCollaboration::GetConnectedUsers()
    const {
  std::lock_guard<std::mutex> lock(users_mutex_);
  std::vector<User> result;
  for (const auto& [id, user] : users_) {
    if (user.is_active) {
      result.push_back(user);
    }
  }
  return result;
}

bool WasmCollaboration::IsConnected() const {
  return is_connected_ && websocket_ && websocket_->IsConnected();
}

void WasmCollaboration::ProcessPendingChanges() {
  std::vector<ChangeEvent> changes_to_apply;

  {
    std::lock_guard<std::mutex> lock(changes_mutex_);
    changes_to_apply = std::move(pending_changes_);
    pending_changes_.clear();
  }

  for (const auto& change : changes_to_apply) {
    if (IsChangeValid(change)) {
      ApplyRemoteChange(change);
    }
  }

  // Check for user timeouts
  CheckUserTimeouts();
}

void WasmCollaboration::HandleMessage(const std::string& message) {
  try {
    json msg = json::parse(message);
    std::string type = msg["type"];

    if (type == "create_response") {
      // Session created successfully
      if (msg["success"]) {
        session_name_ = msg["session_name"];
        ConsoleLog("Session created successfully");
      } else {
        ConsoleError(msg["error"].get<std::string>().c_str());
        is_connected_ = false;
      }
    } else if (type == "join_response") {
      // Joined session successfully
      if (msg["success"]) {
        session_name_ = msg["session_name"];
        ConsoleLog("Joined session successfully");
      } else {
        ConsoleError(msg["error"].get<std::string>().c_str());
        is_connected_ = false;
      }
    } else if (type == "users") {
      // User list update
      std::lock_guard<std::mutex> lock(users_mutex_);
      users_.clear();

      for (const auto& user_data : msg["list"]) {
        User user;
        user.id = user_data["id"];
        user.name = user_data["name"];
        user.color = user_data["color"];
        user.is_active = user_data["active"];
        user.last_activity = GetCurrentTime();
        users_[user.id] = user;
      }

      if (user_list_callback_) {
        user_list_callback_(GetConnectedUsers());
      }

      // Update UI with user list
      json ui_data;
      ui_data["users"] = msg["list"];
      UpdateCollaborationUI("users_update", ui_data.dump().c_str());

    } else if (type == "change") {
      // ROM change from another user
      if (msg["user_id"] != user_id_) {  // Don't process our own changes
        ChangeEvent change;
        change.offset = msg["offset"];
        change.old_data = msg["old_data"].get<std::vector<uint8_t>>();
        change.new_data = msg["new_data"].get<std::vector<uint8_t>>();
        change.user_id = msg["user_id"];
        change.timestamp = msg["timestamp"];

        {
          std::lock_guard<std::mutex> lock(changes_mutex_);
          pending_changes_.push_back(change);
        }

        UpdateUserActivity(change.user_id);
      }
    } else if (type == "cursor") {
      // Cursor position update
      if (msg["user_id"] != user_id_) {  // Don't process our own cursor
        CursorInfo cursor;
        cursor.user_id = msg["user_id"];
        cursor.editor_type = msg["editor"];
        cursor.x = msg["x"];
        cursor.y = msg["y"];
        if (msg.contains("map_id")) {
          cursor.map_id = msg["map_id"];
        }

        {
          std::lock_guard<std::mutex> lock(cursors_mutex_);
          cursors_[cursor.user_id] = cursor;
        }

        if (cursor_callback_) {
          cursor_callback_(cursor);
        }

        // Update UI with cursor position
        json ui_data;
        ui_data["user_id"] = cursor.user_id;
        ui_data["editor"] = cursor.editor_type;
        ui_data["x"] = cursor.x;
        ui_data["y"] = cursor.y;
        UpdateCollaborationUI("cursor_update", ui_data.dump().c_str());

        UpdateUserActivity(cursor.user_id);
      }
    } else if (type == "error") {
      ConsoleError(msg["message"].get<std::string>().c_str());
      if (status_callback_) {
        status_callback_(false, msg["message"]);
      }
    }
  } catch (const json::exception& e) {
    ConsoleError(absl::StrFormat("JSON parse error: %s", e.what()).c_str());
  }
}

std::string WasmCollaboration::GenerateUserId() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);

  std::stringstream ss;
  ss << "user_";
  for (int i = 0; i < 8; ++i) {
    ss << std::hex << dis(gen);
  }
  return ss.str();
}

std::string WasmCollaboration::GenerateUserColor() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, kUserColors.size() - 1);
  return kUserColors[dis(gen)];
}

void WasmCollaboration::UpdateUserActivity(const std::string& user_id) {
  std::lock_guard<std::mutex> lock(users_mutex_);
  if (users_.find(user_id) != users_.end()) {
    users_[user_id].last_activity = GetCurrentTime();
    users_[user_id].is_active = true;
  }
}

void WasmCollaboration::CheckUserTimeouts() {
  double now = GetCurrentTime();
  std::lock_guard<std::mutex> lock(users_mutex_);

  double timeout = WasmConfig::Get().collaboration.user_timeout_seconds;
  bool users_changed = false;
  for (auto& [id, user] : users_) {
    if (user.is_active && (now - user.last_activity) > timeout) {
      user.is_active = false;
      users_changed = true;
    }
  }

  if (users_changed && user_list_callback_) {
    user_list_callback_(GetConnectedUsers());
  }
}

bool WasmCollaboration::IsChangeValid(const ChangeEvent& change) {
  // Validate change doesn't exceed ROM bounds
  if (!rom_) {
    return false;
  }

  if (change.offset + change.new_data.size() > rom_->size()) {
    ConsoleError(
        absl::StrFormat("Change at offset %u exceeds ROM size", change.offset)
            .c_str());
    return false;
  }

  // Could add more validation here (e.g., check if area is editable)
  return true;
}

void WasmCollaboration::ApplyRemoteChange(const ChangeEvent& change) {
  if (!rom_) {
    ConsoleError("ROM not set, cannot apply changes");
    return;
  }

  applying_remote_change_ = true;
  // Apply the change to the ROM
  for (size_t i = 0; i < change.new_data.size(); ++i) {
    rom_->WriteByte(change.offset + i, change.new_data[i]);
  }
  applying_remote_change_ = false;

  // Notify the UI about the change
  if (change_callback_) {
    change_callback_(change);
  }

  // Update UI with change info
  json ui_data;
  ui_data["offset"] = change.offset;
  ui_data["size"] = change.new_data.size();
  ui_data["user_id"] = change.user_id;
  UpdateCollaborationUI("change_applied", ui_data.dump().c_str());
}

void WasmCollaboration::UpdateConnectionState(ConnectionState new_state,
                                              const std::string& message) {
  connection_state_ = new_state;

  // Notify via callback
  if (connection_state_callback_) {
    connection_state_callback_(new_state, message);
  }

  // Update UI
  std::string state_str;
  switch (new_state) {
    case ConnectionState::Disconnected:
      state_str = "disconnected";
      break;
    case ConnectionState::Connecting:
      state_str = "connecting";
      break;
    case ConnectionState::Connected:
      state_str = "connected";
      break;
    case ConnectionState::Reconnecting:
      state_str = "reconnecting";
      break;
  }

  json ui_data;
  ui_data["state"] = state_str;
  ui_data["message"] = message;
  UpdateCollaborationUI("connection_state", ui_data.dump().c_str());
}

void WasmCollaboration::InitiateReconnection() {
  if (!should_reconnect_ || room_code_.empty()) {
    UpdateConnectionState(ConnectionState::Disconnected, "Disconnected");
    return;
  }

  if (reconnection_attempts_ >= max_reconnection_attempts_) {
    ConsoleError(
        absl::StrFormat("Max reconnection attempts reached (%d), giving up",
                        max_reconnection_attempts_)
            .c_str());
    UpdateConnectionState(ConnectionState::Disconnected,
                          "Reconnection failed - max attempts reached");
    ResetReconnectionState();
    return;
  }

  reconnection_attempts_++;
  UpdateConnectionState(
      ConnectionState::Reconnecting,
      absl::StrFormat("Reconnecting... (attempt %d/%d)", reconnection_attempts_,
                      max_reconnection_attempts_));

  // Calculate delay with exponential backoff
  double delay = std::min(
      reconnection_delay_seconds_ * std::pow(2, reconnection_attempts_ - 1),
      max_reconnection_delay_);

  ConsoleLog(absl::StrFormat("Will reconnect in %.1f seconds (attempt %d)",
                             delay, reconnection_attempts_)
                 .c_str());

  // Schedule reconnection using emscripten_set_timeout
  emscripten_async_call(
      [](void* arg) {
        WasmCollaboration* self = static_cast<WasmCollaboration*>(arg);
        self->AttemptReconnection();
      },
      this, delay * 1000);  // Convert to milliseconds
}

void WasmCollaboration::AttemptReconnection() {
  if (is_connected_ || connection_state_ == ConnectionState::Connected) {
    // Already reconnected somehow
    ResetReconnectionState();
    return;
  }

  ConsoleLog(absl::StrFormat("Attempting to reconnect to room %s", room_code_)
                 .c_str());

  // Create new websocket instance
  websocket_ = std::make_unique<net::EmscriptenWebSocket>();

  // Attempt connection
  auto status = websocket_->Connect(websocket_url_);
  if (!status.ok()) {
    ConsoleError(
        absl::StrFormat("Reconnection failed: %s", status.message()).c_str());
    InitiateReconnection();  // Schedule next attempt
    return;
  }

  // Set up WebSocket callbacks for reconnection
  websocket_->OnOpen([this]() {
    ConsoleLog("WebSocket reconnected, rejoining session");
    is_connected_ = true;
    UpdateConnectionState(ConnectionState::Connected,
                          "Reconnected successfully");

    // Send rejoin message
    json msg;
    msg["type"] = "join";
    msg["room"] = room_code_;
    msg["user"] = username_;
    msg["user_id"] = user_id_;
    msg["color"] = user_color_;
    if (!stored_password_.empty()) {
      msg["password"] = stored_password_;
    }
    msg["rejoin"] = true;  // Indicate this is a reconnection

    auto send_status = websocket_->Send(msg.dump());
    if (!send_status.ok()) {
      ConsoleError("Failed to send rejoin message");
    }

    // Reset reconnection state on success
    ResetReconnectionState();

    // Send any queued messages
    std::vector<std::string> messages_to_send;
    {
      std::lock_guard<std::mutex> lock(message_queue_mutex_);
      messages_to_send = std::move(queued_messages_);
      queued_messages_.clear();
    }

    for (const auto& msg : messages_to_send) {
      websocket_->Send(msg);
    }

    if (status_callback_) {
      status_callback_(true, "Reconnected to session");
    }
    UpdateCollaborationUI("session_reconnected", room_code_.c_str());
  });

  websocket_->OnMessage(
      [this](const std::string& message) { HandleMessage(message); });

  websocket_->OnClose([this](int code, const std::string& reason) {
    is_connected_ = false;
    ConsoleLog(
        absl::StrFormat("Reconnection WebSocket closed: %s", reason).c_str());

    // Attempt reconnection again
    InitiateReconnection();

    if (status_callback_) {
      status_callback_(false, absl::StrFormat("Disconnected: %s", reason));
    }
  });

  websocket_->OnError([this](const std::string& error) {
    ConsoleError(absl::StrFormat("Reconnection error: %s", error).c_str());
    is_connected_ = false;

    // Attempt reconnection again
    InitiateReconnection();

    if (status_callback_) {
      status_callback_(false, error);
    }
  });
}

void WasmCollaboration::ResetReconnectionState() {
  reconnection_attempts_ = 0;
  reconnection_delay_seconds_ = 1.0;  // Reset to initial delay
}

void WasmCollaboration::QueueMessageWhileDisconnected(
    const std::string& message) {
  std::lock_guard<std::mutex> lock(message_queue_mutex_);

  // Limit queue size to prevent memory issues
  if (queued_messages_.size() >= max_queued_messages_) {
    ConsoleLog("Message queue full, dropping oldest message");
    queued_messages_.erase(queued_messages_.begin());
  }

  queued_messages_.push_back(message);
  ConsoleLog(absl::StrFormat("Queued message for reconnection (queue size: %d)",
                             queued_messages_.size())
                 .c_str());
}

// ---------------------------------------------------------------------------
// JS bindings for WASM (exported with EMSCRIPTEN_KEEPALIVE)
// ---------------------------------------------------------------------------
extern "C" {

EMSCRIPTEN_KEEPALIVE const char* WasmCollaborationCreate(
    const char* session_name, const char* username, const char* password) {
  static std::string last_room_code;
  if (!session_name || !username) {
    ConsoleError("Invalid session/user parameters");
    return nullptr;
  }
  auto& collab = GetInstance();
  auto result = collab.CreateSession(session_name, username,
                                     password ? std::string(password) : "");
  if (!result.ok()) {
    ConsoleError(std::string(result.status().message()).c_str());
    return nullptr;
  }
  last_room_code = *result;
  return last_room_code.c_str();
}

EMSCRIPTEN_KEEPALIVE int WasmCollaborationJoin(const char* room_code,
                                               const char* username,
                                               const char* password) {
  if (!room_code || !username) {
    ConsoleError("room_code and username are required");
    return 0;
  }
  auto& collab = GetInstance();
  auto status = collab.JoinSession(room_code, username,
                                   password ? std::string(password) : "");
  if (!status.ok()) {
    ConsoleError(std::string(status.message()).c_str());
    return 0;
  }
  return 1;
}

EMSCRIPTEN_KEEPALIVE int WasmCollaborationLeave() {
  auto& collab = GetInstance();
  auto status = collab.LeaveSession();
  return status.ok() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int WasmCollaborationSendCursor(const char* editor_type,
                                                     int x, int y, int map_id) {
  auto& collab = GetInstance();
  auto status = collab.SendCursorPosition(editor_type ? editor_type : "unknown",
                                          x, y, map_id);
  return status.ok() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int WasmCollaborationBroadcastChange(
    uint32_t offset, const uint8_t* new_data, size_t length) {
  if (!new_data && length > 0) {
    return 0;
  }
  auto& collab = GetInstance();
  std::vector<uint8_t> data;
  data.reserve(length);
  for (size_t i = 0; i < length; ++i) {
    data.push_back(new_data[i]);
  }
  std::vector<uint8_t> old_data;  // Not tracked in WASM path
  auto status = collab.BroadcastChange(offset, old_data, data);
  return status.ok() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE void WasmCollaborationSetServerUrl(const char* url) {
  if (!url)
    return;
  auto& collab = GetInstance();
  collab.SetWebSocketUrl(std::string(url));
}

EMSCRIPTEN_KEEPALIVE int WasmCollaborationIsConnected() {
  return GetInstance().IsConnected() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE const char* WasmCollaborationGetRoomCode() {
  static std::string room;
  room = GetInstance().GetRoomCode();
  return room.c_str();
}

EMSCRIPTEN_KEEPALIVE const char* WasmCollaborationGetUserId() {
  static std::string user;
  user = GetInstance().GetUserId();
  return user.c_str();
}

}  // extern "C"

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__
