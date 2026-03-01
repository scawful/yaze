// Unit tests for the 0.7.0 HTTP API endpoints:
//   POST /api/v1/command/execute
//   GET  /api/v1/command/list
//   GET  /api/v1/annotations[?room=X]
//   POST /api/v1/annotations
//   PUT  /api/v1/annotations/<id>   (requires req.matches[1])
//   DELETE /api/v1/annotations/<id> (requires req.matches[1])
//
// Test strategy:
//   - Command tests rely solely on the real CommandRegistry singleton (no ROM
//     needed; "test-list" and "test-status" are both requires_rom=false).
//   - Annotation tests use a per-test temporary directory so every test is
//     isolated and leaves no permanent state on disk.
//   - httplib::Request::matches is std::smatch. To simulate capture-group
//     routing we use std::regex_match against the same pattern the server
//     registers: R"(/api/v1/annotations/(.+))".

#include "cli/service/api/api_handlers.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "httplib.h"
#include "nlohmann/json.hpp"

namespace yaze::cli::api {
namespace {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Populate req.matches as httplib does when a regex route matches.
// The server pattern for annotation update/delete is "/api/v1/annotations/(.+)".
static const std::regex kAnnotationPathRegex(R"(/api/v1/annotations/(.+))");

void SetAnnotationPathMatch(httplib::Request& req, const std::string& path) {
  req.path = path;
  std::regex_match(req.path, req.matches, kAnnotationPathRegex);
}

// Write a minimal annotations JSON file to disk so tests can seed state.
void WriteAnnotationsFile(const std::string& dir, const json& data) {
  std::filesystem::create_directories(dir + "/Docs/Dev/Planning");
  std::ofstream f(dir + "/Docs/Dev/Planning/annotations.json");
  f << data.dump(2);
}

// Read the annotations file that the handler writes.
json ReadAnnotationsFile(const std::string& dir) {
  std::ifstream f(dir + "/Docs/Dev/Planning/annotations.json");
  if (!f.is_open())
    return json::object();
  try {
    return json::parse(f);
  } catch (...) {
    return json::object();
  }
}

// ---------------------------------------------------------------------------
// Test fixture: creates and removes a unique temp directory per test.
// ---------------------------------------------------------------------------

class AnnotationHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    project_dir_ = (std::filesystem::temp_directory_path() /
                    ("yaze_ann_test_" + std::to_string(test_counter_++)))
                       .string();
    std::filesystem::create_directories(project_dir_);
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(project_dir_, ec);
  }

  // Seed the annotations file with a pre-built JSON object.
  void SeedAnnotations(const json& file_data) {
    WriteAnnotationsFile(project_dir_, file_data);
  }

  // Build a simple annotation JSON value.
  static json MakeAnnotation(const std::string& id, int room_id,
                             const std::string& text) {
    json ann;
    ann["id"] = id;
    ann["room_id"] = room_id;
    ann["text"] = text;
    return ann;
  }

  std::string project_dir_;

 private:
  static int test_counter_;
};

int AnnotationHandlerTest::test_counter_ = 0;

// ---------------------------------------------------------------------------
// HandleCommandList tests
// ---------------------------------------------------------------------------

TEST(CommandListHandlerTest, ReturnsStatus200) {
  httplib::Request req;
  httplib::Response res;

  HandleCommandList(req, res);

  EXPECT_EQ(res.status, 200);
}

TEST(CommandListHandlerTest, BodyIsValidJson) {
  httplib::Request req;
  httplib::Response res;

  HandleCommandList(req, res);

  ASSERT_NO_THROW(json::parse(res.body));
}

TEST(CommandListHandlerTest, ResponseContainsCommandsArray) {
  httplib::Request req;
  httplib::Response res;

  HandleCommandList(req, res);

  auto body = json::parse(res.body);
  ASSERT_TRUE(body.contains("commands"));
  EXPECT_TRUE(body["commands"].is_array());
}

