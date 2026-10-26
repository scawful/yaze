#include "app/editor/overworld/lyra_terrain_canvas.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include "app/gfx/types/snes_palette.h"
#include "gtest/gtest.h"
#include "zelda3/overworld/lyra_terrain_source.h"

namespace yaze::editor {

class LyraTerrainCanvasTestPeer {
 public:
  static void Install(LyraTerrainCanvas& canvas,
                      std::shared_ptr<core::LyraTerrainDocument> document) {
    canvas.document_ = std::move(document);
  }
  static void BindSourcePath(LyraTerrainCanvas& canvas, std::string path) {
    canvas.source_path_ = std::move(path);
  }
  // The same lookup DrawProperties performs when a Lyra map is selected.
  static core::LyraMapProperties& Draft(LyraTerrainCanvas& canvas,
                                        int map_index) {
    return canvas.PropertyDraftFor(map_index);
  }
  static std::vector<int> PendingMaps(const LyraTerrainCanvas& canvas) {
    return canvas.PendingPropertyMaps();
  }
  static bool Conflict(const LyraTerrainCanvas& canvas, int map_index) {
    return canvas.PropertyConflict(map_index);
  }
  static absl::Status Apply(LyraTerrainCanvas& canvas, int map_index,
                            zelda3::Overworld& overworld,
                            UndoManager& history) {
    return canvas.ApplyPropertyDraft(map_index, overworld, history);
  }
  static void Revert(LyraTerrainCanvas& canvas, int map_index) {
    canvas.RevertPropertyDraft(map_index);
  }
};

namespace {
namespace fs = std::filesystem;

core::LyraTerrainProfile FixtureProfile() {
  core::LyraTerrainProfile profile;
  profile.tile_count = 4096;
  profile.tile16_definition_fingerprint = "definitions:fixture";
  profile.graphics_fingerprint = "graphics:fixture";
  profile.palette_fingerprint = "palette:fixture";
  profile.animation_fingerprint = "animation:fixture";
  return profile;
}

TEST(LyraBackgroundPaletteTest, AppliesOnlyBackgroundToResolvedCopy) {
  std::vector<uint16_t> native_colors(256);
  for (size_t i = 0; i < native_colors.size(); ++i) {
    native_colors[i] = static_cast<uint16_t>(i * 19);
  }
  gfx::SnesPalette native(native_colors);
  native[0].set_transparent(true);
  native[16].set_transparent(true);
  native[127].set_modified(true);
  native[128].set_transparent(true);
  native[255].set_modified(true);
  auto resolved = native;
  core::LyraMapProperties properties;
  std::array<uint16_t, 128> override_colors{};
  for (size_t i = 0; i < override_colors.size(); ++i) {
    override_colors[i] = static_cast<uint16_t>(0x7FFF - i);
  }
  properties.background_palette = override_colors;

  ASSERT_TRUE(
      zelda3::ApplyLyraBackgroundPaletteOverride(properties, resolved).ok());
  ASSERT_EQ(resolved.size(), 256u);
  for (size_t i = 0; i < resolved.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(native[i].snes(), native_colors[i]);
    EXPECT_EQ(resolved[i].snes(),
              i < 128 ? override_colors[i] : native_colors[i]);
    EXPECT_EQ(resolved[i].is_transparent(), native[i].is_transparent());
    EXPECT_EQ(resolved[i].is_modified(), native[i].is_modified());
  }
}

TEST(LyraBackgroundPaletteTest, InvalidLastColorRefusesBeforeAnyMutation) {
  const std::vector<uint16_t> native_colors(256, 0x1234);
  gfx::SnesPalette resolved(native_colors);
  core::LyraMapProperties properties;
  std::array<uint16_t, 128> override_colors{};
  override_colors[0] = 0x001F;
  override_colors[127] = 0x8000;
  properties.background_palette = override_colors;

  const auto status =
      zelda3::ApplyLyraBackgroundPaletteOverride(properties, resolved);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  for (size_t i = 0; i < resolved.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(resolved[i].snes(), native_colors[i]);
  }
}

TEST(LyraBackgroundPaletteTest, IncompleteResolvedPaletteRefusesAtomically) {
  const std::vector<uint16_t> native_colors(128, 0x1234);
  gfx::SnesPalette resolved(native_colors);
  core::LyraMapProperties properties;
  properties.background_palette = std::array<uint16_t, 128>{};

  const auto status =
      zelda3::ApplyLyraBackgroundPaletteOverride(properties, resolved);
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  ASSERT_EQ(resolved.size(), native_colors.size());
  for (size_t i = 0; i < resolved.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(resolved[i].snes(), native_colors[i]);
  }
}

TEST(LyraBackgroundPaletteTest, AbsentOverrideLeavesNativePaletteUntouched) {
  const std::vector<uint16_t> native_colors = {0, 0x001F, 0x03E0, 0x7C00};
  gfx::SnesPalette resolved(native_colors);
  const core::LyraMapProperties native_properties;
  ASSERT_TRUE(
      zelda3::ApplyLyraBackgroundPaletteOverride(native_properties, resolved)
          .ok());
  ASSERT_EQ(resolved.size(), native_colors.size());
  for (size_t i = 0; i < resolved.size(); ++i) {
    EXPECT_EQ(resolved[i].snes(), native_colors[i]);
  }
}

TEST(LyraBackgroundPaletteTest, AssetKeysSeparatePresenceColorsAndGameState) {
  core::LyraMapProperties properties;
  const auto native_key = zelda3::LyraMapAssetKey(properties, 0);
  properties.background_palette = std::array<uint16_t, 128>{};
  const auto zero_key = zelda3::LyraMapAssetKey(properties, 0);
  EXPECT_NE(native_key, zero_key);

  properties.background_palette->back() = 0x7FFF;
  const auto changed_key = zelda3::LyraMapAssetKey(properties, 0);
  EXPECT_NE(zero_key, changed_key);
  EXPECT_NE(changed_key, zelda3::LyraMapAssetKey(properties, 1));
  properties.background_palette.reset();
  EXPECT_EQ(zelda3::LyraMapAssetKey(properties, 0), native_key);
}

void Write(const fs::path& path, const std::string& bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(file.is_open());
  file << bytes;
  ASSERT_TRUE(file.good());
}

std::string Read(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  EXPECT_TRUE(file.is_open());
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

class LyraTerrainSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = fs::temp_directory_path() /
                 ("yaze_lyra_source_" + std::to_string(nonce));
    fs::create_directories(directory_);
    project_.name = "Lyra source fixture";
    project_.filepath = (directory_ / "Oracle.yaze").string();
    project_.rom_filename = (directory_ / "private.sfc").string();
    source_ = directory_ / "Oracle.lyra-terrain.json";
    Write(project_.rom_filename, "immutable ROM fixture");
    ASSERT_TRUE(project_.Save().ok());
    project_before_ = Read(project_.filepath);
    project_.lyra_terrain_file = source_.string();
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(directory_, ec);
  }

