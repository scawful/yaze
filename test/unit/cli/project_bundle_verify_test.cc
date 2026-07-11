// Unit tests for ProjectBundleVerifyCommandHandler.
//
// Uses temp directories with synthetic .yazeproj bundles and .yaze files.
// No ROM fixture required.

#include "cli/handlers/rom/project_bundle_verify_commands.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "nlohmann/json.hpp"
#include "util/rom_hash.h"

namespace yaze::cli {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

// RAII temp directory.
struct ScopedTempDir {
  fs::path path;
  ScopedTempDir() {
    path = fs::temp_directory_path() /
           ("yaze_pbv_test_" +
            std::to_string(std::hash<std::string>{}(
                std::to_string(reinterpret_cast<uintptr_t>(this)))));
    fs::create_directories(path);
  }
  ~ScopedTempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

std::string MakeProjectYaze(const std::string& name,
                            const std::string& rom = "rom") {
  return "[project]\nname=" + name + "\n\n[files]\nrom_filename=" + rom + "\n";
}

void CreateFullBundle(const fs::path& bundle_dir,
                      const std::string& name = "TestBundle") {
  fs::create_directories(bundle_dir);
  fs::create_directories(bundle_dir / "project");
  fs::create_directories(bundle_dir / "backups");
  fs::create_directories(bundle_dir / "output");

  // project.yaze
  {
    std::ofstream f(bundle_dir / "project.yaze",
                    std::ios::out | std::ios::binary);
    f << MakeProjectYaze(name);
  }

  // ROM file (minimal)
  {
    std::ofstream f(bundle_dir / "rom", std::ios::out | std::ios::binary);
    std::vector<char> rom_data(0x8000, 0);
    f.write(rom_data.data(), static_cast<std::streamsize>(rom_data.size()));
  }
}

// The formatter at depth 0 emits fields directly (no wrapping key).
const json& GetVerify(const json& doc) {
  return doc;
}

// ============================================================================
// Argument validation
// ============================================================================

TEST(ProjectBundleVerifyTest, MissingProjectArgFails) {
  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--format=json"}, nullptr, &out);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(ProjectBundleVerifyTest, EmptyProjectArgFails) {
  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--project=", "--format=json"}, nullptr, &out);
  EXPECT_FALSE(status.ok());
}

// ============================================================================
// .yazeproj bundle verification
// ============================================================================

TEST(ProjectBundleVerifyTest, ValidBundlePasses) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "Test.yazeproj";
  CreateFullBundle(bundle);

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--project=" + bundle.string(), "--format=json"},
                            nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  const auto& verify = GetVerify(doc);
  EXPECT_TRUE(verify.value("ok", false));
  EXPECT_EQ(verify.value("status", ""), "pass");
  EXPECT_TRUE(verify.value("is_bundle", false));
  EXPECT_EQ(verify.value("fail_count", -1), 0);
}

TEST(ProjectBundleVerifyTest, BundleWithoutProjectYazeWarns) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "Bare.yazeproj";
  fs::create_directories(bundle);
  // No project.yaze — Open() auto-creates one

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--project=" + bundle.string(), "--format=json"},
                            nullptr, &out);
  // Should pass (auto-creation) or at least not fail hard
  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  const auto& verify = GetVerify(doc);
  // The bundle_project_yaze check should be "warn"
  bool found_warn = false;
  for (const auto& chk : verify.value("checks", json::array())) {
    if (chk.value("name", "") == "bundle_project_yaze" &&
        chk.value("status", "") == "warn") {
      found_warn = true;
    }
  }
  EXPECT_TRUE(found_warn) << "Expected bundle_project_yaze warn\n" << out;
}

TEST(ProjectBundleVerifyTest, NonexistentPathFails) {
  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=/tmp/does_not_exist_xyzzy.yazeproj", "--format=json"},
      nullptr, &out);
  EXPECT_FALSE(status.ok());

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded());
  const auto& verify = GetVerify(doc);
  EXPECT_FALSE(verify.value("ok", true));
  EXPECT_GT(verify.value("fail_count", 0), 0);
}

