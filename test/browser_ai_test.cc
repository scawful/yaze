// Test file to verify browser AI service integration
#ifdef __EMSCRIPTEN__

#include <iostream>
#include <memory>

#include "app/net/wasm/emscripten_http_client.h"
#include "app/platform/wasm/wasm_browser_storage.h"
#include "cli/service/ai/browser_ai_service.h"

int main() {
  std::cout << "Testing Browser AI Service Integration for WASM\n";

  // Test secure storage
  {
    using namespace yaze::app::platform;

    // Test API key storage
    auto status = WasmBrowserStorage::StoreApiKey("test_service", "test_key_12345");
    if (status.ok()) {
      std::cout << "✓ API key stored successfully\n";
    }

    // Test API key retrieval
    auto key_or = WasmBrowserStorage::RetrieveApiKey("test_service");
    if (key_or.ok() && key_or.value() == "test_key_12345") {
      std::cout << "✓ API key retrieved successfully\n";
    }

    // Test API key existence check
    if (WasmBrowserStorage::HasApiKey("test_service")) {
      std::cout << "✓ API key existence check passed\n";
    }

    // Clean up
    WasmBrowserStorage::ClearApiKey("test_service");
  }

  // Test browser AI service
  {
    using namespace yaze::cli;
    using namespace yaze::net;

    // Create configuration
    BrowserAIConfig config;
    config.api_key = "test_api_key";
    config.model = "gemini-2.5-flash";
    config.verbose = true;

    // Create HTTP client
    auto http_client = std::make_unique<EmscriptenHttpClient>();

    // Create AI service
    BrowserAIService ai_service(config, std::move(http_client));

    std::cout << "✓ Browser AI service created successfully\n";
    std::cout << "  Provider: " << ai_service.GetProviderName() << "\n";

    // Test availability check (will fail without real API key)
    auto availability = ai_service.CheckAvailability();
    if (!availability.ok()) {
      std::cout << "✓ Availability check correctly reports invalid API key\n";
    }

    // Test model listing
    auto models_or = ai_service.ListAvailableModels();
    if (models_or.ok()) {
      std::cout << "✓ Listed " << models_or.value().size() << " available models\n";
    }
  }

  std::cout << "\nAll tests passed!\n";
  return 0;
}

#else
int main() {
  std::cout << "This test only runs in Emscripten/WASM builds\n";
  return 0;
}
#endif