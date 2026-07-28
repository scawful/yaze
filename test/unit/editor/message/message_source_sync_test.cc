#include "app/editor/message/message_source_sync.h"

#include <barrier>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "cli/handlers/game/message_commands.h"
#include "zelda3/dungeon/oracle_rom_safety_preflight.h"
#include "zelda3/resource_labels.h"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <sys/wait.h>
#include <unistd.h>
#endif

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

void ExpectNoPublicationArtifacts(const fs::path& root) {
  for (const auto& entry : fs::recursive_directory_iterator(root)) {
    const std::string filename = entry.path().filename().string();
    EXPECT_EQ(filename.find(".yaze-tmp-"), std::string::npos) << entry.path();
    EXPECT_EQ(filename.find(".yaze-backup-"), std::string::npos)
        << entry.path();
  }
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
    absl::string_view range_last = "0x18F", int count = 3,
    absl::string_view canonical_path = "Data/Messages/expanded.json") {
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
          {"canonical_bundle_path", canonical_path},
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

TEST(MessageSourceSyncTest, RejectsModuloNarrowingBundleVersionAndId) {
  {
    ScopedTempDir temp;
    SourceFixture fixture(temp.path());
    Json incoming = Json::parse(Subset(1, "X"));
    incoming["version"] = uint64_t{4294967297};
    WriteText(fixture.incoming, incoming.dump(2) + "\n");

    auto result_or = SyncMessageSource(fixture.project, fixture.incoming);

    ASSERT_FALSE(result_or.ok());
    EXPECT_EQ(result_or.status().code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(std::string(result_or.status().message()),
                HasSubstr("version must be integer 1"));
  }
  {
    ScopedTempDir temp;
    SourceFixture fixture(temp.path());
    Json incoming = Json::parse(Subset(1, "X"));
    incoming["messages"][0]["id"] = uint64_t{4294967297};
    WriteText(fixture.incoming, incoming.dump(2) + "\n");

    auto result_or = SyncMessageSource(fixture.project, fixture.incoming);

    ASSERT_FALSE(result_or.ok());
    EXPECT_EQ(result_or.status().code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(std::string(result_or.status().message()),
                HasSubstr("ID must be an integer"));
  }
}

TEST(MessageSourceSyncTest, RejectsModuloNarrowingBundleCounts) {
  const std::pair<const char*, uint64_t> cases[] = {
      {"expanded", uint64_t{4294967299}},
      {"vanilla", uint64_t{4294967296}},
  };
  for (const auto& [field, value] : cases) {
    SCOPED_TRACE(field);
    ScopedTempDir temp;
    SourceFixture fixture(temp.path());
    Json canonical = Json::parse(ReadText(fixture.canonical));
    canonical["counts"][field] = value;
    WriteText(fixture.canonical, canonical.dump(2) + "\n");
    WriteText(fixture.incoming, Subset(1, "X"));

    auto result_or = SyncMessageSource(fixture.project, fixture.incoming);

    ASSERT_FALSE(result_or.ok());
    EXPECT_EQ(result_or.status().code(), absl::StatusCode::kInvalidArgument);
    EXPECT_THAT(std::string(result_or.status().message()),
                HasSubstr("bounded non-negative integers"));
  }
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
  ExpectNoPublicationArtifacts(temp.path());

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
     ConcurrentSameCasWritersPublishExactlyOneCompletePair) {
  ScopedTempDir temp;
  SourceFixture fixture(temp.path());
  const fs::path incoming_x = temp.path() / "incoming-x.json";
  const fs::path incoming_y = temp.path() / "incoming-y.json";
  WriteText(incoming_x, Subset(1, "X"));
  WriteText(incoming_y, Subset(1, "Y"));
  const std::string expected_sha =
      ComputeMessageSourceSha256(ReadText(fixture.canonical));

  std::barrier rendezvous(3);
  absl::StatusOr<MessageSourceSyncResult> result_x =
      absl::UnknownError("writer X did not run");
  absl::StatusOr<MessageSourceSyncResult> result_y =
      absl::UnknownError("writer Y did not run");
  std::thread writer_x([&] {
    rendezvous.arrive_and_wait();
    result_x = SyncMessageSource(
        fixture.project, incoming_x,
        {.write = true, .expected_source_sha256 = expected_sha});
  });
  std::thread writer_y([&] {
    rendezvous.arrive_and_wait();
    result_y = SyncMessageSource(
        fixture.project, incoming_y,
        {.write = true, .expected_source_sha256 = expected_sha});
  });
  rendezvous.arrive_and_wait();
  writer_x.join();
  writer_y.join();

  const int successes =
      static_cast<int>(result_x.ok()) + static_cast<int>(result_y.ok());
  ASSERT_EQ(successes, 1);
  const auto& loser = result_x.ok() ? result_y : result_x;
  EXPECT_EQ(loser.status().code(), absl::StatusCode::kAborted);

  const std::string canonical = ReadText(fixture.canonical);
  const std::string expected_x = Bundle({{0, "A"}, {1, "X"}, {2, "C"}});
  const std::string expected_y = Bundle({{0, "A"}, {1, "Y"}, {2, "C"}});
  EXPECT_TRUE(canonical == expected_x || canonical == expected_y);
  const std::string include = ReadText(fixture.include);
  EXPECT_TRUE(absl::StartsWith(
      include, "; Source bundle SHA-256: " +
                   ComputeMessageSourceSha256(canonical) + "\n"));
  EXPECT_THAT(include,
              HasSubstr(canonical == expected_x ? "$17, $7F" : "$18, $7F"));
  EXPECT_TRUE(fs::exists(fixture.canonical.parent_path() /
                         ".yaze-message-source-sync.lock"));
  EXPECT_TRUE(fs::exists(fixture.include.parent_path() /
                         ".yaze-message-source-sync.lock"));
  EXPECT_FALSE(fs::exists(temp.path() / ".yaze-message-source-sync.lock"));
  ExpectNoPublicationArtifacts(temp.path());
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
TEST(MessageSourceSyncTest,
     NestedProjectsSharingTargetsSerializeAcrossProcesses) {
  constexpr int kMessageCount = 4096;
  ScopedTempDir temp;
  const fs::path shared_root = temp.path() / "shared";
  const std::string range_last =
      absl::StrFormat("0x%X", 0x18D + kMessageCount - 1);
  auto outer_project =
      MakeProject(temp.path(), Manifest("shared/Core/generated/messages.asm",
                                        "0x2FFFFF", range_last, kMessageCount,
                                        "shared/Data/Messages/expanded.json"));
  auto nested_project =
      MakeProject(shared_root, Manifest("Core/generated/messages.asm",
                                        "0x2FFFFF", range_last, kMessageCount));

  std::vector<std::pair<int, std::string>> messages;
  messages.reserve(kMessageCount);
  for (int id = 0; id < kMessageCount; ++id) {
    messages.emplace_back(id, "");
  }
  const fs::path canonical = shared_root / "Data/Messages/expanded.json";
  const fs::path include = shared_root / "Core/generated/messages.asm";
  const fs::path incoming_outer = temp.path() / "incoming-outer.json";
  const fs::path incoming_nested = temp.path() / "incoming-nested.json";
  WriteText(canonical, Bundle(messages));
  WriteText(incoming_outer, Subset(0, "X"));
  WriteText(incoming_nested, Subset(1, "Y"));
  const std::string expected_sha =
      ComputeMessageSourceSha256(ReadText(canonical));

  int ready_pipe[2] = {-1, -1};
  int start_pipe[2] = {-1, -1};
  ASSERT_EQ(pipe(ready_pipe), 0);
  ASSERT_EQ(pipe(start_pipe), 0);

  auto launch_writer = [&](const project::YazeProject& project,
                           const fs::path& incoming) {
    const pid_t pid = fork();
    if (pid != 0) {
      return pid;
    }
    close(ready_pipe[0]);
    close(start_pipe[1]);
    const char ready = 'r';
    if (write(ready_pipe[1], &ready, 1) != 1) {
      _exit(20);
    }
    char start = '\0';
    if (read(start_pipe[0], &start, 1) != 1) {
      _exit(21);
    }
    auto result_or = SyncMessageSource(
        project, incoming,
        {.write = true, .expected_source_sha256 = expected_sha});
    if (result_or.ok()) {
      _exit(0);
    }
    _exit(result_or.status().code() == absl::StatusCode::kAborted ? 10 : 22);
  };

  const pid_t outer_pid = launch_writer(outer_project, incoming_outer);
  ASSERT_GT(outer_pid, 0);
  const pid_t nested_pid = launch_writer(nested_project, incoming_nested);
  ASSERT_GT(nested_pid, 0);
  close(ready_pipe[1]);
  close(start_pipe[0]);
  char ready[2] = {};
  size_t ready_total = 0;
  while (ready_total < sizeof(ready)) {
    const ssize_t count =
        read(ready_pipe[0], ready + ready_total, sizeof(ready) - ready_total);
    ASSERT_GT(count, 0);
    ready_total += static_cast<size_t>(count);
  }
  ASSERT_EQ(write(start_pipe[1], "gg", 2), 2);
  close(ready_pipe[0]);
  close(start_pipe[1]);

  int outer_status = 0;
  int nested_status = 0;
  ASSERT_EQ(waitpid(outer_pid, &outer_status, 0), outer_pid);
  ASSERT_EQ(waitpid(nested_pid, &nested_status, 0), nested_pid);
  ASSERT_TRUE(WIFEXITED(outer_status));
  ASSERT_TRUE(WIFEXITED(nested_status));
  const int outer_exit = WEXITSTATUS(outer_status);
  const int nested_exit = WEXITSTATUS(nested_status);
  EXPECT_EQ((outer_exit == 0) + (nested_exit == 0), 1)
      << outer_exit << ", " << nested_exit;
  EXPECT_EQ((outer_exit == 10) + (nested_exit == 10), 1)
      << outer_exit << ", " << nested_exit;

  EXPECT_TRUE(
      fs::exists(canonical.parent_path() / ".yaze-message-source-sync.lock"));
  EXPECT_TRUE(
      fs::exists(include.parent_path() / ".yaze-message-source-sync.lock"));
  EXPECT_FALSE(fs::exists(temp.path() / ".yaze-message-source-sync.lock"));
  EXPECT_FALSE(fs::exists(shared_root / ".yaze-message-source-sync.lock"));
  const std::string final_canonical = ReadText(canonical);
  const std::string final_include = ReadText(include);
  EXPECT_TRUE(absl::StartsWith(
      final_include, "; Source bundle SHA-256: " +
                         ComputeMessageSourceSha256(final_canonical) + "\n"));
  ExpectNoPublicationArtifacts(temp.path());
}
#endif

#if !defined(_WIN32)
TEST(MessageSourceSyncTest, WritePreservesExistingPosixModes) {
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

  constexpr fs::perms kCanonicalMode =
      fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read;
  constexpr fs::perms kIncludeMode =
      fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read |
      fs::perms::group_write | fs::perms::others_read;
  fs::permissions(fixture.canonical, kCanonicalMode, fs::perm_options::replace);
  fs::permissions(fixture.include, kIncludeMode, fs::perm_options::replace);

  WriteText(fixture.incoming, Subset(2, "Y"));
  auto next_preview = SyncMessageSource(fixture.project, fixture.incoming);
  ASSERT_TRUE(next_preview.ok()) << next_preview.status();
  auto next_write = SyncMessageSource(
      fixture.project, fixture.incoming,
      {.write = true,
       .expected_source_sha256 = next_preview->source_sha256_before});
  ASSERT_TRUE(next_write.ok()) << next_write.status();

  constexpr fs::perms kPermissionMask =
      fs::perms::owner_all | fs::perms::group_all | fs::perms::others_all;
  EXPECT_EQ(fs::status(fixture.canonical).permissions() & kPermissionMask,
            kCanonicalMode);
  EXPECT_EQ(fs::status(fixture.include).permissions() & kPermissionMask,
            kIncludeMode);
  ExpectNoPublicationArtifacts(temp.path());
}
#endif

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
  ExpectNoPublicationArtifacts(temp.path());
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

TEST(MessageSourceSyncTest, RejectsCaseVariantOfPersistentLockAsTarget) {
  ScopedTempDir temp;
  auto project = MakeProject(
      temp.path(), Manifest("Core/generated/.YAZE-MESSAGE-SOURCE-SYNC.LOCK"));
  const fs::path canonical = temp.path() / "Data/Messages/expanded.json";
  const fs::path incoming = temp.path() / "incoming.json";
  WriteText(canonical, Bundle({{0, "A"}, {1, "B"}, {2, "C"}}));
  WriteText(incoming, Subset(1, "X"));

  auto result_or = SyncMessageSource(project, incoming);

  ASSERT_FALSE(result_or.ok());
  EXPECT_EQ(result_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(result_or.status().message()),
              HasSubstr("persistent lock path"));
  EXPECT_FALSE(fs::exists(temp.path() / "Core/generated" /
                          ".yaze-message-source-sync.lock"));
}

TEST(MessageSourceSyncTest, RejectsExistingHardLinkAliasOfPersistentLock) {
  ScopedTempDir temp;
  SourceFixture fixture(temp.path());
  const fs::path lock =
      fixture.include.parent_path() / ".yaze-message-source-sync.lock";
  WriteText(lock, "");
  std::error_code link_ec;
  fs::create_hard_link(lock, fixture.include, link_ec);
  if (link_ec) {
    GTEST_SKIP() << "Cannot create hard link: " << link_ec.message();
  }
  WriteText(fixture.incoming, Subset(1, "X"));

  auto result_or = SyncMessageSource(fixture.project, fixture.incoming);

  ASSERT_FALSE(result_or.ok());
  EXPECT_EQ(result_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(result_or.status().message()),
              HasSubstr("aliases a persistent lock"));
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

TEST(MessageSourceSyncTest,
     CliHandlerRejectsMalformedManifestAndRestoresPriorBinding) {
  ScopedTempDir temp;
  const fs::path manifest_path = temp.path() / "hack_manifest.json";
  const fs::path project_path = temp.path() / "project.yaze";
  const fs::path incoming_path = temp.path() / "incoming.json";
  Json malformed_manifest = Json::parse(Manifest());
  malformed_manifest["messages"]["expanded_range"]["count"] = "three";
  WriteText(manifest_path, malformed_manifest.dump(2));
  WriteText(project_path,
            "[project]\nname=MalformedMessageSourceCliTest\n\n"
            "[files]\nhack_manifest_file=hack_manifest.json\n");
  WriteText(incoming_path, Subset(1, "X"));

  core::HackManifest prior_manifest;
  ASSERT_TRUE(prior_manifest
                  .LoadFromString(
                      R"json({"manifest_version":3,"hack_name":"Prior"})json")
                  .ok());
  auto& resource_labels = zelda3::GetResourceLabels();
  resource_labels.SetHackManifest(&prior_manifest);

  cli::handlers::MessageSourceSyncCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--project=" + project_path.string(),
                   "--file=" + incoming_path.string(), "--format=json"},
                  nullptr, &output);

  EXPECT_EQ(resource_labels.hack_manifest(), &prior_manifest);
  resource_labels.SetHackManifest(nullptr);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << output;
  EXPECT_THAT(std::string(status.message()), HasSubstr("Cannot load project"));
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("messages.expanded_range.count"));
  ExpectNoPublicationArtifacts(temp.path());
}

TEST(MessageSourceSyncTest,
     CliHandlerRejectsMissingManifestAndRestoresPriorBinding) {
  ScopedTempDir temp;
  const fs::path project_path = temp.path() / "project.yaze";
  const fs::path incoming_path = temp.path() / "incoming.json";
  WriteText(project_path,
            "[project]\nname=MissingMessageSourceCliTest\n\n"
            "[files]\nhack_manifest_file=missing_manifest.json\n");
  WriteText(incoming_path, Subset(1, "X"));

  core::HackManifest prior_manifest;
  ASSERT_TRUE(prior_manifest
                  .LoadFromString(
                      R"json({"manifest_version":3,"hack_name":"Prior"})json")
                  .ok());
  auto& resource_labels = zelda3::GetResourceLabels();
  resource_labels.SetHackManifest(&prior_manifest);

  cli::handlers::MessageSourceSyncCommandHandler handler;
  std::string output;
  const absl::Status status =
      handler.Run({"--project=" + project_path.string(),
                   "--file=" + incoming_path.string(), "--format=json"},
                  nullptr, &output);

  EXPECT_EQ(resource_labels.hack_manifest(), &prior_manifest);
  resource_labels.SetHackManifest(nullptr);
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound) << output;
  EXPECT_THAT(std::string(status.message()), HasSubstr("Cannot load project"));
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("Could not open manifest"));
  ExpectNoPublicationArtifacts(temp.path());
}

}  // namespace
}  // namespace yaze::editor
