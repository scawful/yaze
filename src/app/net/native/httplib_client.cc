#include "app/net/native/httplib_client.h"

#include <regex>

#include "util/macro.h"  // For RETURN_IF_ERROR and ASSIGN_OR_RETURN

// Include httplib with appropriate settings
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include "httplib.h"

namespace yaze {
namespace net {

HttpLibClient::HttpLibClient() {
  // Constructor
}

HttpLibClient::~HttpLibClient() {
  // Cleanup cached clients
  client_cache_.clear();
}

absl::Status HttpLibClient::ParseUrl(const std::string& url,
                                     std::string& scheme,
                                     std::string& host,
                                     int& port,
                                     std::string& path) const {
  // Basic URL regex pattern
  std::regex url_regex(R"(^(https?):\/\/([^:\/\s]+)(?::(\d+))?(\/.*)?$)");
  std::smatch matches;

  if (!std::regex_match(url, matches, url_regex)) {
    return absl::InvalidArgumentError("Invalid URL format: " + url);
  }

  scheme = matches[1].str();
  host = matches[2].str();

  // Parse port or use defaults
  if (matches[3].matched) {
    port = std::stoi(matches[3].str());
  } else {
    port = (scheme == "https") ? 443 : 80;
  }

  // Parse path (default to "/" if empty)
  path = matches[4].matched ? matches[4].str() : "/";

  return absl::OkStatus();
}

absl::StatusOr<std::shared_ptr<httplib::Client>> HttpLibClient::GetOrCreateClient(
    const std::string& scheme,
    const std::string& host,
    int port) {

  // Create cache key
  std::string cache_key = scheme + "://" + host + ":" + std::to_string(port);

  // Check if client exists in cache
  auto it = client_cache_.find(cache_key);
  if (it != client_cache_.end() && it->second) {
    return it->second;
  }

  // Create new client
  std::shared_ptr<httplib::Client> client;

  if (scheme == "https") {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    client = std::make_shared<httplib::Client>(host, port);
    client->enable_server_certificate_verification(false); // For development
#else
    return absl::UnimplementedError(
        "HTTPS not supported: OpenSSL support not compiled in");
#endif
  } else if (scheme == "http") {
    client = std::make_shared<httplib::Client>(host, port);
  } else {
    return absl::InvalidArgumentError("Unsupported URL scheme: " + scheme);
  }

  if (!client) {
    return absl::InternalError("Failed to create HTTP client");
  }

  // Set timeout from base class
  client->set_connection_timeout(timeout_seconds_);
  client->set_read_timeout(timeout_seconds_);
  client->set_write_timeout(timeout_seconds_);

  // Cache the client
  client_cache_[cache_key] = client;

  return client;
}

Headers HttpLibClient::ConvertHeaders(const void* httplib_headers) const {
  Headers result;

  if (httplib_headers) {
    const auto& headers = *static_cast<const httplib::Headers*>(httplib_headers);
    for (const auto& header : headers) {
      result[header.first] = header.second;
    }
  }

  return result;
}

absl::StatusOr<HttpResponse> HttpLibClient::Get(
    const std::string& url,
    const Headers& headers) {

  std::string scheme, host, path;
  int port;

  RETURN_IF_ERROR(ParseUrl(url, scheme, host, port, path));

  auto client_or = GetOrCreateClient(scheme, host, port);
  ASSIGN_OR_RETURN(auto client, client_or);

  // Convert headers to httplib format
  httplib::Headers httplib_headers;
  for (const auto& [key, value] : headers) {
    httplib_headers.emplace(key, value);
  }

  // Perform GET request
  auto res = client->Get(path.c_str(), httplib_headers);

  if (!res) {
    return absl::UnavailableError("HTTP GET request failed: " + url);
  }

  HttpResponse response;
  response.status_code = res->status;
  response.body = res->body;
  response.headers = ConvertHeaders(&res->headers);

  return response;
}

absl::StatusOr<HttpResponse> HttpLibClient::Post(
    const std::string& url,
    const std::string& body,
    const Headers& headers) {

  std::string scheme, host, path;
  int port;

  RETURN_IF_ERROR(ParseUrl(url, scheme, host, port, path));

  auto client_or = GetOrCreateClient(scheme, host, port);
  ASSIGN_OR_RETURN(auto client, client_or);

  // Convert headers to httplib format
  httplib::Headers httplib_headers;
  for (const auto& [key, value] : headers) {
    httplib_headers.emplace(key, value);
  }

  // Set Content-Type if not provided
  if (httplib_headers.find("Content-Type") == httplib_headers.end()) {
    httplib_headers.emplace("Content-Type", "application/json");
  }

  // Perform POST request
  auto res = client->Post(path.c_str(), httplib_headers, body,
                         httplib_headers.find("Content-Type")->second);

  if (!res) {
    return absl::UnavailableError("HTTP POST request failed: " + url);
  }

  HttpResponse response;
  response.status_code = res->status;
  response.body = res->body;
  response.headers = ConvertHeaders(&res->headers);

  return response;
}

absl::StatusOr<HttpResponse> HttpLibClient::Put(
    const std::string& url,
    const std::string& body,
    const Headers& headers) {

  std::string scheme, host, path;
  int port;

  RETURN_IF_ERROR(ParseUrl(url, scheme, host, port, path));

  auto client_or = GetOrCreateClient(scheme, host, port);
  ASSIGN_OR_RETURN(auto client, client_or);

  // Convert headers to httplib format
  httplib::Headers httplib_headers;
  for (const auto& [key, value] : headers) {
    httplib_headers.emplace(key, value);
  }

  // Set Content-Type if not provided
  if (httplib_headers.find("Content-Type") == httplib_headers.end()) {
    httplib_headers.emplace("Content-Type", "application/json");
  }

  // Perform PUT request
  auto res = client->Put(path.c_str(), httplib_headers, body,
                        httplib_headers.find("Content-Type")->second);

  if (!res) {
    return absl::UnavailableError("HTTP PUT request failed: " + url);
  }

  HttpResponse response;
  response.status_code = res->status;
  response.body = res->body;
  response.headers = ConvertHeaders(&res->headers);

  return response;
}

absl::StatusOr<HttpResponse> HttpLibClient::Delete(
    const std::string& url,
    const Headers& headers) {

  std::string scheme, host, path;
  int port;

  RETURN_IF_ERROR(ParseUrl(url, scheme, host, port, path));

  auto client_or = GetOrCreateClient(scheme, host, port);
  ASSIGN_OR_RETURN(auto client, client_or);

  // Convert headers to httplib format
  httplib::Headers httplib_headers;
  for (const auto& [key, value] : headers) {
    httplib_headers.emplace(key, value);
  }

  // Perform DELETE request
  auto res = client->Delete(path.c_str(), httplib_headers);

  if (!res) {
    return absl::UnavailableError("HTTP DELETE request failed: " + url);
  }

  HttpResponse response;
  response.status_code = res->status;
  response.body = res->body;
  response.headers = ConvertHeaders(&res->headers);

  return response;
}

void HttpLibClient::SetTimeout(int timeout_seconds) {
  IHttpClient::SetTimeout(timeout_seconds);

  // Clear client cache to force recreation with new timeout
  client_cache_.clear();
}

}  // namespace net
}  // namespace yaze