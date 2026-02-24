// Unit tests for ProjectBundlePackCommandHandler and
// ProjectBundleUnpackCommandHandler.
//
// Uses temp directories with synthetic .yazeproj bundles.
// Includes round-trip test and path traversal security test.

#include "cli/handlers/rom/project_bundle_archive_commands.h"
#include "cli/handlers/rom/project_bundle_verify_commands.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include "miniz.h"
#include "nlohmann/json.hpp"

namespace yaze::cli {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

struct ScopedTempDir {
  fs::path path;
  ScopedTempDir() {
    path = fs::temp_directory_path() / ("yaze_pba_test_" +
           std::to_string(std::hash<std::string>{}(
               std::to_string(reinterpret_cast<uintptr_t>(this)))));
    fs::create_directories(path);
  }
  ~ScopedTempDir() {
    std::error_code fserr;
    fs::remove_all(path, fserr);
  }
};

void CreateBundle(const fs::path& bundle_dir) {
  fs::create_directories(bundle_dir / "project");
  fs::create_directories(bundle_dir / "backups");
  fs::create_directories(bundle_dir / "output");
  {
    std::ofstream file(bundle_dir / "project.yaze",
                       std::ios::out | std::ios::binary);
    file << "[project]\nname=ArchiveTest\n\n[files]\nrom_filename=rom\n";
  }
  {
    std::ofstream file(bundle_dir / "rom",
                       std::ios::out | std::ios::binary);
    std::vector<char> rom_data(0x8000, 0);
    file.write(rom_data.data(), static_cast<std::streamsize>(rom_data.size()));
  }
  {
    std::ofstream file(bundle_dir / "project" / "hook.asm",
                       std::ios::out | std::ios::binary);
    file << "; test hook\n";
  }
}

// ============================================================================
// Pack — validation
// ============================================================================

TEST(ProjectBundlePackTest, MissingProjectArgFails) {
  handlers::ProjectBundlePackCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--out=/tmp/x.zip", "--format=json"},
                            nullptr, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(ProjectBundlePackTest, MissingOutArgFails) {
  handlers::ProjectBundlePackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=/tmp/x.yazeproj", "--format=json"}, nullptr, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(ProjectBundlePackTest, NonYazeprojExtensionFails) {
  ScopedTempDir tmp;
  fs::path bad = tmp.path / "notabundle.dir";
  fs::create_directories(bad);

  handlers::ProjectBundlePackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + bad.string(), "--out=" + (tmp.path / "out.zip").string(),
       "--format=json"},
      nullptr, &out);
  EXPECT_FALSE(status.ok());
}

TEST(ProjectBundlePackTest, NonexistentProjectFails) {
  handlers::ProjectBundlePackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=/tmp/nope.yazeproj", "--out=/tmp/nope.zip",
       "--format=json"},
      nullptr, &out);
  EXPECT_FALSE(status.ok());
}

TEST(ProjectBundlePackTest, OutputExistsWithoutOverwriteFails) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "Test.yazeproj";
  CreateBundle(bundle);
  fs::path zip_out = tmp.path / "out.zip";
  { std::ofstream touch(zip_out); touch << "x"; }

  handlers::ProjectBundlePackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + bundle.string(), "--out=" + zip_out.string(),
       "--format=json"},
      nullptr, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kAlreadyExists);
}

// ============================================================================
// Pack — happy path
// ============================================================================

TEST(ProjectBundlePackTest, PacksValidBundle) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "Test.yazeproj";
  CreateBundle(bundle);
  fs::path zip_out = tmp.path / "Test.zip";

  handlers::ProjectBundlePackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + bundle.string(), "--out=" + zip_out.string(),
       "--format=json"},
      nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  EXPECT_TRUE(doc.value("ok", false));
  EXPECT_GT(doc.value("files_packed", 0), 0);
  EXPECT_GT(doc.value("archive_bytes", 0), 0);
  EXPECT_TRUE(fs::exists(zip_out));
}

// ============================================================================
// Unpack — validation
// ============================================================================

