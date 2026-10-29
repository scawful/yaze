#include "core/lyra_terrain_document.h"
#include "core/project.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::project {

namespace {

std::filesystem::path MakeUniqueTempDir(const std::string& prefix) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         (prefix + "_" + std::to_string(now));
}

class ScopedTempDir {
 public:
  explicit ScopedTempDir(std::filesystem::path path) : path_(std::move(path)) {
    std::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

class ScopedCurrentPath {
 public:
  explicit ScopedCurrentPath(const std::filesystem::path& path)
      : original_path_(std::filesystem::current_path()) {
    std::filesystem::current_path(path);
  }

  ~ScopedCurrentPath() {
    std::error_code ec;
    std::filesystem::current_path(original_path_, ec);
  }

 private:
  std::filesystem::path original_path_;
};

class ScopedReadOnlyDirectory {
 public:
  explicit ScopedReadOnlyDirectory(const std::filesystem::path& path)
      : path_(path), original_(std::filesystem::status(path).permissions()) {
    std::filesystem::permissions(
        path_,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);
  }

  ~ScopedReadOnlyDirectory() {
    std::error_code ec;
    std::filesystem::permissions(path_, original_,
                                 std::filesystem::perm_options::replace, ec);
  }

 private:
  std::filesystem::path path_;
  std::filesystem::perms original_;
};

void WriteTextFile(const std::filesystem::path& path, const std::string& data) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::out | std::ios::trunc | std::ios::binary);
  ASSERT_TRUE(file.is_open())
      << "Failed to open file for writing: " << path.string();
  file << data;
  file.close();
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream file(path);
  EXPECT_TRUE(file.is_open())
      << "Failed to open file for reading: " << path.string();
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  file.close();
  return content;
}

std::string WithCrLf(const std::string& content) {
  std::string result;
  result.reserve(content.size() * 2);
  for (char ch : content) {
    if (ch == '\n') {
      result += "\r\n";
    } else {
      result += ch;
    }
  }
  return result;
}

}  // namespace

TEST(ProjectPathsTest, LyraTerrainPathIsDistinctAndSurvivesReopen) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_lyra_project"));
  const auto project_file = temp.path() / "Oracle.yaze";
  const auto terrain_file = temp.path() / "terrain" / "lyra.json";
  const auto catalog_file = temp.path() / "labels.json";
  WriteTextFile(terrain_file, "{}");
  WriteTextFile(catalog_file, "{}");
  YazeProject project;
  project.name = "Oracle";
  project.filepath = project_file.string();
  project.lyra_terrain_file = terrain_file.string();
  project.terrain_catalog_file = catalog_file.string();
  ASSERT_TRUE(project.Save().ok());
  EXPECT_NE(
      ReadTextFile(project_file).find("lyra_terrain_file=terrain/lyra.json"),
      std::string::npos);
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(project_file.string()).ok());
  EXPECT_EQ(reopened.lyra_terrain_file, terrain_file.string());
  EXPECT_EQ(reopened.terrain_catalog_file, catalog_file.string());
  ASSERT_TRUE(reopened
                  .LoadFromString("[project]\nname=Other\n",
                                  (temp.path() / "Other.yaze").string())
                  .ok());
  EXPECT_TRUE(reopened.lyra_terrain_file.empty());
}

TEST(ProjectPathsTest, LyraBuildRequestKeepsIdentityThroughSaveAsAndReset) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_lyra_request_identity"));
  ScopedTempDir elsewhere(MakeUniqueTempDir("yaze_lyra_request_cwd"));
  const auto original = temp.path() / "original" / "Oracle.yaze";
  const auto request = original.parent_path() / "requests" / "native.json";
  const auto destination = temp.path() / "copy" / "Saved.yaze";
  const std::string request_bytes = "source-bound request fixture\n";
  WriteTextFile(request, request_bytes);
  WriteTextFile(original,
                "[project]\nname=Lyra request identity\n\n[files]\n"
                "lyra_build_request_file=requests/native.json\n");
  std::filesystem::create_directories(destination.parent_path());

  {
    ScopedCurrentPath current_path(elsewhere.path());
    YazeProject project;
    ASSERT_TRUE(project.Open(original.string()).ok());
    EXPECT_EQ(project.lyra_build_request_file, request.string());
    ASSERT_TRUE(project.Save().ok());
    EXPECT_NE(ReadTextFile(original).find(
                  "lyra_build_request_file=requests/native.json\n"),
              std::string::npos);
    ASSERT_TRUE(project.SaveAs(destination.string()).ok());
    EXPECT_EQ(project.lyra_build_request_file, request.string());
  }

  EXPECT_NE(
      ReadTextFile(destination)
          .find("lyra_build_request_file=../original/requests/native.json\n"),
      std::string::npos);
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(destination.string()).ok());
  EXPECT_EQ(reopened.lyra_build_request_file, request.string());
  EXPECT_EQ(ReadTextFile(request), request_bytes);
  EXPECT_FALSE(std::filesystem::exists(destination.parent_path() / "requests" /
                                       "native.json"));

  const auto unrelated = temp.path() / "Unrelated.yaze";
  WriteTextFile(unrelated, "[project]\nname=No Lyra request\n");
  ASSERT_TRUE(reopened.Open(unrelated.string()).ok());
  EXPECT_TRUE(reopened.lyra_build_request_file.empty());
}

TEST(ProjectPathsTest, NativeSaveReferencesKeepIdentityThroughSaveAsAndReset) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_native_save_reference_identity"));
  ScopedTempDir elsewhere(MakeUniqueTempDir("yaze_native_save_reference_cwd"));
  const auto original = temp.path() / "original" / "Oracle.yaze";
  const auto producer = original.parent_path() / "control" / "producer.json";
  const auto proof = original.parent_path() / "control" / "proof.json";
  const auto receipt = original.parent_path() / "control" / "receipt.json";
  const auto destination = temp.path() / "copy" / "Saved.yaze";
  WriteTextFile(producer, "producer identity\n");
  WriteTextFile(proof, "immutable transfer proof\n");
  WriteTextFile(receipt, "retained receipt\n");
  WriteTextFile(original,
                "[project]\nname=Native save reference identity\n\n[files]\n"
                "native_save_producer_file=control/producer.json\n"
                "native_save_transfer_proof_file=control/proof.json\n"
                "native_save_receipt_file=control/receipt.json\n");
  std::filesystem::create_directories(destination.parent_path());

  ScopedCurrentPath current_path(elsewhere.path());
  YazeProject project;
  ASSERT_TRUE(project.Open(original.string()).ok());
  EXPECT_EQ(project.native_save_producer_file, producer.string());
  EXPECT_EQ(project.native_save_transfer_proof_file, proof.string());
  EXPECT_EQ(project.native_save_receipt_file, receipt.string());
  ASSERT_TRUE(project.Save().ok());
  const auto saved_original = ReadTextFile(original);
  EXPECT_NE(
      saved_original.find("native_save_producer_file=control/producer.json\n"),
      std::string::npos);
  EXPECT_NE(saved_original.find(
                "native_save_transfer_proof_file=control/proof.json\n"),
            std::string::npos);
  EXPECT_NE(
      saved_original.find("native_save_receipt_file=control/receipt.json\n"),
      std::string::npos);
  ASSERT_TRUE(project.SaveAs(destination.string()).ok());
  EXPECT_EQ(project.native_save_producer_file, producer.string());
  EXPECT_EQ(project.native_save_transfer_proof_file, proof.string());
  EXPECT_EQ(project.native_save_receipt_file, receipt.string());
  const auto saved_copy = ReadTextFile(destination);
  EXPECT_NE(
      saved_copy.find(
          "native_save_producer_file=../original/control/producer.json\n"),
      std::string::npos);
  EXPECT_NE(saved_copy.find("native_save_transfer_proof_file="
                            "../original/control/proof.json\n"),
            std::string::npos);
  EXPECT_NE(saved_copy.find(
                "native_save_receipt_file=../original/control/receipt.json\n"),
            std::string::npos);

  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(destination.string()).ok());
  EXPECT_EQ(reopened.native_save_producer_file, producer.string());
  EXPECT_EQ(reopened.native_save_transfer_proof_file, proof.string());
  EXPECT_EQ(reopened.native_save_receipt_file, receipt.string());
  EXPECT_EQ(ReadTextFile(original), saved_original);
  EXPECT_EQ(ReadTextFile(producer), "producer identity\n");
  EXPECT_EQ(ReadTextFile(proof), "immutable transfer proof\n");
  EXPECT_EQ(ReadTextFile(receipt), "retained receipt\n");
  EXPECT_FALSE(std::filesystem::exists(destination.parent_path() / "control"));

  ASSERT_TRUE(reopened.ResetToDefaults().ok());
  EXPECT_TRUE(reopened.native_save_producer_file.empty());
  EXPECT_TRUE(reopened.native_save_transfer_proof_file.empty());
  EXPECT_TRUE(reopened.native_save_receipt_file.empty());
  const auto reset_descriptor = ReadTextFile(destination);
  EXPECT_EQ(reset_descriptor.find("native_save_producer_file="),
            std::string::npos);
  EXPECT_EQ(reset_descriptor.find("native_save_transfer_proof_file="),
            std::string::npos);
  EXPECT_EQ(reset_descriptor.find("native_save_receipt_file="),
            std::string::npos);
  ASSERT_TRUE(reopened.Open(original.string()).ok());
  ASSERT_FALSE(reopened.native_save_producer_file.empty());
  ASSERT_TRUE(reopened
                  .LoadFromString("[project]\nname=Unrelated\n",
                                  (temp.path() / "Unrelated.yaze").string())
                  .ok());
  EXPECT_TRUE(reopened.native_save_producer_file.empty());
  EXPECT_TRUE(reopened.native_save_transfer_proof_file.empty());
  EXPECT_TRUE(reopened.native_save_receipt_file.empty());
}

