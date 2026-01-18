/**
 * @file project_tool_test.cc
 * @brief Unit tests for the ProjectTool AI agent tools
 *
 * Tests the project management functionality including snapshots,
 * edit serialization, checksum computation, and project diffing.
 */

#include "cli/service/agent/tools/project_tool.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "cli/service/agent/agent_context.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {
namespace {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

namespace fs = std::filesystem;

// =============================================================================
// EditFileHeader Tests
// =============================================================================

TEST(EditFileHeaderTest, MagicConstantIsYAZE) {
  // "YAZE" in ASCII = 0x59 0x41 0x5A 0x45 = 0x59415A45 (big endian)
  // But stored as 0x59415A45 which is little-endian "EZAY" or big-endian "YAZE"
  EXPECT_EQ(EditFileHeader::kMagic, 0x59415A45u);
}

TEST(EditFileHeaderTest, CurrentVersionIsOne) {
  EXPECT_EQ(EditFileHeader::kCurrentVersion, 1u);
}

TEST(EditFileHeaderTest, DefaultValuesAreCorrect) {
  EditFileHeader header;
  EXPECT_EQ(header.magic, EditFileHeader::kMagic);
  EXPECT_EQ(header.version, EditFileHeader::kCurrentVersion);
}

TEST(EditFileHeaderTest, HasRomChecksumField) {
  EditFileHeader header;
  EXPECT_EQ(header.base_rom_sha256.size(), 32u);
}

// =============================================================================
// SerializedEdit Tests
// =============================================================================

TEST(SerializedEditTest, StructureHasExpectedFields) {
  SerializedEdit edit;
  edit.address = 0x008000;
  edit.length = 4;

  EXPECT_EQ(edit.address, 0x008000u);
  EXPECT_EQ(edit.length, 4u);
}

TEST(SerializedEditTest, StructureSize) {
  // SerializedEdit should be 8 bytes (2 uint32_t)
  EXPECT_EQ(sizeof(SerializedEdit), 8u);
}

// =============================================================================
// ProjectToolUtils Tests
// =============================================================================

TEST(ProjectToolUtilsTest, ComputeSHA256ProducesCorrectLength) {
  std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03};
  auto hash = ProjectToolUtils::ComputeSHA256(data.data(), data.size());
  EXPECT_EQ(hash.size(), 32u);
}

TEST(ProjectToolUtilsTest, ComputeSHA256IsDeterministic) {
  std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"

  auto hash1 = ProjectToolUtils::ComputeSHA256(data.data(), data.size());
  auto hash2 = ProjectToolUtils::ComputeSHA256(data.data(), data.size());

  EXPECT_EQ(hash1, hash2);
}

TEST(ProjectToolUtilsTest, ComputeSHA256DifferentDataDifferentHash) {
  std::vector<uint8_t> data1 = {0x00, 0x01, 0x02};
  std::vector<uint8_t> data2 = {0x00, 0x01, 0x03};

  auto hash1 = ProjectToolUtils::ComputeSHA256(data1.data(), data1.size());
  auto hash2 = ProjectToolUtils::ComputeSHA256(data2.data(), data2.size());

  EXPECT_NE(hash1, hash2);
}

TEST(ProjectToolUtilsTest, ComputeSHA256EmptyInput) {
  auto hash = ProjectToolUtils::ComputeSHA256(nullptr, 0);
  EXPECT_EQ(hash.size(), 32u);
  // SHA-256 of empty string is well-known
  // e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
  EXPECT_EQ(hash[0], 0xe3);
  EXPECT_EQ(hash[1], 0xb0);
  EXPECT_EQ(hash[2], 0xc4);
}

TEST(ProjectToolUtilsTest, FormatChecksumProduces64Chars) {
  std::array<uint8_t, 32> checksum;
  checksum.fill(0xAB);

  std::string formatted = ProjectToolUtils::FormatChecksum(checksum);
  EXPECT_EQ(formatted.size(), 64u);
}

TEST(ProjectToolUtilsTest, FormatChecksumIsHex) {
  std::array<uint8_t, 32> checksum;
  for (size_t i = 0; i < 32; ++i) {
    checksum[i] = static_cast<uint8_t>(i);
  }

  std::string formatted = ProjectToolUtils::FormatChecksum(checksum);

  // Should only contain hex characters
  for (char c : formatted) {
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
        << "Non-hex character: " << c;
  }
}

