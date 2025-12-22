#include "app/net/network_factory.h"

#ifdef __EMSCRIPTEN__
#include "app/net/wasm/emscripten_http_client.h"
#include "app/net/wasm/emscripten_websocket.h"
#else
#include "app/net/native/httplib_client.h"
#include "app/net/native/httplib_websocket.h"
#endif

namespace yaze {
namespace net {

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

bool IsSSLSupported() {
#ifdef __EMSCRIPTEN__
  // WASM in browser always supports SSL/TLS through browser APIs
  return true;
#else
  // Native builds depend on OpenSSL availability
  #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  return true;
  #else
  return false;
  #endif
#endif
}

std::string GetNetworkPlatform() {
#ifdef __EMSCRIPTEN__
  return "wasm";
#else
  return "native";
#endif
}

}  // namespace net
}  // namespace yaze