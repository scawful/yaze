#include "app/net/native/httplib_client.h"

#include <regex>

#include "util/macro.h"

#include "httplib.h"

namespace yaze {
namespace net {

namespace {

httplib::Headers ToHttplibHeaders(const Headers& headers) {
  httplib::Headers result;
  for (const auto& [key, value] : headers) {
    result.emplace(key, value);
  }
  return result;
}

// Returns the request's Content-Type, adding application/json when absent.
std::string EnsureContentType(httplib::Headers& headers) {
  auto it = headers.find("Content-Type");
  if (it == headers.end()) {
    it = headers.emplace("Content-Type", "application/json");
  }
  return it->second;
}

HttpResponse ToHttpResponse(const httplib::Response& res) {
  HttpResponse response;
  response.status_code = res.status;
  response.body = res.body;
  for (const auto& header : res.headers) {
    response.headers[header.first] = header.second;
  }
  return response;
}

}  // namespace

HttpLibClient::HttpLibClient() = default;

HttpLibClient::~HttpLibClient() = default;

absl::Status HttpLibClient::ParseUrl(const std::string& url,
                                     std::string& scheme,
                                     std::string& host,
                                     int& port,
                                     std::string& path) const {
  std::regex url_regex(R"(^(https?):\/\/([^:\/\s]+)(?::(\d+))?(\/.*)?$)");
  std::smatch matches;

  if (!std::regex_match(url, matches, url_regex)) {
    return absl::InvalidArgumentError("Invalid URL format: " + url);
  }

  scheme = matches[1].str();
  host = matches[2].str();

  if (matches[3].matched) {
    port = std::stoi(matches[3].str());
  } else {
    port = (scheme == "https") ? 443 : 80;
  }

  path = matches[4].matched ? matches[4].str() : "/";

  return absl::OkStatus();
}

absl::StatusOr<std::shared_ptr<httplib::Client>> HttpLibClient::GetOrCreateClient(
    const std::string& scheme,
    const std::string& host,
    int port) {
  std::string cache_key = scheme + "://" + host + ":" + std::to_string(port);

  auto it = client_cache_.find(cache_key);
  if (it != client_cache_.end() && it->second) {
    return it->second;
  }

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

  client->set_connection_timeout(timeout_seconds_);
  client->set_read_timeout(timeout_seconds_);
  client->set_write_timeout(timeout_seconds_);

  client_cache_[cache_key] = client;

  return client;
}

absl::StatusOr<HttpResponse> HttpLibClient::Get(
    const std::string& url,
    const Headers& headers) {
  std::string scheme, host, path;
  int port;
  RETURN_IF_ERROR(ParseUrl(url, scheme, host, port, path));
  ASSIGN_OR_RETURN(auto client, GetOrCreateClient(scheme, host, port));

  auto res = client->Get(path.c_str(), ToHttplibHeaders(headers));
  if (!res) {
    return absl::UnavailableError("HTTP GET request failed: " + url);
  }
  return ToHttpResponse(*res);
}

absl::StatusOr<HttpResponse> HttpLibClient::Post(
    const std::string& url,
    const std::string& body,
    const Headers& headers) {
  std::string scheme, host, path;
  int port;
  RETURN_IF_ERROR(ParseUrl(url, scheme, host, port, path));
  ASSIGN_OR_RETURN(auto client, GetOrCreateClient(scheme, host, port));

  httplib::Headers httplib_headers = ToHttplibHeaders(headers);
  std::string content_type = EnsureContentType(httplib_headers);
  auto res = client->Post(path.c_str(), httplib_headers, body, content_type);
  if (!res) {
    return absl::UnavailableError("HTTP POST request failed: " + url);
  }
  return ToHttpResponse(*res);
}

absl::StatusOr<HttpResponse> HttpLibClient::Put(
    const std::string& url,
    const std::string& body,
    const Headers& headers) {
  std::string scheme, host, path;
  int port;
  RETURN_IF_ERROR(ParseUrl(url, scheme, host, port, path));
  ASSIGN_OR_RETURN(auto client, GetOrCreateClient(scheme, host, port));

  httplib::Headers httplib_headers = ToHttplibHeaders(headers);
  std::string content_type = EnsureContentType(httplib_headers);
  auto res = client->Put(path.c_str(), httplib_headers, body, content_type);
  if (!res) {
    return absl::UnavailableError("HTTP PUT request failed: " + url);
  }
  return ToHttpResponse(*res);
}

absl::StatusOr<HttpResponse> HttpLibClient::Delete(
    const std::string& url,
    const Headers& headers) {
  std::string scheme, host, path;
  int port;
  RETURN_IF_ERROR(ParseUrl(url, scheme, host, port, path));
  ASSIGN_OR_RETURN(auto client, GetOrCreateClient(scheme, host, port));

  auto res = client->Delete(path.c_str(), ToHttplibHeaders(headers));
  if (!res) {
    return absl::UnavailableError("HTTP DELETE request failed: " + url);
  }
  return ToHttpResponse(*res);
}

void HttpLibClient::SetTimeout(int timeout_seconds) {
  IHttpClient::SetTimeout(timeout_seconds);

  // Cached clients keep the timeout they were created with.
  client_cache_.clear();
}

}  // namespace net
}  // namespace yaze