  void ExpectProtectedFilesUnchanged() {
    EXPECT_EQ(Read(project_.filepath), project_before_);
    EXPECT_EQ(Read(project_.rom_filename), "immutable ROM fixture");
  }

  fs::path directory_;
  fs::path source_;
  project::YazeProject project_;
  std::string project_before_;
};

TEST_F(LyraTerrainSourceTest, CreatesSidecarReopensAndPreservesUndoAfterSave) {
  auto document = core::LyraTerrainDocument::Create(FixtureProfile(), 8);
  ASSERT_TRUE(document.ok());
  ASSERT_TRUE(document->BeginStroke().ok());
  ASSERT_TRUE(document->Paint(7, 31, 101).ok());
  ASSERT_TRUE(document->Paint(7, 32, 102).ok());
  ASSERT_TRUE(document->Paint(7, 64, 103).ok());
  auto stroke = document->EndStroke();
  ASSERT_TRUE(stroke.ok());
  auto published = PublishLyraTerrainSource(project_, source_.string(),
                                            std::nullopt, *document);
  ASSERT_TRUE(published.ok()) << published.status();
  EXPECT_EQ(Read(source_), *published);
  EXPECT_TRUE(
      document->dirty());  // Publication does not silently clear drafts.
  ExpectProtectedFilesUnchanged();
  ASSERT_TRUE(project_.Save().ok());
  document->MarkSaved();

  project::YazeProject reopened_project;
  ASSERT_TRUE(reopened_project.Open(project_.filepath).ok());
  EXPECT_EQ(reopened_project.lyra_terrain_file, source_.string());
  auto reopened = core::LyraTerrainDocument::Parse(
      Read(reopened_project.lyra_terrain_file), FixtureProfile());
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->maps(), document->maps());
  EXPECT_FALSE(reopened->dirty());

