#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_collaboration.h"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <chrono>
#include <random>
#include <sstream>

#include "absl/strings/str_format.h"
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

}  // namespace

WasmCollaboration::WasmCollaboration() {
  user_id_ = GenerateUserId();
  user_color_ = GenerateUserColor();
  websocket_ = std::make_unique<net::EmscriptenWebSocket>();
}

WasmCollaboration::~WasmCollaboration() {
  if (is_connected_) {
    LeaveSession();
  }
}

absl::StatusOr<std::string> WasmCollaboration::CreateSession(
    const std::string& session_name, const std::string& username) {
  if (is_connected_) {
    return absl::FailedPreconditionError("Already connected to a session");
  }

  // Generate room code
  char* room_code_ptr = GenerateRandomRoomCode();
  room_code_ = std::string(room_code_ptr);
  free(room_code_ptr);

  session_name_ = session_name;
  username_ = username;

  // Connect to WebSocket server
  auto status = websocket_->Connect(websocket_url_);
  if (!status.ok()) {
    return status;
  }

  // Set up WebSocket callbacks
  websocket_->OnOpen([this]() {
    ConsoleLog("WebSocket connected, creating session");
    is_connected_ = true;

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

    auto send_status = websocket_->Send(msg.dump());
    if (!send_status.ok()) {
      ConsoleError("Failed to send create message");
    }

    if (status_callback_) {
      status_callback_(true, "Session created");
    }
    UpdateCollaborationUI("session_created", room_code_.c_str());
  });

  websocket_->OnMessage([this](const std::string& message) {
    HandleMessage(message);
  });

  websocket_->OnClose([this](int code, const std::string& reason) {
    is_connected_ = false;
    if (status_callback_) {
      status_callback_(false, absl::StrFormat("Disconnected: %s", reason));
    }
    UpdateCollaborationUI("disconnected", "");
  });

  websocket_->OnError([this](const std::string& error) {
    ConsoleError(error.c_str());
    is_connected_ = false;
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
                                           const std::string& username) {
  if (is_connected_) {
    return absl::FailedPreconditionError("Already connected to a session");
  }

  room_code_ = room_code;
  username_ = username;

  // Connect to WebSocket server
  auto status = websocket_->Connect(websocket_url_);
  if (!status.ok()) {
    return status;
  }

  // Set up WebSocket callbacks
  websocket_->OnOpen([this]() {
    ConsoleLog("WebSocket connected, joining session");
    is_connected_ = true;

    // Send join session message
    json msg;
    msg["type"] = "join";
    msg["room"] = room_code_;
    msg["user"] = username_;
    msg["user_id"] = user_id_;
    msg["color"] = user_color_;

    auto send_status = websocket_->Send(msg.dump());
    if (!send_status.ok()) {
      ConsoleError("Failed to send join message");
    }

    if (status_callback_) {
      status_callback_(true, "Joined session");
    }
    UpdateCollaborationUI("session_joined", room_code_.c_str());
  });

  websocket_->OnMessage([this](const std::string& message) {
    HandleMessage(message);
  });

  websocket_->OnClose([this](int code, const std::string& reason) {
    is_connected_ = false;
    if (status_callback_) {
      status_callback_(false, absl::StrFormat("Disconnected: %s", reason));
    }
    UpdateCollaborationUI("disconnected", "");
  });

  websocket_->OnError([this](const std::string& error) {
    ConsoleError(error.c_str());
    is_connected_ = false;
    if (status_callback_) {
      status_callback_(false, error);
    }
  });

  // Note: is_connected_ will be set in OnOpen callback
  UpdateCollaborationUI("session_joining", room_code_.c_str());
  return absl::OkStatus();
}

absl::Status WasmCollaboration::LeaveSession() {
  if (!is_connected_) {
    return absl::FailedPreconditionError("Not connected to a session");
  }

  // Send leave message
  json msg;
  msg["type"] = "leave";
  msg["room"] = room_code_;
  msg["user_id"] = user_id_;

  auto status = websocket_->Send(msg.dump());
  if (!status.ok()) {
    ConsoleError("Failed to send leave message");
  }

  // Close WebSocket connection
  websocket_->Close();
  is_connected_ = false;

  // Clear state
  room_code_.clear();
  session_name_.clear();

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
  if (!is_connected_) {
    return absl::FailedPreconditionError("Not connected to a session");
  }

  if (old_data.size() > max_change_size_ || new_data.size() > max_change_size_) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Change size exceeds maximum of %d bytes", max_change_size_));
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

  auto status = websocket_->Send(msg.dump());
  if (!status.ok()) {
    return absl::InternalError("Failed to send change");
  }

  UpdateUserActivity(user_id_);
  return absl::OkStatus();
}

absl::Status WasmCollaboration::SendCursorPosition(
    const std::string& editor_type, int x, int y, int map_id) {
  if (!is_connected_) {
    return absl::FailedPreconditionError("Not connected to a session");
  }

  // Rate limit cursor updates
  double now = GetCurrentTime();
  if (now - last_cursor_send_ < cursor_send_interval_) {
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
    return absl::InternalError("Failed to send cursor position");
  }

  UpdateUserActivity(user_id_);
  return absl::OkStatus();
}

std::vector<WasmCollaboration::User> WasmCollaboration::GetConnectedUsers() const {
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

  bool users_changed = false;
  for (auto& [id, user] : users_) {
    if (user.is_active && (now - user.last_activity) > user_timeout_seconds_) {
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
    ConsoleError(absl::StrFormat("Change at offset %u exceeds ROM size",
                                change.offset).c_str());
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

  // Apply the change to the ROM
  for (size_t i = 0; i < change.new_data.size(); ++i) {
    rom_->WriteByte(change.offset + i, change.new_data[i]);
  }

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

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__