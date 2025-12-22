#ifndef YAZE_APP_NET_NATIVE_HTTPLIB_WEBSOCKET_H_
#define YAZE_APP_NET_NATIVE_HTTPLIB_WEBSOCKET_H_

#include <atomic>
#include <memory>
#include <thread>

#include "app/net/websocket_interface.h"

// Forward declaration
namespace httplib {
class Client;
}

namespace yaze {
namespace net {

/**
 * @class HttpLibWebSocket
 * @brief Native WebSocket implementation using HTTP fallback
 *
 * Note: cpp-httplib doesn't have full WebSocket support, so this
 * implementation uses HTTP long-polling as a fallback. For production
 * use, consider integrating a proper WebSocket library like websocketpp
 * or libwebsockets.
 */
class HttpLibWebSocket : public IWebSocket {
 public:
  HttpLibWebSocket();
  ~HttpLibWebSocket() override;

  /**
   * @brief Connect to a WebSocket server
   * @param url The WebSocket URL (ws:// or wss://)
   * @return Status indicating success or failure
   */
  absl::Status Connect(const std::string& url) override;

  /**
   * @brief Send a text message
   * @param message The text message to send
   * @return Status indicating success or failure
   */
  absl::Status Send(const std::string& message) override;

  /**
   * @brief Send a binary message
   * @param data The binary data to send
   * @param length The length of the data
   * @return Status indicating success or failure
   */
  absl::Status SendBinary(const uint8_t* data, size_t length) override;

  /**
   * @brief Close the WebSocket connection
   * @param code Optional close code
   * @param reason Optional close reason
   * @return Status indicating success or failure
   */
  absl::Status Close(int code = 1000,
                    const std::string& reason = "") override;

  /**
   * @brief Get the current connection state
   * @return Current WebSocket state
   */
  WebSocketState GetState() const override { return state_; }

  /**
   * @brief Set callback for text message events
   * @param callback Function to call when a text message is received
   */
  void OnMessage(MessageCallback callback) override {
    message_callback_ = callback;
  }

  /**
   * @brief Set callback for binary message events
   * @param callback Function to call when binary data is received
   */
  void OnBinaryMessage(BinaryMessageCallback callback) override {
    binary_message_callback_ = callback;
  }

  /**
   * @brief Set callback for connection open events
   * @param callback Function to call when connection is established
   */
  void OnOpen(OpenCallback callback) override {
    open_callback_ = callback;
  }

  /**
   * @brief Set callback for connection close events
   * @param callback Function to call when connection is closed
   */
  void OnClose(CloseCallback callback) override {
    close_callback_ = callback;
  }

  /**
   * @brief Set callback for error events
   * @param callback Function to call when an error occurs
   */
  void OnError(ErrorCallback callback) override {
    error_callback_ = callback;
  }

 private:
  /**
   * @brief Parse WebSocket URL into HTTP components
   * @param ws_url WebSocket URL (ws:// or wss://)
   * @param http_url Output: Converted HTTP URL
   * @return Status indicating success or failure
   */
  absl::Status ParseWebSocketUrl(const std::string& ws_url,
                                 std::string& http_url);

  /**
   * @brief Background thread for receiving messages (polling)
   */
  void ReceiveLoop();

  /**
   * @brief Stop the receive loop
   */
  void StopReceiveLoop();

  // HTTP client for fallback implementation
  std::shared_ptr<httplib::Client> client_;

  // Background receive thread
  std::thread receive_thread_;
  std::atomic<bool> stop_receive_;

  // Connection details
  std::string session_id_;
  std::string http_endpoint_;
};

}  // namespace net
}  // namespace yaze

#endif  // YAZE_APP_NET_NATIVE_HTTPLIB_WEBSOCKET_H_