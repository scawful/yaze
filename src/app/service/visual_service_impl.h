#ifndef YAZE_APP_SERVICE_VISUAL_SERVICE_IMPL_H_
#define YAZE_APP_SERVICE_VISUAL_SERVICE_IMPL_H_

#include "util/grpc_win_compat.h"

#ifdef YAZE_WITH_GRPC

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "grpcpp/impl/service_type.h"

namespace yaze {

// Forward declarations for proto types
namespace proto {
class ComparePngDataRequest;
class ComparePngDataResponse;
class ComparePngFilesRequest;
class ComparePngFilesResponse;
class CompareWithReferenceRequest;
class CompareWithReferenceResponse;
class CompareRegionRequest;
class CompareRegionResponse;
class GenerateDiffImageRequest;
class GenerateDiffImageResponse;
class RunRegressionTestRequest;
class RunRegressionTestResponse;
class ListReferenceImagesRequest;
class ListReferenceImagesResponse;
class SaveReferenceImageRequest;
class SaveReferenceImageResponse;
class AnalyzeScreenshotRequest;
class AnalyzeScreenshotResponse;
class VerifyVisualConditionRequest;
class VerifyVisualConditionResponse;
}  // namespace proto

namespace test {
class VisualDiffEngine;
class AIVisionVerifier;
}  // namespace test

/**
 * @brief Implementation of VisualService gRPC service.
 *
 * Provides visual comparison and analysis tools for:
 * - AI agents via MCP tool calls
 * - Visual regression testing
 * - Screenshot-based debugging workflows
 * - Multimodal AI analysis integration
 */
class VisualServiceImpl {
 public:
  VisualServiceImpl();
  ~VisualServiceImpl();

  // Configure reference image directory
  void SetReferenceImageDir(const std::string& path);
  const std::string& GetReferenceImageDir() const { return reference_dir_; }

  // Optionally set AI vision verifier for analysis tools
  void SetAIVisionVerifier(test::AIVisionVerifier* verifier);

  // --- Visual Comparison Operations ---

  /**
   * @brief Compare two PNG images from raw data.
   */
  absl::Status ComparePngData(const proto::ComparePngDataRequest* request,
                              proto::ComparePngDataResponse* response);

  /**
   * @brief Compare two PNG files.
   */
  absl::Status ComparePngFiles(const proto::ComparePngFilesRequest* request,
                               proto::ComparePngFilesResponse* response);

  /**
   * @brief Compare PNG data against a reference file.
   */
  absl::Status CompareWithReference(
      const proto::CompareWithReferenceRequest* request,
      proto::CompareWithReferenceResponse* response);

  /**
   * @brief Compare a specific region of two images.
   */
  absl::Status CompareRegion(const proto::CompareRegionRequest* request,
                             proto::CompareRegionResponse* response);

  // --- Diff Image Generation ---

  /**
   * @brief Generate a diff image from two PNGs.
   */
  absl::Status GenerateDiffImage(const proto::GenerateDiffImageRequest* request,
                                 proto::GenerateDiffImageResponse* response);

  // --- Visual Regression Testing ---

  /**
   * @brief Run a regression test against a reference.
   */
  absl::Status RunRegressionTest(const proto::RunRegressionTestRequest* request,
                                 proto::RunRegressionTestResponse* response);

  /**
   * @brief List available reference images.
   */
  absl::Status ListReferenceImages(
      const proto::ListReferenceImagesRequest* request,
      proto::ListReferenceImagesResponse* response);

  /**
   * @brief Save a new reference image.
   */
  absl::Status SaveReferenceImage(
      const proto::SaveReferenceImageRequest* request,
      proto::SaveReferenceImageResponse* response);

  // --- AI Visual Analysis ---

  /**
   * @brief Analyze a screenshot using AI vision.
   */
  absl::Status AnalyzeScreenshot(const proto::AnalyzeScreenshotRequest* request,
                                 proto::AnalyzeScreenshotResponse* response);

  /**
   * @brief Verify a visual condition using AI.
   */
  absl::Status VerifyVisualCondition(
      const proto::VerifyVisualConditionRequest* request,
      proto::VerifyVisualConditionResponse* response);

 private:
  // Get or create diff engine
  test::VisualDiffEngine& GetDiffEngine();

  // Reference image directory
  std::string reference_dir_;

  // Diff engine (lazy initialized)
  std::unique_ptr<test::VisualDiffEngine> diff_engine_;

  // AI vision verifier (optional, not owned)
  test::AIVisionVerifier* vision_verifier_ = nullptr;
};

/**
 * @brief Factory function to create gRPC service wrapper.
 *
 * Creates the gRPC service wrapper for VisualServiceImpl.
 * The wrapper handles the conversion between gRPC and absl::Status.
 *
 * @param impl Pointer to implementation (not owned)
 * @return Unique pointer to gRPC service
 */
std::unique_ptr<grpc::Service> CreateVisualServiceGrpc(VisualServiceImpl* impl);

}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_SERVICE_VISUAL_SERVICE_IMPL_H_
