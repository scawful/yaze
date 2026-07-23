#include "app/editor/message/message_data.h"
#include "app/editor/message/message_editor.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/project.h"
#include "rom/rom.h"
#include "rom/write_fence.h"

namespace yaze::editor {

class MessageEditorSaveTestPeer {
 public:
  static void SetVanillaMessages(MessageEditor& editor,
                                 std::vector<std::vector<uint8_t>> data,
                                 bool dirty) {
    editor.list_of_texts_.clear();
    for (size_t index = 0; index < data.size(); ++index) {
      MessageData message;
      message.ID = static_cast<int>(index);
      message.Data = std::move(data[index]);
      editor.list_of_texts_.push_back(std::move(message));
    }
    editor.dirty_state_.vanilla_messages = dirty;
    if (dirty) {
      editor.rom_->set_dirty(true);
    }
  }

  static void SetExpandedMessages(MessageEditor& editor,
                                  const std::vector<std::string>& texts,
                                  bool dirty) {
    editor.expanded_messages_.clear();
    for (size_t index = 0; index < texts.size(); ++index) {
      MessageData message;
      message.ID = static_cast<int>(index);
      message.RawString = texts[index];
      message.ContentsParsed = texts[index];
      message.Data = ParseMessageToData(texts[index]);
      editor.expanded_messages_.push_back(std::move(message));
    }
    editor.dirty_state_.expanded_messages = dirty;
    if (dirty) {
      editor.rom_->set_dirty(true);
    }
  }

  static void SetFontWidthDirty(MessageEditor& editor, uint8_t value) {
    editor.message_preview_.width_array[0] = value;
    editor.MarkDomainDirty(MessageEditor::SaveDomain::kFontWidths);
  }

  static bool VanillaDirty(const MessageEditor& editor) {
    return editor.dirty_state_.vanilla_messages;
  }

  static bool ExpandedDirty(const MessageEditor& editor) {
    return editor.dirty_state_.expanded_messages;
  }

  static bool FontWidthsDirty(const MessageEditor& editor) {
    return editor.dirty_state_.font_widths;
  }

  static void SetExpandedAddress(MessageEditor& editor, int address) {
    editor.expanded_messages_.front().Address = address;
  }

  static int ExpandedAddress(const MessageEditor& editor) {
    return editor.expanded_messages_.front().Address;
  }

  static void SelectExpandedMessage(MessageEditor& editor, size_t index) {
    ASSERT_LT(index, editor.expanded_messages_.size());
    editor.current_message_index_ = static_cast<int>(index);
    editor.current_message_is_expanded_ = true;
    editor.current_message_ = editor.expanded_messages_[index];
  }

  static int CurrentMessageAddress(const MessageEditor& editor) {
    return editor.current_message_.Address;
  }

  static void EditCurrentMessage(MessageEditor& editor,
                                 const std::string& text) {
    editor.UpdateCurrentMessageFromText(text);
  }