TEST(ProjectPathsTest, NativeSaveFutureReferencesDoNotCreateControlFiles) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_native_save_future_references"));
  const auto original = temp.path() / "original" / "Oracle.yaze";
  const auto producer = original.parent_path() / "producer.json";
  const auto proof = original.parent_path() / "future" / "proof.json";
  const auto receipt = original.parent_path() / "future" / "receipt.json";
  const auto destination = temp.path() / "copy" / "Saved.yaze";
  WriteTextFile(producer, "existing producer\n");
  WriteTextFile(original,
                "[project]\nname=Future native save controls\n\n[files]\n"
                "native_save_producer_file=producer.json\n"
                "native_save_transfer_proof_file=future/proof.json\n"
                "native_save_receipt_file=future/receipt.json\n");
  std::filesystem::create_directories(destination.parent_path());

  YazeProject project;
  ASSERT_TRUE(project.Open(original.string()).ok());
  ASSERT_TRUE(project.Save().ok());
  ASSERT_TRUE(project.SaveAs(destination.string()).ok());
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(destination.string()).ok());
  EXPECT_EQ(reopened.native_save_producer_file, producer.string());
  EXPECT_EQ(reopened.native_save_transfer_proof_file, proof.string());
  EXPECT_EQ(reopened.native_save_receipt_file, receipt.string());
  EXPECT_EQ(ReadTextFile(producer), "existing producer\n");
  EXPECT_FALSE(std::filesystem::exists(proof.parent_path()));
  EXPECT_FALSE(std::filesystem::exists(destination.parent_path() / "future"));
  EXPECT_FALSE(
      std::filesystem::exists(destination.parent_path() / "producer.json"));
}

TEST(ProjectPathsTest, LyraSaveAsPreservesFutureNativeSaveReferences) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_lyra_future_native_save_refs"));
  const auto original = temp.path() / "original" / "Oracle.yaze";
  const auto terrain = original.parent_path() / "Oracle.lyra-terrain.json";
  const auto producer = original.parent_path() / "producer.json";
  const auto proof =
      original.parent_path() / "future" / "generation" / "proof.json";
  const auto receipt =
      original.parent_path() / "future" / "generation" / "receipt.json";
  const auto destination = temp.path() / "copy" / "Saved.yaze";
  auto destination_terrain = destination;
  destination_terrain.replace_extension(".lyra-terrain.json");
  WriteTextFile(producer, "existing producer identity\n");
  WriteTextFile(original,
                "[project]\nname=Future Lyra native save controls\n\n[files]\n"
                "lyra_terrain_file=Oracle.lyra-terrain.json\n"
                "native_save_producer_file=producer.json\n"
                "native_save_transfer_proof_file=future/generation/proof.json\n"
                "native_save_receipt_file=future/generation/receipt.json\n");
  const auto original_bytes = ReadTextFile(original);
  std::filesystem::create_directories(destination.parent_path());
  YazeProject project;
  ASSERT_TRUE(project.Open(original.string()).ok());
  core::LyraTerrainProfile profile;
  profile.tile_count = 4096;
  profile.tile16_definition_fingerprint = "definitions:fixture";
  profile.graphics_fingerprint = "graphics:fixture";
  profile.palette_fingerprint = "palette:fixture";
  profile.animation_fingerprint = "animation:fixture";
  auto document = core::LyraTerrainDocument::Create(profile, 11);
  ASSERT_TRUE(document.ok()) << document.status();

  const auto status = project.SaveAsWithLyraTerrain(destination.string(),
                                                    *document, std::nullopt);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(project.native_save_producer_file, producer.string());
  EXPECT_EQ(project.native_save_transfer_proof_file, proof.string());
  EXPECT_EQ(project.native_save_receipt_file, receipt.string());
  EXPECT_EQ(ReadTextFile(original), original_bytes);
  EXPECT_EQ(ReadTextFile(producer), "existing producer identity\n");
  EXPECT_EQ(ReadTextFile(destination_terrain), document->Serialize());
  EXPECT_FALSE(std::filesystem::exists(terrain));
  EXPECT_FALSE(std::filesystem::exists(original.parent_path() / "future"));
  EXPECT_FALSE(std::filesystem::exists(destination.parent_path() / "future"));
  const auto saved_descriptor = ReadTextFile(destination);
  EXPECT_NE(saved_descriptor.find(
                "native_save_producer_file=../original/producer.json\n"),
            std::string::npos);
  EXPECT_NE(saved_descriptor.find("native_save_transfer_proof_file="
                                  "../original/future/generation/proof.json\n"),
            std::string::npos);
  EXPECT_NE(
      saved_descriptor.find("native_save_receipt_file="
                            "../original/future/generation/receipt.json\n"),
      std::string::npos);
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(destination.string()).ok());
  EXPECT_EQ(reopened.native_save_producer_file, producer.string());
  EXPECT_EQ(reopened.native_save_transfer_proof_file, proof.string());
  EXPECT_EQ(reopened.native_save_receipt_file, receipt.string());
}

#ifdef YAZE_ENABLE_JSON_PROJECT_FORMAT
TEST(ProjectPathsTest, JsonNativeSaveReferencesSurviveSaveAsAndReopen) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_native_save_reference_json"));
  ScopedTempDir elsewhere(MakeUniqueTempDir("yaze_native_save_json_cwd"));
  const auto original = temp.path() / "original" / "Oracle.yaze";
  const auto producer = original.parent_path() / "control" / "producer.json";
  const auto proof = original.parent_path() / "control" / "proof.json";
  const auto receipt = original.parent_path() / "control" / "receipt.json";
  const auto destination = temp.path() / "copy" / "Saved.yaze";
  WriteTextFile(producer, "JSON producer identity\n");
  WriteTextFile(proof, "JSON immutable transfer proof\n");
  WriteTextFile(receipt, "JSON retained receipt\n");
  WriteTextFile(original, R"json({"yaze_project": {
    "name": "JSON native save references",
    "native_save_producer_file": "control/producer.json",
    "native_save_transfer_proof_file": "control/proof.json",
    "native_save_receipt_file": "control/receipt.json"
  }})json");
  std::filesystem::create_directories(destination.parent_path());

  ScopedCurrentPath current_path(elsewhere.path());
  YazeProject project;
  ASSERT_TRUE(project.Open(original.string()).ok());
  EXPECT_EQ(project.native_save_producer_file, producer.string());
  EXPECT_EQ(project.native_save_transfer_proof_file, proof.string());
  EXPECT_EQ(project.native_save_receipt_file, receipt.string());
  // The public Save path exports native INI after loading a JSON descriptor.
  ASSERT_TRUE(project.Save().ok());
  ASSERT_TRUE(project.SaveAs(destination.string()).ok());
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(destination.string()).ok());
  EXPECT_EQ(reopened.native_save_producer_file, producer.string());
  EXPECT_EQ(reopened.native_save_transfer_proof_file, proof.string());
  EXPECT_EQ(reopened.native_save_receipt_file, receipt.string());
  EXPECT_EQ(ReadTextFile(producer), "JSON producer identity\n");
  EXPECT_EQ(ReadTextFile(proof), "JSON immutable transfer proof\n");
  EXPECT_EQ(ReadTextFile(receipt), "JSON retained receipt\n");
  EXPECT_FALSE(std::filesystem::exists(destination.parent_path() / "control"));

  const auto unrelated = temp.path() / "Unrelated.yaze";
  WriteTextFile(unrelated,
                R"json({"yaze_project": {"name": "No native save refs"}})json");
  ASSERT_TRUE(reopened.Open(unrelated.string()).ok());
  EXPECT_TRUE(reopened.native_save_producer_file.empty());
  EXPECT_TRUE(reopened.native_save_transfer_proof_file.empty());
  EXPECT_TRUE(reopened.native_save_receipt_file.empty());
}

