#include "app/editor/message/message_data.h"
#include "app/editor/message/message_editor.h"
#include "app/editor/message/message_source_sync.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

#include "imgui/imgui.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "absl/strings/str_format.h"
#include "app/editor/session_types.h"
#include "core/project.h"
#include "rom/rom.h"
#include "rom/snes.h"

namespace yaze::editor {

class MessageEditorPublishGuardTestPeer {
 public:
  static absl::Status LoadCanonical(MessageEditor& editor) {
    return editor.LoadCanonicalExpandedSource();
  }

  static size_t ExpandedCount(const MessageEditor& editor) {
    return editor.expanded_messages_.size();
  }

  static int ExpandedBaseId(const MessageEditor& editor) {
    return editor.expanded_message_base_id_;
  }

  static const std::string& BundleStatus(const MessageEditor& editor) {
    return editor.message_bundle_status_;
  }

  static bool BundleStatusError(const MessageEditor& editor) {
    return editor.message_bundle_status_error_;
  }

  static const std::string& LoadedSourceSha(const MessageEditor& editor) {
    return editor.source_loaded_canonical_sha_;
  }

  static void Import(MessageEditor& editor, const std::string& path) {
    editor.ImportMessageBundleFromFile(path);
  }

  static void Preview(MessageEditor& editor) { editor.PreviewMessageSource(); }

  static void Publish(MessageEditor& editor) { editor.PublishMessageSource(); }

  static int ExpandedId(const MessageEditor& editor, int index) {
    return editor.expanded_messages_[index].ID;
  }

  static std::pair<bool, bool> DrawPublishButtons(MessageEditor& editor) {
    const auto state = editor.DrawMessageSourcePublishButtons();
    return {state.preview_disabled, state.publish_disabled};
  }

  static const std::string& Status(const MessageEditor& editor) {
    return editor.source_sync_status_;
  }

  static bool Ready(const MessageEditor& editor) {
    return editor.source_sync_ready_;
  }

  static bool Changed(const MessageEditor& editor) {
    return editor.source_sync_changed_;
  }

  static bool StatusError(const MessageEditor& editor) {
    return editor.source_sync_status_error_;
  }

  static void SetExpandedText(MessageEditor& editor, int index,
                              const std::string& text) {
    editor.expanded_messages_[index].RawString = text;
    editor.expanded_messages_[index].ContentsParsed = text;
    editor.expanded_messages_[index].Data = ParseMessageToData(text);
  }

  static std::string ExpandedText(const MessageEditor& editor, int index) {
    return editor.expanded_messages_[index].RawString;
  }

  static void MarkExpandedDirty(MessageEditor& editor) {
    editor.MarkDomainDirty(MessageEditor::SaveDomain::kExpandedMessages);
  }

  static bool ExpandedDirty(const MessageEditor& editor) {
    return editor.dirty_state_.expanded_messages;
  }

  static bool VanillaDirty(const MessageEditor& editor) {
    return editor.dirty_state_.vanilla_messages;
  }

  static bool FontDirty(const MessageEditor& editor) {
    return editor.dirty_state_.font_widths;
  }

  static void SetVanillaDirty(MessageEditor& editor) {
    MessageData message;
    message.ID = 0;
    message.Data = {0x00, kBankSwitchCommand};
    editor.list_of_texts_ = {std::move(message)};
    editor.MarkDomainDirty(MessageEditor::SaveDomain::kVanillaMessages);
  }

