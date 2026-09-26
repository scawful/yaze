#include "app/editor/system/session/collision_source_pairing.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "unique_temp_path.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/track_collision_generator.h"

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace yaze::editor {
namespace {

namespace fs = std::filesystem;

constexpr int kRoom = 0x25;

std::string ReadFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

void WriteFile(const fs::path& path, const std::string& bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << bytes;
}

void SetCollisionTile(Rom* rom, int offset, uint8_t value) {
  zelda3::CustomCollisionMap map;
  map.has_data = true;
  map.tiles[static_cast<size_t>(offset)] = value;
  ASSERT_TRUE(zelda3::WriteTrackCollision(rom, kRoom, map).ok());
}

// A ROM file with collision in one room and the matching tracked JSON.
class CollisionSourcePairingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = ::yaze::test::UniqueTempPath("yaze_collision_pair", "");
    rom_path_ = root_ / "rom.sfc";
    json_path_ = root_ / "Data" / "dungeons" / "custom_collision.json";
    fs::create_directories(json_path_.parent_path());

    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
    SetCollisionTile(&rom_, 100, 0xB7);
    Rom::SaveSettings settings;
    settings.filename = rom_path_.string();
    ASSERT_TRUE(rom_.SaveToFile(settings).ok());
    rom_.set_filename(rom_path_.string());
    auto json = zelda3::DumpCustomCollisionSourceFromRom(&rom_);
    ASSERT_TRUE(json.ok());
    WriteFile(json_path_, *json);
  }

  void TearDown() override {
    std::error_code ec;
#if !defined(_WIN32)
    fs::permissions(json_path_.parent_path(), fs::perms::owner_all,
                    fs::perm_options::add, ec);
#endif
    fs::remove_all(root_, ec);
  }

  fs::path root_;
  fs::path rom_path_;
  fs::path json_path_;
  Rom rom_;
};

TEST_F(CollisionSourcePairingTest, UnchangedCollisionLeavesJsonUntouched) {
  const std::string json_before = ReadFile(json_path_);
  const auto json_time = fs::last_write_time(json_path_);
  ASSERT_TRUE(rom_.WriteByte(0x100, 0x42).ok());

  ASSERT_TRUE(SaveRomWithCollisionSource(&rom_, json_path_).ok());

  EXPECT_EQ(ReadFile(json_path_), json_before);
  EXPECT_EQ(fs::last_write_time(json_path_), json_time);
  EXPECT_EQ(static_cast<uint8_t>(ReadFile(rom_path_)[0x100]), 0x42);
  EXPECT_FALSE(rom_.dirty());
}

TEST_F(CollisionSourcePairingTest, ChangedCollisionWritesBothFiles) {
  const std::string json_before = ReadFile(json_path_);
  SetCollisionTile(&rom_, 200, 0xB0);

  ASSERT_TRUE(SaveRomWithCollisionSource(&rom_, json_path_).ok());

  const std::string rom_bytes = ReadFile(rom_path_);
  EXPECT_EQ(rom_bytes, std::string(rom_.vector().begin(), rom_.vector().end()));
  auto expected = zelda3::DumpCustomCollisionSourceFromRom(&rom_);
  ASSERT_TRUE(expected.ok());
  EXPECT_EQ(ReadFile(json_path_), *expected);
  EXPECT_NE(ReadFile(json_path_), json_before);
  EXPECT_FALSE(rom_.dirty());
}

TEST_F(CollisionSourcePairingTest, MissingJsonIsCreated) {
  fs::remove(json_path_);
  SetCollisionTile(&rom_, 200, 0xB0);

  ASSERT_TRUE(SaveRomWithCollisionSource(&rom_, json_path_).ok());

  auto expected = zelda3::DumpCustomCollisionSourceFromRom(&rom_);
  ASSERT_TRUE(expected.ok());
  EXPECT_EQ(ReadFile(json_path_), *expected);
}

