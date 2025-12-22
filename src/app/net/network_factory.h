#ifndef YAZE_APP_NET_NETWORK_FACTORY_H_
#define YAZE_APP_NET_NETWORK_FACTORY_H_

#include <memory>
#include <string>

#include "app/net/http_client.h"
#include "app/net/websocket_interface.h"

namespace yaze {
namespace net {

/**
 * @brief Factory functions for creating network clients
 *
 * These functions return the appropriate implementation based on the
 * build platform (native or WASM). This allows the rest of the codebase
 * to use networking features without worrying about platform differences.
 */

/**
 * @brief Create an HTTP client for the current platform
 * @return Unique pointer to IHttpClient implementation
 *
 * Returns:
 * - HttpLibClient for native builds
 * - EmscriptenHttpClient for WASM builds
 */
std::unique_ptr<IHttpClient> CreateHttpClient();

/**
 * @brief Create a WebSocket client for the current platform
 * @return Unique pointer to IWebSocket implementation
 *
 * Returns:
 * - HttpLibWebSocket (or native WebSocket) for native builds
 * - EmscriptenWebSocket for WASM builds
 */
std::unique_ptr<IWebSocket> CreateWebSocket();

/**
 * @brief Check if the current platform supports SSL/TLS
 * @return true if SSL/TLS is available, false otherwise
 */
bool IsSSLSupported();

/**
 * @brief Get the platform name for debugging
 * @return Platform string ("native", "wasm", etc.)
 */
std::string GetNetworkPlatform();

}  // namespace net
}  // namespace yaze

#endif  // YAZE_APP_NET_NETWORK_FACTORY_H_