  static void SetFontDirty(MessageEditor& editor) {
    editor.message_preview_.width_array[0] = 0x0C;
    editor.MarkDomainDirty(MessageEditor::SaveDomain::kFontWidths);
  }
};

namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

class ScopedTempDir {
 public:
  ScopedTempDir() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = fs::temp_directory_path() /
            ("yaze_message_publish_guard_" + std::to_string(nonce));
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

std::map<int, std::string> ParseExpandedBundle(const std::string& text) {
  const Json document = Json::parse(text);
  std::map<int, std::string> messages;
  for (const auto& message : document.at("messages")) {
    messages[message.at("id").get<int>()] =
        message.at("text").get<std::string>();
  }
  return messages;
}

std::map<std::string, std::string> AsmMessageBlocks(
    const std::string& include) {
  std::map<std::string, std::string> blocks;
  size_t cursor = 0;
  while (cursor < include.size()) {
    const size_t start = include.find("Message_", cursor);
    if (start == std::string::npos) {
      break;
    }
    const size_t line_end = include.find('\n', start);
    if (line_end == std::string::npos) {
      break;
    }
    std::string label = include.substr(start, line_end - start);
    if (!label.empty() && label.back() == ':') {
      label.pop_back();
    }
    const size_t next = include.find("\nMessage_", line_end);
    const size_t end = next == std::string::npos ? include.size() : next + 1;
    blocks.emplace(label, include.substr(start, end - start));
    if (next == std::string::npos) {
      break;
    }
    cursor = next + 1;
  }
  return blocks;
}

void ExpectInsideRoot(const fs::path& root, const fs::path& path) {
  const std::string root_text = root.string();
  const std::string path_text = path.string();
  ASSERT_FALSE(root_text.empty());
  EXPECT_EQ(path_text.compare(0, root_text.size(), root_text), 0) << path_text;
}

class ScopedImGuiFrame {
 public:
  ScopedImGuiFrame() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  ~ScopedImGuiFrame() { ImGui::DestroyContext(); }

  template <typename Draw>
  void DrawOnce(Draw&& draw) {
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(640.0f, 240.0f), ImGuiCond_Always);
    if (!ImGui::Begin("MessageSourcePublishProbe")) {
      ADD_FAILURE() << "message source probe window did not begin";
      ImGui::End();
      ImGui::Render();
      return;
    }
    draw();
    ImGui::End();
    ImGui::Render();
  }
};

std::string Bundle(const std::vector<std::pair<int, std::string>>& messages) {
  Json document;
  document["format"] = "yaze-message-bundle";
  document["version"] = 1;
  document["counts"] = {{"vanilla", 0},
                        {"expanded", static_cast<int>(messages.size())}};
  document["messages"] = Json::array();
  for (const auto& [id, text] : messages) {
    document["messages"].push_back(
        {{"id", id}, {"bank", "expanded"}, {"text", text}});
  }
  return document.dump(2) + "\n";
}

struct PublishFixture {
  explicit PublishFixture(
      size_t rom_size = 0x180100,
      const std::string& source_path = "Data/dialogue/expanded_messages.json")
      : root(), canonical(root.path() / source_path), rom(MakeRom(rom_size)) {
    fs::create_directories(root.path() / "Core/Generated");
    WriteText(canonical, Bundle({{0, "Alpha"}, {1, "Beta"}}));
    Json manifest = {{"manifest_version", 3},
                     {"hack_name", "Oracle of Secrets"},
                     {"messages",
                      {{"data_start", "0x2F8026"},
                       {"data_end", "0x2F80FF"},
                       {"vanilla_count", 1},
                       {"expanded_range",
                        {{"first", "0x18D"}, {"last", "0x18E"}, {"count", 2}}},
                       {"source",
                        {{"format", "yaze-message-bundle"},
                         {"version", 1},
                         {"canonical_bundle_path", source_path},
                         {"generated_asm_include_path",
                          "Core/Generated/expanded_messages.asm"}}}}}};
    project.filepath = (root.path() / "Oracle-of-Secrets.yaze").string();
    project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
    if (!project.hack_manifest.LoadFromString(manifest.dump()).ok()) {
      throw std::runtime_error("failed to load publish-guard manifest");
    }
    dependencies.rom = &rom;
    dependencies.project = &project;
    dependencies.session_id = static_cast<size_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
  }

  static Rom MakeRom(size_t size) {
    Rom rom;
    std::vector<uint8_t> data(size, 0xFF);
    EXPECT_TRUE(rom.LoadFromData(data).ok());
    return rom;
  }

  ScopedTempDir root;
  fs::path canonical;
  Rom rom;
  project::YazeProject project;
  EditorDependencies dependencies;
};

}  // namespace

