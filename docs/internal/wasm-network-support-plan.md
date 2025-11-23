# WASM Network Support Plan for yaze

## Executive Summary

This document outlines the architectural changes required to enable AI services (Gemini/Ollama) and WebSocket collaboration features in the browser-based WASM build of yaze. The main challenge is replacing native networking libraries (cpp-httplib, OpenSSL, curl) with browser-compatible APIs provided by Emscripten.

## Current Architecture Analysis

### 1. Gemini AI Service (`src/cli/service/ai/gemini_ai_service.cc`)

**Current Implementation:**
- Uses `httplib` (cpp-httplib) for HTTPS requests with OpenSSL support
- Falls back to `curl` command via `popen()` for API calls
- Depends on OpenSSL for SSL/TLS encryption
- Uses base64 encoding for image uploads

**Key Dependencies:**
- `httplib.h` - C++ HTTP library
- OpenSSL - SSL/TLS support
- `popen()`/`pclose()` - Process execution for curl
- File system access for temporary JSON files

### 2. Ollama AI Service (`src/cli/service/ai/ollama_ai_service.cc`)

**Current Implementation:**
- Uses `httplib::Client` for HTTP requests to local Ollama server
- Communicates over HTTP (not HTTPS) on localhost:11434
- JSON parsing with nlohmann/json

**Key Dependencies:**
- `httplib.h` - C++ HTTP library
- No SSL/TLS requirement (local HTTP only)

### 3. WebSocket Client (`src/app/net/websocket_client.cc`)

**Current Implementation:**
- Uses `httplib::Client` as a placeholder (not true WebSocket)
- Currently implements HTTP POST fallback instead of WebSocket
- Conditional OpenSSL support for secure connections
- Thread-based receive loop

**Key Dependencies:**
- `httplib.h` - C++ HTTP library
- OpenSSL (optional) - For WSS support
- `std::thread` - For receive loop
- Platform-specific socket libraries (ws2_32 on Windows)

### 4. HTTP Server (`src/cli/service/api/http_server.cc`)

**Current Implementation:**
- Uses `httplib::Server` for REST API endpoints
- Runs in separate thread
- Provides health check and model listing endpoints

**Key Dependencies:**
- `httplib.h` - C++ HTTP server
- `std::thread` - For server thread

## Emscripten Capabilities

### Available APIs:

1. **Fetch API** (`emscripten_fetch()`)
   - Asynchronous HTTP/HTTPS requests
   - Supports GET, POST, PUT, DELETE
   - CORS-aware
   - Can handle binary data and streams

2. **WebSocket API**
   - Native browser WebSocket support
   - Accessible via Emscripten's WebSocket wrapper
   - Full duplex communication
   - Binary and text message support

3. **Web Workers** (via pthread emulation)
   - Background processing
   - Shared memory support with SharedArrayBuffer
   - Can handle async operations

## Required Changes

### Phase 1: Abstract Network Layer

Create platform-agnostic network interfaces:

```cpp
// src/app/net/http_client.h
class IHttpClient {
public:
  virtual ~IHttpClient() = default;
  virtual absl::StatusOr<HttpResponse> Get(const std::string& url,
                                           const Headers& headers) = 0;
  virtual absl::StatusOr<HttpResponse> Post(const std::string& url,
                                            const std::string& body,
                                            const Headers& headers) = 0;
};

// src/app/net/websocket.h
class IWebSocket {
public:
  virtual ~IWebSocket() = default;
  virtual absl::Status Connect(const std::string& url) = 0;
  virtual absl::Status Send(const std::string& message) = 0;
  virtual void OnMessage(std::function<void(const std::string&)> callback) = 0;
};
```

### Phase 2: Native Implementation

Keep existing implementations for native builds:

```cpp
// src/app/net/native/httplib_client.cc
class HttpLibClient : public IHttpClient {
  // Current httplib implementation
};

// src/app/net/native/httplib_websocket.cc
class HttpLibWebSocket : public IWebSocket {
  // Current httplib-based implementation
};
```