TEST(ProjectBundleUnpackTest, MissingArchiveArgFails) {
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--out=/tmp/x", "--format=json"},
                            nullptr, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(ProjectBundleUnpackTest, MissingOutArgFails) {
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--archive=/tmp/x.zip", "--format=json"},
                            nullptr, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(ProjectBundleUnpackTest, NonexistentArchiveFails) {
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=/tmp/nope.zip", "--out=/tmp/nope_dir", "--format=json"},
      nullptr, &out);
  EXPECT_FALSE(status.ok());
}

// ============================================================================
// Unpack — path traversal security
// ============================================================================

TEST(ProjectBundleUnpackTest, RejectsPathTraversal) {
  ScopedTempDir tmp;

  // Craft a malicious zip with "../evil.txt" entry using raw miniz API.
  fs::path malicious_zip = tmp.path / "evil.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(mz_zip_writer_init_file(&zip, malicious_zip.string().c_str(),
                                         0));
    const char* payload = "malicious content";
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "../evil.txt", payload,
                                       std::strlen(payload),
                                       MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }
  ASSERT_TRUE(fs::exists(malicious_zip));

  fs::path unpack_dir = tmp.path / "unpack_target";
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + malicious_zip.string(),
       "--out=" + unpack_dir.string(), "--format=json"},
      nullptr, &out);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  // Verify nothing was written outside the target directory
  EXPECT_FALSE(fs::exists(tmp.path / "evil.txt"))
      << "Path traversal: file escaped target directory";
}

TEST(ProjectBundleUnpackTest, RejectsNestedTraversal) {
  ScopedTempDir tmp;

  // Craft a zip with a deeply nested ".." traversal that miniz accepts.
  fs::path nested_zip = tmp.path / "nested_evil.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(mz_zip_writer_init_file(&zip,
                                         nested_zip.string().c_str(), 0));
    const char* payload = "escaped";
    ASSERT_TRUE(mz_zip_writer_add_mem(
        &zip, "bundle/../../../escaped.txt", payload, std::strlen(payload),
        MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }

  fs::path unpack_dir = tmp.path / "unpack_nested";
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + nested_zip.string(),
       "--out=" + unpack_dir.string(), "--format=json"},
      nullptr, &out);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  // Verify nothing escaped
  EXPECT_FALSE(fs::exists(tmp.path / "escaped.txt"));
}

TEST(ProjectBundleUnpackTest, TraversalAfterValidEntryCleansUpByDefault) {
  ScopedTempDir tmp;

  fs::path mixed_zip = tmp.path / "mixed.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(
        mz_zip_writer_init_file(&zip, mixed_zip.string().c_str(), 0));
    const char* payload = "ok";
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "good.txt", payload,
                                      std::strlen(payload),
                                      MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "../evil.txt", payload,
                                      std::strlen(payload),
                                      MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }

  fs::path unpack_dir = tmp.path / "unpack_mixed";
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + mixed_zip.string(),
       "--out=" + unpack_dir.string(), "--format=json"},
      nullptr, &out);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_EQ(doc.value("cleanup", ""), "removed");
  EXPECT_FALSE(fs::exists(unpack_dir));
}

TEST(ProjectBundleUnpackTest, TraversalAfterValidEntryKeepPartialPreservesOutput) {
  ScopedTempDir tmp;

  fs::path mixed_zip = tmp.path / "mixed_keep.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(
        mz_zip_writer_init_file(&zip, mixed_zip.string().c_str(), 0));
    const char* payload = "ok";
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "good.txt", payload,
                                      std::strlen(payload),
                                      MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "../evil.txt", payload,
                                      std::strlen(payload),
                                      MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }

  fs::path unpack_dir = tmp.path / "unpack_mixed_keep";
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + mixed_zip.string(),
       "--out=" + unpack_dir.string(),
       "--keep-partial-output", "--format=json"},
      nullptr, &out);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_EQ(doc.value("cleanup", ""), "skipped (--keep-partial-output)");
  EXPECT_TRUE(fs::exists(unpack_dir / "good.txt"));
}

