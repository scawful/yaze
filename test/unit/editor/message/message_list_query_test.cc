#include "app/editor/message/message_list_query.h"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/editor/layout/layout_presets.h"
#include "app/editor/message/message_editor.h"
#include "app/editor/message/ui/message_editor_views.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/resource/arena.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {

class MessageEditorListPanelTestPeer {
 public:
  static void Seed(MessageEditor& editor) {
    editor.expanded_message_base_id_ = 0x18D;
    editor.parsed_messages_.resize(0x18F);
    const auto add = [&](int id, bool expanded, const char* text) {
      MessageData message;
      message.ID = id;
      message.RawString = text;
      message.ContentsParsed = text;
      const int display_id = expanded ? 0x18D + id : id;
      editor.parsed_messages_[display_id] = text;
      (expanded ? editor.expanded_messages_ : editor.list_of_texts_)
          .push_back(std::move(message));
    };
    add(0, false, "Keep zero");
    add(1, false, "Hide one");
    add(2, false, "Keep two");
    add(0, true, "Keep expanded");
    add(1, true, "Hide expanded");
  }

  static void Filter(MessageEditor& editor, const std::string& filter) {
    editor.list_filter_ = filter;
  }

  static int CurrentId(const MessageEditor& editor) {
    return editor.CurrentDisplayId();
  }

  static const std::string& CurrentText(const MessageEditor& editor) {
    return editor.message_text_box_.text;
  }

  static int RowCount(const MessageEditor& editor) {
    return static_cast<int>(editor.CollectMessageRows(true).size());
  }
};

namespace {

class MessageWriterWorkspaceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(1280, 800);
    io.DeltaTime = 1.0f / 60;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    std::vector<uint8_t> bytes(0x200000, 0);
    bytes[kTextData] = 0x00;
    bytes[kTextData + 1] = kMessageTerminator;
    bytes[kTextData + 2] = 0xFF;
    ASSERT_TRUE(rom_.LoadFromData(bytes).ok());
    windows_.RegisterSession(0);
    windows_.SetActiveSession(0);
    project_.workspace_settings.last_layout_preset = "Writer";
    EditorDependencies dependencies;
    dependencies.rom = &rom_;
    dependencies.project = &project_;
    dependencies.window_manager = &windows_;
    editor_ = std::make_unique<MessageEditor>(&rom_, dependencies);
    editor_->Initialize();
  }
  void TearDown() override {
    gfx::Arena::Get().ClearTextureQueue();
    editor_.reset();
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
  }
  std::string Frame(const std::function<void()>& draw) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_Always);
    ImGui::Begin("WriterWorkspaceTest", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    draw();
    const std::string text = context_->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    return text;
  }
  float TextPaneHeight() const {
    for (const auto* window : context_->Windows) {
      if (window->LastFrameActive == context_->FrameCount &&
          std::string(window->Name).find("##MessageTextPane") !=
              std::string::npos) {
        return window->Size.y;
      }
    }
    return 0;
  }
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  Rom rom_;
  project::YazeProject project_;
  WorkspaceWindowManager windows_;
  std::unique_ptr<MessageEditor> editor_;
};

TEST_F(MessageWriterWorkspaceTest, WriterWindowsHaveRealRegisteredContents) {
  const auto preset = LayoutPresets::GetWriterPreset();
  windows_.HideAll(0);
  for (const auto& id : preset.default_visible_panels) {
    SCOPED_TRACE(id);
    ASSERT_NE(windows_.GetWindowContent(0, id), nullptr);
    ASSERT_TRUE(windows_.OpenWindow(0, id));
    EXPECT_TRUE(windows_.IsWindowOpen(0, id));
  }
  EXPECT_FALSE(
      windows_.IsWindowOpen(0, LayoutPresets::Panels::kMessageFontAtlas));
  const auto* preview =
      windows_.GetWindowDescriptor(0, LayoutPresets::Panels::kMessagePreview);
  ASSERT_NE(preview, nullptr);
  EXPECT_EQ(preview->display_name, "Message Preview");
  EXPECT_TRUE(preview->IsListedInWindowBrowser());
}

TEST_F(MessageWriterWorkspaceTest, WriterUsesSeparatePreviewAndLargerTextPane) {
  project_.workspace_settings.last_layout_preset.clear();
  const auto embedded = Frame([&] { editor_->DrawCurrentMessage(); });
  const auto embedded_height = TextPaneHeight();
  EXPECT_NE(embedded.find("Message Preview"), std::string::npos);
  project_.workspace_settings.last_layout_preset = "Writer";
  const auto writer = Frame([&] { editor_->DrawCurrentMessage(); });
  EXPECT_EQ(writer.find("Message Preview"), std::string::npos);
  EXPECT_GT(TextPaneHeight(), embedded_height + 100);
  auto* preview =
      windows_.GetWindowContent(0, LayoutPresets::Panels::kMessagePreview);
  ASSERT_NE(preview, nullptr);
  const auto separate = Frame([&] { preview->Draw(nullptr); });
  EXPECT_NE(separate.find("Message Preview"), std::string::npos);
  EXPECT_FALSE(rom_.dirty());
}

