#include "core/native_save_receipt.h"

#include "gtest/gtest.h"

#if defined(YAZE_WITH_JSON)

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"

namespace yaze::core {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

std::string JsonPathText(const fs::path& path) {
  const auto bytes = path.generic_u8string();
  return std::string(bytes.begin(), bytes.end());
}

class ScopedTempDir {
 public:
  ScopedTempDir() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = fs::temp_directory_path() /
            ("yaze_native_save_receipt_" + std::to_string(nonce));
    fs::create_directories(path_);
    path_ = fs::canonical(path_);
  }

  ~ScopedTempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

void WriteBytes(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output.is_open()) << path;
  output.write(content.data(), static_cast<std::streamsize>(content.size()));
  ASSERT_TRUE(output.good()) << path;
}

std::string ReadBytes(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  EXPECT_TRUE(input.is_open()) << path;
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

class NativeSaveReceiptTest : public ::testing::Test {
 protected:
  void SetUp() override {
    receipt_path_ = temp_.path() / "receipt.json";
    request_.receipt_path = receipt_path_;
    request_.operation_id = "native-save-1";
    request_.operation = "save";
    AddInput("rom", "saved.sfc", std::string("rom\0saved\n", 10));
    AddInput("producer", "producer.json", "{\"saved\":true}\n");
    AddInput("dependency.layout", "layout.bin", "layout\n");
  }

  void AddInput(const std::string& role, const fs::path& filename,
                const std::string& bytes) {
    const fs::path path = temp_.path() / filename;
    WriteBytes(path, bytes);
    inputs_[path] = bytes;
    request_.artifacts.push_back(
        {role, path, ComputeSourceArtifactSha256(bytes)});
  }

  void SeedReceipt() {
    const auto result = PublishNativeSaveReceipt(request_);
    ASSERT_TRUE(result.ok()) << result.status();
    ASSERT_EQ(result->generation, 1u);
    ASSERT_EQ(ReadBytes(receipt_path_), result->json);
  }

  void ExpectInputsUnchanged() const {
    for (const auto& [path, bytes] : inputs_) {
      EXPECT_EQ(ReadBytes(path), bytes) << path;
    }
  }

  void ExpectNoTemporaryArtifacts() const {
    for (const auto& entry : fs::recursive_directory_iterator(temp_.path())) {
      const auto filename = entry.path().filename().string();
      EXPECT_EQ(filename.find(".yaze-tmp-"), std::string::npos) << entry.path();
      EXPECT_EQ(filename.find(".yaze-backup-"), std::string::npos)
          << entry.path();
    }
  }

  void ExpectRefusedWithoutMutation(const NativeSaveReceiptRequest& request) {
    const std::optional<std::string> before =
        fs::exists(receipt_path_)
            ? std::optional<std::string>(ReadBytes(receipt_path_))
            : std::nullopt;
    const auto result = PublishNativeSaveReceipt(request);
    EXPECT_FALSE(result.ok());
    if (before.has_value()) {
      EXPECT_EQ(ReadBytes(receipt_path_), *before);
    } else {
      EXPECT_FALSE(fs::exists(receipt_path_));
    }
    ExpectInputsUnchanged();
    ExpectNoTemporaryArtifacts();
  }

  ScopedTempDir temp_;
  fs::path receipt_path_;
  NativeSaveReceiptRequest request_;
  std::map<fs::path, std::string> inputs_;
};

#if defined(__EMSCRIPTEN__)

TEST_F(NativeSaveReceiptTest, UnavailableInBrowserBuilds) {
  const auto result = PublishNativeSaveReceipt(request_);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_FALSE(fs::exists(receipt_path_));
  ExpectInputsUnchanged();
}

#else

std::optional<std::string> GetAppDataOverride() {
  const char* value = std::getenv("YAZE_APP_DATA_DIR");
  return value == nullptr ? std::nullopt : std::optional<std::string>(value);
}

class ScopedAppDataOverride {
 public:
  explicit ScopedAppDataOverride(const fs::path& path)
      : original_(GetAppDataOverride()), result_(Set(path.string())) {}

  ~ScopedAppDataOverride() {
    if (original_.has_value()) {
      EXPECT_EQ(Set(*original_), 0);
    } else {
#if defined(_WIN32)
      EXPECT_EQ(_putenv_s("YAZE_APP_DATA_DIR", ""), 0);
#else
      EXPECT_EQ(unsetenv("YAZE_APP_DATA_DIR"), 0);
#endif
    }
    EXPECT_EQ(GetAppDataOverride(), original_);
  }

  ScopedAppDataOverride(const ScopedAppDataOverride&) = delete;
  ScopedAppDataOverride& operator=(const ScopedAppDataOverride&) = delete;
  bool ok() const { return result_ == 0; }

 private:
  static int Set(const std::string& value) {
#if defined(_WIN32)
    return _putenv_s("YAZE_APP_DATA_DIR", value.c_str());
#else
    return setenv("YAZE_APP_DATA_DIR", value.c_str(), 1);
#endif
  }
  const std::optional<std::string> original_;
  const int result_;
};

void ExpectCommittedContext(const NativeSaveContext& context,
                            const NativeSaveReceiptRequest& request,
                            uint64_t generation) {
  const auto document = json::parse(ReadBytes(context.path));
  EXPECT_EQ(document.size(), 7u);
  EXPECT_EQ(document.at("kind"), "yaze.native_save_context.v1");
  EXPECT_EQ(document.at("editing_source_path"), context.editing_source_path);
  EXPECT_EQ(document.at("state"), "committed");
  EXPECT_EQ(document.at("project_path"), context.project_path);
  const auto& commit = document.at("native_commit");
  EXPECT_EQ(commit.size(), 3u);
  EXPECT_EQ(commit.at("receipt_path"),
            JsonPathText(fs::canonical(request.receipt_path)));
  EXPECT_EQ(commit.at("generation").get<uint64_t>(), generation);
  EXPECT_EQ(commit.at("operation_id"), request.operation_id);
  const auto rom =
      std::find_if(request.artifacts.begin(), request.artifacts.end(),
                   [](const NativeSaveArtifact& artifact) {
                     return artifact.role == "rom";
                   });
  ASSERT_NE(rom, request.artifacts.end());
  EXPECT_EQ(document.at("rom_path"), JsonPathText(fs::canonical(rom->path)));
  const auto producer =
      std::find_if(request.artifacts.begin(), request.artifacts.end(),
                   [](const NativeSaveArtifact& artifact) {
                     return artifact.role == "producer";
                   });
  ASSERT_NE(producer, request.artifacts.end());
  const json expected_producer = {
      {"role", "producer"},
      {"path", JsonPathText(fs::canonical(producer->path))},
      {"sha256", producer->expected_sha256},
      {"bytes", fs::file_size(producer->path)}};
  EXPECT_EQ(document.at("producer"), expected_producer);
}

TEST_F(NativeSaveReceiptTest, NativeSeparatorsReplayGenericReceiptAndContext) {
  ScopedAppDataOverride app_data(temp_.path() / "private context");
  ASSERT_TRUE(app_data.ok());
  const auto nested = temp_.path() / fs::u8path(u8"native-\u00E9 paths");
  fs::create_directory(nested);
  AddInput("dependency.unicode", nested / fs::u8path(u8"layout-\u00E9.bin"),
           "unicode dependency bytes");
  receipt_path_ = nested / "receipt.json";
  request_.receipt_path = receipt_path_;
  request_.receipt_path.make_preferred();
  for (auto& artifact : request_.artifacts)
    artifact.path.make_preferred();
  const auto context =
      GetNativeSaveContext(request_.artifacts[0].path.string(), "");
  ASSERT_TRUE(context.ok()) << context.status();
  request_.context = *context;

  const auto first = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(first.ok()) << first.status();
  const auto document = json::parse(first->json);
  EXPECT_EQ(document.at("receipt_path"),
            JsonPathText(fs::canonical(receipt_path_)));
  for (const auto& artifact : document.at("artifacts")) {
    const auto text = artifact.at("path").get<std::string>();
    EXPECT_EQ(text.find('\\'), std::string::npos);
    const auto decoded = fs::u8path(text);
    EXPECT_TRUE(decoded.is_absolute());
    EXPECT_EQ(decoded, fs::canonical(decoded));
  }
  ExpectCommittedContext(*context, request_, 1);

  // A persisted v1 receipt remains readable and idempotent when callers supply
  // native-format paths. A new operation keeps the same artifact identities.
  const auto replay = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(replay.ok()) << replay.status();
  EXPECT_EQ(replay->generation, 1u);
  EXPECT_EQ(replay->json, first->json);
  request_.operation_id = "native-path-save-2";
  const auto second = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(second.ok()) << second.status();
  EXPECT_EQ(second->generation, 2u);
  EXPECT_EQ(json::parse(second->json).at("artifacts"),
            document.at("artifacts"));
  ExpectCommittedContext(*context, request_, 2);
  ExpectInputsUnchanged();
  ExpectNoTemporaryArtifacts();
}

TEST_F(NativeSaveReceiptTest, PublishesCanonicalSortedReceiptAtGenerationOne) {
  fs::create_directory(temp_.path() / "nested");
  request_.receipt_path = temp_.path() / "nested" / ".." / "receipt.json";
  request_.artifacts[1].path = temp_.path() / "." / "producer.json";
  const auto result = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->generation, 1u);
  EXPECT_EQ(ReadBytes(receipt_path_), result->json);
  const auto document = json::parse(result->json);
  EXPECT_EQ(document.size(), 8u);
  EXPECT_EQ(document.at("kind"), "yaze.native_save.v1");
  EXPECT_EQ(document.at("generation").get<uint64_t>(), 1u);
  EXPECT_EQ(document.at("operation_id"), request_.operation_id);
  EXPECT_EQ(document.at("operation"), "save");
  EXPECT_EQ(document.at("bindings_complete"), true);
  EXPECT_EQ(document.at("binding_errors"), json::array());
  EXPECT_EQ(document.at("receipt_path"),
            JsonPathText(fs::canonical(receipt_path_)));
  const auto& artifacts = document.at("artifacts");
  ASSERT_EQ(artifacts.size(), 3u);
  EXPECT_EQ(artifacts[0].at("role"), "dependency.layout");
  EXPECT_EQ(artifacts[1].at("role"), "producer");
  EXPECT_EQ(artifacts[2].at("role"), "rom");
  for (const auto& artifact : artifacts) {
    EXPECT_EQ(artifact.size(), 4u);
    const fs::path path = artifact.at("path").get<std::string>();
    EXPECT_TRUE(path.is_absolute());
    EXPECT_EQ(path, fs::canonical(path));
    const auto& bytes = inputs_.at(path);
    EXPECT_EQ(artifact.at("sha256"), ComputeSourceArtifactSha256(bytes));
    EXPECT_EQ(artifact.at("bytes").get<uint64_t>(), bytes.size());
  }
  ExpectInputsUnchanged();
  ExpectNoTemporaryArtifacts();
}