  static absl::StatusOr<std::vector<std::pair<uint32_t, uint32_t>>>
  PlannedRanges(const MessageEditor& editor) {
    auto plan_or = editor.BuildSavePlan(/*include_font_widths=*/true,
                                        /*include_vanilla_messages=*/true,
                                        /*include_expanded_messages=*/true);
    if (!plan_or.ok()) {
      return plan_or.status();
    }
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    for (const auto& write : plan_or->writes) {
      ranges.emplace_back(write.start, write.end());
    }
    return ranges;
  }
};

namespace {

Rom MakeRom(size_t size, uint8_t fill = 0) {
  Rom rom;
  std::vector<uint8_t> data(size, fill);
  auto status = rom.LoadFromData(data);
  EXPECT_TRUE(status.ok()) << status.message();
  return rom;
}

EditorDependencies MakeDependencies(Rom* rom,
                                    project::YazeProject* project = nullptr) {
  EditorDependencies dependencies;
  dependencies.rom = rom;
  dependencies.project = project;
  return dependencies;
}

}  // namespace

TEST(ExpandedMessageWriteTest, WritesExpectedBytesToRegion) {
  Rom rom = MakeRom(256);

  const int start = 100;
  const int end = 120;  // inclusive

  ASSERT_TRUE(WriteExpandedTextData(&rom, start, end, {"A", "B"}).ok());

  std::vector<uint8_t> expected;
  auto a = ParseMessageToData("A");
  auto b = ParseMessageToData("B");
  expected.insert(expected.end(), a.begin(), a.end());
  expected.push_back(kMessageTerminator);
  expected.insert(expected.end(), b.begin(), b.end());
  expected.push_back(kMessageTerminator);
  expected.push_back(0xFF);

  auto bytes_or = rom.ReadByteVector(start, expected.size());
  ASSERT_TRUE(bytes_or.ok()) << bytes_or.status().message();
  EXPECT_EQ(bytes_or.value(), expected);
}

TEST(ExpandedMessageWriteTest, IsWriteFenceAware) {
  Rom rom = MakeRom(256);

  // Outer fence disallows the expanded region, so the writer must be blocked
  // if it routes through Rom::Write* (and doesn't bypass via raw pointers).
  yaze::rom::WriteFence outer;
  ASSERT_TRUE(outer.Allow(/*start=*/0, /*end=*/10, "deny").ok());
  yaze::rom::ScopedWriteFence scope(&rom, &outer);

  auto status = WriteExpandedTextData(&rom, /*start=*/100, /*end=*/120, {"A"});
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied);
}

TEST(ExpandedMessageWriteTest, InvalidTextFailsBeforeRomMutation) {
  Rom rom = MakeRom(256);
  const auto before = rom.vector();

  auto status =
      WriteExpandedTextData(&rom, /*start=*/100, /*end=*/120, {"A[UNKNOWN]B"});
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_EQ(rom.vector(), before);
}

TEST(ExpandedMessageWriteTest, BankCommandFailsBeforeRomMutation) {
  Rom rom = MakeRom(256);
  const auto before = rom.vector();

  auto status =
      WriteExpandedTextData(&rom, /*start=*/100, /*end=*/120, {"A[BANK]B"});
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_EQ(rom.vector(), before);
}

TEST(ExpandedMessageWriteTest, BankByteIsAllowedAsCommandArgument) {
  Rom rom = MakeRom(256);

  auto status =
      WriteExpandedTextData(&rom, /*start=*/100, /*end=*/120, {"[W:80]A"});
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(rom.vector()[100], 0x6B);
  EXPECT_EQ(rom.vector()[101], 0x80);
}

TEST(ExpandedMessageWriteTest, LegacyWriterPreflightsBankCommand) {
  std::vector<uint8_t> data(256, 0x5A);
  const auto before = data;

  auto status = WriteExpandedTextData(data.data(), /*start=*/100, /*end=*/120,
                                      {"A", "B[BANK]"});
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_EQ(data, before);
}

TEST(VanillaMessageWriteTest, CommandArgument80DoesNotSwitchBanks) {
  Rom rom = MakeRom(/*size=*/0x180100, /*fill=*/0xA5);
  MessageData message;
  message.Data = ParseMessageToData("[W:80]");
  ASSERT_EQ(message.Data, (std::vector<uint8_t>{0x6B, 0x80}));

  ASSERT_TRUE(WriteAllTextData(&rom, {message}).ok());

  EXPECT_EQ(rom.vector()[kTextData], 0x6B);
  EXPECT_EQ(rom.vector()[kTextData + 1], 0x80);
  EXPECT_EQ(rom.vector()[kTextData + 2], kMessageTerminator);
  EXPECT_EQ(rom.vector()[kTextData + 3], 0xFF);
  EXPECT_EQ(rom.vector()[kTextData2], 0xA5);
}