TEST(MessageProjectSourceLoadTest,
     EditorLoadUsesDeclaredProjectPathWithoutWrites) {
  PublishFixture fixture(0x180100, "Dialogue/custom-canonical.json");
  fixture.project.code_folder = "Core";
  const auto rom_before = fixture.rom.vector();
  const auto source_before = ReadText(fixture.canonical);
  const auto include_path =
      fixture.root.path() / "Core/Generated/expanded_messages.asm";
  WriteText(include_path,
            "Existing generated output is not a load prerequisite");
  const auto include_before = ReadText(include_path);
  EditorSet editors(&fixture.rom);
  editors.ApplyDependencies(fixture.dependencies);
  auto* editor = editors.GetEditorAs<MessageEditor>(EditorType::kMessage);
  ASSERT_NE(editor, nullptr);
  editor->Initialize();

  ASSERT_TRUE(editor->Load().ok());

  ASSERT_EQ(MessageEditorPublishGuardTestPeer::ExpandedCount(*editor), 2u);
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedBaseId(*editor), 0x18D);
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(*editor, 0),
            "Alpha");
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(*editor, 1),
            "Beta");
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::LoadedSourceSha(*editor),
            ComputeMessageSourceSha256(source_before));
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::BundleStatusError(*editor));
  EXPECT_NE(MessageEditorPublishGuardTestPeer::BundleStatus(*editor).find(
                "Dialogue/custom-canonical.json"),
            std::string::npos);
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::ExpandedDirty(*editor));
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::VanillaDirty(*editor));
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::FontDirty(*editor));
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::Ready(*editor));
  EXPECT_EQ(fixture.rom.vector(), rom_before);
  EXPECT_FALSE(fixture.rom.dirty());
  EXPECT_EQ(ReadText(fixture.canonical), source_before);
  EXPECT_EQ(ReadText(include_path), include_before);
}

TEST(MessageProjectSourceLoadTest,
     RepeatedLoadPreservesDraftAndOriginalSourceIdentity) {
  PublishFixture fixture;
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  ASSERT_TRUE(editor.Load().ok());
  const auto source_sha =
      MessageEditorPublishGuardTestPeer::LoadedSourceSha(editor);
  MessageEditorPublishGuardTestPeer::SetExpandedText(editor, 0, "Draft");
  MessageEditorPublishGuardTestPeer::MarkExpandedDirty(editor);
  WriteText(fixture.canonical, Bundle({{0, "External"}, {1, "Beta"}}));

  ASSERT_TRUE(editor.Load().ok());

  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(editor, 0),
            "Draft");
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::LoadedSourceSha(editor),
            source_sha);
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
  EXPECT_FALSE(editor.LoadCanonicalExpandedSource().ok());
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(editor, 0),
            "Draft");
}

TEST(MessageProjectSourceLoadTest, FirstLoadPreservesManualImport) {
  PublishFixture fixture;
  const auto incoming = fixture.root.path() / "draft.json";
  WriteText(incoming, Bundle({{0, "Imported draft"}}));
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  MessageEditorPublishGuardTestPeer::Import(editor, incoming.string());
  ASSERT_EQ(MessageEditorPublishGuardTestPeer::ExpandedCount(editor), 1u);

  ASSERT_TRUE(editor.Load().ok());

  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedCount(editor), 1u);
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(editor, 0),
            "Imported draft");
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
  EXPECT_TRUE(
      MessageEditorPublishGuardTestPeer::LoadedSourceSha(editor).empty());
}

TEST(MessageProjectSourceLoadTest, FirstLoadPreservesPendingVanillaEdits) {
  PublishFixture fixture;
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  MessageEditorPublishGuardTestPeer::SetVanillaDirty(editor);
  const auto rom_before = fixture.rom.vector();

  ASSERT_TRUE(editor.Load().ok());

  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedCount(editor), 0u);
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::VanillaDirty(editor));
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::BundleStatusError(editor));
  EXPECT_NE(MessageEditorPublishGuardTestPeer::BundleStatus(editor).find(
                "pending edits"),
            std::string::npos);
  EXPECT_EQ(fixture.rom.vector(), rom_before);
  EXPECT_TRUE(fixture.rom.dirty());
}

