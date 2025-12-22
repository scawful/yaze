#ifdef __EMSCRIPTEN__

#include "app/net/wasm/emscripten_websocket.h"

#include <cstring>

namespace yaze {
namespace net {

EmscriptenWebSocket::EmscriptenWebSocket()
    : socket_(0), socket_valid_(false) {
  state_ = WebSocketState::kDisconnected;
}

EmscriptenWebSocket::~EmscriptenWebSocket() {
  if (socket_valid_ && state_ != WebSocketState::kDisconnected) {
    Close();
  }
}

EM_BOOL EmscriptenWebSocket::OnOpenCallback(
    int eventType,
    const EmscriptenWebSocketOpenEvent* websocketEvent,
    void* userData) {

  EmscriptenWebSocket* self = static_cast<EmscriptenWebSocket*>(userData);

  self->state_ = WebSocketState::kConnected;

  if (self->open_callback_) {
    self->open_callback_();
  }

  return EM_TRUE;
}

EM_BOOL EmscriptenWebSocket::OnCloseCallback(
    int eventType,
    const EmscriptenWebSocketCloseEvent* websocketEvent,
    void* userData) {

  EmscriptenWebSocket* self = static_cast<EmscriptenWebSocket*>(userData);

  self->state_ = WebSocketState::kClosed;
  self->socket_valid_ = false;

  if (self->close_callback_) {
    self->close_callback_(websocketEvent->code,
                         websocketEvent->reason ? websocketEvent->reason : "");
  }

  self->state_ = WebSocketState::kDisconnected;

  return EM_TRUE;
}

EM_BOOL EmscriptenWebSocket::OnErrorCallback(
    int eventType,
    const EmscriptenWebSocketErrorEvent* websocketEvent,
    void* userData) {

  EmscriptenWebSocket* self = static_cast<EmscriptenWebSocket*>(userData);

  self->state_ = WebSocketState::kError;

  if (self->error_callback_) {
    self->error_callback_("WebSocket error occurred");
  }

  return EM_TRUE;
}

EM_BOOL EmscriptenWebSocket::OnMessageCallback(
    int eventType,
    const EmscriptenWebSocketMessageEvent* websocketEvent,
    void* userData) {

  EmscriptenWebSocket* self = static_cast<EmscriptenWebSocket*>(userData);

  if (websocketEvent->isText) {
    // Text message
    if (self->message_callback_) {
      // Convert UTF-8 data to string
      std::string message(reinterpret_cast<const char*>(websocketEvent->data),
                         websocketEvent->numBytes);
      self->message_callback_(message);
    }
  } else {
    // Binary message
    if (self->binary_message_callback_) {
      self->binary_message_callback_(websocketEvent->data,
                                     websocketEvent->numBytes);
    }
  }

  return EM_TRUE;
}

absl::Status EmscriptenWebSocket::Connect(const std::string& url) {
  if (state_ != WebSocketState::kDisconnected) {
    return absl::FailedPreconditionError(
        "WebSocket already connected or connecting");
  }

  state_ = WebSocketState::kConnecting;
  url_ = url;

  // Create WebSocket attributes
  EmscriptenWebSocketCreateAttributes attrs = {
    url.c_str(),
    nullptr,    // protocols (NULL = default)
    EM_TRUE     // createOnMainThread
  };

  // Create the WebSocket
  socket_ = emscripten_websocket_new(&attrs);

  if (socket_ <= 0) {
    state_ = WebSocketState::kError;
    return absl::InternalError("Failed to create WebSocket");
  }

  socket_valid_ = true;

  // Set callbacks
  EMSCRIPTEN_RESULT result;

  result = emscripten_websocket_set_onopen_callback(
      socket_, this, OnOpenCallback);
  if (result != EMSCRIPTEN_RESULT_SUCCESS) {
    state_ = WebSocketState::kError;
    socket_valid_ = false;
    return absl::InternalError("Failed to set onopen callback");
  }

  result = emscripten_websocket_set_onclose_callback(
      socket_, this, OnCloseCallback);
  if (result != EMSCRIPTEN_RESULT_SUCCESS) {
    state_ = WebSocketState::kError;
    socket_valid_ = false;
    return absl::InternalError("Failed to set onclose callback");
  }

  result = emscripten_websocket_set_onerror_callback(
      socket_, this, OnErrorCallback);
  if (result != EMSCRIPTEN_RESULT_SUCCESS) {
    state_ = WebSocketState::kError;
    socket_valid_ = false;
    return absl::InternalError("Failed to set onerror callback");
  }

  result = emscripten_websocket_set_onmessage_callback(
      socket_, this, OnMessageCallback);
  if (result != EMSCRIPTEN_RESULT_SUCCESS) {
    state_ = WebSocketState::kError;
    socket_valid_ = false;
    return absl::InternalError("Failed to set onmessage callback");
  }

  // Connection is asynchronous in the browser
  // The OnOpenCallback will be called when connected

  return absl::OkStatus();
}

absl::Status EmscriptenWebSocket::Send(const std::string& message) {
  if (state_ != WebSocketState::kConnected || !socket_valid_) {
    return absl::FailedPreconditionError("WebSocket not connected");
  }

  EMSCRIPTEN_RESULT result = emscripten_websocket_send_utf8_text(
      socket_, message.c_str());

  if (result != EMSCRIPTEN_RESULT_SUCCESS) {
    return absl::InternalError("Failed to send text message");
  }

  return absl::OkStatus();
}

absl::Status EmscriptenWebSocket::SendBinary(const uint8_t* data,
                                             size_t length) {
  if (state_ != WebSocketState::kConnected || !socket_valid_) {
    return absl::FailedPreconditionError("WebSocket not connected");
  }

  EMSCRIPTEN_RESULT result = emscripten_websocket_send_binary(
      socket_, const_cast<void*>(static_cast<const void*>(data)), length);

  if (result != EMSCRIPTEN_RESULT_SUCCESS) {
    return absl::InternalError("Failed to send binary message");
  }

  return absl::OkStatus();
}

absl::Status EmscriptenWebSocket::Close(int code, const std::string& reason) {
  if (state_ == WebSocketState::kDisconnected ||
      state_ == WebSocketState::kClosed ||
      !socket_valid_) {
    return absl::OkStatus();
  }

  state_ = WebSocketState::kClosing;

  EMSCRIPTEN_RESULT result = emscripten_websocket_close(
      socket_, code, reason.c_str());

  if (result != EMSCRIPTEN_RESULT_SUCCESS) {
    // Force state to closed even if close fails
    state_ = WebSocketState::kClosed;
    socket_valid_ = false;
    return absl::InternalError("Failed to close WebSocket");
  }

  // OnCloseCallback will be called when the close completes

  return absl::OkStatus();
}

}  // namespace net
}  // namespace yaze

#endif  // __EMSCRIPTEN__