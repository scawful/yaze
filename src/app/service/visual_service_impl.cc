#include "app/service/visual_service_impl.h"

#ifdef YAZE_WITH_GRPC

#include <chrono>
#include <filesystem>
#include <fstream>

#include "absl/strings/str_cat.h"
#include "app/test/visual_diff_engine.h"
#include "grpcpp/grpcpp.h"
#include "protos/visual_service.grpc.pb.h"
#include "protos/visual_service.pb.h"

namespace yaze {

namespace {

// Convert proto config to C++ config
test::VisualDiffConfig ProtoToConfig(const proto::ComparisonConfig& proto) {
  test::VisualDiffConfig config;
  config.tolerance = proto.tolerance() > 0 ? proto.tolerance() : 0.95f;
  config.color_threshold =
      proto.color_threshold() > 0 ? proto.color_threshold() : 10;
  config.generate_diff_image = proto.generate_diff_image();

  switch (proto.algorithm()) {
    case proto::ComparisonConfig::SSIM:
      config.algorithm = test::VisualDiffConfig::Algorithm::kSSIM;
      break;
    case proto::ComparisonConfig::PERCEPTUAL_HASH:
      config.algorithm = test::VisualDiffConfig::Algorithm::kPerceptualHash;
      break;
    default:
      config.algorithm = test::VisualDiffConfig::Algorithm::kPixelExact;
      break;
  }

  switch (proto.diff_style()) {
    case proto::ComparisonConfig::SIDE_BY_SIDE:
      config.diff_style = test::VisualDiffConfig::DiffStyle::kSideBySide;
      break;
    case proto::ComparisonConfig::HEATMAP:
      config.diff_style = test::VisualDiffConfig::DiffStyle::kHeatmap;
      break;
    default:
      config.diff_style = test::VisualDiffConfig::DiffStyle::kRedHighlight;
      break;
  }

  // Convert ignore regions
  for (const auto& region : proto.ignore_regions()) {
    config.ignore_regions.push_back(test::ScreenRegion{
        region.x(), region.y(), region.width(), region.height()});
  }

  return config;
}

// Convert C++ result to proto
void ResultToProto(const test::VisualDiffResult& result,
                   proto::VisualDiffResult* proto) {
  proto->set_identical(result.identical);
  proto->set_passed(result.passed);
  proto->set_similarity(result.similarity);
  proto->set_difference_pct(result.difference_pct);
  proto->set_width(result.width);
  proto->set_height(result.height);
  proto->set_total_pixels(result.total_pixels);
  proto->set_differing_pixels(result.differing_pixels);

  if (!result.diff_image_png.empty()) {
    proto->set_diff_image_png(result.diff_image_png.data(),
                              result.diff_image_png.size());
  }

  proto->set_diff_description(result.diff_description);

  for (const auto& region : result.significant_regions) {
    auto* proto_region = proto->add_significant_regions();
    proto_region->set_x(region.x);
    proto_region->set_y(region.y);
    proto_region->set_width(region.width);
    proto_region->set_height(region.height);
    proto_region->set_local_diff_pct(region.local_diff_pct);
  }

  if (!result.error_message.empty()) {
    proto->set_error(result.error_message);
  }
}

}  // namespace

// ============================================================================
// VisualServiceImpl
// ============================================================================

VisualServiceImpl::VisualServiceImpl()
    : reference_dir_("/tmp/yaze_visual_references") {}

VisualServiceImpl::~VisualServiceImpl() = default;

void VisualServiceImpl::SetReferenceImageDir(const std::string& path) {
  reference_dir_ = path;
  std::filesystem::create_directories(path);
}

void VisualServiceImpl::SetAIVisionVerifier(test::AIVisionVerifier* verifier) {
  vision_verifier_ = verifier;
}

test::VisualDiffEngine& VisualServiceImpl::GetDiffEngine() {
  if (!diff_engine_) {
    diff_engine_ = std::make_unique<test::VisualDiffEngine>();
  }
  return *diff_engine_;
}

absl::Status VisualServiceImpl::ComparePngData(
    const proto::ComparePngDataRequest* request,
    proto::ComparePngDataResponse* response) {
  if (request->png_a().empty() || request->png_b().empty()) {
    return absl::InvalidArgumentError("Both PNG images must be provided");
  }

  auto& engine = GetDiffEngine();
  if (request->has_config()) {
    engine.SetConfig(ProtoToConfig(request->config()));
  }

  std::vector<uint8_t> png_a(request->png_a().begin(), request->png_a().end());
  std::vector<uint8_t> png_b(request->png_b().begin(), request->png_b().end());

  auto result = engine.ComparePngData(png_a, png_b);
  if (!result.ok()) {
    return result.status();
  }

  ResultToProto(*result, response->mutable_result());
  return absl::OkStatus();
}

absl::Status VisualServiceImpl::ComparePngFiles(
    const proto::ComparePngFilesRequest* request,
    proto::ComparePngFilesResponse* response) {
  if (request->path_a().empty() || request->path_b().empty()) {
    return absl::InvalidArgumentError("Both file paths must be provided");
  }

  auto& engine = GetDiffEngine();
  if (request->has_config()) {
    engine.SetConfig(ProtoToConfig(request->config()));
  }

  auto result = engine.ComparePngFiles(request->path_a(), request->path_b());
  if (!result.ok()) {
    return result.status();
  }

  ResultToProto(*result, response->mutable_result());
  return absl::OkStatus();
}

absl::Status VisualServiceImpl::CompareWithReference(
    const proto::CompareWithReferenceRequest* request,
    proto::CompareWithReferenceResponse* response) {
  if (request->png_data().empty()) {
    return absl::InvalidArgumentError("PNG data must be provided");
  }
  if (request->reference_path().empty()) {
    return absl::InvalidArgumentError("Reference path must be provided");
  }

  auto& engine = GetDiffEngine();
  if (request->has_config()) {
    engine.SetConfig(ProtoToConfig(request->config()));
  }

  std::vector<uint8_t> png_data(request->png_data().begin(),
                                request->png_data().end());

  auto result =
      engine.CompareWithReference(png_data, request->reference_path());
  if (!result.ok()) {
    return result.status();
  }

  ResultToProto(*result, response->mutable_result());
  return absl::OkStatus();
}

absl::Status VisualServiceImpl::CompareRegion(
    const proto::CompareRegionRequest* request,
    proto::CompareRegionResponse* response) {
  if (request->png_a().empty() || request->png_b().empty()) {
    return absl::InvalidArgumentError("Both PNG images must be provided");
  }

  auto& engine = GetDiffEngine();
  if (request->has_config()) {
    engine.SetConfig(ProtoToConfig(request->config()));
  }

  std::vector<uint8_t> png_a(request->png_a().begin(), request->png_a().end());
  std::vector<uint8_t> png_b(request->png_b().begin(), request->png_b().end());

  test::ScreenRegion region{request->region().x(), request->region().y(),
                            request->region().width(),
                            request->region().height()};

  auto result = engine.CompareRegion(png_a, png_b, region);
  if (!result.ok()) {
    return result.status();
  }

  ResultToProto(*result, response->mutable_result());
  return absl::OkStatus();
}

absl::Status VisualServiceImpl::GenerateDiffImage(
    const proto::GenerateDiffImageRequest* request,
    proto::GenerateDiffImageResponse* response) {
  if (request->png_a().empty() || request->png_b().empty()) {
    return absl::InvalidArgumentError("Both PNG images must be provided");
  }

  test::VisualDiffConfig config;
  config.generate_diff_image = true;

  switch (request->style()) {
    case proto::ComparisonConfig::SIDE_BY_SIDE:
      config.diff_style = test::VisualDiffConfig::DiffStyle::kSideBySide;
      break;
    case proto::ComparisonConfig::HEATMAP:
      config.diff_style = test::VisualDiffConfig::DiffStyle::kHeatmap;
      break;
    default:
      config.diff_style = test::VisualDiffConfig::DiffStyle::kRedHighlight;
      break;
  }

  test::VisualDiffEngine engine(config);

  // Decode PNGs
  auto screenshot_a = test::VisualDiffEngine::DecodePng(
      std::vector<uint8_t>(request->png_a().begin(), request->png_a().end()));
  if (!screenshot_a.ok()) {
    response->set_success(false);
    response->set_error(std::string(screenshot_a.status().message()));
    return absl::OkStatus();
  }

  auto screenshot_b = test::VisualDiffEngine::DecodePng(
      std::vector<uint8_t>(request->png_b().begin(), request->png_b().end()));
  if (!screenshot_b.ok()) {
    response->set_success(false);
    response->set_error(std::string(screenshot_b.status().message()));
    return absl::OkStatus();
  }

  // Generate diff
  auto diff_result = engine.GenerateDiffPng(*screenshot_a, *screenshot_b);
  if (!diff_result.ok()) {
    response->set_success(false);
    response->set_error(std::string(diff_result.status().message()));
    return absl::OkStatus();
  }

  response->set_success(true);
  response->set_diff_image_png(diff_result->data(), diff_result->size());
  return absl::OkStatus();
}

absl::Status VisualServiceImpl::RunRegressionTest(
    const proto::RunRegressionTestRequest* request,
    proto::RunRegressionTestResponse* response) {
  if (request->current_screenshot().empty()) {
    return absl::InvalidArgumentError("Current screenshot must be provided");
  }
  if (request->reference_id().empty()) {
    return absl::InvalidArgumentError("Reference ID must be provided");
  }

  // Build reference path
  std::string reference_path =
      absl::StrCat(reference_dir_, "/", request->reference_id(), ".png");

  if (!std::filesystem::exists(reference_path)) {
    return absl::NotFoundError(
        absl::StrCat("Reference image not found: ", request->reference_id()));
  }

  auto& engine = GetDiffEngine();
  if (request->has_config()) {
    engine.SetConfig(ProtoToConfig(request->config()));
  }

  std::vector<uint8_t> current(request->current_screenshot().begin(),
                               request->current_screenshot().end());

  auto result = engine.CompareWithReference(current, reference_path);
  if (!result.ok()) {
    return result.status();
  }

  response->set_passed(result->passed);
  ResultToProto(*result, response->mutable_result());
  response->set_test_name(request->test_name());
  response->set_reference_id(request->reference_id());
  response->set_timestamp_ms(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());

  return absl::OkStatus();
}

absl::Status VisualServiceImpl::ListReferenceImages(
    const proto::ListReferenceImagesRequest* request,
    proto::ListReferenceImagesResponse* response) {
  if (!std::filesystem::exists(reference_dir_)) {
    return absl::OkStatus();  // Empty list
  }

  for (const auto& entry :
       std::filesystem::directory_iterator(reference_dir_)) {
    if (entry.path().extension() == ".png") {
      auto* img = response->add_images();
      img->set_id(entry.path().stem().string());
      img->set_path(entry.path().string());

      // Apply filters
      if (!request->category().empty()) {
        // Category filtering would require metadata storage
        // For now, skip filtering
      }
      if (!request->prefix().empty()) {
        if (img->id().find(request->prefix()) != 0) {
          response->mutable_images()->RemoveLast();
          continue;
        }
      }

      // Try to get image dimensions
      auto screenshot = test::VisualDiffEngine::LoadPng(entry.path().string());
      if (screenshot.ok()) {
        img->set_width(screenshot->width);
        img->set_height(screenshot->height);
      }

      // Get file creation time (portable: avoid file_clock::to_sys which
      // is missing on MSVC's STL)
      auto ftime = std::filesystem::last_write_time(entry);
      auto file_duration = ftime.time_since_epoch();
      auto ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(file_duration);
      img->set_created_timestamp_ms(ms.count());
    }
  }

  return absl::OkStatus();
}

absl::Status VisualServiceImpl::SaveReferenceImage(
    const proto::SaveReferenceImageRequest* request,
    proto::SaveReferenceImageResponse* response) {
  if (request->png_data().empty()) {
    return absl::InvalidArgumentError("PNG data must be provided");
  }
  if (request->reference_id().empty()) {
    return absl::InvalidArgumentError("Reference ID must be provided");
  }

  std::filesystem::create_directories(reference_dir_);

  std::string path =
      absl::StrCat(reference_dir_, "/", request->reference_id(), ".png");

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    response->set_success(false);
    response->set_error(
        absl::StrCat("Failed to open file for writing: ", path));
    return absl::OkStatus();
  }

