#include "app/editor/overworld/lyra_terrain_canvas.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "app/editor/core/undo_action.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "core/features.h"
#include "core/rom_settings.h"
#include "core/source_artifact_publisher.h"
#include "fresh_rom_fixture_output.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "testing.h"
#include "unique_temp_path.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/lyra_terrain_source.h"
#include "zelda3/overworld/overworld.h"

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

class NoOpAction final : public UndoAction {
 public:
  absl::Status Undo() override { return absl::OkStatus(); }
  absl::Status Redo() override { return absl::OkStatus(); }
  std::string Description() const override { return "No-op"; }
};

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

TEST_F(LyraTerrainSourceTest, SourceRecheckRollsBackSaveAndSaveAs) {
  auto document = core::LyraTerrainDocument::Create(FixtureProfile());
  ASSERT_TRUE(document.ok()) << document.status();
  auto before = PublishLyraTerrainSource(project_, source_.string(),
                                         std::nullopt, *document);
  ASSERT_TRUE(before.ok()) << before.status();
  ASSERT_TRUE(project_.Save().ok());
  const auto descriptor_before = Read(project_.filepath);
  const auto original_path = project_.filepath;
  ASSERT_TRUE(document->BeginStroke().ok());
  ASSERT_TRUE(document->Paint(3, 4, 17).ok());
  ASSERT_TRUE(document->EndStroke().ok());
  int rechecks = 0;
  const auto reject_drift = [&]() -> absl::Status {
    ++rechecks;
    // Simulate an external source writer while outputs are still protected
    // by rollback copies. The publisher must not overwrite that input.
    Write(project_.rom_filename, "external source revision");
    return absl::FailedPreconditionError("Native source receipt changed");
  };
  EXPECT_EQ(PublishLyraTerrainSource(project_, source_.string(), *before,
                                     *document, reject_drift)
                .status()
                .code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(Read(source_), *before);
  EXPECT_EQ(Read(project_.filepath), descriptor_before);
  EXPECT_EQ(Read(project_.rom_filename), "external source revision");
  EXPECT_TRUE(document->dirty());
  const auto destination = directory_ / "Rejected.yaze";
  EXPECT_EQ(project_
                .SaveAsWithLyraTerrain(destination.string(), *document, *before,
                                       reject_drift)
                .code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rechecks, 2);
  EXPECT_FALSE(fs::exists(destination));
  EXPECT_FALSE(fs::exists(directory_ / "Rejected.lyra-terrain.json"));
  EXPECT_EQ(project_.filepath, original_path);
  EXPECT_EQ(Read(source_), *before);
  EXPECT_EQ(Read(original_path), descriptor_before);
  EXPECT_TRUE(document->dirty());
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

TEST_F(LyraTerrainOpenTest, AssetRebindRequiresDocumentAndNativeSource) {
  LyraTerrainCanvas empty;
  EXPECT_EQ(empty.PreviewAssetRebind(overworld_).status().code(),
            absl::StatusCode::kFailedPrecondition);

  const auto before = document_->Serialize();
  const auto status = canvas_.PreviewAssetRebind(overworld_).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(status.message(), "Open the main project ROM to draw Lyra terrain");
  EXPECT_EQ(document_->Serialize(), before);
  EXPECT_FALSE(canvas_.dirty());
}

TEST_F(LyraTerrainOpenTest, AssetRebindRefusesStalePreviewAndOtherProject) {
  Peer::BindSourcePath(canvas_, project_.lyra_terrain_file);
  auto edited = FixtureProfile();
  edited.tile16_definition_fingerprint = "definitions:edited";
  auto preview = document_->PlanAssetRebind(edited, core::LyraStateBindings{},
                                            std::nullopt);
  ASSERT_TRUE(preview.ok()) << preview.status();
  UndoManager history;

  auto other = project_;
  other.lyra_terrain_file = (directory_ / "other.json").string();
  EXPECT_EQ(
      canvas_.ApplyAssetRebind(&other, overworld_, *preview, history).code(),
      absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(
      canvas_.ApplyAssetRebind(nullptr, overworld_, *preview, history).code(),
      absl::StatusCode::kFailedPrecondition);

  // The native source cannot be resolved, so even a current preview fails
  // before changing the document.
  EXPECT_EQ(
      canvas_.ApplyAssetRebind(&project_, overworld_, *preview, history).code(),
      absl::StatusCode::kFailedPrecondition);

  auto later = document_->authoring();
  later.flight_points.push_back({7, 0xA0, 1, 1, "Later", std::nullopt});
  ASSERT_TRUE(document_->SetAuthoring(document_->authoring(), later).ok());
  const auto before = document_->Serialize();
  const auto stale =
      canvas_.ApplyAssetRebind(&project_, overworld_, *preview, history);
  EXPECT_EQ(stale.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(stale.message(),
            "Lyra terrain changed after the rebind preview; preview again");
  EXPECT_EQ(document_->Serialize(), before);
  EXPECT_EQ(document_->profile(), FixtureProfile());
  EXPECT_EQ(history.UndoStackSize(), 0u);
  ExpectProtectedFilesUnchanged();
}

TEST_F(LyraTerrainOpenTest, AssetRebindRefusesPaintAfterPreviewAtomically) {
  Peer::BindSourcePath(canvas_, project_.lyra_terrain_file);
  auto edited = FixtureProfile();
  edited.tile16_definition_fingerprint = "definitions:edited";
  auto preview = document_->PlanAssetRebind(edited, core::LyraStateBindings{},
                                            std::nullopt);
  ASSERT_TRUE(preview.ok()) << preview.status();
  UndoManager history;
  history.Push(std::make_unique<NoOpAction>());
  history.Push(std::make_unique<NoOpAction>());
  ASSERT_TRUE(history.Undo().ok());
  ASSERT_EQ(history.UndoStackSize(), 1u);
  ASSERT_EQ(history.RedoStackSize(), 1u);

  ASSERT_TRUE(document_->BeginStroke().ok());
  ASSERT_TRUE(document_->Paint(5, 5, 17).ok());
  auto late = document_->EndStroke();
  ASSERT_TRUE(late.ok()) << late.status();
  const auto painted = document_->Serialize();

  // Refused from the document alone; native sources are never read.
  const auto status =
      canvas_.ApplyAssetRebind(&project_, overworld_, *preview, history);
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(status.message(),
            "Lyra terrain changed after the rebind preview; preview again");
  EXPECT_EQ(document_->Serialize(), painted);
  EXPECT_EQ(document_->profile(), FixtureProfile());
  EXPECT_EQ(history.UndoStackSize(), 1u);
  EXPECT_EQ(history.RedoStackSize(), 1u);

  // With the paint undone, the preview is current again and Apply proceeds to
  // the native source, which this fixture cannot resolve.
  ASSERT_TRUE(document_->ApplyStroke(*late, true).ok());
  const auto native =
      canvas_.ApplyAssetRebind(&project_, overworld_, *preview, history);
  EXPECT_EQ(native.message(), "Open the main project ROM to draw Lyra terrain");
  EXPECT_EQ(document_->profile(), FixtureProfile());
  EXPECT_EQ(history.UndoStackSize(), 1u);
  EXPECT_EQ(history.RedoStackSize(), 1u);
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

// Opt-in: YAZE_TEST_ROM_OOS names an Oracle ROM that is only ever read. All
// writes go to fresh private copies under one unique temp directory.
TEST(LyraTerrainNativeRebindTest,
     RebindsEditedTile16ThroughHistorySaveAndReopen) {
  const char* source_env = std::getenv("YAZE_TEST_ROM_OOS");
  if (source_env == nullptr || !fs::is_regular_file(source_env))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to an Oracle ROM (read only)";
  const fs::path source(source_env);
  const auto source_bytes = Read(source);
  ASSERT_FALSE(source_bytes.empty());
  const auto source_sha = core::ComputeSourceArtifactSha256(source_bytes);

  const char* retained_output = std::getenv("YAZE_LYRA_REBIND_ARTIFACT_DIR");
  const bool retain = retained_output != nullptr && *retained_output != '\0';
  const auto root = retain ? fs::absolute(retained_output)
                           : test::UniqueTempPath("yaze_lyra_rebind");
  ASSERT_TRUE(fs::create_directory(root));
  const auto previous_addresses = core::RomSettings::Get().address_overrides();
  const auto previous_flags = core::FeatureFlags::get();
  absl::Cleanup restore = [&]() {
    gfx::Arena::Get().ClearTextureQueue();
    core::RomSettings::Get().SetAddressOverrides(previous_addresses);
    core::FeatureFlags::get() = previous_flags;
    std::error_code ec;
    if (!retain)
      fs::remove_all(root, ec);
  };
  core::RomSettings::Get().ClearOverrides();
  core::FeatureFlags::get().overworld.kLoadCustomOverworld = true;
  Rom::LoadOptions rom_options;
  rom_options.load_resource_labels = false;
  // Editor defaults; Save validation reloads the ROM file with the same ones.
  zelda3::LoadOptions data_options;
  data_options.expand_rom = false;

  const auto base_rom = root / "private-base.sfc";
  {
    Rom copy;
    ASSERT_OK(copy.LoadFromFile(source.string(), rom_options));
    ASSERT_OK(test::SaveFreshFixtureRom(copy, base_rom));
  }
  Rom rom;
  ASSERT_OK(rom.LoadFromFile(base_rom.string(), rom_options));
  zelda3::GameData data;
  ASSERT_OK(zelda3::LoadGameData(rom, data, data_options));
  zelda3::Overworld overworld(&rom, &data);
  ASSERT_OK(overworld.Load(&rom));

  project::YazeProject project;
  project.name = "Lyra native rebind fixture";
  project.feature_flags = core::FeatureFlags::get();
  project.filepath = (root / "Oracle.yaze").string();
  project.rom_filename = base_rom.string();
  ASSERT_OK(project.Save());
  {
    LyraTerrainCanvas creator;
    ASSERT_OK(creator.Create(&project, overworld));
    ASSERT_OK(creator.Save(&project, overworld));
    ASSERT_OK(project.Save());
    creator.ProjectSaveCommitted();
    EXPECT_FALSE(creator.dirty());
  }
  const auto sidecar = project.GetAbsolutePath(project.lyra_terrain_file);
  const auto created_bytes = Read(sidecar);

  UndoManager history;
  LyraTerrainCanvas canvas;
  ASSERT_OK(canvas.Load(&project, overworld));
  ASSERT_OK(canvas.PrepareMapAssets(overworld, 0));
  const auto* document = canvas.document();
  ASSERT_NE(document, nullptr);
  const auto original_profile = document->profile();

  // Real per-map binding plus every metadata kind the rebind must retain.
  auto authoring = document->authoring();
  auto& a0 = authoring.maps[0];
  a0.properties.message_id = 0x42;
  a0.properties.music = {1, 2, 3, 4};
  a0.sprites.push_back({1, 0x13, 4, 5, 0});
  a0.doors.push_back({2, 6, 7, 0x10});
  a0.exits.push_back({3, 0x104, 8, 9, 0x12});
  authoring.flight_points.push_back(
      {4, 0xA0, 10, 11, "Rebind Point", std::nullopt});
  authoring.edge_connections.push_back(
      {5, 0x98, 0xA0, core::LyraEdgePassage{3, 6}});
  ASSERT_OK(canvas.EditAuthoring(authoring, overworld, history));
  ASSERT_EQ(history.UndoStackSize(), 1u);
  for (int state = 0; state < 3; ++state) {
    const auto resolved =
        zelda3::ResolveLyraMapAssets(overworld, a0.properties, state);
    ASSERT_TRUE(resolved.ok()) << resolved.status();
    EXPECT_EQ(document->authoring().maps[0].asset_bindings[state],
              resolved->binding);
  }

  constexpr uint16_t kEditedTile = 0x123;
  ASSERT_LT(kEditedTile, original_profile.tile_count);
  ASSERT_OK(canvas.PrepareMapAssets(overworld, 0));
  const TileBrush brush{1, 1, {kEditedTile}};
  ASSERT_OK(canvas.PaintPattern(brush, 3, 4, 1, 1, true, history));
  ASSERT_OK(canvas.PaintPattern(brush, 40, 70, 1, 1, true, history));
  ASSERT_EQ(history.UndoStackSize(), 3u);
  EXPECT_EQ(document->MapTile(0xA0, 3, 4).value(), kEditedTile);
  EXPECT_EQ(document->MapTile(0xB1, 8, 6).value(), kEditedTile);
  const auto maps_before = document->maps();
  const auto authoring_before = document->authoring();

  // Native in-memory edit of one used definition: flip quadrant 0.
  auto& tiles16 = *overworld.mutable_tiles16();
  const gfx::Tile16 original_tile = tiles16[kEditedTile];
  gfx::TileInfo flipped = original_tile.tile0_;
  flipped.horizontal_mirror_ = !flipped.horizontal_mirror_;
  ASSERT_NE(gfx::TileInfoToWord(flipped),
            gfx::TileInfoToWord(original_tile.tile0_));
  tiles16[kEditedTile] =
      gfx::Tile16(flipped, original_tile.tile1_, original_tile.tile2_,
                  original_tile.tile3_);

  core::LyraTerrainProfile edited_profile;
  ASSERT_OK(zelda3::BindLyraTerrainSource(overworld, &edited_profile));
  EXPECT_NE(edited_profile.tile16_definition_fingerprint,
            original_profile.tile16_definition_fingerprint);
  EXPECT_EQ(edited_profile.graphics_fingerprint,
            original_profile.graphics_fingerprint);
  const auto stale = canvas.PrepareMapAssets(overworld, 0);
  EXPECT_EQ(stale.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(stale.message().find("rebind"), std::string::npos) << stale;
  EXPECT_EQ(canvas.Save(&project, overworld).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(Read(sidecar), created_bytes);
  EXPECT_FALSE(
      core::LyraTerrainDocument::Parse(created_bytes, edited_profile).ok());

  auto preview = canvas.PreviewAssetRebind(overworld);
  ASSERT_TRUE(preview.ok()) << preview.status();
  EXPECT_EQ(preview->before_profile, original_profile);
  EXPECT_EQ(preview->after_profile, edited_profile);
  EXPECT_EQ(preview->terrain_fingerprint, document->TerrainFingerprint());
  EXPECT_TRUE(preview->binding_changes.empty());
  EXPECT_EQ(preview->changed_tiles, std::vector<uint16_t>{kEditedTile});
  EXPECT_EQ(preview->affected_maps, (std::vector<int>{0xA0, 0xB1}));
  EXPECT_TRUE(preview->invalid_cells.empty());
  EXPECT_EQ(document->profile(), original_profile);
  EXPECT_EQ(history.UndoStackSize(), 3u);

  ASSERT_OK(canvas.ApplyAssetRebind(&project, overworld, *preview, history));
  EXPECT_EQ(history.UndoStackSize(), 4u);
  EXPECT_EQ(history.RedoStackSize(), 0u);
  EXPECT_EQ(document->profile(), edited_profile);
  EXPECT_EQ(document->maps(), maps_before);
  EXPECT_EQ(document->authoring(), authoring_before);
  ASSERT_OK(canvas.PrepareMapAssets(overworld, 0));
  {
    core::LyraTerrainProfile atlas_profile;
    gfx::Bitmap bound_atlas;
    ASSERT_OK(
        zelda3::BindLyraTerrainSource(overworld, &atlas_profile, &bound_atlas));
    EXPECT_EQ(canvas.atlas().vector(), bound_atlas.vector());
  }

  // Undo restores the old profile, which the edited source refuses again.
  ASSERT_OK(history.Undo());
  EXPECT_EQ(document->profile(), original_profile);
  EXPECT_EQ(document->maps(), maps_before);
  EXPECT_EQ(document->authoring(), authoring_before);
  EXPECT_EQ(canvas.PrepareMapAssets(overworld, 0).code(),
            absl::StatusCode::kFailedPrecondition);
  ASSERT_OK(history.Redo());
  EXPECT_EQ(document->profile(), edited_profile);
  ASSERT_OK(canvas.PrepareMapAssets(overworld, 0));
  EXPECT_EQ(history.UndoStackSize(), 4u);
  EXPECT_EQ(history.RedoStackSize(), 0u);

  // The rebind is still memory-only. Neither save path may publish bindings
  // that a cold reopen of the unchanged ROM file cannot reproduce.
  const auto project_bytes = Read(project.filepath);
  const auto root_entries = [&root]() {
    std::set<std::string> names;
    for (const auto& entry : fs::directory_iterator(root))
      names.insert(entry.path().filename().string());
    return names;
  };
  const auto entries_before = root_entries();
  for (const bool save_as : {false, true}) {
    const auto refused =
        save_as ? canvas.SaveAs(&project, (root / "Refused.yaze").string(),
                                overworld)
                : canvas.Save(&project, overworld);
    EXPECT_EQ(refused.code(), absl::StatusCode::kFailedPrecondition) << save_as;
    EXPECT_NE(refused.message().find("Save ROM before saving the Lyra project"),
              std::string::npos)
        << refused;
    EXPECT_EQ(root_entries(), entries_before);
    EXPECT_EQ(Read(sidecar), created_bytes);
    EXPECT_EQ(Read(project.filepath), project_bytes);
    EXPECT_EQ(project.GetAbsolutePath(project.lyra_terrain_file), sidecar);
    EXPECT_EQ(canvas.document(), document);
    EXPECT_TRUE(canvas.dirty());
    EXPECT_EQ(document->profile(), edited_profile);
    EXPECT_EQ(document->maps(), maps_before);
    EXPECT_EQ(document->authoring(), authoring_before);
    EXPECT_EQ(history.UndoStackSize(), 4u);
    EXPECT_EQ(history.RedoStackSize(), 0u);
  }
  ASSERT_OK(canvas.PrepareMapAssets(overworld, 0));

  // Disclosed fixture patch: persist the same definition word at the table
  // the loader detected, then write the loaded private ROM file through the
  // normal ROM save path, as Save ROM does.
  bool expanded = false;
  auto base_tiles = zelda3::ReadMap16Tiles(rom, &expanded);
  ASSERT_TRUE(base_tiles.ok()) << base_tiles.status();
  const int table =
      expanded ? zelda3::GetMap16TilesExpanded() : zelda3::kMap16Tiles;
  ASSERT_OK(
      rom.WriteWord(table + kEditedTile * 8, gfx::TileInfoToWord(flipped)));
  auto persisted = zelda3::ReadMap16Tiles(rom);
  ASSERT_TRUE(persisted.ok()) << persisted.status();
  ASSERT_EQ(persisted->size(), base_tiles->size());
  for (size_t tile = 0; tile < persisted->size(); ++tile) {
    if (tile == kEditedTile)
      EXPECT_EQ((*persisted)[tile], tiles16[kEditedTile]);
    else
      EXPECT_EQ((*persisted)[tile], (*base_tiles)[tile]) << tile;
  }
  const auto base_bytes = Read(base_rom);
  ASSERT_EQ(fs::path(rom.filename()), fs::absolute(base_rom));
  ASSERT_OK(rom.SaveToFile(Rom::SaveSettings{}));
  EXPECT_NE(Read(base_rom), base_bytes);
  // A descriptor pointing at the old source must fail even though the live
  // ROM filename now names the successfully saved private native revision.
  project.rom_filename = source.string();
  EXPECT_EQ(canvas.Save(&project, overworld).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(Read(sidecar), created_bytes);
  EXPECT_TRUE(canvas.dirty());
  project.rom_filename = base_rom.string();

  ASSERT_OK(canvas.Save(&project, overworld));
  ASSERT_OK(project.Save());
  canvas.ProjectSaveCommitted();
  EXPECT_FALSE(canvas.dirty());
  const auto saved_bytes = Read(sidecar);
  EXPECT_NE(saved_bytes, created_bytes);

  // Independent reopen from the persisted private bytes only.
  {
    project::YazeProject reopened;
    ASSERT_OK(reopened.Open(project.filepath));
    Rom reopened_rom;
    ASSERT_OK(reopened_rom.LoadFromFile(
        reopened.GetAbsolutePath(reopened.rom_filename), rom_options));
    EXPECT_EQ(reopened_rom.vector(), rom.vector());
    zelda3::GameData reopened_data;
    ASSERT_OK(zelda3::LoadGameData(reopened_rom, reopened_data, data_options));
    zelda3::Overworld reopened_overworld(&reopened_rom, &reopened_data);
    ASSERT_OK(reopened_overworld.Load(&reopened_rom));
    EXPECT_EQ(reopened_overworld.tiles16()[kEditedTile], tiles16[kEditedTile]);
    LyraTerrainCanvas reopened_canvas;
    ASSERT_OK(reopened_canvas.Load(&reopened, reopened_overworld));
    ASSERT_OK(reopened_canvas.PrepareMapAssets(reopened_overworld, 0));
    const auto* restored = reopened_canvas.document();
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->maps(), maps_before);
    EXPECT_EQ(restored->authoring(), authoring_before);
    EXPECT_EQ(restored->profile(), edited_profile);
    EXPECT_FALSE(reopened_canvas.dirty());
    EXPECT_EQ(reopened_canvas.atlas().vector(), canvas.atlas().vector());
  }

  // Normal project Save As forks the rebound sidecar.
  ASSERT_OK(canvas.SaveAs(&project, (root / "Fork.yaze").string(), overworld));
  const auto forked = core::LyraTerrainDocument::Parse(
      Read(project.GetAbsolutePath(project.lyra_terrain_file)), edited_profile);
  ASSERT_TRUE(forked.ok()) << forked.status();
  EXPECT_EQ(forked->maps(), maps_before);
  EXPECT_EQ(forked->authoring(), authoring_before);
  EXPECT_EQ(Read(sidecar), saved_bytes);

  EXPECT_EQ(Read(source), source_bytes);
  EXPECT_EQ(core::ComputeSourceArtifactSha256(Read(source)), source_sha);
}

}  // namespace
}  // namespace yaze::editor
