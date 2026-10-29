#include "app/editor/overworld/scratch_workspace.h"

#include "app/editor/overworld/overworld_editor.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {

class OverworldScratchTestPeer {
 public:
  static absl::Status Load(OverworldEditor& editor,
                           const std::filesystem::path& path) {
    const auto status = editor.scratch_workspace_.Load(path);
    if (status.ok()) {
      editor.scratch_path_ = path;
      editor.scratch_initialized_ = true;
    }
    return status;
  }
  static absl::Status Capture(OverworldEditor& editor, const TileBrush& brush) {
    editor.BeginScratchEdit();
    const auto status = editor.scratch_workspace_.Capture(brush, 0x12);
    editor.FinishScratchEdit("Capture scratch test layout");
    return status;
  }
  static absl::Status CopyBrush(OverworldEditor& editor) {
    return editor.LoadScratchToSelection();
  }
  static const ScratchWorkspaceState& State(const OverworldEditor& editor) {
    return editor.scratch_workspace_.state();
  }
  static absl::Status BuildPreview(OverworldEditor& editor) {
    return editor.RebuildScratchBitmapFromTileData();
  }
  static const gfx::Bitmap& Preview(const OverworldEditor& editor) {
    return editor.scratch_bitmap_;
  }
  static ImVec2 CanvasSize(const OverworldEditor& editor) {
    return editor.scratch_canvas_.canvas_size();
  }
  static absl::Status PrepareUi(OverworldEditor& editor) {
    editor.map_blockset_loaded_ = true;
    editor.tile16_blockset_.tile_size = {16, 16};
    editor.tile16_blockset_.map_size = {8, 2};
    std::vector<uint8_t> pixels(128 * 32);
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 128; ++x)
        pixels[y * 128 + x] = (y / 16) * 8 + x / 16;
    editor.tile16_blockset_.atlas.Create(128, 32, 8, pixels);
    gfx::SnesPalette palette;
    for (int i = 0; i < 16; ++i)
      palette.AddColor(gfx::SnesColor(i * 8, i * 8, i * 8));
    editor.tile16_blockset_.atlas.SetPalette(palette);
    editor.current_tile16_ = 7;
    editor.scratch_canvas_.set_global_scale(1.5f);
    editor.scratch_canvas_.set_scrolling(ImVec2(-16, -8));
    return editor.scratch_workspace_.Resize(4, 2);
  }
  static absl::Status Draw(OverworldEditor& editor) {
    return editor.DrawScratchSpace();
  }
  static ImVec2 CellCenter(OverworldEditor& editor, int x, int y) {
    const auto& canvas = editor.scratch_canvas_;
    const auto origin = canvas.zero_point();
    const auto scroll = canvas.scrolling();
    const float scale = canvas.GetGlobalScale();
    return ImVec2(origin.x + scroll.x + 2 + (x + 0.5f) * 16 * scale,
                  origin.y + scroll.y + 2 + (y + 0.5f) * 16 * scale);
  }
  static int CurrentTile(const OverworldEditor& editor) {
    return editor.current_tile16_;
  }
  static void SetCurrentTile(OverworldEditor& editor, int id) {
    editor.current_tile16_ = id;
  }
  static gfx::Bitmap& Atlas(OverworldEditor& editor) {
    return editor.tile16_blockset_.atlas;
  }
  static const TileBrush* SelectionBrush(const OverworldEditor& editor) {
    return editor.selection_brush();
  }
  static bool ScratchSelecting(const OverworldEditor& editor) {
    return editor.scratch_selecting_;
  }
  static bool ScratchSelectActive(const OverworldEditor& editor) {
    return editor.scratch_select_active_;
  }
  static ImVec2 ScratchSelectMin(const OverworldEditor& editor) {
    return editor.scratch_select_min_;
  }
  static ImVec2 ScratchSelectMax(const OverworldEditor& editor) {
    return editor.scratch_select_max_;
  }
};

