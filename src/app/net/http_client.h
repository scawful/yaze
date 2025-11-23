#ifndef YAZE_APP_NET_HTTP_CLIENT_H_
#define YAZE_APP_NET_HTTP_CLIENT_H_

#include <map>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace net {

/**
 * @brief HTTP headers type definition
 */
using Headers = std::map<std::string, std::string>;

/**
 * @struct HttpResponse
 * @brief HTTP response structure containing status, body, and headers
 */
struct HttpResponse {
  int status_code = 0;
  std::string body;
  Headers headers;

  bool IsSuccess() const {
    return status_code >= 200 && status_code < 300;
  }

  bool IsClientError() const {
    return status_code >= 400 && status_code < 500;
  }

  bool IsServerError() const {
    return status_code >= 500 && status_code < 600;
  }
};

/**
 * @class IHttpClient
 * @brief Abstract interface for HTTP client implementations
 *
 * This interface abstracts HTTP operations to support both native
 * (using cpp-httplib) and WASM (using emscripten fetch) implementations.
 * All methods return absl::Status or absl::StatusOr for consistent error handling.
 */
class IHttpClient {
 public:
  virtual ~IHttpClient() = default;

  /**
   * @brief Perform an HTTP GET request
   * @param url The URL to request
   * @param headers Optional HTTP headers
   * @return HttpResponse or error status
   */
  virtual absl::StatusOr<HttpResponse> Get(
      const std::string& url,
      const Headers& headers = {}) = 0;

  /**
   * @brief Perform an HTTP POST request
   * @param url The URL to post to
   * @param body The request body
   * @param headers Optional HTTP headers
   * @return HttpResponse or error status
   */
  virtual absl::StatusOr<HttpResponse> Post(
      const std::string& url,
      const std::string& body,
      const Headers& headers = {}) = 0;

  /**
   * @brief Perform an HTTP PUT request
   * @param url The URL to put to
   * @param body The request body
   * @param headers Optional HTTP headers
   * @return HttpResponse or error status
   */
  virtual absl::StatusOr<HttpResponse> Put(
      const std::string& url,
      const std::string& body,
      const Headers& headers = {}) {
    // Default implementation returns not implemented
    return absl::UnimplementedError("PUT method not implemented");
  }

  /**
   * @brief Perform an HTTP DELETE request
   * @param url The URL to delete
   * @param headers Optional HTTP headers
   * @return HttpResponse or error status
   */
  virtual absl::StatusOr<HttpResponse> Delete(
      const std::string& url,
      const Headers& headers = {}) {
    // Default implementation returns not implemented
    return absl::UnimplementedError("DELETE method not implemented");
  }

  /**
   * @brief Set a timeout for HTTP requests
   * @param timeout_seconds Timeout in seconds
   */
  virtual void SetTimeout(int timeout_seconds) {
    timeout_seconds_ = timeout_seconds;
  }

  /**
   * @brief Get the current timeout setting
   * @return Timeout in seconds
   */
  int GetTimeout() const { return timeout_seconds_; }

 protected:
  int timeout_seconds_ = 30;  // Default 30 second timeout
};

}  // namespace net
}  // namespace yaze

#endif  // YAZE_APP_NET_HTTP_CLIENT_H_