TEST(MessageEditorSavePlanTest,
     LoadedExpandedBankStaysByteIdenticalAfterVanillaEdit) {
  Rom rom = MakeRom(/*size=*/0x180100, /*fill=*/0xA5);
  MessageEditor editor(&rom, MakeDependencies(&rom));
  MessageEditorSaveTestPeer::SetExpandedMessages(editor, {"EXPANDED"},
                                                 /*dirty=*/false);
  MessageEditorSaveTestPeer::SetVanillaMessages(editor, {{0x00}},
                                                /*dirty=*/true);

  const auto expanded_before =
      rom.ReadByteVector(kExpandedTextDataDefault, /*length=*/32).value();
  ASSERT_TRUE(editor.Save().ok());
  const auto expanded_after =
      rom.ReadByteVector(kExpandedTextDataDefault, /*length=*/32).value();

  EXPECT_EQ(expanded_after, expanded_before);
  EXPECT_FALSE(MessageEditorSaveTestPeer::VanillaDirty(editor));
  EXPECT_FALSE(MessageEditorSaveTestPeer::ExpandedDirty(editor));
}

TEST(MessageEditorSavePlanTest,
     DirtyOracleExpandedBankFailsBeforeOtherWritesAndStaysRetryable) {
  Rom rom = MakeRom(/*size=*/0x180100, /*fill=*/0xA5);
  project::YazeProject yaze_project;
  yaze_project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  ASSERT_TRUE(yaze_project.hack_manifest
                  .LoadFromString(R"json(
{
  "manifest_version": 3,
  "hack_name": "Oracle of Secrets",
  "editor_managed_regions": {
    "regions": [
      {"start":"0x2F8000", "end":"0x2FFFFF"}
    ]
  },
  "owned_banks": {
    "banks": [
      {"bank":"0x2F", "bank_start":"0x2F8000",
       "bank_end":"0x2FFFFF", "ownership":"asm_owned",
       "ownership_note":"Core/message.asm"}
    ]
  }
}
)json")
                  .ok());
  EXPECT_TRUE(yaze_project.hack_manifest
                  .AnalyzePcWriteRanges({{kExpandedTextDataDefault,
                                          kExpandedTextDataDefault + 1}})
                  .empty());

  MessageEditor editor(&rom, MakeDependencies(&rom, &yaze_project));
  MessageEditorSaveTestPeer::SetVanillaMessages(editor, {{0x00}},
                                                /*dirty=*/true);
  MessageEditorSaveTestPeer::SetExpandedMessages(editor, {"DRAFT"},
                                                 /*dirty=*/true);
  MessageEditorSaveTestPeer::SetFontWidthDirty(editor, 0x0C);
  const auto before = rom.vector();

  const auto status = editor.Save();

  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_NE(std::string(status.message()).find("Core/message.asm"),
            std::string::npos);
  EXPECT_NE(std::string(status.message()).find("rebuild Oracle of Secrets"),
            std::string::npos);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(MessageEditorSaveTestPeer::FontWidthsDirty(editor));
  EXPECT_TRUE(MessageEditorSaveTestPeer::VanillaDirty(editor));
  EXPECT_TRUE(MessageEditorSaveTestPeer::ExpandedDirty(editor));
}

TEST(MessageEditorSavePlanTest,
     VanillaHookOverlapFailsBeforeFontOrMessageMutation) {
  constexpr uint32_t kProtectedPc = 0x076E75;
  Rom rom = MakeRom(/*size=*/0x180100, /*fill=*/0xA5);
  project::YazeProject yaze_project;
  yaze_project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  ASSERT_TRUE(yaze_project.hack_manifest
                  .LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Protected vanilla message hook",
  "protected_regions": {
    "regions": [
      {"start":"0x0EEE75", "end":"0x0EEE7D",
       "module":"Core/message.asm"}
    ]
  }
}
)json")
                  .ok());

  std::vector<uint8_t> vanilla_data = {kBankSwitchCommand};
  vanilla_data.resize(
      1 + (kProtectedPc - static_cast<uint32_t>(kTextData2)) + 1, 0x00);
  MessageEditor editor(&rom, MakeDependencies(&rom, &yaze_project));
  MessageEditorSaveTestPeer::SetVanillaMessages(
      editor, {std::move(vanilla_data)}, /*dirty=*/true);
  MessageEditorSaveTestPeer::SetFontWidthDirty(editor, 0x0C);
  const auto before = rom.vector();

  const auto status = editor.Save();

  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(MessageEditorSaveTestPeer::FontWidthsDirty(editor));
  EXPECT_TRUE(MessageEditorSaveTestPeer::VanillaDirty(editor));
}

