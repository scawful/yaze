#include "rom/rom.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "mocks/mock_rom.h"
#include "rom/transaction.h"
#include "test_utils.h"
#include "testing.h"

namespace yaze {
namespace test {

using ::testing::_;
using ::testing::Return;

const static std::vector<uint8_t> kMockRomData = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
};

class ScopedTempDirectory {
 public:
  ScopedTempDirectory() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("yaze_rom_save_test_" + std::to_string(nonce));
    std::filesystem::create_directories(path_);
  }

  ~ScopedTempDirectory() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(input),
                              std::istreambuf_iterator<char>());
}

void WriteFileBytes(const std::filesystem::path& path,
                    const std::vector<uint8_t>& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output.is_open());
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  ASSERT_TRUE(output.good());
}

int CountFilesWithPrefix(const std::filesystem::path& directory,
                         const std::string& prefix) {
  std::error_code ec;
  int count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
    if (ec) {
      return count;
    }
    if (entry.path().filename().string().rfind(prefix, 0) == 0) {
      ++count;
    }
  }
  return count;
}

class RomTest : public ::testing::Test {
 protected:
  Rom rom_;
};

TEST_F(RomTest, Uninitialized) {
  EXPECT_EQ(rom_.size(), 0);
  EXPECT_EQ(rom_.data(), nullptr);
}

TEST_F(RomTest, LoadFromFile) {
#if defined(__linux__)
  GTEST_SKIP() << "ROM file loading skipped on Linux CI (no ROM available)";
#endif
  YAZE_SKIP_IF_ROM_MISSING(RomRole::kVanilla, "RomTest.LoadFromFile");
  const std::string rom_path = TestRomManager::GetRomPath(RomRole::kVanilla);
  EXPECT_OK(rom_.LoadFromFile(rom_path));
  EXPECT_EQ(rom_.size(), 0x200000);
  EXPECT_NE(rom_.data(), nullptr);
}

TEST_F(RomTest, LoadFromFileInvalid) {
  EXPECT_THAT(rom_.LoadFromFile("invalid.sfc"),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_EQ(rom_.size(), 0);
  EXPECT_EQ(rom_.data(), nullptr);
}

TEST_F(RomTest, LoadFromFileEmpty) {
  EXPECT_THAT(rom_.LoadFromFile(""),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(RomTest, LoadFromFileTooLarge) {
#if defined(__linux__)
  GTEST_SKIP() << "File tests skipped on Linux CI (filesystem access)";
#endif
  // A file above the 16MB cap must be rejected before its bytes are allocated.
  const char* tmp_name = "test_too_large_rom.sfc";
  std::error_code ec;
  { std::ofstream(tmp_name, std::ios::binary).put('\0'); }
  std::filesystem::resize_file(tmp_name, 16 * 1024 * 1024 + 1, ec);
  ASSERT_FALSE(ec);
  EXPECT_THAT(rom_.LoadFromFile(tmp_name),
              StatusIs(absl::StatusCode::kInvalidArgument));
  std::filesystem::remove(tmp_name, ec);
}

TEST_F(RomTest, ReadByteOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));

  for (size_t i = 0; i < kMockRomData.size(); ++i) {
    uint8_t byte;
    ASSERT_OK_AND_ASSIGN(byte, rom_.ReadByte(i));
    EXPECT_EQ(byte, kMockRomData[i]);
  }
}

TEST_F(RomTest, ReadByteInvalid) {
  EXPECT_THAT(rom_.ReadByte(0).status(),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST_F(RomTest, ReadWordOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));

  for (size_t i = 0; i < kMockRomData.size(); i += 2) {
    // Little endian
    EXPECT_THAT(
        rom_.ReadWord(i),
        IsOkAndHolds<uint16_t>((kMockRomData[i]) | kMockRomData[i + 1] << 8));
  }
}

TEST_F(RomTest, ReadWordInvalid) {
  EXPECT_THAT(rom_.ReadWord(0).status(),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST_F(RomTest, ReadLongOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));

  for (size_t i = 0; i < kMockRomData.size(); i += 4) {
    // Little endian
    EXPECT_THAT(rom_.ReadLong(i),
                IsOkAndHolds<uint32_t>((kMockRomData[i]) | kMockRomData[i] |
                                       kMockRomData[i + 1] << 8 |
                                       kMockRomData[i + 2] << 16));
  }
}

TEST_F(RomTest, ReadBytesOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));

  std::vector<uint8_t> bytes;
  ASSERT_OK_AND_ASSIGN(bytes, rom_.ReadByteVector(0, kMockRomData.size()));
  EXPECT_THAT(bytes, ::testing::ContainerEq(kMockRomData));
}

