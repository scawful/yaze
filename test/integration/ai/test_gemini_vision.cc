#include <filesystem>
#include <fstream>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "cli/service/ai/gemini_ai_service.h"

#ifdef YAZE_WITH_GRPC
#include "app/service/screenshot_utils.h"
#endif

namespace yaze {
namespace test {

class GeminiVisionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Check if GEMINI_API_KEY is set
    const char* api_key = std::getenv("GEMINI_API_KEY");
    if (!api_key || std::string(api_key).empty()) {
      GTEST_SKIP() << "GEMINI_API_KEY not set. Skipping multimodal tests.";
    }
    
    api_key_ = api_key;
    
    // Create test data directory
    test_dir_ = std::filesystem::temp_directory_path() / "yaze_multimodal_test";
    std::filesystem::create_directories(test_dir_);
  }
  
  void TearDown() override {
    // Clean up test directory
    if (std::filesystem::exists(test_dir_)) {
      std::filesystem::remove_all(test_dir_);
    }
  }
  
  // Helper: Create a simple test image (16x16 PNG)
  std::filesystem::path CreateTestImage() {
    auto image_path = test_dir_ / "test_image.png";
    
    // Create a minimal PNG file (16x16 red square)
    // PNG signature + IHDR + IDAT + IEND
    const unsigned char png_data[] = {
      // PNG signature
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
      // IHDR chunk
      0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
      0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10,
      0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x91, 0x68,
      0x36,
      // IDAT chunk (minimal data)
      0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, 0x54,
      0x08, 0x99, 0x63, 0xF8, 0xCF, 0xC0, 0x00, 0x00,
      0x03, 0x01, 0x01, 0x00, 0x18, 0xDD, 0x8D, 0xB4,
      // IEND chunk
      0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44,
      0xAE, 0x42, 0x60, 0x82
    };
    
    std::ofstream file(image_path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(png_data), sizeof(png_data));
    file.close();
    
    return image_path;
  }
  
  std::string api_key_;
  std::filesystem::path test_dir_;
};

TEST_F(GeminiVisionTest, BasicImageAnalysis) {
  cli::GeminiConfig config;
  config.api_key = api_key_;
  config.model = "gemini-2.5-flash";  // Vision-capable model
  config.verbose = false;
  
  cli::GeminiAIService service(config);
  
  // Create test image
  auto image_path = CreateTestImage();
  ASSERT_TRUE(std::filesystem::exists(image_path));
  
  // Send multimodal request
  auto response = service.GenerateMultimodalResponse(
      image_path.string(),
      "Describe this image in one sentence."
  );
  
  ASSERT_TRUE(response.ok()) << response.status().message();
  EXPECT_FALSE(response->text_response.empty());
  
  std::cout << "Vision API response: " << response->text_response << std::endl;
}

TEST_F(GeminiVisionTest, ImageWithSpecificPrompt) {
  cli::GeminiConfig config;
  config.api_key = api_key_;
  config.model = "gemini-2.5-flash";
  config.verbose = false;
  
  cli::GeminiAIService service(config);
  
  auto image_path = CreateTestImage();
  
  // Ask specific question about the image
  auto response = service.GenerateMultimodalResponse(
      image_path.string(),
      "What color is the dominant color in this image? Answer with just the color name."
  );
  
  ASSERT_TRUE(response.ok()) << response.status().message();
  EXPECT_FALSE(response->text_response.empty());
  
  // Response should mention "red" since we created a red square
  std::string response_lower = response->text_response;
  std::transform(response_lower.begin(), response_lower.end(),
                response_lower.begin(), ::tolower);
  EXPECT_TRUE(response_lower.find("red") != std::string::npos ||
              response_lower.find("pink") != std::string::npos)
      << "Expected color 'red' or 'pink' in response: " << response->text_response;
}

TEST_F(GeminiVisionTest, InvalidImagePath) {
  cli::GeminiConfig config;
  config.api_key = api_key_;
  config.model = "gemini-2.5-flash";
  
  cli::GeminiAIService service(config);
  
  // Try with non-existent image
  auto response = service.GenerateMultimodalResponse(
      "/nonexistent/image.png",
      "Describe this image."
  );
  
  EXPECT_FALSE(response.ok());
  EXPECT_TRUE(absl::IsNotFound(response.status()) ||
              absl::IsInternal(response.status()));
}

#ifdef YAZE_WITH_GRPC
// Integration test with screenshot capture
TEST_F(GeminiVisionTest, ScreenshotCaptureIntegration) {
  // Note: This test requires a running YAZE instance with gRPC test harness
  // Skip if we can't connect
  
  cli::GeminiConfig config;
  config.api_key = api_key_;
  config.model = "gemini-2.5-flash";
  config.verbose = false;
  
  cli::GeminiAIService service(config);
  
  // Attempt to capture a screenshot
  auto screenshot_result = yaze::test::CaptureHarnessScreenshot(
      (test_dir_ / "screenshot.png").string());
  
  if (!screenshot_result.ok()) {
    GTEST_SKIP() << "Screenshot capture failed (YAZE may not be running): "
                 << screenshot_result.status().message();
  }
  
  // Analyze the captured screenshot
  auto response = service.GenerateMultimodalResponse(
      screenshot_result->file_path,
      "What UI elements are visible in this screenshot? List them."
  );
  
  ASSERT_TRUE(response.ok()) << response.status().message();
  EXPECT_FALSE(response->text_response.empty());
  
  std::cout << "Screenshot analysis: " << response->text_response << std::endl;
}
#endif

// Performance test
TEST_F(GeminiVisionTest, MultipleRequestsSequential) {
  cli::GeminiConfig config;
  config.api_key = api_key_;
  config.model = "gemini-2.5-flash";
  config.verbose = false;
  
  cli::GeminiAIService service(config);
  
  auto image_path = CreateTestImage();
  
  // Make 3 sequential requests
  const int num_requests = 3;
  for (int i = 0; i < num_requests; ++i) {
    auto response = service.GenerateMultimodalResponse(
        image_path.string(),
        absl::StrCat("Request ", i + 1, ": Describe this image briefly.")
    );
    
    ASSERT_TRUE(response.ok()) << "Request " << i + 1 << " failed: "
                               << response.status().message();
    EXPECT_FALSE(response->text_response.empty());
  }
}

// Rate limiting test (should handle gracefully)
TEST_F(GeminiVisionTest, RateLimitHandling) {
  cli::GeminiConfig config;
  config.api_key = api_key_;
  config.model = "gemini-2.5-flash";
  config.verbose = false;
  
  cli::GeminiAIService service(config);
  
  auto image_path = CreateTestImage();
  
  // Make many rapid requests (may hit rate limit)
  int successful = 0;
  int rate_limited = 0;
  
  for (int i = 0; i < 10; ++i) {
    auto response = service.GenerateMultimodalResponse(
        image_path.string(),
        "Describe this image."
    );
    
    if (response.ok()) {
      successful++;
    } else if (absl::IsResourceExhausted(response.status()) ||
               response.status().message().find("429") != std::string::npos) {
      rate_limited++;
    }
  }
  
  // At least some requests should succeed
  EXPECT_GT(successful, 0) << "No successful requests out of 10";
  
  // If we hit rate limits, that's expected behavior (not a failure)
  if (rate_limited > 0) {
    std::cout << "Note: Hit rate limit on " << rate_limited << " out of 10 requests (expected)" << std::endl;
  }
}

}  // namespace test
}  // namespace yaze

// Note: main() is provided by yaze_test.cc for the unified test runner