TEST(MessageProjectSourceLoadTest,
     MissingSourceShowsPathAndCanBeRetriedExplicitly) {
  PublishFixture fixture;
  fs::remove(fixture.canonical);
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  const auto rom_before = fixture.rom.vector();

  ASSERT_TRUE(editor.Load().ok());

  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::BundleStatusError(editor));
  EXPECT_NE(MessageEditorPublishGuardTestPeer::BundleStatus(editor).find(
                "expanded_messages.json"),
            std::string::npos);
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedCount(editor), 0u);
  EXPECT_FALSE(editor.OpenMessageById(0x18D));
  EXPECT_EQ(fixture.rom.vector(), rom_before);
  EXPECT_FALSE(fixture.rom.dirty());
  EXPECT_FALSE(fs::exists(fixture.canonical));
  WriteText(fixture.canonical, Bundle({{0, "Restored"}, {1, "Beta"}}));
  ASSERT_TRUE(editor.LoadCanonicalExpandedSource().ok());
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(editor, 0),
            "Restored");
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::BundleStatusError(editor));
}

TEST(MessageProjectSourceLoadTest,
     InvalidSourceNeverPartiallyLoadsOrMarksDirty) {
  for (const auto& invalid : std::vector<std::string>{
           "not JSON", Bundle({{0, "Incomplete"}}),
           Bundle({{0, "First"}, {0, "Duplicate"}}),
           Bundle({{0, "Valid"}, {2, "Out of range"}}),
           Bundle({{0, "Valid"}, {1, "[BANK]"}}),
           Bundle({{0, std::string(300, 'A')}, {1, "Capacity"}})}) {
    SCOPED_TRACE(invalid);
    PublishFixture fixture;
    WriteText(fixture.canonical, invalid);
    MessageEditor editor(&fixture.rom, fixture.dependencies);
    const auto rom_before = fixture.rom.vector();

    ASSERT_TRUE(editor.Load().ok());

    EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedCount(editor), 0u);
    EXPECT_TRUE(MessageEditorPublishGuardTestPeer::BundleStatusError(editor));
    EXPECT_TRUE(
        MessageEditorPublishGuardTestPeer::LoadedSourceSha(editor).empty());
    EXPECT_FALSE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
    EXPECT_EQ(fixture.rom.vector(), rom_before);
    EXPECT_FALSE(fixture.rom.dirty());
    EXPECT_EQ(ReadText(fixture.canonical), invalid);
  }
}

TEST(MessageProjectSourceLoadTest, SessionProjectSourcesRemainIsolated) {
  PublishFixture first;
  PublishFixture second;
  WriteText(second.canonical, Bundle({{0, "Second"}, {1, "Session"}}));
  EditorSet first_editors(&first.rom);
  EditorSet second_editors(&second.rom);
  first_editors.ApplyDependencies(first.dependencies);
  second_editors.ApplyDependencies(second.dependencies);
  auto* first_editor =
      first_editors.GetEditorAs<MessageEditor>(EditorType::kMessage);
  auto* second_editor =
      second_editors.GetEditorAs<MessageEditor>(EditorType::kMessage);
  ASSERT_TRUE(first_editor->Load().ok());
  ASSERT_TRUE(second_editor->Load().ok());

  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(*first_editor, 0),
            "Alpha");
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(*second_editor, 0),
            "Second");
  MessageEditorPublishGuardTestPeer::SetExpandedText(*first_editor, 0,
                                                     "First draft");
  MessageEditorPublishGuardTestPeer::MarkExpandedDirty(*first_editor);
  ASSERT_TRUE(second_editor->Load().ok());
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(*second_editor, 0),
            "Second");
  EXPECT_FALSE(
      MessageEditorPublishGuardTestPeer::ExpandedDirty(*second_editor));
  EXPECT_NE(MessageEditorPublishGuardTestPeer::LoadedSourceSha(*first_editor),
            MessageEditorPublishGuardTestPeer::LoadedSourceSha(*second_editor));
}

TEST(MessageProjectSourceLoadTest,
     ProjectsWithoutSourceKeepExistingLoadBehavior) {
  PublishFixture fixture;
  project::YazeProject generic;
  fixture.dependencies.project = &generic;
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  const auto incoming = fixture.root.path() / "generic-draft.json";
  WriteText(incoming, Bundle({{0, "Generic draft"}}));
  MessageEditorPublishGuardTestPeer::Import(editor, incoming.string());
  ASSERT_TRUE(editor.Load().ok());
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(editor, 0),
            "Generic draft");
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::BundleStatusError(editor));
  fixture.dependencies.project = nullptr;
  MessageEditor vanilla(&fixture.rom, fixture.dependencies);
  ASSERT_TRUE(vanilla.Load().ok());
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedCount(vanilla), 0u);
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::BundleStatusError(vanilla));
}