TEST(ProjectPathsTest, JsonLyraBuildRequestSurvivesSaveAsAndReopen) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_lyra_request_json"));
  const auto original = temp.path() / "original" / "Oracle.yaze";
  const auto request = original.parent_path() / "requests" / "native.json";
  const auto destination = temp.path() / "copy" / "Saved.yaze";
  const std::string request_bytes = "source-bound JSON request fixture\n";
  WriteTextFile(request, request_bytes);
  WriteTextFile(original, R"json({"yaze_project": {
    "name": "JSON Lyra request",
    "lyra_build_request_file": "requests/native.json"
  }})json");
  std::filesystem::create_directories(destination.parent_path());

  YazeProject project;
  ASSERT_TRUE(project.Open(original.string()).ok());
  EXPECT_EQ(project.lyra_build_request_file, request.string());
  // The public Save path writes native INI even when Open read JSON.
  ASSERT_TRUE(project.Save().ok());
  ASSERT_TRUE(project.SaveAs(destination.string()).ok());
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(destination.string()).ok());
  EXPECT_EQ(reopened.lyra_build_request_file, request.string());
  EXPECT_EQ(ReadTextFile(request), request_bytes);
  EXPECT_FALSE(std::filesystem::exists(destination.parent_path() / "requests" /
                                       "native.json"));

  const auto unrelated = temp.path() / "Unrelated.yaze";
  WriteTextFile(
      unrelated,
      R"json({"yaze_project": {"name": "No JSON Lyra request"}})json");
  ASSERT_TRUE(reopened.Open(unrelated.string()).ok());
  EXPECT_TRUE(reopened.lyra_build_request_file.empty());
}
#endif  // YAZE_ENABLE_JSON_PROJECT_FORMAT

TEST(ProjectPathsTest, OracleIdentityRequiresManifestRatherThanProjectName) {
  YazeProject project;
  project.name = "Oracle of Secrets";
  EXPECT_FALSE(project.IsOracleCompatible());
  ASSERT_TRUE(
      project.hack_manifest
          .LoadFromString(
              R"json({"manifest_version":2,"hack_name":"Generic Hack"})json")
          .ok());
  EXPECT_FALSE(project.IsOracleCompatible());
  project.hack_manifest.SetOracleProgressionState(
      core::OracleProgressionState{});
  EXPECT_TRUE(project.IsOracleCompatible());
}

TEST(ProjectPathsTest, SpriteBinPathsKeepIdentityThroughSaveAsAndReopen) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_sprite_bin_identity"));
  ScopedTempDir elsewhere(MakeUniqueTempDir("yaze_sprite_bin_cwd"));
  const auto original = temp.path() / "original" / "Oracle.yaze";
  const auto bin = original.parent_path() / "sprites.bin";
  const auto copy = temp.path() / "copy" / "Saved.yaze";
  const auto lyra_copy = temp.path() / "lyra-copy" / "Saved.yaze";
  WriteTextFile(bin, "original BIN bytes");
  WriteTextFile(copy.parent_path() / "sprites.bin", "different BIN bytes");
  std::filesystem::create_directories(lyra_copy.parent_path());
  YazeProject project;
  project.name = "Sprite BIN identity";
  project.filepath = original.string();
  project.lyra_terrain_file =
      (original.parent_path() / "Oracle.lyra-terrain.json").string();
  SpriteAssetBinding asset;
  asset.zsm_path = "actor.zsm";
  asset.graphics_sources[0] = {"bin", "sprites.bin", 0, 3};
  asset.graphics_sources[1] = {"raw_rom", "", 0x300000, 4};
  project.sprite_assets.push_back(asset);
  ASSERT_TRUE(project.Save().ok());
  EXPECT_NE(ReadTextFile(original).find("\"path\":\"sprites.bin\""),
            std::string::npos);

  ScopedCurrentPath current_path(elsewhere.path());
  ASSERT_TRUE(project.Open(original.string()).ok());
  ASSERT_EQ(project.sprite_assets.size(), 1u);
  EXPECT_EQ(project.sprite_assets[0].graphics_sources[0].path, bin.string());
  ASSERT_TRUE(project.SaveAs(copy.string()).ok());
  EXPECT_EQ(project.sprite_assets[0].graphics_sources[0].path, bin.string());
  EXPECT_NE(ReadTextFile(copy).find("\"path\":\"../original/sprites.bin\""),
            std::string::npos);
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(copy.string()).ok());
  ASSERT_EQ(reopened.sprite_assets.size(), 1u);
  EXPECT_EQ(reopened.sprite_assets[0].graphics_sources[0].path, bin.string());
  EXPECT_EQ(ReadTextFile(reopened.sprite_assets[0].graphics_sources[0].path),
            "original BIN bytes");
  EXPECT_EQ(reopened.sprite_assets[0].graphics_sources[1].path, "");
  EXPECT_EQ(reopened.sprite_assets[0].graphics_sources[1].offset, 0x300000u);
  EXPECT_EQ(reopened.sprite_assets[0].graphics_sources[2].path, "");

  core::LyraTerrainProfile profile;
  profile.tile_count = 4096;
  profile.tile16_definition_fingerprint = "definitions:fixture";
  profile.graphics_fingerprint = "graphics:fixture";
  profile.palette_fingerprint = "palette:fixture";
  profile.animation_fingerprint = "animation:fixture";
  auto document = core::LyraTerrainDocument::Create(profile, 11);
  ASSERT_TRUE(document.ok()) << document.status();
  ASSERT_TRUE(
      reopened
          .SaveAsWithLyraTerrain(lyra_copy.string(), *document, std::nullopt)
          .ok());
  YazeProject lyra_reopened;
  ASSERT_TRUE(lyra_reopened.Open(lyra_copy.string()).ok());
  ASSERT_EQ(lyra_reopened.sprite_assets.size(), 1u);
  EXPECT_EQ(lyra_reopened.sprite_assets[0].graphics_sources[0].path,
            bin.string());
  EXPECT_EQ(
      ReadTextFile(lyra_reopened.sprite_assets[0].graphics_sources[0].path),
      "original BIN bytes");
  EXPECT_EQ(ReadTextFile(copy.parent_path() / "sprites.bin"),
            "different BIN bytes");
}

TEST(ProjectPathsTest, LyraSaveAsOwnsUnpublishedTerrainAndPreservesAssetPaths) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_lyra_save_as"));
  const auto original = temp.path() / "original" / "Oracle.yaze";
  const auto original_terrain =
      original.parent_path() / "Oracle.lyra-terrain.json";
  const auto destination = temp.path() / "copy" / "Saved.yaze";
  const auto destination_terrain =
      destination.parent_path() / "Saved.lyra-terrain.json";
  std::filesystem::create_directories(destination.parent_path());
  const auto rom = original.parent_path() / "private.sfc";
  WriteTextFile(rom, "private ROM");
  const auto request = original.parent_path() / "requests" / "native.json";
  const std::string request_bytes = "source-bound request fixture\n";
  WriteTextFile(request, request_bytes);
  YazeProject project;
  project.name = "Oracle";
  project.filepath = original.string();
  project.rom_filename = rom.string();
  project.lyra_build_request_file = "requests/native.json";
  ASSERT_TRUE(project.Save().ok());
  const auto original_bytes = ReadTextFile(original);
  // Unsaved first-use project state must reach the copy, not the original.
  project.lyra_terrain_file = original_terrain.string();
  project.terrain_catalog_file = "labels.json";
  core::LyraTerrainProfile profile;
  profile.tile_count = 4096;
  profile.tile16_definition_fingerprint = "definitions:fixture";
  profile.graphics_fingerprint = "graphics:fixture";
  profile.palette_fingerprint = "palette:fixture";
  profile.animation_fingerprint = "animation:fixture";
  auto document = core::LyraTerrainDocument::Create(profile, 11);
  ASSERT_TRUE(document.ok()) << document.status();
  const auto status = project.SaveAsWithLyraTerrain(destination.string(),
                                                    *document, std::nullopt);
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(project.filepath, destination.string());
  EXPECT_EQ(project.lyra_terrain_file, destination_terrain.string());
  EXPECT_EQ(project.lyra_build_request_file, request.string());
  EXPECT_EQ(ReadTextFile(original), original_bytes);
  EXPECT_EQ(ReadTextFile(request), request_bytes);
  EXPECT_NE(
      ReadTextFile(destination)
          .find("lyra_build_request_file=../original/requests/native.json\n"),
      std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(destination.parent_path() / "requests" /
                                       "native.json"));
  EXPECT_FALSE(std::filesystem::exists(original_terrain));
  EXPECT_EQ(ReadTextFile(rom), "private ROM");
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(destination.string()).ok());
  EXPECT_EQ(reopened.lyra_terrain_file, destination_terrain.string());
  EXPECT_EQ(reopened.lyra_build_request_file, request.string());
  EXPECT_EQ(reopened.rom_filename, rom.string());
  EXPECT_EQ(reopened.terrain_catalog_file,
            (original.parent_path() / "labels.json").string());
  const auto saved = core::LyraTerrainDocument::Parse(
      ReadTextFile(destination_terrain), profile);
  ASSERT_TRUE(saved.ok()) << saved.status();
  EXPECT_EQ(saved->MapTile(0xBF, 31, 31).value(), 11);
}