  ASSERT_TRUE(document->ApplyStroke(*stroke, true).ok());
  EXPECT_TRUE(document->dirty());
  auto undone = PublishLyraTerrainSource(project_, source_.string(), *published,
                                         *document);
  ASSERT_TRUE(undone.ok()) << undone.status();
  auto undone_reopened =
      core::LyraTerrainDocument::Parse(*undone, FixtureProfile());
  ASSERT_TRUE(undone_reopened.ok());
  EXPECT_EQ(undone_reopened->MapTile(0xA8, 7, 0).value(), 8);
  ASSERT_TRUE(document->ApplyStroke(*stroke, false).ok());
  EXPECT_FALSE(document->dirty());
}

TEST_F(LyraTerrainSourceTest,
       BackgroundPalettePublishesAndReopensWithoutRomWrite) {
  auto document = core::LyraTerrainDocument::Create(FixtureProfile(), 8);
  ASSERT_TRUE(document.ok());
  const auto baseline = document->authoring();
  auto overridden = baseline;
  std::array<uint16_t, 128> palette{};
  palette[0] = 0x001F;
  palette[127] = 0x7FFF;
  overridden.maps[0].properties.background_palette = palette;
  overridden.maps[0].asset_bindings = {"defs:0", "gfx:0", "palette:0"};
  ASSERT_TRUE(document->SetAuthoring(baseline, overridden).ok());

  auto published = PublishLyraTerrainSource(project_, source_.string(),
                                            std::nullopt, *document);
  ASSERT_TRUE(published.ok()) << published.status();
  ExpectProtectedFilesUnchanged();
  auto reopened =
      core::LyraTerrainDocument::Parse(Read(source_), FixtureProfile());
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->authoring(), overridden);
  EXPECT_FALSE(reopened->dirty());
  document->MarkSaved();
  ASSERT_TRUE(document->SetAuthoring(overridden, baseline).ok());
  EXPECT_TRUE(document->dirty());

  auto undone = PublishLyraTerrainSource(project_, source_.string(), *published,
                                         *document);
  ASSERT_TRUE(undone.ok()) << undone.status();
  auto native = core::LyraTerrainDocument::Parse(*undone, FixtureProfile());
  ASSERT_TRUE(native.ok()) << native.status();
  EXPECT_FALSE(native->authoring().maps[0].properties.background_palette);
  ExpectProtectedFilesUnchanged();
  ASSERT_TRUE(document->SetAuthoring(baseline, overridden).ok());
  EXPECT_FALSE(document->dirty());
}