TEST(ProjectBundleUnpackTest, NonBundleZipFails) {
  ScopedTempDir tmp;

  // Create a plain zip (no .yazeproj structure)
  fs::path plain_zip = tmp.path / "plain.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(mz_zip_writer_init_file(&zip, plain_zip.string().c_str(), 0));
    const char* payload = "just a text file";
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "foo.txt", payload,
                                       std::strlen(payload),
                                       MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }

  fs::path unpack_dir = tmp.path / "unpack_plain";
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + plain_zip.string(),
       "--out=" + unpack_dir.string(), "--format=json"},
      nullptr, &out);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_FALSE(doc.value("ok", true));
  EXPECT_FALSE(doc.value("is_valid_bundle", true));
}

// ============================================================================
// Round-trip: pack -> unpack -> verify
// ============================================================================

TEST(ProjectBundleArchiveTest, RoundTripPackUnpackVerify) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "RoundTrip.yazeproj";
  CreateBundle(bundle);
  fs::path zip_out = tmp.path / "RoundTrip.zip";
  fs::path unpack_dir = tmp.path / "unpacked";

  // Step 1: Pack
  {
    handlers::ProjectBundlePackCommandHandler handler;
    std::string out;
    auto status = handler.Run(
        {"--project=" + bundle.string(), "--out=" + zip_out.string(),
         "--format=json"},
        nullptr, &out);
    ASSERT_TRUE(status.ok()) << "Pack failed: " << status.message();
    ASSERT_TRUE(fs::exists(zip_out));
  }

  // Step 2: Unpack
  {
    handlers::ProjectBundleUnpackCommandHandler handler;
    std::string out;
    auto status = handler.Run(
        {"--archive=" + zip_out.string(), "--out=" + unpack_dir.string(),
         "--format=json"},
        nullptr, &out);
    ASSERT_TRUE(status.ok()) << "Unpack failed: " << status.message()
                             << "\n" << out;

    auto doc = json::parse(out, nullptr, false);
    ASSERT_FALSE(doc.is_discarded());
    EXPECT_TRUE(doc.value("ok", false));
    EXPECT_TRUE(doc.value("is_valid_bundle", false));
    EXPECT_GT(doc.value("files_extracted", 0), 0);
  }

  // Step 3: Verify the unpacked bundle
  {
    fs::path unpacked_bundle = unpack_dir / "RoundTrip.yazeproj";
    ASSERT_TRUE(fs::is_directory(unpacked_bundle))
        << "Unpacked bundle not found: " << unpacked_bundle;

    handlers::ProjectBundleVerifyCommandHandler handler;
    std::string out;
    auto status = handler.Run(
        {"--project=" + unpacked_bundle.string(), "--format=json"},
        nullptr, &out);
    EXPECT_TRUE(status.ok()) << "Verify failed: " << status.message()
                             << "\n" << out;

    auto doc = json::parse(out, nullptr, false);
    ASSERT_FALSE(doc.is_discarded());
    EXPECT_TRUE(doc.value("ok", false));
    EXPECT_EQ(doc.value("fail_count", -1), 0);
  }

  // Step 4: Verify key files survived round-trip
  fs::path unpacked_bundle = unpack_dir / "RoundTrip.yazeproj";
  EXPECT_TRUE(fs::exists(unpacked_bundle / "project.yaze"));
  EXPECT_TRUE(fs::exists(unpacked_bundle / "rom"));
  EXPECT_TRUE(fs::exists(unpacked_bundle / "project" / "hook.asm"));

  // ROM size preserved
  std::error_code fserr;
  EXPECT_EQ(fs::file_size(unpacked_bundle / "rom", fserr), 0x8000u);
}

TEST(ProjectBundlePackTest, OverwriteAllowsReplace) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "OW.yazeproj";
  CreateBundle(bundle);
  fs::path zip_out = tmp.path / "OW.zip";

  // First pack
  {
    handlers::ProjectBundlePackCommandHandler handler;
    std::string out;
    handler.Run(
        {"--project=" + bundle.string(), "--out=" + zip_out.string(),
         "--format=json"},
        nullptr, &out);
  }

  // Second pack with --overwrite
  {
    handlers::ProjectBundlePackCommandHandler handler;
    std::string out;
    auto status = handler.Run(
        {"--project=" + bundle.string(), "--out=" + zip_out.string(),
         "--overwrite", "--format=json"},
        nullptr, &out);
    EXPECT_TRUE(status.ok()) << status.message();
  }
}