TEST(ProjectPathsTest, LyraSaveAsRefusesDestinationThatIsBuildRequest) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_lyra_request_alias"));
  const auto original = temp.path() / "original" / "Oracle.yaze";
  const auto terrain = original.parent_path() / "Oracle.lyra-terrain.json";
  const auto destination = temp.path() / "copy" / "Saved.yaze";
  auto destination_terrain = destination;
  destination_terrain.replace_extension(".lyra-terrain.json");
  const std::string request_bytes =
      "{\"kind\":\"source-bound request fixture\"}\n";
  WriteTextFile(destination, request_bytes);
  std::filesystem::create_directories(original.parent_path());
  YazeProject project;
  project.name = "Oracle";
  project.filepath = original.string();
  project.lyra_build_request_file = destination.string();
  const auto save_status = project.Save();
  ASSERT_TRUE(save_status.ok()) << save_status;
  const auto original_bytes = ReadTextFile(original);
  project.lyra_terrain_file = terrain.string();
  core::LyraTerrainProfile profile;
  profile.tile_count = 4096;
  profile.tile16_definition_fingerprint = "definitions:fixture";
  profile.graphics_fingerprint = "graphics:fixture";
  profile.palette_fingerprint = "palette:fixture";
  profile.animation_fingerprint = "animation:fixture";
  auto document = core::LyraTerrainDocument::Create(profile, 11);
  ASSERT_TRUE(document.ok()) << document.status();
  const auto document_before = document->Serialize();

  const auto status = project.SaveAsWithLyraTerrain(destination.string(),
                                                    *document, std::nullopt);

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_EQ(project.filepath, original.string());
  EXPECT_EQ(project.lyra_terrain_file, terrain.string());
  EXPECT_EQ(project.lyra_build_request_file, destination.string());
  EXPECT_EQ(ReadTextFile(original), original_bytes);
  EXPECT_EQ(ReadTextFile(destination), request_bytes);
  EXPECT_EQ(document->Serialize(), document_before);
  EXPECT_FALSE(std::filesystem::exists(terrain));
  EXPECT_FALSE(std::filesystem::exists(destination_terrain));
}

TEST(ProjectPathsTest, LyraSaveAsRefusesNativeSaveReferenceDestinationAliases) {
  struct Reference {
    const char* name;
    std::string YazeProject::* path;
  };
  const std::vector<Reference> references = {
      {"producer", &YazeProject::native_save_producer_file},
      {"proof", &YazeProject::native_save_transfer_proof_file},
      {"receipt", &YazeProject::native_save_receipt_file}};
  for (const auto& reference : references) {
    for (bool targets_terrain : {false, true}) {
      for (bool hard_link : {false, true}) {
        SCOPED_TRACE(reference.name);
        SCOPED_TRACE(targets_terrain ? "terrain destination"
                                     : "project destination");
        SCOPED_TRACE(hard_link ? "hard-link alias" : "same-path alias");
        ScopedTempDir temp(
            MakeUniqueTempDir("yaze_native_save_reference_alias"));
        const auto original = temp.path() / "original" / "Oracle.yaze";
        const auto terrain =
            original.parent_path() / "Oracle.lyra-terrain.json";
        const auto destination = temp.path() / "copy" / "Saved.yaze";
        auto destination_terrain = destination;
        destination_terrain.replace_extension(".lyra-terrain.json");
        const auto target = targets_terrain ? destination_terrain : destination;
        const auto source = hard_link ? temp.path() / "protected.json" : target;
        const std::string control_bytes = "immutable native save control\n";
        WriteTextFile(source, control_bytes);
        std::filesystem::create_directories(original.parent_path());
        std::filesystem::create_directories(destination.parent_path());
        if (hard_link) {
          std::error_code error;
          std::filesystem::create_hard_link(source, target, error);
          ASSERT_FALSE(error) << error.message();
        }
        YazeProject project;
        project.name = "Native save reference alias";
        project.filepath = original.string();
        project.*(reference.path) = source.string();
        ASSERT_TRUE(project.Save().ok());
        const auto original_bytes = ReadTextFile(original);
        project.lyra_terrain_file = terrain.string();
        core::LyraTerrainProfile profile;
        profile.tile_count = 4096;
        profile.tile16_definition_fingerprint = "definitions:fixture";
        profile.graphics_fingerprint = "graphics:fixture";
        profile.palette_fingerprint = "palette:fixture";
        profile.animation_fingerprint = "animation:fixture";
        auto document = core::LyraTerrainDocument::Create(profile, 11);
        ASSERT_TRUE(document.ok()) << document.status();
        const auto document_before = document->Serialize();

        const auto status = project.SaveAsWithLyraTerrain(
            destination.string(), *document, std::nullopt);

        EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
        EXPECT_EQ(project.filepath, original.string());
        EXPECT_EQ(project.lyra_terrain_file, terrain.string());
        EXPECT_EQ(project.*(reference.path), source.string());
        EXPECT_EQ(ReadTextFile(original), original_bytes);
        EXPECT_EQ(ReadTextFile(source), control_bytes);
        EXPECT_EQ(ReadTextFile(target), control_bytes);
        EXPECT_EQ(document->Serialize(), document_before);
        EXPECT_FALSE(std::filesystem::exists(terrain));
        EXPECT_FALSE(std::filesystem::exists(
            targets_terrain ? destination : destination_terrain));
        if (hard_link) {
          EXPECT_TRUE(std::filesystem::equivalent(source, target));
        }
      }
    }
  }
}

TEST(ProjectPathsTest,
     LyraSaveAsCopiesFromReadOnlyDirectoryWithoutReplacingOriginalDescriptor) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_lyra_save_as_read_only"));
  const auto original = temp.path() / "original" / "Oracle.yaze";
  const auto terrain = original.parent_path() / "Oracle.lyra-terrain.json";
  const auto alias = temp.path() / "original-alias.yaze";
  const auto destination = temp.path() / "copy" / "Saved.yaze";
  std::filesystem::create_directories(destination.parent_path());
  const auto rom = original.parent_path() / "private.sfc";
  WriteTextFile(rom, "private ROM");
  YazeProject project;
  project.name = "Oracle";
  project.filepath = original.string();
  project.rom_filename = rom.string();
  ASSERT_TRUE(project.Save().ok());
  const auto original_bytes = ReadTextFile(original);
  std::error_code ec;
  std::filesystem::create_hard_link(original, alias, ec);
  if (ec) {
    GTEST_SKIP() << "Hard links are unavailable: " << ec.message();
  }
  project.lyra_terrain_file = terrain.string();
  core::LyraTerrainProfile profile;
  profile.tile_count = 4096;
  profile.tile16_definition_fingerprint = "definitions:fixture";
  profile.graphics_fingerprint = "graphics:fixture";
  profile.palette_fingerprint = "palette:fixture";
  profile.animation_fingerprint = "animation:fixture";
  auto document = core::LyraTerrainDocument::Create(profile, 11);
  ASSERT_TRUE(document.ok()) << document.status();
  const ScopedReadOnlyDirectory read_only(original.parent_path());
  const auto status = project.SaveAsWithLyraTerrain(destination.string(),
                                                    *document, std::nullopt);
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_TRUE(std::filesystem::equivalent(original, alias));
  EXPECT_EQ(ReadTextFile(original), original_bytes);
  EXPECT_EQ(ReadTextFile(alias), original_bytes);
  EXPECT_FALSE(std::filesystem::exists(terrain));
  EXPECT_FALSE(std::filesystem::exists(original.parent_path() /
                                       ".yaze-message-source-sync.lock"));
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(destination.string()).ok());
  EXPECT_EQ(ReadTextFile(reopened.lyra_terrain_file), document->Serialize());
}