TEST(MessageSourcePublishGuardTest, ExternalChangeBeforePreviewIsNotPublished) {
  PublishFixture fixture;
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::LoadCanonical(editor).ok());
  WriteText(fixture.canonical, Bundle({{0, "Alpha"}, {1, "EXTERNAL"}}));
  MessageEditorPublishGuardTestPeer::SetExpandedText(editor, 0, "Alphb");
  MessageEditorPublishGuardTestPeer::MarkExpandedDirty(editor);

  MessageEditorPublishGuardTestPeer::Preview(editor);

  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::Ready(editor));
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::StatusError(editor));
  EXPECT_NE(
      MessageEditorPublishGuardTestPeer::Status(editor).find("changed on disk"),
      std::string::npos);
  EXPECT_NE(ReadText(fixture.canonical).find("EXTERNAL"), std::string::npos);
  EXPECT_EQ(ReadText(fixture.canonical).find("Alphb"), std::string::npos);
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
  EXPECT_FALSE(fixture.rom.dirty());
}

TEST(MessageSourcePublishGuardTest, DraftChangeAfterPreviewIsNotPublished) {
  PublishFixture fixture;
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::LoadCanonical(editor).ok());
  MessageEditorPublishGuardTestPeer::SetExpandedText(editor, 1, "Betb");
  MessageEditorPublishGuardTestPeer::Preview(editor);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::Ready(editor));
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::Changed(editor));

  MessageEditorPublishGuardTestPeer::SetExpandedText(editor, 1, "Betc");
  MessageEditorPublishGuardTestPeer::Publish(editor);

  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::StatusError(editor));
  EXPECT_NE(
      MessageEditorPublishGuardTestPeer::Status(editor).find("draft changed"),
      std::string::npos);
  EXPECT_NE(ReadText(fixture.canonical).find("Beta"), std::string::npos);
  EXPECT_EQ(ReadText(fixture.canonical).find("Betc"), std::string::npos);
}

TEST(MessageSourcePublishGuardTest, DirtyMarkInvalidatesPreviewBeforePublish) {
  PublishFixture fixture;
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::LoadCanonical(editor).ok());
  MessageEditorPublishGuardTestPeer::SetExpandedText(editor, 1, "Betb");
  MessageEditorPublishGuardTestPeer::Preview(editor);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::Ready(editor));

  MessageEditorPublishGuardTestPeer::MarkExpandedDirty(editor);
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::Ready(editor));
  MessageEditorPublishGuardTestPeer::Publish(editor);

  EXPECT_NE(MessageEditorPublishGuardTestPeer::Status(editor).find(
                "Preview the publication"),
            std::string::npos);
  EXPECT_NE(ReadText(fixture.canonical).find("Beta"), std::string::npos);
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
}

TEST(MessageSourcePublishGuardTest,
     ExternalChangeAfterPreviewKeepsTheDraftAndTheNewSource) {
  PublishFixture fixture;
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::LoadCanonical(editor).ok());
  MessageEditorPublishGuardTestPeer::SetExpandedText(editor, 1, "Betb");
  MessageEditorPublishGuardTestPeer::MarkExpandedDirty(editor);
  MessageEditorPublishGuardTestPeer::Preview(editor);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::Ready(editor));
  WriteText(fixture.canonical, Bundle({{0, "Alpha"}, {1, "EXTERNAL"}}));

  MessageEditorPublishGuardTestPeer::Publish(editor);

  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::StatusError(editor));
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
  EXPECT_NE(ReadText(fixture.canonical).find("EXTERNAL"), std::string::npos);
  EXPECT_EQ(ReadText(fixture.canonical).find("Betb"), std::string::npos);
  EXPECT_FALSE(fixture.rom.dirty());
}