TEST_F(NativeSaveReceiptTest, NewOperationAdvancesGenerationForUnchangedFiles) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native save 2";
  const auto second = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(second.ok()) << second.status();
  EXPECT_EQ(second->generation, 2u);
  EXPECT_EQ(json::parse(second->json).at("operation_id"), "native save 2");
  request_.operation_id = "native-save-as-3";
  request_.operation = "save_as";
  const auto third = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(third.ok()) << third.status();
  EXPECT_EQ(third->generation, 3u);
  EXPECT_EQ(json::parse(third->json).at("operation"), "save_as");
  EXPECT_EQ(ReadBytes(receipt_path_), third->json);
  ExpectInputsUnchanged();
}

TEST_F(NativeSaveReceiptTest, ReversedBindingsReplayExactPersistedReceipt) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const std::string before = ReadBytes(receipt_path_);
  std::reverse(request_.artifacts.begin(), request_.artifacts.end());
  const auto result = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->generation, 1u);
  EXPECT_EQ(result->json, before);
  EXPECT_EQ(ReadBytes(receipt_path_), before);
  ExpectInputsUnchanged();
}

TEST_F(NativeSaveReceiptTest,
       IncompleteSaveRecordsErrorsAndCompleteNextSaveAdvances) {
  request_.binding_errors = {
      "dependency.project_layout: configured file is missing",
      "dependency.overlay: expected SHA-256 is stale"};
  const auto incomplete = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(incomplete.ok()) << incomplete.status();
  EXPECT_EQ(incomplete->generation, 1u);
  const auto incomplete_json = json::parse(incomplete->json);
  EXPECT_EQ(incomplete_json.at("bindings_complete"), false);
  EXPECT_EQ(incomplete_json.at("binding_errors"), request_.binding_errors);
  EXPECT_EQ(ReadBytes(receipt_path_), incomplete->json);
  auto conflicting = request_;
  conflicting.binding_errors = {"dependency.project_layout: different error"};
  ExpectRefusedWithoutMutation(conflicting);

  request_.operation_id = "native-save-2";
  request_.binding_errors.clear();
  const auto complete = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(complete.ok()) << complete.status();
  EXPECT_EQ(complete->generation, 2u);
  const auto complete_json = json::parse(complete->json);
  EXPECT_EQ(complete_json.at("bindings_complete"), true);
  EXPECT_EQ(complete_json.at("binding_errors"), json::array());
  EXPECT_EQ(ReadBytes(receipt_path_), complete->json);
  ExpectInputsUnchanged();
}

