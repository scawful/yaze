#ifdef __EMSCRIPTEN__

#include "app/net/wasm/emscripten_http_client.h"

#include <emscripten/fetch.h>
#include <cstring>
#include <vector>

namespace yaze {
namespace net {

EmscriptenHttpClient::EmscriptenHttpClient() {
  // Constructor
}

EmscriptenHttpClient::~EmscriptenHttpClient() {
  // Destructor
}

void EmscriptenHttpClient::OnFetchSuccess(emscripten_fetch_t* fetch) {
  FetchResult* result = static_cast<FetchResult*>(fetch->userData);

  {
    std::lock_guard<std::mutex> lock(result->mutex);
    result->success = true;
    result->status_code = fetch->status;
    result->body = std::string(fetch->data, fetch->numBytes);

    // Parse response headers if available
    // Note: Emscripten fetch API has limited header access due to CORS
    // Only headers exposed by Access-Control-Expose-Headers are available

    result->completed = true;
  }
  result->cv.notify_one();

  emscripten_fetch_close(fetch);
}

void EmscriptenHttpClient::OnFetchError(emscripten_fetch_t* fetch) {
  FetchResult* result = static_cast<FetchResult*>(fetch->userData);

  {
    std::lock_guard<std::mutex> lock(result->mutex);
    result->success = false;
    result->status_code = fetch->status;

    if (fetch->status == 0) {
      result->error_message = "Network error or CORS blocking";
    } else {
      result->error_message = "HTTP error: " + std::to_string(fetch->status);
    }

    result->completed = true;
  }
  result->cv.notify_one();

  emscripten_fetch_close(fetch);
}

void EmscriptenHttpClient::OnFetchProgress(emscripten_fetch_t* fetch) {
  // Progress callback - can be used for download progress
  // Not implemented for now
  (void)fetch;  // Suppress unused parameter warning
}

absl::StatusOr<HttpResponse> EmscriptenHttpClient::PerformFetch(
    const std::string& method,
    const std::string& url,
    const std::string& body,
    const Headers& headers) {

  // Create result structure
  FetchResult result;

  // Initialize fetch attributes
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);

  // Set HTTP method
  strncpy(attr.requestMethod, method.c_str(), sizeof(attr.requestMethod) - 1);
  attr.requestMethod[sizeof(attr.requestMethod) - 1] = '\0';

  // Set attributes for synchronous-style operation
  // We use async fetch with callbacks but wait for completion
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

  // Set callbacks
  attr.onsuccess = OnFetchSuccess;
  attr.onerror = OnFetchError;
  attr.onprogress = OnFetchProgress;
  attr.userData = &result;

  // Set timeout
  attr.timeoutMSecs = timeout_seconds_ * 1000;

  // Prepare headers
  std::vector<const char*> header_strings;
  std::vector<std::string> header_storage;

  for (const auto& [key, value] : headers) {
    header_storage.push_back(key);
    header_storage.push_back(value);
  }

  // Add Content-Type for POST/PUT if not provided
  if ((method == "POST" || method == "PUT") && !body.empty()) {
    bool has_content_type = false;
    for (const auto& [key, value] : headers) {
      if (key == "Content-Type") {
        has_content_type = true;
        break;
      }
    }
    if (!has_content_type) {
      header_storage.push_back("Content-Type");
      header_storage.push_back("application/json");
    }
  }

  // Convert to C-style array
  for (const auto& str : header_storage) {
    header_strings.push_back(str.c_str());
  }
  header_strings.push_back(nullptr);  // Null-terminate

  if (!header_strings.empty() && header_strings.size() > 1) {
    attr.requestHeaders = header_strings.data();
  }

  // Set request body for POST/PUT
  if (!body.empty() && (method == "POST" || method == "PUT")) {
    attr.requestData = body.c_str();
    attr.requestDataSize = body.length();
  }

  // Perform the fetch
  emscripten_fetch_t* fetch = emscripten_fetch(&attr, url.c_str());

  if (!fetch) {
    return absl::InternalError("Failed to initiate fetch request");
  }

  // Wait for completion (convert async to sync)
  {
    std::unique_lock<std::mutex> lock(result.mutex);
    result.cv.wait(lock, [&result] { return result.completed; });
  }

  // Check result
  if (!result.success) {
    return absl::UnavailableError(result.error_message.empty()
        ? "Fetch request failed"
        : result.error_message);
  }

  // Build response
  HttpResponse response;
  response.status_code = result.status_code;
  response.body = result.body;
  response.headers = result.headers;

  return response;
}

absl::StatusOr<HttpResponse> EmscriptenHttpClient::Get(
    const std::string& url,
    const Headers& headers) {
  return PerformFetch("GET", url, "", headers);
}

absl::StatusOr<HttpResponse> EmscriptenHttpClient::Post(
    const std::string& url,
    const std::string& body,
    const Headers& headers) {
  return PerformFetch("POST", url, body, headers);
}

absl::StatusOr<HttpResponse> EmscriptenHttpClient::Put(
    const std::string& url,
    const std::string& body,
    const Headers& headers) {
  return PerformFetch("PUT", url, body, headers);
}

absl::StatusOr<HttpResponse> EmscriptenHttpClient::Delete(
    const std::string& url,
    const Headers& headers) {
  return PerformFetch("DELETE", url, "", headers);
}

}  // namespace net
}  // namespace yaze

#endif  // __EMSCRIPTEN__