TEST(MessageSourcePublishGuardTest,
     SuccessfulPublishClearsExpandedDraftAndKeepsOtherDirtyState) {
  PublishFixture fixture;
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::LoadCanonical(editor).ok());
  MessageEditorPublishGuardTestPeer::SetVanillaDirty(editor);
  MessageEditorPublishGuardTestPeer::SetFontDirty(editor);
  ASSERT_TRUE(fixture.rom.dirty());
  const auto rom_before = fixture.rom.vector();
  MessageEditorPublishGuardTestPeer::SetExpandedText(editor, 1, "Betb");
  MessageEditorPublishGuardTestPeer::MarkExpandedDirty(editor);
  MessageEditorPublishGuardTestPeer::Preview(editor);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::Changed(editor))
      << MessageEditorPublishGuardTestPeer::Status(editor);

  MessageEditorPublishGuardTestPeer::Publish(editor);

  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::StatusError(editor))
      << MessageEditorPublishGuardTestPeer::Status(editor);
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::VanillaDirty(editor));
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::FontDirty(editor));
  EXPECT_TRUE(fixture.rom.dirty());
  EXPECT_EQ(fixture.rom.vector(), rom_before);
  EXPECT_NE(ReadText(fixture.canonical).find("Betb"), std::string::npos);
  EXPECT_EQ(ReadText(fixture.canonical).find("\"Beta\""), std::string::npos);

  ASSERT_TRUE(editor.Save().ok())
      << MessageEditorPublishGuardTestPeer::Status(editor);
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::VanillaDirty(editor));
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::FontDirty(editor));
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
  EXPECT_NE(ReadText(fixture.canonical).find("Betb"), std::string::npos);

  MessageEditorPublishGuardTestPeer::SetExpandedText(editor, 1, "Betc");
  MessageEditorPublishGuardTestPeer::MarkExpandedDirty(editor);
  const auto status = editor.Save();
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_NE(std::string(status.message()).find(kOracleExpandedRomWriteRefusal),
            std::string::npos);
  EXPECT_NE(ReadText(fixture.canonical).find("Betb"), std::string::npos);
  EXPECT_EQ(ReadText(fixture.canonical).find("Betc"), std::string::npos);
  EXPECT_TRUE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
}

TEST(MessageSourcePublishGuardTest, FailedRomInspectionPreservesTheDraft) {
  PublishFixture fixture;
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::LoadCanonical(editor).ok());
  const std::string before =
      MessageEditorPublishGuardTestPeer::ExpandedText(editor, 1);

  const auto status = editor.LoadExpandedMessagesFromRom();

  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound) << status;
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(editor, 1), before);
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
}