TEST_F(NativeSaveReceiptTest,
       ReusedOperationIdRejectsDifferentBindingsOrOperation) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  auto changed = request_;
  changed.operation = "save_as";
  ExpectRefusedWithoutMutation(changed);
  const auto alternate = temp_.path() / "alternate-producer.json";
  WriteBytes(alternate, inputs_.at(request_.artifacts[1].path));
  inputs_[alternate] = ReadBytes(alternate);
  changed = request_;
  changed.artifacts[1].path = alternate;
  ExpectRefusedWithoutMutation(changed);
  changed = request_;
  changed.artifacts.pop_back();
  ExpectRefusedWithoutMutation(changed);
}

TEST_F(NativeSaveReceiptTest, ReusedOperationIdRejectsNewlySavedInputBytes) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  auto& dependency = request_.artifacts.back();
  const std::string changed = "new native save layout\n";
  WriteBytes(dependency.path, changed);
  inputs_[dependency.path] = changed;
  dependency.expected_sha256 = ComputeSourceArtifactSha256(changed);
  ExpectRefusedWithoutMutation(request_);
}

TEST_F(NativeSaveReceiptTest,
       StaleExpectedHashPreservesCompletedSaveAndReceipt) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native-save-2";
  for (size_t i = 0; i < request_.artifacts.size(); ++i) {
    SCOPED_TRACE(i);
    auto stale = request_;
    stale.artifacts[i].expected_sha256 = ComputeSourceArtifactSha256("stale");
    ExpectRefusedWithoutMutation(stale);
  }
}

TEST_F(NativeSaveReceiptTest, MissingInputPreservesPriorReceiptAndOtherInputs) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native-save-2";
  for (size_t i = 0; i < request_.artifacts.size(); ++i) {
    SCOPED_TRACE(i);
    auto missing = request_;
    missing.artifacts[i].path = temp_.path() / "missing-input";
    ExpectRefusedWithoutMutation(missing);
    EXPECT_FALSE(fs::exists(missing.artifacts[i].path));
  }
}

TEST_F(NativeSaveReceiptTest,
       ProducerAndDependenciesHashBinaryFilesAcrossChunks) {
  const std::string producer(1024 * 1024 + 17, '\0');
  const std::string dependency(1024 * 1024 + 9, '\xE9');
  for (auto& artifact : request_.artifacts) {
    if (artifact.role == "rom") {
      continue;
    }
    const auto& bytes = artifact.role == "producer" ? producer : dependency;
    WriteBytes(artifact.path, bytes);
    inputs_[artifact.path] = bytes;
    artifact.expected_sha256 = ComputeSourceArtifactSha256(bytes);
  }
  const auto result = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(result.ok()) << result.status();
  const auto artifacts = json::parse(result->json).at("artifacts");
  EXPECT_EQ(artifacts[0].at("bytes").get<uint64_t>(), dependency.size());
  EXPECT_EQ(artifacts[1].at("bytes").get<uint64_t>(), producer.size());
  ExpectInputsUnchanged();
}

