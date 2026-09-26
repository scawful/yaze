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

TEST(MessageBankTokenParseTest, LegacyParserDistinguishesBankFromDictionary) {
  EXPECT_EQ(ParseMessageToData("[BANK]"),
            (std::vector<uint8_t>{kBankSwitchCommand}));
  EXPECT_EQ(ParseMessageToData("[D:00]"), (std::vector<uint8_t>{DICTOFF}));
}

TEST(MessageBankTokenParseTest,
     DiagnosticsParserDistinguishesBankFromDictionary) {
  const auto bank = ParseMessageToDataWithDiagnostics("[BANK]");
  const auto dictionary = ParseMessageToDataWithDiagnostics("[D:00]");

  ASSERT_TRUE(bank.ok());
  ASSERT_TRUE(dictionary.ok());
  EXPECT_EQ(bank.bytes, (std::vector<uint8_t>{kBankSwitchCommand}));
  EXPECT_EQ(dictionary.bytes, (std::vector<uint8_t>{DICTOFF}));
}

TEST(MessageBankTokenParseTest, ArgumentCommandWithoutArgumentFailsClosed) {
  const auto parsed = ParseMessageToDataWithDiagnostics("[W]");

  EXPECT_FALSE(parsed.ok());
  EXPECT_TRUE(parsed.bytes.empty());
}

TEST(VanillaMessageWriteTest, CommandArgument80DoesNotSwitchBanks) {
  Rom rom = MakeRom(/*size=*/0x180100, /*fill=*/0xA5);
  MessageData message;
  message.Data = ParseMessageToData("[W:80][BANK]");
  ASSERT_EQ(message.Data,
            (std::vector<uint8_t>{0x6B, 0x80, kBankSwitchCommand}));

  ASSERT_TRUE(WriteAllTextData(&rom, {message}).ok());

  EXPECT_EQ(rom.vector()[kTextData], 0x6B);
  EXPECT_EQ(rom.vector()[kTextData + 1], 0x80);
  EXPECT_EQ(rom.vector()[kTextData + 2], kBankSwitchCommand);
  EXPECT_EQ(rom.vector()[kTextData2], kMessageTerminator);
  EXPECT_EQ(rom.vector()[kTextData2 + 1], 0xFF);
}

TEST(VanillaMessageSavePlanTest, ExposesExactSplitWritesAndCounts) {
  MessageData message;
  message.Data = {0x00, kBankSwitchCommand, 0x01};

  auto plan_or =
      BuildVanillaMessageSavePlan({message}, /*expected_message_count=*/1);
  ASSERT_TRUE(plan_or.ok()) << plan_or.status();
  const VanillaMessageSavePlan& plan = *plan_or;

  EXPECT_EQ(plan.message_count(), 1);
  EXPECT_EQ(plan.bank_switch_count(), 1);
  ASSERT_EQ(plan.writes().size(), 2);
  EXPECT_EQ(plan.writes()[0].start(), kTextData);
  EXPECT_EQ(plan.writes()[0].bytes(),
            (std::vector<uint8_t>{0x00, kBankSwitchCommand}));
  EXPECT_EQ(plan.writes()[1].start(), kTextData2);
  EXPECT_EQ(plan.writes()[1].bytes(),
            (std::vector<uint8_t>{0x01, kMessageTerminator, 0xFF}));
  EXPECT_EQ(plan.write_ranges(),
            (std::vector<std::pair<uint32_t, uint32_t>>{
                {kTextData, kTextData + 2}, {kTextData2, kTextData2 + 3}}));
}