TEST(ProjectPathsTest, LyraSaveAsRefusesOccupiedAliasedAndStaleSources) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_lyra_save_as_refusal"));
  const auto original = temp.path() / "Oracle.yaze";
  const auto terrain = temp.path() / "Oracle.lyra-terrain.json";
  const auto destination = temp.path() / "copy" / "Saved.yaze";
  const auto destination_terrain =
      destination.parent_path() / "Saved.lyra-terrain.json";
  const auto rom = temp.path() / "private.sfc";
  WriteTextFile(rom, "private ROM");
  WriteTextFile(destination, "unrelated project");
  YazeProject project;
  project.name = "Oracle";
  project.filepath = original.string();
  project.rom_filename = rom.string();
  project.lyra_terrain_file = terrain.string();
  ASSERT_TRUE(project.Save().ok());
  const auto original_bytes = ReadTextFile(original);
  core::LyraTerrainProfile profile;
  profile.tile_count = 4096;
  profile.tile16_definition_fingerprint = "definitions:fixture";
  profile.graphics_fingerprint = "graphics:fixture";
  profile.palette_fingerprint = "palette:fixture";
  profile.animation_fingerprint = "animation:fixture";
  auto document = core::LyraTerrainDocument::Create(profile, 11);
  ASSERT_TRUE(document.ok()) << document.status();
  const auto baseline = document->Serialize();
  WriteTextFile(terrain, baseline);
  ASSERT_TRUE(document->BeginStroke().ok());
  ASSERT_TRUE(document->Paint(3, 4, 23).ok());
  ASSERT_TRUE(document->EndStroke().ok());
  const auto draft = document->Serialize();
  auto expect_refused = [&]() {
    const auto status = project.SaveAsWithLyraTerrain(destination.string(),
                                                      *document, baseline);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(project.filepath, original.string());
    EXPECT_EQ(project.lyra_terrain_file, terrain.string());
    EXPECT_EQ(ReadTextFile(original), original_bytes);
    EXPECT_EQ(ReadTextFile(destination), "unrelated project");
    EXPECT_EQ(ReadTextFile(rom), "private ROM");
    EXPECT_EQ(document->Serialize(), draft);
    EXPECT_TRUE(document->dirty());
  };
  WriteTextFile(destination_terrain, "unrelated terrain");
  expect_refused();
  EXPECT_EQ(ReadTextFile(destination_terrain), "unrelated terrain");
  std::filesystem::remove(destination_terrain);
  std::error_code link_error;
  std::filesystem::create_hard_link(terrain, destination_terrain, link_error);
  ASSERT_FALSE(link_error) << link_error.message();
  expect_refused();
  EXPECT_EQ(ReadTextFile(terrain), baseline);
  std::filesystem::remove(destination_terrain);
  WriteTextFile(terrain, "changed outside this editor");
  expect_refused();
  EXPECT_FALSE(std::filesystem::exists(destination_terrain));
  EXPECT_EQ(ReadTextFile(terrain), "changed outside this editor");
  WriteTextFile(terrain, baseline);
  const auto status =
      project.SaveAsWithLyraTerrain(destination.string(), *document, baseline);
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(ReadTextFile(terrain), baseline);
  EXPECT_EQ(ReadTextFile(original), original_bytes);
  EXPECT_EQ(ReadTextFile(destination_terrain), draft);
}

TEST(ProjectPathsTest, TerrainCatalogPathSurvivesSaveAndResetsOnNextProject) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_terrain_project"));
  const auto project_file = temp.path() / "Terrain.yaze";
  const auto catalog_file = temp.path() / "terrain.json";
  WriteTextFile(catalog_file, "{}");
  YazeProject project;
  project.name = "Terrain";
  project.filepath = project_file.string();
  project.terrain_catalog_file = catalog_file.string();
  ASSERT_TRUE(project.Save().ok());
  EXPECT_NE(
      ReadTextFile(project_file).find("terrain_catalog_file=terrain.json"),
      std::string::npos);
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(project_file.string()).ok());
  EXPECT_EQ(reopened.terrain_catalog_file, catalog_file.string());
  ASSERT_TRUE(reopened
                  .LoadFromString("[project]\nname=Other\n",
                                  (temp.path() / "Other.yaze").string())
                  .ok());
  EXPECT_TRUE(reopened.terrain_catalog_file.empty());
}

TEST(ProjectPathsTest, PaletteCatalogPathSurvivesSaveAndResetsOnNextProject) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_palette_project"));
  const auto project_file = temp.path() / "Oracle.yaze";
  const auto catalog_file = temp.path() / "palette.json";
  WriteTextFile(catalog_file, "{}");
  YazeProject project;
  project.name = "Oracle";
  project.filepath = project_file.string();
  project.palette_catalog_file = catalog_file.string();
  ASSERT_TRUE(project.Save().ok());
  EXPECT_NE(
      ReadTextFile(project_file).find("palette_catalog_file=palette.json"),
      std::string::npos);
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(project_file.string()).ok());
  EXPECT_EQ(reopened.palette_catalog_file, catalog_file.string());
  ASSERT_TRUE(reopened
                  .LoadFromString("[project]\nname=Other\n",
                                  (temp.path() / "Other.yaze").string())
                  .ok());
  EXPECT_TRUE(reopened.palette_catalog_file.empty());
}

TEST(ProjectPathsTest, DungeonPatternPathSurvivesSaveAndResetsOnNextProject) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_dungeon_patterns_project"));
  const auto project_file = temp.path() / "Patterns.yaze";
  const auto patterns_file = temp.path() / "walls.asm";
  WriteTextFile(patterns_file, "org $009E3A\ndw $4890\n");
  YazeProject project;
  project.name = "Patterns";
  project.filepath = project_file.string();
  project.dungeon_tile_patterns = patterns_file.string();
  ASSERT_TRUE(project.Save().ok());
  EXPECT_NE(ReadTextFile(project_file).find("dungeon_tile_patterns=walls.asm"),
            std::string::npos);
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(project_file.string()).ok());
  EXPECT_EQ(reopened.dungeon_tile_patterns, patterns_file.string());
  ASSERT_TRUE(reopened
                  .LoadFromString("[project]\nname=Other\n",
                                  (temp.path() / "Other.yaze").string())
                  .ok());
  EXPECT_TRUE(reopened.dungeon_tile_patterns.empty());
}

TEST(ProjectPathsTest, OpenNormalizesRelativePathsToAbsolute) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_paths"));

  const auto rom_path = temp.path() / "roms" / "test.sfc";
  const auto code_path = temp.path() / "code";
  const auto backup_path = temp.path() / "backups";
  const auto output_path = temp.path() / "output";
  const auto labels_path = temp.path() / "labels.txt";
  const auto symbols_path = temp.path() / "symbols.txt";

  std::filesystem::create_directories(rom_path.parent_path());
  std::filesystem::create_directories(code_path);
  std::filesystem::create_directories(backup_path);
  std::filesystem::create_directories(output_path);
  WriteTextFile(rom_path, "not a real rom");

  const auto project_file = temp.path() / "TestProject.yaze";
  WriteTextFile(project_file,
                R"(
[project]
name=TestProject

[files]
rom_filename=roms/test.sfc
code_folder=code
rom_backup_folder=backups
output_folder=output
labels_filename=labels.txt
symbols_filename=symbols.txt
)");

  YazeProject project;
  ASSERT_TRUE(project.Open(project_file.string()).ok());

  EXPECT_EQ(project.rom_filename, rom_path.string());
  EXPECT_EQ(project.code_folder, code_path.string());
  EXPECT_EQ(project.rom_backup_folder, backup_path.string());
  EXPECT_EQ(project.output_folder, output_path.string());
  EXPECT_EQ(project.labels_filename, labels_path.string());
  EXPECT_EQ(project.symbols_filename, symbols_path.string());

  ASSERT_TRUE(project.Save().ok());
  const auto saved = ReadTextFile(project_file);

  // Serialized project files should remain portable (relative paths).
  EXPECT_NE(saved.find("rom_filename=roms/test.sfc"), std::string::npos);
  EXPECT_NE(saved.find("code_folder=code"), std::string::npos);
  EXPECT_NE(saved.find("rom_backup_folder=backups"), std::string::npos);
}