TEST_F(NativeSaveReceiptTest,
       RomLargerThan64MiBIsRejectedBeforeHashComparison) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const auto oversized = temp_.path() / "oversized.sfc";
  WriteBytes(oversized, "");
  constexpr uint64_t kOversizedBytes = 64u * 1024u * 1024u + 1u;
  fs::resize_file(oversized, kOversizedBytes);
  request_.operation_id = "native-save-2";
  request_.artifacts[0].path = oversized;
  const std::string before = ReadBytes(receipt_path_);
  const auto result = PublishNativeSaveReceipt(request_);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(ReadBytes(receipt_path_), before);
  EXPECT_EQ(fs::file_size(oversized), kOversizedBytes);
  std::ifstream input(oversized, std::ios::binary);
  ASSERT_TRUE(input.is_open());
  std::array<char, 8192> buffer;
  bool all_zero = true;
  while (input.read(buffer.data(), buffer.size()) || input.gcount() != 0) {
    all_zero &= std::all_of(buffer.begin(), buffer.begin() + input.gcount(),
                            [](char byte) { return byte == '\0'; });
  }
  EXPECT_TRUE(input.eof());
  EXPECT_TRUE(all_zero);
  ExpectInputsUnchanged();
  ExpectNoTemporaryArtifacts();
}

TEST_F(NativeSaveReceiptTest, InvalidRequestSchemaPreservesPriorReceipt) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native-save-2";
  std::vector<NativeSaveReceiptRequest> invalid;
  auto changed = request_;
  changed.operation_id.clear();
  invalid.push_back(changed);
  changed = request_;
  changed.operation_id = "operation with\nnewline";
  invalid.push_back(changed);
  changed = request_;
  changed.operation_id = std::string(129, 'x');
  invalid.push_back(changed);
  changed = request_;
  changed.operation = "autosave";
  invalid.push_back(changed);
  for (const auto& hash :
       {std::string(63, 'a'), std::string(64, 'A'), std::string(64, 'g')}) {
    changed = request_;
    changed.artifacts[0].expected_sha256 = hash;
    invalid.push_back(changed);
  }
  for (size_t i = 0; i < invalid.size(); ++i) {
    SCOPED_TRACE(i);
    ExpectRefusedWithoutMutation(invalid[i]);
  }
}

TEST_F(NativeSaveReceiptTest, MissingDuplicateOrInvalidRolesAreRejected) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native-save-2";
  for (size_t i : {size_t{0}, size_t{1}}) {
    auto missing = request_;
    missing.artifacts.erase(missing.artifacts.begin() + i);
    ExpectRefusedWithoutMutation(missing);
  }
  auto duplicate = request_;
  duplicate.artifacts.back().role = "producer";
  ExpectRefusedWithoutMutation(duplicate);
  for (const auto& role :
       {std::string(), std::string("Dependency.layout"),
        std::string("dependency bad"), std::string(129, 'a')}) {
    SCOPED_TRACE(role);
    auto invalid = request_;
    invalid.artifacts.back().role = role;
    ExpectRefusedWithoutMutation(invalid);
  }
}

TEST_F(NativeSaveReceiptTest, RelativeAndNonFileInputPathsAreRejected) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native-save-2";
  auto invalid = request_;
  invalid.receipt_path = fs::relative(receipt_path_, fs::current_path());
  ExpectRefusedWithoutMutation(invalid);
  invalid = request_;
  invalid.receipt_path = temp_.path() / "missing-parent" / "receipt.json";
  ExpectRefusedWithoutMutation(invalid);
  EXPECT_FALSE(fs::exists(invalid.receipt_path));
  invalid = request_;
  invalid.artifacts[0].path =
      fs::relative(request_.artifacts[0].path, fs::current_path());
  ExpectRefusedWithoutMutation(invalid);
  invalid = request_;
  invalid.artifacts[0].path = temp_.path();
  ExpectRefusedWithoutMutation(invalid);
}

TEST_F(NativeSaveReceiptTest, MalformedPriorReceiptIsPreserved) {
  for (const auto& bytes : {std::string("{"), std::string("[]"),
                            std::string("null"), std::string()}) {
    SCOPED_TRACE(bytes);
    WriteBytes(receipt_path_, bytes);
    ExpectRefusedWithoutMutation(request_);
  }
}

TEST_F(NativeSaveReceiptTest, PriorReceiptSchemaErrorsArePreserved) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const auto original = json::parse(ReadBytes(receipt_path_));
  std::vector<json> invalid;
  auto changed = original;
  changed["kind"] = "yaze.other.v1";
  invalid.push_back(changed);
  for (const auto& generation : {json(0), json(-1), json(1.5), json("1")}) {
    changed = original;
    changed["generation"] = generation;
    invalid.push_back(changed);
  }
  changed = original;
  changed["unexpected"] = true;
  invalid.push_back(changed);
  changed = original;
  changed["operation"] = "autosave";
  invalid.push_back(changed);
  changed = original;
  changed["receipt_path"] = (temp_.path() / "other-receipt.json").string();
  invalid.push_back(changed);
  changed = original;
  changed["artifacts"][0]["unexpected"] = true;
  invalid.push_back(changed);
  changed = original;
  changed["artifacts"][0]["sha256"] = std::string(64, 'A');
  invalid.push_back(changed);
  changed = original;
  changed["artifacts"][0]["bytes"] = -1;
  invalid.push_back(changed);
  changed = original;
  changed["artifacts"][0]["path"] = "relative-input";
  invalid.push_back(changed);
  changed = original;
  changed["artifacts"].erase(2);
  invalid.push_back(changed);
  changed = original;
  changed["artifacts"][0]["role"] = "producer";
  invalid.push_back(changed);
  for (size_t i = 0; i < invalid.size(); ++i) {
    SCOPED_TRACE(i);
    WriteBytes(receipt_path_, invalid[i].dump());
    ExpectRefusedWithoutMutation(request_);
  }
}