TEST(ProjectBundleVerifyTest, UnrecognizedFormatFails) {
  ScopedTempDir tmp;
  fs::path bad_file = tmp.path / "test.bin";
  {
    std::ofstream f(bad_file);
    f << "data";
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--project=" + bad_file.string(), "--format=json"},
                            nullptr, &out);
  EXPECT_FALSE(status.ok());

  auto doc = json::parse(out, nullptr, false);
  const auto& verify = GetVerify(doc);
  EXPECT_FALSE(verify.value("ok", true));
}

// ============================================================================
// .yaze file verification
// ============================================================================

TEST(ProjectBundleVerifyTest, ValidYazeFilePasses) {
  ScopedTempDir tmp;
  fs::path yaze_file = tmp.path / "test.yaze";
  {
    std::ofstream f(yaze_file, std::ios::out | std::ios::binary);
    f << MakeProjectYaze("TestProject", "myrom.sfc");
  }
  // Create the ROM file so the rom_accessible check passes
  {
    std::ofstream f(tmp.path / "myrom.sfc", std::ios::out | std::ios::binary);
    std::vector<char> rom_data(0x8000, 0);
    f.write(rom_data.data(), static_cast<std::streamsize>(rom_data.size()));
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + yaze_file.string(), "--format=json"}, nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  auto doc = json::parse(out, nullptr, false);
  const auto& verify = GetVerify(doc);
  EXPECT_TRUE(verify.value("ok", false));
  EXPECT_FALSE(verify.value("is_bundle", true));
  EXPECT_EQ(verify.value("warn_count", -1), 0) << out;

  bool found_portability_pass = false;
  for (const auto& chk : verify.value("checks", json::array())) {
    if (chk.value("name", "") == "path_portability" &&
        chk.value("status", "") == "pass") {
      found_portability_pass = true;
    }
  }
  EXPECT_TRUE(found_portability_pass) << out;
}

TEST(ProjectBundleVerifyTest, MissingRomFails) {
  ScopedTempDir tmp;
  fs::path yaze_file = tmp.path / "test.yaze";
  {
    std::ofstream f(yaze_file, std::ios::out | std::ios::binary);
    f << MakeProjectYaze("TestProject", "nonexistent.sfc");
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + yaze_file.string(), "--format=json"}, nullptr, &out);
  EXPECT_FALSE(status.ok());

  auto doc = json::parse(out, nullptr, false);
  const auto& verify = GetVerify(doc);
  EXPECT_FALSE(verify.value("ok", true));

  bool found_rom_fail = false;
  for (const auto& chk : verify.value("checks", json::array())) {
    if (chk.value("name", "") == "rom_accessible" &&
        chk.value("status", "") == "fail") {
      found_rom_fail = true;
    }
  }
  EXPECT_TRUE(found_rom_fail) << "Expected rom_accessible fail\n" << out;
}

TEST(ProjectBundleVerifyTest, ExplicitMissingHackManifestFails) {
  ScopedTempDir tmp;
  fs::path yaze_file = tmp.path / "test.yaze";
  {
    std::ofstream f(yaze_file, std::ios::out | std::ios::binary);
    f << MakeProjectYaze("TestProject", "rom.sfc")
      << "hack_manifest_file=manifests/missing.json\n";
  }
  {
    std::ofstream f(tmp.path / "rom.sfc", std::ios::out | std::ios::binary);
    std::vector<char> rom_data(0x8000, 0);
    f.write(rom_data.data(), static_cast<std::streamsize>(rom_data.size()));
  }
  // A conventional fallback must not mask a broken explicit reference.
  {
    std::ofstream f(tmp.path / "hack_manifest.json");
    f << "{}";
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + yaze_file.string(), "--format=json"}, nullptr, &out);
  EXPECT_FALSE(status.ok());

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  const auto& verify = GetVerify(doc);
  EXPECT_FALSE(verify.value("ok", true));

  bool found_manifest_fail = false;
  for (const auto& chk : verify.value("checks", json::array())) {
    if (chk.value("name", "") == "hack_manifest_ready" &&
        chk.value("status", "") == "fail") {
      found_manifest_fail = true;
    }
  }
  EXPECT_TRUE(found_manifest_fail) << "Expected hack_manifest_ready fail\n"
                                   << out;
}