namespace {

TEST(ScratchWorkspaceTest, CaptureKeepsOtherPagesAndCopiesRowMajorBrush) {
  ScratchWorkspace workspace;
  ASSERT_TRUE(workspace.Stamp({1, 1, {10}}, 0, 0).ok());
  const uint64_t original = workspace.current().id;
  ASSERT_TRUE(workspace.Capture({3, 2, {1, 2, 3, 4, 5, 6}}, 0x23).ok());
  EXPECT_NE(workspace.current().id, original);
  EXPECT_EQ(workspace.state().pages.size(), 2);
  EXPECT_EQ(workspace.current().source_map, 0x23);
  const auto brush = workspace.current().AsBrush();
  EXPECT_EQ(brush.width, 3);
  EXPECT_EQ(brush.height, 2);
  EXPECT_EQ(brush.tile_ids, (std::vector<int>{1, 2, 3, 4, 5, 6}));
  ASSERT_TRUE(workspace.Select(original));
  EXPECT_EQ(workspace.current().tile_data[0][0], 10);
}

TEST(ScratchWorkspaceTest, DuplicateIsIndependentAndUndoDoesNotRecycleIds) {
  ScratchWorkspace workspace;
  ASSERT_TRUE(workspace.Stamp({1, 1, {22}}, 0, 0).ok());
  const auto before = workspace.state();
  ASSERT_TRUE(workspace.AddPage(true).ok());
  const auto copied_id = workspace.current().id;
  ASSERT_TRUE(workspace.Stamp({1, 1, {33}}, 0, 0).ok());
  EXPECT_EQ(workspace.state().pages.front().tile_data[0][0], 22);
  workspace.Restore(before);
  ASSERT_TRUE(workspace.AddPage().ok());
  EXPECT_GT(workspace.current().id, copied_id);
}

TEST(ScratchWorkspaceTest, RemovingPagePreservesOtherIdsAndUndoRestoresIt) {
  ScratchWorkspace workspace;
  EXPECT_FALSE(workspace.DeletePage().ok());
  ASSERT_TRUE(workspace.AddPage().ok());
  const uint64_t removed = workspace.current().id;
  const auto before = workspace.state();
  ASSERT_TRUE(workspace.DeletePage().ok());
  EXPECT_EQ(workspace.current().id, 1);
  EXPECT_EQ(workspace.state().pages.size(), 1);
  workspace.Restore(before);
  EXPECT_EQ(workspace.current().id, removed);
  EXPECT_EQ(workspace.state().pages.size(), 2);
}

TEST(ScratchWorkspaceTest, StampClipsAtPageEdgeWithoutChangingSourceStride) {
  ScratchWorkspace workspace;
  ASSERT_TRUE(workspace.Resize(2, 3).ok());
  ASSERT_TRUE(workspace.Stamp({3, 2, {1, 2, 3, 4, 5, 6}}, 1, 1).ok());
  EXPECT_EQ(workspace.current().tile_data[1][1], 1);
  EXPECT_EQ(workspace.current().tile_data[1][2], 4);
  EXPECT_EQ(workspace.current().tile_data[0][2], 0);
  const auto before = workspace.state();
  EXPECT_FALSE(workspace.Stamp({1, 1, {9}}, -1, 1).ok());
  EXPECT_FALSE(workspace.Stamp({2, 2, {9}}, 0, 0).ok());
  EXPECT_TRUE(workspace.state() == before);
}

TEST(ScratchWorkspaceTest, ResizeRoundTripPreservesTemporarilyHiddenCells) {
  ScratchWorkspace workspace;
  ASSERT_TRUE(workspace.Stamp({1, 1, {0xABC}}, 15, 15).ok());
  ASSERT_TRUE(workspace.Resize(1, 1).ok());
  ASSERT_TRUE(workspace.Rename("Unplaced forest shrine").ok());
  ScratchWorkspace reopened;
  ASSERT_TRUE(reopened.Parse(workspace.Serialize()).ok());
  ASSERT_TRUE(reopened.Resize(16, 16).ok());
  EXPECT_EQ(reopened.current().name, "Unplaced forest shrine");
  EXPECT_EQ(reopened.current().tile_data[15][15], 0xABC);
}

TEST(ScratchWorkspaceTest, ReadsLegacySinglePageWithoutReplacingIt) {
  ScratchWorkspace workspace;
  ASSERT_TRUE(workspace
                  .Parse(R"({"version":1,"name":"Old plan",
    "in_use":true,"width":2,"height":2,"tiles":[[7,8],[9,10]]})")
                  .ok());
  EXPECT_EQ(workspace.current().name, "Old plan");
  EXPECT_EQ(workspace.current().AsBrush().tile_ids,
            (std::vector<int>{7, 8, 9, 10}));
  const auto serialized = nlohmann::json::parse(workspace.Serialize());
  EXPECT_EQ(serialized["version"], 2);
  EXPECT_EQ(serialized["pages"].size(), 1);
}