TEST_F(LyraTerrainSourceTest, RefusesChangedAndDeletedSidecar) {
  auto document = core::LyraTerrainDocument::Create(FixtureProfile());
  ASSERT_TRUE(document.ok());
  auto before = PublishLyraTerrainSource(project_, source_.string(),
                                         std::nullopt, *document);
  ASSERT_TRUE(before.ok());
  ASSERT_TRUE(document->BeginStroke().ok());
  ASSERT_TRUE(document->Paint(1, 32, 17).ok());
  ASSERT_TRUE(document->EndStroke().ok());
  Write(source_, "external edit");
  EXPECT_EQ(
      PublishLyraTerrainSource(project_, source_.string(), *before, *document)
          .status()
          .code(),
      absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(Read(source_), "external edit");
  EXPECT_TRUE(document->dirty());
  fs::remove(source_);
  EXPECT_EQ(
      PublishLyraTerrainSource(project_, source_.string(), *before, *document)
          .status()
          .code(),
      absl::StatusCode::kFailedPrecondition);
  EXPECT_FALSE(fs::exists(source_));
  ExpectProtectedFilesUnchanged();
}

TEST_F(LyraTerrainSourceTest,
       RefusesFileCreatedSinceNewDraftAndMissingProject) {
  auto document = core::LyraTerrainDocument::Create(FixtureProfile());
  ASSERT_TRUE(document.ok());
  Write(source_, "someone else's source");
  EXPECT_FALSE(PublishLyraTerrainSource(project_, source_.string(),
                                        std::nullopt, *document)
                   .ok());
  EXPECT_EQ(Read(source_), "someone else's source");
  ExpectProtectedFilesUnchanged();
  fs::remove(source_);
  fs::remove(project_.filepath);
  EXPECT_FALSE(PublishLyraTerrainSource(project_, source_.string(),
                                        std::nullopt, *document)
                   .ok());
  EXPECT_FALSE(fs::exists(source_));
}

TEST_F(LyraTerrainSourceTest, RefusesChangedReferenceAndRomAlias) {
  auto document = core::LyraTerrainDocument::Create(FixtureProfile());
  ASSERT_TRUE(document.ok());
  project_.lyra_terrain_file = (directory_ / "another.json").string();
  EXPECT_FALSE(PublishLyraTerrainSource(project_, source_.string(),
                                        std::nullopt, *document)
                   .ok());
  project_.lyra_terrain_file = source_.string();
  fs::create_hard_link(project_.rom_filename, source_);
  EXPECT_FALSE(PublishLyraTerrainSource(project_, source_.string(),
                                        Read(source_), *document)
                   .ok());
  ExpectProtectedFilesUnchanged();
}

TEST_F(LyraTerrainSourceTest, EdgeConnectionSurvivesSaveReopenAndUndo) {
  auto document = core::LyraTerrainDocument::Create(FixtureProfile());
  ASSERT_TRUE(document.ok());
  const auto empty = document->authoring();
  auto unassigned = empty;
  unassigned.edge_connections.push_back({1, 0x99, 0xA1, std::nullopt});
  auto assigned = unassigned;
  assigned.edge_connections[0].passage = core::LyraEdgePassage{4, 9};
  ASSERT_TRUE(document->SetAuthoring(empty, unassigned).ok());
  ASSERT_TRUE(document->SetAuthoring(unassigned, assigned).ok());

  auto published = PublishLyraTerrainSource(project_, source_.string(),
                                            std::nullopt, *document);
  ASSERT_TRUE(published.ok()) << published.status();
  EXPECT_TRUE(document->dirty());
  ExpectProtectedFilesUnchanged();
  ASSERT_TRUE(project_.Save().ok());
  document->MarkSaved();

  auto reopened =
      core::LyraTerrainDocument::Parse(Read(source_), FixtureProfile());
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->authoring(), assigned);
  EXPECT_FALSE(reopened->dirty());

  // Undoing the assignment keeps the pair with an undecided passage.
  ASSERT_TRUE(document->SetAuthoring(assigned, unassigned).ok());
  EXPECT_TRUE(document->dirty());
  auto undone = PublishLyraTerrainSource(project_, source_.string(), *published,
                                         *document);
  ASSERT_TRUE(undone.ok()) << undone.status();
  auto undone_reopened =
      core::LyraTerrainDocument::Parse(*undone, FixtureProfile());
  ASSERT_TRUE(undone_reopened.ok()) << undone_reopened.status();
  EXPECT_EQ(undone_reopened->authoring(), unassigned);
  ASSERT_TRUE(document->SetAuthoring(unassigned, assigned).ok());
  EXPECT_FALSE(document->dirty());
}

using Peer = LyraTerrainCanvasTestPeer;

class LyraTerrainOpenTest : public LyraTerrainSourceTest {
 protected:
  void SetUp() override {
    LyraTerrainSourceTest::SetUp();
    auto document = core::LyraTerrainDocument::Create(FixtureProfile());
    ASSERT_TRUE(document.ok()) << document.status();
    document_ =
        std::make_shared<core::LyraTerrainDocument>(*std::move(document));
    Peer::Install(canvas_, document_);
  }