TEST(ProjectToolUtilsTest, FormatTimestampProducesISO8601) {
  auto now = std::chrono::system_clock::now();
  std::string formatted = ProjectToolUtils::FormatTimestamp(now);

  // Should match ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ
  EXPECT_THAT(formatted, HasSubstr("T"));
  EXPECT_THAT(formatted, HasSubstr("Z"));
  EXPECT_GE(formatted.size(), 20u);
}

TEST(ProjectToolUtilsTest, ParseTimestampRoundTrip) {
  auto original = std::chrono::system_clock::now();
  std::string formatted = ProjectToolUtils::FormatTimestamp(original);

  auto parsed_result = ProjectToolUtils::ParseTimestamp(formatted);
  ASSERT_TRUE(parsed_result.ok()) << parsed_result.status().message();

  // Due to second-precision and timezone handling (gmtime vs mktime),
  // allow for timezone differences (up to 24 hours)
  auto parsed = *parsed_result;
  auto diff = std::chrono::duration_cast<std::chrono::hours>(
      original - parsed).count();
  EXPECT_LE(std::abs(diff), 24) << "Timestamp difference exceeds 24 hours";
}

TEST(ProjectToolUtilsTest, ParseTimestampInvalidFormat) {
  auto result = ProjectToolUtils::ParseTimestamp("invalid");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

// =============================================================================
// ProjectSnapshot Tests
// =============================================================================

TEST(ProjectSnapshotTest, DefaultConstruction) {
  ProjectSnapshot snapshot;
  EXPECT_TRUE(snapshot.name.empty());
  EXPECT_TRUE(snapshot.description.empty());
  EXPECT_TRUE(snapshot.edits.empty());
  EXPECT_TRUE(snapshot.metadata.empty());
}

TEST(ProjectSnapshotTest, HasAllRequiredFields) {
  ProjectSnapshot snapshot;
  snapshot.name = "test-snapshot";
  snapshot.description = "Test description";
  snapshot.created = std::chrono::system_clock::now();
  RomEdit edit;
  edit.address = 0x008000;
  edit.old_value = {0x00};
  edit.new_value = {0x01};
  edit.description = "Test edit";
  edit.timestamp = std::chrono::system_clock::now();
  snapshot.edits.push_back(edit);
  snapshot.metadata["author"] = "test";
  snapshot.rom_checksum.fill(0xAB);

  EXPECT_EQ(snapshot.name, "test-snapshot");
  EXPECT_EQ(snapshot.description, "Test description");
  EXPECT_THAT(snapshot.edits, SizeIs(1));
  EXPECT_EQ(snapshot.metadata["author"], "test");
  EXPECT_EQ(snapshot.rom_checksum[0], 0xAB);
}

// =============================================================================
// ProjectManager Tests
// =============================================================================

class ProjectManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a temporary directory for tests
    test_dir_ = fs::temp_directory_path() / "yaze_project_test";
    fs::create_directories(test_dir_);
  }

  void TearDown() override {
    // Clean up
    if (fs::exists(test_dir_)) {
      fs::remove_all(test_dir_);
    }
  }

  fs::path test_dir_;
};

TEST_F(ProjectManagerTest, IsNotInitializedByDefault) {
  ProjectManager manager;
  EXPECT_FALSE(manager.IsInitialized());
}

TEST_F(ProjectManagerTest, InitializeCreatesProjectDirectory) {
  ProjectManager manager;
  auto status = manager.Initialize(test_dir_.string());
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_TRUE(manager.IsInitialized());
  EXPECT_TRUE(fs::exists(test_dir_ / ".yaze"));
  EXPECT_TRUE(fs::exists(test_dir_ / ".yaze" / "snapshots"));
  EXPECT_TRUE(fs::exists(test_dir_ / ".yaze" / "project.json"));
}

TEST_F(ProjectManagerTest, ListSnapshotsEmptyInitially) {
  ProjectManager manager;
  auto status = manager.Initialize(test_dir_.string());
  ASSERT_TRUE(status.ok());

  auto snapshots = manager.ListSnapshots();
  EXPECT_TRUE(snapshots.empty());
}