// ============================================================================
// Cross-platform entry name safety
// ============================================================================

TEST(ProjectBundlePackTest, ZipEntryNamesUseForwardSlashes) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "Slash.yazeproj";
  CreateBundle(bundle);
  fs::path zip_out = tmp.path / "Slash.zip";

  handlers::ProjectBundlePackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + bundle.string(), "--out=" + zip_out.string(),
       "--format=json"},
      nullptr, &out);
  ASSERT_TRUE(status.ok()) << status.message();

  // Read zip and verify all entry names use forward slashes only
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof(zip));
  ASSERT_TRUE(mz_zip_reader_init_file(&zip, zip_out.string().c_str(), 0));

  int num_files = static_cast<int>(mz_zip_reader_get_num_files(&zip));
  ASSERT_GT(num_files, 0);

  for (int idx = 0; idx < num_files; ++idx) {
    mz_zip_archive_file_stat file_stat;
    ASSERT_TRUE(mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(idx),
                                         &file_stat));
    std::string entry_name(file_stat.m_filename);
    EXPECT_EQ(entry_name.find('\\'), std::string::npos)
        << "Backslash in zip entry: " << entry_name;
  }
  mz_zip_reader_end(&zip);
}

// ============================================================================
// Overwrite removes stale files deterministically
// ============================================================================

TEST(ProjectBundleUnpackTest, OverwriteClearsStaleFiles) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "Stale.yazeproj";
  CreateBundle(bundle);
  fs::path zip_out = tmp.path / "Stale.zip";
  fs::path unpack_dir = tmp.path / "unpack_stale";

  // Pack and unpack first time
  {
    handlers::ProjectBundlePackCommandHandler handler;
    std::string out;
    handler.Run({"--project=" + bundle.string(), "--out=" + zip_out.string(),
                 "--format=json"}, nullptr, &out);
  }
  {
    handlers::ProjectBundleUnpackCommandHandler handler;
    std::string out;
    handler.Run({"--archive=" + zip_out.string(),
                 "--out=" + unpack_dir.string(), "--format=json"},
                nullptr, &out);
  }

  // Plant a stale file in the unpacked output
  fs::path stale_file = unpack_dir / "Stale.yazeproj" / "STALE_FILE.txt";
  { std::ofstream stale(stale_file); stale << "should be removed"; }
  ASSERT_TRUE(fs::exists(stale_file));

  // Unpack again with --overwrite
  {
    handlers::ProjectBundleUnpackCommandHandler handler;
    std::string out;
    auto status = handler.Run(
        {"--archive=" + zip_out.string(), "--out=" + unpack_dir.string(),
         "--overwrite", "--format=json"},
        nullptr, &out);
    EXPECT_TRUE(status.ok()) << status.message();
  }

  // Stale file should be gone
  EXPECT_FALSE(fs::exists(stale_file))
      << "Stale file survived --overwrite: " << stale_file;

  // Original files should still exist
  EXPECT_TRUE(fs::exists(unpack_dir / "Stale.yazeproj" / "project.yaze"));
  EXPECT_TRUE(fs::exists(unpack_dir / "Stale.yazeproj" / "rom"));
}

// ============================================================================
// Non-bundle zip: reports failure + invalid bundle payload.
// ============================================================================

TEST(ProjectBundleUnpackTest, NonBundleZipCleansUpByDefault) {
  ScopedTempDir tmp;

  // Create a plain (non-bundle) zip
  fs::path plain_zip = tmp.path / "partial.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(mz_zip_writer_init_file(&zip, plain_zip.string().c_str(), 0));
    const char* payload = "plain file";
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "just_a_file.txt", payload,
                                       std::strlen(payload),
                                       MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }

  fs::path unpack_dir = tmp.path / "unpack_partial";
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + plain_zip.string(),
       "--out=" + unpack_dir.string(), "--format=json"},
      nullptr, &out);

  // Should fail (not a bundle)
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_FALSE(doc.value("ok", true));
  EXPECT_EQ(doc.value("status", ""), "fail");
  EXPECT_EQ(doc.value("cleanup", ""), "removed");

  // Output directory should be cleaned up by default.
  EXPECT_FALSE(fs::exists(unpack_dir))
      << "Output directory should be removed on invalid bundle";
}

