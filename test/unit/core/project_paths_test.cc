#include "core/project.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
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

#ifndef __EMSCRIPTEN__
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

    ASSERT_TRUE(project.Save().ok());
  }
  const auto saved = ReadTextFile(project_file);
  EXPECT_NE(saved.find("[labels_room]"), std::string::npos);
  EXPECT_NE(saved.find("37=Water Grate"), std::string::npos);
  EXPECT_EQ(saved.find("37=Thieves' Town"), std::string::npos);
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

}  // namespace yaze::project
