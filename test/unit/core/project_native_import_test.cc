#include "core/project_native_import.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "fixtures/lyra/native_directory_fixture.h"
#include "unique_temp_path.h"
#include "util/rom_hash.h"

namespace yaze::project {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

std::string Read(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
void Write(const fs::path& path, const std::string& text) {
  std::ofstream output(path, std::ios::binary);
  output.write(text.data(), text.size());
}
std::string Hash(const std::string& text) {
  return core::ComputeSourceArtifactSha256(text);
}

class NativeOverworldProjectImportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = test::UniqueTempPath("native_import") / "Project.yazeproj";
    fs::create_directories(root_ / "source");
    const std::string source(0x200000, '\0');
    Write(root_ / "rom", source);
    Write(root_ / "terrain.json", "saved legacy terrain\n");
    Write(root_ / "source/engine.asm", "compiled engine source\n");
    Write(root_ / "manifest.json",
          Json{{"romChecksum", "old"},
               {"rom_sha1", "old"},
               {"editorProducer", {{"identity", "retain"}}}}
              .dump());
    descriptor_ =
        "[project]\nname=Native import\nproject_id=native_import_fixture\n"
        "[files]\nrom_filename=rom\n"
        "code_folder=source\nlyra_terrain_file=terrain.json\n"
        "native_save_producer_file=transfer/producer.json\n"
        "native_save_transfer_proof_file=transfer/proof.json\n"
        "[rom]\nwrite_policy=block\nexpected_hash=old\n"
        "[music]\nstorage_key=native_import_fixture\n"
        "[future_settings]\nuser_note=preserve\n";
    Write(root_ / "project.yaze", descriptor_);
    ASSERT_TRUE(
        project_.LoadFromString(descriptor_, (root_ / "project.yaze").string())
            .ok());
    test::native_overworld::NativeDirectoryFixture fixture(192);
    fixture.UpgradeToRevision4();
    fixture.AddAuthoringMetadata();
    image_.assign(reinterpret_cast<const char*>(fixture.bytes.data()),
                  fixture.bytes.size());
    const auto layout =
        rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size());
    ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid) << layout.error;
    Write(root_ / "candidate.sfc", image_);
    receipt_ = {
        {"complete_image", true},
        {"original_geometry_preserved", true},
        {"original_stream_pools_preserved", true},
        {"appearance_activation_ready", true},
        {"capacity", 192},
        {"descriptor_revision", 4},
        {"byte_count", image_.size()},
        {"native_payload_count", layout.entries.size()},
        {"gameplay_area_count", 21},
        {"inspection",
         {{"validated_maps", 192},
          {"original_terrain_maps_preserved", 160},
          {"checked_tail_terrain_maps", 32},
          {"checked_tail_asset_states", 96},
          {"gameplay_area_count", 21},
          {"native_source_sha256", std::string(64, 'a')}}},
        {"source_rom_sha256", Hash(source)},
        {"source_terrain_sha256", Hash(Read(root_ / "terrain.json"))},
        {"image_sha256", Hash(image_)},
        {"engine",
         {{"source_files",
           {{"engine.asm", Hash(Read(root_ / "source/engine.asm"))}}}}}};
    SaveReceipt();
  }
  void TearDown() override {
    std::error_code error;
    fs::remove_all(root_.parent_path(), error);
  }
  void SaveReceipt() { Write(root_ / "import.json", receipt_.dump()); }
  auto Prepare() {
    return PrepareNativeOverworldProjectImport(
        project_, (root_ / "candidate.sfc").string(),
        (root_ / "import.json").string(), Hash(descriptor_));
  }
  fs::path root_;
  YazeProject project_;
  std::string descriptor_, image_;
  Json receipt_;
};