TEST(ScratchWorkspaceTest, RejectsMalformedDataWithoutChangingWorkspace) {
  ScratchWorkspace workspace;
  ASSERT_TRUE(workspace.Stamp({1, 1, {13}}, 0, 0).ok());
  const auto before = workspace.state();
  for (const auto* contents :
       {R"({"version":1,"name":55,"width":1,"height":1,"tiles":[[1]]})",
        R"({"version":1,"width":0,"height":1,"tiles":[[1]]})",
        R"({"version":1,"width":2,"height":1,"tiles":[[1]]})",
        R"({"version":1,"width":1,"height":1,"tiles":[[-1]]})",
        R"({"version":1,"width":1,"height":1,"tiles":[[65536]]})",
        R"({"version":1,"width":1,"height":1,"tiles":[["1"]]})",
        R"({"version":7,"tiles":[]})"}) {
    EXPECT_FALSE(workspace.Parse(contents).ok()) << contents;
    EXPECT_TRUE(workspace.state() == before);
  }
  auto valid = nlohmann::json::parse(workspace.Serialize());
  valid["pages"].push_back(valid["pages"][0]);
  EXPECT_FALSE(workspace.Parse(valid.dump()).ok());
  EXPECT_TRUE(workspace.state() == before);
}

TEST(ScratchWorkspaceTest, BoundsPageCreationAndMetadataBeforeMutation) {
  ScratchWorkspace workspace;
  EXPECT_FALSE(workspace.Resize(33, 1).ok());
  EXPECT_FALSE(workspace.Rename("").ok());
  EXPECT_FALSE(workspace.Capture({33, 1, std::vector<int>(33, 1)}, 0).ok());
  EXPECT_EQ(workspace.state().pages.size(), 1);
  for (size_t i = 1; i < ScratchWorkspace::kMaxPages; ++i)
    ASSERT_TRUE(workspace.AddPage().ok());
  const auto before = workspace.state();
  EXPECT_FALSE(workspace.AddPage().ok());
  EXPECT_TRUE(workspace.state() == before);
}

class ScratchWorkspaceFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static std::atomic<int> sequence{0};
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() /
                 ("yaze-scratch-test-" + std::to_string(stamp) + "-" +
                  std::to_string(sequence++));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
    path_ = directory_ / "ScratchPad.dat";
  }
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
  }
  std::string Read() const {
    std::ifstream input(path_, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), {});
  }
  void Write(const std::string& contents) const {
    std::ofstream output(path_, std::ios::binary);
    output << contents;
  }
  std::filesystem::path directory_;
  std::filesystem::path path_;
};

TEST_F(ScratchWorkspaceFileTest, SavesAndReopensMultiplePagesAtomically) {
  ScratchWorkspace workspace;
  ASSERT_TRUE(workspace.Load(path_).ok());
  ASSERT_TRUE(workspace.Capture({2, 1, {0x88, 0x99}}, 0x10).ok());
  ASSERT_TRUE(workspace.Save(path_).ok());
  ScratchWorkspace reopened;
  ASSERT_TRUE(reopened.Load(path_).ok());
  EXPECT_TRUE(reopened.state() == workspace.state());
  std::vector<std::filesystem::path> artifacts;
  for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
    if (entry.path().filename() != core::kSourceArtifactPublicationLockBasename)
      artifacts.push_back(entry.path().filename());
  }
  EXPECT_EQ(artifacts, (std::vector<std::filesystem::path>{path_.filename()}));
}