### Phase 3: Emscripten Implementation

Create browser-compatible implementations:

```cpp
// src/app/net/wasm/emscripten_http_client.cc
#ifdef __EMSCRIPTEN__
class EmscriptenHttpClient : public IHttpClient {
  absl::StatusOr<HttpResponse> Get(const std::string& url,
                                   const Headers& headers) override {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    // Set headers
    std::vector<const char*> header_strings;
    for (const auto& [key, value] : headers) {
      header_strings.push_back(key.c_str());
      header_strings.push_back(value.c_str());
    }
    header_strings.push_back(nullptr);
    attr.requestHeaders = header_strings.data();

    // Synchronous fetch (blocks until complete)
    emscripten_fetch_t* fetch = emscripten_fetch(&attr, url.c_str());

    HttpResponse response;
    response.status = fetch->status;
    response.body = std::string(fetch->data, fetch->numBytes);

    emscripten_fetch_close(fetch);
    return response;
  }

  absl::StatusOr<HttpResponse> Post(const std::string& url,
                                    const std::string& body,
                                    const Headers& headers) override {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.requestData = body.c_str();
    attr.requestDataSize = body.length();

    // Similar implementation as Get()
    // ...
  }
};
#endif

// src/app/net/wasm/emscripten_websocket.cc
#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>

class EmscriptenWebSocket : public IWebSocket {
  EMSCRIPTEN_WEBSOCKET_T socket_;

  absl::Status Connect(const std::string& url) override {
    EmscriptenWebSocketCreateAttributes attrs = {
      url.c_str(),
      nullptr,  // protocols
      EM_TRUE   // createOnMainThread
    };

    socket_ = emscripten_websocket_new(&attrs);
    if (socket_ <= 0) {
      return absl::InternalError("Failed to create WebSocket");
    }

    // Set callbacks
    emscripten_websocket_set_onopen_callback(socket_, this, OnOpenCallback);
    emscripten_websocket_set_onmessage_callback(socket_, this, OnMessageCallback);
    emscripten_websocket_set_onerror_callback(socket_, this, OnErrorCallback);

    return absl::OkStatus();
  }

  absl::Status Send(const std::string& message) override {
    EMSCRIPTEN_RESULT result = emscripten_websocket_send_text(
        socket_, message.c_str(), message.length());
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
      return absl::InternalError("Failed to send WebSocket message");
    }
    return absl::OkStatus();
  }
};
#endif
```

### Phase 4: Factory Pattern

Create factories to instantiate the correct implementation:

```cpp
// src/app/net/network_factory.cc
std::unique_ptr<IHttpClient> CreateHttpClient() {
#ifdef __EMSCRIPTEN__
  return std::make_unique<EmscriptenHttpClient>();
#else
  return std::make_unique<HttpLibClient>();
#endif
}

std::unique_ptr<IWebSocket> CreateWebSocket() {
#ifdef __EMSCRIPTEN__
  return std::make_unique<EmscriptenWebSocket>();
#else
  return std::make_unique<HttpLibWebSocket>();
#endif
}
```

### Phase 5: Service Modifications

Update AI services to use the abstraction:

```cpp
// src/cli/service/ai/gemini_ai_service.cc
class GeminiAIService {
  std::unique_ptr<IHttpClient> http_client_;

  GeminiAIService(const GeminiConfig& config)
    : config_(config),
      http_client_(CreateHttpClient()) {
    // Initialize
  }

  absl::Status CheckAvailability() {
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/"
                     + config_.model;
    Headers headers = {{"x-goog-api-key", config_.api_key}};

    auto response_or = http_client_->Get(url, headers);
    if (!response_or.ok()) {
      return response_or.status();
    }

    auto& response = response_or.value();
    if (response.status != 200) {
      return absl::UnavailableError("API not available");
    }

    return absl::OkStatus();
  }
};
```

## CMake Configuration

Update CMake to handle WASM builds:

