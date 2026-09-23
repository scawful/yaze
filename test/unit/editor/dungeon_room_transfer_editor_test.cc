#include "app/editor/dungeon/inspectors/dungeon_room_transfer_editor.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_transfer.h"
#include "app/gui/automation/widget_id_registry.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {

class DungeonRoomTransferEditorTestPeer {
 public:
  static gui::CanvasMenuItem RoomMenu(DungeonCanvasViewer& viewer,
                                      int room_id) {
    return viewer.BuildRoomContextMenu(room_id);
  }
};

namespace {

class DungeonRoomTransferEditorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    for (int room_id : {0, 1}) {
      rooms_[room_id] = zelda3::Room(room_id, &rom_);
      rooms_[room_id].SetLoaded(true);
      rooms_[room_id].SetTileObjects({zelda3::RoomObject(0x000, 4, 4, 0, 0)});
    }
    rooms_[1].AddTileObject(zelda3::RoomObject(0x002, 8, 8, 0, 0));
    viewer_.RefreshRomBackedState(&rom_, nullptr, &rooms_, 0);
    viewer_.SetRoomTransferCallbacks(
        [this](int room_id) -> absl::StatusOr<std::string> {
          ++exports_;
          return SerializeDungeonRoomDocument(
              CaptureDungeonRoomDocument(rooms_[room_id]));
        },
        [this](int target, int source, const std::string& json,
               const DungeonRoomTransferOptions& options)
            -> absl::StatusOr<DungeonRoomTransferPlan> {
          ++previews_;
          last_source_ = source;
          last_target_ = target;
          if (reject_preview_) {
            return absl::FailedPreconditionError("Test preview rejection");
          }
          if (source >= 0) {
            return PlanDungeonRoomTransfer(
                rooms_[target], CaptureDungeonRoomDocument(rooms_[source]),
                options);
          }
          const auto document = ParseDungeonRoomDocument(json);
          if (!document.ok()) {
            return document.status();
          }
          return PlanDungeonRoomTransfer(rooms_[target], *document, options);
        },
        [this](const DungeonRoomTransferPlan& plan) {
          ++attempts_;
          if (reject_apply_) {
            return absl::FailedPreconditionError("Test apply rejection");
          }
          ApplyDungeonRoomDocument(rooms_[plan.target_room_id], plan.after);
          ++changes_;
          return absl::OkStatus();
        });
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1000, 1400);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.AddMousePosEvent(-1000, -1000);
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
    gui::WidgetIdRegistry::Instance().Clear();
  }

  void DrawFrame() {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width_, 1300), ImGuiCond_Always);
    ImGui::Begin("RoomTransferHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    if (popup_) {
      DrawDungeonRoomTransferPopup(viewer_);
    } else {
      DrawDungeonRoomTransferEditor(viewer_);
    }
    logged_text_ = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    const auto* window = ImGui::GetCurrentWindow();
    EXPECT_LE(window->DC.CursorMaxPos.x, window->WorkRect.Max.x + 1);
    ImGui::End();
    ImGui::Render();
    EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
    EXPECT_EQ(ImGui::GetCurrentContext()->StyleVarStack.Size, 0);
  }

  std::optional<gui::WidgetIdRegistry::WidgetInfo> Widget(const char* name) {
    const auto normalized = gui::WidgetIdRegistry::NormalizeLabel(name);
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (path.find("Dungeon/RoomTransfer/") != std::string::npos &&
          widget.label == normalized) {
        return widget;
      }
    }
    return std::nullopt;
  }

  void Click(const char* name) {
    const auto widget = Widget(name);
    ASSERT_TRUE(widget) << name;
    ASSERT_TRUE(widget->enabled) << name;
    ASSERT_TRUE(widget->visible) << name;
    ASSERT_TRUE(widget->bounds.valid) << name;
    const auto bounds = widget->bounds;
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((bounds.min_x + bounds.max_x) / 2,
                        (bounds.min_y + bounds.max_y) / 2);
    DrawFrame();
    io.AddMouseButtonEvent(0, true);
    DrawFrame();
    io.AddMouseButtonEvent(0, false);
    DrawFrame();
    DrawFrame();
  }

  void Prepare() {
    DrawFrame();
    DrawFrame();
  }

  Rom rom_;
  DungeonRoomStore rooms_{&rom_};
  DungeonCanvasViewer viewer_{&rom_};
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  std::string logged_text_;
  float width_ = 320;
  int exports_ = 0;
  int previews_ = 0;
  int attempts_ = 0;
  int changes_ = 0;
  int last_source_ = -2;
  int last_target_ = -1;
  bool reject_preview_ = false;
  bool reject_apply_ = false;
  bool popup_ = false;
};

