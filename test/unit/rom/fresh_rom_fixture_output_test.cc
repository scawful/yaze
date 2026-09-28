#include "fresh_rom_fixture_output.h"

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::test {
namespace {

class FreshRomFixtureOutputTest : public ::testing::Test {
 protected:
  void SetUp() override {
    directory_ = UniqueTempPath("fresh_fixture");
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
    source_ = directory_ / "source.sfc";
    std::ofstream(source_, std::ios::binary) << "original ROM";
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>{1, 2, 3, 4}).ok());
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(directory_, ec);
  }

  std::string Read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()};
  }

  std::filesystem::path directory_;
  std::filesystem::path source_;
  Rom rom_;
};

TEST_F(FreshRomFixtureOutputTest, RejectsSourceRomWithoutChangingItsBytes) {
  EXPECT_FALSE(ValidateFreshFixtureOutput(source_).ok());
  EXPECT_FALSE(SaveFreshFixtureRom(rom_, source_).ok());
  EXPECT_EQ(Read(source_), "original ROM");
}

TEST_F(FreshRomFixtureOutputTest, RejectsHardLinkToSourceRom) {
  const auto alias = directory_ / "alias.sfc";
  std::filesystem::create_hard_link(source_, alias);
  EXPECT_FALSE(ValidateFreshFixtureOutput(alias).ok());
  EXPECT_FALSE(SaveFreshFixtureRom(rom_, alias).ok());
  EXPECT_EQ(Read(source_), "original ROM");
  EXPECT_TRUE(std::filesystem::equivalent(source_, alias));
}

TEST_F(FreshRomFixtureOutputTest, RejectsSymlinkAndDanglingSymlink) {
  const auto alias = directory_ / "alias.sfc";
  std::error_code ec;
  std::filesystem::create_symlink(source_, alias, ec);
  if (ec) {
    GTEST_SKIP() << "Symlinks unavailable: " << ec.message();
  }
  EXPECT_FALSE(ValidateFreshFixtureOutput(alias).ok());
  EXPECT_FALSE(SaveFreshFixtureRom(rom_, alias).ok());
  EXPECT_EQ(Read(source_), "original ROM");
  const auto missing = directory_ / "missing.sfc";
  const auto dangling = directory_ / "dangling.sfc";
  std::filesystem::create_symlink(missing, dangling);
  EXPECT_FALSE(ValidateFreshFixtureOutput(dangling).ok());
  EXPECT_FALSE(SaveFreshFixtureRom(rom_, dangling).ok());
  EXPECT_TRUE(std::filesystem::is_symlink(dangling));
  EXPECT_FALSE(std::filesystem::exists(missing));
}

TEST_F(FreshRomFixtureOutputTest, RejectsFileCreatedAfterPreflight) {
  const auto target = directory_ / "edited.sfc";
  ASSERT_TRUE(ValidateFreshFixtureOutput(target).ok());
  std::ofstream(target) << "another writer";
  EXPECT_FALSE(SaveFreshFixtureRom(rom_, target).ok());
  EXPECT_EQ(Read(target), "another writer");
}

TEST_F(FreshRomFixtureOutputTest, PublicationNeverReplacesCompetingOutput) {
  const auto target = directory_ / "edited.sfc";
  ASSERT_TRUE(ValidateFreshFixtureOutput(target).ok());
  // Model a writer winning after the last check, immediately before publish.
  std::ofstream(target) << "another writer";
  EXPECT_FALSE(PublishFreshFixtureOutput(source_, target).ok());
  EXPECT_EQ(Read(target), "another writer");
  EXPECT_EQ(Read(source_), "original ROM");
}

TEST_F(FreshRomFixtureOutputTest, PublicationNeverFollowsCompetingSymlink) {
  const auto target = directory_ / "edited.sfc";
  ASSERT_TRUE(ValidateFreshFixtureOutput(target).ok());
  std::error_code ec;
  std::filesystem::create_symlink(source_, target, ec);
  if (ec) {
    GTEST_SKIP() << "Symlinks unavailable: " << ec.message();
  }
  EXPECT_FALSE(PublishFreshFixtureOutput(source_, target).ok());
  EXPECT_TRUE(std::filesystem::is_symlink(target));
  EXPECT_EQ(Read(source_), "original ROM");
}

TEST_F(FreshRomFixtureOutputTest, PublishesFreshRomAndRemovesStagingFiles) {
  const auto target = directory_ / "edited.sfc";
  const auto saved = SaveFreshFixtureRom(rom_, target);
  ASSERT_TRUE(saved.ok()) << saved;
  EXPECT_EQ(Read(target), std::string("\x01\x02\x03\x04", 4));
  EXPECT_EQ(Read(source_), "original ROM");
  EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory_),
                          std::filesystem::directory_iterator()),
            2);
}

TEST_F(FreshRomFixtureOutputTest, FailedRomSaveLeavesNoOutputOrStagingFiles) {
  Rom empty;
  const auto target = directory_ / "edited.sfc";
  EXPECT_FALSE(SaveFreshFixtureRom(empty, target).ok());
  EXPECT_FALSE(std::filesystem::exists(target));
  EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory_),
                          std::filesystem::directory_iterator()),
            1);
}

}  // namespace
}  // namespace yaze::test