TEST_F(RomTest, ReadBytesOutOfRange) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));

  std::vector<uint8_t> bytes;
  EXPECT_THAT(rom_.ReadByteVector(kMockRomData.size() + 1, 1).status(),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST_F(RomTest, WriteByteOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));

  for (size_t i = 0; i < kMockRomData.size(); ++i) {
    EXPECT_OK(rom_.WriteByte(i, 0xFF));
    uint8_t byte;
    ASSERT_OK_AND_ASSIGN(byte, rom_.ReadByte(i));
    EXPECT_EQ(byte, 0xFF);
  }
}

TEST_F(RomTest, WriteByteNegativeAddressRejected) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));
  EXPECT_THAT(rom_.WriteByte(-1, 0xFF),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST_F(RomTest, WriteWordOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));

  for (size_t i = 0; i < kMockRomData.size(); i += 2) {
    EXPECT_OK(rom_.WriteWord(i, 0xFFFF));
    uint16_t word;
    ASSERT_OK_AND_ASSIGN(word, rom_.ReadWord(i));
    EXPECT_EQ(word, 0xFFFF);
  }
}

TEST_F(RomTest, WriteWordNegativeAddressRejected) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));
  EXPECT_THAT(rom_.WriteWord(-1, 0xFFFF),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST_F(RomTest, WriteLongOk) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));

  for (size_t i = 0; i < kMockRomData.size(); i += 4) {
    EXPECT_OK(rom_.WriteLong(i, 0xFFFFFF));
    uint32_t word;
    ASSERT_OK_AND_ASSIGN(word, rom_.ReadLong(i));
    EXPECT_EQ(word, 0xFFFFFF);
  }
}