TEST(ProjectBundleVerifyTest, ExplicitMalformedHackManifestFails) {
  ScopedTempDir tmp;
  fs::path yaze_file = tmp.path / "test.yaze";
  {
    std::ofstream f(yaze_file, std::ios::out | std::ios::binary);
    f << MakeProjectYaze("TestProject", "rom.sfc")
      << "hack_manifest_file=manifests/broken.json\n";
  }
  {
    std::ofstream f(tmp.path / "rom.sfc", std::ios::out | std::ios::binary);
    std::vector<char> rom_data(0x8000, 0);
    f.write(rom_data.data(), static_cast<std::streamsize>(rom_data.size()));
  }
  fs::create_directories(tmp.path / "manifests");
  {
    std::ofstream f(tmp.path / "manifests" / "broken.json");
    f << "{not-json";
  }
  // A valid conventional fallback must not mask the malformed explicit file.
  {
    std::ofstream f(tmp.path / "hack_manifest.json");
    f << "{}";
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + yaze_file.string(), "--format=json"}, nullptr, &out);
  EXPECT_FALSE(status.ok());

  auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  const auto& verify = GetVerify(doc);
  bool found_manifest_fail = false;
  for (const auto& chk : verify.value("checks", json::array())) {
    if (chk.value("name", "") == "hack_manifest_ready" &&
        chk.value("status", "") == "fail") {
      found_manifest_fail = true;
    }
  }
  EXPECT_TRUE(found_manifest_fail) << "Expected hack_manifest_ready fail\n"
                                   << out;
}

// ============================================================================
// Report file
// ============================================================================

TEST(ProjectBundleVerifyTest, ReportWritesJsonFile) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "Report.yazeproj";
  CreateFullBundle(bundle);
  fs::path report = tmp.path / "report.json";

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--project=" + bundle.string(),
                             "--report=" + report.string(), "--format=json"},
                            nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message();

  // Report file should exist and be valid JSON
  std::ifstream rf(report);
  ASSERT_TRUE(rf.is_open());
  std::string report_content((std::istreambuf_iterator<char>(rf)),
                             std::istreambuf_iterator<char>());
  auto report_doc = json::parse(report_content, nullptr, false);
  ASSERT_FALSE(report_doc.is_discarded()) << report_content;
  EXPECT_TRUE(report_doc.value("ok", false));
  EXPECT_TRUE(report_doc.contains("checks"));
}

TEST(ProjectBundleVerifyTest, AbsolutePathsWarn) {
  ScopedTempDir tmp;
  fs::path yaze_file = tmp.path / "abs.yaze";
  {
    std::ofstream f(yaze_file, std::ios::out | std::ios::binary);
    // Use an absolute ROM path to trigger portability warning
    f << "[project]\nname=AbsTest\n\n[files]\nrom_filename=/absolute/path/"
         "rom.sfc\n";
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + yaze_file.string(), "--format=json"}, nullptr, &out);
  // ROM not found → fail. But portability warning should still appear.

  auto doc = json::parse(out, nullptr, false);
  const auto& verify = GetVerify(doc);

  bool found_portability_warn = false;
  for (const auto& chk : verify.value("checks", json::array())) {
    if (chk.value("name", "") == "path_portability" &&
        chk.value("status", "") == "warn") {
      found_portability_warn = true;
    }
  }
  EXPECT_TRUE(found_portability_warn) << "Expected path_portability warn\n"
                                      << out;
}

