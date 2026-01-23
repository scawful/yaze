#include "cli/service/ai/service_factory.h"

#include "cli/service/ai/browser_ai_service.h"
#include "cli/service/ai/ai_service.h"
#include "app/net/http_client.h"
#include "absl/strings/match.h"

namespace {

std::string NormalizeOpenAIApiBase(std::string base) {
  if (base.empty()) {
    return base;
  }
  if (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  if (!absl::EndsWith(base, "/v1")) {
    base += "/v1";
  }
  return base;
}

}  // namespace

#ifdef __EMSCRIPTEN__
#include "app/net/wasm/emscripten_http_client.h"
#endif

namespace yaze {
namespace cli {

std::unique_ptr<AIService> CreateAIService() {
  AIServiceConfig config;
  config.provider = "gemini";
  config.model = "gemini-1.5-flash";
  return CreateAIService(config);
}

std::unique_ptr<AIService> CreateAIService(const AIServiceConfig& config) {
  // For browser, we always use BrowserAIService wrapping Gemini
  // The browser client handles API keys via config/JS

  BrowserAIConfig browser_config;
  browser_config.provider = config.provider.empty() ? "gemini" : config.provider;
  browser_config.model = config.model;
  if (browser_config.model.empty()) {
    browser_config.model =
        (browser_config.provider == "openai") ? "gpt-4o-mini"
                                              : "gemini-1.5-flash";
  }
  if (browser_config.provider == "openai") {
    browser_config.api_key = config.openai_api_key;
    browser_config.api_base = NormalizeOpenAIApiBase(config.openai_base_url);
  } else {
    browser_config.api_key = config.gemini_api_key;
  }
  browser_config.verbose = config.verbose;

#ifdef __EMSCRIPTEN__
  auto http_client = std::make_unique<net::EmscriptenHttpClient>();
#else
  // Fallback for non-wasm builds if this file is included
  std::unique_ptr<net::IHttpClient> http_client = nullptr;
#endif

  return std::make_unique<BrowserAIService>(browser_config, std::move(http_client));
}

absl::StatusOr<std::unique_ptr<AIService>> CreateAIServiceStrict(
    const AIServiceConfig& config) {
  return CreateAIService(config);
}

}  // namespace cli
}  // namespace yaze
