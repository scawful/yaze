#ifndef YAZE_APP_NET_NATIVE_HTTPLIB_CLIENT_H_
#define YAZE_APP_NET_NATIVE_HTTPLIB_CLIENT_H_

#include <memory>
#include <string>

#include "app/net/http_client.h"

// Forward declaration to avoid including httplib.h in header
namespace httplib {
class Client;
}

namespace yaze {
namespace net {

/**
 * @class HttpLibClient
 * @brief Native HTTP client implementation using cpp-httplib
 *
 * This implementation wraps the cpp-httplib library for native builds,
 * providing HTTP/HTTPS support with optional SSL/TLS via OpenSSL.
 */
class HttpLibClient : public IHttpClient {
 public:
  HttpLibClient();
  ~HttpLibClient() override;

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

  /**
   * @brief Set a timeout for HTTP requests
   * @param timeout_seconds Timeout in seconds
   */
  void SetTimeout(int timeout_seconds) override;

 private:
  /**
   * @brief Parse URL into components
   * @param url The URL to parse
   * @param scheme Output: URL scheme (http/https)
   * @param host Output: Host name
   * @param port Output: Port number
   * @param path Output: Path component
   * @return Status indicating success or failure
   */
  absl::Status ParseUrl(const std::string& url,
                       std::string& scheme,
                       std::string& host,
                       int& port,
                       std::string& path) const;

  /**
   * @brief Create or get cached httplib client for a host
   * @param scheme URL scheme (http/https)
   * @param host Host name
   * @param port Port number
   * @return httplib::Client pointer or error
   */
  absl::StatusOr<std::shared_ptr<httplib::Client>> GetOrCreateClient(
      const std::string& scheme,
      const std::string& host,
      int port);

  /**
   * @brief Convert httplib headers to our Headers type
   * @param httplib_headers httplib header structure
   * @return Headers map
   */
  Headers ConvertHeaders(const void* httplib_headers) const;

  // Cache clients per host to avoid reconnection overhead
  std::map<std::string, std::shared_ptr<httplib::Client>> client_cache_;
};

}  // namespace net
}  // namespace yaze

#endif  // YAZE_APP_NET_NATIVE_HTTPLIB_CLIENT_H_