TEST(ProjectBundleVerifyTest, AdditionalRomsNonFirstAbsolutePathWarns) {
  ScopedTempDir tmp;
  const fs::path yaze_file = tmp.path / "additional_roms.yaze";
  {
    std::ofstream f(yaze_file, std::ios::out | std::ios::binary);
    f << MakeProjectYaze("AdditionalRoms", "rom.sfc")
      << "additional_roms=relative.sfc,/absolute/second.sfc\n";
  }
  {
    std::ofstream rom(tmp.path / "rom.sfc", std::ios::out | std::ios::binary);
    const std::vector<char> data(0x8000, 0);
    rom.write(data.data(), static_cast<std::streamsize>(data.size()));
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--project=" + yaze_file.string(), "--format=json"}, nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  bool found_absolute_entry = false;
  for (const auto& chk : GetVerify(doc).value("checks", json::array())) {
    if (chk.value("name", "") == "path_portability" &&
        chk.value("status", "") == "warn") {
      found_absolute_entry =
          chk.value("detail", "").find("/absolute/second.sfc") !=
          std::string::npos;
    }
  }
  EXPECT_TRUE(found_absolute_entry) << out;
}

TEST(ProjectBundleVerifyTest, WindowsDriveAndUncPathsWarnOnAnyHost) {
  ScopedTempDir tmp;
  const fs::path yaze_file = tmp.path / "windows_paths.yaze";
  {
    std::ofstream f(yaze_file, std::ios::out | std::ios::binary);
    f << MakeProjectYaze("WindowsPaths", "rom.sfc")
      << "code_folder=C:\\Oracle\\asm\n"
      << "assets_folder=\\\\server\\share\\assets\n";
  }
  {
    std::ofstream rom(tmp.path / "rom.sfc", std::ios::out | std::ios::binary);
    const std::vector<char> data(0x8000, 0);
    rom.write(data.data(), static_cast<std::streamsize>(data.size()));
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--project=" + yaze_file.string(), "--format=json"}, nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  bool found_drive_path = false;
  bool found_unc_path = false;
  for (const auto& chk : GetVerify(doc).value("checks", json::array())) {
    if (chk.value("name", "") != "path_portability" ||
        chk.value("status", "") != "warn") {
      continue;
    }
    const std::string detail = chk.value("detail", "");
    found_drive_path = detail.find("C:\\Oracle\\asm") != std::string::npos;
    found_unc_path =
        detail.find("\\\\server\\share\\assets") != std::string::npos;
  }
  EXPECT_TRUE(found_drive_path) << out;
  EXPECT_TRUE(found_unc_path) << out;
}

// ============================================================================
// Nested path resolution (P0-01 integration)
// ============================================================================

TEST(ProjectBundleVerifyTest, NestedPathResolvesToBundleRoot) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "Nested.yazeproj";
  CreateFullBundle(bundle);

  // Point at a file INSIDE the bundle
  fs::path nested = bundle / "project.yaze";

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--project=" + nested.string(), "--format=json"},
                            nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  auto doc = json::parse(out, nullptr, false);
  const auto& verify = GetVerify(doc);
  EXPECT_TRUE(verify.value("ok", false));
  EXPECT_TRUE(verify.value("is_bundle", false));
}

// ============================================================================
// --check-rom-hash tests
// ============================================================================

TEST(ProjectBundleVerifyTest, CheckStandaloneCrc32HashUsesLoadedRomBytes) {
  ScopedTempDir tmp;
  const fs::path rom_path = tmp.path / "rom.sfc";
  const std::vector<uint8_t> data(0x8000, 0x2A);
  {
    std::ofstream rom(rom_path, std::ios::out | std::ios::binary);
    rom.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
  }
  const std::string crc32 =
      yaze::util::ComputeRomHash(data.data(), data.size());
  ASSERT_EQ(crc32.size(), 8);

  const fs::path project_path = tmp.path / "standalone.yaze";
  {
    std::ofstream project(project_path);
    project << MakeProjectYaze("Standalone", "rom.sfc")
            << "\n[rom]\nexpected_hash=" << crc32 << "\n";
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--project=" + project_path.string(),
                                   "--check-rom-hash", "--format=json"},
                                  nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  bool found_crc32_pass = false;
  for (const auto& chk : GetVerify(doc).value("checks", json::array())) {
    found_crc32_pass |=
        chk.value("name", "") == "rom_hash_check" &&
        chk.value("status", "") == "pass" &&
        chk.value("detail", "").find("CRC32 match") != std::string::npos;
  }
  EXPECT_TRUE(found_crc32_pass) << out;
}

TEST(ProjectBundleVerifyTest, CheckStandaloneSha1HashUsesLoadedRomBytes) {
  ScopedTempDir tmp;
  const fs::path rom_path = tmp.path / "rom.sfc";
  const std::vector<uint8_t> data(0x8000, 0x2A);
  {
    std::ofstream rom(rom_path, std::ios::out | std::ios::binary);
    rom.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
  }
  const std::string sha1 = yaze::util::ComputeSha1Hex(data.data(), data.size());
  ASSERT_FALSE(sha1.empty());

  const fs::path project_path = tmp.path / "standalone.yaze";
  {
    std::ofstream project(project_path);
    project << MakeProjectYaze("Standalone", "rom.sfc")
            << "\n[rom]\nexpected_hash=" << sha1 << "\n";
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--project=" + project_path.string(),
                                   "--check-rom-hash", "--format=json"},
                                  nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  bool found_pass = false;
  for (const auto& chk : GetVerify(doc).value("checks", json::array())) {
    found_pass |= chk.value("name", "") == "rom_hash_check" &&
                  chk.value("status", "") == "pass";
  }
  EXPECT_TRUE(found_pass) << out;
}

TEST(ProjectBundleVerifyTest, CheckStandaloneHashStripsSmcHeader) {
  ScopedTempDir tmp;
  const fs::path rom_path = tmp.path / "headered.sfc";
  const std::vector<uint8_t> header(0x200, 0x7E);
  const std::vector<uint8_t> data(0x100000, 0x2A);
  {
    std::ofstream rom(rom_path, std::ios::out | std::ios::binary);
    rom.write(reinterpret_cast<const char*>(header.data()),
              static_cast<std::streamsize>(header.size()));
    rom.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
  }
  const std::string crc32 =
      yaze::util::ComputeRomHash(data.data(), data.size());

  const fs::path project_path = tmp.path / "standalone.yaze";
  {
    std::ofstream project(project_path);
    project << MakeProjectYaze("Standalone", "headered.sfc")
            << "\n[rom]\nexpected_hash=" << crc32 << "\n";
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--project=" + project_path.string(),
                                   "--check-rom-hash", "--format=json"},
                                  nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  bool found_pass = false;
  for (const auto& chk : GetVerify(doc).value("checks", json::array())) {
    found_pass |= chk.value("name", "") == "rom_hash_check" &&
                  chk.value("status", "") == "pass";
  }
  EXPECT_TRUE(found_pass) << out;
}

TEST(ProjectBundleVerifyTest, CheckStandaloneHashRejectsInvalidMetadata) {
  ScopedTempDir tmp;
  const fs::path rom_path = tmp.path / "rom.sfc";
  {
    std::ofstream rom(rom_path, std::ios::out | std::ios::binary);
    const std::vector<char> data(0x8000, 0x2A);
    rom.write(data.data(), static_cast<std::streamsize>(data.size()));
  }
  const fs::path project_path = tmp.path / "standalone.yaze";
  {
    std::ofstream project(project_path);
    project << MakeProjectYaze("Standalone", "rom.sfc")
            << "\n[rom]\nexpected_hash=not-a-hash\n";
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--project=" + project_path.string(),
                                   "--check-rom-hash", "--format=json"},
                                  nullptr, &out);
  EXPECT_FALSE(status.ok());

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  bool found_metadata_fail = false;
  for (const auto& chk : GetVerify(doc).value("checks", json::array())) {
    found_metadata_fail |=
        chk.value("name", "") == "rom_hash_check" &&
        chk.value("status", "") == "fail" &&
        chk.value("detail", "").find("expected_hash") != std::string::npos;
  }
  EXPECT_TRUE(found_metadata_fail) << out;
}

