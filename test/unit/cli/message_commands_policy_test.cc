#include "cli/handlers/game/message_commands.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "app/editor/message/message_data.h"
#include "rom/rom.h"
#include "zelda3/resource_labels.h"

namespace yaze::cli {
namespace {

namespace fs = std::filesystem;
using ::testing::HasSubstr;

class ScopedTempDir {
 public:
  ScopedTempDir() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = fs::temp_directory_path() /
            ("yaze_message_cli_policy_" + std::to_string(nonce));
    fs::create_directories(path_);
  }

  ~ScopedTempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

void WriteBinaryFile(const fs::path& path, const std::vector<uint8_t>& data) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output.is_open());
  output.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
  ASSERT_TRUE(output.good());
}

void WriteTextFile(const fs::path& path, const std::string& text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output.is_open());
  output << text;
  ASSERT_TRUE(output.good());
}

std::vector<uint8_t> ReadBinaryFile(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  EXPECT_TRUE(input.is_open());
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

std::vector<uint8_t> MakeMessageRomData(int expanded_message_count = 1) {
  std::vector<uint8_t> data(0x180000, 0xA5);
  data[editor::kTextData] = editor::FindMatchingCharacter('A');
  data[editor::kTextData + 1] = editor::kMessageTerminator;
  data[editor::kTextData + 2] = 0xFF;
  int expanded_pos = editor::kExpandedTextDataDefault;
  for (int i = 0; i < expanded_message_count; ++i) {
    data[expanded_pos++] = editor::FindMatchingCharacter('X');
    data[expanded_pos++] = editor::kMessageTerminator;
  }
  data[expanded_pos] = 0xFF;
  return data;
}

std::string MakeManifest(const std::string& hack_name, bool editor_managed,
                         bool include_expanded_range = true,
                         int expanded_count = 2,
                         const std::string& last_expanded_id = "0x18E") {
  nlohmann::json messages = {{"data_start", "0x2F8000"},
                             {"data_end", "0x2F80FF"},
                             {"vanilla_count", 1}};
  if (include_expanded_range) {
    messages["expanded_range"] = {{"first", "0x18D"},
                                  {"last", last_expanded_id},
                                  {"count", expanded_count}};
  }
  nlohmann::json manifest = {{"manifest_version", 3},
                             {"hack_name", hack_name},
                             {"messages", std::move(messages)},
                             {"owned_banks",
                              {{"banks",
                                {{{"bank", "0x2F"},
                                  {"bank_start", "0x2F8000"},
                                  {"bank_end", "0x2FFFFF"},
                                  {"ownership", "asm_owned"},
                                  {"ownership_note", "Core/message.asm"}}}}}}};
  if (editor_managed) {
    manifest["editor_managed_regions"] = {
        {"regions", {{{"start", "0x2F8000"}, {"end", "0x2F8100"}}}}};
  }
  return manifest.dump(2);
}

fs::path CreateProject(const fs::path& root, const fs::path& rom_path,
                       const std::string& hack_name,
                       const std::string& write_policy, bool editor_managed,
                       bool include_expanded_range = true) {
  const fs::path manifest_path = root / "hack_manifest.json";
  const fs::path project_path = root / "test.yaze";
  WriteTextFile(manifest_path, MakeManifest(hack_name, editor_managed,
                                            include_expanded_range));
  WriteTextFile(
      project_path,
      "[project]\nname=MessagePolicyTest\n\n[files]\nrom_filename=" +
          rom_path.filename().string() +
          "\nhack_manifest_file=hack_manifest.json\n\n[rom]\nwrite_policy=" +
          write_policy + "\n");
  return project_path;
}

fs::path WriteBundle(const fs::path& root, const nlohmann::json& messages) {
  const fs::path path = root / "messages.json";
  WriteTextFile(path, nlohmann::json{{"version", editor::kMessageBundleVersion},
                                     {"messages", messages}}
                          .dump(2));
  return path;
}

Rom LoadRom(const fs::path& path) {
  Rom rom;
  auto status = rom.LoadFromFile(path.string());
  EXPECT_TRUE(status.ok()) << status;
  return rom;
}

TEST(MessageCommandsPolicyTest, MessageWriteWithoutProjectFailsClosed) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData());
  Rom rom = LoadRom(rom_path);
  const auto before = rom.vector();

  handlers::MessageWriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--id=0", "--text=B", "--format=json"}, &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << output;
  EXPECT_THAT(std::string(status.message()), HasSubstr("explicit --project"));
  EXPECT_EQ(rom.vector(), before);
}

TEST(MessageCommandsPolicyTest, ExpandedBundleApplyWithoutProjectFailsClosed) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData());
  const auto before = ReadBinaryFile(rom_path);
  const fs::path bundle_path = WriteBundle(
      temp.path(), nlohmann::json::array(
                       {{{"id", 0}, {"bank", "expanded"}, {"text", "B"}}}));

  handlers::MessageImportBundleCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--file=" + bundle_path.string(), "--apply",
                   "--rom=" + rom_path.string(), "--format=json"},
                  nullptr, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << output;
  EXPECT_THAT(std::string(status.message()), HasSubstr("explicit --project"));
  EXPECT_EQ(ReadBinaryFile(rom_path), before);
}