TEST(MessageEditorSavePlanTest, AllowedVanillaPlanUsesExactHalfOpenRanges) {
  Rom rom = MakeRom(/*size=*/0x180100, /*fill=*/0xA5);
  project::YazeProject yaze_project;
  yaze_project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  ASSERT_TRUE(yaze_project.hack_manifest
                  .LoadFromString(R"json(
{
  "manifest_version": 3,
  "hack_name": "Exact message ranges",
  "editor_managed_regions": {
    "regions": [
      {"start":"0x0ECADF", "end":"0x0ECB43"},
      {"start":"0x0EDF40", "end":"0x0EDF43"},
      {"start":"0x1C8000", "end":"0x1C8002"}
    ]
  },
  "owned_banks": {
    "banks": [
      {"bank":"0x0E", "bank_start":"0x0E8000",
       "bank_end":"0x0EFFFF", "ownership":"asm_owned"},
      {"bank":"0x1C", "bank_start":"0x1C8000",
       "bank_end":"0x1CFFFF", "ownership":"asm_owned"}
    ]
  }
}
)json")
                  .ok());

  MessageEditor editor(&rom, MakeDependencies(&rom, &yaze_project));
  MessageEditorSaveTestPeer::SetVanillaMessages(
      editor, {{0x00, kBankSwitchCommand, 0x01}}, /*dirty=*/true);
  MessageEditorSaveTestPeer::SetFontWidthDirty(editor, 0x0C);

  auto ranges_or = MessageEditorSaveTestPeer::PlannedRanges(editor);
  ASSERT_TRUE(ranges_or.ok()) << ranges_or.status();
  const std::vector<std::pair<uint32_t, uint32_t>> expected = {
      {kCharactersWidth, kCharactersWidth + kWidthArraySize},
      {kTextData, kTextData + 2},
      {kTextData2, kTextData2 + 3}};
  EXPECT_EQ(ranges_or.value(), expected);

  ASSERT_TRUE(editor.Save().ok());
  EXPECT_EQ(rom.vector()[kTextData], 0x00);
  EXPECT_EQ(rom.vector()[kTextData + 1], kBankSwitchCommand);
  EXPECT_EQ(rom.vector()[kTextData2], 0x01);
  EXPECT_EQ(rom.vector()[kTextData2 + 1], kMessageTerminator);
  EXPECT_EQ(rom.vector()[kTextData2 + 2], 0xFF);
}

TEST(MessageEditorSavePlanTest,
     CommandArgument80StaysPrimaryWithExactHalfOpenRange) {
  Rom rom = MakeRom(/*size=*/0x180100, /*fill=*/0xA5);
  MessageEditor editor(&rom, MakeDependencies(&rom));
  const auto encoded = ParseMessageToData("[W:80]");
  ASSERT_EQ(encoded, (std::vector<uint8_t>{0x6B, 0x80}));
  MessageEditorSaveTestPeer::SetVanillaMessages(editor, {encoded},
                                                /*dirty=*/true);

  auto ranges_or = MessageEditorSaveTestPeer::PlannedRanges(editor);
  ASSERT_TRUE(ranges_or.ok()) << ranges_or.status();
  EXPECT_EQ(
      ranges_or.value(),
      (std::vector<std::pair<uint32_t, uint32_t>>{{kTextData, kTextData + 4}}));

  ASSERT_TRUE(editor.Save().ok());
  EXPECT_EQ(rom.vector()[kTextData], 0x6B);
  EXPECT_EQ(rom.vector()[kTextData + 1], 0x80);
  EXPECT_EQ(rom.vector()[kTextData + 2], kMessageTerminator);
  EXPECT_EQ(rom.vector()[kTextData + 3], 0xFF);
  EXPECT_EQ(rom.vector()[kTextData2], 0xA5);
}