TEST_F(NativeOverworldProjectImportTest, PublishesBundleAndPreservesSaveGates) {
  auto plan = Prepare();
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(Read(root_ / "project.yaze"), descriptor_);
  EXPECT_EQ(Read(root_ / "rom").size(), 0x200000);
  const auto old_producer = project_.native_save_producer_file;
  const auto old_proof = project_.native_save_transfer_proof_file;
  ASSERT_TRUE(plan->Publish().ok());
  EXPECT_EQ(Read(root_ / "rom"), image_);
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open((root_ / "project.yaze").string()).ok());
  EXPECT_TRUE(reopened.lyra_terrain_file.empty());
  EXPECT_TRUE(reopened.lyra_build_request_file.empty());
  EXPECT_EQ(reopened.native_save_producer_file, old_producer);
  EXPECT_EQ(reopened.native_save_transfer_proof_file, old_proof);
  EXPECT_EQ(reopened.rom_metadata.write_policy, RomWritePolicy::kBlock);
  EXPECT_EQ(
      reopened.rom_metadata.expected_hash,
      util::ComputeRomHash(reinterpret_cast<const uint8_t*>(image_.data()),
                           image_.size()));
  const auto manifest = Json::parse(Read(root_ / "manifest.json"));
  EXPECT_EQ(manifest["romChecksum"],
            util::ComputeFileSha1Hex((root_ / "rom").string()));
  EXPECT_EQ(manifest["rom_sha1"], manifest["romChecksum"]);
  EXPECT_EQ(manifest["editorProducer"]["identity"], "retain");
  EXPECT_NE(Read(root_ / "project.yaze").find("user_note=preserve"),
            std::string::npos);
  EXPECT_EQ(Read(root_ / "terrain.json"), "saved legacy terrain\n");
  EXPECT_FALSE(fs::exists(root_ / "project.yaze.native-save.json"));
}

TEST_F(NativeOverworldProjectImportTest, RollbackRestoresAllPublishedFiles) {
  auto plan = Prepare();
  ASSERT_TRUE(plan.ok()) << plan.status();
  const auto rom_before = Read(root_ / "rom");
  const auto manifest_before = Read(root_ / "manifest.json");
  const auto result = plan->Publish(
      [] { return absl::FailedPreconditionError("injected reopen failure"); });
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(Read(root_ / "project.yaze"), descriptor_);
  EXPECT_EQ(Read(root_ / "rom"), rom_before);
  EXPECT_EQ(Read(root_ / "manifest.json"), manifest_before);
}

TEST_F(NativeOverworldProjectImportTest,
       RefusesChangedInputsBeforePublication) {
  for (const auto* name : {"terrain.json", "candidate.sfc", "import.json",
                           "source/engine.asm", "project.yaze", "rom"}) {
    SCOPED_TRACE(name);
    auto plan = Prepare();
    ASSERT_TRUE(plan.ok()) << plan.status();
    const auto original = Read(root_ / name);
    Write(root_ / name, original + "changed");
    EXPECT_FALSE(plan->Publish().ok());
    EXPECT_EQ(Read(root_ / name), original + "changed");
    if (std::string(name) != "rom")
      EXPECT_EQ(Read(root_ / "rom").size(), 0x200000);
    Write(root_ / name, original);
  }
}

TEST_F(NativeOverworldProjectImportTest, RejectsWrongReceiptAndInvalidImage) {
  receipt_["image_sha256"] = std::string(64, '0');
  SaveReceipt();
  EXPECT_FALSE(Prepare().ok());
  receipt_["image_sha256"] = Hash(image_);
  SaveReceipt();
  auto broken = image_;
  broken[0x147000] ^= 1;
  Write(root_ / "candidate.sfc", broken);
  receipt_["image_sha256"] = Hash(broken);
  SaveReceipt();
  EXPECT_FALSE(Prepare().ok());
  EXPECT_EQ(Read(root_ / "project.yaze"), descriptor_);
}

TEST_F(NativeOverworldProjectImportTest,
       RequiresFinalInspectionAndMatchingAreas) {
  const auto original = receipt_;
  receipt_.erase("inspection");
  SaveReceipt();
  EXPECT_FALSE(Prepare().ok());
  for (const auto* field :
       {"validated_maps", "original_terrain_maps_preserved",
        "checked_tail_terrain_maps", "checked_tail_asset_states",
        "gameplay_area_count"}) {
    SCOPED_TRACE(field);
    receipt_ = original;
    receipt_["inspection"][field] = 0;
    SaveReceipt();
    EXPECT_FALSE(Prepare().ok());
  }
  receipt_ = original;
  receipt_["gameplay_area_count"] = 32;
  SaveReceipt();
  EXPECT_FALSE(Prepare().ok());
}

