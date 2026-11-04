#include "core/gfx_sheet_source.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/gfx/types/snes_tile.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "util/indexed_png.h"

namespace yaze::core {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
void Write(const fs::path& path, const std::string& text) {
  std::ofstream output(path, std::ios::binary);
  output << text;
  ASSERT_TRUE(output.good());
}
std::string Read(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
class GfxSheetSourceTest : public testing::Test {
 protected:
  void SetUp() override {
    root = fs::temp_directory_path() /
           ("yaze-source-sheets-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "Data/graphics");
    project.filepath = (root / "source.yaze").string();
    project.code_folder = root.string();
    project.name = "Explicit source fixture";
    project.graphics_sheets.source_spec =
        "Data/graphics/source_sprite_sheets.json";
    pixels.assign(4096, 0);
    pixels[0] = 2;
    pixels[136] = 5;
    spec = {{"schema_version", 2},
            {"start", "0x40D000"},
            {"capacity", "0x3000"},
            {"encoding", "lz2"},
            {"sheets", Json::array()}};
    for (int id : {0xDF, 0xFE}) {
      const auto name = std::to_string(id) + ".txt";
      Write(root / "Data/graphics" / name, GfxSheetSource::EncodeText(pixels));
      spec["sheets"].push_back(
          {{"global_sheet", id},
           {"source", name},
           {"decoded_sha256", GfxSheetSource::DecodedSha256(pixels)},
           {"why", "Existing declared source fixture"}});
    }
    for (int id : {0x64, 0x40, 0x43, 0x47}) {
      const auto name =
          "legacy-" + std::to_string(id) + (id == 0x64 ? ".png" : ".txt");
      if (id == 0x64) {
        auto png = util::EncodeIndexedPng(
            128, 32, pixels,
            std::vector<std::array<uint8_t, 4>>(8, {0, 0, 0, 255}));
        ASSERT_TRUE(png.ok());
        ASSERT_TRUE(util::WriteBinaryFile(
                        (root / "Data/graphics" / name).string(), *png)
                        .ok());
      } else
        Write(root / "Data/graphics" / name,
              GfxSheetSource::EncodeText(pixels));
      spec["sheets"].push_back({{"sprite_sheet", id},
                                {"source", name},
                                {"base_sha256", std::string(64, 'a')},
                                {"identity", false},
                                {"why", "Unchanged legacy source fixture"}});
    }
    Bind();
  }
  void Bind() {
    Write(Spec(), spec.dump(2) + "\n");
    project.graphics_sheets.source_spec_sha1 =
        GfxSheetSource::SpecSha1(Read(Spec()));
    ASSERT_TRUE(project.Save().ok());
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root, ec);
  }
  fs::path Spec() const {
    return root / "Data/graphics/source_sprite_sheets.json";
  }
  fs::path Sheet(int id = 0xDF) const {
    return root / "Data/graphics" / (std::to_string(id) + ".txt");
  }
  fs::path root;
  project::YazeProject project;
  Json spec;
  std::vector<uint8_t> pixels;
};
TEST_F(GfxSheetSourceTest, TextRoundTripMatchesPlanarHashAndTileOrder) {
  const auto decoded =
      GfxSheetSource::DecodeText(GfxSheetSource::EncodeText(pixels));
  ASSERT_TRUE(decoded.ok());
  EXPECT_EQ(*decoded, pixels);
  const auto raw = gfx::IndexedToSnesSheet(*decoded, 3);
  ASSERT_EQ(raw.size(), 0x600u);
  EXPECT_EQ(raw[0], 0);
  EXPECT_EQ(raw[1], 0x80);
  EXPECT_EQ(gfx::SnesTo8bppSheet(raw, 3), pixels);
  EXPECT_FALSE(GfxSheetSource::DecodeText(std::string(32 * 128, '8')).ok());
}
TEST_F(GfxSheetSourceTest, CurrentSpecAndDecodedHashFailIndependently) {
  ASSERT_TRUE(GfxSheetSource::Load(project).ok());
  Write(Spec(), Read(Spec()) + " ");
  EXPECT_FALSE(GfxSheetSource::Load(project).ok());
  Bind();
  Write(Sheet(), GfxSheetSource::EncodeText(std::vector<uint8_t>(4096, 7)));
  EXPECT_FALSE(GfxSheetSource::Load(project).ok());
}
TEST_F(GfxSheetSourceTest,
       RejectsSentinelDuplicateTraversalAliasAndPngBeforeMutation) {
  const auto original = spec;
  const auto descriptor = Read(project.filepath);
  for (int variant = 0; variant < 6; ++variant) {
    spec = original;
    if (variant == 0)
      spec["sheets"][0]["global_sheet"] = 0xFF;
    if (variant == 1)
      spec["sheets"][1]["global_sheet"] = 0xDF;
    if (variant == 2)
      spec["sheets"][0]["source"] = "../223.txt";
    if (variant == 3)
      spec["sheets"][1]["source"] = "223.txt";
    if (variant == 4) {
      fs::create_hard_link(Sheet(), root / "Data/graphics/hard.txt");
      spec["sheets"][1]["source"] = "hard.txt";
    }
    if (variant == 5) {
      Write(root / "Data/graphics/source.png", Read(Sheet()));
      spec["sheets"][0]["source"] = "source.png";
    }
    Bind();
    const auto before = Read(Sheet());
    EXPECT_FALSE(GfxSheetSource::Load(project).ok()) << variant;
    EXPECT_EQ(Read(Sheet()), before);
  }
  EXPECT_FALSE(descriptor.empty());
}
TEST_F(GfxSheetSourceTest,
       SourceSymlinkEscapeAndPostLoadRedirectionAreRefused) {
  Write(root / "outside.txt", Read(Sheet()));
  fs::remove(Sheet());
  fs::create_symlink(root / "outside.txt", Sheet());
  EXPECT_FALSE(GfxSheetSource::Load(project).ok());
  fs::remove(Sheet());
  Write(Sheet(), GfxSheetSource::EncodeText(pixels));
  auto source = GfxSheetSource::Load(project);
  ASSERT_TRUE(source.ok());
  fs::remove(Sheet());
  fs::create_symlink(root / "outside.txt", Sheet());
  EXPECT_FALSE(source->Prepare(project, {{0xDF, pixels}}).ok());
}
TEST_F(GfxSheetSourceTest,
       DomainReadbackFailureRollsBackPayloadsSpecAndDescriptor) {
  auto source = GfxSheetSource::Load(project);
  ASSERT_TRUE(source.ok());
  auto edited = pixels;
  edited[0] = 6;
  auto plan = source->Prepare(project, {{0xDF, edited}, {0xFE, edited}});
  ASSERT_TRUE(plan.ok());
  const auto before_spec = Read(Spec()), before_df = Read(Sheet()),
             before_fe = Read(Sheet(0xFE));
  const auto before_descriptor = Read(project.filepath);
  auto candidate = project;
  candidate.graphics_sheets.source_spec_sha1 = plan->sha1;
  auto saved = candidate.SaveWithSourceArtifacts(
      plan->updates,
      [&]() {
        EXPECT_NE(Read(Spec()), before_spec);
        EXPECT_NE(Read(Sheet()), before_df);
        EXPECT_NE(Read(Sheet(0xFE)), before_fe);
        EXPECT_NE(Read(project.filepath), before_descriptor);
        return absl::DataLossError("domain readback failure injected");
      },
      plan->descriptor);
  EXPECT_FALSE(saved.ok());
  EXPECT_EQ(Read(Spec()), before_spec);
  EXPECT_EQ(Read(Sheet()), before_df);
  EXPECT_EQ(Read(Sheet(0xFE)), before_fe);
  EXPECT_EQ(Read(project.filepath), before_descriptor);
  EXPECT_TRUE(source->CheckDisk().ok());
  EXPECT_EQ(project.graphics_sheets.source_spec_sha1, source->spec_sha1());
}
TEST_F(GfxSheetSourceTest,
       DescriptorAndExternalPayloadCasFailurePublishNothing) {
  auto source = GfxSheetSource::Load(project);
  ASSERT_TRUE(source.ok());
  auto edited = pixels;
  edited[0] = 3;
  auto plan = source->Prepare(project, {{0xDF, edited}});
  ASSERT_TRUE(plan.ok());
  const auto before = Read(Spec()), df = Read(Sheet());
  Write(project.filepath, Read(project.filepath) + "# external descriptor\n");
  auto candidate = project;
  candidate.graphics_sheets.source_spec_sha1 = plan->sha1;
  auto saved =
      candidate.SaveWithSourceArtifacts(plan->updates, {}, plan->descriptor);
  EXPECT_FALSE(saved.ok());
  EXPECT_EQ(Read(Spec()), before);
  EXPECT_EQ(Read(Sheet()), df);
  Write(Sheet(), "# same pixels, different exact bytes\n" + df);
  EXPECT_FALSE(source->Prepare(project, {{0xDF, edited}}).ok());
  EXPECT_EQ(Read(Spec()), before);
}
TEST_F(GfxSheetSourceTest, AtomicSourceSaveReopenAndUndoUseNewCurrentBaseline) {
  auto source = GfxSheetSource::Load(project);
  ASSERT_TRUE(source.ok());
  const auto legacy_png = Read(root / "Data/graphics/legacy-100.png");
  const auto original_entries = spec["sheets"];
  auto edited = pixels;
  edited[0] = 7;
  for (const auto& desired : {edited, pixels}) {
    auto plan = source->Prepare(project, {{0xDF, desired}});
    ASSERT_TRUE(plan.ok());
    auto candidate = project;
    candidate.graphics_sheets.source_spec_sha1 = plan->sha1;
    auto saved = candidate.SaveWithSourceArtifacts(
        plan->updates,
        [&]() { return GfxSheetSource::Load(candidate).status(); },
        plan->descriptor);
    ASSERT_TRUE(saved.ok()) << saved.status();
    project = std::move(candidate);
    source->Commit(std::move(*plan), *saved);
    project::YazeProject reopened;
    ASSERT_TRUE(reopened.Open(project.filepath).ok());
    EXPECT_EQ(reopened.graphics_sheets.source_spec,
              "Data/graphics/source_sprite_sheets.json");
    auto fresh = GfxSheetSource::Load(reopened);
    ASSERT_TRUE(fresh.ok()) << fresh.status();
    EXPECT_EQ(fresh->sheets().at(0xDF).pixels, desired);
    EXPECT_EQ(fresh->sheets().at(0xFE).pixels, pixels);
    EXPECT_EQ(Read(root / "Data/graphics/legacy-100.png"), legacy_png);
    const auto saved_spec = Json::parse(Read(Spec()));
    for (int index = 2; index < 6; ++index)
      EXPECT_EQ(saved_spec["sheets"][index], original_entries[index]);
  }
}
TEST_F(GfxSheetSourceTest, MixedUnboundSheetAndInvalidIndicesRefuseWholePlan) {
  auto source = GfxSheetSource::Load(project);
  ASSERT_TRUE(source.ok());
  const auto before = Read(Spec());
  auto invalid = pixels;
  invalid[0] = 8;
  EXPECT_FALSE(source->Prepare(project, {{0xDF, pixels}, {0x20, pixels}}).ok());
  EXPECT_FALSE(
      source->Prepare(project, {{0xDF, pixels}, {0xFE, invalid}}).ok());
  EXPECT_EQ(Read(Spec()), before);
}
TEST_F(GfxSheetSourceTest,
       CoreSaveAsCannotCreateSecondDescriptorSharingSources) {
  const auto before = Read(project.filepath), spec_before = Read(Spec());
  const auto destination = root / "second-owner.yaze";
  EXPECT_FALSE(project.SaveAs(destination.string()).ok());
  EXPECT_FALSE(fs::exists(destination));
  EXPECT_EQ(Read(project.filepath), before);
  EXPECT_EQ(Read(Spec()), spec_before);
}
TEST_F(GfxSheetSourceTest,
       SourceOwnershipKeepsProjectProtectionAndRomReservationsSeparate) {
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json({"manifest_version":3,
    "graphics_sheet_regions":{"reserved_sheets":["0xDF","0xFE"]},
    "source_sprite_sheets":{"schema_version":1,"spec":"Data/graphics/source_sprite_sheets.json",
      "global_sheet_ids":["0xDF","0xFE"]}})json")
                  .ok());
  auto source = GfxSheetSource::Load(project);
  ASSERT_TRUE(source.ok());
  auto edited = pixels;
  edited[0] = 6;
  EXPECT_TRUE(source->Prepare(project, {{0xDF, edited}}).ok());
  auto rom_policy = BuildGfxSheetWritePolicy(project);
  ASSERT_TRUE(rom_policy.ok());
  EXPECT_EQ(rom_policy->reserved_sheets.count(0xDF), 1u);
  EXPECT_EQ(rom_policy->reserved_sheets.count(0xFE), 1u);
  project.graphics_sheets.reserved_sheets = {0xDF};
  EXPECT_FALSE(source->Prepare(project, {{0xDF, edited}}).ok());
  project.graphics_sheets.reserved_sheets.clear();
  project.graphics_sheets.reserved_blocks[0xDF] = {0};
  EXPECT_FALSE(source->Prepare(project, {{0xDF, edited}}).ok());
  edited = pixels;
  edited[32] = 6;
  EXPECT_TRUE(source->Prepare(project, {{0xDF, edited}}).ok());
}

TEST_F(GfxSheetSourceTest, IntegerAndSchemaFieldsMatchPythonWithoutNarrowing) {
  const auto original = spec;
  for (const auto& value : std::vector<Json>{
           223, "223", "+223", "0xDF", "0o337", "0b1101_1111", "2_23", "0x_DF",
           "  +0xDF\t", "\xD9\xA2\xD9\xA2\xD9\xA3", "\xEF\xBC\x90xDF"}) {
    spec = original;
    spec["sheets"][0]["global_sheet"] = value;
    Bind();
    auto source = GfxSheetSource::Load(project);
    ASSERT_TRUE(source.ok()) << value << ": " << source.status();
    EXPECT_EQ(source->sheets().at(0xDF).pixels, pixels);
  }
  for (const auto& value : std::vector<Json>{
           true, 223.0, "0337", "0_337", "0x__DF", "22__3", "223_", "0x", "",
           Json(uint64_t{4294967519ULL}), Json(int64_t{-4294967073LL}),
           Json(uint64_t{18446744073709551615ULL})}) {
    spec = original;
    spec["sheets"][0]["global_sheet"] = value;
    Bind();
    const auto before_source = Read(Sheet()), before_spec = Read(Spec()),
               before_descriptor = Read(project.filepath);
    EXPECT_FALSE(GfxSheetSource::Load(project).ok()) << value;
    EXPECT_EQ(Read(Sheet()), before_source);
    EXPECT_EQ(Read(Spec()), before_spec);
    EXPECT_EQ(Read(project.filepath), before_descriptor);
  }
  for (const auto& version : std::vector<Json>{2.0, true, "2"}) {
    spec = original;
    spec["schema_version"] = version;
    Bind();
    EXPECT_FALSE(GfxSheetSource::Load(project).ok()) << version;
  }
  // This value formerly narrowed to legacy sprite $0C before adding $73.
  spec = original;
  spec["sheets"][2]["sprite_sheet"] = uint64_t{4294967308ULL};
  Bind();
  EXPECT_FALSE(GfxSheetSource::Load(project).ok());
}

TEST_F(GfxSheetSourceTest, TextRowsMatchPythonSplitlinesAndColumnZeroComments) {
  const std::string row(128, '.');
  for (const std::string separator :
       {"\n", "\r\n", "\r", "\v", "\f", "\x1C", "\x1D", "\x1E", "\xC2\x85",
        "\xE2\x80\xA8", "\xE2\x80\xA9"}) {
    std::string text = "# source credit" + separator + separator;
    for (int y = 0; y < 32; ++y)
      text += row + separator;
    auto decoded = GfxSheetSource::DecodeText(text);
    ASSERT_TRUE(decoded.ok()) << separator << ": " << decoded.status();
    EXPECT_EQ(*decoded, std::vector<uint8_t>(4096, 0));
  }
  const auto plain = GfxSheetSource::EncodeText(pixels);
  for (const auto& prefix :
       {" \t\n", " # indented comment\n", "\t# indented comment\n",
        "# invalid UTF8: \xFF\n"})
    EXPECT_FALSE(GfxSheetSource::DecodeText(std::string(prefix) + plain).ok());
  EXPECT_TRUE(
      GfxSheetSource::DecodeText("# column zero comment\n\n" + plain).ok());
}

TEST_F(GfxSheetSourceTest,
       SourceEditPreservesCommentsBlankLinesAndCrLfTemplate) {
  std::string before = "# source and credit notes\r\n\r\n";
  const auto canonical = GfxSheetSource::EncodeText(pixels);
  std::vector<size_t> positions;
  for (int row = 0; row < 32; ++row) {
    if (row == 7)
      before += "# keep this middle note\r\n\r\n";
    auto line = canonical.substr(row * 129, 128);
    if (row == 0)
      line[10] = '0';  // Preserve unchanged zero spelling too.
    for (int x = 0; x < 128; ++x)
      positions.push_back(before.size() + x);
    before += line + "\r\n";
  }
  Write(Sheet(), before);
  Bind();
  auto source = GfxSheetSource::Load(project);
  ASSERT_TRUE(source.ok());
  auto edited = pixels;
  edited[0] = 6;
  edited[136] = 0;
  auto plan = source->Prepare(project, {{0xDF, edited}});
  ASSERT_TRUE(plan.ok());
  auto expected = before;
  expected[positions[0]] = '6';
  expected[positions[136]] = '.';
  EXPECT_EQ(plan->sheets.at(0xDF).bytes, expected);
  auto candidate = project;
  candidate.graphics_sheets.source_spec_sha1 = plan->sha1;
  auto saved = candidate.SaveWithSourceArtifacts(
      plan->updates, [&]() { return GfxSheetSource::Load(candidate).status(); },
      plan->descriptor);
  ASSERT_TRUE(saved.ok()) << saved.status();
  EXPECT_EQ(Read(Sheet()), expected);
  auto decoded = GfxSheetSource::DecodeText(Read(Sheet()));
  ASSERT_TRUE(decoded.ok());
  EXPECT_EQ(*decoded, edited);
  EXPECT_EQ(Json::parse(Read(Spec()))["sheets"][0]["decoded_sha256"],
            GfxSheetSource::DecodedSha256(edited));
  EXPECT_EQ(Read(Sheet())[positions[10]], '0');
}

}  // namespace
}  // namespace yaze::core
