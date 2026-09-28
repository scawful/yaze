#include "app/editor/dungeon/dungeon_room_document_file.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "app/editor/dungeon/dungeon_room_transfer.h"
#include "app/editor/dungeon/inspectors/dungeon_room_transfer_editor.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

class DungeonRoomDocumentFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    directory_ =
        std::filesystem::temp_directory_path() /
        ("yaze-room-file-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
    DungeonRoomDocument document;
    document.source_room_id = 5;
    document.contents.objects.emplace_back(0x21, 8, 8, 0, 0);
    auto json = SerializeDungeonRoomDocument(document);
    ASSERT_TRUE(json.ok()) << json.status();
    json_ = *json;
  }
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
  }
  std::string Path(const char* name = "room.json") const {
    return (directory_ / name).string();
  }
  void Put(const std::string& path, const std::string& contents) {
    std::ofstream out(path, std::ios::binary);
    out << contents;
    ASSERT_TRUE(out.good());
  }
  std::string Raw(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()};
  }
  std::filesystem::path directory_;
  std::string json_;
};

TEST_F(DungeonRoomDocumentFileTest, ExportReopensWithExactAuthoredData) {
  ASSERT_TRUE(WriteDungeonRoomDocumentFile(Path(), json_).ok());
  auto loaded = ReadDungeonRoomDocumentFile(Path());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(*loaded, json_);
  EXPECT_EQ(ParseDungeonRoomDocument(*loaded)->source_room_id, 5);
}

TEST_F(DungeonRoomDocumentFileTest, ReplacesExistingValidTemplate) {
  ASSERT_TRUE(WriteDungeonRoomDocumentFile(Path(), json_).ok());
  auto document = ParseDungeonRoomDocument(json_);
  ASSERT_TRUE(document.ok());
  document->source_room_id = 9;
  auto replacement = SerializeDungeonRoomDocument(*document);
  ASSERT_TRUE(replacement.ok());
  ASSERT_TRUE(WriteDungeonRoomDocumentFile(Path(), *replacement).ok());
  EXPECT_EQ(Raw(Path()), *replacement);
  EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory_),
                          std::filesystem::directory_iterator()),
            1);
}

TEST_F(DungeonRoomDocumentFileTest,
       RejectsRomExtensionAndUnrelatedJsonUnchanged) {
  Put(Path("source.sfc"), "ROM sentinel");
  EXPECT_FALSE(WriteDungeonRoomDocumentFile(Path("source.sfc"), json_).ok());
  EXPECT_EQ(Raw(Path("source.sfc")), "ROM sentinel");
  Put(Path(), "{\"unrelated\":true}");
  EXPECT_FALSE(WriteDungeonRoomDocumentFile(Path(), json_).ok());
  EXPECT_EQ(Raw(Path()), "{\"unrelated\":true}");
}

TEST_F(DungeonRoomDocumentFileTest, InvalidExportDoesNotCreateOrReplaceFile) {
  EXPECT_FALSE(WriteDungeonRoomDocumentFile(Path(), "{}").ok());
  EXPECT_FALSE(std::filesystem::exists(Path()));
  Put(Path(), json_);
  EXPECT_FALSE(WriteDungeonRoomDocumentFile(Path(), "{}").ok());
  EXPECT_EQ(Raw(Path()), json_);
}

TEST_F(DungeonRoomDocumentFileTest, RejectsOversizedAndMalformedImports) {
  Put(Path(), std::string(kMaxDungeonRoomDocumentBytes + 1, ' '));
  EXPECT_EQ(ReadDungeonRoomDocumentFile(Path()).status().code(),
            absl::StatusCode::kResourceExhausted);
  Put(Path(), "{}");
  EXPECT_FALSE(ReadDungeonRoomDocumentFile(Path()).ok());
  EXPECT_FALSE(ReadDungeonRoomDocumentFile(Path("missing.json")).ok());
  EXPECT_FALSE(ReadDungeonRoomDocumentFile(directory_.string()).ok());
}

TEST_F(DungeonRoomDocumentFileTest, RejectsDirectoryAndMissingParentForExport) {
  std::filesystem::create_directory(Path("directory.json"));
  EXPECT_FALSE(
      WriteDungeonRoomDocumentFile(Path("directory.json"), json_).ok());
  EXPECT_FALSE(
      WriteDungeonRoomDocumentFile(Path("missing/room.json"), json_).ok());
}

#ifndef _WIN32
TEST_F(DungeonRoomDocumentFileTest, RefusesSymlinkWithoutChangingTarget) {
  Put(Path("target.json"), json_);
  std::filesystem::create_symlink(Path("target.json"), Path());
  EXPECT_FALSE(WriteDungeonRoomDocumentFile(Path(), json_).ok());
  EXPECT_TRUE(std::filesystem::is_symlink(Path()));
  EXPECT_EQ(Raw(Path("target.json")), json_);
}
#endif

TEST_F(DungeonRoomDocumentFileTest,
       SuccessfulLoadInvalidatesPreviewWithoutApplying) {
  Put(Path(), json_);
  DungeonRoomTransferEditorState state;
  state.preview = std::make_shared<DungeonRoomTransferPlan>();
  state.source_room_id = 7;
  state.domains = kTransferItems;
  state.error = "old error";
  LoadDungeonRoomTransferFile(state, Path());
  EXPECT_TRUE(state.import_json);
  EXPECT_EQ(state.json, json_);
  EXPECT_FALSE(state.preview);
  EXPECT_EQ(state.domains, kTransferItems);
  EXPECT_EQ(state.source_room_id, 7);
  EXPECT_TRUE(state.error.empty());
  EXPECT_FALSE(state.status.empty());
}

TEST_F(DungeonRoomDocumentFileTest,
       FailedOrCancelledLoadPreservesExistingForm) {
  DungeonRoomTransferEditorState state;
  state.json = json_;
  state.preview = std::make_shared<DungeonRoomTransferPlan>();
  auto preview = state.preview;
  state.status = "prior result";
  LoadDungeonRoomTransferFile(state, "");
  EXPECT_EQ(state.status, "prior result");
  EXPECT_TRUE(state.error.empty());
  LoadDungeonRoomTransferFile(state, Path("missing.json"));
  EXPECT_EQ(state.json, json_);
  EXPECT_EQ(state.preview, preview);
  EXPECT_FALSE(state.import_json);
  EXPECT_FALSE(state.error.empty());
}

}  // namespace
}  // namespace yaze::editor