```cmake
# src/app/net/net_library.cmake
if(EMSCRIPTEN)
  set(YAZE_NET_SRC
    app/net/wasm/emscripten_http_client.cc
    app/net/wasm/emscripten_websocket.cc
    app/net/network_factory.cc
  )

  # Add Emscripten fetch and WebSocket flags
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s FETCH=1")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s WEBSOCKET=1")
else()
  set(YAZE_NET_SRC
    app/net/native/httplib_client.cc
    app/net/native/httplib_websocket.cc
    app/net/network_factory.cc
  )

  # Link native dependencies
  target_link_libraries(yaze_net PUBLIC httplib OpenSSL::SSL)
endif()
```

## CORS Considerations

For browser builds, the following CORS requirements apply:

1. **Gemini API**: Google's API servers must include appropriate CORS headers
2. **Ollama**: Local Ollama server needs `--cors` flag or proxy setup
3. **WebSocket Server**: Must handle WebSocket upgrade correctly

### Proxy Server Option

For services without CORS support, implement a proxy:

```javascript
// proxy-server.js (Node.js)
const express = require('express');
const { createProxyMiddleware } = require('http-proxy-middleware');

const app = express();

// Proxy for Ollama
app.use('/ollama', createProxyMiddleware({
  target: 'http://localhost:11434',
  changeOrigin: true,
  pathRewrite: { '^/ollama': '' }
}));

// Proxy for Gemini
app.use('/gemini', createProxyMiddleware({
  target: 'https://generativelanguage.googleapis.com',
  changeOrigin: true,
  pathRewrite: { '^/gemini': '' }
}));

app.listen(3000);
```

## Testing Strategy

1. **Unit Tests**: Mock network interfaces for both native and WASM
2. **Integration Tests**: Test actual API calls in native builds
3. **Browser Tests**: Manual testing of WASM build in browser
4. **E2E Tests**: Selenium/Playwright for automated browser testing

## Implementation Timeline

### Week 1: Foundation
- Create abstract interfaces (IHttpClient, IWebSocket)
- Implement factory pattern
- Update CMake for conditional compilation

### Week 2: Native Refactoring
- Refactor existing code to use interfaces
- Create native implementations
- Ensure no regression in current functionality

### Week 3: WASM Implementation
- Implement EmscriptenHttpClient
- Implement EmscriptenWebSocket
- Test basic functionality

### Week 4: Service Integration
- Update GeminiAIService
- Update OllamaAIService
- Update WebSocketClient

### Week 5: Testing & Refinement
- Comprehensive testing
- CORS handling
- Performance optimization

## Risk Mitigation

### Risk 1: CORS Blocking
**Mitigation**: Implement proxy server as fallback, document CORS requirements

### Risk 2: API Key Security
**Mitigation**:
- Never embed API keys in WASM binary
- Require user to input API key via UI
- Store in browser's localStorage with encryption

### Risk 3: Performance Issues
**Mitigation**:
- Use Web Workers for background processing
- Implement request caching
- Add loading indicators for long operations

### Risk 4: Browser Compatibility
**Mitigation**:
- Test on Chrome, Firefox, Safari, Edge
- Use feature detection
- Provide fallbacks for unsupported features

## Security Considerations

1. **API Keys**: Must be user-provided, never hardcoded
2. **HTTPS Only**: All API calls must use HTTPS in production
3. **Input Validation**: Sanitize all user inputs before API calls
4. **Rate Limiting**: Implement client-side rate limiting
5. **Content Security Policy**: Configure CSP headers properly

## Conclusion

The transition to WASM-compatible networking is achievable through careful abstraction and platform-specific implementations. The key is maintaining a clean separation between platform-agnostic interfaces and platform-specific implementations. This approach allows the codebase to support both native and browser environments without sacrificing functionality or performance.

The proposed architecture provides:
- Clean abstraction layers
- Minimal changes to existing service code
- Easy testing and mocking
- Future extensibility for other platforms

Next steps:
1. Review and approve this plan
2. Create feature branch for implementation
3. Begin with abstract interface definitions
4. Implement incrementally with continuous testing