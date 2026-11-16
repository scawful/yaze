// Opening a project directly (no launcher script) must give the same setup the
// Oracle launcher did: per-project app-data/proposal/sandbox roots, and a ROM
// hash refresh that also keeps the bundle manifest in step.

#include "core/project_workspace.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/strings/match.h"
#include "core/project.h"
#include "unique_temp_path.h"
#include "util/flag.h"
#include "util/rom_hash.h"

namespace yaze::project {
namespace {

namespace fs = std::filesystem;
using ::testing::HasSubstr;

class ScopedDir {
 public:
  explicit ScopedDir(fs::path path) : path_(std::move(path)) {
    fs::create_directories(path_);
  }
  ~ScopedDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

// Saves an environment variable and restores it on scope exit.
class ScopedEnv {
 public:
  explicit ScopedEnv(const char* name) : name_(name) {
    if (const char* value = std::getenv(name)) {
      saved_ = value;
    }
  }
  ~ScopedEnv() {
#ifndef _WIN32
    if (saved_) {
      setenv(name_, saved_->c_str(), 1);
    } else {
      unsetenv(name_);
    }
#endif
  }
  void Unset() {
#ifndef _WIN32
    unsetenv(name_);
#endif
  }
  void Set(const std::string& value) {
#ifndef _WIN32
    setenv(name_, value.c_str(), 1);
#endif
  }

 private:
  const char* name_;
  std::optional<std::string> saved_;
};

std::string ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void WriteFile(const fs::path& path, const std::string& content) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << content;
}

constexpr char kProjectText[] = R"(# yaze project
[project]
name=Direct Open

[rom]
role=dev
expected_hash=0123456789abcdef0123456789abcdef01234567
write_policy=block
adopt_hash_on_open=true
editor_lock_file=../.rom-editor.lock
editor_control=true

[workspace]
app_data_folder=.workspace/yaze
proposal_folder=.workspace/proposals
sandbox_folder=/abs/sandboxes
font_global_scale=1.25
)";

TEST(ProjectWorkspaceTest, ProjectKeysRoundTrip) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_direct_open_keys"));
  const fs::path path = dir.path() / "Direct.yaze";
  WriteFile(path, kProjectText);

  YazeProject project;
  ASSERT_TRUE(project.Open(path.string()).ok());
  EXPECT_TRUE(project.rom_metadata.adopt_hash_on_open);
  EXPECT_EQ(project.rom_metadata.editor_lock_file, "../.rom-editor.lock");
  EXPECT_TRUE(project.rom_metadata.editor_control);
  EXPECT_EQ(project.workspace_folders.app_data_folder, ".workspace/yaze");
  EXPECT_EQ(project.workspace_folders.proposal_folder, ".workspace/proposals");
  EXPECT_EQ(project.workspace_folders.sandbox_folder, "/abs/sandboxes");

  ASSERT_TRUE(project.Save().ok());
  const std::string saved = ReadFile(path);
  EXPECT_THAT(saved, HasSubstr("adopt_hash_on_open=true\n"));
  EXPECT_THAT(saved, HasSubstr("editor_lock_file=../.rom-editor.lock\n"));
  EXPECT_THAT(saved, HasSubstr("editor_control=true\n"));
  // The folders share the existing [workspace] section; no second one.
  const auto first = saved.find("[workspace]");
  ASSERT_NE(first, std::string::npos);
  EXPECT_EQ(saved.find("[workspace]", first + 1), std::string::npos) << saved;
  EXPECT_THAT(saved, HasSubstr("app_data_folder=.workspace/yaze\n"));

  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(path.string()).ok());
  EXPECT_TRUE(reopened.rom_metadata.adopt_hash_on_open);
  EXPECT_EQ(reopened.rom_metadata.editor_lock_file, "../.rom-editor.lock");
  EXPECT_TRUE(reopened.rom_metadata.editor_control);
  EXPECT_EQ(reopened.workspace_folders.sandbox_folder, "/abs/sandboxes");
  EXPECT_FLOAT_EQ(reopened.workspace_settings.font_global_scale, 1.25f);
}

