#ifndef YAZE_APP_NET_WEBSOCKET_INTERFACE_H_
#define YAZE_APP_NET_WEBSOCKET_INTERFACE_H_

#include <functional>
#include <string>

#include "absl/status/status.h"

namespace yaze {
namespace net {

/**
 * @enum WebSocketState
 * @brief WebSocket connection states
 */
enum class WebSocketState {
  kDisconnected,  // Not connected
  kConnecting,    // Connection in progress
  kConnected,     // Successfully connected
  kClosing,       // Close handshake in progress
  kClosed,        // Connection closed
  kError          // Error state
};

/**
 * @class IWebSocket
 * @brief Abstract interface for WebSocket client implementations
 *
 * This interface abstracts WebSocket operations to support both native
 * (using various libraries) and WASM (using emscripten WebSocket) implementations.
 * All methods use absl::Status for consistent error handling.
 */
class IWebSocket {
 public:
  // Callback types for WebSocket events
  using MessageCallback = std::function<void(const std::string& message)>;
  using BinaryMessageCallback = std::function<void(const uint8_t* data, size_t length)>;
  using OpenCallback = std::function<void()>;
  using CloseCallback = std::function<void(int code, const std::string& reason)>;
  using ErrorCallback = std::function<void(const std::string& error)>;

  virtual ~IWebSocket() = default;

  /**
   * @brief Connect to a WebSocket server
   * @param url The WebSocket URL (ws:// or wss://)
   * @return Status indicating success or failure
   */
  virtual absl::Status Connect(const std::string& url) = 0;

  /**
   * @brief Send a text message
   * @param message The text message to send
   * @return Status indicating success or failure
   */
  virtual absl::Status Send(const std::string& message) = 0;

  /**
   * @brief Send a binary message
   * @param data The binary data to send
   * @param length The length of the data
   * @return Status indicating success or failure
   */
  virtual absl::Status SendBinary(const uint8_t* data, size_t length) {
    // Default implementation - can be overridden if binary is supported
    return absl::UnimplementedError("Binary messages not implemented");
  }

  /**
   * @brief Close the WebSocket connection
   * @param code Optional close code (default: 1000 for normal closure)
   * @param reason Optional close reason
   * @return Status indicating success or failure
   */
  virtual absl::Status Close(int code = 1000,
                            const std::string& reason = "") = 0;

  /**
   * @brief Get the current connection state
   * @return Current WebSocket state
   */
  virtual WebSocketState GetState() const = 0;

  /**
   * @brief Check if the WebSocket is connected
   * @return true if connected, false otherwise
   */
  virtual bool IsConnected() const {
    return GetState() == WebSocketState::kConnected;
  }

  /**
   * @brief Set callback for text message events
   * @param callback Function to call when a text message is received
   */
  virtual void OnMessage(MessageCallback callback) = 0;

  /**
   * @brief Set callback for binary message events
   * @param callback Function to call when binary data is received
   */
  virtual void OnBinaryMessage(BinaryMessageCallback callback) {
    // Default implementation - can be overridden if binary is supported
    binary_message_callback_ = callback;
  }

  /**
   * @brief Set callback for connection open events
   * @param callback Function to call when connection is established
   */
  virtual void OnOpen(OpenCallback callback) = 0;

  /**
   * @brief Set callback for connection close events
   * @param callback Function to call when connection is closed
   */
  virtual void OnClose(CloseCallback callback) = 0;

  /**
   * @brief Set callback for error events
   * @param callback Function to call when an error occurs
   */
  virtual void OnError(ErrorCallback callback) = 0;

  /**
   * @brief Get the WebSocket URL
   * @return The URL this socket is connected/connecting to
   */
  virtual std::string GetUrl() const { return url_; }

  /**
   * @brief Set automatic reconnection
   * @param enable Enable or disable auto-reconnect
   * @param delay_seconds Delay between reconnection attempts
   */
  virtual void SetAutoReconnect(bool enable, int delay_seconds = 5) {
    auto_reconnect_ = enable;
    reconnect_delay_seconds_ = delay_seconds;
  }

 protected:
  std::string url_;
  WebSocketState state_ = WebSocketState::kDisconnected;

  // Callbacks (may be used by implementations)
  MessageCallback message_callback_;
  BinaryMessageCallback binary_message_callback_;
  OpenCallback open_callback_;
  CloseCallback close_callback_;
  ErrorCallback error_callback_;

  // Auto-reconnect settings
  bool auto_reconnect_ = false;
  int reconnect_delay_seconds_ = 5;
};

}  // namespace net
}  // namespace yaze

#endif  // YAZE_APP_NET_WEBSOCKET_INTERFACE_H_