TEST(VanillaMessageSavePlanTest, CommandArgument80DoesNotSwitchBanks) {
  MessageData message;
  message.Data = ParseMessageToData("[W:80][BANK]");
  ASSERT_EQ(message.Data,
            (std::vector<uint8_t>{0x6B, 0x80, kBankSwitchCommand}));

  auto plan_or = BuildVanillaMessageSavePlan({message});
  ASSERT_TRUE(plan_or.ok()) << plan_or.status();
  const VanillaMessageSavePlan& plan = *plan_or;

  EXPECT_EQ(plan.bank_switch_count(), 1);
  ASSERT_EQ(plan.writes().size(), 2);
  EXPECT_EQ(plan.writes()[0].bytes(),
            (std::vector<uint8_t>{0x6B, 0x80, kBankSwitchCommand}));
  EXPECT_EQ(plan.writes()[1].bytes(),
            (std::vector<uint8_t>{kMessageTerminator, 0xFF}));
}

TEST(VanillaMessageSavePlanTest, RejectsDuplicateBankCommand) {
  MessageData message;
  message.Data = {kBankSwitchCommand, 0x00, kBankSwitchCommand};

  auto plan_or = BuildVanillaMessageSavePlan({message});

  ASSERT_FALSE(plan_or.ok());
  EXPECT_EQ(plan_or.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(VanillaMessageSavePlanTest, RejectsMissingBankCommand) {
  MessageData message;
  message.Data = ParseMessageToData("[W:80]");

  auto plan_or = BuildVanillaMessageSavePlan({message});

  ASSERT_FALSE(plan_or.ok());
  EXPECT_EQ(plan_or.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(VanillaMessageSavePlanTest, RejectsBareStreamMarkersInsideMessageData) {
  for (const uint8_t marker : {kMessageTerminator, uint8_t{0xFF}}) {
    MessageData message;
    message.Data = {kBankSwitchCommand, marker};

    auto plan_or = BuildVanillaMessageSavePlan({message});

    ASSERT_FALSE(plan_or.ok()) << "marker=" << static_cast<int>(marker);
    EXPECT_EQ(plan_or.status().code(), absl::StatusCode::kInvalidArgument)
        << "marker=" << static_cast<int>(marker);
  }
}

TEST(VanillaMessageSavePlanTest, RejectsCommandMissingItsArgument) {
  MessageData message;
  message.Data = {kBankSwitchCommand, 0x6B};

  auto plan_or = BuildVanillaMessageSavePlan({message});

  ASSERT_FALSE(plan_or.ok());
  EXPECT_EQ(plan_or.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(VanillaMessageSavePlanTest,
     RejectsVanillaMessageCountsOnBothSidesOfExpected) {
  constexpr size_t kExpectedVanillaMessageCount = 397;
  for (const size_t actual_count : {size_t{396}, size_t{398}}) {
    std::vector<MessageData> messages(actual_count);

    auto plan_or = BuildVanillaMessageSavePlan(
        messages, /*expected_message_count=*/kExpectedVanillaMessageCount);

    ASSERT_FALSE(plan_or.ok()) << "actual_count=" << actual_count;
    EXPECT_EQ(plan_or.status().code(), absl::StatusCode::kFailedPrecondition)
        << "actual_count=" << actual_count;
  }
}

TEST(VanillaMessageSavePlanTest, RejectsPrimaryBankOverflow) {
  constexpr size_t kPrimaryCapacity = kTextDataEnd - kTextData + 1;
  MessageData message;
  message.Data.assign(kPrimaryCapacity, 0x00);
  message.Data.push_back(kBankSwitchCommand);

  auto plan_or = BuildVanillaMessageSavePlan({message});

  ASSERT_FALSE(plan_or.ok());
  EXPECT_EQ(plan_or.status().code(), absl::StatusCode::kResourceExhausted);
}

TEST(VanillaMessageSavePlanTest, RejectsSecondaryBankOverflow) {
  constexpr size_t kSecondaryCapacity = kTextData2End - kTextData2 + 1;
  MessageData message;
  message.Data.assign(kSecondaryCapacity, 0x00);
  message.Data.front() = kBankSwitchCommand;

  auto plan_or = BuildVanillaMessageSavePlan({message});

  ASSERT_FALSE(plan_or.ok());
  EXPECT_EQ(plan_or.status().code(), absl::StatusCode::kResourceExhausted);
}

TEST(VanillaMessageSavePlanTest, ApplyRollsBackWhenOuterFenceRejectsLaterRun) {
  Rom rom = MakeRom(/*size=*/0x180100, /*fill=*/0xA5);
  MessageData message;
  message.Data = {0x00, kBankSwitchCommand, 0x01};
  auto plan_or = BuildVanillaMessageSavePlan({message});
  ASSERT_TRUE(plan_or.ok()) << plan_or.status();
  const auto before = rom.vector();

  yaze::rom::WriteFence outer;
  ASSERT_TRUE(outer.Allow(kTextData, kTextData + 2, "primary only").ok());
  yaze::rom::ScopedWriteFence scope(&rom, &outer);

  const auto status = ApplyVanillaMessageSavePlan(&rom, *plan_or);

  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(rom.vector(), before);
}

TEST(MessageEditorSavePlanTest,
     LoadedExpandedBankStaysByteIdenticalAfterVanillaEdit) {
  Rom rom = MakeRom(/*size=*/0x180100, /*fill=*/0xA5);
  MessageEditor editor(&rom, MakeDependencies(&rom));
  MessageEditorSaveTestPeer::SetExpandedMessages(editor, {"EXPANDED"},
                                                 /*dirty=*/false);
  MessageEditorSaveTestPeer::SetVanillaMessages(
      editor, {{0x00, kBankSwitchCommand}}, /*dirty=*/true);

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
  MessageEditorSaveTestPeer::SetVanillaMessages(
      editor, {{0x00, kBankSwitchCommand}}, /*dirty=*/true);
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
  const auto encoded = ParseMessageToData("[W:80][BANK]");
  ASSERT_EQ(encoded, (std::vector<uint8_t>{0x6B, 0x80, kBankSwitchCommand}));
  MessageEditorSaveTestPeer::SetVanillaMessages(editor, {encoded},
                                                /*dirty=*/true);

  auto ranges_or = MessageEditorSaveTestPeer::PlannedRanges(editor);
  ASSERT_TRUE(ranges_or.ok()) << ranges_or.status();
  EXPECT_EQ(ranges_or.value(),
            (std::vector<std::pair<uint32_t, uint32_t>>{
                {kTextData, kTextData + 3}, {kTextData2, kTextData2 + 2}}));

  ASSERT_TRUE(editor.Save().ok());
  EXPECT_EQ(rom.vector()[kTextData], 0x6B);
  EXPECT_EQ(rom.vector()[kTextData + 1], 0x80);
  EXPECT_EQ(rom.vector()[kTextData + 2], kBankSwitchCommand);
  EXPECT_EQ(rom.vector()[kTextData2], kMessageTerminator);
  EXPECT_EQ(rom.vector()[kTextData2 + 1], 0xFF);
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

TEST(VanillaMessageSavePlanTest, ChangedRangesSkipBytesAlreadyInRom) {
  MessageData message;
  message.Data = {0x00, 0x01, kBankSwitchCommand, 0x02};
  auto plan_or = BuildVanillaMessageSavePlan({message});
  ASSERT_TRUE(plan_or.ok()) << plan_or.status();

  std::vector<uint8_t> rom(0x100000, 0x00);
  // Primary: 00 01 80 (00 is already there). Secondary: 02 7F FF.
  rom[kTextData + 1] = 0x01;
  rom[kTextData2] = 0x02;
  rom[kTextData2 + 1] = 0x55;  // differs from the planned terminator
  rom[kTextData2 + 2] = 0xFF;

  EXPECT_EQ(
      plan_or->ChangedRanges(rom.data(), rom.size()),
      (std::vector<std::pair<uint32_t, uint32_t>>{
          {kTextData + 2, kTextData + 3}, {kTextData2 + 1, kTextData2 + 2}}));
  // Bytes past the ROM count as changed.
  EXPECT_EQ(plan_or->ChangedRanges(rom.data(), kTextData2 + 1).back(),
            (std::pair<uint32_t, uint32_t>{kTextData2 + 1, kTextData2 + 3}));
}

TEST(MessageDictionaryTest, ReadDictionaryEntryBytesFollowsPointerTable) {
  std::vector<uint8_t> rom(0x100000, 0x00);
  // Entry i starts at $0E:C800 + 2i and is 2 bytes long; the extra pointer
  // after the last entry ends it.
  for (int index = 0; index <= kNumDictionaryEntries; ++index) {
    const uint16_t pointer = 0xC800 + index * 2;
    rom[kPointersDictionaries + index * 2] = pointer & 0xFF;
    rom[kPointersDictionaries + index * 2 + 1] = pointer >> 8;
  }
  rom[0x74800] = 0x11;
  rom[0x74801] = 0x12;
  rom[0x74802] = 0x21;

  const auto entries = ReadDictionaryEntryBytes(rom.data(), rom.size());
  ASSERT_EQ(entries.size(), static_cast<size_t>(kNumDictionaryEntries));
  EXPECT_EQ(entries[0], (std::vector<uint8_t>{0x11, 0x12}));
  EXPECT_EQ(entries[1].front(), 0x21);
  EXPECT_TRUE(
      ReadDictionaryEntryBytes(rom.data(), kPointersDictionaries + 4).empty());
}

TEST(MessageDictionaryTest, CompressionUsesFewestBytes) {
  // Longest-first matching takes "abc" and leaves "d", "e" (3 bytes); the
  // shortest encoding is "ab" + "cde" (2 bytes).
  const std::vector<std::vector<uint8_t>> dictionary = {
      {0x00, 0x01, 0x02},  // abc
      {0x02, 0x03, 0x04},  // cde
      {0x00, 0x01},        // ab
  };
  EXPECT_EQ(
      CompressMessageWithDictionary({0x00, 0x01, 0x02, 0x03, 0x04}, dictionary),
      (std::vector<uint8_t>{DICTOFF + 2, DICTOFF + 1}));
}

TEST(MessageDictionaryTest, CompressionLeavesCommandsAndTokensAlone) {
  const std::vector<std::vector<uint8_t>> dictionary = {{0x00, 0x01}};
  // [W:00] takes 0x00 as its argument, so only the later "ab" is a word.
  // An existing token and a command without an argument stay as they are.
  const std::vector<uint8_t> data = {0x6B, 0x00,        0x01, 0x00,
                                     0x01, DICTOFF + 5, 0x74, 0x00};
  EXPECT_EQ(CompressMessageWithDictionary(data, dictionary),
            (std::vector<uint8_t>{0x6B, 0x00, 0x01, DICTOFF + 0, DICTOFF + 5,
                                  0x74, 0x00}));
}

TEST(MessageDictionaryTest, CompressedMessageDecodesToSameCharacters) {
  const std::vector<std::vector<uint8_t>> dictionary = {
      {0x1D, 0x21, 0x1E, 0x59}, {0x21, 0x1E}, {0x59, 0x59}};
  const std::vector<uint8_t> data = {0x1D, 0x21, 0x1E, 0x59, 0x59, 0x59,
                                     0x75, 0x21, 0x1E, 0x7A, 0x59, 0x59};
  const auto compressed = CompressMessageWithDictionary(data, dictionary);
  ASSERT_LT(compressed.size(), data.size());

  std::vector<uint8_t> decoded;
  for (size_t index = 0; index < compressed.size(); ++index) {
    const uint8_t value = compressed[index];
    if (value >= DICTOFF) {
      const auto& word = dictionary[value - DICTOFF];
      decoded.insert(decoded.end(), word.begin(), word.end());
      continue;
    }
    decoded.push_back(value);
    if (value == 0x7A) {  // [S:xx] argument
      decoded.push_back(compressed[++index]);
    }
  }
  EXPECT_EQ(decoded, data);
}

}  // namespace yaze::editor