TEST_F(ScratchWorkspaceFileTest, FailedLoadCannotOverwriteRecoveryFile) {
  const std::string original = "invalid scratch recovery data";
  Write(original);
  ScratchWorkspace workspace;
  EXPECT_FALSE(workspace.Load(path_).ok());
  ASSERT_TRUE(workspace.AddPage().ok());
  EXPECT_FALSE(workspace.Save(path_).ok());
  EXPECT_EQ(Read(), original);
}

TEST_F(ScratchWorkspaceFileTest, ConcurrentSessionDoesNotOverwriteSavedPages) {
  ScratchWorkspace first;
  ScratchWorkspace second;
  ASSERT_TRUE(first.Load(path_).ok());
  ASSERT_TRUE(second.Load(path_).ok());
  ASSERT_TRUE(second.Capture({1, 1, {9}}, 3).ok());
  ASSERT_TRUE(second.Save(path_).ok());
  const auto other_contents = Read();
  ASSERT_TRUE(first.Capture({1, 1, {5}}, 4).ok());
  EXPECT_EQ(first.Save(path_).code(), absl::StatusCode::kAborted);
  EXPECT_EQ(first.current().tile_data[0][0], 5);
  EXPECT_EQ(Read(), other_contents);
}

TEST_F(ScratchWorkspaceFileTest,
       EditorCopiesRealClipboardAndReplaysUndoHistory) {
  SharedClipboard clipboard;
  EditorDependencies dependencies;
  dependencies.shared_clipboard = &clipboard;
  OverworldEditor editor(nullptr, dependencies);
  ASSERT_TRUE(OverworldScratchTestPeer::Load(editor, path_).ok());
  ASSERT_TRUE(
      OverworldScratchTestPeer::Capture(editor, {3, 2, {1, 2, 3, 4, 5, 6}})
          .ok());
  EXPECT_TRUE(editor.HasPendingProjectDraftChanges());
  ASSERT_TRUE(OverworldScratchTestPeer::CopyBrush(editor).ok());
  EXPECT_TRUE(clipboard.has_overworld_tile16);
  EXPECT_EQ(clipboard.overworld_width, 3);
  EXPECT_EQ(clipboard.overworld_height, 2);
  EXPECT_EQ(clipboard.overworld_tile16_ids,
            (std::vector<int>{1, 2, 3, 4, 5, 6}));
  ASSERT_TRUE(OverworldScratchTestPeer::BuildPreview(editor).ok());
  EXPECT_EQ(OverworldScratchTestPeer::Preview(editor).width(), 48);
  EXPECT_EQ(OverworldScratchTestPeer::Preview(editor).height(), 32);
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_EQ(OverworldScratchTestPeer::State(editor).pages.size(), 1);
  EXPECT_FALSE(editor.HasPendingProjectDraftChanges());
  ASSERT_TRUE(editor.Redo().ok());
  EXPECT_EQ(OverworldScratchTestPeer::State(editor).pages.size(), 2);
  ScratchWorkspace reopened;
  ASSERT_TRUE(reopened.Load(path_).ok());
  EXPECT_EQ(reopened.current().AsBrush().tile_ids,
            (std::vector<int>{1, 2, 3, 4, 5, 6}));
}

TEST_F(ScratchWorkspaceFileTest,
       FailedEditorAutosaveProtectsCloseAndNormalSave) {
  OverworldEditor editor(nullptr);
  ASSERT_TRUE(OverworldScratchTestPeer::Load(editor, path_).ok());
  ScratchWorkspace other;
  ASSERT_TRUE(other.Load(path_).ok());
  ASSERT_TRUE(other.Rename("Other session").ok());
  ASSERT_TRUE(other.Save(path_).ok());
  const auto other_contents = Read();
  ASSERT_TRUE(OverworldScratchTestPeer::Capture(editor, {1, 1, {45}}).ok());
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_TRUE(editor.HasPendingProjectDraftChanges());
  EXPECT_EQ(editor.Save().code(), absl::StatusCode::kAborted);
  EXPECT_EQ(Read(), other_contents);
  ASSERT_TRUE(editor.Redo().ok());
  EXPECT_EQ(OverworldScratchTestPeer::State(editor).pages.size(), 2);
  EXPECT_TRUE(editor.HasPendingProjectDraftChanges());
}