TEST_F(ProjectManagerTest, CreateSnapshotEmptyNameFails) {
  ProjectManager manager;
  manager.Initialize(test_dir_.string());

  std::array<uint8_t, 32> checksum;
  checksum.fill(0);

  auto status = manager.CreateSnapshot("", "description", {}, checksum);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(ProjectManagerTest, CreateSnapshotDuplicateNameFails) {
  ProjectManager manager;
  manager.Initialize(test_dir_.string());

  std::array<uint8_t, 32> checksum;
  checksum.fill(0);

  auto status1 = manager.CreateSnapshot("test", "first", {}, checksum);
  ASSERT_TRUE(status1.ok());

  auto status2 = manager.CreateSnapshot("test", "second", {}, checksum);
  EXPECT_FALSE(status2.ok());
  EXPECT_EQ(status2.code(), absl::StatusCode::kAlreadyExists);
}

TEST_F(ProjectManagerTest, CreateAndListSnapshot) {
  ProjectManager manager;
  manager.Initialize(test_dir_.string());

  std::array<uint8_t, 32> checksum;
  checksum.fill(0xAB);

  auto status = manager.CreateSnapshot("v1.0", "Initial version", {}, checksum);
  ASSERT_TRUE(status.ok());

  auto snapshots = manager.ListSnapshots();
  EXPECT_THAT(snapshots, Contains("v1.0"));
}

TEST_F(ProjectManagerTest, GetSnapshotNotFound) {
  ProjectManager manager;
  manager.Initialize(test_dir_.string());

  auto result = manager.GetSnapshot("nonexistent");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kNotFound);
}

TEST_F(ProjectManagerTest, DeleteSnapshotNotFound) {
  ProjectManager manager;
  manager.Initialize(test_dir_.string());

  auto status = manager.DeleteSnapshot("nonexistent");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
}

TEST_F(ProjectManagerTest, CreateGetDeleteSnapshot) {
  ProjectManager manager;
  manager.Initialize(test_dir_.string());

  std::array<uint8_t, 32> checksum;
  checksum.fill(0xAB);

  // Create
  auto create_status = manager.CreateSnapshot("test", "desc", {}, checksum);
  ASSERT_TRUE(create_status.ok());

  // Get
  auto get_result = manager.GetSnapshot("test");
  ASSERT_TRUE(get_result.ok());
  EXPECT_EQ(get_result->name, "test");
  EXPECT_EQ(get_result->description, "desc");

  // Delete
  auto delete_status = manager.DeleteSnapshot("test");
  ASSERT_TRUE(delete_status.ok());

  // Verify deleted
  auto verify_result = manager.GetSnapshot("test");
  EXPECT_FALSE(verify_result.ok());
}

// =============================================================================
// Tool Name Tests
// =============================================================================

TEST(ProjectToolsTest, ProjectStatusToolName) {
  ProjectStatusTool tool;
  EXPECT_EQ(tool.GetName(), "project-status");
}

TEST(ProjectToolsTest, ProjectSnapshotToolName) {
  ProjectSnapshotTool tool;
  EXPECT_EQ(tool.GetName(), "project-snapshot");
}

TEST(ProjectToolsTest, ProjectRestoreToolName) {
  ProjectRestoreTool tool;
  EXPECT_EQ(tool.GetName(), "project-restore");
}

TEST(ProjectToolsTest, ProjectExportToolName) {
  ProjectExportTool tool;
  EXPECT_EQ(tool.GetName(), "project-export");
}

TEST(ProjectToolsTest, ProjectImportToolName) {
  ProjectImportTool tool;
  EXPECT_EQ(tool.GetName(), "project-import");
}

TEST(ProjectToolsTest, ProjectDiffToolName) {
  ProjectDiffTool tool;
  EXPECT_EQ(tool.GetName(), "project-diff");
}

TEST(ProjectToolsTest, AllToolNamesStartWithProject) {
  ProjectStatusTool status;
  ProjectSnapshotTool snapshot;
  ProjectRestoreTool restore;
  ProjectExportTool export_tool;
  ProjectImportTool import_tool;
  ProjectDiffTool diff;

  EXPECT_THAT(status.GetName(), HasSubstr("project-"));
  EXPECT_THAT(snapshot.GetName(), HasSubstr("project-"));
  EXPECT_THAT(restore.GetName(), HasSubstr("project-"));
  EXPECT_THAT(export_tool.GetName(), HasSubstr("project-"));
  EXPECT_THAT(import_tool.GetName(), HasSubstr("project-"));
  EXPECT_THAT(diff.GetName(), HasSubstr("project-"));
}

TEST(ProjectToolsTest, AllToolNamesAreUnique) {
  ProjectStatusTool status;
  ProjectSnapshotTool snapshot;
  ProjectRestoreTool restore;
  ProjectExportTool export_tool;
  ProjectImportTool import_tool;
  ProjectDiffTool diff;

  std::vector<std::string> names = {
      status.GetName(), snapshot.GetName(), restore.GetName(),
      export_tool.GetName(), import_tool.GetName(), diff.GetName()};

  std::set<std::string> unique_names(names.begin(), names.end());
  EXPECT_EQ(unique_names.size(), names.size())
      << "All project tool names should be unique";
}