  std::shared_ptr<core::LyraTerrainDocument> document_;
  LyraTerrainCanvas canvas_;
  zelda3::Overworld overworld_{nullptr};
};

TEST_F(LyraTerrainOpenTest,
       InvalidSelectedSourceRetainsCleanCanvasAndReference) {
  const auto document_before = document_->Serialize();
  const auto reference_before = project_.lyra_terrain_file;
  const auto status = canvas_.Open(
      &project_, (directory_ / "invalid.txt").string(), overworld_);

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(canvas_.document(), document_.get());
  EXPECT_EQ(canvas_.document()->Serialize(), document_before);
  EXPECT_EQ(project_.lyra_terrain_file, reference_before);
  EXPECT_FALSE(canvas_.dirty());
  ExpectProtectedFilesUnchanged();
}

TEST_F(LyraTerrainOpenTest,
       NativeBindingFailureRetainsCleanCanvasAndReference) {
  Write(directory_ / "another.json", document_->Serialize());
  const auto reference_before = project_.lyra_terrain_file;
  const auto status = canvas_.Open(
      &project_, (directory_ / "another.json").string(), overworld_);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(status.message(), "Open the main project ROM to draw Lyra terrain");
  EXPECT_EQ(canvas_.document(), document_.get());
  EXPECT_EQ(project_.lyra_terrain_file, reference_before);
  EXPECT_FALSE(canvas_.dirty());
  ExpectProtectedFilesUnchanged();
}

TEST_F(LyraTerrainOpenTest, PendingPropertiesRefuseReplacementBeforeLoading) {
  Peer::Draft(canvas_, 0).main_palette = 5;
  Peer::Draft(canvas_, 1).message_id = 7;
  const auto document_before = document_->Serialize();
  const auto reference_before = project_.lyra_terrain_file;
  const auto status = canvas_.Open(
      &project_, (directory_ / "replacement.json").string(), overworld_);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(status.message(),
            "Save the current Lyra terrain draft before replacing it");
  EXPECT_EQ(canvas_.document(), document_.get());
  EXPECT_EQ(canvas_.document()->Serialize(), document_before);
  EXPECT_EQ(project_.lyra_terrain_file, reference_before);
  EXPECT_EQ(Peer::PendingMaps(canvas_), std::vector<int>({0xA0, 0xA1}));
  EXPECT_EQ(Peer::Draft(canvas_, 0).main_palette, 5);
  EXPECT_EQ(Peer::Draft(canvas_, 1).message_id, 7);
  EXPECT_TRUE(canvas_.dirty());
  ExpectProtectedFilesUnchanged();
}

TEST_F(LyraTerrainOpenTest, PaintedDraftRefusesReplacementBeforeLoading) {
  ASSERT_TRUE(document_->BeginStroke().ok());
  ASSERT_TRUE(document_->Paint(2, 3, 17).ok());
  ASSERT_TRUE(document_->EndStroke().ok());
  const auto document_before = document_->Serialize();
  const auto reference_before = project_.lyra_terrain_file;
  const auto status = canvas_.Open(
      &project_, (directory_ / "replacement.json").string(), overworld_);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(status.message(),
            "Save the current Lyra terrain draft before replacing it");
  EXPECT_EQ(canvas_.document(), document_.get());
  EXPECT_EQ(canvas_.document()->Serialize(), document_before);
  EXPECT_EQ(project_.lyra_terrain_file, reference_before);
  EXPECT_TRUE(canvas_.dirty());
  ExpectProtectedFilesUnchanged();
}