TEST_F(NativeOverworldProjectImportTest,
       RetainsStrictGateAfterLegacyRetirement) {
  project_.native_save_producer_file.clear();
  project_.native_save_transfer_proof_file.clear();
  project_.lyra_build_request_file = "request.json";
  ASSERT_TRUE(project_.Save().ok());
  descriptor_ = Read(root_ / "project.yaze");
  EXPECT_FALSE(Prepare().ok());
  EXPECT_EQ(Read(root_ / "rom").size(), 0x200000);
}

TEST_F(NativeOverworldProjectImportTest, RejectsConflictingManifestOutput) {
  project_.build_target = (root_ / "source/Roms/oos168x.sfc").string();
  project_.hack_manifest_file = (root_ / "hack_manifest.json").string();
  const Json manifest = {
      {"manifest_version", 3},
      {"hack_name", "Native fixture"},
      {"build_pipeline",
       {{"dev_rom", "rom"}, {"patched_rom", "old/Roms/oos168x.sfc"}}}};
  Write(root_ / "hack_manifest.json", manifest.dump());
  ASSERT_TRUE(project_.hack_manifest.LoadFromString(manifest.dump()).ok());
  ASSERT_TRUE(project_.Save().ok());
  descriptor_ = Read(root_ / "project.yaze");
  EXPECT_FALSE(Prepare().ok());
  auto coherent = manifest;
  coherent["build_pipeline"]["patched_rom"] = "source/Roms/oos168x.sfc";
  Write(root_ / "hack_manifest.json", coherent.dump());
  auto plan = Prepare();
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(plan->Publish().ok());
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open((root_ / "project.yaze").string()).ok());
  ASSERT_TRUE(reopened.hack_manifest.loaded());
  EXPECT_EQ(reopened.GetAbsolutePath(
                reopened.hack_manifest.build_pipeline().patched_rom),
            reopened.build_target);
  EXPECT_EQ(
      Json::parse(Read(root_ / "hack_manifest.json"))["rom"]["dev_rom_size"],
      image_.size());
}

TEST_F(NativeOverworldProjectImportTest,
       SourceRefreshBindsOutputAndGuardsAdapter) {
  fs::create_directories(root_ / "new-source/Scripts/Build");
  fs::create_directories(root_ / "new-source/Roms");
  fs::copy_file(root_ / "source/engine.asm", root_ / "new-source/engine.asm");
  Write(root_ / "new-source/Scripts/Build/build_rom.sh", "compiler");
  Write(root_ / "new-source/Scripts/Build/authoring_bundle_build.py",
        "adapter");
  project_.output_folder = (root_ / "source/Roms").string();
  ASSERT_TRUE(project_.Save().ok());
  descriptor_ = Read(root_ / "project.yaze");
  NativeOverworldImportBuildSettings build{
      "new-source",
      "python3 new-source/Scripts/Build/authoring_bundle_build.py "
      "--project project.yaze",
      "new-source/Roms/oos168x.sfc", "new-source/engine.asm"};
  auto plan = PrepareNativeOverworldProjectImport(
      project_, (root_ / "candidate.sfc").string(),
      (root_ / "import.json").string(), Hash(descriptor_), build);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->project.output_folder, (root_ / "new-source/Roms").string());
  Write(root_ / "new-source/Scripts/Build/authoring_bundle_build.py",
        "changed");
  EXPECT_FALSE(plan->Publish().ok());
  EXPECT_EQ(Read(root_ / "project.yaze"), descriptor_);
}

TEST_F(NativeOverworldProjectImportTest,
       RejectsReadOnlyDirtyAndEscapingSource) {
  project_.rom_metadata.write_policy = RomWritePolicy::kReadOnly;
  EXPECT_FALSE(Prepare().ok());
  ASSERT_TRUE(
      project_.LoadFromString(descriptor_, (root_ / "project.yaze").string())
          .ok());
  project_.build_script = "unsaved change";
  EXPECT_FALSE(Prepare().ok());
  ASSERT_TRUE(
      project_.LoadFromString(descriptor_, (root_ / "project.yaze").string())
          .ok());
  receipt_["engine"]["source_files"] = {{"../rom", Hash(Read(root_ / "rom"))}};
  SaveReceipt();
  EXPECT_FALSE(Prepare().ok());
}
}  // namespace
}  // namespace yaze::project