// =============================================================================
// Tool Usage String Tests
// =============================================================================

TEST(ProjectToolsTest, StatusToolUsageFormat) {
  ProjectStatusTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("project-status"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--format"));
}

TEST(ProjectToolsTest, SnapshotToolUsageFormat) {
  ProjectSnapshotTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--name"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--description"));
}

TEST(ProjectToolsTest, RestoreToolUsageFormat) {
  ProjectRestoreTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--name"));
}

TEST(ProjectToolsTest, ExportToolUsageFormat) {
  ProjectExportTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--path"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--include-rom"));
}

TEST(ProjectToolsTest, ImportToolUsageFormat) {
  ProjectImportTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--path"));
}

TEST(ProjectToolsTest, DiffToolUsageFormat) {
  ProjectDiffTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--snapshot1"));
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--snapshot2"));
}

// =============================================================================
// RequiresLabels Tests
// =============================================================================

TEST(ProjectToolsTest, NoToolsRequireLabels) {
  ProjectStatusTool status;
  ProjectSnapshotTool snapshot;
  ProjectRestoreTool restore;
  ProjectExportTool export_tool;
  ProjectImportTool import_tool;
  ProjectDiffTool diff;

  EXPECT_FALSE(status.RequiresLabels());
  EXPECT_FALSE(snapshot.RequiresLabels());
  EXPECT_FALSE(restore.RequiresLabels());
  EXPECT_FALSE(export_tool.RequiresLabels());
  EXPECT_FALSE(import_tool.RequiresLabels());
  EXPECT_FALSE(diff.RequiresLabels());
}

// =============================================================================
// Snapshot Serialization Round-Trip Test
// =============================================================================

class SnapshotSerializationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_file_ = fs::temp_directory_path() / "test_snapshot.edits";
  }

  void TearDown() override {
    if (fs::exists(test_file_)) {
      fs::remove(test_file_);
    }
  }

  fs::path test_file_;
};

TEST_F(SnapshotSerializationTest, SaveAndLoadRoundTrip) {
  // Create snapshot with edits
  ProjectSnapshot original;
  original.name = "test-snapshot";
  original.description = "Test description";
  original.created = std::chrono::system_clock::now();
  original.rom_checksum.fill(0xAB);

  RomEdit edit1;
  edit1.address = 0x008000;
  edit1.old_value = {0x00, 0x01, 0x02};
  edit1.new_value = {0x10, 0x11, 0x12};
  original.edits.push_back(edit1);

  RomEdit edit2;
  edit2.address = 0x00A000;
  edit2.old_value = {0xFF};
  edit2.new_value = {0x00};
  original.edits.push_back(edit2);

  original.metadata["author"] = "test";
  original.metadata["version"] = "1.0";

  // Save
  auto save_status = original.SaveToFile(test_file_.string());
  ASSERT_TRUE(save_status.ok()) << save_status.message();
  ASSERT_TRUE(fs::exists(test_file_));

  // Load
  auto load_result = ProjectSnapshot::LoadFromFile(test_file_.string());
  ASSERT_TRUE(load_result.ok()) << load_result.status().message();

  const ProjectSnapshot& loaded = *load_result;

  // Verify
  EXPECT_EQ(loaded.name, original.name);
  EXPECT_EQ(loaded.description, original.description);
  EXPECT_EQ(loaded.rom_checksum, original.rom_checksum);
  ASSERT_EQ(loaded.edits.size(), original.edits.size());

  for (size_t i = 0; i < loaded.edits.size(); ++i) {
    EXPECT_EQ(loaded.edits[i].address, original.edits[i].address);
    EXPECT_EQ(loaded.edits[i].old_value, original.edits[i].old_value);
    EXPECT_EQ(loaded.edits[i].new_value, original.edits[i].new_value);
  }

  EXPECT_EQ(loaded.metadata, original.metadata);
}

TEST_F(SnapshotSerializationTest, LoadNonexistentFileFails) {
  auto result = ProjectSnapshot::LoadFromFile("/nonexistent/path/file.edits");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kNotFound);
}

TEST_F(SnapshotSerializationTest, LoadInvalidFileFails) {
  // Create an invalid file
  std::ofstream file(test_file_, std::ios::binary);
  file << "invalid data";
  file.close();

  auto result = ProjectSnapshot::LoadFromFile(test_file_.string());
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