// Draw the same WindowContent callback registered by MessageEditor::Initialize,
// without loading ROM graphics or opening a native window.
class MessageListPanelIdTest : public ::testing::Test {
 protected:
  MessageListPanelIdTest()
      : first_panel_([this] { first_.DrawMessageList(); }),
        second_panel_([this] { second_.DrawMessageList(); }) {}

  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(1600, 800);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigDebugHighlightIdConflicts = true;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    MessageEditorListPanelTestPeer::Seed(first_);
    MessageEditorListPanelTestPeer::Seed(second_);
    Frame();
    Frame();
  }

  void TearDown() override {
    // Clicking a message queues its preview bitmap; no renderer consumes it in
    // this headless test. Remove the queue before the editors are destroyed.
    gfx::Arena::Get().ClearTextureQueue();
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
  }

  struct HoverResult {
    ImGuiID id;
    int submissions;
  };

  HoverResult Frame(ImVec2 mouse = ImVec2(-100, -100)) {
    ImGui::GetIO().AddMousePosEvent(mouse.x, mouse.y);
    ImGui::NewFrame();
    DrawPanel("Messages###message_id_first", first_panel_, first_, 0);
    DrawPanel("Messages###message_id_second", second_panel_, second_, 1);
    const HoverResult result{context_->HoveredId,
                             context_->HoveredIdPreviousFrameItemCount};
    ImGui::Render();
    return result;
  }

  HoverResult Hover(int panel, int column, int row) {
    const ImVec2 position = CellPosition(panel, column, row);
    Frame(position);
    return Frame(position);
  }

  void ClickRow(int panel, int row) {
    const ImVec2 position = CellPosition(panel, 0, row);
    Frame(position);
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    Frame(position);
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    Frame(position);
  }

  void DrawPanel(const char* name, MessageListPanel& panel,
                 MessageEditor& editor, int index) {
    ImGui::SetNextWindowPos(ImVec2(10 + index * 790, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(770, 700), ImGuiCond_Always);
    ImGui::Begin(name, nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize);
    const ImGuiID table_id = ImGui::GetID("##MessagesTable");
    panel.Draw(nullptr);
    auto* table = ImGui::TableFindByID(table_id);
    EXPECT_NE(table, nullptr);
    if (table != nullptr) {
      auto& geometry = geometry_[index];
      geometry.row_count = MessageEditorListPanelTestPeer::RowCount(editor);
      geometry.row_height = table->RowPosY2 - table->RowPosY1;
      geometry.last_row_center = (table->RowPosY1 + table->RowPosY2) * 0.5f;
      for (int column = 0; column < 2; ++column) {
        geometry.column_x[column] = table->Columns[column].WorkMinX + 12;
      }
    }
    ImGui::End();
  }

  ImVec2 CellPosition(int panel, int column, int row) const {
    const auto& geometry = geometry_[panel];
    return ImVec2(geometry.column_x[column],
                  geometry.last_row_center -
                      geometry.row_height * (geometry.row_count - row - 1));
  }

  struct TableGeometry {
    std::array<float, 2> column_x{};
    float row_height = 0;
    float last_row_center = 0;
    int row_count = 0;
  };
  std::array<TableGeometry, 2> geometry_;
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  MessageEditor first_;
  MessageEditor second_;
  MessageListPanel first_panel_;
  MessageListPanel second_panel_;
};

TEST_F(MessageListPanelIdTest, RepeatedBankBadgesHaveNoConflictsAcrossPanels) {
  std::vector<ImGuiID> ids;
  for (int panel = 0; panel < 2; ++panel) {
    for (int row = 0; row < 5; ++row) {
      SCOPED_TRACE(::testing::Message() << "panel=" << panel << " row=" << row);
      const auto badge = Hover(panel, 1, row);
      ASSERT_NE(badge.id, 0u);
      EXPECT_EQ(badge.submissions, 1);
      for (const ImGuiID previous : ids) {
        EXPECT_NE(badge.id, previous);
      }
      ids.push_back(badge.id);
    }
  }
  EXPECT_TRUE(ImGui::GetIO().ConfigDebugHighlightIdConflicts);
}

