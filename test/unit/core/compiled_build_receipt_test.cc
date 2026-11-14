#include "core/compiled_build_receipt.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "unique_temp_path.h"
#include "util/rom_hash.h"

namespace yaze::core {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

std::string Text(const fs::path& path) {
  const auto text = path.generic_u8string();
  return std::string(text.begin(), text.end());
}

std::string Sha256(const std::vector<uint8_t>& bytes) {
  return ComputeSourceArtifactSha256(std::string_view(
      reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

void WriteFile(const fs::path& path, std::string_view bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  ASSERT_TRUE(out.good());
}

void WriteFile(const fs::path& path, const std::vector<uint8_t>& bytes) {
  WriteFile(path, std::string_view(reinterpret_cast<const char*>(bytes.data()),
                                   bytes.size()));
}

// One authoring project, its saved ROM and a completed build of it.
class CompiledBuildReceiptTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto unique = test::UniqueTempPath("compiled_receipt");
    fs::create_directories(unique);
    dir_ = fs::canonical(unique);
    base_bytes_.assign(0x8000, 0x11);
    output_bytes_.assign(0x8000, 0x22);
    base_ = dir_ / "rom";
    output_ = dir_ / "source" / "Roms" / "oos168x.sfc";
    manifest_ = dir_ / "source" / "Roms" / "oos168x.manifest.json";
    receipt_ = dir_ / "source" / "Roms" / "oos168x.build.json";
    WriteFile(base_, base_bytes_);
    WriteFile(output_, output_bytes_);
    WriteFile(manifest_, std::string_view("{\"manifest_version\":3}"));

    project_.name = "authoring";
    project_.filepath = Text(dir_ / "project.yaze");
    project_.rom_filename = "rom";
    project_.code_folder = "source";
    project_.build_target = Text(output_);
    project_.rom_metadata.expected_hash =
        util::ComputeSha1Hex(base_bytes_.data(), base_bytes_.size());
    receipt_json_ = {
        {"schema_version", 1},
        {"state", "completed_with_gaps"},
        {"root", Text(dir_ / "source")},
        {"base_rom", Row(base_, base_bytes_)},
        {"artifacts",
         {{"output_rom", Row(output_, output_bytes_)},
          {"manifest",
           {{"path", Text(manifest_)},
            {"sha256", ComputeSourceArtifactSha256("{\"manifest_version\":3}")},
            {"size", 22}}}}},
        {"finish",
         {{"exit_code", 0},
          {"required_gaps", Json::array()},
          {"optional_gaps", Json::array({"smoke"})}}}};
  }

  void TearDown() override {
    std::error_code error;
    fs::remove_all(dir_, error);
  }

  static Json Row(const fs::path& path, const std::vector<uint8_t>& bytes) {
    return {{"path", Text(path)},
            {"sha256", Sha256(bytes)},
            {"size", bytes.size()}};
  }

  absl::StatusOr<CompiledBuildIdentity> Verify() {
    WriteFile(receipt_, receipt_json_.dump());
    return VerifyProjectBuildReceipt(project_, receipt_);
  }

  void ExpectRefused(std::string_view reason) {
    const auto result = Verify();
    ASSERT_EQ(result.status().code(), absl::StatusCode::kFailedPrecondition)
        << result.status();
    EXPECT_NE(result.status().message().find(reason), std::string_view::npos)
        << result.status();
  }

  fs::path dir_, base_, output_, manifest_, receipt_;
  std::vector<uint8_t> base_bytes_, output_bytes_;
  project::YazeProject project_;
  Json receipt_json_;
};

TEST_F(CompiledBuildReceiptTest, AcceptsCurrentBuildAndBindsOutputBytes) {
  const auto build = Verify();
  ASSERT_TRUE(build.ok()) << build.status();
  EXPECT_EQ(build->state, "completed_with_gaps");
  EXPECT_EQ(build->base_rom_sha256, Sha256(base_bytes_));
  EXPECT_EQ(build->output_rom_sha256, Sha256(output_bytes_));
  EXPECT_EQ(build->output_rom_size, output_bytes_.size());
  EXPECT_EQ(fs::u8path(build->output_rom_path), output_);
  EXPECT_EQ(fs::u8path(build->manifest_path), manifest_);
  EXPECT_EQ(build->receipt_sha256,
            ComputeSourceArtifactSha256(receipt_json_.dump()));
  EXPECT_TRUE(CheckCompiledBuildBytes(*build, output_bytes_.data(),
                                      output_bytes_.size())
                  .ok());
  const auto base =
      CheckCompiledBuildBytes(*build, base_bytes_.data(), base_bytes_.size());
  EXPECT_EQ(base.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(base.message().find("receipt's output"), std::string_view::npos);
}

TEST_F(CompiledBuildReceiptTest, ManifestIsOptionalProvenance) {
  receipt_json_["artifacts"].erase("manifest");
  const auto build = Verify();
  ASSERT_TRUE(build.ok()) << build.status();
  EXPECT_TRUE(build->manifest_path.empty());
  EXPECT_TRUE(build->manifest_sha256.empty());
}

TEST_F(CompiledBuildReceiptTest, RefusesIncompleteOrFailedBuilds) {
  const Json original = receipt_json_;
  for (const auto& [key, value] : std::vector<std::pair<std::string, Json>>{
           {"state", "failed"}, {"state", "running"}, {"schema_version", 2}}) {
    SCOPED_TRACE(key);
    receipt_json_ = original;
    receipt_json_[key] = value;
    ExpectRefused("incomplete or failed");
  }
  receipt_json_ = original;
  receipt_json_["finish"]["exit_code"] = 1;
  ExpectRefused("incomplete or failed");
  receipt_json_ = original;
  receipt_json_["finish"]["required_gaps"] = Json::array({"assemble"});
  ExpectRefused("incomplete or failed");
}

TEST_F(CompiledBuildReceiptTest, RefusesReceiptFromAnotherProject) {
  const Json original = receipt_json_;
  receipt_json_["root"] = Text(dir_ / "other");
  ExpectRefused("another code folder");

  receipt_json_ = original;
  const auto other_rom = dir_ / "other.sfc";
  WriteFile(other_rom, base_bytes_);
  receipt_json_["base_rom"] = Row(other_rom, base_bytes_);
  ExpectRefused("not the project ROM");

  receipt_json_ = original;
  project_.build_target = Text(dir_ / "source" / "Roms" / "other.sfc");
  ExpectRefused("not the project build target");
}

TEST_F(CompiledBuildReceiptTest, RefusesArtifactsChangedSinceTheBuild) {
  auto changed = output_bytes_;
  changed[0x100] ^= 1;
  WriteFile(output_, changed);
  ExpectRefused("output ROM changed since the build");

  WriteFile(output_, output_bytes_);
  WriteFile(base_, output_bytes_);
  ExpectRefused("base ROM changed since the build");

  WriteFile(base_, base_bytes_);
  WriteFile(manifest_, std::string_view("{\"manifest_version\":4}"));
  ExpectRefused("output manifest changed since the build");

  WriteFile(manifest_, std::string_view("{\"manifest_version\":3}"));
  receipt_json_["artifacts"]["output_rom"]["size"] = 0x4000;
  ExpectRefused("byte count differs");
}

TEST_F(CompiledBuildReceiptTest, KeepsTheAuthoringIdentityGuard) {
  project_.rom_metadata.expected_hash = std::string(40, '0');
  ExpectRefused("expected ROM identity");
}

TEST_F(CompiledBuildReceiptTest, RefusesAnOutputThatAliasesTheBaseRom) {
  receipt_json_["artifacts"]["output_rom"] = Row(base_, base_bytes_);
  project_.build_target = Text(base_);
  ExpectRefused("aliases the base ROM");
}

TEST_F(CompiledBuildReceiptTest, RefusesMalformedReceiptsAndProjects) {
  receipt_json_.erase("finish");
  ExpectRefused("malformed receipt");

  receipt_json_ = Json::array();
  ExpectRefused("JSON object");

  WriteFile(receipt_, std::string_view("{not json"));
  const auto parse = VerifyProjectBuildReceipt(project_, receipt_);
  EXPECT_EQ(parse.status().code(), absl::StatusCode::kFailedPrecondition);

  const auto missing = VerifyProjectBuildReceipt(project_, dir_ / "none.json");
  EXPECT_EQ(missing.status().code(), absl::StatusCode::kFailedPrecondition);

  project_.build_target.clear();
  ExpectRefused("build target");
}

}  // namespace
}  // namespace yaze::core