TEST(ProjectWorkspaceTest, UnsetKeysAreNotWritten) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_direct_open_unset"));
  const fs::path path = dir.path() / "Plain.yaze";
  WriteFile(path, "[project]\nname=Plain\n\n[rom]\nrole=dev\n");
  YazeProject project;
  ASSERT_TRUE(project.Open(path.string()).ok());
  EXPECT_FALSE(project.rom_metadata.adopt_hash_on_open);
  ASSERT_TRUE(project.Save().ok());
  const std::string saved = ReadFile(path);
  EXPECT_EQ(saved.find("adopt_hash_on_open"), std::string::npos);
  EXPECT_EQ(saved.find("editor_lock_file"), std::string::npos);
  EXPECT_EQ(saved.find("editor_control"), std::string::npos);
  EXPECT_EQ(saved.find("app_data_folder"), std::string::npos);
}

TEST(ProjectWorkspaceTest, EditorControlReadsOnlyTheRomSection) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_editor_control"));
  const fs::path on = dir.path() / "On.yaze";
  WriteFile(on, kProjectText);
  EXPECT_TRUE(ReadProjectEditorControl(on));

  const fs::path off = dir.path() / "Off.yaze";
  WriteFile(off, "[rom]\nrole=dev\neditor_control=false\n");
  EXPECT_FALSE(ReadProjectEditorControl(off));

  // The same key in another section does not count.
  const fs::path elsewhere = dir.path() / "Elsewhere.yaze";
  WriteFile(elsewhere, "[rom]\nrole=dev\n[workspace]\neditor_control=true\n");
  EXPECT_FALSE(ReadProjectEditorControl(elsewhere));

  EXPECT_FALSE(ReadProjectEditorControl(dir.path() / "Missing.yaze"));
}

TEST(ProjectWorkspaceTest, EditorControlFlagOverridesProjectBothWays) {
  // No flag: the project decides.
  EXPECT_TRUE(ResolveEditorControl(false, false, true));
  EXPECT_FALSE(ResolveEditorControl(false, false, false));
  // --editor_control=false turns a project's setting off.
  EXPECT_FALSE(ResolveEditorControl(true, false, true));
  // --editor_control=true turns it on for a project without the setting.
  EXPECT_TRUE(ResolveEditorControl(true, true, false));
}

TEST(ProjectWorkspaceTest, FlagParserMarksOnlyGivenFlagsSpecified) {
  yaze::util::FlagRegistry registry;
  auto* given_off = registry.RegisterFlag<bool>("--given_off", false, "");
  auto* given_bare = registry.RegisterFlag<bool>("--given_bare", false, "");
  auto* absent = registry.RegisterFlag<bool>("--absent", false, "");
  std::vector<std::string> tokens = {"yaze", "--given_off=false",
                                     "--given_bare"};
  yaze::util::FlagParser(&registry).Parse(&tokens);
  EXPECT_TRUE(given_off->specified());
  EXPECT_FALSE(given_off->Get());
  EXPECT_TRUE(given_bare->specified());
  EXPECT_TRUE(given_bare->Get());
  EXPECT_FALSE(absent->specified());
}

TEST(ProjectWorkspaceTest, BundlePathResolvesToDescriptor) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_direct_open_bundle"));
  const fs::path bundle = dir.path() / "Oracle.yazeproj";
  fs::create_directories(bundle);
  EXPECT_EQ(ProjectFileForPath(bundle), bundle / "project.yaze");
  EXPECT_EQ(ProjectFileForPath(dir.path() / "x.sfc"), dir.path() / "x.sfc");
}

TEST(ProjectWorkspaceTest, ReadFoldersResolvesAgainstProjectDir) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_direct_open_read"));
  const fs::path bundle = dir.path() / "Oracle.yazeproj";
  fs::create_directories(bundle);
  WriteFile(bundle / "project.yaze", kProjectText);
  const auto folders = ReadProjectWorkspaceFolders(bundle / "project.yaze");
  EXPECT_EQ(folders.app_data_folder, (bundle / ".workspace/yaze").string());
  EXPECT_EQ(folders.proposal_folder,
            (bundle / ".workspace/proposals").string());
  EXPECT_EQ(folders.sandbox_folder, "/abs/sandboxes");
}