TEST_F(CollisionSourcePairingTest, JsonEditedOutsideYazeIsRefused) {
  // A valid source that no longer matches the ROM on disk.
  Rom other;
  ASSERT_TRUE(other.LoadFromData(std::vector<uint8_t>(0x200000, 0x00)).ok());
  SetCollisionTile(&other, 300, 0xB8);
  auto edited = zelda3::DumpCustomCollisionSourceFromRom(&other);
  ASSERT_TRUE(edited.ok());
  WriteFile(json_path_, *edited);
  const std::string rom_before = ReadFile(rom_path_);
  SetCollisionTile(&rom_, 200, 0xB0);
  rom_.set_dirty(true);

  const auto status = SaveRomWithCollisionSource(&rom_, json_path_);

  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_EQ(ReadFile(json_path_), *edited);
  EXPECT_EQ(ReadFile(rom_path_), rom_before);
  EXPECT_TRUE(rom_.dirty());
}

TEST_F(CollisionSourcePairingTest, FailedJsonWriteLeavesBothFilesUnchanged) {
#if defined(_WIN32)
  GTEST_SKIP() << "Uses POSIX directory permissions";
#else
  if (geteuid() == 0) {
    GTEST_SKIP() << "Root ignores directory permissions";
  }
  const std::string rom_before = ReadFile(rom_path_);
  const std::string json_before = ReadFile(json_path_);
  SetCollisionTile(&rom_, 200, 0xB0);
  rom_.set_dirty(true);
  fs::permissions(json_path_.parent_path(),
                  fs::perms::owner_read | fs::perms::owner_exec,
                  fs::perm_options::replace);

  const auto status = SaveRomWithCollisionSource(&rom_, json_path_);

  fs::permissions(json_path_.parent_path(), fs::perms::owner_all,
                  fs::perm_options::add);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ReadFile(rom_path_), rom_before);
  EXPECT_EQ(ReadFile(json_path_), json_before);
  EXPECT_TRUE(rom_.dirty());
#endif
}

// Local check against Oracle of Secrets' own build validator, on copies:
//   YAZE_ORACLE_ROOT=~/src/hobby/oracle-of-secrets
TEST(CollisionSourcePairingOracleTest, SavedPairPassesOracleValidator) {
  const char* oracle_env = std::getenv("YAZE_ORACLE_ROOT");
  if (oracle_env == nullptr) {
    GTEST_SKIP() << "Set YAZE_ORACLE_ROOT to run against Oracle copies";
  }
  const fs::path oracle(oracle_env);
  const fs::path root = ::yaze::test::UniqueTempPath("yaze_oracle_pair", "");
  const fs::path rom_path = root / "Roms" / "oos168.sfc";
  const fs::path json_path =
      root / "Data" / "dungeons" / "custom_collision.json";
  fs::create_directories(rom_path.parent_path());
  fs::create_directories(json_path.parent_path());
  fs::copy_file(oracle / "Roms" / "oos168.sfc", rom_path);
  fs::copy_file(oracle / "Data" / "dungeons" / "custom_collision.json",
                json_path);
  const std::string validator =
      (oracle / "Scripts" / "Generate" / "validate_custom_collision_source.py")
          .string();
  const auto validate = [&] {
    return std::system(absl::StrFormat("python3 '%s' --root '%s' --rom '%s'",
                                       validator, root.string(),
                                       rom_path.string())
                           .c_str());
  };
  ASSERT_EQ(validate(), 0) << "the copied pair must start out valid";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path.string()).ok());
  const std::string json_before = ReadFile(json_path);
  // A no-op save keeps the tracked JSON byte-identical.
  ASSERT_TRUE(SaveRomWithCollisionSource(&rom, json_path).ok());
  EXPECT_EQ(ReadFile(json_path), json_before);
  EXPECT_EQ(validate(), 0);

  // Change one collision tile in a room that already has collision.
  auto map = zelda3::LoadCustomCollisionMap(&rom, 0x89);
  ASSERT_TRUE(map.ok());
  ASSERT_TRUE(map->has_data);
  map->tiles[0] = map->tiles[0] == 0xB0 ? 0xB1 : 0xB0;
  ASSERT_TRUE(zelda3::WriteTrackCollision(&rom, 0x89, *map).ok());
  ASSERT_TRUE(SaveRomWithCollisionSource(&rom, json_path).ok());
  EXPECT_NE(ReadFile(json_path), json_before);
  EXPECT_EQ(validate(), 0) << "saved ROM and JSON must still match";

  std::error_code ec;
  fs::remove_all(root, ec);
}

