#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "unique_temp_path.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace {

std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::filesystem::path FixtureDir() {
  const char* path = std::getenv("YAZE_LYRA_FIXTURE_DIR");
  return path == nullptr ? std::filesystem::path()
                         : std::filesystem::path(path);
}

TEST(LyraLayoutDescriptorTest, ShortAndUnrelatedImagesAreNotAttributed) {
  std::vector<uint8_t> short_rom(0x100000, 0x77);
  EXPECT_EQ(rom::InspectLyraLayout(short_rom.data(), short_rom.size()).presence,
            rom::LyraLayoutPresence::kAbsent);
  std::vector<uint8_t> other_hack(0x200000, 0x55);
  EXPECT_EQ(
      rom::InspectLyraLayout(other_hack.data(), other_hack.size()).presence,
      rom::LyraLayoutPresence::kAbsent);
}

TEST(LyraLayoutDescriptorTest, PortableVectorsPreserveProductCompatibility) {
  const auto dir = FixtureDir();
  if (dir.empty())
    GTEST_SKIP() << "Set YAZE_LYRA_FIXTURE_DIR";
  std::ifstream in(dir / "editor_cases.json");
  ASSERT_TRUE(in.good());
  const auto cases = nlohmann::json::parse(in)["cases"];
  ASSERT_EQ(cases.size(), 16u);
  for (const auto& item : cases) {
    const std::string name = item["name"];
    SCOPED_TRACE(name);
    auto bytes = ReadBytes(dir / item["base"].get<std::string>());
    ASSERT_FALSE(bytes.empty());
    for (const auto& patch : item["patches"]) {
      const size_t pc = patch["pc"];
      const std::string hex = patch["hex"];
      ASSERT_EQ(hex.size() % 2, 0u);
      ASSERT_LE(pc + hex.size() / 2, bytes.size());
      for (size_t i = 0; i < hex.size(); i += 2) {
        bytes[pc + i / 2] =
            static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16));
      }
    }
    if (!item["truncate_to"].is_null()) {
      bytes.resize(item["truncate_to"].get<size_t>());
    }
    const auto result = rom::InspectLyraLayout(bytes.data(), bytes.size());
    const std::string expected = item["expected"];
    if (expected == "supported_offline_inspection_only") {
      EXPECT_EQ(result.presence, rom::LyraLayoutPresence::kValid);
      EXPECT_EQ(result.revision, 1);
      EXPECT_EQ(result.capacity, 192);
      EXPECT_EQ(result.active_count, 160);
      EXPECT_FALSE(result.write_supported);
      EXPECT_EQ(result.entries.size(), 35u);
    } else if (expected == "legacy_160" ||
               name == "old_tail_marker_without_descriptor") {
      // The Oracle fixture detector rejects the older $EA format. Yaze's
      // established marker detector still owns and accepts that format.
      EXPECT_EQ(result.presence, rom::LyraLayoutPresence::kAbsent);
      if (name == "old_tail_marker_without_descriptor") {
        Rom older;
        ASSERT_TRUE(older.LoadFromData(bytes).ok());
        const auto profile = zelda3::DetectOverworldRomProfile(older);
        EXPECT_EQ(profile.layout_kind,
                  zelda3::OverworldRomProfile::LayoutKind::kOlderTailEa);
        EXPECT_EQ(profile.editable_map_count, 192);
      }
    } else {
      EXPECT_EQ(result.presence, rom::LyraLayoutPresence::kInvalid)
          << result.error;
      EXPECT_FALSE(result.write_supported);
      Rom malformed;
      ASSERT_TRUE(malformed.LoadFromData(bytes).ok());
      EXPECT_FALSE(malformed.CheckLayoutWritable().ok());
      const auto profile = zelda3::DetectOverworldRomProfile(malformed);
      EXPECT_EQ(profile.layout_kind,
                zelda3::OverworldRomProfile::LayoutKind::kLyraOffline);
      zelda3::Overworld overworld(&malformed);
      EXPECT_FALSE(overworld.Load(&malformed).ok());
    }
  }
}

TEST(LyraLayoutDescriptorTest, HeaderedImageAndTwoSessionsKeepSavePolicy) {
  const auto dir = FixtureDir();
  if (dir.empty())
    GTEST_SKIP() << "Set YAZE_LYRA_FIXTURE_DIR";
  const auto offline = ReadBytes(dir / "OFFLINE_ONLY.bin");
  const auto legacy = ReadBytes(dir / "legacy_160_input.bin");
  ASSERT_FALSE(offline.empty());
  ASSERT_FALSE(legacy.empty());
  Rom lyra;
  std::vector<uint8_t> headered(0x200, 0);
  headered.insert(headered.end(), offline.begin(), offline.end());
  ASSERT_TRUE(lyra.LoadFromData(headered).ok());
  EXPECT_TRUE(lyra.has_copier_header());
  ASSERT_EQ(lyra.InspectLyraLayout().presence, rom::LyraLayoutPresence::kValid);
  Rom copied = lyra;
  EXPECT_FALSE(copied.CheckLayoutWritable().ok());
  copied.Close();
  EXPECT_TRUE(copied.CheckLayoutWritable().ok());
  zelda3::Overworld overworld(&lyra);
  EXPECT_FALSE(overworld.Load(&lyra).ok());
  EXPECT_FALSE(overworld.Save(&lyra).ok());
  Rom ordinary;
  ASSERT_TRUE(ordinary.LoadFromData(legacy).ok());
  EXPECT_EQ(ordinary.InspectLyraLayout().presence,
            rom::LyraLayoutPresence::kAbsent);
  EXPECT_TRUE(ordinary.CheckLayoutWritable().ok());

  const auto before = lyra.vector();
  const bool dirty = lyra.dirty();
  const auto destination = test::UniqueTempPath("lyra_save_refusal");
  const auto refused = lyra.SaveToFile({.filename = destination.string()});
  EXPECT_FALSE(refused.ok());
  EXPECT_FALSE(std::filesystem::exists(destination));
  EXPECT_EQ(lyra.vector(), before);
  EXPECT_EQ(lyra.dirty(), dirty);
  EXPECT_FALSE(lyra.WriteByte(0x100, 0x42).ok());
  EXPECT_EQ(lyra.vector(), before);

  // A damaged signature must not downgrade this loaded session to legacy.
  lyra.mutable_vector()[0x147000] = 0;
  EXPECT_EQ(lyra.InspectLyraLayout().presence,
            rom::LyraLayoutPresence::kInvalid);
  EXPECT_FALSE(lyra.CheckLayoutWritable().ok());
  std::fill_n(lyra.mutable_vector().begin() + 0x147000, 1024, 0);
  EXPECT_EQ(lyra.InspectLyraLayout().presence,
            rom::LyraLayoutPresence::kInvalid);
  EXPECT_FALSE(lyra.CheckLayoutWritable().ok());
  EXPECT_TRUE(ordinary.CheckLayoutWritable().ok());
}

}  // namespace
}  // namespace yaze