TEST(ProjectBundleUnpackTest, KeepPartialOutputPreservesFilesOnFailure) {
  ScopedTempDir tmp;

  // Create a plain (non-bundle) zip
  fs::path plain_zip = tmp.path / "keep.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(mz_zip_writer_init_file(&zip, plain_zip.string().c_str(), 0));
    const char* payload = "debug content";
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "debug.txt", payload,
                                       std::strlen(payload),
                                       MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }

  fs::path unpack_dir = tmp.path / "unpack_keep";
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + plain_zip.string(),
       "--out=" + unpack_dir.string(),
       "--keep-partial-output", "--format=json"},
      nullptr, &out);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_EQ(doc.value("cleanup", ""), "skipped (--keep-partial-output)");

  // Output directory should still exist with extracted files.
  EXPECT_TRUE(fs::exists(unpack_dir))
      << "Output directory should be preserved with --keep-partial-output";
  EXPECT_TRUE(fs::exists(unpack_dir / "debug.txt"));
}

// ============================================================================
// --dry-run tests
// ============================================================================

TEST(ProjectBundleUnpackTest, DryRunDoesNotCreateFiles) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "DryRun.yazeproj";
  CreateBundle(bundle);
  fs::path zip_out = tmp.path / "DryRun.zip";
  fs::path unpack_dir = tmp.path / "unpack_dry";

  // Pack first
  {
    handlers::ProjectBundlePackCommandHandler handler;
    std::string out;
    handler.Run({"--project=" + bundle.string(), "--out=" + zip_out.string(),
                 "--format=json"}, nullptr, &out);
  }

  // Unpack with --dry-run
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + zip_out.string(), "--out=" + unpack_dir.string(),
       "--dry-run", "--format=json"},
      nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_TRUE(doc.value("ok", false));
  EXPECT_TRUE(doc.value("dry_run", false));
  EXPECT_TRUE(doc.value("is_valid_bundle", false));
  EXPECT_GT(doc.value("files_counted", 0), 0);
  EXPECT_EQ(doc.value("files_extracted", -1), 0);

  // Output directory must NOT exist
  EXPECT_FALSE(fs::exists(unpack_dir))
      << "dry-run must not create output directory";
}

TEST(ProjectBundleUnpackTest, DryRunOverwriteDoesNotDeleteExistingOutput) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "DryRunKeep.yazeproj";
  CreateBundle(bundle);
  fs::path zip_out = tmp.path / "DryRunKeep.zip";
  fs::path unpack_dir = tmp.path / "existing_output";
  fs::create_directories(unpack_dir);
  const fs::path sentinel = unpack_dir / "keep_me.txt";
  {
    std::ofstream out(sentinel, std::ios::out | std::ios::binary);
    out << "keep";
  }

  // Pack a valid bundle.
  {
    handlers::ProjectBundlePackCommandHandler handler;
    std::string out;
    auto status = handler.Run(
        {"--project=" + bundle.string(), "--out=" + zip_out.string(),
         "--format=json"},
        nullptr, &out);
    ASSERT_TRUE(status.ok()) << status.message() << "\n" << out;
  }

  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + zip_out.string(), "--out=" + unpack_dir.string(),
       "--dry-run", "--overwrite", "--format=json"},
      nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_TRUE(doc.value("ok", false));
  EXPECT_EQ(doc.value("files_extracted", -1), 0);
  EXPECT_TRUE(fs::exists(unpack_dir));
  EXPECT_TRUE(fs::exists(sentinel))
      << "dry-run must not delete or mutate existing output";
}