  file.write(request->png_data().data(), request->png_data().size());

  response->set_success(true);
  response->set_path(path);
  return absl::OkStatus();
}

absl::Status VisualServiceImpl::AnalyzeScreenshot(
    const proto::AnalyzeScreenshotRequest* request,
    proto::AnalyzeScreenshotResponse* response) {
  if (!vision_verifier_) {
    response->set_success(false);
    response->set_error("AI vision verifier not configured");
    return absl::OkStatus();
  }

  // TODO: Implement integration with AIVisionVerifier
  // This would call vision_verifier_->AnalyzeImage() or similar

  response->set_success(false);
  response->set_error("AI analysis not yet implemented");
  return absl::OkStatus();
}

absl::Status VisualServiceImpl::VerifyVisualCondition(
    const proto::VerifyVisualConditionRequest* request,
    proto::VerifyVisualConditionResponse* response) {
  if (!vision_verifier_) {
    response->set_success(false);
    response->set_error("AI vision verifier not configured");
    return absl::OkStatus();
  }

  // TODO: Implement integration with AIVisionVerifier
  // This would call vision_verifier_->VerifyCondition() or similar

  response->set_success(false);
  response->set_error("Visual condition verification not yet implemented");
  return absl::OkStatus();
}

// ============================================================================
// gRPC Service Wrapper
// ============================================================================

