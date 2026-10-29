#include "app/editor/graphics/catalog_palette_preview.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "absl/status/status.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {
namespace {
class CatalogPalettePreviewTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = std::filesystem::temp_directory_path() /
            ("yaze_catalog_palette_" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root_ / "palettes");
    // Deliberately not a decodable ROM: this API verifies bytes, not ROM tables.
    std::ofstream(root_ / "source.sfc", std::ios::binary) << kSource;
    document_ = {
        {"schema_version", 1},
        {"sources",
         nlohmann::json::array(
             {{{"path", "../source.sfc"},
               {"title", "Source"},
               {"sha256", core::ComputeSourceArtifactSha256(kSource)},
               {"rows",
                nlohmann::json::array(
                    {{{"label", "Wall"},
                      {"bgr5558", {0, 31, 992, 31744, 32767, 1, 32, 1024}},
                      {"colors", "ignored, not trusted RGB"}}})}}})}};
  }
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }
  void Write() {
    std::ofstream(root_ / "palettes" / "rom-palettes.json") << document_.dump();
  }
  auto Load() {
    return LoadCatalogPreviewPalettes((root_ / "catalog.json").string());
  }
  static constexpr const char* kSource = "read-only palette source fixture";
  std::filesystem::path root_;
  nlohmann::json document_;
};

TEST_F(CatalogPalettePreviewTest, ValidRelativeSourceDerivesRgbFromWords) {
  Write();
  const auto result = Load();
  ASSERT_TRUE(result.ok()) << result.status();
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->front().label, "Source / Wall");
  EXPECT_EQ(result->front().source_path, (root_ / "source.sfc").string());
  const std::array<std::array<uint8_t, 3>, 8> expected = {{{0, 0, 0},
                                                           {248, 0, 0},
                                                           {0, 248, 0},
                                                           {0, 0, 248},
                                                           {248, 248, 248},
                                                           {8, 0, 0},
                                                           {0, 8, 0},
                                                           {0, 0, 8}}};
  EXPECT_EQ(result->front().colors, expected);
  std::ifstream source(root_ / "source.sfc", std::ios::binary);
  EXPECT_EQ(std::string(std::istreambuf_iterator<char>(source), {}), kSource);
}

// Matches the current exporter's root/source/row keys, including ignored RGB.
TEST_F(CatalogPalettePreviewTest,
       CurrentExporterShapeAcceptsMetadataAndBgr555) {
  document_["errors"] = nlohmann::json::array();
  document_["method"] = "legacy US palette extraction";
  auto& source = document_["sources"][0];
  source["copier_bytes"] = 512;
  auto& row = source["rows"][0];
  row["label"] = "Overworld main 00 / row 0 / seven-color";
  row["bgr555"] = row["bgr5558"];
  row.erase("bgr5558");
  row["offset_pc"] = 911048;
  row["group"] = "Overworld main";
  row["colors"] = nlohmann::json::array();
  for (int i = 0; i < 8; ++i)
    row["colors"].push_back({255, 255, 255});
  // Current pretty export is >1 MiB. Keep the fixture compact in source while
  // exercising that size boundary and the real 3256-row count at load time.
  const auto prototype = row;
  source["rows"] = nlohmann::json::array();
  for (int i = 0; i < 3256; ++i)
    source["rows"].push_back(prototype);
  const auto pretty = document_.dump(2);
  ASSERT_GT(pretty.size(), 1024u * 1024u);
  ASSERT_LT(pretty.size(), 8u * 1024u * 1024u);
  std::ofstream(root_ / "palettes" / "rom-palettes.json") << pretty;
  const auto result = Load();
  ASSERT_TRUE(result.ok()) << result.status();
  ASSERT_EQ(result->size(), 3256u);
  const std::array<uint8_t, 3> red = {248, 0, 0};
  EXPECT_EQ(result->front().colors[1], red);
  EXPECT_EQ(result->back().colors[1], red);
}

