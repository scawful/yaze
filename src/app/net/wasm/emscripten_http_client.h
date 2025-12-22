#ifndef YAZE_APP_NET_WASM_EMSCRIPTEN_HTTP_CLIENT_H_
#define YAZE_APP_NET_WASM_EMSCRIPTEN_HTTP_CLIENT_H_

#ifdef __EMSCRIPTEN__

#include <mutex>
#include <condition_variable>

#include <emscripten/fetch.h>

#include "app/net/http_client.h"

namespace yaze {
namespace net {

/**
 * @class EmscriptenHttpClient
 * @brief WASM HTTP client implementation using Emscripten Fetch API
 *
 * This implementation wraps the Emscripten fetch API for browser-based
 * HTTP/HTTPS requests. All requests are subject to browser CORS policies.
 */
class EmscriptenHttpClient : public IHttpClient {
 public:
  EmscriptenHttpClient();
  ~EmscriptenHttpClient() override;

  /**
   * @brief Perform an HTTP GET request
   * @param url The URL to request
   * @param headers Optional HTTP headers
   * @return HttpResponse or error status
   */
  absl::StatusOr<HttpResponse> Get(
      const std::string& url,
      const Headers& headers = {}) override;

  /**
   * @brief Perform an HTTP POST request
   * @param url The URL to post to
   * @param body The request body
   * @param headers Optional HTTP headers
   * @return HttpResponse or error status
   */
  absl::StatusOr<HttpResponse> Post(
      const std::string& url,
      const std::string& body,
      const Headers& headers = {}) override;

  /**
   * @brief Perform an HTTP PUT request
   * @param url The URL to put to
   * @param body The request body
   * @param headers Optional HTTP headers
   * @return HttpResponse or error status
   */
  absl::StatusOr<HttpResponse> Put(
      const std::string& url,
      const std::string& body,
      const Headers& headers = {}) override;

  /**
   * @brief Perform an HTTP DELETE request
   * @param url The URL to delete
   * @param headers Optional HTTP headers
   * @return HttpResponse or error status
   */
  absl::StatusOr<HttpResponse> Delete(
      const std::string& url,
      const Headers& headers = {}) override;

 private:
  /**
   * @brief Structure to hold fetch result data
   */
  struct FetchResult {
    bool completed = false;
    bool success = false;
    int status_code = 0;
    std::string body;
    Headers headers;
    std::string error_message;
    std::mutex mutex;
    std::condition_variable cv;
  };

  /**
   * @brief Perform a fetch request
   * @param method HTTP method
   * @param url Request URL
   * @param body Request body (for POST/PUT)
   * @param headers Request headers
   * @return HttpResponse or error status
   */
  absl::StatusOr<HttpResponse> PerformFetch(
      const std::string& method,
      const std::string& url,
      const std::string& body = "",
      const Headers& headers = {});

  /**
   * @brief Success callback for fetch operations
   */
  static void OnFetchSuccess(emscripten_fetch_t* fetch);

  /**
   * @brief Error callback for fetch operations
   */
  static void OnFetchError(emscripten_fetch_t* fetch);

  /**
   * @brief Progress callback for fetch operations
   */
  static void OnFetchProgress(emscripten_fetch_t* fetch);
};

}  // namespace net
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_NET_WASM_EMSCRIPTEN_HTTP_CLIENT_H_