TEST(ProjectBundleVerifyTest, CheckStandaloneRomHashMismatchFails) {
  ScopedTempDir tmp;
  const fs::path rom_path = tmp.path / "rom.sfc";
  {
    std::ofstream rom(rom_path, std::ios::out | std::ios::binary);
    const std::vector<char> data(0x8000, 0x2A);
    rom.write(data.data(), static_cast<std::streamsize>(data.size()));
  }
  const fs::path project_path = tmp.path / "standalone.yaze";
  {
    std::ofstream project(project_path);
    project << MakeProjectYaze("Standalone", "rom.sfc")
            << "\n[rom]\nexpected_hash="
            << "0000000000000000000000000000000000000000\n";
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  const auto status = handler.Run({"--project=" + project_path.string(),
                                   "--check-rom-hash", "--format=json"},
                                  nullptr, &out);
  EXPECT_FALSE(status.ok());

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  bool found_fail = false;
  for (const auto& chk : GetVerify(doc).value("checks", json::array())) {
    found_fail |= chk.value("name", "") == "rom_hash_check" &&
                  chk.value("status", "") == "fail";
  }
  EXPECT_TRUE(found_fail) << out;
}

TEST(ProjectBundleVerifyTest, CheckRomHashSkippedByDefault) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "HashSkip.yazeproj";
  CreateFullBundle(bundle);

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run({"--project=" + bundle.string(), "--format=json"},
                            nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message();

  auto doc = json::parse(out, nullptr, false);
  const auto& verify = GetVerify(doc);
  // No rom_hash_check should appear when flag not set
  bool found_hash_check = false;
  for (const auto& chk : verify.value("checks", json::array())) {
    if (chk.value("name", "") == "rom_hash_check") {
      found_hash_check = true;
    }
  }
  EXPECT_FALSE(found_hash_check) << "hash check should not run without flag";
}

TEST(ProjectBundleVerifyTest, CheckRomHashNoManifestWarns) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "NoManifest.yazeproj";
  CreateFullBundle(bundle);
  // Bundle has no manifest.json by default

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + bundle.string(), "--check-rom-hash", "--format=json"},
      nullptr, &out);
  // Should still pass overall (warn, not fail)
  EXPECT_TRUE(status.ok()) << status.message();

  auto doc = json::parse(out, nullptr, false);
  const auto& verify = GetVerify(doc);
  bool found_warn = false;
  for (const auto& chk : verify.value("checks", json::array())) {
    if (chk.value("name", "") == "rom_hash_check" &&
        chk.value("status", "") == "warn") {
      found_warn = true;
    }
  }
  EXPECT_TRUE(found_warn) << "Expected rom_hash_check warn\n" << out;
}

