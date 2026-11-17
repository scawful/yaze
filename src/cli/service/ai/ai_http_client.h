#ifndef YAZE_SRC_CLI_SERVICE_AI_AI_HTTP_CLIENT_H_
#define YAZE_SRC_CLI_SERVICE_AI_AI_HTTP_CLIENT_H_

#include <map>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace yaze::cli {

struct AiHttpResponse {
  int status_code = 0;
  std::string body;
};

// Performs an HTTP/HTTPS GET or POST without spawning a shell and without
// passing API keys or headers on process argv.
absl::StatusOr<AiHttpResponse> PerformAiHttpRequest(
    absl::string_view method, absl::string_view base_url,
    absl::string_view path_and_query,
    const std::map<std::string, std::string>& headers, absl::string_view body,
    int timeout_ms);

}  // namespace yaze::cli

#endif  // YAZE_SRC_CLI_SERVICE_AI_AI_HTTP_CLIENT_H_
