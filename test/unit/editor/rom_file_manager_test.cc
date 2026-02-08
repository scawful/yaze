#include "app/editor/system/rom_file_manager.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "rom/rom.h"

namespace yaze::editor {

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

void WriteBinaryFile(const std::filesystem::path& path,
                     const std::vector<uint8_t>& data) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(file.is_open()) << "Failed to open file for writing: "
                              << path.string();
  file.write(reinterpret_cast<const char*>(data.data()),
             static_cast<std::streamsize>(data.size()));
  file.close();
}

std::vector<uint8_t> ReadBinaryFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  EXPECT_TRUE(file.is_open()) << "Failed to open file for reading: "
                              << path.string();
  file.seekg(0, std::ios::end);
  const auto size = static_cast<size_t>(file.tellg());
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char*>(data.data()),
            static_cast<std::streamsize>(size));
  file.close();
  return data;
}

}  // namespace

TEST(RomFileManagerTest, BackupCopiesOnDiskRomNotInMemoryBuffer) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_rom_backup"));

  const auto rom_path = temp.path() / "test.sfc";
  const std::vector<uint8_t> original(512 * 1024, 0xAA);
  WriteBinaryFile(rom_path, original);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path.string()).ok());

  // Mutate the in-memory ROM. Backup should still copy the on-disk file.
  ASSERT_TRUE(rom.WriteByte(0, 0xBB).ok());

  RomFileManager manager(/*toast_manager=*/nullptr);
  manager.SetBackupFolder((temp.path() / "backups").string());

  ASSERT_TRUE(manager.CreateBackup(&rom).ok());

  const auto backups = manager.ListBackups(rom.filename());
  ASSERT_EQ(backups.size(), 1u);

  const auto backup_data = ReadBinaryFile(backups[0].path);
  ASSERT_EQ(backup_data.size(), original.size());
  EXPECT_EQ(backup_data[0], 0xAA);

  // Sanity: the source file is unchanged on disk.
  const auto source_data = ReadBinaryFile(rom_path);
  ASSERT_EQ(source_data.size(), original.size());
  EXPECT_EQ(source_data[0], 0xAA);
}

}  // namespace yaze::editor