TEST(CommandListHandlerTest, ResponseContainsCount) {
  httplib::Request req;
  httplib::Response res;

  HandleCommandList(req, res);

  auto body = json::parse(res.body);
  ASSERT_TRUE(body.contains("count"));
  EXPECT_TRUE(body["count"].is_number());
}

TEST(CommandListHandlerTest, CountMatchesCommandsArrayLength) {
  httplib::Request req;
  httplib::Response res;

  HandleCommandList(req, res);

  auto body = json::parse(res.body);
  EXPECT_EQ(body["count"], body["commands"].size());
}

TEST(CommandListHandlerTest, CountIsPositive) {
  httplib::Request req;
  httplib::Response res;

  HandleCommandList(req, res);

  auto body = json::parse(res.body);
  EXPECT_GT(body["count"].get<int>(), 0);
}

TEST(CommandListHandlerTest, EachCommandEntryHasRequiredFields) {
  httplib::Request req;
  httplib::Response res;

  HandleCommandList(req, res);

  auto body = json::parse(res.body);
  ASSERT_TRUE(body["commands"].is_array());
  ASSERT_FALSE(body["commands"].empty());

  for (const auto& cmd : body["commands"]) {
    EXPECT_TRUE(cmd.contains("name")) << "Missing 'name' in: " << cmd.dump();
    EXPECT_TRUE(cmd.contains("category"))
        << "Missing 'category' in: " << cmd.dump();
    EXPECT_TRUE(cmd.contains("description"))
        << "Missing 'description' in: " << cmd.dump();
    EXPECT_TRUE(cmd.contains("usage")) << "Missing 'usage' in: " << cmd.dump();
    EXPECT_TRUE(cmd.contains("requires_rom"))
        << "Missing 'requires_rom' in: " << cmd.dump();
  }
}

TEST(CommandListHandlerTest, ContainsKnownCommand) {
  // "test-list" is a stable, requires_rom=false command in the "test" category.
  httplib::Request req;
  httplib::Response res;

  HandleCommandList(req, res);

  auto body = json::parse(res.body);
  bool found = false;
  for (const auto& cmd : body["commands"]) {
    if (cmd["name"] == "test-list") {
      found = true;
      EXPECT_EQ(cmd["category"], "test");
      EXPECT_FALSE(cmd["requires_rom"].get<bool>());
      break;
    }
  }
  EXPECT_TRUE(found) << "Expected 'test-list' in command catalog";
}

TEST(CommandListHandlerTest, ContainsRomCategory) {
  httplib::Request req;
  httplib::Response res;

  HandleCommandList(req, res);

  auto body = json::parse(res.body);
  bool found_rom_category = false;
  for (const auto& cmd : body["commands"]) {
    if (cmd["category"] == "rom") {
      found_rom_category = true;
      break;
    }
  }
  EXPECT_TRUE(found_rom_category)
      << "Expected at least one command in the 'rom' category";
}

TEST(CommandListHandlerTest, SetsCorsHeader) {
  httplib::Request req;
  httplib::Response res;

  HandleCommandList(req, res);

  auto it = res.headers.find("Access-Control-Allow-Origin");
  ASSERT_NE(it, res.headers.end());
  EXPECT_EQ(it->second, "*");
}

// ---------------------------------------------------------------------------
// HandleCommandExecute tests
// ---------------------------------------------------------------------------

TEST(CommandExecuteHandlerTest, MissingCommandFieldReturns400) {
  httplib::Request req;
  req.body = R"({})";
  httplib::Response res;

  HandleCommandExecute(req, res, nullptr);

  EXPECT_EQ(res.status, 400);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
}

TEST(CommandExecuteHandlerTest, EmptyCommandFieldReturns400) {
  httplib::Request req;
  req.body = R"({"command":""})";
  httplib::Response res;

  HandleCommandExecute(req, res, nullptr);

  EXPECT_EQ(res.status, 400);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
}