TEST_F(RomTest, WriteVectorNegativeAddressRejected) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));
  EXPECT_THAT(rom_.WriteVector(-1, std::vector<uint8_t>{0x01}),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST_F(RomTest, WriteTransactionSuccess) {
  MockRom mock_rom;
  EXPECT_OK(mock_rom.LoadFromData(kMockRomData));

  EXPECT_CALL(mock_rom, WriteHelper(_))
      .WillRepeatedly(Return(absl::OkStatus()));

  EXPECT_OK(mock_rom.WriteTransaction(
      Rom::WriteAction{0x1000, uint8_t{0xFF}},
      Rom::WriteAction{0x1001, uint16_t{0xABCD}},
      Rom::WriteAction{0x1002, std::vector<uint8_t>{0x12, 0x34}}));
}

TEST_F(RomTest, WriteTransactionFailure) {
  MockRom mock_rom;
  EXPECT_OK(mock_rom.LoadFromData(kMockRomData));

  EXPECT_CALL(mock_rom, WriteHelper(_))
      .WillOnce(Return(absl::OkStatus()))
      .WillOnce(Return(absl::InternalError("Write failed")));

  EXPECT_EQ(
      mock_rom.WriteTransaction(Rom::WriteAction{0x1000, uint8_t{0xFF}},
                                Rom::WriteAction{0x1001, uint16_t{0xABCD}}),
      absl::InternalError("Write failed"));
}

TEST_F(RomTest, ReadTransactionSuccess) {
  MockRom mock_rom;
  EXPECT_OK(mock_rom.LoadFromData(kMockRomData));
  uint8_t byte_val;
  uint16_t word_val;

  EXPECT_OK(mock_rom.ReadTransaction(byte_val, 0x0000, word_val, 0x0001));

  EXPECT_EQ(byte_val, 0x00);
  EXPECT_EQ(word_val, 0x0201);
}

TEST_F(RomTest, ReadTransactionFailure) {
  MockRom mock_rom;
  EXPECT_OK(mock_rom.LoadFromData(kMockRomData));
  uint8_t byte_val;

  EXPECT_THAT(mock_rom.ReadTransaction(byte_val, 0x1000),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST_F(RomTest, SaveTruncatesExistingFile) {
#if defined(__linux__)
  GTEST_SKIP() << "File save tests skipped on Linux CI (filesystem access)";
#endif
  // Prepare ROM data and save to a temp file twice; second save should
  // overwrite, not append
  EXPECT_OK(rom_.LoadFromData(kMockRomData));

  const char* tmp_name = "test_temp_rom.sfc";
  yaze::Rom::SaveSettings settings;
  settings.filename = tmp_name;

  // First save
  EXPECT_OK(rom_.SaveToFile(settings));

  // Modify one byte and save again
  EXPECT_OK(rom_.WriteByte(0, 0xEE));
  EXPECT_OK(rom_.SaveToFile(settings));

  // Load the saved file and verify size equals original data size and first
  // byte matches
  std::ifstream verify(tmp_name, std::ios::binary);
  ASSERT_TRUE(verify.is_open());
  std::vector<uint8_t> file_bytes((std::istreambuf_iterator<char>(verify)),
                                  std::istreambuf_iterator<char>());
  EXPECT_EQ(file_bytes.size(), kMockRomData.size());
  ASSERT_FALSE(file_bytes.empty());
  EXPECT_EQ(file_bytes[0], 0xEE);
}

TEST_F(RomTest, BestEffortBackupFailureStillSaves) {
  ScopedTempDirectory temp;
  const auto target = temp.path() / "target.sfc";
  const auto missing_source = temp.path() / "missing-source.sfc";
  const std::vector<uint8_t> disk_before = {0xAA, 0xBB, 0xCC};
  WriteFileBytes(target, disk_before);

  EXPECT_OK(rom_.LoadFromData(kMockRomData));
  EXPECT_OK(rom_.WriteByte(0, 0xEE));
  rom_.set_filename(missing_source.string());

  Rom::SaveSettings settings;
  settings.backup = true;
  settings.filename = target.string();
  EXPECT_OK(rom_.SaveToFile(settings));

  EXPECT_EQ(ReadFileBytes(target), rom_.vector());
  EXPECT_EQ(CountFilesWithPrefix(temp.path(), "target.sfc_backup_"), 0);
}

TEST_F(RomTest, RequiredBackupFailureRollsBackTransactionAndLeavesNoArtifacts) {
  ScopedTempDirectory temp;
  const auto target = temp.path() / "target.sfc";
  const auto missing_source = temp.path() / "missing-source.sfc";
  const std::vector<uint8_t> disk_before = {0xAA, 0xBB, 0xCC};
  WriteFileBytes(target, disk_before);

  EXPECT_OK(rom_.LoadFromData(kMockRomData));
  rom_.set_filename(missing_source.string());
  const auto rom_before = rom_.vector();
  const bool dirty_before = rom_.dirty();
  const std::string filename_before = rom_.filename();

  {
    ScopedRomTransaction transaction(rom_);
    EXPECT_OK(rom_.WriteByte(0, 0xEE));

    Rom::SaveSettings settings;
    settings.require_backup = true;
    settings.filename = target.string();
    const absl::Status status = rom_.SaveToFile(settings);

    EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
    EXPECT_THAT(std::string(status.message()),
                ::testing::HasSubstr("Could not create required ROM backup"));
  }

  EXPECT_EQ(rom_.vector(), rom_before);
  EXPECT_EQ(rom_.dirty(), dirty_before);
  EXPECT_EQ(rom_.filename(), filename_before);
  EXPECT_EQ(ReadFileBytes(target), disk_before);
  EXPECT_FALSE(std::filesystem::exists(target.string() + ".tmp"));
  EXPECT_EQ(CountFilesWithPrefix(temp.path(), "target.sfc_backup_"), 0);
}

TEST_F(RomTest, TransactionRollbackRestoresOriginals) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));
  // Force an out-of-range write to trigger failure after a successful write
  yaze::Transaction tx{rom_};
  auto status =
      tx.WriteByte(0x01, 0xAA)        // valid
          .WriteWord(0xFFFF, 0xBBBB)  // invalid: should fail and rollback
          .Commit();
  EXPECT_FALSE(status.ok());
  auto b1 = rom_.ReadByte(0x01);
  ASSERT_TRUE(b1.ok());
  // Should be restored to original 0x01 value (from kMockRomData)
  EXPECT_EQ(*b1, kMockRomData[0x01]);
}

TEST_F(RomTest, ScopedRomTransactionRollsBackBufferMetadataAndDirtyState) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));
  rom_.set_filename("before.sfc");
  rom_.set_dirty(false);

  {
    yaze::ScopedRomTransaction tx{rom_};
    EXPECT_OK(rom_.WriteByte(0x01, 0xAA));
    rom_.Expand(0x100);
    rom_.set_filename("after.sfc");
  }

  EXPECT_EQ(rom_.vector(), kMockRomData);
  EXPECT_EQ(rom_.size(), kMockRomData.size());
  EXPECT_EQ(rom_.filename(), "before.sfc");
  EXPECT_FALSE(rom_.dirty());
}

TEST_F(RomTest, ScopedRomTransactionCommitKeepsChanges) {
  EXPECT_OK(rom_.LoadFromData(kMockRomData));
  {
    yaze::ScopedRomTransaction tx{rom_};
    EXPECT_OK(rom_.WriteByte(0x01, 0xAA));
    tx.Commit();
  }

  const auto value = rom_.ReadByte(0x01);
  ASSERT_TRUE(value.ok());
  EXPECT_EQ(*value, 0xAA);
  EXPECT_TRUE(rom_.dirty());
}

}  // namespace test
}  // namespace yaze