#ifndef _WIN32
TEST(ProjectWorkspaceTest, ApplySetsUnsetVariablesAndCreatesFolders) {
  ScopedEnv app_data(kAppDataDirEnv);
  ScopedEnv proposals(kProposalRootEnv);
  ScopedEnv sandboxes(kSandboxRootEnv);
  ScopedDir dir(yaze::test::UniqueTempPath("project_direct_open_apply"));
  const fs::path bundle = dir.path() / "Oracle.yazeproj";
  fs::create_directories(bundle);
  WriteFile(bundle / "project.yaze",
            "[project]\nname=Oracle\n\n[workspace]\n"
            "app_data_folder=.workspace/yaze\n"
            "proposal_folder=.workspace/proposals\n"
            "sandbox_folder=.workspace/sandboxes\n");
  app_data.Unset();
  proposals.Unset();
  sandboxes.Set("/already/set");

  const auto applied = ApplyProjectWorkspaceEnvironment(bundle);
  ASSERT_EQ(applied.size(), 2u);
  EXPECT_EQ(std::string(std::getenv(kAppDataDirEnv)),
            (bundle / ".workspace/yaze").string());
  EXPECT_EQ(std::string(std::getenv(kProposalRootEnv)),
            (bundle / ".workspace/proposals").string());
  EXPECT_EQ(std::string(std::getenv(kSandboxRootEnv)), "/already/set");
  EXPECT_TRUE(fs::is_directory(bundle / ".workspace/yaze"));
  EXPECT_TRUE(fs::is_directory(bundle / ".workspace/proposals"));
  EXPECT_FALSE(fs::exists(bundle / ".workspace/sandboxes"));
}

TEST(ProjectWorkspaceTest, ApplyIgnoresRomFiles) {
  ScopedEnv app_data(kAppDataDirEnv);
  app_data.Unset();
  ScopedDir dir(yaze::test::UniqueTempPath("project_direct_open_rom"));
  WriteFile(dir.path() / "game.sfc", "not a project");
  EXPECT_TRUE(
      ApplyProjectWorkspaceEnvironment(dir.path() / "game.sfc").empty());
  EXPECT_EQ(std::getenv(kAppDataDirEnv), nullptr);
}
#endif

TEST(ProjectWorkspaceTest, ManifestChecksumUpdatesExactlyOneEntry) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_direct_open_manifest"));
  const std::string old_sha = "0123456789abcdef0123456789abcdef01234567";
  // Starts with a digit: a "$1"+sha replacement would read it as group $13.
  const std::string new_sha = "370c358886ea6b70340780134b915f8a7fbb1082";
  WriteFile(dir.path() / "manifest.json",
            "{\n  \"name\": \"Oracle\",\n  \"romChecksum\": \"" + old_sha +
                "\",\n  \"other\": 1\n}\n");
  ASSERT_TRUE(UpdateBundleManifestRomChecksum(dir.path(), new_sha).ok());
  EXPECT_EQ(ReadFile(dir.path() / "manifest.json"),
            "{\n  \"name\": \"Oracle\",\n  \"romChecksum\": \"" + new_sha +
                "\",\n  \"other\": 1\n}\n");
  EXPECT_FALSE(fs::exists(dir.path() / "manifest.json.tmp"));
}