TEST(CommandExecuteHandlerTest, UnknownCommandReturns404) {
  httplib::Request req;
  req.body = R"({"command":"no-such-command-xyz"})";
  httplib::Response res;

  HandleCommandExecute(req, res, nullptr);

  EXPECT_EQ(res.status, 404);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
  EXPECT_NE(body["error"].get<std::string>().find("no-such-command-xyz"),
            std::string::npos);
}

TEST(CommandExecuteHandlerTest, InvalidJsonBodyReturns400) {
  httplib::Request req;
  req.body = "not-json{{{";
  httplib::Response res;

  HandleCommandExecute(req, res, nullptr);

  EXPECT_EQ(res.status, 400);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
}

TEST(CommandExecuteHandlerTest, ValidNoRomCommandReturns200) {
  // "test-list" does not require a ROM and should succeed with rom=nullptr.
  httplib::Request req;
  req.body = R"({"command":"test-list","args":[]})";
  httplib::Response res;

  HandleCommandExecute(req, res, nullptr);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "ok");
  EXPECT_EQ(body["command"], "test-list");
}

TEST(CommandExecuteHandlerTest, SuccessfulResponseContainsResultField) {
  httplib::Request req;
  req.body = R"({"command":"test-list","args":[]})";
  httplib::Response res;

  HandleCommandExecute(req, res, nullptr);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("result"));
}

TEST(CommandExecuteHandlerTest, SuccessfulResponseEchoesCommandName) {
  httplib::Request req;
  req.body = R"({"command":"test-list"})";
  httplib::Response res;

  HandleCommandExecute(req, res, nullptr);

  auto body = json::parse(res.body);
  EXPECT_EQ(body["command"], "test-list");
}

TEST(CommandExecuteHandlerTest, SetsCorsHeaderOnSuccess) {
  httplib::Request req;
  req.body = R"({"command":"test-list"})";
  httplib::Response res;

  HandleCommandExecute(req, res, nullptr);

  auto it = res.headers.find("Access-Control-Allow-Origin");
  ASSERT_NE(it, res.headers.end());
  EXPECT_EQ(it->second, "*");
}

TEST(CommandExecuteHandlerTest, SetsCorsHeaderOnError) {
  httplib::Request req;
  req.body = R"({"command":"nonexistent-command"})";
  httplib::Response res;

  HandleCommandExecute(req, res, nullptr);

  auto it = res.headers.find("Access-Control-Allow-Origin");
  ASSERT_NE(it, res.headers.end());
  EXPECT_EQ(it->second, "*");
}

TEST(CommandExecuteHandlerTest, ArgsArrayPassedToCommand) {
  // "test-status" is requires_rom=false; passing --format=json exercises the
  // args forwarding path without a ROM.
  httplib::Request req;
  req.body = R"({"command":"test-status","args":["--format=json"]})";
  httplib::Response res;

  HandleCommandExecute(req, res, nullptr);

  // The command may succeed or fail depending on build configuration, but it
  // must not return 404 (unknown) or 400 (bad JSON parse).
  EXPECT_NE(res.status, 404);
  EXPECT_NE(res.status, 400);
}

// ---------------------------------------------------------------------------
// HandleAnnotationList tests
// ---------------------------------------------------------------------------

TEST_F(AnnotationHandlerTest, EmptyProjectPathReturns503) {
  httplib::Request req;
  httplib::Response res;

  HandleAnnotationList(req, res, "");

  EXPECT_EQ(res.status, 503);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
}

TEST_F(AnnotationHandlerTest, NoFileYieldsEmptyAnnotationsArray) {
  httplib::Request req;
  httplib::Response res;

  HandleAnnotationList(req, res, project_dir_);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  ASSERT_TRUE(body.contains("annotations"));
  EXPECT_TRUE(body["annotations"].is_array());
  EXPECT_TRUE(body["annotations"].empty());
}

TEST_F(AnnotationHandlerTest, ListReturnsAllAnnotations) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-1", 10, "note one"),
                   MakeAnnotation("ann-2", 11, "note two")});
  SeedAnnotations(file_data);

  httplib::Request req;
  httplib::Response res;

  HandleAnnotationList(req, res, project_dir_);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["annotations"].size(), 2u);
}