TEST(ProjectPathsTest, WaterFillSaveScopeRoundTripsThroughIni) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_water_fill_save_scope"));
  const auto project_file = temp.path() / "WaterFillScope.yaze";

  YazeProject project;
  project.filepath = project_file.string();
  project.name = "Water Fill Save Scope";
  project.feature_flags.dungeon.kSaveWaterFillZones = false;
  ASSERT_TRUE(project.Save().ok());

  const auto saved = ReadTextFile(project_file);
  EXPECT_NE(saved.find("save_dungeon_water_fill_zones=false"),
            std::string::npos);

  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(project_file.string()).ok());
  EXPECT_FALSE(reopened.feature_flags.dungeon.kSaveWaterFillZones);
}

TEST(ProjectPathsTest, CrLfStandaloneMatchesLfAndPreservesFeatureFlags) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_crlf_standalone"));
  const std::string descriptor = R"([project]
name=Cross Platform Project

[files]
rom_filename=rom.sfc

[feature_flags]
save_dungeon_objects=false
save_dungeon_water_fill_zones=false
enable_custom_objects=true
)";

  const auto lf_file = temp.path() / "Lf.yaze";
  const auto crlf_file = temp.path() / "CrLf.yaze";
  WriteTextFile(lf_file, descriptor);
  WriteTextFile(crlf_file, WithCrLf(descriptor));

  YazeProject lf_project;
  YazeProject crlf_project;
  ASSERT_TRUE(lf_project.Open(lf_file.string()).ok());
  ASSERT_TRUE(crlf_project.Open(crlf_file.string()).ok());

  EXPECT_EQ(crlf_project.name, lf_project.name);
  EXPECT_EQ(crlf_project.rom_filename, lf_project.rom_filename);
  EXPECT_FALSE(crlf_project.feature_flags.dungeon.kSaveObjects);
  EXPECT_FALSE(crlf_project.feature_flags.dungeon.kSaveWaterFillZones);
  EXPECT_TRUE(crlf_project.feature_flags.kEnableCustomObjects);
}

TEST(ProjectPathsTest, CrLfBundlePreservesWaterFillSaveScope) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_crlf_bundle"));
  const auto bundle = temp.path() / "Portable.yazeproj";
  std::filesystem::create_directories(bundle);
  WriteTextFile(bundle / "rom", "not a real rom");
  WriteTextFile(bundle / "project.yaze", WithCrLf(R"([project]
name=Portable Project

[files]
rom_filename=rom

[feature_flags]
save_dungeon_water_fill_zones=false
)"));

  YazeProject project;
  ASSERT_TRUE(project.Open(bundle.string()).ok());
  EXPECT_EQ(project.name, "Portable Project");
  EXPECT_EQ(project.rom_filename, (bundle / "rom").string());
  EXPECT_FALSE(project.feature_flags.dungeon.kSaveWaterFillZones);
}

TEST(ProjectPathsTest, LoneCarriageReturnSeparatorsFailExplicitly) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_lone_cr"));
  const std::vector<std::string> descriptors = {
      "[project]\rname=Unsafe\r[feature_flags]"
      "\rsave_dungeon_water_fill_zones=false\r",
      "[project]\r\nname=Unsafe\r",
  };
  for (const auto& descriptor : descriptors) {
    SCOPED_TRACE(descriptor);
    YazeProject project;
    const auto status = project.LoadFromString(
        descriptor, (temp.path() / "Unsafe.yaze").string());

    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_NE(status.message().find("lone carriage returns"),
              std::string::npos);
  }
}

// The reader refuses a lone CR, so the writer must refuse to emit one.
// Values are streamed verbatim — metadata.description and resource labels
// injected from hack_manifest.json among them — so a lone CR pasted from an
// old-Mac source would otherwise round-trip through Save() and leave the
// project permanently unopenable, with the previous good file already
// overwritten. Driven through Save() rather than SerializeToString(), which
// is private, and because the property that matters is that the existing
// descriptor survives the refusal.
TEST(ProjectPathsTest, SaveRefusesToWriteALoneCarriageReturn) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_cr_writer"));
  const auto project_file = temp.path() / "CrWriter.yaze";

  YazeProject good;
  good.filepath = project_file.string();
  good.name = "Good Project";
  good.rom_filename = (temp.path() / "rom.sfc").string();
  ASSERT_TRUE(good.Save().ok());
  const std::string original = ReadTextFile(project_file);
  ASSERT_NE(original.find("Good Project"), std::string::npos);

  YazeProject poisoned = good;
  poisoned.name = "Carriage Return\rInjected";
  const auto status = poisoned.Save();

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(status.message().find("carriage return"), std::string::npos)
      << status.message();
  // The refusal must not have consumed the good descriptor.
  EXPECT_EQ(ReadTextFile(project_file), original);
}

// CRLF inside a value is fine in both directions: the reader normalizes it.
TEST(ProjectPathsTest, SaveAcceptsCrLfInsideAValue) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_crlf_writer"));
  YazeProject project;
  project.filepath = (temp.path() / "CrLfWriter.yaze").string();
  project.name = "Windows\r\nStyle";
  project.rom_filename = (temp.path() / "rom.sfc").string();

  EXPECT_TRUE(project.Save().ok());
}

TEST(ProjectPathsTest, CustomObjectSubtypeSlotsRoundTripThroughIni) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_custom_object_slots"));
  const auto project_file = temp.path() / "CustomObjectSlots.yaze";

  YazeProject project;
  project.filepath = project_file.string();
  project.name = "Custom Object Slots";
  project.custom_object_files = {
      {0x31, {"", "", "track_corner.bin", "", "track_floor.bin"}},
      {0x32, {"furnace.bin", "", "chair.bin"}},
  };

  ASSERT_TRUE(project.Save().ok());
  const std::string serialized = ReadTextFile(project_file);
  EXPECT_NE(serialized.find("object_0x31=,,track_corner.bin,,track_floor.bin"),
            std::string::npos);
  EXPECT_NE(serialized.find("object_0x32=furnace.bin,,chair.bin"),
            std::string::npos);

  YazeProject reopened;
  ASSERT_TRUE(reopened.LoadFromString(serialized, project_file.string()).ok());
  EXPECT_EQ(reopened.custom_object_files, project.custom_object_files);
}

#ifndef __EMSCRIPTEN__
namespace {

std::filesystem::path NextProjectSaveStagingPath(
    const std::filesystem::path& target) {
  // A failed create reports its staging path and consumes one sequence number.
  // Probe a missing parent to observe that number without creating a file or
  // depending on how many project saves earlier tests performed.
  const auto probe = target.parent_path() / "missing-parent" / "probe.yaze";
  const auto status = WriteProjectFileAtomically(probe.string(), "probe", true);
  EXPECT_FALSE(status.ok());
  const std::string message(status.message());
  const std::string prefix = probe.string() + ".tmp.";
  const size_t prefix_at = message.find(prefix);
  if (status.ok() || prefix_at == std::string::npos) {
    ADD_FAILURE() << "Missing staging path in create failure: " << status;
    return {};
  }
  const size_t pid_at = prefix_at + prefix.size();
  const size_t separator = message.find('.', pid_at);
  if (separator == std::string::npos) {
    ADD_FAILURE() << "Missing staging sequence in create failure: " << status;
    return {};
  }
  uint64_t sequence = 0;
  const auto parsed =
      std::from_chars(message.data() + separator + 1,
                      message.data() + message.size(), sequence);
  if (parsed.ec != std::errc()) {
    ADD_FAILURE() << "Invalid staging sequence in create failure: " << status;
    return {};
  }
  auto staging_path = target;
  staging_path += ".tmp." + message.substr(pid_at, separator - pid_at) + "." +
                  std::to_string(sequence + 1);
  return staging_path;
}

}  // namespace

TEST(ProjectPathsTest, AtomicWriterPreservesPreexistingRegularStagingFile) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_regular_stage"));
  const auto target = temp.path() / "Project.yaze";
  WriteTextFile(target, "original descriptor");

  for (const bool replace_existing : {false, true}) {
    SCOPED_TRACE(replace_existing);
    const auto staging_path = NextProjectSaveStagingPath(target);
    ASSERT_FALSE(staging_path.empty());
    WriteTextFile(staging_path, "foreign staging contents");

    const auto status = WriteProjectFileAtomically(
        target.string(), "replacement descriptor", replace_existing);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(ReadTextFile(target), "original descriptor");
    ASSERT_TRUE(std::filesystem::is_regular_file(staging_path));
    EXPECT_EQ(ReadTextFile(staging_path), "foreign staging contents");
  }
}