TEST(ProjectWorkspaceTest, ManifestChecksumRefusesAmbiguousOrMissing) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_direct_open_manifest_bad"));
  const std::string sha = "fedcba9876543210fedcba9876543210fedcba98";
  EXPECT_EQ(UpdateBundleManifestRomChecksum(dir.path(), sha).code(),
            absl::StatusCode::kNotFound);

  const std::string none = "{\"name\": \"Oracle\"}\n";
  WriteFile(dir.path() / "manifest.json", none);
  EXPECT_EQ(UpdateBundleManifestRomChecksum(dir.path(), sha).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(ReadFile(dir.path() / "manifest.json"), none);

  const std::string two =
      "{\"romChecksum\": \"aa\", \"backup\": {\"romChecksum\": \"bb\"}}\n";
  WriteFile(dir.path() / "manifest.json", two);
  EXPECT_EQ(UpdateBundleManifestRomChecksum(dir.path(), sha).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(ReadFile(dir.path() / "manifest.json"), two);

  EXPECT_EQ(UpdateBundleManifestRomChecksum(dir.path(), "1234").code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ProjectWorkspaceTest, RecordOnOpenWritesSha1LikeTheLauncher) {
  const std::string sha1 = "370c358886ea6b70340780134b915f8a7fbb1082";
  const std::string crc32 = "f45a16af";
  // ROM changed: record its SHA-1.
  EXPECT_EQ(RomHashToRecordOnOpen(std::string(40, '1'), sha1, crc32), sha1);
  // A yaze save left the CRC32 of the same ROM: still rewrite as SHA-1.
  EXPECT_EQ(RomHashToRecordOnOpen(crc32, sha1, crc32), sha1);
  EXPECT_EQ(RomHashToRecordOnOpen("0xF45A16AF", sha1, crc32), sha1);
  // Already recorded (any case or spacing): nothing to write.
  EXPECT_EQ(RomHashToRecordOnOpen(sha1, sha1, crc32), "");
  EXPECT_EQ(RomHashToRecordOnOpen(" 370C358886EA6B70340780134B915F8A7FBB1082 ",
                                  sha1, crc32),
            "");
  // No hash recorded means no check is wanted; do not add one.
  EXPECT_EQ(RomHashToRecordOnOpen("", sha1, crc32), "");
  // No SHA-1 known: fall back to CRC32.
  EXPECT_EQ(RomHashToRecordOnOpen("00000000", "", crc32), crc32);
  EXPECT_EQ(RomHashToRecordOnOpen(crc32, "", crc32), "");
  EXPECT_EQ(RomHashToRecordOnOpen(crc32, "", ""), "");
}

TEST(ProjectWorkspaceTest, UpdateProjectDescriptorKeyUpdatesOnlyTargetKey) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_descriptor_key_update"));
  const fs::path path = dir.path() / "project.yaze";
  const std::string initial =
      "# yaze Project File\n"
      "# Last Modified: 2026-11-16 13:24:06\n"
      "\n"
      "[project]\n"
      "name=Oracle-of-Secrets\n"
      "last_modified=2026-11-16 13:24:06\n"
      "\n"
      "[rom]\n"
      "role=dev\n"
      "expected_hash=6b4b73eb\n"
      "adopt_hash_on_open=false\n"
      "\n"
      "[graphics_sheets]\n"
      "source_spec=Data/graphics/d4_zora_sprite_sheets.json\n"
      "source_spec_sha1=1111111111111111111111111111111111111111\n"
      "# scawful 2026-09-25: gfx 0x7B/0x7C must stay empty.\n"
      "reserved_sheets=0x7B,0x7C\n";
  WriteFile(path, initial);

  auto updated_hash =
      UpdateProjectDescriptorKey(path, "rom", "expected_hash", "cafebabe");
  ASSERT_TRUE(updated_hash.ok()) << updated_hash.status();
  EXPECT_EQ(ReadFile(path), *updated_hash);
  EXPECT_THAT(*updated_hash,
              HasSubstr("# Last Modified: 2026-11-16 13:24:06\n"));
  EXPECT_THAT(*updated_hash, HasSubstr("last_modified=2026-11-16 13:24:06\n"));
  EXPECT_THAT(*updated_hash, HasSubstr("expected_hash=cafebabe\n"));
  EXPECT_THAT(
      *updated_hash,
      HasSubstr("# scawful 2026-09-25: gfx 0x7B/0x7C must stay empty.\n"));

  auto updated_sha =
      UpdateProjectDescriptorKey(path, "graphics_sheets", "source_spec_sha1",
                                 "2222222222222222222222222222222222222222");
  ASSERT_TRUE(updated_sha.ok()) << updated_sha.status();
  EXPECT_THAT(
      *updated_sha,
      HasSubstr("source_spec_sha1=2222222222222222222222222222222222222222\n"));
  EXPECT_THAT(*updated_sha, HasSubstr("expected_hash=cafebabe\n"));
  EXPECT_THAT(*updated_sha, HasSubstr("reserved_sheets=0x7B,0x7C\n"));
  EXPECT_FALSE(fs::exists(path.string() + ".tmp"));

  // Missing key inside an existing section is inserted in that section.
  auto inserted_key =
      UpdateProjectDescriptorKey(path, "rom", "custom_tag", "alpha");
  ASSERT_TRUE(inserted_key.ok()) << inserted_key.status();
  EXPECT_THAT(*inserted_key,
              HasSubstr("adopt_hash_on_open=false\ncustom_tag=alpha\n"));

  // Missing section is appended cleanly at the end.
  auto appended_section =
      UpdateProjectDescriptorKey(path, "extra_section", "flag_key", "true");
  ASSERT_TRUE(appended_section.ok()) << appended_section.status();
  EXPECT_THAT(*appended_section, HasSubstr("[extra_section]\nflag_key=true\n"));
}

TEST(ProjectWorkspaceTest, UpdateProjectDescriptorKeyRejectsJsonAndDuplicates) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_descriptor_key_reject"));
  const fs::path json_path = dir.path() / "project.yaze";
  const std::string json_text = "{\"project\": {\"name\": \"Json\"}}\n";
  WriteFile(json_path, json_text);
  auto json_res =
      UpdateProjectDescriptorKey(json_path, "rom", "expected_hash", "abcd");
  EXPECT_EQ(json_res.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_TRUE(absl::StrContains(json_res.status().message(),
                                "not an INI project file"));
  EXPECT_EQ(ReadFile(json_path), json_text);

  const fs::path dup_path = dir.path() / "dup.yaze";
  const std::string dup_text =
      "[rom]\nexpected_hash=11111111\nexpected_hash=22222222\n";
  WriteFile(dup_path, dup_text);
  auto dup_res =
      UpdateProjectDescriptorKey(dup_path, "rom", "expected_hash", "33333333");
  EXPECT_EQ(dup_res.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(ReadFile(dup_path), dup_text);

  EXPECT_EQ(
      UpdateProjectDescriptorKey(dir.path() / "missing.yaze", "rom", "k", "v")
          .status()
          .code(),
      absl::StatusCode::kFailedPrecondition);
}

TEST(ProjectWorkspaceTest, InspectRomIdentityMismatchReportsBackupsAndHashes) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_rom_mismatch_info"));
  const fs::path bundle = dir.path() / "Oracle.yazeproj";
  const fs::path backups = bundle / "backups";
  fs::create_directories(backups);

  const fs::path rom_path = bundle / "rom";
  const std::vector<uint8_t> rom_bytes(0x8000, 0x3C);
  WriteFile(rom_path, std::string(rom_bytes.begin(), rom_bytes.end()));
  const auto id =
      yaze::util::ComputeRomIdentityHashes(rom_bytes.data(), rom_bytes.size());

  const fs::path older_backup = backups / "rom_old.sfc";
  const fs::path newer_backup = backups / "rom_new.sfc";
  const fs::path dot_file = backups / ".DS_Store";
  const fs::path tmp_file = backups / "rom_partial.tmp";
  WriteFile(older_backup, "old");
  WriteFile(newer_backup, "new");
  WriteFile(dot_file, "ignore");
  WriteFile(tmp_file, "ignore");

  const auto base_time = fs::last_write_time(older_backup);
  fs::last_write_time(newer_backup, base_time + std::chrono::seconds(10));
  fs::last_write_time(dot_file, base_time + std::chrono::seconds(20));
  fs::last_write_time(tmp_file, base_time + std::chrono::seconds(30));

  const fs::path proj_path = bundle / "project.yaze";
  WriteFile(proj_path,
            "[project]\nname=Oracle\n[files]\nrom_filename=rom\n"
            "[rom]\nexpected_hash=deadbeef\n");
  YazeProject project;
  ASSERT_TRUE(project.Open(proj_path.string()).ok());

  const auto crc_info = InspectRomIdentityMismatch(
      rom_bytes.data(), rom_bytes.size(), rom_path.string(), project);
  EXPECT_EQ(crc_info.expected_hash, "deadbeef");
  EXPECT_EQ(crc_info.loaded_hash, id.crc32);
  EXPECT_EQ(crc_info.loaded_sha1, id.sha1);
  EXPECT_NE(crc_info.rom_mtime, "unknown");
  EXPECT_THAT(crc_info.newest_backup, HasSubstr("rom_new.sfc ("));

  const std::string formatted = FormatRomIdentityMismatchFields(crc_info);
  EXPECT_THAT(formatted, HasSubstr("expected_hash=deadbeef"));
  EXPECT_THAT(formatted, HasSubstr("loaded_hash=" + id.crc32));
  EXPECT_THAT(formatted, HasSubstr("rom_mtime="));
  EXPECT_THAT(formatted, HasSubstr("newest_backup=rom_new.sfc ("));

  project.rom_metadata.expected_hash =
      "0000000000000000000000000000000000000000";
  const auto sha_info = InspectRomIdentityMismatch(
      rom_bytes.data(), rom_bytes.size(), rom_path.string(), project);
  EXPECT_EQ(sha_info.loaded_hash, id.sha1);
}

}  // namespace
}  // namespace yaze::project