// The real save path on a copy of the Oracle project: EditorManager opens
// the copied .yaze (with [files] custom_collision_json and its hack
// manifest), a collision edit is saved with File > Save, and Oracle's
// validator must accept the pair. Save As must leave the tracked JSON alone.
//   YAZE_ORACLE_ROOT=~/src/hobby/oracle-of-secrets
TEST(CollisionSourcePairingOracleTest,
     EditorManagerSaveKeepsProjectPairInStep) {
  const char* oracle_env = std::getenv("YAZE_ORACLE_ROOT");
  if (oracle_env == nullptr) {
    GTEST_SKIP() << "Set YAZE_ORACLE_ROOT to run against Oracle copies";
  }
  const fs::path oracle(oracle_env);
  const fs::path root = ::yaze::test::UniqueTempPath("yaze_oracle_project", "");
  for (const char* rel :
       {"Oracle-of-Secrets.yaze", "Roms/oos168.sfc", "Roms/hack_manifest.json",
        "Data/dungeons/custom_collision.json"}) {
    fs::create_directories((root / rel).parent_path());
    fs::copy_file(oracle / rel, root / rel);
  }
  const fs::path rom_path = root / "Roms" / "oos168.sfc";
  const fs::path json_path =
      root / "Data" / "dungeons" / "custom_collision.json";
  const std::string validator =
      (oracle / "Scripts" / "Generate" / "validate_custom_collision_source.py")
          .string();
  const auto validate = [&] {
    return std::system(absl::StrFormat("python3 '%s' --root '%s' --rom '%s'",
                                       validator, root.string(),
                                       rom_path.string())
                           .c_str());
  };
  ASSERT_EQ(validate(), 0);

  const auto flags_before = core::FeatureFlags::get();
  ImGuiContext* imgui = ImGui::CreateContext();
  ImGui::SetCurrentContext(imgui);
  ImGui::GetIO().DisplaySize = ImVec2(1280, 720);
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  {
    auto renderer = std::make_unique<gfx::NullRenderer>();
    auto manager = std::make_unique<EditorManager>();
    manager->Initialize(renderer.get(), "");
    manager->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager->user_settings().prefs().backup_before_save = false;
    const auto open =
        manager->OpenRomOrProject((root / "Oracle-of-Secrets.yaze").string());
    ASSERT_TRUE(open.ok()) << open;
    ASSERT_NE(manager->GetCurrentProject(), nullptr);
    EXPECT_EQ(manager->GetCurrentProject()->custom_collision_json,
              "Data/dungeons/custom_collision.json");

    Rom* rom = manager->GetCurrentRom();
    ASSERT_NE(rom, nullptr);
    const std::string json_before = ReadFile(json_path);
    auto map = zelda3::LoadCustomCollisionMap(rom, 0x89);
    ASSERT_TRUE(map.ok());
    map->tiles[0] = map->tiles[0] == 0xB0 ? 0xB1 : 0xB0;
    ASSERT_TRUE(zelda3::WriteTrackCollision(rom, 0x89, *map).ok());
    rom->set_dirty(true);

    const auto save = manager->SaveRom();
    ASSERT_TRUE(save.ok()) << save;
    EXPECT_NE(ReadFile(json_path), json_before) << "JSON was not updated";
    EXPECT_EQ(validate(), 0) << "saved ROM and JSON must match";

    // Save As writes only the new file; the tracked JSON stays as it is.
    const std::string json_after_save = ReadFile(json_path);
    map->tiles[0] = map->tiles[0] == 0xB0 ? 0xB1 : 0xB0;
    ASSERT_TRUE(zelda3::WriteTrackCollision(rom, 0x89, *map).ok());
    rom->set_dirty(true);
    const auto save_as =
        manager->SaveRomAs((root / "Roms" / "practice-copy.sfc").string());
    ASSERT_TRUE(save_as.ok()) << save_as;
    EXPECT_EQ(ReadFile(json_path), json_after_save);
  }
  ImGui::DestroyContext(imgui);
  core::FeatureFlags::get() = flags_before;
  std::error_code ec;
  fs::remove_all(root, ec);
}

}  // namespace
}  // namespace yaze::editor