TEST_F(MessageListPanelIdTest,
       FilteringKeepsIdentityAndSelectsTheRightMessage) {
  const auto original_two = Hover(0, 0, 2);
  const auto original_expanded = Hover(0, 0, 3);
  ASSERT_NE(original_two.id, 0u);
  ASSERT_NE(original_expanded.id, 0u);

  MessageEditorListPanelTestPeer::Filter(first_, "Keep");
  Frame();
  Frame();
  EXPECT_EQ(Hover(0, 0, 1).id, original_two.id);
  EXPECT_EQ(Hover(0, 0, 2).id, original_expanded.id);
  ClickRow(0, 1);
  EXPECT_EQ(MessageEditorListPanelTestPeer::CurrentId(first_), 2);
  EXPECT_EQ(MessageEditorListPanelTestPeer::CurrentText(first_), "Keep two");
  ClickRow(0, 2);
  EXPECT_EQ(MessageEditorListPanelTestPeer::CurrentId(first_), 0x18D);
  EXPECT_EQ(MessageEditorListPanelTestPeer::CurrentText(first_),
            "Keep expanded");
  EXPECT_NE(MessageEditorListPanelTestPeer::CurrentId(second_), 0x18D);

  MessageEditorListPanelTestPeer::Filter(first_, "");
  Frame();
  Frame();
  EXPECT_EQ(Hover(0, 0, 2).id, original_two.id);
  EXPECT_EQ(Hover(0, 0, 3).id, original_expanded.id);
}

TEST(MessageListQueryTest, EmptyQueryKeepsEveryBank) {
  EXPECT_TRUE(MessageListQueryMatches(1, 0x0E0000, "Hello", false,
                                      MessageBankFilter::kAll, ""));
  EXPECT_TRUE(MessageListQueryMatches(400, 0x100000, "More", true,
                                      MessageBankFilter::kAll, ""));
}

TEST(MessageListQueryTest, BankFilterHidesTheOtherBank) {
  EXPECT_FALSE(MessageListQueryMatches(400, 0x100000, "More", true,
                                       MessageBankFilter::kVanilla, ""));
  EXPECT_TRUE(MessageListQueryMatches(400, 0x100000, "More", true,
                                      MessageBankFilter::kExpanded, ""));
  EXPECT_FALSE(MessageListQueryMatches(1, 0x0E0000, "Hello", false,
                                       MessageBankFilter::kExpanded, ""));
}

TEST(MessageListQueryTest, MatchesTextIdAndAddress) {
  EXPECT_TRUE(MessageListQueryMatches(0x42, 0x0E1234, "Link saved Hyrule",
                                      false, MessageBankFilter::kAll,
                                      "hyrule"));
  EXPECT_TRUE(MessageListQueryMatches(0x42, 0x0E1234, "Link", false,
                                      MessageBankFilter::kAll, "66"));
  EXPECT_TRUE(MessageListQueryMatches(0x42, 0x0E1234, "Link", false,
                                      MessageBankFilter::kAll, "$42"));
  EXPECT_TRUE(MessageListQueryMatches(0x42, 0x0E1234, "Link", false,
                                      MessageBankFilter::kAll, "0xE1234"));
  EXPECT_FALSE(MessageListQueryMatches(0x42, 0x0E1234, "Link", false,
                                       MessageBankFilter::kAll, "ganon"));
}

TEST(MessageListQueryTest, FindNextRespectsCaseAndWholeWord) {
  const std::string_view text = "The cat catalogs a cat";
  auto loose = FindNextTextMatch(text, "cat", 0, false, false);
  ASSERT_TRUE(loose.has_value());
  EXPECT_EQ(*loose, 4u);

  auto next = FindNextTextMatch(text, "cat", *loose + 1, false, false);
  ASSERT_TRUE(next.has_value());
  EXPECT_EQ(*next, 8u);

  auto whole = FindNextTextMatch(text, "cat", 0, false, true);
  ASSERT_TRUE(whole.has_value());
  EXPECT_EQ(*whole, 4u);
  auto whole_next = FindNextTextMatch(text, "cat", *whole + 1, false, true);
  ASSERT_TRUE(whole_next.has_value());
  EXPECT_EQ(*whole_next, 19u);

  EXPECT_FALSE(FindNextTextMatch("Link", "link", 0, true, false).has_value());
  EXPECT_TRUE(FindNextTextMatch("Link", "link", 0, false, false).has_value());
}

TEST(MessageListQueryTest, FindPreviousStopsBeforeCursor) {
  const std::string_view text = "cat and cat";
  auto previous = FindPreviousTextMatch(text, "cat", 8, false, true);
  ASSERT_TRUE(previous.has_value());
  EXPECT_EQ(*previous, 0u);

  auto last =
      FindPreviousTextMatch(text, "cat", std::string::npos, false, true);
  ASSERT_TRUE(last.has_value());
  EXPECT_EQ(*last, 8u);

  EXPECT_FALSE(FindPreviousTextMatch(text, "cat", 0, false, true).has_value());
  EXPECT_FALSE(FindNextTextMatch(text, "", 0, false, false).has_value());
}

TEST(MessageListQueryTest, PreviewKeepsTheFirstLineShort) {
  EXPECT_EQ(MessageListPreview("Hello\nWorld", 96), "Hello");
  EXPECT_EQ(MessageListPreview("abcdef", 4), "abcd...");
}

}  // namespace
}  // namespace yaze::editor
