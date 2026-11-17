#include "cli/service/ai/ai_http_client.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
#define YAZE_AI_IOS_URLSESSION 1
#endif
#endif

#if defined(YAZE_AI_IOS_URLSESSION)
#include "cli/service/ai/ios_urlsession_http_client.h"
#else
#include "httplib.h"
#endif

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "cli/service/ai/subprocess_runner.h"

namespace yaze::cli {

namespace {

std::string NormalizeUrlJoin(absl::string_view base_url,
                             absl::string_view path_and_query) {
  std::string base(base_url);
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  if (path_and_query.empty()) {
    return base;
  }
  if (path_and_query.front() == '/') {
    return absl::StrCat(base, path_and_query);
  }
  return absl::StrCat(base, "/", path_and_query);
}

#if !defined(YAZE_AI_IOS_URLSESSION) && !defined(CPPHTTPLIB_OPENSSL_SUPPORT)
std::string EscapeCurlConfigString(absl::string_view value) {
  std::string out = "\"";
  for (char c : value) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
      out.push_back(c);
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else {
      out.push_back(c);
    }
  }
  out.push_back('"');
  return out;
}
#endif

}  // namespace

absl::StatusOr<AiHttpResponse> PerformAiHttpRequest(
    absl::string_view method, absl::string_view base_url,
    absl::string_view path_and_query,
    const std::map<std::string, std::string>& headers, absl::string_view body,
    int timeout_ms) {
  const std::string url = NormalizeUrlJoin(base_url, path_and_query);
#if defined(YAZE_AI_IOS_URLSESSION)
  auto resp_or = ios::UrlSessionHttpRequest(std::string(method), url, headers,
                                            std::string(body), timeout_ms);
  if (!resp_or.ok()) {
    return resp_or.status();
  }
  AiHttpResponse out;
  out.status_code = resp_or->status_code;
  out.body = std::move(resp_or->body);
  return out;
#else
  const bool is_plain_http =
      absl::StartsWith(absl::AsciiStrToLower(base_url), "http://");
#if defined(CPPHTTPLIB_OPENSSL_SUPPORT)
  constexpr bool kCanUseInProcessHttps = true;
#else
  constexpr bool kCanUseInProcessHttps = false;
#endif

  if (kCanUseInProcessHttps || is_plain_http) {
    try {
      httplib::Client cli{std::string(base_url)};
      const int conn_sec = std::max(1, std::min(10, timeout_ms / 1000));
      const int io_sec = std::max(1, (timeout_ms + 999) / 1000);
      cli.set_connection_timeout(conn_sec, 0);
      cli.set_read_timeout(io_sec, 0);
      cli.set_write_timeout(io_sec, 0);
      cli.set_follow_location(true);

      httplib::Headers req_headers;
      std::string content_type = "application/json";
      for (const auto& [k, v] : headers) {
        if (absl::EqualsIgnoreCase(k, "content-type")) {
          content_type = v;
        }
        req_headers.emplace(k, v);
      }

      const std::string path = path_and_query.empty()
                                   ? "/"
                                   : (path_and_query.front() == '/'
                                          ? std::string(path_and_query)
                                          : absl::StrCat("/", path_and_query));

      httplib::Result res;
      if (absl::EqualsIgnoreCase(method, "GET")) {
        res = cli.Get(path, req_headers);
      } else if (absl::EqualsIgnoreCase(method, "POST")) {
        res = cli.Post(path, req_headers, std::string(body), content_type);
      } else {
        return absl::InvalidArgumentError(
            absl::StrCat("Unsupported HTTP method: ", method));
      }

      if (!res) {
        return absl::UnavailableError(
            absl::StrCat("HTTP request failed for ", base_url, ": ",
                         httplib::to_string(res.error())));
      }
      AiHttpResponse out;
      out.status_code = res->status;
      out.body = std::move(res->body);
      return out;
    } catch (const std::exception& e) {
      return absl::InternalError(
          absl::StrCat("HTTP client exception: ", e.what()));
    }
  }

#if !defined(CPPHTTPLIB_OPENSSL_SUPPORT)
  // Fallback for builds without OpenSSL HTTPS (e.g. Windows + BoringSSL gRPC):
  // run curl without a shell and pass sensitive headers over stdin (-K -) so
  // API keys never appear on argv or in the filesystem.
  auto curl_or = ResolveCliBinary("curl");
  if (!curl_or.ok()) {
    return absl::UnavailableError(
        "HTTPS support disabled (no OpenSSL and curl not found)");
  }

  std::filesystem::path body_temp;
  struct TempCleaner {
    std::filesystem::path path;
    ~TempCleaner() {
      if (!path.empty()) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
      }
    }
  } cleaner;

  std::string curl_config;
  absl::StrAppend(&curl_config, "url = ", EscapeCurlConfigString(url), "\n");
  absl::StrAppend(&curl_config, "request = ",
                  EscapeCurlConfigString(absl::AsciiStrToUpper(method)), "\n");
  for (const auto& [k, v] : headers) {
    absl::StrAppend(&curl_config, "header = ",
                    EscapeCurlConfigString(absl::StrCat(k, ": ", v)), "\n");
  }

  SubprocessOptions opts;
  opts.executable = *curl_or;
  opts.args = {"-sS",
               "--max-time",
               absl::StrCat(std::max(1, (timeout_ms + 999) / 1000)),
               "-w",
               "\n%{http_code}",
               "-K",
               "-"};
  if (!body.empty()) {
    body_temp = std::filesystem::temp_directory_path() /
                absl::StrCat(
                    "yaze_http_body_",
                    std::chrono::steady_clock::now().time_since_epoch().count(),
                    ".json");
    cleaner.path = body_temp;
    std::ofstream out(body_temp, std::ios::binary);
    out.write(body.data(), static_cast<std::streamsize>(body.size()));
    out.close();
    opts.args.push_back("--data-binary");
    opts.args.push_back(absl::StrCat("@", body_temp.string()));
  }
  opts.stdin_data = std::move(curl_config);
  opts.timeout = std::chrono::milliseconds(std::max(timeout_ms + 5000, 10000));

  auto sub_or = RunSubprocess(opts);
  if (!sub_or.ok()) {
    return sub_or.status();
  }
  if (sub_or->exit_code != 0) {
    return absl::UnavailableError(absl::StrCat(
        "HTTPS request via curl failed (exit ", sub_or->exit_code, ")"));
  }
  const size_t nl = sub_or->stdout_output.rfind('\n');
  if (nl == std::string::npos) {
    return absl::InternalError("Malformed HTTP response from curl");
  }
  int status_code = 0;
  if (!absl::SimpleAtoi(
          absl::StripAsciiWhitespace(
              absl::string_view(sub_or->stdout_output).substr(nl + 1)),
          &status_code)) {
    return absl::InternalError("Failed to parse HTTP status code from curl");
  }
  AiHttpResponse out;
  out.status_code = status_code;
  out.body = sub_or->stdout_output.substr(0, nl);
  return out;
#else
  return absl::UnavailableError("Unreachable HTTP path");
#endif
#endif  // YAZE_AI_IOS_URLSESSION
}

}  // namespace yaze::cli
