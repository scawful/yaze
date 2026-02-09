#pragma once

#include <map>
#include <string>

#include "absl/status/statusor.h"

namespace yaze::cli::ios {

struct UrlSessionHttpResponse {
  int status_code = 0;
  std::string body;
  std::map<std::string, std::string> headers;
};

// iOS-only: Perform an HTTP request using NSURLSession.
// Intended for AI provider calls to avoid shelling out to curl and to get TLS
// support without OpenSSL.
absl::StatusOr<UrlSessionHttpResponse> UrlSessionHttpRequest(
    const std::string& method, const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body, int timeout_ms);

}  // namespace yaze::cli::ios