TEST(ProjectPathsTest, AtomicWriterPreservesPreexistingSymlinkStagingFile) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_symlink_stage"));
  const auto target = temp.path() / "Project.yaze";
  const auto victim = temp.path() / "unrelated.txt";
  WriteTextFile(target, "original descriptor");
  WriteTextFile(victim, "unrelated contents");

  for (const bool replace_existing : {false, true}) {
    SCOPED_TRACE(replace_existing);
    const auto staging_path = NextProjectSaveStagingPath(target);
    ASSERT_FALSE(staging_path.empty());
    std::error_code error;
    std::filesystem::create_symlink(victim, staging_path, error);
#if defined(_WIN32)
    if (error == std::errc::permission_denied ||
        error == std::errc::operation_not_permitted) {
      GTEST_SKIP()
          << "Cannot create staging symlink without Windows privilege: "
          << error.message();
    }
#endif
    ASSERT_FALSE(error) << error.message();

    const auto status = WriteProjectFileAtomically(
        target.string(), "replacement descriptor", replace_existing);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(ReadTextFile(target), "original descriptor");
    EXPECT_EQ(ReadTextFile(victim), "unrelated contents");
    ASSERT_TRUE(std::filesystem::is_symlink(staging_path));
    EXPECT_EQ(std::filesystem::read_symlink(staging_path), victim);
  }
}

TEST(ProjectPathsTest, AtomicWriterPublishesNewAndReplacementWithoutStage) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_writer_publication"));

  for (const bool replace_existing : {false, true}) {
    SCOPED_TRACE(replace_existing);
    const auto target =
        temp.path() / (replace_existing ? "Existing.yaze" : "New.yaze");
    if (replace_existing) {
      WriteTextFile(target, "original descriptor");
    }
    const auto staging_path = NextProjectSaveStagingPath(target);
    ASSERT_FALSE(staging_path.empty());

    const auto status = WriteProjectFileAtomically(
        target.string(), "published descriptor", replace_existing);

    ASSERT_TRUE(status.ok()) << status;
    EXPECT_EQ(ReadTextFile(target), "published descriptor");
    EXPECT_FALSE(std::filesystem::exists(staging_path));
  }
}

TEST(ProjectPathsTest, AtomicWriterNoReplaceFailureCleansOnlyOwnedStage) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_owned_stage_cleanup"));
  const auto target = temp.path() / "Existing.yaze";
  WriteTextFile(target, "original descriptor");
  const auto staging_path = NextProjectSaveStagingPath(target);
  ASSERT_FALSE(staging_path.empty());
  auto foreign_path = staging_path;
  foreign_path += ".foreign";
  WriteTextFile(foreign_path, "foreign staging contents");

  const auto status = WriteProjectFileAtomically(
      target.string(), "replacement descriptor", false);

  EXPECT_EQ(status.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_EQ(ReadTextFile(target), "original descriptor");
  EXPECT_FALSE(std::filesystem::exists(staging_path));
  EXPECT_EQ(ReadTextFile(foreign_path), "foreign staging contents");
}

TEST(ProjectPathsTest, SaveAtomicallyReplacesExistingDescriptor) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_atomic_save"));
  const auto project_file = temp.path() / "Atomic.yaze";
  WriteTextFile(project_file, "old descriptor contents");

  YazeProject project;
  project.filepath = project_file.string();
  project.name = "Atomic Replacement";
  ASSERT_TRUE(project.Save().ok());

  const auto saved = ReadTextFile(project_file);
  EXPECT_EQ(saved.find("old descriptor contents"), std::string::npos);
  EXPECT_NE(saved.find("name=Atomic Replacement"), std::string::npos);

  const std::string temp_prefix = project_file.filename().string() + ".tmp.";
  for (const auto& entry : std::filesystem::directory_iterator(temp.path())) {
    EXPECT_NE(entry.path().filename().string().rfind(temp_prefix, 0), 0u)
        << "Temporary project file was not cleaned up: "
        << entry.path().string();
  }
}

TEST(ProjectPathsTest, SaveNewNeverReplacesExistingDescriptor) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_exclusive_save"));
  const auto project_file = temp.path() / "Exclusive.yaze";

  YazeProject original;
  original.filepath = project_file.string();
  original.name = "Original Project";
  ASSERT_TRUE(original.SaveNew().ok());
  const std::string original_contents = ReadTextFile(project_file);

  YazeProject competing;
  competing.filepath = project_file.string();
  competing.name = "Competing Project";
  const auto status = competing.SaveNew();

  EXPECT_EQ(status.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_EQ(ReadTextFile(project_file), original_contents);
}

TEST(ProjectPathsTest, SaveFailurePreservesExistingTarget) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_atomic_failure"));
  const auto project_file = temp.path() / "Blocked.yaze";
  std::filesystem::create_directories(project_file);
  const auto marker = project_file / "original.txt";
  WriteTextFile(marker, "original target");

  YazeProject project;
  project.filepath = project_file.string();
  project.name = "Cannot Replace";
  const auto status = project.Save();

  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(std::filesystem::is_directory(project_file));
  EXPECT_EQ(ReadTextFile(marker), "original target");

  const std::string temp_prefix = project_file.filename().string() + ".tmp.";
  for (const auto& entry : std::filesystem::directory_iterator(temp.path())) {
    EXPECT_NE(entry.path().filename().string().rfind(temp_prefix, 0), 0u)
        << "Temporary project file was not cleaned up: "
        << entry.path().string();
  }
}
#endif

TEST(ProjectPathsTest, OpenLoadsZ3dkConfigWhenPresent) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_z3dk_paths"));

  const auto code_path = temp.path() / "code";
  const auto output_path = temp.path() / "output";
  const auto rom_path = temp.path() / "roms" / "base.sfc";
  std::filesystem::create_directories(code_path / "include");
  std::filesystem::create_directories(code_path / "asm");
  std::filesystem::create_directories(code_path / "build");
  std::filesystem::create_directories(code_path / "logs");
  std::filesystem::create_directories(code_path / "toolchain");
  std::filesystem::create_directories(output_path);
  std::filesystem::create_directories(rom_path.parent_path());
  WriteTextFile(rom_path, "not a real rom");

  WriteTextFile(code_path / "z3dk.toml",
                R"(
preset = "oracle"
include_paths = ["include", "asm"]
defines = ["FEATURE=1", "DEBUG"]
main_files = ["Main.asm", "Entry.asm"]
emit = ["build/hooks.json", "build/lint.json"]
std_includes = "toolchain/stdincludes.txt"
std_defines = "toolchain/stddefines.txt"
mapper = "lorom"
rom = "../roms/base.sfc"
rom_size = 1048576
symbols = "nocash"
symbols_path = "build/project.sym"
lsp_log_enabled = true
lsp_log_path = "logs/z3lsp.log"
warn_unused_symbols = true
warn_branch_outside_bank = false
warn_unknown_width = true
warn_org_collision = true
warn_unauthorized_hook = false
)");

  const auto project_file = temp.path() / "Z3dkProject.yaze";
  WriteTextFile(project_file,
                R"(
[project]
name=Z3dkProject

[files]
code_folder=code
output_folder=output
)");

  YazeProject project;
  ASSERT_TRUE(project.Open(project_file.string()).ok());

