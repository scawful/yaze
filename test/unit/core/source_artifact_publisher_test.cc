#include "core/source_artifact_publisher.h"

#include <barrier>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace yaze::core {
namespace {

namespace fs = std::filesystem;

class ScopedTempDir {
 public:
  ScopedTempDir() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = fs::temp_directory_path() /
            ("yaze_source_artifact_publisher_" + std::to_string(nonce));
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
  fs::create_directories(path.parent_path());
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

TEST(SourceArtifactPublisherTest, Sha256MatchesPublishedKnownVectors) {
  EXPECT_EQ(ComputeSourceArtifactSha256(""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(ComputeSourceArtifactSha256("abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(SourceArtifactPublisherTest, RejectsRelativeTargetPaths) {
  const absl::Status status =
      ValidateSourceArtifactPublicationTargets({"relative/source.json"});

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
}

TEST(SourceArtifactPublisherTest, RejectsHardLinkedTargetAliases) {
  ScopedTempDir temp;
  const fs::path primary = temp.path() / "source.json";
  const fs::path alias = temp.path() / "source-alias.json";
  WriteText(primary, "canonical\n");
  std::error_code link_ec;
  fs::create_hard_link(primary, alias, link_ec);
  if (link_ec) {
    GTEST_SKIP() << "Hard links are unavailable: " << link_ec.message();
  }

  const absl::Status status =
      ValidateSourceArtifactPublicationTargets({primary, alias});

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_NE(std::string(status.message()).find("alias each other"),
            std::string::npos);
}

TEST(SourceArtifactPublisherTest,
     ConcurrentSameCasWritersPublishExactlyOneCompleteSet) {
  ScopedTempDir temp;
  const fs::path primary = temp.path() / "source.json";
  const fs::path generated = temp.path() / "generated" / "source.asm";
  WriteText(primary, "initial\n");
  WriteText(generated, "generated-initial\n");
  const std::vector<fs::path> targets = {primary, generated};
  const std::string expected_sha =
      ComputeSourceArtifactSha256(ReadText(primary));

  std::barrier rendezvous(3);
  absl::Status status_a = absl::UnknownError("writer A did not run");
  absl::Status status_b = absl::UnknownError("writer B did not run");
  auto writer = [&](char suffix, absl::Status* status) {
    rendezvous.arrive_and_wait();
    auto lock_or = AcquireSourceArtifactPublicationLock(targets);
    if (!lock_or.ok()) {
      *status = lock_or.status();
      return;
    }
    const std::string primary_before = ReadText(primary);
    const std::string generated_before = ReadText(generated);
    *status = PublishSourceArtifacts(
        **lock_or,
        {{.target = primary,
          .before = primary_before,
          .after = std::string("primary-") + suffix + "\n"},
         {.target = generated,
          .before = generated_before,
          .after = std::string("generated-") + suffix + "\n"}},
        expected_sha);
  };

  std::thread writer_a(writer, 'A', &status_a);
  std::thread writer_b(writer, 'B', &status_b);
  rendezvous.arrive_and_wait();
  writer_a.join();
  writer_b.join();

  const int successes =
      static_cast<int>(status_a.ok()) + static_cast<int>(status_b.ok());
  ASSERT_EQ(successes, 1) << status_a << "; " << status_b;
  const absl::Status& loser = status_a.ok() ? status_b : status_a;
  EXPECT_EQ(loser.code(), absl::StatusCode::kAborted);

  const std::string primary_after = ReadText(primary);
  const std::string generated_after = ReadText(generated);
  EXPECT_TRUE(
      (primary_after == "primary-A\n" && generated_after == "generated-A\n") ||
      (primary_after == "primary-B\n" && generated_after == "generated-B\n"));
  ExpectNoPublicationArtifacts(temp.path());
}

TEST(SourceArtifactPublisherTest,
     ExternalCommentByteChangeAbortsWithoutOverwritingIt) {
  ScopedTempDir temp;
  const fs::path primary = temp.path() / "source.json";
  const fs::path generated = temp.path() / "source.asm";
  WriteText(primary, "canonical\n");
  WriteText(generated, "generated\n");
  const std::vector<fs::path> targets = {primary, generated};
  const std::string primary_before = ReadText(primary);
  const std::string generated_before = ReadText(generated);
  const std::string expected_sha = ComputeSourceArtifactSha256(primary_before);

  // Simulate an external editor changing only a source comment after the
  // caller captured the CAS hash but before the write lock is acquired.
  const std::string externally_changed = primary_before + "# comment\n";
  WriteText(primary, externally_changed);

  auto lock_or = AcquireSourceArtifactPublicationLock(targets);
  ASSERT_TRUE(lock_or.ok()) << lock_or.status();
  const absl::Status status = PublishSourceArtifacts(
      **lock_or,
      {{.target = primary, .before = externally_changed, .after = "updated\n"},
       {.target = generated,
        .before = generated_before,
        .after = "generated-updated\n"}},
      expected_sha);

  EXPECT_EQ(status.code(), absl::StatusCode::kAborted) << status;
  EXPECT_NE(std::string(status.message()).find("SHA-256 preflight"),
            std::string::npos);
  EXPECT_EQ(ReadText(primary), externally_changed);
  EXPECT_EQ(ReadText(generated), generated_before);
  ExpectNoPublicationArtifacts(temp.path());
}

TEST(SourceArtifactPublisherTest, ValidatorFailureRollsBackEveryArtifact) {
  ScopedTempDir temp;
  const fs::path primary = temp.path() / "source.json";
  const fs::path generated = temp.path() / "source.asm";
  WriteText(primary, "canonical\n");
  WriteText(generated, "generated\n");
  const std::vector<fs::path> targets = {primary, generated};
  const std::string primary_before = ReadText(primary);
  const std::string generated_before = ReadText(generated);

  auto lock_or = AcquireSourceArtifactPublicationLock(targets);
  ASSERT_TRUE(lock_or.ok()) << lock_or.status();
  const absl::Status status = PublishSourceArtifacts(
      **lock_or,
      {{.target = primary, .before = primary_before, .after = "updated\n"},
       {.target = generated,
        .before = generated_before,
        .after = "generated-updated\n"}},
      ComputeSourceArtifactSha256(primary_before),
      [] { return absl::FailedPreconditionError("domain readback rejected"); });

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_EQ(ReadText(primary), primary_before);
  EXPECT_EQ(ReadText(generated), generated_before);
  ExpectNoPublicationArtifacts(temp.path());
}

TEST(SourceArtifactPublisherTest,
     ThrowingValidatorIsConvertedToStatusAndRollsBackEveryArtifact) {
  ScopedTempDir temp;
  const fs::path primary = temp.path() / "source.json";
  const fs::path generated = temp.path() / "source.asm";
  WriteText(primary, "canonical\n");
  WriteText(generated, "generated\n");
  const std::vector<fs::path> targets = {primary, generated};
  const std::string primary_before = ReadText(primary);
  const std::string generated_before = ReadText(generated);

  auto lock_or = AcquireSourceArtifactPublicationLock(targets);
  ASSERT_TRUE(lock_or.ok()) << lock_or.status();
  const absl::Status status = PublishSourceArtifacts(
      **lock_or,
      {{.target = primary, .before = primary_before, .after = "updated\n"},
       {.target = generated,
        .before = generated_before,
        .after = "generated-updated\n"}},
      ComputeSourceArtifactSha256(primary_before),
      []() -> absl::Status { throw std::runtime_error("validator failure"); });

  EXPECT_EQ(status.code(), absl::StatusCode::kInternal) << status;
  EXPECT_NE(std::string(status.message()).find("validator failure"),
            std::string::npos);
  EXPECT_EQ(ReadText(primary), primary_before);
  EXPECT_EQ(ReadText(generated), generated_before);
  ExpectNoPublicationArtifacts(temp.path());
}

}  // namespace
}  // namespace yaze::core