class VisualServiceGrpc final : public proto::VisualService::Service {
 public:
  explicit VisualServiceGrpc(VisualServiceImpl* impl) : impl_(impl) {}

#define IMPL_RPC(method)                                            \
  grpc::Status method(grpc::ServerContext* context,                 \
                      const proto::method##Request* request,        \
                      proto::method##Response* response) override { \
    (void)context;                                                  \
    auto status = impl_->method(request, response);                 \
    if (!status.ok()) {                                             \
      return grpc::Status(grpc::StatusCode::INTERNAL,               \
                          std::string(status.message()));           \
    }                                                               \
    return grpc::Status::OK;                                        \
  }

  IMPL_RPC(ComparePngData)
  IMPL_RPC(ComparePngFiles)
  IMPL_RPC(CompareWithReference)
  IMPL_RPC(CompareRegion)
  IMPL_RPC(GenerateDiffImage)
  IMPL_RPC(RunRegressionTest)
  IMPL_RPC(ListReferenceImages)
  IMPL_RPC(SaveReferenceImage)
  IMPL_RPC(AnalyzeScreenshot)
  IMPL_RPC(VerifyVisualCondition)

#undef IMPL_RPC

 private:
  VisualServiceImpl* impl_;
};

std::unique_ptr<grpc::Service> CreateVisualServiceGrpc(
    VisualServiceImpl* impl) {
  return std::make_unique<VisualServiceGrpc>(impl);
}

}  // namespace yaze

#endif  // YAZE_WITH_GRPC