#ifdef YAZE_WITH_Z3DK
  ASSERT_TRUE(project.HasZ3dkConfig());
  EXPECT_EQ(project.z3dk_settings.config_path,
            (code_path / "z3dk.toml").string());
  EXPECT_EQ(project.z3dk_settings.preset, "oracle");
  ASSERT_EQ(project.z3dk_settings.include_paths.size(), 2u);
  EXPECT_EQ(project.z3dk_settings.include_paths[0],
            (code_path / "include").string());
  EXPECT_EQ(project.z3dk_settings.include_paths[1],
            (code_path / "asm").string());
  ASSERT_EQ(project.z3dk_settings.defines.size(), 2u);
  EXPECT_EQ(project.z3dk_settings.defines[0].first, "FEATURE");
  EXPECT_EQ(project.z3dk_settings.defines[0].second, "1");
  EXPECT_EQ(project.z3dk_settings.defines[1].first, "DEBUG");
  EXPECT_EQ(project.z3dk_settings.defines[1].second, "1");
  ASSERT_EQ(project.z3dk_settings.main_files.size(), 2u);
  EXPECT_EQ(project.z3dk_settings.main_files[0],
            (code_path / "Main.asm").string());
  EXPECT_EQ(project.z3dk_settings.main_files[1],
            (code_path / "Entry.asm").string());
  ASSERT_EQ(project.z3dk_settings.emits.size(), 2u);
  EXPECT_EQ(project.z3dk_settings.emits[0],
            (code_path / "build" / "hooks.json").string());
  EXPECT_EQ(project.z3dk_settings.emits[1],
            (code_path / "build" / "lint.json").string());
  EXPECT_EQ(project.z3dk_settings.std_includes_path,
            (code_path / "toolchain" / "stdincludes.txt").string());
  EXPECT_EQ(project.z3dk_settings.std_defines_path,
            (code_path / "toolchain" / "stddefines.txt").string());
  EXPECT_EQ(project.z3dk_settings.mapper, "lorom");
  EXPECT_EQ(project.z3dk_settings.rom_path, rom_path.string());
  EXPECT_EQ(project.z3dk_settings.rom_size, 1048576);
  EXPECT_EQ(project.z3dk_settings.symbols_format, "nocash");
  EXPECT_EQ(project.z3dk_settings.symbols_path,
            (code_path / "build" / "project.sym").string());
  ASSERT_TRUE(project.z3dk_settings.lsp_log_enabled.has_value());
  EXPECT_TRUE(*project.z3dk_settings.lsp_log_enabled);
  EXPECT_EQ(project.z3dk_settings.lsp_log_path,
            (code_path / "logs" / "z3lsp.log").string());
  EXPECT_EQ(project.z3dk_settings.artifact_paths.symbols_mlb,
            (output_path / "symbols.mlb").string());
  EXPECT_EQ(project.z3dk_settings.artifact_paths.sourcemap_json,
            (output_path / "sourcemap.json").string());
  EXPECT_EQ(project.z3dk_settings.artifact_paths.annotations_json,
            (output_path / "annotations.json").string());
  EXPECT_EQ(project.z3dk_settings.artifact_paths.hooks_json,
            (code_path / "build" / "hooks.json").string());
  EXPECT_EQ(project.z3dk_settings.artifact_paths.lint_json,
            (code_path / "build" / "lint.json").string());
  EXPECT_FALSE(project.z3dk_settings.warn_branch_outside_bank);
  EXPECT_FALSE(project.z3dk_settings.warn_unauthorized_hook);
#else
  EXPECT_FALSE(project.HasZ3dkConfig());
#endif
}

TEST(ProjectPathsTest, OpenInjectsOracleDungeonRoomLabelsIntoProjectFile) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_oracle_labels"));

  const auto planning = temp.path() / "Docs" / "Dev" / "Planning";
  std::filesystem::create_directories(planning);
  WriteTextFile(planning / "dungeons.json", R"json({
    "dungeons": [
      {
        "id": "D4",
        "name": "Zora Temple",
        "rooms": [
          {"id": "0x25", "name": "Water Grate"}
        ]
      }
    ]
  })json");

  const auto project_file = temp.path() / "Oracle.yaze";
  WriteTextFile(project_file,
                R"(
[project]
name=Oracle of Secrets

[files]
code_folder=Core
hack_manifest_file=hack_manifest.json

[labels_room]
37=Thieves' Town

[labels_room_override]
37=Flooded Hall
)");

  {
    ScopedCurrentPath current_path(temp.path());
    const auto relative_project_file = project_file.filename();
    ASSERT_FALSE(relative_project_file.is_absolute());

    YazeProject project;
    ASSERT_TRUE(project.Open(relative_project_file.string()).ok());
    EXPECT_TRUE(std::filesystem::path(project.filepath).is_absolute());
    EXPECT_FALSE(project.hack_manifest.loaded());
    ASSERT_TRUE(project.hack_manifest.HasProjectRegistry());

    ASSERT_TRUE(project.resource_labels.contains("room"));
    EXPECT_EQ(project.resource_labels["room"]["37"], "Water Grate");
    EXPECT_EQ(project.resource_labels["room_override"]["37"], "Flooded Hall");

    ASSERT_TRUE(project.Save().ok());
  }
  const auto saved = ReadTextFile(project_file);
  EXPECT_NE(saved.find("[labels_room]"), std::string::npos);
  EXPECT_NE(saved.find("37=Water Grate"), std::string::npos);
  EXPECT_EQ(saved.find("37=Thieves' Town"), std::string::npos);
  EXPECT_NE(saved.find("[labels_room_override]"), std::string::npos);
  EXPECT_NE(saved.find("37=Flooded Hall"), std::string::npos);
}

TEST(ProjectPathsTest, ExplicitMissingHackManifestDoesNotUseFallback) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_manifest_paths"));

  WriteTextFile(temp.path() / "rom.sfc", "not a real rom");
  WriteTextFile(temp.path() / "hack_manifest.json", "{}");

  const auto project_file = temp.path() / "Oracle.yaze";
  WriteTextFile(project_file,
                R"(
[project]
name=Oracle of Secrets

[files]
rom_filename=rom.sfc
hack_manifest_file=manifests/missing.json
)");

  YazeProject project;
  ASSERT_TRUE(project.Open(project_file.string()).ok());

  const auto missing_manifest = temp.path() / "manifests" / "missing.json";
  EXPECT_EQ(project.hack_manifest_file, missing_manifest.string());
  EXPECT_FALSE(project.hack_manifest.loaded());

  const auto validation = project.Validate();
  EXPECT_FALSE(validation.ok());
  EXPECT_NE(validation.message().find("Hack manifest file does not exist"),
            std::string::npos);

  const auto missing_files = project.GetMissingFiles();
  EXPECT_NE(std::find(missing_files.begin(), missing_files.end(),
                      missing_manifest.string()),
            missing_files.end());
}

TEST(ProjectPathsTest, ExplicitMalformedHackManifestFailsValidation) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_malformed_manifest"));

  WriteTextFile(temp.path() / "rom.sfc", "not a real rom");
  WriteTextFile(temp.path() / "manifests" / "broken.json", "{not-json");
  WriteTextFile(temp.path() / "hack_manifest.json", "{}");

  const auto project_file = temp.path() / "Oracle.yaze";
  WriteTextFile(project_file,
                R"(
[project]
name=Oracle of Secrets

[files]
rom_filename=rom.sfc
hack_manifest_file=manifests/broken.json
)");

  YazeProject project;
  ASSERT_TRUE(project.Open(project_file.string()).ok());
  EXPECT_FALSE(project.hack_manifest.loaded());

  const auto validation = project.Validate();
  EXPECT_FALSE(validation.ok());
  EXPECT_NE(validation.message().find("Hack manifest file failed to load"),
            std::string::npos);
}

// Regression (2026-09-28): Open() does not make these keys absolute, and the
// serializer passed the relative value straight to GetRelativePath, which
// resolves it against the process working directory. Saving from any other
// directory rewrote Oracle's custom_collision_json=Data/dungeons/... into a
// path relative to the working directory.
TEST(ProjectPathsTest, RelativeFileKeysSurviveSaveFromAnotherDirectory) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_relative_keys"));
  ScopedTempDir elsewhere(MakeUniqueTempDir("yaze_project_relative_cwd"));
  const auto project_file = temp.path() / "Oracle.yaze";
  WriteTextFile(project_file,
                R"(
[project]
name=Relative Keys

[files]
custom_collision_json=Data/dungeons/custom_collision.json
cutscene_shots=Data/cutscenes/shots.json

[agent_settings]
custom_system_prompt=.gemini/yaze_agent_prompt.md
)");

  {
    ScopedCurrentPath current_path(elsewhere.path());
    YazeProject project;
    ASSERT_TRUE(project.Open(project_file.string()).ok());
    EXPECT_EQ(project.GetAbsolutePath(project.custom_collision_json),
              (temp.path() / "Data" / "dungeons" / "custom_collision.json")
                  .lexically_normal()
                  .make_preferred()
                  .string());
    ASSERT_TRUE(project.Save().ok());
  }
  const auto saved = ReadTextFile(project_file);
  EXPECT_NE(
      saved.find("custom_collision_json=Data/dungeons/custom_collision.json\n"),
      std::string::npos)
      << saved;
  EXPECT_NE(saved.find("cutscene_shots=Data/cutscenes/shots.json\n"),
            std::string::npos)
      << saved;
  EXPECT_NE(saved.find("custom_system_prompt=.gemini/yaze_agent_prompt.md\n"),
            std::string::npos)
      << saved;
}

}  // namespace yaze::project