TEST_F(LyraTerrainOpenTest, MissingProjectAndEmptySelectionRetainCanvas) {
  const auto reference_before = project_.lyra_terrain_file;
  EXPECT_EQ(canvas_.Open(nullptr, source_.string(), overworld_).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(canvas_.Open(&project_, "", overworld_).code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(canvas_.document(), document_.get());
  EXPECT_EQ(project_.lyra_terrain_file, reference_before);
  EXPECT_FALSE(canvas_.dirty());
  ExpectProtectedFilesUnchanged();
}

TEST_F(LyraTerrainOpenTest, SameSourceCreateRetainsCleanDocument) {
  Peer::BindSourcePath(canvas_, project_.lyra_terrain_file);
  const auto reference_before = project_.lyra_terrain_file;
  const auto status = canvas_.Create(&project_, overworld_);

  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(canvas_.document(), document_.get());
  EXPECT_EQ(project_.lyra_terrain_file, reference_before);
  EXPECT_FALSE(canvas_.dirty());
  ExpectProtectedFilesUnchanged();
}

TEST_F(LyraTerrainOpenTest, FailedFreshCreateDoesNotPublishProjectReference) {
  LyraTerrainCanvas fresh;
  project_.lyra_terrain_file.clear();
  const auto status = fresh.Create(&project_, overworld_);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_TRUE(project_.lyra_terrain_file.empty());
  EXPECT_FALSE(fresh.loaded());
  EXPECT_FALSE(fresh.dirty());
  EXPECT_FALSE(fs::exists(source_));
  ExpectProtectedFilesUnchanged();
}

TEST_F(LyraTerrainSourceTest,
       AutomaticLoadRequiresOracleManifestAndNativeAssets) {
  LyraTerrainCanvas canvas;
  Rom unloaded_rom;
  zelda3::Overworld overworld(&unloaded_rom);
  project_.lyra_terrain_file.clear();
  project_.name = "Oracle of Secrets";
  EXPECT_TRUE(canvas.Load(&project_, overworld).ok());
  EXPECT_TRUE(project_.lyra_terrain_file.empty());
  EXPECT_FALSE(canvas.loaded());
  ASSERT_TRUE(
      project_.hack_manifest
          .LoadFromString(
              R"json({"manifest_version":2,"hack_name":"Oracle of Secrets"})json")
          .ok());
  EXPECT_EQ(canvas.Load(&project_, overworld).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_TRUE(project_.lyra_terrain_file.empty());
  EXPECT_FALSE(canvas.loaded());
  EXPECT_FALSE(canvas.dirty());
  ExpectProtectedFilesUnchanged();
}

class LyraPropertyDraftTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto document = core::LyraTerrainDocument::Create(FixtureProfile());
    ASSERT_TRUE(document.ok()) << document.status();
    document_ =
        std::make_shared<core::LyraTerrainDocument>(*std::move(document));
    Peer::Install(canvas_, document_);
  }

  // Stands in for Undo/Redo or another authoring edit changing live values.
  void SetLive(int map_index, const core::LyraMapProperties& properties) {
    auto after = document_->authoring();
    after.maps[map_index].properties = properties;
    after.maps[map_index].asset_bindings = {"binding:0", "binding:1",
                                            "binding:2"};
    ASSERT_TRUE(document_->SetAuthoring(document_->authoring(), after).ok());
  }

  const core::LyraMapProperties& Live(int map_index) const {
    return document_->authoring().maps[map_index].properties;
  }

  std::shared_ptr<core::LyraTerrainDocument> document_;
  LyraTerrainCanvas canvas_;
  zelda3::Overworld overworld_{nullptr};
  UndoManager history_;
};

TEST_F(LyraPropertyDraftTest, MapNavigationRetainsEachPendingDraft) {
  Peer::Draft(canvas_, 0).main_palette = 5;
  Peer::Draft(canvas_, 1);
  EXPECT_EQ(Peer::Draft(canvas_, 0).main_palette, 5);
  EXPECT_EQ(Peer::PendingMaps(canvas_), std::vector<int>({0xA0}));
  EXPECT_FALSE(document_->dirty());
  EXPECT_TRUE(canvas_.dirty());

  Peer::Draft(canvas_, 1).message_id = 7;
  EXPECT_EQ(Peer::PendingMaps(canvas_), std::vector<int>({0xA0, 0xA1}));
  Peer::Revert(canvas_, 1);
  EXPECT_EQ(Peer::PendingMaps(canvas_), std::vector<int>({0xA0}));
  EXPECT_EQ(Peer::Draft(canvas_, 0).main_palette, 5);
  EXPECT_EQ(Live(0).main_palette, 0);
  Peer::Revert(canvas_, 0);
  EXPECT_FALSE(canvas_.dirty());
}

TEST_F(LyraPropertyDraftTest, BackgroundPaletteDraftIsPerMapAndRevertUsesLive) {
  const auto native = Live(0);
  std::array<uint16_t, 128> palette{};
  palette[0] = 0x001F;
  palette[127] = 0x7FFF;
  Peer::Draft(canvas_, 0).background_palette = palette;

  EXPECT_EQ(Peer::Draft(canvas_, 0).background_palette, palette);
  EXPECT_FALSE(Peer::Draft(canvas_, 1).background_palette.has_value());
  EXPECT_EQ(Live(0), native);
  EXPECT_FALSE(document_->dirty());
  EXPECT_EQ(Peer::PendingMaps(canvas_), std::vector<int>({0xA0}));
  EXPECT_TRUE(canvas_.dirty());

  Peer::Revert(canvas_, 0);
  EXPECT_EQ(Peer::Draft(canvas_, 0), native);
  EXPECT_TRUE(Peer::PendingMaps(canvas_).empty());
  EXPECT_FALSE(canvas_.dirty());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(LyraPropertyDraftTest,
       NativePaletteResetIsReversibleWithoutGlobalEdits) {
  auto live = Live(0);
  std::array<uint16_t, 128> palette{};
  palette[16] = 0x03E0;
  live.background_palette = palette;
  live.main_palette = 5;
  live.palette_set = 14;
  SetLive(0, live);
  document_->MarkSaved();
  ASSERT_EQ(Peer::Draft(canvas_, 0), live);

  auto& draft = Peer::Draft(canvas_, 0);
  draft.background_palette.reset();
  EXPECT_EQ(draft.main_palette, 5);
  EXPECT_EQ(draft.palette_set, 14);
  EXPECT_EQ(Live(0), live);
  EXPECT_FALSE(document_->dirty());
  EXPECT_TRUE(canvas_.dirty());

  Peer::Revert(canvas_, 0);
  EXPECT_EQ(Peer::Draft(canvas_, 0).background_palette, palette);
  EXPECT_EQ(Live(0), live);
  EXPECT_FALSE(canvas_.dirty());
  EXPECT_EQ(history_.UndoStackSize(), 0u);
}

TEST_F(LyraPropertyDraftTest, BackgroundPaletteLiveChangeRefusesStaleDraft) {
  std::array<uint16_t, 128> draft_palette{};
  draft_palette[0] = 0x001F;
  Peer::Draft(canvas_, 0).background_palette = draft_palette;
  auto newer = Live(0);
  std::array<uint16_t, 128> live_palette{};
  live_palette[0] = 0x7C00;
  newer.background_palette = live_palette;
  SetLive(0, newer);
  const auto before = document_->authoring();
  ASSERT_TRUE(Peer::Conflict(canvas_, 0));

  const auto status = Peer::Apply(canvas_, 0, overworld_, history_);
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(status.message().find("Revert Properties"), std::string::npos);
  EXPECT_EQ(document_->authoring(), before);
  EXPECT_EQ(Peer::Draft(canvas_, 0).background_palette, draft_palette);
  EXPECT_EQ(history_.UndoStackSize(), 0u);
  Peer::Revert(canvas_, 0);
  EXPECT_EQ(Peer::Draft(canvas_, 0).background_palette, live_palette);
}

TEST_F(LyraPropertyDraftTest, CleanDraftFollowsLiveIncludingUndo) {
  const auto original = Live(0);
  EXPECT_EQ(Peer::Draft(canvas_, 0), original);
  Peer::Draft(canvas_, 2);
  auto changed = original;
  changed.palette_set = 3;
  SetLive(0, changed);
  SetLive(2, changed);
  EXPECT_EQ(Peer::Draft(canvas_, 0), changed);
  SetLive(0, original);
  EXPECT_EQ(Peer::Draft(canvas_, 0), original);
  // Map $A2 was not revisited; its stale clean entry is not pending.
  EXPECT_TRUE(Peer::PendingMaps(canvas_).empty());
  EXPECT_EQ(Peer::Draft(canvas_, 2), changed);
}

TEST_F(LyraPropertyDraftTest, ConflictRefusesApplyAndRetainsEverything) {
  Peer::Draft(canvas_, 0).main_palette = 5;
  auto newer = Live(0);
  newer.music[0] = 9;
  SetLive(0, newer);
  const auto authoring_before = document_->authoring();
  ASSERT_TRUE(Peer::Conflict(canvas_, 0));

  const auto status = Peer::Apply(canvas_, 0, overworld_, history_);
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(status.message().find("Revert Properties"), std::string::npos);
  EXPECT_EQ(document_->authoring(), authoring_before);
  EXPECT_EQ(history_.UndoStackSize(), 0u);
  EXPECT_EQ(Peer::Draft(canvas_, 0).main_palette, 5);
  EXPECT_EQ(Peer::Draft(canvas_, 0).music[0], 0);
  EXPECT_EQ(Peer::PendingMaps(canvas_), std::vector<int>({0xA0}));

  Peer::Revert(canvas_, 0);
  EXPECT_EQ(Peer::Draft(canvas_, 0), newer);
  EXPECT_TRUE(Peer::PendingMaps(canvas_).empty());
}

TEST_F(LyraPropertyDraftTest, PendingDraftEqualToNewLiveIsClean) {
  const auto original = Live(0);
  auto& draft = Peer::Draft(canvas_, 0);
  draft.main_palette = 5;
  const auto edited = draft;
  SetLive(0, edited);
  EXPECT_TRUE(Peer::PendingMaps(canvas_).empty());
  EXPECT_FALSE(Peer::Conflict(canvas_, 0));
  EXPECT_EQ(Peer::Draft(canvas_, 0), edited);
  SetLive(0, original);
  EXPECT_EQ(Peer::Draft(canvas_, 0), original);
}

TEST_F(LyraPropertyDraftTest, SaveRefusesPendingBeforeProjectAccess) {
  Peer::Draft(canvas_, 0).main_palette = 5;
  Peer::Draft(canvas_, 3).message_id = 1;
  const auto status = canvas_.Save(nullptr, overworld_);
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(status.message(),
            "Apply or Revert Lyra properties for maps $A0, $A3 before saving "
            "the project.");
  canvas_.ProjectSaveCommitted();
  EXPECT_TRUE(canvas_.dirty());
  EXPECT_EQ(Peer::PendingMaps(canvas_), std::vector<int>({0xA0, 0xA3}));
  EXPECT_FALSE(document_->dirty());
}

TEST_F(LyraPropertyDraftTest, ResetClearsEveryDraft) {
  Peer::Draft(canvas_, 0).main_palette = 5;
  Peer::Draft(canvas_, 5).message_id = 2;
  canvas_.Reset();
  EXPECT_FALSE(canvas_.dirty());
  EXPECT_TRUE(Peer::PendingMaps(canvas_).empty());
  Peer::Install(canvas_, document_);
  EXPECT_TRUE(Peer::PendingMaps(canvas_).empty());
  EXPECT_EQ(Peer::Draft(canvas_, 0), Live(0));
  EXPECT_FALSE(canvas_.dirty());
}

TEST_F(LyraPropertyDraftTest, EdgeConnectionEditsLeavePropertyDraftsIntact) {
  Peer::Draft(canvas_, 0).main_palette = 5;
  const auto before = document_->authoring();
  auto connected = before;
  connected.edge_connections.push_back({1, 0x98, 0xA0, std::nullopt});
  ASSERT_TRUE(document_->SetAuthoring(before, connected).ok());

  EXPECT_FALSE(Peer::Conflict(canvas_, 0));
  EXPECT_EQ(Peer::PendingMaps(canvas_), std::vector<int>({0xA0}));
  EXPECT_EQ(Peer::Draft(canvas_, 0).main_palette, 5);
  Peer::Revert(canvas_, 0);
  EXPECT_TRUE(canvas_.dirty());

  ASSERT_TRUE(document_->SetAuthoring(connected, before).ok());
  EXPECT_FALSE(canvas_.dirty());
}

}  // namespace
}  // namespace yaze::editor