TEST_F(AnnotationHandlerTest, RoomFilterReturnsonlyMatchingAnnotations) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-1", 10, "room 10 note"),
                   MakeAnnotation("ann-2", 11, "room 11 note"),
                   MakeAnnotation("ann-3", 10, "another room 10 note")});
  SeedAnnotations(file_data);

  httplib::Request req;
  req.params.emplace("room", "10");
  httplib::Response res;

  HandleAnnotationList(req, res, project_dir_);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  ASSERT_EQ(body["annotations"].size(), 2u);
  for (const auto& ann : body["annotations"]) {
    EXPECT_EQ(ann["room_id"].get<int>(), 10);
  }
}

TEST_F(AnnotationHandlerTest, RoomFilterWithNoMatchesReturnsEmptyArray) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-1", 5, "room 5")});
  SeedAnnotations(file_data);

  httplib::Request req;
  req.params.emplace("room", "99");
  httplib::Response res;

  HandleAnnotationList(req, res, project_dir_);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body["annotations"].empty());
}

TEST_F(AnnotationHandlerTest, InvalidRoomParamReturns400) {
  httplib::Request req;
  req.params.emplace("room", "not-a-number");
  httplib::Response res;

  HandleAnnotationList(req, res, project_dir_);

  EXPECT_EQ(res.status, 400);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
}

TEST_F(AnnotationHandlerTest, ListSetsCorsHeader) {
  httplib::Request req;
  httplib::Response res;

  HandleAnnotationList(req, res, project_dir_);

  auto it = res.headers.find("Access-Control-Allow-Origin");
  ASSERT_NE(it, res.headers.end());
  EXPECT_EQ(it->second, "*");
}

// ---------------------------------------------------------------------------
// HandleAnnotationCreate tests
// ---------------------------------------------------------------------------

TEST_F(AnnotationHandlerTest, CreateEmptyProjectPathReturns503) {
  httplib::Request req;
  req.body = MakeAnnotation("ann-1", 1, "hello").dump();
  httplib::Response res;

  HandleAnnotationCreate(req, res, "");

  EXPECT_EQ(res.status, 503);
}

TEST_F(AnnotationHandlerTest, CreateValidAnnotationReturns201) {
  httplib::Request req;
  req.body = MakeAnnotation("ann-1", 10, "first note").dump();
  httplib::Response res;

  HandleAnnotationCreate(req, res, project_dir_);

  EXPECT_EQ(res.status, 201);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "ok");
}

TEST_F(AnnotationHandlerTest, CreateResponseContainsId) {
  httplib::Request req;
  req.body = MakeAnnotation("ann-42", 10, "with id").dump();
  httplib::Response res;

  HandleAnnotationCreate(req, res, project_dir_);

  EXPECT_EQ(res.status, 201);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["id"], "ann-42");
}

TEST_F(AnnotationHandlerTest, CreatePersistsAnnotationToDisk) {
  httplib::Request req;
  req.body = MakeAnnotation("ann-persist", 7, "persisted").dump();
  httplib::Response res;

  HandleAnnotationCreate(req, res, project_dir_);

  ASSERT_EQ(res.status, 201);
  json on_disk = ReadAnnotationsFile(project_dir_);
  ASSERT_TRUE(on_disk.contains("annotations"));
  bool found = false;
  for (const auto& ann : on_disk["annotations"]) {
    if (ann.value("id", "") == "ann-persist") {
      found = true;
      EXPECT_EQ(ann["room_id"].get<int>(), 7);
      EXPECT_EQ(ann["text"], "persisted");
    }
  }
  EXPECT_TRUE(found) << "Created annotation not found on disk";
}

