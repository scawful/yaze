#ifndef YAZE_APP_NET_WASM_EMSCRIPTEN_WEBSOCKET_H_
#define YAZE_APP_NET_WASM_EMSCRIPTEN_WEBSOCKET_H_

#ifdef __EMSCRIPTEN__

#include <emscripten/websocket.h>

#include "app/net/websocket_interface.h"

namespace yaze {
namespace net {

/**
 * @class EmscriptenWebSocket
 * @brief WASM WebSocket implementation using Emscripten WebSocket API
 *
 * This implementation wraps the Emscripten WebSocket API which provides
 * direct access to the browser's native WebSocket implementation.
 */
class EmscriptenWebSocket : public IWebSocket {
 public:
  EmscriptenWebSocket();
  ~EmscriptenWebSocket() override;

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
  // Emscripten WebSocket callbacks (static, with user data)
  static EM_BOOL OnOpenCallback(int eventType,
                                const EmscriptenWebSocketOpenEvent* websocketEvent,
                                void* userData);

  static EM_BOOL OnCloseCallback(int eventType,
                                 const EmscriptenWebSocketCloseEvent* websocketEvent,
                                 void* userData);

  static EM_BOOL OnErrorCallback(int eventType,
                                 const EmscriptenWebSocketErrorEvent* websocketEvent,
                                 void* userData);

  static EM_BOOL OnMessageCallback(int eventType,
                                   const EmscriptenWebSocketMessageEvent* websocketEvent,
                                   void* userData);

  // Emscripten WebSocket handle
  EMSCRIPTEN_WEBSOCKET_T socket_;

  // Track if socket is valid
  bool socket_valid_;
};

}  // namespace net
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_NET_WASM_EMSCRIPTEN_WEBSOCKET_H_