TEST_F(NativeSaveReceiptTest,
       GenerationOverflowRefusesNewOperationButAllowsReplay) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  auto document = json::parse(ReadBytes(receipt_path_));
  document["generation"] = std::numeric_limits<uint64_t>::max();
  const std::string maximum_receipt = document.dump() + "\n";
  WriteBytes(receipt_path_, maximum_receipt);
  const auto replay = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(replay.ok()) << replay.status();
  EXPECT_EQ(replay->generation, std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(replay->json, maximum_receipt);
  request_.operation_id = "native-save-2";
  ExpectRefusedWithoutMutation(request_);
}

TEST_F(NativeSaveReceiptTest, SharedCanonicalInputPathRetainsRolesAndReplays) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native-save-2";
  request_.artifacts[1].path = request_.artifacts[0].path;
  request_.artifacts[1].expected_sha256 = request_.artifacts[0].expected_sha256;
  const auto result = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->generation, 2u);
  const auto artifacts = json::parse(result->json).at("artifacts");
  ASSERT_EQ(artifacts.size(), 3u);
  EXPECT_EQ(artifacts[1].at("role"), "producer");
  EXPECT_EQ(artifacts[2].at("role"), "rom");
  EXPECT_EQ(artifacts[1].at("path"), artifacts[2].at("path"));
  EXPECT_EQ(artifacts[1].at("sha256"), artifacts[2].at("sha256"));
  EXPECT_EQ(artifacts[1].at("bytes"), artifacts[2].at("bytes"));
  EXPECT_EQ(ReadBytes(receipt_path_), result->json);
  std::reverse(request_.artifacts.begin(), request_.artifacts.end());
  const auto replay = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(replay.ok()) << replay.status();
  EXPECT_EQ(replay->generation, 2u);
  EXPECT_EQ(replay->json, result->json);
  EXPECT_EQ(ReadBytes(receipt_path_), result->json);
  ExpectInputsUnchanged();
  ExpectNoTemporaryArtifacts();
}

TEST_F(NativeSaveReceiptTest,
       SharedCanonicalInputPathWithConflictingHashesRefuses) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native-save-2";
  request_.artifacts[1].path = request_.artifacts[0].path;
  ASSERT_NE(request_.artifacts[1].expected_sha256,
            request_.artifacts[0].expected_sha256);
  ExpectRefusedWithoutMutation(request_);
}

TEST_F(NativeSaveReceiptTest,
       DifferentInputPathsWithHardLinkAliasesAreRejected) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native-save-2";
  const auto alias = temp_.path() / "rom-hard-link";
  std::error_code ec;
  fs::create_hard_link(request_.artifacts[0].path, alias, ec);
  ASSERT_FALSE(ec) << ec.message();
  inputs_[alias] = ReadBytes(alias);
  auto aliased = request_;
  aliased.artifacts[1].path = alias;
  aliased.artifacts[1].expected_sha256 = request_.artifacts[0].expected_sha256;
  ExpectRefusedWithoutMutation(aliased);
  EXPECT_TRUE(fs::equivalent(alias, request_.artifacts[0].path));
}

TEST_F(NativeSaveReceiptTest, ReceiptInputPathsAndHardLinkAliasesAreRejected) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native-save-2";
  auto aliased = request_;
  aliased.artifacts[1].path = receipt_path_;
  aliased.artifacts[1].expected_sha256 =
      ComputeSourceArtifactSha256(ReadBytes(receipt_path_));
  ExpectRefusedWithoutMutation(aliased);
  const auto alias = temp_.path() / "receipt-hard-link";
  std::error_code ec;
  fs::create_hard_link(receipt_path_, alias, ec);
  ASSERT_FALSE(ec) << ec.message();
  inputs_[alias] = ReadBytes(alias);
  aliased.artifacts[1].path = alias;
  ExpectRefusedWithoutMutation(aliased);
  EXPECT_TRUE(fs::equivalent(alias, receipt_path_));
}

TEST_F(NativeSaveReceiptTest, SymlinkInputIsRejectedWithoutChangingItsTarget) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native-save-2";
  const auto alias = temp_.path() / "rom-symlink";
  std::error_code ec;
  fs::create_symlink(request_.artifacts[0].path, alias, ec);
#if defined(_WIN32)
  if (ec == std::errc::operation_not_permitted ||
      ec == std::errc::permission_denied) {
    GTEST_SKIP() << "Symbolic link privilege is unavailable: " << ec.message();
  }
#endif
  ASSERT_FALSE(ec) << ec.message();
  request_.artifacts[0].path = alias;
  ExpectRefusedWithoutMutation(request_);
  EXPECT_TRUE(fs::is_symlink(alias));
}

TEST_F(NativeSaveReceiptTest,
       SymlinkReceiptIsRejectedWithoutChangingItsTarget) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  request_.operation_id = "native-save-2";
  const auto target = temp_.path() / "receipt-target.json";
  fs::rename(receipt_path_, target);
  const std::string before = ReadBytes(target);
  std::error_code ec;
  fs::create_symlink(target, receipt_path_, ec);
#if defined(_WIN32)
  if (ec == std::errc::operation_not_permitted ||
      ec == std::errc::permission_denied) {
    GTEST_SKIP() << "Symbolic link privilege is unavailable: " << ec.message();
  }
#endif
  ASSERT_FALSE(ec) << ec.message();
  ExpectRefusedWithoutMutation(request_);
  EXPECT_TRUE(fs::is_symlink(receipt_path_));
  EXPECT_EQ(ReadBytes(target), before);
}