TEST(MessageEditorSavePlanTest, TruncatedExpandedRegionFailsBeforeMutation) {
  Rom rom = MakeRom(/*size=*/kExpandedTextDataDefault + 16, /*fill=*/0xA5);
  MessageEditor editor(&rom, MakeDependencies(&rom));
  MessageEditorSaveTestPeer::SetExpandedMessages(editor, {"DRAFT"},
                                                 /*dirty=*/true);
  const auto before = rom.vector();

  const auto status = editor.Save();

  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(MessageEditorSaveTestPeer::ExpandedDirty(editor));
}

TEST(MessageEditorSavePlanTest,
     RepackedSelectedExpandedAddressSurvivesNextEdit) {
  Rom rom = MakeRom(/*size=*/0x180100, /*fill=*/0xA5);
  MessageEditor editor(&rom, MakeDependencies(&rom));
  MessageEditorSaveTestPeer::SetExpandedMessages(editor, {"DRAFT"},
                                                 /*dirty=*/true);
  MessageEditorSaveTestPeer::SetExpandedAddress(editor, 0x1234);
  MessageEditorSaveTestPeer::SelectExpandedMessage(editor, 0);

  ASSERT_TRUE(editor.Save().ok());
  EXPECT_EQ(MessageEditorSaveTestPeer::ExpandedAddress(editor),
            kExpandedTextDataDefault);
  EXPECT_EQ(MessageEditorSaveTestPeer::CurrentMessageAddress(editor),
            kExpandedTextDataDefault);

  MessageEditorSaveTestPeer::EditCurrentMessage(editor, "UPDATED");
  EXPECT_EQ(MessageEditorSaveTestPeer::ExpandedAddress(editor),
            kExpandedTextDataDefault);
  EXPECT_TRUE(MessageEditorSaveTestPeer::ExpandedDirty(editor));
}

TEST(MessageEditorSavePlanTest,
     OuterSaveRollbackRestoresExpandedAddressAndDirtyState) {
  Rom rom = MakeRom(/*size=*/0x180100, /*fill=*/0xA5);
  MessageEditor editor(&rom, MakeDependencies(&rom));
  MessageEditorSaveTestPeer::SetExpandedMessages(editor, {"DRAFT"},
                                                 /*dirty=*/true);
  constexpr int kOriginalAddress = 0x1234;
  MessageEditorSaveTestPeer::SetExpandedAddress(editor, kOriginalAddress);
  MessageEditorSaveTestPeer::SelectExpandedMessage(editor, 0);

  ASSERT_TRUE(editor.BeginSaveTransaction().ok());
  ASSERT_TRUE(editor.Save().ok());
  EXPECT_EQ(MessageEditorSaveTestPeer::ExpandedAddress(editor),
            kExpandedTextDataDefault);
  EXPECT_EQ(MessageEditorSaveTestPeer::CurrentMessageAddress(editor),
            kExpandedTextDataDefault);

  editor.RollbackSaveTransaction();
  EXPECT_EQ(MessageEditorSaveTestPeer::ExpandedAddress(editor),
            kOriginalAddress);
  EXPECT_EQ(MessageEditorSaveTestPeer::CurrentMessageAddress(editor),
            kOriginalAddress);
  EXPECT_TRUE(MessageEditorSaveTestPeer::ExpandedDirty(editor));
}

}  // namespace yaze::editor