TEST_F(AnnotationHandlerTest, CreateMultipleAnnotationsAccumulates) {
  for (int i = 0; i < 3; ++i) {
    httplib::Request req;
    req.body = MakeAnnotation("ann-" + std::to_string(i), i, "note").dump();
    httplib::Response res;
    HandleAnnotationCreate(req, res, project_dir_);
    ASSERT_EQ(res.status, 201) << "Failed on iteration " << i;
  }

  json on_disk = ReadAnnotationsFile(project_dir_);
  EXPECT_EQ(on_disk["annotations"].size(), 3u);
}

TEST_F(AnnotationHandlerTest, CreateInvalidJsonBodyReturns400) {
  httplib::Request req;
  req.body = "not-json";
  httplib::Response res;

  HandleAnnotationCreate(req, res, project_dir_);

  EXPECT_EQ(res.status, 400);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
}

TEST_F(AnnotationHandlerTest, CreateSetsCorsHeader) {
  httplib::Request req;
  req.body = MakeAnnotation("c1", 1, "cors test").dump();
  httplib::Response res;

  HandleAnnotationCreate(req, res, project_dir_);

  auto it = res.headers.find("Access-Control-Allow-Origin");
  ASSERT_NE(it, res.headers.end());
  EXPECT_EQ(it->second, "*");
}

// ---------------------------------------------------------------------------
// HandleAnnotationUpdate tests
// ---------------------------------------------------------------------------

TEST_F(AnnotationHandlerTest, UpdateEmptyProjectPathReturns503) {
  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ann-1");
  req.body = R"({"text":"updated"})";
  httplib::Response res;

  HandleAnnotationUpdate(req, res, "");

  EXPECT_EQ(res.status, 503);
}

TEST_F(AnnotationHandlerTest, UpdateMissingIdInPathReturns400) {
  // req.matches is empty (no regex match performed).
  httplib::Request req;
  req.body = R"({"text":"updated"})";
  httplib::Response res;

  HandleAnnotationUpdate(req, res, project_dir_);

  EXPECT_EQ(res.status, 400);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
}

TEST_F(AnnotationHandlerTest, UpdateNonexistentAnnotationReturns404) {
  // No annotations file at all — any ID will be "not found".
  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ghost-id");
  req.body = R"({"text":"will not apply"})";
  httplib::Response res;

  HandleAnnotationUpdate(req, res, project_dir_);

  EXPECT_EQ(res.status, 404);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
  EXPECT_NE(body["error"].get<std::string>().find("ghost-id"),
            std::string::npos);
}

TEST_F(AnnotationHandlerTest, UpdateExistingAnnotationReturns200) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-upd", 3, "original text")});
  SeedAnnotations(file_data);

  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ann-upd");
  req.body = R"({"text":"updated text"})";
  httplib::Response res;

  HandleAnnotationUpdate(req, res, project_dir_);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "ok");
  EXPECT_EQ(body["id"], "ann-upd");
}

TEST_F(AnnotationHandlerTest, UpdateMergesFieldsOnDisk) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-merge", 5, "original")});
  SeedAnnotations(file_data);

  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ann-merge");
  req.body = R"({"text":"merged","extra":"new-field"})";
  httplib::Response res;

  HandleAnnotationUpdate(req, res, project_dir_);

  ASSERT_EQ(res.status, 200);
  json on_disk = ReadAnnotationsFile(project_dir_);
  bool found = false;
  for (const auto& ann : on_disk["annotations"]) {
    if (ann.value("id", "") == "ann-merge") {
      found = true;
      EXPECT_EQ(ann["text"], "merged");
      EXPECT_EQ(ann["extra"], "new-field");
      // Pre-existing field not mentioned in update must be preserved.
      EXPECT_EQ(ann["room_id"].get<int>(), 5);
    }
  }
  EXPECT_TRUE(found) << "Updated annotation not found on disk";
}