TEST_F(NativeSaveReceiptTest,
       DirectoryAtPersistentLockPathPreservesPriorReceipt) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const auto lock_path = temp_.path() / kSourceArtifactPublicationLockBasename;
  ASSERT_TRUE(fs::remove(lock_path));
  ASSERT_TRUE(fs::create_directory(lock_path));
  request_.operation_id = "native-save-2";
  ExpectRefusedWithoutMutation(request_);
  EXPECT_TRUE(fs::is_directory(lock_path));
}

TEST_F(NativeSaveReceiptTest,
       SymlinkAtPersistentLockPathPreservesPriorReceipt) {
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const auto lock_path = temp_.path() / kSourceArtifactPublicationLockBasename;
  ASSERT_TRUE(fs::remove(lock_path));
  const auto target = temp_.path() / "lock-target";
  WriteBytes(target, "preserved lock target\n");
  inputs_[target] = ReadBytes(target);
  std::error_code ec;
  fs::create_symlink(target, lock_path, ec);
#if defined(_WIN32)
  if (ec == std::errc::operation_not_permitted ||
      ec == std::errc::permission_denied) {
    GTEST_SKIP() << "Symbolic link privilege is unavailable: " << ec.message();
  }
#endif
  ASSERT_FALSE(ec) << ec.message();
  request_.operation_id = "native-save-2";
  ExpectRefusedWithoutMutation(request_);
  EXPECT_TRUE(fs::is_symlink(lock_path));
}

TEST_F(NativeSaveReceiptTest, ContextIdentityUsesCurrentPrivateAppDataRoot) {
  const auto original = GetAppDataOverride();
  const auto origin = request_.artifacts[0].path;
  const std::string canonical_origin = JsonPathText(fs::canonical(origin));
  const std::string key = ComputeSourceArtifactSha256(canonical_origin);
  {
    ScopedAppDataOverride app_data(temp_.path() / "app-data-one");
    ASSERT_TRUE(app_data.ok());
    const auto first = GetNativeSaveContext(
        (origin.parent_path() / "." / origin.filename()).string(),
        (temp_.path() / "first.yaze").string());
    ASSERT_TRUE(first.ok()) << first.status();
    EXPECT_EQ(first->editing_source_path, canonical_origin);
    EXPECT_EQ(first->path.filename(), key + ".json");
    EXPECT_TRUE(first->path.is_absolute());
    EXPECT_EQ(first->path.parent_path(),
              temp_.path() / "app-data-one" / "native-saves");
    EXPECT_TRUE(fs::is_directory(first->path.parent_path()));
    EXPECT_FALSE(fs::exists(first->path));
    const auto changed = GetNativeSaveContext(
        canonical_origin, (temp_.path() / "second.yaze").string());
    ASSERT_TRUE(changed.ok()) << changed.status();
    EXPECT_EQ(changed->editing_source_path, canonical_origin);
    EXPECT_EQ(changed->path, first->path);
    EXPECT_EQ(changed->project_path,
              JsonPathText(temp_.path() / "second.yaze"));
    const auto different_origin = GetNativeSaveContext(
        request_.artifacts[2].path.string(), changed->project_path);
    ASSERT_TRUE(different_origin.ok()) << different_origin.status();
    EXPECT_NE(different_origin->path, first->path);
    EXPECT_EQ(different_origin->path.filename(),
              ComputeSourceArtifactSha256(
                  JsonPathText(fs::canonical(request_.artifacts[2].path))) +
                  ".json");
    EXPECT_FALSE(fs::exists(different_origin->path));
    {
      ScopedAppDataOverride other_app_data(temp_.path() / "app-data-two");
      ASSERT_TRUE(other_app_data.ok());
      const auto other = GetNativeSaveContext(canonical_origin, "");
      ASSERT_TRUE(other.ok()) << other.status();
      EXPECT_EQ(other->editing_source_path, canonical_origin);
      EXPECT_EQ(other->path.filename(), first->path.filename());
      EXPECT_NE(other->path.parent_path(), first->path.parent_path());
      EXPECT_EQ(other->path.parent_path(),
                temp_.path() / "app-data-two" / "native-saves");
      EXPECT_FALSE(fs::exists(other->path));
    }
    const auto restored =
        GetNativeSaveContext(canonical_origin, first->project_path);
    ASSERT_TRUE(restored.ok()) << restored.status();
    EXPECT_EQ(restored->path, first->path);
    EXPECT_FALSE(fs::exists(restored->path));
  }
  EXPECT_EQ(GetAppDataOverride(), original);
  ExpectInputsUnchanged();
}

TEST_F(NativeSaveReceiptTest, ContextCommitsMatchingGenerationsAndReplays) {
  ScopedAppDataOverride app_data(temp_.path() / "app-data");
  ASSERT_TRUE(app_data.ok());
  const auto context = GetNativeSaveContext(
      request_.artifacts[0].path.string(), request_.artifacts[1].path.string());
  ASSERT_TRUE(context.ok()) << context.status();
  EXPECT_FALSE(fs::exists(context->path));
  request_.context = *context;
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const auto receipt = json::parse(ReadBytes(receipt_path_));
  EXPECT_EQ(receipt.size(), 8u);
  EXPECT_FALSE(receipt.contains("context_path"));
  EXPECT_FALSE(receipt.contains("context_id"));
  ExpectCommittedContext(*context, request_, 1);
  request_.operation_id = "native-save-2";
  const auto second = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(second.ok()) << second.status();
  EXPECT_EQ(second->generation, 2u);
  ExpectCommittedContext(*context, request_, 2);
  const std::string pointer_before = ReadBytes(context->path);
  const auto replay = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(replay.ok()) << replay.status();
  EXPECT_EQ(replay->generation, 2u);
  EXPECT_EQ(replay->json, second->json);
  EXPECT_EQ(ReadBytes(receipt_path_), second->json);
  EXPECT_EQ(ReadBytes(context->path), pointer_before);
  ExpectInputsUnchanged();
  ExpectNoTemporaryArtifacts();
}