TEST(OracleExpandedSourceAcceptance, ChangedPublishWritesOnlyTheEditedLine) {
  const char* root_env = std::getenv("YAZE_ORACLE_ACCEPTANCE_ROOT");
  ASSERT_NE(root_env, nullptr);
  ASSERT_NE(root_env[0], '\0')
      << "YAZE_ORACLE_ACCEPTANCE_ROOT must name an isolated Oracle copy";
  const fs::path root = fs::weakly_canonical(root_env);
  project::YazeProject project;
  const fs::path project_path = root / "Oracle-of-Secrets.yaze";
  ASSERT_TRUE(project.Open(project_path.string()).ok());
  EXPECT_EQ(project.rom_metadata.write_policy, project::RomWritePolicy::kBlock);
  ASSERT_TRUE(project.hack_manifest.loaded());
  const auto& layout = project.hack_manifest.message_layout();
  ASSERT_TRUE(layout.source.has_value());

  const fs::path canonical =
      fs::weakly_canonical(root / layout.source->canonical_bundle_path);
  const fs::path include =
      fs::weakly_canonical(root / layout.source->generated_asm_include_path);
  const fs::path build_script =
      fs::weakly_canonical(root / "Scripts/Build/build_rom.sh");
  const fs::path dev_rom = fs::weakly_canonical(root / "Roms/oos168.sfc");
  ExpectInsideRoot(root, canonical);
  ExpectInsideRoot(root, include);
  ExpectInsideRoot(root, build_script);
  ExpectInsideRoot(root, dev_rom);
  ExpectInsideRoot(root, fs::weakly_canonical(project_path));

  const std::map<int, std::string> bundle_before =
      ParseExpandedBundle(ReadText(canonical));
  const auto include_before = AsmMessageBlocks(ReadText(include));
  ASSERT_FALSE(bundle_before.empty());
  ASSERT_FALSE(include_before.empty());

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(dev_rom.string()).ok());
  const std::string dev_rom_before =
      ComputeMessageSourceSha256(ReadText(dev_rom));
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  dependencies.session_id = 20260929;
  MessageEditor editor(&rom, dependencies);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::LoadCanonical(editor).ok());

  int edited = -1;
  std::string original;
  std::string updated;
  for (const auto& [id, text] : bundle_before) {
    const auto pos = text.find("The beach,");
    if (pos == std::string::npos) {
      continue;
    }
    edited = id;
    original = text;
    updated = text;
    updated.replace(pos, std::string("The beach,").size(), "The beech,");
    MessageEditorPublishGuardTestPeer::SetExpandedText(editor, id, updated);
    break;
  }
  ASSERT_GE(edited, 0);
  ASSERT_EQ(MessageEditorPublishGuardTestPeer::ExpandedId(editor, edited),
            edited);
  MessageEditorPublishGuardTestPeer::MarkExpandedDirty(editor);

  {
    ScopedImGuiFrame gui;
    std::pair<bool, bool> before_preview;
    gui.DrawOnce([&] {
      before_preview =
          MessageEditorPublishGuardTestPeer::DrawPublishButtons(editor);
    });
    EXPECT_FALSE(before_preview.first);
    EXPECT_TRUE(before_preview.second);
  }

  MessageEditorPublishGuardTestPeer::Preview(editor);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::Ready(editor))
      << MessageEditorPublishGuardTestPeer::Status(editor);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::Changed(editor))
      << MessageEditorPublishGuardTestPeer::Status(editor);

  {
    ScopedImGuiFrame gui;
    std::pair<bool, bool> after_preview;
    gui.DrawOnce([&] {
      after_preview =
          MessageEditorPublishGuardTestPeer::DrawPublishButtons(editor);
    });
    EXPECT_FALSE(after_preview.first);
    EXPECT_FALSE(after_preview.second);
  }

  MessageEditorPublishGuardTestPeer::Publish(editor);
  ASSERT_FALSE(MessageEditorPublishGuardTestPeer::StatusError(editor))
      << MessageEditorPublishGuardTestPeer::Status(editor);
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::ExpandedDirty(editor));
  EXPECT_FALSE(rom.dirty());
  EXPECT_EQ(ComputeMessageSourceSha256(ReadText(dev_rom)), dev_rom_before);
  EXPECT_EQ(project.rom_metadata.write_policy, project::RomWritePolicy::kBlock);

  const std::map<int, std::string> bundle_after =
      ParseExpandedBundle(ReadText(canonical));
  ASSERT_EQ(bundle_before.size(), bundle_after.size());
  int changed_messages = 0;
  for (const auto& [id, text] : bundle_before) {
    ASSERT_TRUE(bundle_after.contains(id)) << id;
    if (bundle_after.at(id) == text) {
      continue;
    }
    ++changed_messages;
    EXPECT_EQ(id, edited);
    EXPECT_EQ(text, original);
    EXPECT_EQ(bundle_after.at(id), updated);
  }
  EXPECT_EQ(changed_messages, 1);

  const auto include_after = AsmMessageBlocks(ReadText(include));
  const std::string edited_label =
      absl::StrFormat("Message_%03X", layout.first_expanded_id + edited);
  ASSERT_TRUE(include_before.contains(edited_label)) << edited_label;
  ASSERT_TRUE(include_after.contains(edited_label)) << edited_label;
  EXPECT_NE(include_before.at(edited_label), include_after.at(edited_label));
  ASSERT_EQ(include_before.size(), include_after.size());
  for (const auto& [label, block] : include_before) {
    ASSERT_TRUE(include_after.contains(label)) << label;
    if (label == edited_label) {
      continue;
    }
    EXPECT_EQ(include_after.at(label), block) << label;
  }
  std::cout << "edited_local_id=" << edited << "\n";
  std::cout << "edited_label=" << edited_label << "\n";
  std::cout << "base_rom_sha256=" << dev_rom_before << "\n";
  std::cout << "canonical=" << canonical << "\n";
  std::cout << "include=" << include << "\n";
}