TEST_F(CatalogPalettePreviewTest, ConflictingWordFieldsAreRejected) {
  auto& row = document_["sources"][0]["rows"][0];
  row["bgr555"] = row["bgr5558"];
  row["bgr555"][1] = 0;
  Write();
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(CatalogPalettePreviewTest, HashMismatchRejectsAllRows) {
  document_["sources"][0]["sha256"] = std::string(64, '0');
  Write();
  const auto result = Load();
  EXPECT_EQ(result.status().code(), absl::StatusCode::kDataLoss);
}

TEST_F(CatalogPalettePreviewTest, InvalidWordsAreRejectedWithoutTruncation) {
  for (const auto& bad : nlohmann::json::array(
           {-1, 32768, 18446744073709551615ULL, 1.5, true, "31"})) {
    document_["sources"][0]["rows"][0]["bgr5558"][1] = bad;
    Write();
    EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument)
        << bad.dump();
  }
  document_["sources"][0]["rows"][0]["bgr5558"] = {0, 1};
  Write();
  EXPECT_FALSE(Load().ok());
}

TEST_F(CatalogPalettePreviewTest, MissingFileAndEmptySourcesReturnEmpty) {
  auto result = Load();
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result->empty());
  document_["sources"] = nlohmann::json::array();
  Write();
  result = Load();
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result->empty());
}

TEST_F(CatalogPalettePreviewTest, MalformedSchemaAndJsonAreErrors) {
  document_["schema_version"] = 2;
  Write();
  EXPECT_FALSE(Load().ok());
  std::ofstream(root_ / "palettes" / "rom-palettes.json") << "broken";
  EXPECT_FALSE(Load().ok());
}

TEST_F(CatalogPalettePreviewTest, DuplicateSourceStillChecksEveryExpectedHash) {
  auto second = document_["sources"][0];
  second["sha256"] = std::string(64, '0');
  document_["sources"].push_back(second);
  Write();
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kDataLoss);
}

TEST_F(CatalogPalettePreviewTest, ExcessSourcesAndRowsAreRejected) {
  const auto source = document_["sources"][0];
  document_["sources"] = nlohmann::json::array();
  for (int i = 0; i < 65; ++i)
    document_["sources"].push_back(source);
  Write();
  EXPECT_FALSE(Load().ok());
  document_["sources"] = nlohmann::json::array({source});
  const auto row = source["rows"][0];
  for (int i = 0; i < 4096; ++i)
    document_["sources"][0]["rows"].push_back(row);
  Write();
  EXPECT_FALSE(Load().ok());
}

TEST_F(CatalogPalettePreviewTest, OversizedJsonAndDeepJsonAreRejected) {
  std::ofstream(root_ / "palettes" / "rom-palettes.json")
      << std::string(8 * 1024 * 1024 + 1, ' ');
  EXPECT_FALSE(Load().ok());
  std::ofstream(root_ / "palettes" / "rom-palettes.json")
      << std::string(40, '[') << '0' << std::string(40, ']');
  EXPECT_FALSE(Load().ok());
}

TEST_F(CatalogPalettePreviewTest, MissingSourceIsAnError) {
  document_["sources"][0]["path"] = "missing.sfc";
  Write();
  EXPECT_FALSE(Load().ok());
}
TEST(CatalogPalettePreviewIntegrationTest, OptInRealCatalogLoadsPalettes) {
  const char* filename = std::getenv("YAZE_TEST_GRAPHICS_CATALOG");
  if (!filename || !*filename)
    GTEST_SKIP() << "YAZE_TEST_GRAPHICS_CATALOG is unset";
  const auto result = LoadCatalogPreviewPalettes(filename);
  ASSERT_TRUE(result.ok()) << result.status();
  ASSERT_FALSE(result->empty());
  RecordProperty("palette_rows", static_cast<int>(result->size()));
  for (const auto& palette : *result) {
    EXPECT_FALSE(palette.label.empty());
    EXPECT_FALSE(palette.source_path.empty());
    for (const auto& color : palette.colors)
      for (const auto channel : color)
        EXPECT_EQ(channel % 8, 0);
  }
}

}  // namespace
}  // namespace yaze::editor