TEST(ProjectBundleUnpackTest, DryRunAllowsDoubleDotInsideFilenameComponent) {
  ScopedTempDir tmp;
  fs::path zip_path = tmp.path / "double_dot_name.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(mz_zip_writer_init_file(&zip, zip_path.string().c_str(), 0));
    const char* payload = "x";
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "Good.yazeproj/project.yaze", payload,
                                      std::strlen(payload),
                                      MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "Good.yazeproj/a..b.txt", payload,
                                      std::strlen(payload),
                                      MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }

  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + zip_path.string(), "--out=" + (tmp.path / "out").string(),
       "--dry-run", "--format=json"},
      nullptr, &out);

  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;
  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_TRUE(doc.value("ok", false));
  EXPECT_TRUE(doc.value("is_valid_bundle", false));
}

TEST(ProjectBundleUnpackTest, DryRunDetectsTraversal) {
  ScopedTempDir tmp;

  fs::path evil_zip = tmp.path / "dry_evil.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(mz_zip_writer_init_file(&zip, evil_zip.string().c_str(), 0));
    const char* payload = "bad";
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "../escape.txt", payload,
                                       std::strlen(payload),
                                       MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }

  fs::path unpack_dir = tmp.path / "unpack_dry_evil";
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + evil_zip.string(), "--out=" + unpack_dir.string(),
       "--dry-run", "--format=json"},
      nullptr, &out);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  // Nothing should be written
  EXPECT_FALSE(fs::exists(unpack_dir));
}

TEST(ProjectBundleUnpackTest, DryRunNonBundleReportsFail) {
  ScopedTempDir tmp;

  fs::path plain_zip = tmp.path / "dry_plain.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(mz_zip_writer_init_file(&zip, plain_zip.string().c_str(), 0));
    const char* payload = "just text";
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "readme.txt", payload,
                                       std::strlen(payload),
                                       MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }

  fs::path unpack_dir = tmp.path / "unpack_dry_plain";
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + plain_zip.string(), "--out=" + unpack_dir.string(),
       "--dry-run", "--format=json"},
      nullptr, &out);

  EXPECT_FALSE(status.ok());

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_FALSE(doc.value("ok", true));
  EXPECT_FALSE(doc.value("is_valid_bundle", true));
  EXPECT_TRUE(doc.value("dry_run", false));
  EXPECT_FALSE(fs::exists(unpack_dir));
}

TEST(ProjectBundleUnpackTest, DryRunMixedRootsReportsFail) {
  ScopedTempDir tmp;

  fs::path mixed_zip = tmp.path / "dry_mixed_roots.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(mz_zip_writer_init_file(&zip, mixed_zip.string().c_str(), 0));
    const char* payload = "x";
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "A.yazeproj/project.yaze", payload,
                                       std::strlen(payload),
                                       MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "B.yazeproj/rom", payload,
                                       std::strlen(payload),
                                       MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }

  fs::path unpack_dir = tmp.path / "unpack_dry_mixed";
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + mixed_zip.string(), "--out=" + unpack_dir.string(),
       "--dry-run", "--format=json"},
      nullptr, &out);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_FALSE(doc.value("is_valid_bundle", true));
  EXPECT_EQ(doc.value("error", ""), "Archive contains multiple root folders");
  EXPECT_FALSE(fs::exists(unpack_dir));
}

TEST(ProjectBundleUnpackTest, DryRunMissingProjectYazeReportsFail) {
  ScopedTempDir tmp;

  fs::path no_proj_zip = tmp.path / "dry_missing_project_yaze.zip";
  {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    ASSERT_TRUE(
        mz_zip_writer_init_file(&zip, no_proj_zip.string().c_str(), 0));
    const char* payload = "x";
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip, "NoProj.yazeproj/readme.txt",
                                       payload, std::strlen(payload),
                                       MZ_DEFAULT_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));
    mz_zip_writer_end(&zip);
  }

  fs::path unpack_dir = tmp.path / "unpack_dry_missing_project";
  handlers::ProjectBundleUnpackCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--archive=" + no_proj_zip.string(), "--out=" + unpack_dir.string(),
       "--dry-run", "--format=json"},
      nullptr, &out);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  EXPECT_FALSE(doc.value("is_valid_bundle", true));
  EXPECT_EQ(doc.value("error", ""),
            "Bundle is missing required root file: project.yaze");
  EXPECT_FALSE(fs::exists(unpack_dir));
}

}  // namespace
}  // namespace yaze::cli
