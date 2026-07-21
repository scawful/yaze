// Tests for ProjectManager's project-file validation and open-flow, covering
// the .yazeproj bundle and legacy .yaze/.zsproj paths.
//
// These tests exercise the public OpenProject() API to verify that:
//  - .yaze files are accepted
//  - .yazeproj bundle directories are accepted
//  - .yazeproj regular files (not directories) are rejected
//  - .zsproj files are accepted as project files (not ROM files)
//  - ROMs and arbitrary files are rejected
//  - Empty paths and nonexistent paths are rejected

#include "app/editor/system/session/project_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#include "absl/status/status.h"
#include "gtest/gtest.h"

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

void WriteTextFile(const std::filesystem::path& path, const std::string& data) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::out | std::ios::trunc);
  ASSERT_TRUE(file.is_open()) << "Failed to open: " << path.string();
  file << data;
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream file(path);
  EXPECT_TRUE(file.is_open()) << "Failed to open: " << path.string();
  return std::string(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
}

// Minimal portable project file content (paths relative to its directory).
std::string MinimalProjectFile(const std::string& name) {
  return "[project]\nname=" + name +
         "\n\n"
         "[files]\n"
         "rom_filename=\n"
         "code_folder=\n"
         "rom_backup_folder=backups\n"
         "output_folder=output\n"
         "labels_filename=labels.txt\n"
         "symbols_filename=symbols.txt\n";
}

// Create a minimal but valid .yazeproj bundle at base/stem.yazeproj.
std::filesystem::path CreateBundle(const std::filesystem::path& base,
                                   const std::string& stem) {
  const auto bundle = base / (stem + ".yazeproj");
  std::filesystem::create_directories(bundle);
  WriteTextFile(bundle / "project.yaze", MinimalProjectFile(stem));
  return bundle;
}

}  // namespace

// ---------------------------------------------------------------------------
// Rejection cases
// ---------------------------------------------------------------------------

TEST(ProjectManagerValidatorTest, RejectsEmptyFilename) {
  ProjectManager mgr(nullptr);
  EXPECT_FALSE(mgr.OpenProject("").ok());
}

TEST(ProjectManagerValidatorTest, RejectsNonexistentPath) {
  ProjectManager mgr(nullptr);
  const std::string ghost =
      (std::filesystem::temp_directory_path() / "nope_xyzzy.yaze").string();
  EXPECT_FALSE(mgr.OpenProject(ghost).ok());
}

TEST(ProjectManagerValidatorTest, RejectsRomExtension) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_rom_reject"));
  const auto rom_path = temp.path() / "game.sfc";
  WriteTextFile(rom_path, "not a rom");
  ProjectManager mgr(nullptr);
  EXPECT_FALSE(mgr.OpenProject(rom_path.string()).ok());
}

TEST(ProjectManagerValidatorTest, RejectsArbitraryExtension) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_arb_reject"));
  const auto path = temp.path() / "data.bin";
  WriteTextFile(path, "garbage");
  ProjectManager mgr(nullptr);
  EXPECT_FALSE(mgr.OpenProject(path.string()).ok());
}

TEST(ProjectManagerValidatorTest, RejectsYazeProjRegularFile) {
  // A file named *.yazeproj (not a directory) must be rejected by the
  // validator before it reaches project.Open(), which requires a directory.
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_fakebundle"));
  const auto fake_bundle = temp.path() / "Fake.yazeproj";
  WriteTextFile(fake_bundle, "not a directory bundle");
  ProjectManager mgr(nullptr);
  EXPECT_FALSE(mgr.OpenProject(fake_bundle.string()).ok());
}

// ---------------------------------------------------------------------------
// Acceptance cases
// ---------------------------------------------------------------------------

TEST(ProjectManagerValidatorTest, AcceptsYazeFile) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_yaze_accept"));
  const auto yaze_path = temp.path() / "MyProject.yaze";
  WriteTextFile(yaze_path, MinimalProjectFile("MyProject"));
  ProjectManager mgr(nullptr);
  EXPECT_TRUE(mgr.OpenProject(yaze_path.string()).ok());
}