TEST_F(NativeSaveReceiptTest, ContextSaveAsMovesPointerAndPreservesOldBranch) {
  ScopedAppDataOverride app_data(temp_.path() / "app-data");
  ASSERT_TRUE(app_data.ok());
  const auto original_context = GetNativeSaveContext(
      request_.artifacts[0].path.string(), request_.artifacts[1].path.string());
  ASSERT_TRUE(original_context.ok()) << original_context.status();
  request_.context = *original_context;
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const std::string original_receipt = ReadBytes(receipt_path_);
  const auto branch = temp_.path() / "save-as";
  const auto new_rom = branch / "saved.sfc";
  const auto new_project = branch / "project.yaze";
  WriteBytes(new_rom, "new saved ROM\n");
  WriteBytes(new_project, "new producer\n");
  inputs_[new_rom] = ReadBytes(new_rom);
  inputs_[new_project] = ReadBytes(new_project);
  request_.receipt_path = branch / "receipt.json";
  request_.operation_id = "native-save-as-2";
  request_.operation = "save_as";
  request_.artifacts[0] = {"rom", new_rom,
                           ComputeSourceArtifactSha256(inputs_.at(new_rom))};
  request_.artifacts[1] = {
      "producer", new_project,
      ComputeSourceArtifactSha256(inputs_.at(new_project))};
  const auto context = GetNativeSaveContext(
      original_context->editing_source_path, new_project.string());
  ASSERT_TRUE(context.ok()) << context.status();
  EXPECT_EQ(context->path, original_context->path);
  request_.context = *context;
  const auto result = PublishNativeSaveReceipt(request_);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->generation, 1u);
  EXPECT_EQ(ReadBytes(request_.receipt_path), result->json);
  EXPECT_EQ(ReadBytes(receipt_path_), original_receipt);
  ExpectCommittedContext(*context, request_, 1);
  ExpectInputsUnchanged();
  ExpectNoTemporaryArtifacts();
}

TEST_F(NativeSaveReceiptTest,
       ContextRetirementRecordsNewIdentityWithoutGeneration) {
  ScopedAppDataOverride app_data(temp_.path() / "app-data");
  ASSERT_TRUE(app_data.ok());
  const auto original_context = GetNativeSaveContext(
      request_.artifacts[0].path.string(), request_.artifacts[1].path.string());
  ASSERT_TRUE(original_context.ok()) << original_context.status();
  request_.context = *original_context;
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const std::string receipt_before = ReadBytes(receipt_path_);
  const auto committed = json::parse(ReadBytes(original_context->path));
  for (const std::string state : {"rebound", "session_changed"}) {
    SCOPED_TRACE(state);
    const auto active =
        GetNativeSaveContext(original_context->editing_source_path,
                             (temp_.path() / (state + ".yaze")).string());
    ASSERT_TRUE(active.ok()) << active.status();
    const auto status = RetireNativeSaveContext(*active, state);
    ASSERT_TRUE(status.ok()) << status;
    const auto document = json::parse(ReadBytes(active->path));
    EXPECT_EQ(document.size(), 7u);
    EXPECT_EQ(document.at("kind"), "yaze.native_save_context.v1");
    EXPECT_EQ(document.at("editing_source_path"),
              original_context->editing_source_path);
    EXPECT_EQ(document.at("project_path"), active->project_path);
    EXPECT_EQ(document.at("state"), "rebound");
    EXPECT_EQ(document.at("rom_path"), committed.at("rom_path"));
    EXPECT_EQ(document.at("producer"), committed.at("producer"));
    EXPECT_TRUE(document.at("native_commit").is_null());
    EXPECT_EQ(ReadBytes(receipt_path_), receipt_before);
  }
  ExpectInputsUnchanged();
}

TEST_F(NativeSaveReceiptTest,
       ContextReplayAfterRetirementCannotResurrectPointer) {
  ScopedAppDataOverride app_data(temp_.path() / "app-data");
  ASSERT_TRUE(app_data.ok());
  const auto context = GetNativeSaveContext(
      request_.artifacts[0].path.string(), request_.artifacts[1].path.string());
  ASSERT_TRUE(context.ok()) << context.status();
  request_.context = *context;
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const auto active = GetNativeSaveContext(
      context->editing_source_path, (temp_.path() / "active.yaze").string());
  ASSERT_TRUE(active.ok()) << active.status();
  ASSERT_TRUE(RetireNativeSaveContext(*active, "session_changed").ok());
  const std::string retired = ReadBytes(active->path);
  ExpectRefusedWithoutMutation(request_);
  EXPECT_EQ(ReadBytes(active->path), retired);
  EXPECT_EQ(json::parse(retired).at("state"), "rebound");
  EXPECT_TRUE(json::parse(retired).at("native_commit").is_null());
}

TEST_F(NativeSaveReceiptTest, ContextRetirementOfMissingPointerCreatesNoFile) {
  ScopedAppDataOverride app_data(temp_.path() / "app-data");
  ASSERT_TRUE(app_data.ok());
  const auto context =
      GetNativeSaveContext(request_.artifacts[0].path.string(), "");
  ASSERT_TRUE(context.ok()) << context.status();
  ASSERT_FALSE(fs::exists(context->path));
  EXPECT_TRUE(RetireNativeSaveContext(*context, "rebound").ok());
  EXPECT_TRUE(RetireNativeSaveContext(*context, "session_changed").ok());
  EXPECT_FALSE(fs::exists(context->path));
  EXPECT_FALSE(fs::exists(receipt_path_));
  ExpectInputsUnchanged();
}