TEST_F(DungeonRoomTransferEditorTest, PreviewIsExplicitAndDoesNotMutate) {
  Prepare();
  EXPECT_EQ(previews_, 0);
  EXPECT_FALSE(Widget("TransferApply"));
  Click("TransferPreview");
  ASSERT_TRUE(viewer_.room_transfer_state().preview);
  EXPECT_EQ(previews_, 1);
  EXPECT_EQ(attempts_, 0);
  EXPECT_EQ(last_target_, 0);
  EXPECT_EQ(last_source_, 1);
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 1);
  EXPECT_EQ(viewer_.room_transfer_state().preview->options.domains,
            kTransferCore);
  EXPECT_FALSE(
      viewer_.room_transfer_state().preview->options.copy_destinations);
  DrawFrame();
  DrawFrame();
  EXPECT_EQ(previews_, 1);
  EXPECT_NE(logged_text_.find("Objects: 1"), std::string::npos);
  EXPECT_NE(logged_text_.find("preserve"), std::string::npos);
}

TEST_F(DungeonRoomTransferEditorTest, ApplyPublishesOnceAndClearsReviewedPlan) {
  Prepare();
  Click("TransferPreview");
  Click("TransferApply");
  EXPECT_EQ(attempts_, 1);
  EXPECT_EQ(changes_, 1);
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 2);
  EXPECT_EQ(rooms_[1].GetTileObjects().size(), 2);
  EXPECT_FALSE(viewer_.room_transfer_state().preview);
  EXPECT_FALSE(Widget("TransferApply"));
  EXPECT_NE(logged_text_.find("Replacement applied"), std::string::npos);
}

TEST_F(DungeonRoomTransferEditorTest, DomainChangeRequiresNewPreview) {
  Prepare();
  Click("TransferPreview");
  Click("TransferSprites");
  EXPECT_FALSE(viewer_.room_transfer_state().preview);
  EXPECT_EQ(previews_, 1);
  EXPECT_EQ(viewer_.room_transfer_state().domains,
            kTransferCore & ~kTransferSprites);
  EXPECT_FALSE(Widget("TransferApply"));
  Click("TransferPreview");
  ASSERT_TRUE(viewer_.room_transfer_state().preview);
  EXPECT_EQ(previews_, 2);
  EXPECT_EQ(viewer_.room_transfer_state().preview->options.domains,
            kTransferCore & ~kTransferSprites);
}

TEST_F(DungeonRoomTransferEditorTest, DestinationsOptInRequiresRoomProperties) {
  Prepare();
  Click("TransferDestinations");
  Click("TransferPreview");
  ASSERT_TRUE(viewer_.room_transfer_state().preview);
  EXPECT_TRUE(viewer_.room_transfer_state().preview->options.copy_destinations);
  Click("TransferMetadata");
  EXPECT_FALSE(viewer_.room_transfer_state().copy_destinations);
  EXPECT_FALSE(viewer_.room_transfer_state().preview);
  ASSERT_TRUE(Widget("TransferDestinations"));
  EXPECT_FALSE(Widget("TransferDestinations")->enabled);
}

TEST_F(DungeonRoomTransferEditorTest,
       ImportUsesClipboardDocumentAndExplicitApply) {
  const auto source =
      SerializeDungeonRoomDocument(CaptureDungeonRoomDocument(rooms_[1]));
  ASSERT_TRUE(source.ok()) << source.status();
  Prepare();
  Click("TransferImportMode");
  ImGui::SetClipboardText(source->c_str());
  Click("TransferPaste");
  EXPECT_EQ(previews_, 0);
  EXPECT_EQ(viewer_.room_transfer_state().json, *source);
  Click("TransferPreview");
  ASSERT_TRUE(viewer_.room_transfer_state().preview);
  EXPECT_EQ(last_source_, -1);
  EXPECT_EQ(attempts_, 0);
  Click("TransferApply");
  EXPECT_EQ(changes_, 1);
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 2);
}

TEST_F(DungeonRoomTransferEditorTest, InvalidImportShowsFailureWithoutApply) {
  Prepare();
  Click("TransferImportMode");
  ImGui::SetClipboardText("{not room json");
  Click("TransferPaste");
  Click("TransferPreview");
  EXPECT_FALSE(viewer_.room_transfer_state().preview);
  EXPECT_FALSE(viewer_.room_transfer_state().error.empty());
  EXPECT_FALSE(Widget("TransferApply"));
  EXPECT_EQ(attempts_, 0);
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 1);
}