class ScratchWorkspaceUiTest : public ScratchWorkspaceFileTest {
 protected:
  void SetUp() override {
    ScratchWorkspaceFileTest::SetUp();
    gfx::Arena::Get().ClearTextureQueue();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1200, 1000);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigInputTrickleEventQueue = false;
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    editor_ = std::make_unique<OverworldEditor>(nullptr);
    ASSERT_TRUE(OverworldScratchTestPeer::Load(*editor_, path_).ok());
    ASSERT_TRUE(OverworldScratchTestPeer::PrepareUi(*editor_).ok());
    Frame(ImVec2(-100, -100));
    Frame(ImVec2(-100, -100));
  }
  void TearDown() override {
    gfx::Arena::Get().ClearTextureQueue();
    editor_.reset();
    ImGui::DestroyContext();
    ScratchWorkspaceFileTest::TearDown();
  }
  void Frame(ImVec2 mouse, bool left = false, bool right = false) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(mouse.x, mouse.y);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, left);
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, right);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1100, 950), ImGuiCond_Always);
    ImGui::Begin("ScratchMouseHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    EXPECT_TRUE(OverworldScratchTestPeer::Draw(*editor_).ok());
    ImGui::End();
    ImGui::Render();
  }
  void ActivateToolbar(const char* label) {
    auto* window = ImGui::FindWindowByName("ScratchMouseHost");
    ASSERT_NE(window, nullptr);
    // Queue activation of the actual toolbar button, then let its callback and
    // the rest of DrawScratchSpace execute in the same ImGui frame.
    ImGui::ActivateItemByID(window->GetID(label));
    Frame(ImVec2(-100, -100));
  }
  std::unique_ptr<OverworldEditor> editor_;
};

TEST_F(ScratchWorkspaceUiTest, ToolbarLayoutChangesUseCurrentPageInSameFrame) {
  const uint64_t first_id =
      OverworldScratchTestPeer::State(*editor_).selected_id;
  EXPECT_EQ(OverworldScratchTestPeer::CanvasSize(*editor_).x, 68);
  EXPECT_EQ(OverworldScratchTestPeer::CanvasSize(*editor_).y, 36);

  // New layout grows the page vector and selects a default 16x16 page. Its
  // preview and canvas must both use that page before this frame finishes.
  ActivateToolbar(ICON_MD_ADD "##ScratchNew");
  ASSERT_EQ(OverworldScratchTestPeer::State(*editor_).pages.size(), 2);
  const uint64_t second_id =
      OverworldScratchTestPeer::State(*editor_).selected_id;
  EXPECT_NE(second_id, first_id);
  EXPECT_EQ(OverworldScratchTestPeer::Preview(*editor_).width(), 256);
  EXPECT_EQ(OverworldScratchTestPeer::Preview(*editor_).height(), 256);
  EXPECT_EQ(OverworldScratchTestPeer::CanvasSize(*editor_).x, 260);
  EXPECT_EQ(OverworldScratchTestPeer::CanvasSize(*editor_).y, 260);

  ActivateToolbar(ICON_MD_CHEVRON_LEFT "##ScratchPrev");
  EXPECT_EQ(OverworldScratchTestPeer::State(*editor_).selected_id, first_id);
  EXPECT_EQ(OverworldScratchTestPeer::CanvasSize(*editor_).x, 68);
  EXPECT_EQ(OverworldScratchTestPeer::CanvasSize(*editor_).y, 36);

  ActivateToolbar(ICON_MD_CHEVRON_RIGHT "##ScratchNext");
  EXPECT_EQ(OverworldScratchTestPeer::State(*editor_).selected_id, second_id);
  EXPECT_EQ(OverworldScratchTestPeer::CanvasSize(*editor_).x, 260);
  EXPECT_EQ(OverworldScratchTestPeer::CanvasSize(*editor_).y, 260);
}