TEST_F(NativeSaveReceiptTest, ContextMalformedOrForeignIdentityIsPreserved) {
  ScopedAppDataOverride app_data(temp_.path() / "app-data");
  ASSERT_TRUE(app_data.ok());
  const auto context = GetNativeSaveContext(
      request_.artifacts[0].path.string(), request_.artifacts[1].path.string());
  ASSERT_TRUE(context.ok()) << context.status();
  request_.context = *context;
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  auto invalid_origin = json::parse(ReadBytes(context->path));
  invalid_origin["editing_source_path"] = "relative-origin.sfc";
  auto foreign_origin = json::parse(ReadBytes(context->path));
  foreign_origin["editing_source_path"] =
      JsonPathText(fs::canonical(request_.artifacts[2].path));
  request_.operation_id = "native-save-2";
  for (const auto& bytes :
       {std::string("{"), invalid_origin.dump(), foreign_origin.dump()}) {
    SCOPED_TRACE(bytes);
    WriteBytes(context->path, bytes);
    ExpectRefusedWithoutMutation(request_);
    EXPECT_EQ(ReadBytes(context->path), bytes);
    EXPECT_FALSE(RetireNativeSaveContext(*context, "rebound").ok());
    EXPECT_EQ(ReadBytes(context->path), bytes);
  }
}

TEST_F(NativeSaveReceiptTest, ContextSymlinkRefusesWithoutChangingTarget) {
  ScopedAppDataOverride app_data(temp_.path() / "app-data");
  ASSERT_TRUE(app_data.ok());
  const auto context = GetNativeSaveContext(
      request_.artifacts[0].path.string(), request_.artifacts[1].path.string());
  ASSERT_TRUE(context.ok()) << context.status();
  request_.context = *context;
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const std::string before = ReadBytes(context->path);
  const auto target = context->path.parent_path() / "context-target.json";
  fs::rename(context->path, target);
  std::error_code ec;
  fs::create_symlink(target, context->path, ec);
#if defined(_WIN32)
  if (ec == std::errc::operation_not_permitted ||
      ec == std::errc::permission_denied) {
    GTEST_SKIP() << "Symbolic link privilege is unavailable: " << ec.message();
  }
#endif
  ASSERT_FALSE(ec) << ec.message();
  request_.operation_id = "native-save-2";
  ExpectRefusedWithoutMutation(request_);
  EXPECT_TRUE(fs::is_symlink(context->path));
  EXPECT_EQ(ReadBytes(target), before);
  EXPECT_FALSE(RetireNativeSaveContext(*context, "rebound").ok());
  EXPECT_EQ(ReadBytes(target), before);
}

TEST_F(NativeSaveReceiptTest, ContextOutputAliasingReceiptOrInputRefuses) {
  ScopedAppDataOverride app_data(temp_.path() / "app-data");
  ASSERT_TRUE(app_data.ok());
  const auto context = GetNativeSaveContext(
      request_.artifacts[0].path.string(), request_.artifacts[1].path.string());
  ASSERT_TRUE(context.ok()) << context.status();
  request_.context = *context;
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const std::string pointer_before = ReadBytes(context->path);
  request_.operation_id = "native-save-2";
  auto duplicate_outputs = request_;
  duplicate_outputs.receipt_path = context->path;
  ExpectRefusedWithoutMutation(duplicate_outputs);
  EXPECT_EQ(ReadBytes(context->path), pointer_before);

  auto output_input_alias = request_;
  output_input_alias.artifacts[1].path = context->path;
  output_input_alias.artifacts[1].expected_sha256 =
      ComputeSourceArtifactSha256(pointer_before);
  ExpectRefusedWithoutMutation(output_input_alias);
  EXPECT_EQ(ReadBytes(context->path), pointer_before);
}

TEST_F(NativeSaveReceiptTest, ContextLockDirectoryFailurePreservesPublication) {
  ScopedAppDataOverride app_data(temp_.path() / "app-data");
  ASSERT_TRUE(app_data.ok());
  const auto context = GetNativeSaveContext(
      request_.artifacts[0].path.string(), request_.artifacts[1].path.string());
  ASSERT_TRUE(context.ok()) << context.status();
  request_.context = *context;
  ASSERT_NO_FATAL_FAILURE(SeedReceipt());
  const std::string pointer_before = ReadBytes(context->path);
  const auto lock_path =
      context->path.parent_path() / kSourceArtifactPublicationLockBasename;
  ASSERT_TRUE(fs::remove(lock_path));
  ASSERT_TRUE(fs::create_directory(lock_path));
  request_.operation_id = "native-save-2";
  ExpectRefusedWithoutMutation(request_);
  EXPECT_EQ(ReadBytes(context->path), pointer_before);
  EXPECT_TRUE(fs::is_directory(lock_path));
}

#endif  // defined(__EMSCRIPTEN__)

}  // namespace
}  // namespace yaze::core

#else

namespace yaze::core {
namespace {

TEST(NativeSaveReceiptTest, JsonDisabledApisAreUnavailable) {
  EXPECT_TRUE(absl::IsUnimplemented(GetNativeSaveContext("", "").status()));
  EXPECT_TRUE(absl::IsUnimplemented(RetireNativeSaveContext({}, "rebound")));
  EXPECT_TRUE(absl::IsUnimplemented(PublishNativeSaveReceipt({}).status()));
}

}  // namespace
}  // namespace yaze::core

#endif  // defined(YAZE_WITH_JSON)