TEST_F(DungeonRoomTransferEditorTest, PreviewRejectionPreservesRoom) {
  reject_preview_ = true;
  Prepare();
  Click("TransferPreview");
  EXPECT_EQ(previews_, 1);
  EXPECT_EQ(attempts_, 0);
  EXPECT_FALSE(Widget("TransferApply"));
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 1);
  EXPECT_NE(logged_text_.find("Test preview rejection"), std::string::npos);
}

TEST_F(DungeonRoomTransferEditorTest, ApplyRejectionRequiresNewPreview) {
  reject_apply_ = true;
  Prepare();
  Click("TransferPreview");
  Click("TransferApply");
  EXPECT_EQ(attempts_, 1);
  EXPECT_EQ(changes_, 0);
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 1);
  EXPECT_FALSE(viewer_.room_transfer_state().preview);
  EXPECT_NE(logged_text_.find("Test apply rejection"), std::string::npos);
}

TEST_F(DungeonRoomTransferEditorTest, ReadOnlyViewAllowsExportOnly) {
  viewer_.SetHeaderReadOnly(true);
  Prepare();
  for (const char* name : {"TransferPreview", "TransferSource",
                           "TransferObjects", "TransferImportMode"}) {
    ASSERT_TRUE(Widget(name)) << name;
    EXPECT_FALSE(Widget(name)->enabled) << name;
  }
  Click("TransferExport");
  EXPECT_EQ(exports_, 1);
  const auto parsed = ParseDungeonRoomDocument(ImGui::GetClipboardText());
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->contents.objects.size(), 1);
  EXPECT_EQ(previews_, 0);
  EXPECT_EQ(attempts_, 0);
}

TEST_F(DungeonRoomTransferEditorTest, RefreshClearsPlanAndDestinationDraft) {
  Prepare();
  Click("TransferDestinations");
  Click("TransferPreview");
  ASSERT_TRUE(viewer_.room_transfer_state().preview);
  viewer_.RefreshRomBackedState(&rom_, nullptr, &rooms_, 1);
  DrawFrame();
  EXPECT_FALSE(viewer_.room_transfer_state().preview);
  EXPECT_FALSE(viewer_.room_transfer_state().copy_destinations);
  EXPECT_EQ(viewer_.room_transfer_state().room_id, 1);
  EXPECT_EQ(viewer_.room_transfer_state().source_room_id, 0);
}

TEST_F(DungeonRoomTransferEditorTest, ContextMenuOpensPersistentSharedDialog) {
  auto menu = DungeonRoomTransferEditorTestPeer::RoomMenu(viewer_, 0);
  bool invoked = false;
  for (auto& item : menu.subitems) {
    if (item.label == "Clone / Import Room...") {
      ASSERT_TRUE(item.callback);
      item.callback();
      invoked = true;
    }
  }
  ASSERT_TRUE(invoked);
  EXPECT_TRUE(viewer_.room_transfer_state().request_popup);
  popup_ = true;
  Prepare();
  EXPECT_TRUE(viewer_.room_transfer_state().popup_open);
  ASSERT_TRUE(Widget("TransferPreview"));
  DrawFrame();
  EXPECT_TRUE(viewer_.room_transfer_state().popup_open);
  EXPECT_EQ(previews_, 0);
}

TEST_F(DungeonRoomTransferEditorTest, NarrowInspectorContainsPreviewAndStatus) {
  width_ = 260;
  Prepare();
  Click("TransferPreview");
  ASSERT_TRUE(viewer_.room_transfer_state().preview);
  EXPECT_NE(logged_text_.find("Preview for room"), std::string::npos);
}

TEST_F(DungeonRoomTransferEditorTest, MismatchedCanvasContextRejectsCommands) {
  Prepare();
  Click("TransferPreview");
  const auto plan = viewer_.room_transfer_state().preview;
  ASSERT_TRUE(plan);
  viewer_.object_interaction().SetCurrentRoom(&rooms_, 1);
  EXPECT_FALSE(viewer_.ApplyRoomTransfer(*plan).ok());
  EXPECT_FALSE(viewer_.PreviewRoomTransfer(1, "", {}).ok());
  EXPECT_EQ(attempts_, 0);
}

}  // namespace
}  // namespace yaze::editor