TEST_F(AnnotationHandlerTest, UpdateOnlyModifiesTargetAnnotation) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-a", 1, "note a"),
                   MakeAnnotation("ann-b", 2, "note b")});
  SeedAnnotations(file_data);

  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ann-a");
  req.body = R"({"text":"modified a"})";
  httplib::Response res;

  HandleAnnotationUpdate(req, res, project_dir_);

  ASSERT_EQ(res.status, 200);
  json on_disk = ReadAnnotationsFile(project_dir_);
  for (const auto& ann : on_disk["annotations"]) {
    if (ann.value("id", "") == "ann-b") {
      EXPECT_EQ(ann["text"], "note b") << "Untargeted annotation was modified";
    }
  }
}

TEST_F(AnnotationHandlerTest, UpdateInvalidJsonBodyReturns400) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-bad", 1, "text")});
  SeedAnnotations(file_data);

  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ann-bad");
  req.body = "not-json{{";
  httplib::Response res;

  HandleAnnotationUpdate(req, res, project_dir_);

  EXPECT_EQ(res.status, 400);
}

TEST_F(AnnotationHandlerTest, UpdateSetsCorsHeader) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-cors", 1, "cors")});
  SeedAnnotations(file_data);

  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ann-cors");
  req.body = R"({"text":"x"})";
  httplib::Response res;

  HandleAnnotationUpdate(req, res, project_dir_);

  auto it = res.headers.find("Access-Control-Allow-Origin");
  ASSERT_NE(it, res.headers.end());
  EXPECT_EQ(it->second, "*");
}

// ---------------------------------------------------------------------------
// HandleAnnotationDelete tests
// ---------------------------------------------------------------------------

TEST_F(AnnotationHandlerTest, DeleteEmptyProjectPathReturns503) {
  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ann-1");
  httplib::Response res;

  HandleAnnotationDelete(req, res, "");

  EXPECT_EQ(res.status, 503);
}

TEST_F(AnnotationHandlerTest, DeleteMissingIdInPathReturns400) {
  httplib::Request req;
  httplib::Response res;

  HandleAnnotationDelete(req, res, project_dir_);

  EXPECT_EQ(res.status, 400);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
}

TEST_F(AnnotationHandlerTest, DeleteNonexistentAnnotationReturns404) {
  // Annotations file exists but does not contain this ID.
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-x", 1, "exists")});
  SeedAnnotations(file_data);

  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/does-not-exist");
  httplib::Response res;

  HandleAnnotationDelete(req, res, project_dir_);

  EXPECT_EQ(res.status, 404);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
  EXPECT_NE(body["error"].get<std::string>().find("does-not-exist"),
            std::string::npos);
}

TEST_F(AnnotationHandlerTest, DeleteExistingAnnotationReturns200) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-del", 2, "to delete")});
  SeedAnnotations(file_data);

  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ann-del");
  httplib::Response res;

  HandleAnnotationDelete(req, res, project_dir_);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "ok");
  EXPECT_EQ(body["id"], "ann-del");
}

TEST_F(AnnotationHandlerTest, DeleteRemovesAnnotationFromDisk) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-gone", 4, "to remove"),
                   MakeAnnotation("ann-stay", 4, "to keep")});
  SeedAnnotations(file_data);

  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ann-gone");
  httplib::Response res;

  HandleAnnotationDelete(req, res, project_dir_);

  ASSERT_EQ(res.status, 200);
  json on_disk = ReadAnnotationsFile(project_dir_);
  ASSERT_EQ(on_disk["annotations"].size(), 1u);
  EXPECT_EQ(on_disk["annotations"][0]["id"], "ann-stay");
}

TEST_F(AnnotationHandlerTest, DeleteOnlyOneAnnotationWhenMultipleExist) {
  json file_data;
  file_data["annotations"] = json::array({MakeAnnotation("ann-1", 1, "one"),
                                          MakeAnnotation("ann-2", 2, "two"),
                                          MakeAnnotation("ann-3", 3, "three")});
  SeedAnnotations(file_data);

  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ann-2");
  httplib::Response res;

  HandleAnnotationDelete(req, res, project_dir_);

  ASSERT_EQ(res.status, 200);
  json on_disk = ReadAnnotationsFile(project_dir_);
  EXPECT_EQ(on_disk["annotations"].size(), 2u);
  for (const auto& ann : on_disk["annotations"]) {
    EXPECT_NE(ann["id"], "ann-2") << "Deleted annotation still present on disk";
  }
}