TEST(MessageCommandsPolicyTest,
     OracleBlockPolicyRejectsEditorManagedExpandedRegion) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData());
  const fs::path project_path =
      CreateProject(temp.path(), rom_path, "Oracle of Secrets", "block", true);
  Rom rom = LoadRom(rom_path);
  const auto before = rom.vector();

  handlers::MessageWriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--id=0", "--text=B", "--project=" + project_path.string(),
                   "--format=json"},
                  &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << output;
  EXPECT_THAT(std::string(status.message()), HasSubstr("Core/message.asm"));
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("rebuild Oracle of Secrets"));
  EXPECT_EQ(rom.vector(), before);
}

TEST(MessageCommandsPolicyTest, UnrelatedProjectRomIsRejected) {
  ScopedTempDir temp;
  const fs::path active_rom_path = temp.path() / "active.sfc";
  const fs::path project_rom_path = temp.path() / "project.sfc";
  WriteBinaryFile(active_rom_path, MakeMessageRomData());
  WriteBinaryFile(project_rom_path, MakeMessageRomData());
  const fs::path project_path = CreateProject(temp.path(), project_rom_path,
                                              "Other Hack", "allow", false);
  Rom rom = LoadRom(active_rom_path);
  const auto before = rom.vector();

  handlers::MessageWriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--id=0", "--text=B", "--project=" + project_path.string(),
                   "--format=json"},
                  &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << output;
  EXPECT_THAT(std::string(status.message()), HasSubstr("Project ROM mismatch"));
  EXPECT_EQ(rom.vector(), before);
}

TEST(MessageCommandsPolicyTest, WramMessageRegionIsRejected) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData());
  const fs::path project_path =
      CreateProject(temp.path(), rom_path, "Other Hack", "allow", false);
  auto manifest = nlohmann::json::parse(MakeManifest("Other Hack", false));
  manifest["messages"]["data_start"] = "0x7E8000";
  manifest["messages"]["data_end"] = "0x7E80FF";
  WriteTextFile(temp.path() / "hack_manifest.json", manifest.dump(2));
  Rom rom = LoadRom(rom_path);
  const auto before = rom.vector();

  handlers::MessageWriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--id=0", "--text=B", "--project=" + project_path.string(),
                   "--format=json"},
                  &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << output;
  EXPECT_THAT(std::string(status.message()), HasSubstr("valid LoROM"));
  EXPECT_EQ(rom.vector(), before);
}

TEST(MessageCommandsPolicyTest, MixedApplyLateFailureLeavesRomUnchanged) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData());
  const fs::path project_path =
      CreateProject(temp.path(), rom_path, "Other Hack", "allow", false);
  const fs::path bundle_path = WriteBundle(
      temp.path(),
      nlohmann::json::array(
          {{{"id", 0}, {"bank", "vanilla"}, {"text", "B"}},
           {{"id", 0}, {"bank", "expanded"}, {"text", "A[BANK]B"}}}));
  const auto before = ReadBinaryFile(rom_path);

  handlers::MessageImportBundleCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--file=" + bundle_path.string(), "--apply",
                   "--rom=" + rom_path.string(),
                   "--project=" + project_path.string(), "--format=json"},
                  nullptr, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << output;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("only valid in the vanilla message stream"));
  EXPECT_EQ(ReadBinaryFile(rom_path), before);
}

TEST(MessageCommandsPolicyTest, PermittedNonOracleProjectCanWriteExpanded) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData());
  const fs::path project_path =
      CreateProject(temp.path(), rom_path, "Other Hack", "block", true);
  Rom rom = LoadRom(rom_path);

  handlers::MessageWriteCommandHandler handler;
  core::HackManifest prior_manifest;
  ASSERT_TRUE(prior_manifest
                  .LoadFromString(
                      R"json({"manifest_version":3,"hack_name":"Prior"})json")
                  .ok());
  auto& resource_labels = zelda3::GetResourceLabels();
  resource_labels.SetHackManifest(&prior_manifest);
  std::string output;
  const auto status =
      handler.Run({"--id=0", "--text=B", "--project=" + project_path.string(),
                   "--format=json"},
                  &rom, &output);

  EXPECT_EQ(resource_labels.hack_manifest(), &prior_manifest);
  resource_labels.SetHackManifest(nullptr);
  ASSERT_TRUE(status.ok()) << status << "\n" << output;
  EXPECT_EQ(rom.vector()[editor::kExpandedTextDataDefault],
            editor::FindMatchingCharacter('B'));
}