TEST(OracleExpandedSourceAcceptance, ReopenedBuiltBankContainsTheEditedLine) {
  const char* root_env = std::getenv("YAZE_ORACLE_ACCEPTANCE_ROOT");
  ASSERT_NE(root_env, nullptr);
  ASSERT_NE(root_env[0], '\0')
      << "YAZE_ORACLE_ACCEPTANCE_ROOT must name an isolated Oracle copy";
  const fs::path root = fs::weakly_canonical(root_env);
  project::YazeProject project;
  ASSERT_TRUE(project.Open((root / "Oracle-of-Secrets.yaze").string()).ok());
  EXPECT_EQ(project.rom_metadata.write_policy, project::RomWritePolicy::kBlock);
  EXPECT_EQ(project.hack_manifest.message_layout().data_start, 0x2F8026u);
  ASSERT_TRUE(project.hack_manifest.message_layout().source.has_value());

  const fs::path canonical = fs::weakly_canonical(
      root /
      project.hack_manifest.message_layout().source->canonical_bundle_path);
  const fs::path base_rom = fs::weakly_canonical(root / "Roms/oos168.sfc");
  const fs::path patched_rom = fs::weakly_canonical(root / "Roms/oos168x.sfc");
  ExpectInsideRoot(root, canonical);
  ExpectInsideRoot(root, base_rom);
  ExpectInsideRoot(root, patched_rom);

  const std::map<int, std::string> bundle =
      ParseExpandedBundle(ReadText(canonical));
  int edited = -1;
  std::string expected;
  int beech_count = 0;
  for (const auto& [id, text] : bundle) {
    if (text.find("The beech,") == std::string::npos) {
      continue;
    }
    ++beech_count;
    edited = id;
    expected = text;
  }
  ASSERT_EQ(beech_count, 1);
  ASSERT_GE(edited, 0);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(patched_rom.string()).ok());
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  dependencies.session_id = 20260929;
  MessageEditor editor(&rom, dependencies);
  ASSERT_TRUE(editor.LoadExpandedMessagesFromRom().ok());
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(editor, edited),
            expected);
  int rom_hits = 0;
  for (const auto& [id, text] : bundle) {
    if (MessageEditorPublishGuardTestPeer::ExpandedText(editor, id)
            .find("The beech,") != std::string::npos) {
      ++rom_hits;
      EXPECT_EQ(id, edited);
    }
  }
  EXPECT_EQ(rom_hits, 1);
  EXPECT_EQ(MessageEditorPublishGuardTestPeer::Ready(editor), false);
  EXPECT_FALSE(rom.dirty());
  std::cout << "edited_local_id=" << edited << "\n";
  std::cout << "base_rom_sha256="
            << ComputeMessageSourceSha256(ReadText(base_rom)) << "\n";
  std::cout << "patched_rom_sha256="
            << ComputeMessageSourceSha256(ReadText(patched_rom)) << "\n";
}

TEST(MessageSourcePublishGuardTest, RomInspectionUsesTheManifestDataStart) {
  PublishFixture fixture;
  const int manifest_start = static_cast<int>(SnesToPc(0x2F8026));
  const int manifest_end = static_cast<int>(SnesToPc(0x2F80FF));
  ASSERT_NE(manifest_start, kExpandedTextDataDefault);
  ASSERT_TRUE(WriteExpandedTextData(&fixture.rom, manifest_start, manifest_end,
                                    {"Hello"})
                  .ok());
  MessageEditor editor(&fixture.rom, fixture.dependencies);
  ASSERT_TRUE(MessageEditorPublishGuardTestPeer::LoadCanonical(editor).ok());

  ASSERT_TRUE(editor.LoadExpandedMessagesFromRom().ok());

  EXPECT_EQ(MessageEditorPublishGuardTestPeer::ExpandedText(editor, 0),
            "Hello");
  EXPECT_FALSE(MessageEditorPublishGuardTestPeer::Ready(editor));
}

}  // namespace yaze::editor