TEST_F(ScratchWorkspaceUiTest,
       RealMousePaintDragAndPickUseScrolledScaledCanvas) {
  auto first = OverworldScratchTestPeer::CellCenter(*editor_, 1, 0);
  Frame(first);
  Frame(first, true);
  auto second = OverworldScratchTestPeer::CellCenter(*editor_, 2, 0);
  Frame(second, true);
  Frame(second);
  const auto& page = OverworldScratchTestPeer::State(*editor_).pages.front();
  EXPECT_EQ(page.tile_data[1][0], 7);
  EXPECT_EQ(page.tile_data[2][0], 7);
  EXPECT_EQ(page.tile_data[0][0], 0);
  OverworldScratchTestPeer::SetCurrentTile(*editor_, 3);
  Frame(second, false, true);
  Frame(second);
  EXPECT_EQ(OverworldScratchTestPeer::CurrentTile(*editor_), 7);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(
      OverworldScratchTestPeer::State(*editor_).pages.front().tile_data[1][0],
      0);
  EXPECT_EQ(
      OverworldScratchTestPeer::State(*editor_).pages.front().tile_data[2][0],
      0);
}

TEST_F(ScratchWorkspaceUiTest,
       PixelAndPaletteChangesRefreshWithoutAtlasCreate) {
  ASSERT_TRUE(OverworldScratchTestPeer::Capture(*editor_, {1, 1, {7}}).ok());
  Frame(ImVec2(-100, -100));
  EXPECT_EQ(OverworldScratchTestPeer::Preview(*editor_).vector()[0], 7);
  auto& atlas = OverworldScratchTestPeer::Atlas(*editor_);
  const auto generation = atlas.generation();
  auto pixels = atlas.vector();
  for (int y = 0; y < 16; ++y)
    for (int x = 112; x < 128; ++x)
      pixels[y * 128 + x] = 5;
  atlas.set_data(pixels);
  auto palette = atlas.palette();
  palette[5].set_snes(0x001F);
  atlas.SetPalette(palette);
  ASSERT_EQ(atlas.generation(), generation);
  Frame(ImVec2(-100, -100));
  EXPECT_EQ(OverworldScratchTestPeer::Preview(*editor_).vector()[0], 5);
  EXPECT_EQ(OverworldScratchTestPeer::Preview(*editor_).palette()[5].snes(),
            0x001F);
}

TEST_F(ScratchWorkspaceUiTest, RightClickDragReleasesOutsideCanvasCleanly) {
  auto start = OverworldScratchTestPeer::CellCenter(*editor_, 0, 0);
  Frame(start, false, true);
  EXPECT_TRUE(OverworldScratchTestPeer::ScratchSelecting(*editor_));

  Frame(ImVec2(1500, 1500), false, true);
  EXPECT_TRUE(OverworldScratchTestPeer::ScratchSelecting(*editor_));
  Frame(ImVec2(1500, 1500), false, false);
  EXPECT_FALSE(OverworldScratchTestPeer::ScratchSelecting(*editor_));
  EXPECT_TRUE(OverworldScratchTestPeer::ScratchSelectActive(*editor_));
  EXPECT_EQ(OverworldScratchTestPeer::ScratchSelectMin(*editor_).x, 0);
  EXPECT_EQ(OverworldScratchTestPeer::ScratchSelectMin(*editor_).y, 0);
  EXPECT_EQ(OverworldScratchTestPeer::ScratchSelectMax(*editor_).x, 3);
  EXPECT_EQ(OverworldScratchTestPeer::ScratchSelectMax(*editor_).y, 1);
}

TEST_F(ScratchWorkspaceFileTest, RemovedFileAndNonFileTargetsRemainProtected) {
  ScratchWorkspace workspace;
  ASSERT_TRUE(workspace.Load(path_).ok());
  ASSERT_TRUE(workspace.Save(path_).ok());
  ASSERT_TRUE(std::filesystem::remove(path_));
  EXPECT_EQ(workspace.Save(path_).code(), absl::StatusCode::kAborted);
  ASSERT_TRUE(std::filesystem::create_directory(path_));
  EXPECT_FALSE(workspace.Load(path_).ok());
  EXPECT_FALSE(workspace.Save(path_).ok());
  EXPECT_TRUE(std::filesystem::is_directory(path_));
}

}  // namespace
}  // namespace yaze::editor