TEST_F(AnnotationHandlerTest, DeleteIdempotentSecondDeleteReturns404) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-idem", 1, "once")});
  SeedAnnotations(file_data);

  // First delete — must succeed.
  {
    httplib::Request req;
    SetAnnotationPathMatch(req, "/api/v1/annotations/ann-idem");
    httplib::Response res;
    HandleAnnotationDelete(req, res, project_dir_);
    ASSERT_EQ(res.status, 200);
  }

  // Second delete of the same ID — must return 404.
  {
    httplib::Request req;
    SetAnnotationPathMatch(req, "/api/v1/annotations/ann-idem");
    httplib::Response res;
    HandleAnnotationDelete(req, res, project_dir_);
    EXPECT_EQ(res.status, 404);
  }
}

TEST_F(AnnotationHandlerTest, DeleteSetsCorsHeader) {
  json file_data;
  file_data["annotations"] =
      json::array({MakeAnnotation("ann-hdr", 1, "cors")});
  SeedAnnotations(file_data);

  httplib::Request req;
  SetAnnotationPathMatch(req, "/api/v1/annotations/ann-hdr");
  httplib::Response res;

  HandleAnnotationDelete(req, res, project_dir_);

  auto it = res.headers.find("Access-Control-Allow-Origin");
  ASSERT_NE(it, res.headers.end());
  EXPECT_EQ(it->second, "*");
}

// ---------------------------------------------------------------------------
// Round-trip: create -> list -> update -> delete
// ---------------------------------------------------------------------------

TEST_F(AnnotationHandlerTest, FullCrudRoundTrip) {
  // 1. Create
  {
    httplib::Request req;
    req.body = MakeAnnotation("rt-1", 20, "initial text").dump();
    httplib::Response res;
    HandleAnnotationCreate(req, res, project_dir_);
    ASSERT_EQ(res.status, 201);
  }

  // 2. List — should contain the new annotation
  {
    httplib::Request req;
    httplib::Response res;
    HandleAnnotationList(req, res, project_dir_);
    ASSERT_EQ(res.status, 200);
    auto body = json::parse(res.body);
    ASSERT_EQ(body["annotations"].size(), 1u);
    EXPECT_EQ(body["annotations"][0]["id"], "rt-1");
    EXPECT_EQ(body["annotations"][0]["text"], "initial text");
  }

  // 3. Update
  {
    httplib::Request req;
    SetAnnotationPathMatch(req, "/api/v1/annotations/rt-1");
    req.body = R"({"text":"updated text"})";
    httplib::Response res;
    HandleAnnotationUpdate(req, res, project_dir_);
    ASSERT_EQ(res.status, 200);
  }

  // 4. List again — text must reflect update
  {
    httplib::Request req;
    httplib::Response res;
    HandleAnnotationList(req, res, project_dir_);
    ASSERT_EQ(res.status, 200);
    auto body = json::parse(res.body);
    ASSERT_EQ(body["annotations"].size(), 1u);
    EXPECT_EQ(body["annotations"][0]["text"], "updated text");
  }

  // 5. Delete
  {
    httplib::Request req;
    SetAnnotationPathMatch(req, "/api/v1/annotations/rt-1");
    httplib::Response res;
    HandleAnnotationDelete(req, res, project_dir_);
    ASSERT_EQ(res.status, 200);
  }

  // 6. List — must be empty
  {
    httplib::Request req;
    httplib::Response res;
    HandleAnnotationList(req, res, project_dir_);
    ASSERT_EQ(res.status, 200);
    auto body = json::parse(res.body);
    EXPECT_TRUE(body["annotations"].empty());
  }
}

}  // namespace
}  // namespace yaze::cli::api