TEST(MessageCommandsPolicyTest, WarnPolicyReportsExpandedRegionConflict) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData());
  const fs::path project_path =
      CreateProject(temp.path(), rom_path, "Other Hack", "warn", false);
  Rom rom = LoadRom(rom_path);

  handlers::MessageWriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--id=0", "--text=B", "--project=" + project_path.string(),
                   "--format=json"},
                  &rom, &output);

  ASSERT_TRUE(status.ok()) << status << "\n" << output;
  EXPECT_THAT(output, HasSubstr("write_policy_warning"));
  EXPECT_THAT(output, HasSubstr("conflicts with hack manifest"));
  EXPECT_EQ(rom.vector()[editor::kExpandedTextDataDefault],
            editor::FindMatchingCharacter('B'));
}

TEST(MessageCommandsPolicyTest,
     MissingManifestIdRangeStillRejectsOversizedIdBeforeAllocation) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData());
  const fs::path project_path =
      CreateProject(temp.path(), rom_path, "Other Hack", "allow", false,
                    /*include_expanded_range=*/false);
  Rom rom = LoadRom(rom_path);
  const auto before = rom.vector();

  handlers::MessageWriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--id=1000000000", "--text=B",
                   "--project=" + project_path.string(), "--format=json"},
                  &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange) << output;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("outside the manifest range"));
  EXPECT_EQ(rom.vector(), before);
}

TEST(MessageCommandsPolicyTest,
     ExistingExpandedBankBeyondManifestLimitFailsClosed) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData(/*expanded_message_count=*/3));
  const fs::path project_path =
      CreateProject(temp.path(), rom_path, "Other Hack", "allow", false);
  Rom rom = LoadRom(rom_path);
  const auto before = rom.vector();

  handlers::MessageWriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--id=0", "--text=B", "--project=" + project_path.string(),
                   "--format=json"},
                  &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << output;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("exceeding the manifest limit"));
  EXPECT_EQ(rom.vector(), before);
}

TEST(MessageCommandsPolicyTest, CountAndIdRangeBothConstrainMutation) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData());
  const fs::path project_path =
      CreateProject(temp.path(), rom_path, "Other Hack", "allow", false);
  WriteTextFile(temp.path() / "hack_manifest.json",
                MakeManifest("Other Hack", false,
                             /*include_expanded_range=*/true,
                             /*expanded_count=*/3,
                             /*last_expanded_id=*/"0x18E"));
  Rom rom = LoadRom(rom_path);
  const auto before = rom.vector();

  handlers::MessageWriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--id=2", "--text=B", "--project=" + project_path.string(),
                   "--format=json"},
                  &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange) << output;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("outside the manifest range"));
  EXPECT_EQ(rom.vector(), before);
}

TEST(MessageCommandsPolicyTest,
     MalformedProjectReturnsStatusAndRestoresPriorManifestBinding) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData());
  const fs::path project_path =
      CreateProject(temp.path(), rom_path, "Other Hack", "allow", false);
  auto malformed_manifest =
      nlohmann::json::parse(MakeManifest("Other Hack", false));
  malformed_manifest["messages"]["expanded_range"]["count"] = "two";
  WriteTextFile(temp.path() / "hack_manifest.json", malformed_manifest.dump(2));
  Rom rom = LoadRom(rom_path);
  const auto before = rom.vector();

  core::HackManifest prior_manifest;
  ASSERT_TRUE(prior_manifest
                  .LoadFromString(
                      R"json({"manifest_version":3,"hack_name":"Prior"})json")
                  .ok());
  auto& resource_labels = zelda3::GetResourceLabels();
  resource_labels.SetHackManifest(&prior_manifest);

  handlers::MessageWriteCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--id=0", "--text=B", "--project=" + project_path.string(),
                   "--format=json"},
                  &rom, &output);

  EXPECT_EQ(resource_labels.hack_manifest(), &prior_manifest);
  resource_labels.SetHackManifest(nullptr);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << output;
  EXPECT_THAT(std::string(status.message()), HasSubstr("Cannot load project"));
  EXPECT_EQ(rom.vector(), before);
}

TEST(MessageCommandsPolicyTest, VanillaOnlyApplyStillWorksWithoutProject) {
  ScopedTempDir temp;
  const fs::path rom_path = temp.path() / "active.sfc";
  WriteBinaryFile(rom_path, MakeMessageRomData());
  const fs::path bundle_path = WriteBundle(
      temp.path(),
      nlohmann::json::array({{{"id", 0}, {"bank", "vanilla"}, {"text", "B"}}}));

  handlers::MessageImportBundleCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--file=" + bundle_path.string(), "--apply", "--range=vanilla",
       "--rom=" + rom_path.string(), "--format=json"},
      nullptr, &output);

  ASSERT_TRUE(status.ok()) << status << "\n" << output;
  const auto after = ReadBinaryFile(rom_path);
  EXPECT_EQ(after[editor::kTextData], editor::FindMatchingCharacter('B'));
}

}  // namespace
}  // namespace yaze::cli