TEST(ProjectBundleVerifyTest, CheckRomHashMatchPasses) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "HashMatch.yazeproj";
  CreateFullBundle(bundle);

  // Compute actual SHA1 of the rom file
  fs::path rom_path = bundle / "rom";
  std::string actual_sha1 = yaze::util::ComputeFileSha1Hex(rom_path.string());
  ASSERT_FALSE(actual_sha1.empty());

  // Write manifest.json with correct hash
  {
    nlohmann::json manifest;
    manifest["rom_sha1"] = actual_sha1;
    std::ofstream mf(bundle / "manifest.json",
                     std::ios::out | std::ios::binary);
    mf << manifest.dump(2);
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + bundle.string(), "--check-rom-hash", "--format=json"},
      nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  auto doc = json::parse(out, nullptr, false);
  const auto& verify = GetVerify(doc);
  bool found_pass = false;
  for (const auto& chk : verify.value("checks", json::array())) {
    if (chk.value("name", "") == "rom_hash_check" &&
        chk.value("status", "") == "pass") {
      found_pass = true;
    }
  }
  EXPECT_TRUE(found_pass) << "Expected rom_hash_check pass\n" << out;
}

TEST(ProjectBundleVerifyTest, CheckBundleHashIncludesSmcHeader) {
  ScopedTempDir tmp;
  const fs::path bundle = tmp.path / "HeaderedHash.yazeproj";
  CreateFullBundle(bundle);

  const fs::path rom_path = bundle / "rom";
  const std::vector<uint8_t> header(0x200, 0x7E);
  const std::vector<uint8_t> data(0x100000, 0x2A);
  {
    std::ofstream rom(rom_path,
                      std::ios::out | std::ios::binary | std::ios::trunc);
    rom.write(reinterpret_cast<const char*>(header.data()),
              static_cast<std::streamsize>(header.size()));
    rom.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
  }

  const std::string raw_file_sha1 =
      yaze::util::ComputeFileSha1Hex(rom_path.string());
  const std::string loaded_bytes_sha1 =
      yaze::util::ComputeSha1Hex(data.data(), data.size());
  ASSERT_FALSE(raw_file_sha1.empty());
  ASSERT_NE(raw_file_sha1, loaded_bytes_sha1);
  {
    nlohmann::json manifest;
    manifest["rom_sha1"] = raw_file_sha1;
    std::ofstream mf(bundle / "manifest.json",
                     std::ios::out | std::ios::binary);
    mf << manifest.dump(2);
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  const auto status = handler.Run(
      {"--project=" + bundle.string(), "--check-rom-hash", "--format=json"},
      nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  const auto doc = json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  bool found_pass = false;
  for (const auto& chk : GetVerify(doc).value("checks", json::array())) {
    found_pass |=
        chk.value("name", "") == "rom_hash_check" &&
        chk.value("status", "") == "pass" &&
        chk.value("detail", "").find("SHA1 match") != std::string::npos;
  }
  EXPECT_TRUE(found_pass) << out;
}

TEST(ProjectBundleVerifyTest, CheckRomHashMatchNormalizesCaseAndWhitespace) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "HashNormalize.yazeproj";
  CreateFullBundle(bundle);

  fs::path rom_path = bundle / "rom";
  std::string actual_sha1 = yaze::util::ComputeFileSha1Hex(rom_path.string());
  ASSERT_FALSE(actual_sha1.empty());

  // Manifest hash intentionally uppercased and padded with whitespace.
  std::string normalized = actual_sha1;
  for (char& ch : normalized) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  {
    nlohmann::json manifest;
    manifest["rom_sha1"] =
        std::string("  \n\t") + normalized + std::string("  ");
    std::ofstream mf(bundle / "manifest.json",
                     std::ios::out | std::ios::binary);
    mf << manifest.dump(2);
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + bundle.string(), "--check-rom-hash", "--format=json"},
      nullptr, &out);
  EXPECT_TRUE(status.ok()) << status.message() << "\n" << out;

  auto doc = json::parse(out, nullptr, false);
  const auto& verify = GetVerify(doc);
  bool found_pass = false;
  for (const auto& chk : verify.value("checks", json::array())) {
    if (chk.value("name", "") == "rom_hash_check" &&
        chk.value("status", "") == "pass") {
      found_pass = true;
    }
  }
  EXPECT_TRUE(found_pass) << "Expected normalized rom_hash_check pass\n" << out;
}

TEST(ProjectBundleVerifyTest, CheckRomHashMismatchFails) {
  ScopedTempDir tmp;
  fs::path bundle = tmp.path / "HashBad.yazeproj";
  CreateFullBundle(bundle);

  // Write manifest with wrong hash
  {
    nlohmann::json manifest;
    manifest["rom_sha1"] = "0000000000000000000000000000000000000000";
    std::ofstream mf(bundle / "manifest.json",
                     std::ios::out | std::ios::binary);
    mf << manifest.dump(2);
  }

  handlers::ProjectBundleVerifyCommandHandler handler;
  std::string out;
  auto status = handler.Run(
      {"--project=" + bundle.string(), "--check-rom-hash", "--format=json"},
      nullptr, &out);
  EXPECT_FALSE(status.ok());

  auto doc = json::parse(out, nullptr, false);
  const auto& verify = GetVerify(doc);
  bool found_fail = false;
  for (const auto& chk : verify.value("checks", json::array())) {
    if (chk.value("name", "") == "rom_hash_check" &&
        chk.value("status", "") == "fail") {
      found_fail = true;
    }
  }
  EXPECT_TRUE(found_fail) << "Expected rom_hash_check fail\n" << out;
}

}  // namespace
}  // namespace yaze::cli