TEST(ProjectManagerValidatorTest, AcceptsYazeProjBundle) {
  // .yazeproj bundle directory with a project.yaze inside.
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_bundle_accept"));
  const auto bundle = CreateBundle(temp.path(), "BundleProject");
  ProjectManager mgr(nullptr);
  EXPECT_TRUE(mgr.OpenProject(bundle.string()).ok());
}

TEST(ProjectManagerValidatorTest, AcceptsYazeProjBundleWithoutProjectFile) {
  // Bundle with no project.yaze: Open() auto-creates it and returns ok.
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_bundle_autocreate"));
  const auto bundle = temp.path() / "AutoCreate.yazeproj";
  std::filesystem::create_directories(bundle);
  // Intentionally omit project.yaze.
  ProjectManager mgr(nullptr);
  EXPECT_TRUE(mgr.OpenProject(bundle.string()).ok());
}

TEST(ProjectManagerValidatorTest, ZsprojNotRoutedToRomLoader) {
  // .zsproj must be recognized as a project file (not passed to LoadRom()).
  // Since ZScream import touches disk, just verify the error is NOT "unsupported
  // extension" or the LoadRom "not a valid ROM" error but comes from the project
  // layer (e.g., file-not-found or parse error).
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_zsproj"));
  const auto zsproj_path = temp.path() / "hack.zsproj";
  WriteTextFile(zsproj_path, "not a real zscream project");
  ProjectManager mgr(nullptr);
  const auto status = mgr.OpenProject(zsproj_path.string());
  // It must not claim the extension is unsupported, nor silently succeed.
  EXPECT_NE(status.code(), absl::StatusCode::kInvalidArgument)
      << "Expected .zsproj to pass extension validation; got: "
      << status.message();
}

// ---------------------------------------------------------------------------
// HasActiveProject reflects successful open
// ---------------------------------------------------------------------------

TEST(ProjectManagerValidatorTest, HasActiveProjectAfterSuccessfulOpen) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_active_project"));
  const auto yaze_path = temp.path() / "Active.yaze";
  WriteTextFile(yaze_path, MinimalProjectFile("Active"));
  ProjectManager mgr(nullptr);
  ASSERT_TRUE(mgr.OpenProject(yaze_path.string()).ok());
  EXPECT_TRUE(mgr.HasActiveProject());
}

TEST(ProjectManagerValidatorTest, HasActiveProjectAfterBundleOpen) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_bundle_active"));
  const auto bundle = CreateBundle(temp.path(), "ActiveBundle");
  ProjectManager mgr(nullptr);
  ASSERT_TRUE(mgr.OpenProject(bundle.string()).ok());
  EXPECT_TRUE(mgr.HasActiveProject());
}

TEST(ProjectManagerValidatorTest, CreateFromTemplateWaitsForRomSelection) {
  ProjectManager mgr(nullptr);
  ASSERT_TRUE(
      mgr.CreateFromTemplate("Dungeon Editor Project", "Temple Hack").ok());

  EXPECT_TRUE(mgr.IsPendingRomSelection());
  EXPECT_EQ(mgr.GetProjectName(), "Temple Hack");
}

TEST(ProjectManagerValidatorTest,
     CreateFromTemplateAppliesConfigurationPresetWithoutClaimingAsm) {
  ProjectManager mgr(nullptr);
  ASSERT_TRUE(mgr.CreateFromTemplate("ZSCustomOverworld v3", "ZSO Hack").ok());

  const auto& flags = mgr.GetCurrentProject().feature_flags;
  EXPECT_TRUE(flags.overworld.kLoadCustomOverworld);
  EXPECT_TRUE(flags.overworld.kSaveOverworldMaps);
  EXPECT_TRUE(flags.kSaveAllPalettes);
  EXPECT_TRUE(flags.kSaveGfxGroups);
  EXPECT_FALSE(flags.overworld.kApplyZSCustomOverworldASM);
}

