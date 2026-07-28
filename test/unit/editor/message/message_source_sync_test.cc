#include "app/editor/message/message_source_sync.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "cli/handlers/game/message_commands.h"
#include "zelda3/dungeon/oracle_rom_safety_preflight.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {
namespace {

namespace fs = std::filesystem;
using ::testing::HasSubstr;
using Json = nlohmann::json;

class ScopedTempDir {
 public:
  ScopedTempDir() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = fs::temp_directory_path() /
            ("yaze_message_source_sync_" + std::to_string(nonce));
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

void WriteText(const fs::path& path, const std::string& content) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output.is_open()) << path;
  output << content;
  ASSERT_TRUE(output.good()) << path;
}

std::string ReadText(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  EXPECT_TRUE(input.is_open()) << path;
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

std::string Bundle(const std::vector<std::pair<int, std::string>>& messages,
                   int count = -1) {
  if (count < 0) {
    count = static_cast<int>(messages.size());
  }
  Json document;
  document["format"] = "yaze-message-bundle";
  document["version"] = 1;
  document["counts"] = {{"vanilla", 0}, {"expanded", count}};
  document["messages"] = Json::array();
  for (const auto& [id, text] : messages) {
    document["messages"].push_back(
        {{"id", id}, {"bank", "expanded"}, {"text", text}});
  }
  return document.dump(2) + "\n";
}

std::string Subset(int id, const std::string& text,
                   absl::string_view text_field = "text") {
  Json document;
  document["format"] = "yaze-message-bundle";
  document["version"] = 1;
  document["counts"] = {{"vanilla", 0}, {"expanded", 1}};
  Json entry;
  entry["id"] = id;
  entry["bank"] = "expanded";
  entry[std::string(text_field)] = text;
  document["messages"] = Json::array({std::move(entry)});
  return document.dump(2) + "\n";
}

std::string Manifest(
    absl::string_view include_path = "Core/generated/messages.asm",
    absl::string_view data_end = "0x2F80FF",
    absl::string_view range_last = "0x18F", int count = 3) {
  Json manifest = {
      {"manifest_version", 3},
      {"hack_name", "Message Source Test"},
      {"messages",
       {{"data_start", "0x2F8026"},
        {"data_end", data_end},
        {"expanded_range",
         {{"first", "0x18D"}, {"last", range_last}, {"count", count}}},
        {"source",
         {{"format", "yaze-message-bundle"},
          {"version", 1},
          {"canonical_bundle_path", "Data/Messages/expanded.json"},
          {"generated_asm_include_path", include_path}}}}}};
  return manifest.dump(2);
}

project::YazeProject MakeProject(const fs::path& root,
                                 const std::string& manifest = Manifest()) {
  fs::create_directories(root / "Data/Messages");
  fs::create_directories(root / "Core/generated");
  project::YazeProject project;
  project.filepath = (root / "project.yaze").string();
  EXPECT_TRUE(project.hack_manifest.LoadFromString(manifest).ok());
  return project;
}

struct SourceFixture {
  explicit SourceFixture(const fs::path& root)
      : project(MakeProject(root)),
        canonical(root / "Data/Messages/expanded.json"),
        include(root / "Core/generated/messages.asm"),
        incoming(root / "incoming.json") {
    WriteText(canonical, Bundle({{0, "A"}, {1, "B"}, {2, "C"}}));
  }

  project::YazeProject project;
  fs::path canonical;
  fs::path include;
  fs::path incoming;
};

TEST(MessageSourceSyncTest, DryRunDoesNotPublishEitherArtifact) {
  ScopedTempDir temp;
  SourceFixture fixture(temp.path());
  WriteText(fixture.incoming, Subset(1, "X"));
  const std::string before = ReadText(fixture.canonical);

  auto result_or = SyncMessageSource(fixture.project, fixture.incoming);

  ASSERT_TRUE(result_or.ok()) << result_or.status();
  EXPECT_TRUE(result_or->changed);
  EXPECT_FALSE(result_or->wrote);
  EXPECT_EQ(result_or->incoming_updates, 1);
  EXPECT_EQ(result_or->expanded_count, 3);
  EXPECT_EQ(ReadText(fixture.canonical), before);
  EXPECT_FALSE(fs::exists(fixture.include));
}

TEST(MessageSourceSyncTest, Sha256MatchesPublishedKnownVectors) {
  EXPECT_EQ(ComputeMessageSourceSha256(""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(ComputeMessageSourceSha256("abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(MessageSourceSyncTest,
     WritePublishesDeterministicBundleAndHashBoundInclude) {
  ScopedTempDir temp;
  SourceFixture fixture(temp.path());
  Json incoming = Json::parse(Subset(1, "[D:$a]X"));
  incoming["messages"][0]["raw"] = "stale";
  WriteText(fixture.incoming, incoming.dump(2) + "\n");
  auto preview_or = SyncMessageSource(fixture.project, fixture.incoming);
  ASSERT_TRUE(preview_or.ok()) << preview_or.status();

  auto write_or = SyncMessageSource(
      fixture.project, fixture.incoming,
      {.write = true,
       .expected_source_sha256 = preview_or->source_sha256_before});

  ASSERT_TRUE(write_or.ok()) << write_or.status();
  EXPECT_TRUE(write_or->wrote);
  ASSERT_EQ(write_or->backup_paths.size(), 1u);
  EXPECT_TRUE(fs::exists(write_or->backup_paths.front()));
  EXPECT_EQ(ReadText(write_or->backup_paths.front()),
            Bundle({{0, "A"}, {1, "B"}, {2, "C"}}));

  const std::string canonical = ReadText(fixture.canonical);
  EXPECT_EQ(canonical, Bundle({{0, "A"}, {1, "[D:0A]X"}, {2, "C"}}));
  EXPECT_EQ(canonical.back(), '\n');

  const std::string include = ReadText(fixture.include);
  EXPECT_TRUE(absl::StartsWith(
      include,
      "; Source bundle SHA-256: " + write_or->proposed_source_sha256 + "\n"));
  const size_t body_start = include.find("Message_18D:");
  ASSERT_NE(body_start, std::string::npos);
  const std::string body = include.substr(body_start);
  const fs::path body_path = temp.path() / "body.asm";
  WriteText(body_path, body);
  auto body_hash_or = zelda3::ComputeSha256(body_path.string());
  ASSERT_TRUE(body_hash_or.ok()) << body_hash_or.status();
  EXPECT_THAT(
      include,
      HasSubstr("; Generated ASM body SHA-256: " + *body_hash_or + "\n"));
  EXPECT_THAT(include, HasSubstr("Message_18D:\n  db $00, $7F\n\n"));
  EXPECT_THAT(include, HasSubstr("Message_18E:\n  db $92, $17, $7F\n\n"));
  EXPECT_THAT(include, HasSubstr("Message_18F:\n  db $02, $7F\n\n"));
  EXPECT_TRUE(absl::EndsWith(include, "db $FF\n"));
  EXPECT_EQ(include.find("\norg "), std::string::npos);
}

TEST(MessageSourceSyncTest,
     WritePreservesEmptyMessageAndAcceptedCommandArgumentSpellings) {
  ScopedTempDir temp;
  SourceFixture fixture(temp.path());
  WriteText(fixture.incoming, Bundle({{0, ""}, {1, "[W:7][W:7F][W:FF]"}}, 2));
  auto preview_or = SyncMessageSource(fixture.project, fixture.incoming);
  ASSERT_TRUE(preview_or.ok()) << preview_or.status();

  auto write_or = SyncMessageSource(
      fixture.project, fixture.incoming,
      {.write = true,
       .expected_source_sha256 = preview_or->source_sha256_before});

  ASSERT_TRUE(write_or.ok()) << write_or.status();
  EXPECT_EQ(ReadText(fixture.canonical),
            Bundle({{0, ""}, {1, "[W:7][W:7F][W:FF]"}, {2, "C"}}));
  const std::string include = ReadText(fixture.include);
  EXPECT_THAT(include, HasSubstr("Message_18D:\n  db $7F\n\n"));
  EXPECT_THAT(include, HasSubstr("Message_18E:\n"
                                 "  db $6B, $07, $6B, $7F, $6B, $FF, $7F\n\n"));
}

TEST(MessageSourceSyncTest, RejectsMissingAndLowercaseCommandArguments) {
  for (const std::string& invalid_text : {"[W]", "[W:ff]"}) {
    SCOPED_TRACE(invalid_text);
    ScopedTempDir temp;
    SourceFixture fixture(temp.path());
    WriteText(fixture.incoming, Subset(1, invalid_text));
    const std::string before = ReadText(fixture.canonical);

    auto result_or = SyncMessageSource(fixture.project, fixture.incoming);

    ASSERT_FALSE(result_or.ok());
    EXPECT_EQ(result_or.status().code(), absl::StatusCode::kInvalidArgument);
    EXPECT_EQ(ReadText(fixture.canonical), before);
    EXPECT_FALSE(fs::exists(fixture.include));
  }
}

TEST(MessageSourceSyncTest, WriteRequiresMatchingSourceCas) {
  ScopedTempDir temp;
  SourceFixture fixture(temp.path());
  WriteText(fixture.incoming, Subset(0, "X"));
  const std::string before = ReadText(fixture.canonical);

  auto result_or = SyncMessageSource(
      fixture.project, fixture.incoming,
      {.write = true,
       .expected_source_sha256 =
           "0000000000000000000000000000000000000000000000000000000000000000"});

  ASSERT_FALSE(result_or.ok());
  EXPECT_EQ(result_or.status().code(), absl::StatusCode::kAborted);
  EXPECT_THAT(std::string(result_or.status().message()), HasSubstr("CAS"));
  EXPECT_EQ(ReadText(fixture.canonical), before);
  EXPECT_FALSE(fs::exists(fixture.include));
}

TEST(MessageSourceSyncTest, DriftedGeneratedIncludeFailsClosed) {
  ScopedTempDir temp;
  SourceFixture fixture(temp.path());
  WriteText(fixture.incoming, Subset(1, "X"));
  auto first_preview = SyncMessageSource(fixture.project, fixture.incoming);
  ASSERT_TRUE(first_preview.ok()) << first_preview.status();
  auto first_write = SyncMessageSource(
      fixture.project, fixture.incoming,
      {.write = true,
       .expected_source_sha256 = first_preview->source_sha256_before});
  ASSERT_TRUE(first_write.ok()) << first_write.status();

  WriteText(fixture.include, ReadText(fixture.include) + "; manual edit\n");
  const std::string canonical_before = ReadText(fixture.canonical);
  const std::string include_before = ReadText(fixture.include);
  WriteText(fixture.incoming, Subset(2, "Y"));
  auto next_preview = SyncMessageSource(fixture.project, fixture.incoming);
  ASSERT_FALSE(next_preview.ok());
  EXPECT_EQ(next_preview.status().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(std::string(next_preview.status().message()),
              HasSubstr("drifted"));

  auto result_or = SyncMessageSource(
      fixture.project, fixture.incoming,
      {.write = true,
       .expected_source_sha256 = first_write->proposed_source_sha256});

  ASSERT_FALSE(result_or.ok());
  EXPECT_EQ(result_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(std::string(result_or.status().message()), HasSubstr("drifted"));
  EXPECT_EQ(ReadText(fixture.canonical), canonical_before);
  EXPECT_EQ(ReadText(fixture.include), include_before);
}

#if !defined(_WIN32)
TEST(MessageSourceSyncTest, PreparationFailureCleansTempsAndBackups) {
  ScopedTempDir temp;
  SourceFixture fixture(temp.path());
  WriteText(fixture.incoming, Subset(1, "X"));
  auto preview = SyncMessageSource(fixture.project, fixture.incoming);
  ASSERT_TRUE(preview.ok()) << preview.status();

  const fs::path include_dir = fixture.include.parent_path();
  fs::permissions(include_dir,
                  fs::perms::owner_read | fs::perms::owner_exec |
                      fs::perms::group_read | fs::perms::group_exec |
                      fs::perms::others_read | fs::perms::others_exec);
  auto result_or = SyncMessageSource(
      fixture.project, fixture.incoming,
      {.write = true, .expected_source_sha256 = preview->source_sha256_before});
  fs::permissions(include_dir, fs::perms::owner_all);

  ASSERT_FALSE(result_or.ok());
  EXPECT_EQ(ReadText(fixture.canonical),
            Bundle({{0, "A"}, {1, "B"}, {2, "C"}}));
  EXPECT_FALSE(fs::exists(fixture.include));
  for (const auto& entry : fs::recursive_directory_iterator(temp.path())) {
    EXPECT_EQ(entry.path().filename().string().find(".yaze-tmp-"),
              std::string::npos)
        << entry.path();
    EXPECT_EQ(entry.path().filename().string().find(".yaze-backup-"),
              std::string::npos)
        << entry.path();
  }
}
#endif

TEST(MessageSourceSyncTest, RejectsBankSwitchAndIncompleteCanonicalBank) {
  ScopedTempDir temp;
  SourceFixture fixture(temp.path());
  WriteText(fixture.incoming, Subset(1, "A[BANK]B"));
  auto bank_result = SyncMessageSource(fixture.project, fixture.incoming);
  ASSERT_FALSE(bank_result.ok());
  EXPECT_EQ(bank_result.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(bank_result.status().message()), HasSubstr("[BANK]"));

  WriteText(fixture.canonical, Bundle({{0, "A"}, {2, "C"}}, 3));
  WriteText(fixture.incoming, Subset(1, "B"));
  auto incomplete_result = SyncMessageSource(fixture.project, fixture.incoming);
  ASSERT_FALSE(incomplete_result.ok());
  EXPECT_EQ(incomplete_result.status().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(std::string(incomplete_result.status().message()),
              HasSubstr("complete expanded bank"));
}

TEST(MessageSourceSyncTest, RejectsLiteralNewlinesThatEncoderWouldIgnore) {
  ScopedTempDir temp;
  SourceFixture fixture(temp.path());
  WriteText(fixture.incoming, Subset(1, "A\nB"));
  const std::string before = ReadText(fixture.canonical);

  auto result_or = SyncMessageSource(fixture.project, fixture.incoming);

  ASSERT_FALSE(result_or.ok());
  EXPECT_EQ(result_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(result_or.status().message()),
              HasSubstr("Literal newlines are ignored"));
  EXPECT_EQ(ReadText(fixture.canonical), before);
  EXPECT_FALSE(fs::exists(fixture.include));
}

TEST(MessageSourceSyncTest, RejectsManifestCapacityOverflow) {
  ScopedTempDir temp;
  auto project =
      MakeProject(temp.path(), Manifest("Core/generated/messages.asm",
                                        /*data_end=*/"0x2F802A"));
  const fs::path canonical = temp.path() / "Data/Messages/expanded.json";
  const fs::path incoming = temp.path() / "incoming.json";
  WriteText(canonical, Bundle({{0, "A"}, {1, "B"}, {2, "C"}}));
  WriteText(incoming, Subset(1, "B"));

  auto result_or = SyncMessageSource(project, incoming);

  ASSERT_FALSE(result_or.ok());
  EXPECT_EQ(result_or.status().code(), absl::StatusCode::kResourceExhausted);
  EXPECT_THAT(std::string(result_or.status().message()),
              HasSubstr("manifest capacity"));
}

#if !defined(_WIN32)
TEST(MessageSourceSyncTest, RejectsSymlinkParentEscape) {
  ScopedTempDir temp;
  const fs::path outside = temp.path() / "outside";
  const fs::path project_root = temp.path() / "project";
  fs::create_directories(outside);
  auto project = MakeProject(project_root, Manifest("escape/generated.asm"));
  std::error_code link_ec;
  fs::create_directory_symlink(outside, project_root / "escape", link_ec);
  if (link_ec) {
    GTEST_SKIP() << "Cannot create directory symlink: " << link_ec.message();
  }
  const fs::path canonical = project_root / "Data/Messages/expanded.json";
  const fs::path incoming = project_root / "incoming.json";
  WriteText(canonical, Bundle({{0, "A"}, {1, "B"}, {2, "C"}}));
  WriteText(incoming, Subset(1, "X"));

  auto result_or = SyncMessageSource(project, incoming);

  ASSERT_FALSE(result_or.ok());
  EXPECT_EQ(result_or.status().code(), absl::StatusCode::kPermissionDenied);
  EXPECT_THAT(std::string(result_or.status().message()),
              HasSubstr("escapes project root"));
  EXPECT_FALSE(fs::exists(outside / "generated.asm"));
}
#endif

TEST(MessageSourceSyncTest, CliHandlerIsRomIndependentAndReportsDryRun) {
  ScopedTempDir temp;
  SourceFixture fixture(temp.path());
  const fs::path manifest_path = temp.path() / "hack_manifest.json";
  const fs::path project_path = temp.path() / "project.yaze";
  WriteText(manifest_path, Manifest());
  WriteText(project_path,
            "[project]\nname=MessageSourceCliTest\n\n"
            "[files]\nhack_manifest_file=hack_manifest.json\n");
  WriteText(fixture.incoming, Subset(1, "X"));

  cli::handlers::MessageSourceSyncCommandHandler handler;
  EXPECT_FALSE(handler.RequiresRom());
  std::string output;
  auto status =
      handler.Run({"--project=" + project_path.string(),
                   "--file=" + fixture.incoming.string(), "--format=json"},
                  nullptr, &output);

  ASSERT_TRUE(status.ok()) << status << "\n" << output;
  EXPECT_THAT(output, HasSubstr("\"mode\": \"dry-run\""));
  EXPECT_THAT(output, HasSubstr("\"wrote\": false"));
  EXPECT_FALSE(fs::exists(fixture.include));
  zelda3::GetResourceLabels().SetHackManifest(nullptr);
}

}  // namespace
}  // namespace yaze::editor
