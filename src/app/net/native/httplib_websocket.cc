#include "app/net/native/httplib_websocket.h"

#include <chrono>
#include <regex>

#include "util/macro.h"  // For RETURN_IF_ERROR

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include "httplib.h"

namespace yaze {
namespace net {

HttpLibWebSocket::HttpLibWebSocket() : stop_receive_(false) {
  state_ = WebSocketState::kDisconnected;
}

HttpLibWebSocket::~HttpLibWebSocket() {
  if (state_ != WebSocketState::kDisconnected) {
    Close();
  }
}

absl::Status HttpLibWebSocket::ParseWebSocketUrl(const std::string& ws_url,
                                                 std::string& http_url) {
  // Convert ws:// to http:// and wss:// to https://
  std::regex ws_regex(R"(^(wss?)://(.+)$)");
  std::smatch matches;

  if (!std::regex_match(ws_url, matches, ws_regex)) {
    return absl::InvalidArgumentError("Invalid WebSocket URL: " + ws_url);
  }

  std::string scheme = matches[1].str();
  std::string rest = matches[2].str();

  if (scheme == "ws") {
    http_url = "http://" + rest;
  } else if (scheme == "wss") {
    http_url = "https://" + rest;
  } else {
    return absl::InvalidArgumentError("Invalid WebSocket scheme: " + scheme);
  }

  url_ = ws_url;
  return absl::OkStatus();
}

absl::Status HttpLibWebSocket::Connect(const std::string& url) {
  if (state_ != WebSocketState::kDisconnected) {
    return absl::FailedPreconditionError(
        "WebSocket already connected or connecting");
  }

  state_ = WebSocketState::kConnecting;

  // Convert WebSocket URL to HTTP URL
  RETURN_IF_ERROR(ParseWebSocketUrl(url, http_endpoint_));

  // Parse HTTP URL to extract host and port
  std::regex url_regex(R"(^(https?)://([^:/\s]+)(?::(\d+))?(/.*)?)$)");
  std::smatch matches;

  if (!std::regex_match(http_endpoint_, matches, url_regex)) {
    state_ = WebSocketState::kError;
    return absl::InvalidArgumentError("Invalid HTTP URL: " + http_endpoint_);
  }

  std::string scheme = matches[1].str();
  std::string host = matches[2].str();
  int port = matches[3].matched ? std::stoi(matches[3].str())
                                : (scheme == "https" ? 443 : 80);

  // Create HTTP client
  if (scheme == "https") {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    client_ = std::make_shared<httplib::Client>(host, port);
    client_->enable_server_certificate_verification(false); // For development
#else
    state_ = WebSocketState::kError;
    return absl::UnimplementedError(
        "WSS not supported: OpenSSL support not compiled in");
#endif
  } else {
    client_ = std::make_shared<httplib::Client>(host, port);
  }

  if (!client_) {
    state_ = WebSocketState::kError;
    return absl::InternalError("Failed to create HTTP client");
  }

  // Set reasonable timeouts
  client_->set_connection_timeout(10);
  client_->set_read_timeout(30);

  // Note: This is a simplified implementation. A real WebSocket implementation
  // would perform the WebSocket handshake here. For now, we'll use HTTP
  // long-polling as a fallback.

  // Generate session ID for this connection
  session_id_ = "session_" + std::to_string(
      std::chrono::system_clock::now().time_since_epoch().count());

  state_ = WebSocketState::kConnected;

  // Call open callback if set
  if (open_callback_) {
    open_callback_();
  }

  // Start receive loop in background thread
  stop_receive_ = false;
  receive_thread_ = std::thread([this]() { ReceiveLoop(); });

  return absl::OkStatus();
}

absl::Status HttpLibWebSocket::Send(const std::string& message) {
  if (state_ != WebSocketState::kConnected) {
    return absl::FailedPreconditionError("WebSocket not connected");
  }

  if (!client_) {
    return absl::InternalError("HTTP client not initialized");
  }

  // Note: This is a simplified implementation using HTTP POST
  // A real WebSocket would send frames over the persistent connection

  httplib::Headers headers = {
      {"Content-Type", "text/plain"},
      {"X-Session-Id", session_id_}
  };

  auto res = client_->Post("/send", headers, message, "text/plain");

  if (!res) {
    return absl::UnavailableError("Failed to send message");
  }

  if (res->status != 200) {
    return absl::InternalError("Server returned status " +
                              std::to_string(res->status));
  }

  return absl::OkStatus();
}

absl::Status HttpLibWebSocket::SendBinary(const uint8_t* data, size_t length) {
  if (state_ != WebSocketState::kConnected) {
    return absl::FailedPreconditionError("WebSocket not connected");
  }

  if (!client_) {
    return absl::InternalError("HTTP client not initialized");
  }

  // Convert binary data to string for HTTP transport
  std::string body(reinterpret_cast<const char*>(data), length);

  httplib::Headers headers = {
      {"Content-Type", "application/octet-stream"},
      {"X-Session-Id", session_id_}
  };

  auto res = client_->Post("/send-binary", headers, body,
                          "application/octet-stream");

  if (!res) {
    return absl::UnavailableError("Failed to send binary data");
  }

  if (res->status != 200) {
    return absl::InternalError("Server returned status " +
                              std::to_string(res->status));
  }

  return absl::OkStatus();
}

absl::Status HttpLibWebSocket::Close(int code, const std::string& reason) {
  if (state_ == WebSocketState::kDisconnected ||
      state_ == WebSocketState::kClosed) {
    return absl::OkStatus();
  }

  state_ = WebSocketState::kClosing;

  // Stop receive loop
  StopReceiveLoop();

  if (client_) {
    // Send close notification to server
    httplib::Headers headers = {
        {"X-Session-Id", session_id_},
        {"X-Close-Code", std::to_string(code)},
        {"X-Close-Reason", reason}
    };

    client_->Post("/close", headers, "", "text/plain");
    client_.reset();
  }

  state_ = WebSocketState::kClosed;

  // Call close callback if set
  if (close_callback_) {
    close_callback_(code, reason);
  }

  state_ = WebSocketState::kDisconnected;

  return absl::OkStatus();
}

void HttpLibWebSocket::ReceiveLoop() {
  while (!stop_receive_ && state_ == WebSocketState::kConnected) {
    if (!client_) {
      break;
    }

    // Long-polling: make a request that blocks until there's a message
    httplib::Headers headers = {
        {"X-Session-Id", session_id_}
    };

    auto res = client_->Get("/poll", headers);

    if (stop_receive_) {
      break;
    }

    if (!res) {
      // Connection error
      if (error_callback_) {
        error_callback_("Connection lost");
      }
      break;
    }

    if (res->status == 200 && !res->body.empty()) {
      // Received a message
      if (message_callback_) {
        message_callback_(res->body);
      }
    } else if (res->status == 204) {
      // No content - continue polling
      continue;
    } else if (res->status >= 400) {
      // Error from server
      if (error_callback_) {
        error_callback_("Server error: " + std::to_string(res->status));
      }
      break;
    }

    // Small delay to prevent tight loop
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void HttpLibWebSocket::StopReceiveLoop() {
  stop_receive_ = true;
  if (receive_thread_.joinable()) {
    receive_thread_.join();
  }
}

}  // namespace net
}  // namespace yaze