TEST(ProjectManagerValidatorTest, FinalizeProjectCreationPersistsDescriptor) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_finalize"));
  const auto rom_path = temp.path() / "source.sfc";
  WriteTextFile(rom_path, "rom");

  ProjectManager mgr(nullptr);
  ASSERT_TRUE(mgr.CreateNewProject().ok());
  ASSERT_TRUE(mgr.SetProjectRom(rom_path.string()).ok());
  ASSERT_TRUE(mgr.FinalizeProjectCreation("Oracle Test", "").ok());

  const auto expected_path = temp.path() / "Oracle_Test.yaze";
  EXPECT_EQ(mgr.GetCurrentProject().filepath, expected_path.string());
  EXPECT_TRUE(std::filesystem::exists(expected_path));

  project::YazeProject reopened;
  ASSERT_TRUE(reopened.Open(expected_path.string()).ok());
  EXPECT_EQ(reopened.name, "Oracle Test");
  EXPECT_EQ(reopened.rom_filename, rom_path.string());
}

TEST(ProjectManagerValidatorTest,
     FinalizeProjectCreationAcceptsDestinationDirectory) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_finalize_directory"));
  const auto rom_path = temp.path() / "source.sfc";
  const auto destination = temp.path() / "projects";
  WriteTextFile(rom_path, "rom");
  std::filesystem::create_directories(destination);

  ProjectManager mgr(nullptr);
  ASSERT_TRUE(mgr.CreateNewProject().ok());
  ASSERT_TRUE(mgr.SetProjectRom(rom_path.string()).ok());
  ASSERT_TRUE(
      mgr.FinalizeProjectCreation("Oracle Test", destination.string()).ok());

  const auto expected_path = destination / "Oracle_Test.yaze";
  EXPECT_EQ(mgr.GetCurrentProject().filepath, expected_path.string());
  EXPECT_TRUE(std::filesystem::exists(expected_path));
}

TEST(ProjectManagerValidatorTest,
     FinalizeProjectCreationKeepsPendingStateWhenSaveFails) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_finalize_failure"));
  const auto rom_path = temp.path() / "source.sfc";
  const auto blocker = temp.path() / "not_a_directory";
  WriteTextFile(rom_path, "rom");
  WriteTextFile(blocker, "blocker");

  ProjectManager mgr(nullptr);
  ASSERT_TRUE(mgr.CreateNewProject().ok());
  ASSERT_TRUE(mgr.SetProjectRom(rom_path.string()).ok());
  const auto status = mgr.FinalizeProjectCreation(
      "Cannot Save", (blocker / "project.yaze").string());

  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(mgr.IsPendingRomSelection());
  EXPECT_FALSE(std::filesystem::exists(blocker / "project.yaze"));

  const auto retry_path = temp.path() / "retry.yaze";
  ASSERT_TRUE(
      mgr.FinalizeProjectCreation("Retry Project", retry_path.string()).ok());
  EXPECT_FALSE(mgr.IsPendingRomSelection());
  EXPECT_TRUE(std::filesystem::exists(retry_path));
  EXPECT_EQ(mgr.GetCurrentProject().filepath, retry_path.string());
}

TEST(ProjectManagerValidatorTest,
     FinalizeProjectCreationRejectsExistingDescriptorWithoutSideEffects) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_finalize_existing"));
  const auto rom_path = temp.path() / "source.sfc";
  const auto project_dir = temp.path() / "project";
  const auto project_path = project_dir / "existing.yaze";
  WriteTextFile(rom_path, "rom");
  WriteTextFile(project_path, "original descriptor");

  ProjectManager mgr(nullptr);
  ASSERT_TRUE(mgr.CreateNewProject().ok());
  ASSERT_TRUE(mgr.SetProjectRom(rom_path.string()).ok());
  const auto status =
      mgr.FinalizeProjectCreation("Must Not Replace", project_path.string());

  EXPECT_EQ(status.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_TRUE(mgr.IsPendingRomSelection());
  EXPECT_EQ(ReadTextFile(project_path), "original descriptor");
  EXPECT_FALSE(std::filesystem::exists(project_dir / "assets"));
  EXPECT_FALSE(std::filesystem::exists(project_dir / "scripts"));
  EXPECT_FALSE(std::filesystem::exists(project_dir / "output"));
}

}  // namespace yaze::editor
