#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/dungeon_entrance_camera.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_render_context.h"
#include "app/editor/dungeon/workspace/room_matrix_census.h"
#include "app/editor/dungeon/workspace/room_matrix_content.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "app/gui/automation/widget_id_registry.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "imgui/imgui_internal.h"
#include "unique_temp_path.h"
#include "unit/zelda3/dungeon/room_census_test_rom.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"
#include "zelda3/dungeon/room_census.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze::editor {

class RoomMatrixCensusTestPeer {
 public:
  static bool DrawChip(RoomMatrixCensusOverlay& overlay, const char* id,
                       const std::string& label, bool first,
                       bool outline = false) {
    return overlay.DrawChip(id, label, ImVec4(0.3f, 0.6f, 0.8f, 1.0f), outline,
                            false, first);
  }
  static util::StopToken NewToken(RoomMatrixCensusOverlay& overlay) {
    overlay.stop_source_ = util::StopSource{};
    return overlay.stop_source_.get_token();
  }
  static util::StopToken Token(RoomMatrixCensusOverlay& overlay) {
    return overlay.stop_source_.get_token();
  }
  static void SetPending(
      RoomMatrixCensusOverlay& overlay,
      std::future<absl::StatusOr<zelda3::RoomCensus>> pending) {
    overlay.pending_ = std::move(pending);
    overlay.stale_ = false;
  }
  static std::string Error(const RoomMatrixCensusOverlay& overlay) {
    return overlay.error_;
  }
  static bool HasCachedCensus(const RoomMatrixCensusOverlay& overlay) {
    return overlay.census_.has_value();
  }
  static bool WaitForPending(RoomMatrixCensusOverlay& overlay) {
    return overlay.pending_.valid() &&
           overlay.pending_.wait_for(std::chrono::seconds(60)) ==
               std::future_status::ready;
  }
};

class DungeonEditorV2EntranceCameraTestPeer {
 public:
  static util::StopToken NewToken(DungeonEditorV2& editor) {
    editor.dungeon_render_context_census_stop_ = util::StopSource{};
    return editor.dungeon_render_context_census_stop_.get_token();
  }
  static util::StopToken Token(DungeonEditorV2& editor) {
    return editor.dungeon_render_context_census_stop_.get_token();
  }
  static void SetPending(
      DungeonEditorV2& editor,
      std::future<absl::StatusOr<zelda3::RoomCensus>> pending) {
    editor.dungeon_render_context_census_pending_ = std::move(pending);
    editor.dungeon_render_context_pending_generation_ =
        editor.dungeon_render_context_census_generation_;
  }
  static void AttachMatrix(DungeonEditorV2& editor, RoomMatrixContent* matrix) {
    editor.room_matrix_panel_ = matrix;
  }
  static bool Shutdown(const DungeonEditorV2& editor) {
    return editor.dungeon_render_context_census_shutdown_;
  }
  static const zelda3::RoomCensus* Census(DungeonEditorV2& editor) {
    return editor.GetDungeonRenderContextCensus();
  }
  static bool WaitForPendingCensus(DungeonEditorV2& editor) {
    return editor.dungeon_render_context_census_pending_.valid() &&
           editor.dungeon_render_context_census_pending_.wait_for(
               std::chrono::seconds(60)) == std::future_status::ready;
  }
  static auto TakePendingCensus(DungeonEditorV2& editor) {
    return std::move(editor.dungeon_render_context_census_pending_);
  }
  static void SynchronizeAssets(DungeonEditorV2& editor) {
    editor.SynchronizeCustomObjectAssets();
  }
  static bool HasCachedCensus(const DungeonEditorV2& editor) {
    return editor.dungeon_render_context_census_.has_value();
  }
  static void SetRegularEntrance(DungeonEditorV2& editor, int slot_index,
                                 const DungeonEntranceCameraState& state) {
    auto& entrance = editor.entrances_[slot_index];
    entrance.room_ = static_cast<int16_t>(state.room_id);
    entrance.x_position_ = state.player_x;
    entrance.y_position_ = state.player_y;
    ApplyDungeonEntranceCameraState(state, &entrance);
  }

  static DungeonEntranceCameraState RegularEntrance(
      const DungeonEditorV2& editor, int slot_index) {
    return CaptureDungeonEntranceCamera(editor.entrances_[slot_index]);
  }

  static void SetSpawnPoint(DungeonEditorV2& editor, int slot_index,
                            zelda3::DungeonSpawnPoint spawn) {
    editor.spawn_points_[slot_index] = std::move(spawn);
  }

  static DungeonEntranceCameraState SpawnPoint(const DungeonEditorV2& editor,
                                               int slot_index) {
    return CaptureDungeonSpawnCamera(editor.spawn_points_[slot_index]);
  }

  static bool LegacyEntranceDirty(const DungeonEditorV2& editor,
                                  int slot_index) {
    return editor.entrances_[slot_index].dirty();
  }

  static void SetRenderContextCensus(DungeonEditorV2& editor,
                                     zelda3::RoomCensus census) {
    editor.dungeon_render_context_census_ = std::move(census);
    editor.dungeon_render_context_census_attempted_ = true;
  }

  static void ClearSelectedEntrance(DungeonEditorV2& editor) {
    editor.current_entrance_id_ = -1;
  }

  static void SetSelectedEntranceGraphics(DungeonEditorV2& editor,
                                          int slot_index, int room_id,
                                          uint8_t main_gfx) {
    editor.current_entrance_id_ = slot_index;
    editor.automatic_entrance_preview_ = false;
    editor.entrances_[slot_index].room_ = static_cast<int16_t>(room_id);
    editor.entrances_[slot_index].blockset_ = main_gfx;
  }

  static void ConfigureViewer(DungeonEditorV2& editor,
                              DungeonCanvasViewer& viewer, int room_id) {
    editor.ConfigureViewerRenderContext(&viewer, room_id);
  }

  static void MaterializeCurrentRoom(DungeonEditorV2& editor, int room_id,
                                     uint8_t room_blockset) {
    editor.current_room_id_ = room_id;
    editor.rooms_[room_id].SetBlockset(room_blockset);
  }

  static absl::Status Repair(DungeonEditorV2& editor, int slot_index) {
    return editor.RepairEntranceCamera(slot_index);
  }
};

namespace {

class RoomMatrixCensusChipTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(900, 600);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.AddMousePosEvent(-1000, -1000);
    ImGui::GetStyle().WindowMinSize = ImVec2(1, 1);
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
    gui::WidgetIdRegistry::Instance().Clear();
  }

  void DrawFrame(const std::function<void()>& draw) {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(content_width_ + ImGui::GetStyle().WindowPadding.x * 2, 200),
        ImGuiCond_Always);
    ImGui::Begin("CensusChipHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoScrollbar);
    content_right_ =
        ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    ImGui::LogToBuffer();
    draw();
    logged_text_ = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    EXPECT_EQ(context_->ColorStack.Size, 0);
    EXPECT_EQ(context_->StyleVarStack.Size, 0);
  }

  float NaturalWidth(const std::string& label) const {
    const auto& style = ImGui::GetStyle();
    return style.FramePadding.x * 2 + ImGui::GetTextLineHeight() - 2 +
           style.ItemInnerSpacing.x + ImGui::CalcTextSize(label.c_str()).x;
  }

  RoomMatrixCensusOverlay overlay_;
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  float content_width_ = 180;
  float content_right_ = 0;
  std::string logged_text_;
};

TEST_F(RoomMatrixCensusChipTest, NarrowRowsBoundHitboxAndDrawClip) {
  for (float width : {1.0f, 4.0f, 16.0f, 32.0f, 80.0f, 180.0f}) {
    content_width_ = width;
    for (bool outline : {false, true}) {
      DrawFrame([&] {
        auto* draw = ImGui::GetWindowDrawList();
        const int first_index = draw->IdxBuffer.Size;
        EXPECT_FALSE(RoomMatrixCensusTestPeer::DrawChip(
            overlay_, "##chip_owner_0", "Dungeon ownership (128)", true,
            outline));
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        EXPECT_GT(max.x, min.x);
        EXPECT_LE(max.x, content_right_ + 0.01f);
        EXPECT_LE(max.x - min.x, width + 0.01f);
        int index = 0;
        bool saw_chip = false;
        for (const auto& command : draw->CmdBuffer) {
          if (index + static_cast<int>(command.ElemCount) > first_index &&
              command.ElemCount != 0) {
            saw_chip = true;
            EXPECT_GE(command.ClipRect.x, min.x - 0.01f);
            EXPECT_GE(command.ClipRect.y, min.y - 0.01f);
            EXPECT_LE(command.ClipRect.z, max.x + 0.01f);
            EXPECT_LE(command.ClipRect.w, max.y + 0.01f);
          }
          index += static_cast<int>(command.ElemCount);
        }
        EXPECT_TRUE(saw_chip);
      });
    }
  }
}

TEST_F(RoomMatrixCensusChipTest, NoEmptyRowSpaceProducesNoHitbox) {
  content_width_ = 0;
  DrawFrame([&] {
    const int vertices = ImGui::GetWindowDrawList()->VtxBuffer.Size;
    EXPECT_FALSE(RoomMatrixCensusTestPeer::DrawChip(overlay_, "##chip_free",
                                                    "Free (12)", true));
    EXPECT_EQ(ImGui::GetWindowDrawList()->VtxBuffer.Size, vertices);
    EXPECT_TRUE(gui::WidgetIdRegistry::Instance().GetAllWidgets().empty());
  });
}

TEST_F(RoomMatrixCensusChipTest, WrapUsesFullEmptyRowWidth) {
  content_width_ = 160;
  DrawFrame([&] {
    RoomMatrixCensusTestPeer::DrawChip(overlay_, "##chip_free", "Free", true);
    const ImVec2 first_max = ImGui::GetItemRectMax();
    const std::string second_label = "Reclaimable (12)";
    ASSERT_LT(NaturalWidth(second_label), content_width_);
    RoomMatrixCensusTestPeer::DrawChip(overlay_, "##chip_reclaimable",
                                       second_label, false);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    EXPECT_GT(min.y, first_max.y);
    EXPECT_NEAR(max.x - min.x, NaturalWidth(second_label), 0.01f);
    EXPECT_LE(max.x, content_right_ + 0.01f);
  });
}

TEST_F(RoomMatrixCensusChipTest, IndentedRowKeepsChipWithinContentBounds) {
  content_width_ = 80;
  DrawFrame([&] {
    ImGui::Indent(20);
    const float row_left = ImGui::GetCursorScreenPos().x;
    RoomMatrixCensusTestPeer::DrawChip(overlay_, "##chip_owner_0",
                                       "Long dungeon owner (128)", true);
    EXPECT_NEAR(ImGui::GetItemRectMin().x, row_left, 0.01f);
    EXPECT_LE(ImGui::GetItemRectMax().x, content_right_ + 0.01f);
    ImGui::Unindent(20);
  });
}

TEST_F(RoomMatrixCensusChipTest,
       Utf8EllipsisRetainsFullTooltipAndClickIdentity) {
  content_width_ = 64;
  const std::string label = "Café 洞窟 (128)";
  ImVec2 min, max;
  ImGuiID expected_id = 0;
  int clicks = 0;
  const auto draw = [&] {
    expected_id = ImGui::GetID("##chip_owner_0");
    if (RoomMatrixCensusTestPeer::DrawChip(overlay_, "##chip_owner_0", label,
                                           true)) {
      ++clicks;
      overlay_.ToggleOwnerFilter(0);
    }
    // Tooltip drawing must retain the chip as the last item.
    EXPECT_EQ(ImGui::GetItemID(), expected_id);
    min = ImGui::GetItemRectMin();
    max = ImGui::GetItemRectMax();
  };
  DrawFrame(draw);
  DrawFrame(draw);
  EXPECT_NE(logged_text_.find(label), std::string::npos);
  const auto widgets = gui::WidgetIdRegistry::Instance().GetAllWidgets();
  ASSERT_EQ(widgets.size(), 1);
  EXPECT_EQ(widgets.begin()->second.imgui_id, expected_id);
  EXPECT_EQ(widgets.begin()->second.label,
            gui::WidgetIdRegistry::NormalizeLabel("Census chip chip_owner_0"));

  auto& io = ImGui::GetIO();
  io.AddMousePosEvent((min.x + max.x) / 2, (min.y + max.y) / 2);
  DrawFrame(draw);
  // The full UTF-8 label is rendered once for logging and again in the tooltip.
  const auto first = logged_text_.find(label);
  ASSERT_NE(first, std::string::npos);
  EXPECT_NE(logged_text_.find(label, first + label.size()), std::string::npos);
  io.AddMouseButtonEvent(0, true);
  DrawFrame(draw);
  io.AddMouseButtonEvent(0, false);
  DrawFrame(draw);
  EXPECT_EQ(clicks, 1);
  EXPECT_TRUE(overlay_.HasActiveChips());
}

zelda3::RoomCensus MakeCensus(
    const std::vector<std::vector<int>>& owner_rooms) {
  zelda3::RoomCensus census;
  census.rooms.resize(zelda3::kRoomCensusRoomCount);
  for (int room_id = 0; room_id < zelda3::kRoomCensusRoomCount; ++room_id) {
    census.rooms[room_id].room_id = room_id;
  }
  for (size_t owner_index = 0; owner_index < owner_rooms.size();
       ++owner_index) {
    census.owners.push_back({
        .id = "D" + std::to_string(owner_index + 1),
        .name = "Dungeon " + std::to_string(owner_index + 1),
    });
    for (const int room_id : owner_rooms[owner_index]) {
      census.rooms[room_id].owner_index = static_cast<int>(owner_index);
    }
  }
  return census;
}

DungeonEntranceCameraState StateFromDerived(int room_id, uint16_t player_x,
                                            uint16_t player_y) {
  const auto derived = DeriveDungeonEntranceCamera(room_id, player_x, player_y);
  return {
      .room_id = room_id,
      .player_x = player_x,
      .player_y = player_y,
      .camera_x = derived.camera_x,
      .camera_y = derived.camera_y,
      .trigger_x = derived.trigger_x,
      .trigger_y = derived.trigger_y,
      .boundaries = derived.boundaries,
      .quadrant = derived.quadrant,
  };
}

// A worker that can complete only after the owner requests stop. The timeout
// bounds a broken test; success depends on the stop signal, not elapsed time.
class WaitingCensusJob {
 public:
  explicit WaitingCensusJob(util::StopToken token,
                            bool return_success = false) {
    std::promise<void> started_promise;
    auto started = started_promise.get_future();
    pending = std::async(
        std::launch::async,
        [observed = observed_stop_, token, return_success,
         started_promise = std::move(
             started_promise)]() mutable -> absl::StatusOr<zelda3::RoomCensus> {
          started_promise.set_value();
          const auto deadline =
              std::chrono::steady_clock::now() + std::chrono::seconds(10);
          while (!token.stop_requested() &&
                 std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
          *observed = token.stop_requested();
          if (return_success) {
            return MakeCensus({{4}});
          }
          return absl::CancelledError("Room census cancelled");
        });
    EXPECT_EQ(started.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
  }
  bool observed_stop() const { return observed_stop_->load(); }
  std::future<absl::StatusOr<zelda3::RoomCensus>> pending;

 private:
  std::shared_ptr<std::atomic<bool>> observed_stop_ =
      std::make_shared<std::atomic<bool>>(false);
};

TEST(DungeonRenderContextTest, TeardownStopsWorkerBeforeJoining) {
  auto editor = std::make_unique<DungeonEditorV2>();
  const auto token = DungeonEditorV2EntranceCameraTestPeer::NewToken(*editor);
  WaitingCensusJob job(token);
  DungeonEditorV2EntranceCameraTestPeer::SetPending(*editor,
                                                    std::move(job.pending));
  editor.reset();
  EXPECT_TRUE(token.stop_requested());
  EXPECT_TRUE(job.observed_stop());
}

TEST(DungeonRenderContextTest, TeardownCannotRestartCensus) {
  DungeonEditorV2 editor;
  editor.PrepareForSessionTeardown();
  editor.PrepareForSessionTeardown();
  EXPECT_TRUE(DungeonEditorV2EntranceCameraTestPeer::Shutdown(editor));
  EXPECT_EQ(DungeonEditorV2EntranceCameraTestPeer::Census(editor), nullptr);
  EXPECT_FALSE(
      DungeonEditorV2EntranceCameraTestPeer::TakePendingCensus(editor).valid());
}

TEST(DungeonRenderContextTest,
     RomReplacementCancelsAndRejectsSuccessfulOldJob) {
  DungeonEditorV2 editor;
  const auto token = DungeonEditorV2EntranceCameraTestPeer::NewToken(editor);
  WaitingCensusJob job(token, /*return_success=*/true);
  DungeonEditorV2EntranceCameraTestPeer::SetPending(editor,
                                                    std::move(job.pending));
  Rom replacement;
  editor.SetRom(&replacement);
  EXPECT_TRUE(token.stop_requested());
  ASSERT_TRUE(
      DungeonEditorV2EntranceCameraTestPeer::WaitForPendingCensus(editor));
  EXPECT_TRUE(job.observed_stop());
  EXPECT_EQ(DungeonEditorV2EntranceCameraTestPeer::Census(editor), nullptr);
  EXPECT_FALSE(DungeonEditorV2EntranceCameraTestPeer::HasCachedCensus(editor));
}

TEST(RoomMatrixCensusTest, TeardownStopsWorkerBeforeJoining) {
  auto overlay = std::make_unique<RoomMatrixCensusOverlay>();
  const auto token = RoomMatrixCensusTestPeer::NewToken(*overlay);
  WaitingCensusJob job(token);
  RoomMatrixCensusTestPeer::SetPending(*overlay, std::move(job.pending));
  overlay.reset();
  EXPECT_TRUE(token.stop_requested());
  EXPECT_TRUE(job.observed_stop());
}

TEST(RoomMatrixCensusTest, DisablingCancelsWithoutPublishingErrorOrRestarting) {
  RoomMatrixCensusOverlay overlay;
  overlay.SetEnabled(true);
  const auto token = RoomMatrixCensusTestPeer::NewToken(overlay);
  WaitingCensusJob job(token);
  RoomMatrixCensusTestPeer::SetPending(overlay, std::move(job.pending));
  overlay.SetEnabled(false);
  EXPECT_TRUE(token.stop_requested());
  ASSERT_TRUE(RoomMatrixCensusTestPeer::WaitForPending(overlay));
  overlay.Update(nullptr);
  EXPECT_TRUE(job.observed_stop());
  EXPECT_FALSE(overlay.computing());
  EXPECT_EQ(overlay.census(), nullptr);
  EXPECT_TRUE(RoomMatrixCensusTestPeer::Error(overlay).empty());
  EXPECT_FALSE(RoomMatrixCensusTestPeer::HasCachedCensus(overlay));
}

TEST(DungeonRenderContextTest, ProjectRebindCancelsRoomMatrixJob) {
  DungeonEditorV2 editor;
  int room = 4;
  ImVector<int> active;
  RoomMatrixContent matrix(&room, &active, [](int) {});
  DungeonEditorV2EntranceCameraTestPeer::AttachMatrix(editor, &matrix);
  auto& overlay = matrix.census_overlay();
  const auto token = RoomMatrixCensusTestPeer::NewToken(overlay);
  WaitingCensusJob job(token, /*return_success=*/true);
  RoomMatrixCensusTestPeer::SetPending(overlay, std::move(job.pending));
  editor.SetDependencies({});
  EXPECT_TRUE(token.stop_requested());
  EXPECT_TRUE(RoomMatrixCensusTestPeer::WaitForPending(overlay));
  overlay.Update(nullptr);
  EXPECT_TRUE(job.observed_stop());
  EXPECT_EQ(overlay.census(), nullptr);
  EXPECT_TRUE(RoomMatrixCensusTestPeer::Error(overlay).empty());
  EXPECT_FALSE(RoomMatrixCensusTestPeer::HasCachedCensus(overlay));
  // The production window manager keeps the panel alive through this boundary.
  editor.PrepareForSessionTeardown();
}

TEST(DungeonRenderContextTest, TeardownDetachesSurvivingRoomMatrix) {
  auto editor = std::make_unique<DungeonEditorV2>();
  int room = 4;
  ImVector<int> active;
  RoomMatrixContent matrix(&room, &active, [](int) { ADD_FAILURE(); });
  matrix.SetCensusManifestProvider([]() -> const core::HackManifest* {
    ADD_FAILURE() << "A detached panel must not consult its old project";
    return nullptr;
  });
  auto& overlay = matrix.census_overlay();
  overlay.SetEnabled(true);
  const auto token = RoomMatrixCensusTestPeer::NewToken(overlay);
  WaitingCensusJob job(token);
  RoomMatrixCensusTestPeer::SetPending(overlay, std::move(job.pending));
  DungeonEditorV2EntranceCameraTestPeer::AttachMatrix(*editor, &matrix);
  editor->PrepareForSessionTeardown();
  editor.reset();
  EXPECT_TRUE(token.stop_requested());
  ASSERT_TRUE(RoomMatrixCensusTestPeer::WaitForPending(overlay));
  overlay.Update(nullptr);
  EXPECT_FALSE(overlay.enabled());
  EXPECT_FALSE(overlay.computing());
  bool open = true;
  // No ImGui context is needed: a detached panel must return before drawing.
  matrix.Draw(&open);
}

TEST(DungeonRenderContextTest, PendingCensusDiscardsReplacedRomAndProject) {
  auto original = std::make_unique<Rom>();
  auto data = zelda3::test::BuildRoomCensusTestRom();
  ASSERT_TRUE(original->LoadFromData(data).ok());
  project::YazeProject project;
  ASSERT_TRUE(
      project.hack_manifest.LoadFromString(R"({"manifest_version":2})").ok());
  DungeonEditorV2 editor(original.get());
  EditorDependencies deps;
  deps.project = &project;
  editor.SetDependencies(deps);
  EXPECT_EQ(DungeonEditorV2EntranceCameraTestPeer::Census(editor), nullptr);

  const auto old_token = DungeonEditorV2EntranceCameraTestPeer::Token(editor);
  ASSERT_TRUE(old_token.has_stop_state());
  Rom replacement;
  // Original room 0 reaches 4; replacement reaches 6.
  data[SnesToPc(0x019010) + zelda3::test::kTestHeaderHolewarp] = 6;
  ASSERT_TRUE(replacement.LoadFromData(data).ok());
  editor.SetRom(&replacement);
  EXPECT_TRUE(old_token.stop_requested());
  original.reset();
  // Rebinding can replace a manifest in the same project object.
  project.hack_manifest = {};
  ASSERT_TRUE(
      project.hack_manifest
          .LoadFromString(R"({"manifest_version":2,"hack_name":"replacement"})")
          .ok());
  editor.SetDependencies(deps);
  ASSERT_TRUE(
      DungeonEditorV2EntranceCameraTestPeer::WaitForPendingCensus(editor));
  EXPECT_EQ(DungeonEditorV2EntranceCameraTestPeer::Census(editor), nullptr)
      << "The completed old generation must be discarded, then rescheduled";
  EXPECT_FALSE(
      DungeonEditorV2EntranceCameraTestPeer::Token(editor).stop_requested());
  ASSERT_TRUE(
      DungeonEditorV2EntranceCameraTestPeer::WaitForPendingCensus(editor));
  const auto* census = DungeonEditorV2EntranceCameraTestPeer::Census(editor);
  ASSERT_NE(census, nullptr);
  EXPECT_TRUE(census->rooms[6].reached);
  EXPECT_FALSE(census->rooms[4].reached);
}

TEST(DungeonRenderContextTest,
     CompletedCensusJobOwnsInputsAfterEditorTeardown) {
  auto rom = std::make_unique<Rom>();
  ASSERT_TRUE(rom->LoadFromData(zelda3::test::BuildRoomCensusTestRom()).ok());
  auto project = std::make_unique<project::YazeProject>();
  ASSERT_TRUE(
      project->hack_manifest.LoadFromString(R"({"manifest_version":2})").ok());
  auto editor = std::make_unique<DungeonEditorV2>(rom.get());
  EditorDependencies deps;
  deps.project = project.get();
  editor->SetDependencies(deps);
  EXPECT_EQ(DungeonEditorV2EntranceCameraTestPeer::Census(*editor), nullptr);
  ASSERT_TRUE(
      DungeonEditorV2EntranceCameraTestPeer::WaitForPendingCensus(*editor));
  // Retain the completed job so its result remains inspectable after
  // destroying all originating editor, project and ROM objects.
  auto pending =
      DungeonEditorV2EntranceCameraTestPeer::TakePendingCensus(*editor);
  ASSERT_TRUE(pending.valid());
  editor.reset();
  project.reset();
  rom.reset();
  auto census = pending.get();
  ASSERT_TRUE(census.ok()) << census.status();
  EXPECT_TRUE(census->rooms[4].reached);
  EXPECT_TRUE(census->rooms[0x105].reached);
}

TEST(DungeonRenderContextTest, ReloadedAssetsInvalidateCachedCensus) {
  DungeonEditorV2 editor;
  DungeonEditorV2EntranceCameraTestPeer::SynchronizeAssets(editor);
  DungeonEditorV2EntranceCameraTestPeer::SetRenderContextCensus(editor, {});
  ASSERT_TRUE(DungeonEditorV2EntranceCameraTestPeer::HasCachedCensus(editor));
  zelda3::CustomObjectManager::Get().ReloadAll();
  DungeonEditorV2EntranceCameraTestPeer::SynchronizeAssets(editor);
  EXPECT_FALSE(DungeonEditorV2EntranceCameraTestPeer::HasCachedCensus(editor));
}

TEST(RoomMatrixCensusTest, ReplacedRomDiscardsPendingResult) {
  auto original = std::make_unique<Rom>();
  auto data = zelda3::test::BuildRoomCensusTestRom();
  ASSERT_TRUE(original->LoadFromData(data).ok());
  RoomMatrixCensusOverlay overlay;
  overlay.SetEnabled(true);
  overlay.Update(original.get());
  const auto old_token = RoomMatrixCensusTestPeer::Token(overlay);
  ASSERT_TRUE(old_token.has_stop_state());
  ASSERT_TRUE(overlay.computing());
  ASSERT_TRUE(RoomMatrixCensusTestPeer::WaitForPending(overlay));

  Rom replacement;
  data[SnesToPc(0x019010) + zelda3::test::kTestHeaderHolewarp] = 6;
  ASSERT_TRUE(replacement.LoadFromData(data).ok());
  original.reset();
  overlay.Update(&replacement);
  EXPECT_TRUE(old_token.stop_requested());
  EXPECT_FALSE(RoomMatrixCensusTestPeer::Token(overlay).stop_requested());
  EXPECT_EQ(overlay.census(), nullptr);
  ASSERT_TRUE(RoomMatrixCensusTestPeer::WaitForPending(overlay));
  overlay.Update(&replacement);
  ASSERT_NE(overlay.census(), nullptr);
  EXPECT_TRUE(overlay.census()->rooms[6].reached);
  EXPECT_FALSE(overlay.census()->rooms[4].reached);
}

TEST(RoomMatrixCensusTest, SameAddressRomReplacementDiscardsPendingResult) {
  Rom rom;
  auto data = zelda3::test::BuildRoomCensusTestRom();
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  RoomMatrixCensusOverlay overlay;
  overlay.SetEnabled(true);
  overlay.Update(&rom);
  ASSERT_TRUE(RoomMatrixCensusTestPeer::WaitForPending(overlay));
  data[SnesToPc(0x019010) + zelda3::test::kTestHeaderHolewarp] = 6;
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  overlay.Update(&rom);
  EXPECT_EQ(overlay.census(), nullptr);
  ASSERT_TRUE(RoomMatrixCensusTestPeer::WaitForPending(overlay));
  overlay.Update(&rom);
  ASSERT_NE(overlay.census(), nullptr);
  EXPECT_TRUE(overlay.census()->rooms[6].reached);
  EXPECT_FALSE(overlay.census()->rooms[4].reached);
}

TEST(RoomMatrixCensusTest, AssetReloadDiscardsCompletedPendingResult) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(zelda3::test::BuildRoomCensusTestRom()).ok());
  RoomMatrixCensusOverlay overlay;
  overlay.SetEnabled(true);
  overlay.Update(&rom);
  ASSERT_TRUE(RoomMatrixCensusTestPeer::WaitForPending(overlay));
  zelda3::CustomObjectManager::Get().ReloadAll();
  overlay.Update(&rom);
  EXPECT_EQ(overlay.census(), nullptr);
  ASSERT_TRUE(RoomMatrixCensusTestPeer::WaitForPending(overlay));
  overlay.Update(&rom);
  ASSERT_NE(overlay.census(), nullptr);
  EXPECT_TRUE(overlay.census()->rooms[4].reached);
}

TEST(DungeonRenderContextTest, SelectedEntranceOwnsSiblingRooms) {
  const auto census = MakeCensus({{0x10, 0x11}, {0x20}});
  const std::array candidates = {
      DungeonRenderEntranceCandidate{
          .slot = 7, .room_id = 0x10, .main_gfx = 0x22},
      DungeonRenderEntranceCandidate{
          .slot = 8, .room_id = 0x20, .main_gfx = 0x33},
  };

  const auto context = ResolveDungeonRenderContext(
      0x11, /*selected_entrance_slot=*/7, /*room_header_blockset=*/0x04,
      &census, candidates);

  EXPECT_EQ(context.source, DungeonRenderContextSource::kSelectedEntrance);
  EXPECT_EQ(context.entrance_slot, 7);
  EXPECT_EQ(context.entrance_blockset, 0x22);
  EXPECT_EQ(context.owner_id, "D1");
}

TEST(DungeonRenderContextTest, ProjectMembershipWorksWithoutManifestOrCensus) {
  core::DungeonEntry dungeon;
  dungeon.id = "D6";
  dungeon.name = "Goron Mines";
  dungeon.rooms = {{.id = 0x89}, {.id = 0x98}};
  const std::array candidates = {
      DungeonRenderEntranceCandidate{.slot = 7, .room_id = 0x90, .main_gfx = 3},
      DungeonRenderEntranceCandidate{
          .slot = 0x2E, .room_id = 0x98, .main_gfx = 4}};
  const auto automatic = ResolveDungeonRenderContext(0x89, -1, 6, nullptr,
                                                     candidates, {}, &dungeon);
  EXPECT_EQ(automatic.entrance_slot, 0x2E);
  EXPECT_EQ(automatic.effective_blockset(), 4);
  EXPECT_EQ(automatic.owner_name, "Goron Mines");
  const auto manual = ResolveDungeonRenderContext(0x89, 7, 6, nullptr,
                                                  candidates, {}, &dungeon);
  EXPECT_EQ(manual.entrance_slot, 7);
  EXPECT_EQ(manual.effective_blockset(), 3);
}

TEST(DungeonRenderContextTest, ProjectEntranceLabelsFollowLiveTargetRoom) {
  const auto folder = test::UniqueTempPath("entrance_project_labels");
  const auto planning = folder / "Docs/Dev/Planning";
  std::filesystem::create_directories(planning);
  absl::Cleanup cleanup = [&] {
    std::error_code ec;
    std::filesystem::remove_all(folder, ec);
  };
  std::ofstream(planning / "dungeons.json") << R"({"dungeons":[{
    "id":"D3","name":"Kalyxo Castle","rooms":[{"id":"0x55"}]}]})";
  project::YazeProject project;
  project.name = "Oracle fixture";
  project.filepath = (folder / "fixture.yaze").string();
  ASSERT_TRUE(project.hack_manifest.LoadProjectRegistry(folder.string()).ok());
  EXPECT_EQ(dungeon_project_labels::GetEntranceLabel(&project, 0x32, 0x55),
            "D3 Kalyxo Castle");
  // A changed entrance target follows the loaded room, never a vanilla name
  // attached to its numeric entrance ID. The unowned Sanctuary stays vanilla.
  EXPECT_EQ(dungeon_project_labels::GetEntranceLabel(&project, 0x02, 0x55),
            "D3 Kalyxo Castle");
  EXPECT_EQ(dungeon_project_labels::GetEntranceLabel(&project, 0x02, 0x12),
            "Sanctuary");
  project.resource_labels["entrance"]["2"] = "Project-specific entrance";
  EXPECT_EQ(dungeon_project_labels::GetEntranceLabel(&project, 0x02, 0x12),
            "Project-specific entrance");
  project::YazeProject vanilla;
  EXPECT_EQ(dungeon_project_labels::GetEntranceLabel(&vanilla, 0x02, 0x12),
            "Sanctuary");
}

TEST(DungeonRenderContextTest, KalyxoCastleAutoExcludesUnrelatedSanctuary) {
  core::DungeonEntry dungeon;
  dungeon.id = "D3";
  dungeon.name = "Kalyxo Castle";
  dungeon.rooms = {{.id = 0x54}, {.id = 0x55}, {.id = 0x56}, {.id = 0x58},
                   {.id = 0x59}, {.id = 0x64}, {.id = 0x65}, {.id = 0x68}};
  constexpr int offset = zelda3::kNumDungeonSpawnPoints;
  const std::array candidates = {
      DungeonRenderEntranceCandidate{offset + 0x02, 0x12, 3},
      DungeonRenderEntranceCandidate{offset + 0x0A, 0x68, 2},
      DungeonRenderEntranceCandidate{offset + 0x28, 0x56, 2},
      DungeonRenderEntranceCandidate{offset + 0x2A, 0x58, 2},
      DungeonRenderEntranceCandidate{offset + 0x2B, 0x59, 2},
      DungeonRenderEntranceCandidate{offset + 0x32, 0x55, 2}};
  for (const int room : {0x54, 0x55, 0x64, 0x65}) {
    const auto context = ResolveDungeonRenderContext(
        room, -1, 1, nullptr, candidates,
        {.entrance_id = 2,
         .main_blockset = 3,
         .source = zelda3::RoomEntranceSource::kDungeonMap},
        &dungeon);
    EXPECT_EQ(context.owner_id, "D3");
    EXPECT_EQ(context.owner_name, "Kalyxo Castle");
    EXPECT_EQ(context.effective_blockset(), 2);
    EXPECT_EQ(context.entrance_slot, offset + (room == 0x55 ? 0x32 : 0x0A));
    EXPECT_EQ(DungeonEntrancePreviewLabel(context.entrance_slot),
              room == 0x55 ? "Entrance 32" : "Entrance 0A");
  }
}

TEST(DungeonRenderContextTest, OracleExpandedEntranceTargetsMatchAssociation) {
  const char* fixture = std::getenv("YAZE_TEST_ROM_OOS");
  if (!fixture || !*fixture)
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to the Oracle main candidate";
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(fixture).ok());
  const auto layout = zelda3::ResolveEntranceLayout(rom.vector());
  ASSERT_TRUE(layout.ok()) << layout.status();
  ASSERT_EQ(layout->kind, zelda3::EntranceLayoutKind::kZscreamExpanded);
  for (const auto& [id, target] :
       std::array<std::pair<int, int>, 5>{{{0x0A, 0x68},
                                           {0x28, 0x56},
                                           {0x2A, 0x58},
                                           {0x2B, 0x59},
                                           {0x32, 0x55}}}) {
    zelda3::RoomEntrance entrance(&rom, id);
    ASSERT_TRUE(entrance.load_status().ok());
    EXPECT_EQ(entrance.room_, target);
    EXPECT_EQ(entrance.blockset_, 2);
    EXPECT_EQ(entrance.dungeon_id_, 0x10);
  }
}

TEST(DungeonRenderContextTest, ConflictingDirectEntrancesStayAmbiguous) {
  const std::array candidates = {
      DungeonRenderEntranceCandidate{.slot = 7, .room_id = 0x89, .main_gfx = 3},
      DungeonRenderEntranceCandidate{
          .slot = 8, .room_id = 0x89, .main_gfx = 4}};
  const auto context = ResolveDungeonRenderContext(
      0x89, -1, 6, nullptr, candidates,
      {.entrance_id = 0,
       .main_blockset = 3,
       .source = zelda3::RoomEntranceSource::kDirect});
  EXPECT_FALSE(context.uses_entrance());
  EXPECT_EQ(context.source, DungeonRenderContextSource::kAmbiguous);
  EXPECT_EQ(context.effective_blockset(), 6);
}

TEST(DungeonRenderContextTest, DirectTargetWorksWithoutCensusOwnership) {
  const std::array candidates = {
      DungeonRenderEntranceCandidate{
          .slot = 9, .room_id = 0x44, .main_gfx = 0x12},
  };

  const auto context = ResolveDungeonRenderContext(
      0x44, /*selected_entrance_slot=*/9, /*room_header_blockset=*/0x08,
      nullptr, candidates);

  EXPECT_EQ(context.source, DungeonRenderContextSource::kSelectedEntrance);
  EXPECT_EQ(context.effective_blockset(), 0x12);
}

TEST(DungeonRenderContextTest, SelectedEntranceSurvivesUnrelatedRoomBrowsing) {
  const auto census = MakeCensus({{0x10, 0x11}, {0x20}});
  const std::array candidates = {
      DungeonRenderEntranceCandidate{.slot = 7, .room_id = 0x10, .main_gfx = 4},
      DungeonRenderEntranceCandidate{
          .slot = 8, .room_id = 0x20, .main_gfx = 10},
  };
  for (const int room_id : {0x10, 0x20, 0x76, 0x11}) {
    const auto context = ResolveDungeonRenderContext(
        room_id, 7, 3, &census, candidates,
        {.entrance_id = 1,
         .main_blockset = 10,
         .source = zelda3::RoomEntranceSource::kNeighbour});
    EXPECT_EQ(context.source, DungeonRenderContextSource::kSelectedEntrance);
    EXPECT_EQ(context.entrance_slot, 7);
    EXPECT_EQ(context.effective_blockset(), 4) << room_id;
  }
  const auto changed =
      ResolveDungeonRenderContext(0x10, 8, 3, &census, candidates);
  EXPECT_EQ(changed.effective_blockset(), 10);
}

TEST(DungeonRenderContextTest, MissingSelectionUsesUnambiguousDirectEntrance) {
  const std::array candidates = {
      DungeonRenderEntranceCandidate{
          .slot = 9, .room_id = 0x44, .main_gfx = 0x12},
  };

  const auto context = ResolveDungeonRenderContext(
      0x44, /*selected_entrance_slot=*/0, /*room_header_blockset=*/0x08,
      nullptr, candidates);

  EXPECT_EQ(context.source, DungeonRenderContextSource::kDirectEntrance);
  EXPECT_TRUE(context.uses_entrance());
  EXPECT_EQ(context.effective_blockset(), 0x12);
}

TEST(DungeonRenderContextTest, InfersOwnerWhenAllEntrancesAgree) {
  const auto census = MakeCensus({{0x30, 0x31, 0x32}, {0x40}});
  const std::array candidates = {
      DungeonRenderEntranceCandidate{
          .slot = 12, .room_id = 0x30, .main_gfx = 0x18},
      DungeonRenderEntranceCandidate{
          .slot = 9, .room_id = 0x31, .main_gfx = 0x18},
      DungeonRenderEntranceCandidate{
          .slot = 20, .room_id = 0x40, .main_gfx = 0x2A},
  };

  const auto context = ResolveDungeonRenderContext(
      0x32, /*selected_entrance_slot=*/-1, /*room_header_blockset=*/0x06,
      &census, candidates);

  EXPECT_EQ(context.source, DungeonRenderContextSource::kInferredOwnerEntrance);
  EXPECT_EQ(context.entrance_slot, 9);
  EXPECT_EQ(context.entrance_blockset, 0x18);
  EXPECT_EQ(context.candidate_slots, (std::vector<int>{9, 12}));
}

TEST(DungeonRenderContextTest, ConflictingOwnerEntrancesFallBack) {
  const auto census = MakeCensus({{0x50, 0x51, 0x52}});
  const std::array candidates = {
      DungeonRenderEntranceCandidate{
          .slot = 7, .room_id = 0x50, .main_gfx = 0x10},
      DungeonRenderEntranceCandidate{
          .slot = 8, .room_id = 0x51, .main_gfx = 0x11},
  };

  const auto context = ResolveDungeonRenderContext(
      0x52, /*selected_entrance_slot=*/99, /*room_header_blockset=*/0x07,
      &census, candidates);

  EXPECT_EQ(context.source, DungeonRenderContextSource::kAmbiguous);
  EXPECT_FALSE(context.uses_entrance());
  EXPECT_EQ(context.effective_blockset(), 0x07);
  EXPECT_EQ(context.candidate_slots, (std::vector<int>{7, 8}));
}

TEST(DungeonRenderContextTest, ViewerUsesResolvedOwnerEntranceSlot) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonEditorV2 editor(&rom);
  auto census = MakeCensus({{0x10, 0x11}});
  DungeonEditorV2EntranceCameraTestPeer::SetRenderContextCensus(
      editor, std::move(census));
  constexpr int kEntranceSlot = zelda3::kNumDungeonSpawnPoints;
  DungeonEditorV2EntranceCameraTestPeer::SetSelectedEntranceGraphics(
      editor, kEntranceSlot, /*room_id=*/0x10, /*main_gfx=*/0x2A);
  DungeonCanvasViewer viewer(&rom);

  DungeonEditorV2EntranceCameraTestPeer::ConfigureViewer(editor, viewer, 0x11);

  EXPECT_EQ(viewer.current_entrance_id(), kEntranceSlot);
  EXPECT_EQ(viewer.current_entrance_blockset(), 0x2A);
}

TEST(DungeonRenderContextTest, AmbiguousOwnerDoesNotTrustMatrixNeighbor) {
  const auto census = MakeCensus({{0x50, 0x51, 0x52}});
  const std::array candidates = {
      DungeonRenderEntranceCandidate{.slot = 7, .room_id = 0x50, .main_gfx = 1},
      DungeonRenderEntranceCandidate{.slot = 8, .room_id = 0x51, .main_gfx = 2},
  };
  const zelda3::RoomDefaultEntrance fallback{
      .entrance_id = 0,
      .main_blockset = 1,
      .source = zelda3::RoomEntranceSource::kNeighbour,
      .via_room = 0x51};
  const auto context =
      ResolveDungeonRenderContext(0x52, -1, 7, &census, candidates, fallback);
  EXPECT_EQ(context.source, DungeonRenderContextSource::kAmbiguous);
  EXPECT_FALSE(context.uses_entrance());
  EXPECT_EQ(context.effective_blockset(), 7);
  EXPECT_STREQ(DungeonRenderContextSourceName(context),
               "Ambiguous owner entrances");
}

TEST(DungeonRenderContextTest, SnapshotDefaultMatchesLoadedSheets) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::GameData data;
  data.main_blockset_ids[1][0] = 9;
  data.room_default_entrances.resize(0x12);
  data.room_default_entrances[0x11] = {
      .entrance_id = 2,
      .main_blockset = 1,
      .source = zelda3::RoomEntranceSource::kDungeonMap};
  DungeonEditorV2 editor(&rom);
  editor.SetGameData(&data);
  DungeonEditorV2EntranceCameraTestPeer::SetSelectedEntranceGraphics(
      editor, 2 + zelda3::kNumDungeonSpawnPoints, 0x10, 1);
  DungeonEditorV2EntranceCameraTestPeer::ClearSelectedEntrance(editor);
  DungeonEditorV2EntranceCameraTestPeer::SetRenderContextCensus(editor, {});
  editor.rooms()[0x11] = zelda3::Room(0x11, &rom, &data);
  DungeonEditorV2EntranceCameraTestPeer::MaterializeCurrentRoom(editor, 0x11,
                                                                4);
  auto& room = editor.rooms()[0x11];
  room.LoadRoomGraphics();
  ASSERT_EQ(room.blocks()[0], 9);
  const auto snapshot = editor.BuildContextSnapshot();
  const auto value_for = [&snapshot](const std::string& id) {
    for (const auto& value : snapshot.metadata) {
      if (value.id == id)
        return value.value;
    }
    return std::string{};
  };
  EXPECT_EQ(value_for("graphics_entrance"), "Entrance 02");
  EXPECT_EQ(value_for("graphics_source"), "Inferred owner entrance");
  EXPECT_EQ(value_for("blockset"), "main 0x01 / room 0x04");
}

TEST(DungeonRenderContextTest, ContextSnapshotReportsOwnerSourceAndCamera) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonEditorV2 editor(&rom);
  DungeonEditorV2EntranceCameraTestPeer::SetRenderContextCensus(
      editor, MakeCensus({{0x10, 0x11}}));
  constexpr int kEntranceSlot = zelda3::kNumDungeonSpawnPoints;
  DungeonEditorV2EntranceCameraTestPeer::SetSelectedEntranceGraphics(
      editor, kEntranceSlot, /*room_id=*/0x10, /*main_gfx=*/0x2A);
  auto camera = StateFromDerived(/*room_id=*/0x10, /*player_x=*/0x0180,
                                 /*player_y=*/0x0290);
  DungeonEditorV2EntranceCameraTestPeer::SetRegularEntrance(
      editor, kEntranceSlot, camera);
  DungeonEditorV2EntranceCameraTestPeer::MaterializeCurrentRoom(
      editor, /*room_id=*/0x11, /*room_blockset=*/0x04);

  const EditorContextSnapshot snapshot = editor.BuildContextSnapshot();
  const auto value_for = [&snapshot](const std::string& id) {
    const auto it = std::find_if(
        snapshot.metadata.begin(), snapshot.metadata.end(),
        [&id](const EditorContextValue& value) { return value.id == id; });
    return it == snapshot.metadata.end() ? std::string{} : it->value;
  };

  EXPECT_EQ(value_for("graphics_entrance"), "Entrance 00");
  EXPECT_EQ(value_for("graphics_source"), "Selected entrance");
  EXPECT_EQ(value_for("dungeon_owner"), "Dungeon 1");
  EXPECT_EQ(value_for("entrance_camera"), "Matches player position");
}

TEST(DungeonEntranceCameraTest, DerivesUpperLeftAndLowerRightStates) {
  const auto upper_left =
      DeriveDungeonEntranceCamera(/*room_id=*/0x00, /*player_x=*/0x0078,
                                  /*player_y=*/0x0078);
  EXPECT_EQ(upper_left.camera_x, 0x0000);
  EXPECT_EQ(upper_left.camera_y, 0x0010);
  EXPECT_EQ(upper_left.trigger_x, 0x007F);
  EXPECT_EQ(upper_left.trigger_y, 0x0087);
  EXPECT_EQ(upper_left.boundaries,
            (std::array<uint8_t, 8>{0, 0, 0, 1, 0, 0, 0, 1}));
  EXPECT_EQ(upper_left.quadrant, 0x00);

  const auto lower_right =
      DeriveDungeonEntranceCamera(/*room_id=*/0x12, /*player_x=*/0x05F8,
                                  /*player_y=*/0x03F8);
  EXPECT_EQ(lower_right.camera_x, 0x0500);
  EXPECT_EQ(lower_right.camera_y, 0x0310);
  EXPECT_EQ(lower_right.trigger_x, 0x017F);
  EXPECT_EQ(lower_right.trigger_y, 0x0187);
  EXPECT_EQ(lower_right.boundaries,
            (std::array<uint8_t, 8>{3, 2, 3, 3, 5, 4, 5, 5}));
  EXPECT_EQ(lower_right.quadrant, 0x12);
}

TEST(DungeonEntranceCameraTest, UsesIndependentHorizontalAndVerticalLimits) {
  const auto horizontal =
      DeriveDungeonEntranceCamera(/*room_id=*/0x11F, /*player_x=*/0x1FF8,
                                  /*player_y=*/0x23F8);
  EXPECT_EQ(horizontal.camera_x, 0x1F00);
  EXPECT_EQ(horizontal.boundaries[7], 0x1F);

  const auto vertical =
      DeriveDungeonEntranceCamera(/*room_id=*/0x127, /*player_x=*/0x0FF8,
                                  /*player_y=*/0x25F8);
  EXPECT_EQ(vertical.camera_y, 0x2510);
  EXPECT_EQ(vertical.boundaries[3], 0x25);
}

TEST(DungeonEntranceCameraTest, DistinguishesSafeDifferencesFromErrors) {
  auto state = StateFromDerived(/*room_id=*/0x23, /*player_x=*/0x0750,
                                /*player_y=*/0x0520);
  state.trigger_x += 8;

  const auto safe_difference = ValidateDungeonEntranceCamera(state);
  EXPECT_TRUE(safe_difference.geometry_valid());
  EXPECT_FALSE(safe_difference.matches_derived());
  EXPECT_TRUE(safe_difference.can_repair());

  state.camera_x = 0;
  state.boundaries[4] = 0x20;
  const auto invalid = ValidateDungeonEntranceCamera(state);
  EXPECT_FALSE(invalid.geometry_valid());
  EXPECT_FALSE(invalid.errors.empty());
  EXPECT_TRUE(invalid.can_repair());

  state.player_x = 0;
  const auto invalid_source = ValidateDungeonEntranceCamera(state);
  EXPECT_FALSE(invalid_source.repair_source_valid);
  EXPECT_FALSE(invalid_source.can_repair());
}

TEST(DungeonEntranceCameraTest, RepairPreservesSpecialRoomQuadrant) {
  auto state = StateFromDerived(/*room_id=*/0x104, /*player_x=*/0x0950,
                                /*player_y=*/0x2150);
  state.camera_x += 8;
  state.quadrant ^= 0x12;

  const auto validation = ValidateDungeonEntranceCamera(state);
  ASSERT_TRUE(validation.geometry_valid());
  EXPECT_FALSE(validation.quadrant_repair_safe);

  const auto repaired = BuildRepairedDungeonEntranceCamera(state);
  EXPECT_EQ(repaired.camera_x, validation.expected.camera_x);
  EXPECT_EQ(repaired.quadrant, state.quadrant);

  auto quadrant_only = StateFromDerived(/*room_id=*/0x104, /*player_x=*/0x0950,
                                        /*player_y=*/0x2150);
  quadrant_only.quadrant ^= 0x12;
  EXPECT_FALSE(ValidateDungeonEntranceCamera(quadrant_only).can_repair());
}

TEST(DungeonEntranceCameraTest, ApplyRestoresDataAndDirtyState) {
  auto state = StateFromDerived(/*room_id=*/0x40, /*player_x=*/0x0180,
                                /*player_y=*/0x08A0);
  state.dirty = true;

  zelda3::RoomEntrance entrance;
  ApplyDungeonEntranceCameraState(state, &entrance);
  EXPECT_EQ(CaptureDungeonEntranceCamera(entrance).camera_x, state.camera_x);
  EXPECT_TRUE(entrance.dirty());

  state.kind = DungeonEntranceRecordKind::kSpawnPoint;
  state.dirty = false;
  zelda3::DungeonSpawnPoint spawn;
  spawn.MarkDirty();
  ApplyDungeonSpawnCameraState(state, &spawn);
  EXPECT_EQ(CaptureDungeonSpawnCamera(spawn).boundaries, state.boundaries);
  EXPECT_FALSE(spawn.dirty());
}

TEST(DungeonEntranceCameraTest, EditorRepairIsOneUndoableDirtyTransition) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonEditorV2 editor(&rom);
  constexpr int kSlot = zelda3::kNumDungeonSpawnPoints;
  auto before = StateFromDerived(/*room_id=*/0x10, /*player_x=*/0x0180,
                                 /*player_y=*/0x0290);
  before.trigger_x -= 8;
  before.dirty = false;
  // The clean pre-repair state must also be the ROM baseline. Undo computes
  // dirty state from the current bytes, including after a later save.
  zelda3::RoomEntrance baseline(&rom, 0, false);
  baseline.room_ = static_cast<int16_t>(before.room_id);
  baseline.x_position_ = before.player_x;
  baseline.y_position_ = before.player_y;
  ApplyDungeonEntranceCameraState(before, &baseline);
  ASSERT_TRUE(baseline.Save(&rom, 0).ok());
  DungeonEditorV2EntranceCameraTestPeer::SetRegularEntrance(editor, kSlot,
                                                            before);

  const absl::Status repair_status =
      DungeonEditorV2EntranceCameraTestPeer::Repair(editor, kSlot);
  ASSERT_TRUE(repair_status.ok()) << repair_status;
  auto repaired =
      DungeonEditorV2EntranceCameraTestPeer::RegularEntrance(editor, kSlot);
  EXPECT_EQ(repaired.trigger_x,
            DeriveDungeonEntranceCamera(before.room_id, before.player_x,
                                        before.player_y)
                .trigger_x);
  EXPECT_TRUE(repaired.dirty);

  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_EQ(
      DungeonEditorV2EntranceCameraTestPeer::RegularEntrance(editor, kSlot),
      before);
  ASSERT_TRUE(editor.Redo().ok());
  EXPECT_EQ(
      DungeonEditorV2EntranceCameraTestPeer::RegularEntrance(editor, kSlot),
      repaired);
}

TEST(DungeonEntranceCameraTest, SpawnRepairMarksOnlyDedicatedSpawnRecord) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonEditorV2 editor(&rom);
  constexpr int kSlot = 0;
  auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, kSlot);
  ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
  auto spawn = *spawn_or;
  const auto derived =
      DeriveDungeonEntranceCamera(/*room_id=*/0x21, /*player_x=*/0x0350,
                                  /*player_y=*/0x0490);
  spawn.room_id = 0x21;
  spawn.x_coordinate = 0x0350;
  spawn.y_coordinate = 0x0490;
  spawn.horizontal_scroll = derived.camera_x;
  spawn.vertical_scroll = derived.camera_y;
  spawn.camera_trigger_x = derived.trigger_x - 8;
  spawn.camera_trigger_y = derived.trigger_y;
  spawn.camera_scroll_boundaries = derived.boundaries;
  spawn.quadrant = derived.quadrant;
  ASSERT_TRUE(spawn.Save(&rom, kSlot).ok());
  DungeonEditorV2EntranceCameraTestPeer::SetSpawnPoint(editor, kSlot,
                                                       std::move(spawn));

  const absl::Status repair_status =
      DungeonEditorV2EntranceCameraTestPeer::Repair(editor, kSlot);
  ASSERT_TRUE(repair_status.ok()) << repair_status;
  EXPECT_TRUE(
      DungeonEditorV2EntranceCameraTestPeer::SpawnPoint(editor, kSlot).dirty);
  EXPECT_FALSE(DungeonEditorV2EntranceCameraTestPeer::LegacyEntranceDirty(
      editor, kSlot));

  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_FALSE(
      DungeonEditorV2EntranceCameraTestPeer::SpawnPoint(editor, kSlot).dirty);
}

TEST(DungeonEntranceCameraTest, ViewerOverlayRoundTripsSelectedCamera) {
  Rom rom;
  DungeonCanvasViewer viewer(&rom);
  const auto state = StateFromDerived(/*room_id=*/0x42,
                                      /*player_x=*/0x0550,
                                      /*player_y=*/0x0890);

  viewer.SetEntranceCameraOverlay(/*entrance_slot=*/0x12, state);
  ASSERT_TRUE(viewer.entrance_camera_overlay().has_value());
  EXPECT_EQ(*viewer.entrance_camera_overlay(), state);

  viewer.ClearEntranceCameraOverlay();
  EXPECT_FALSE(viewer.entrance_camera_overlay().has_value());
}

TEST(DungeonEntranceCameraTest, RegularRepairFieldsSaveAndReadBack) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::RoomEntrance entrance(&rom, /*entrance_id=*/0,
                                /*is_spawn_point=*/false);
  auto expected = StateFromDerived(/*room_id=*/0x12, /*player_x=*/0x0550,
                                   /*player_y=*/0x0350);
  expected.dirty = true;
  entrance.room_ = expected.room_id;
  entrance.x_position_ = expected.player_x;
  entrance.y_position_ = expected.player_y;
  ApplyDungeonEntranceCameraState(expected, &entrance);

  ASSERT_TRUE(entrance.Save(&rom, /*entrance_id=*/0).ok());
  const zelda3::RoomEntrance reloaded(&rom, /*entrance_id=*/0,
                                      /*is_spawn_point=*/false);
  expected.dirty = false;
  EXPECT_EQ(CaptureDungeonEntranceCamera(reloaded), expected);
}

TEST(DungeonEntranceCameraTest, SpawnRepairFieldsSaveAndReadBack) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, /*spawn_id=*/0);
  ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
  auto spawn = *spawn_or;
  auto expected = StateFromDerived(/*room_id=*/0x21, /*player_x=*/0x0350,
                                   /*player_y=*/0x0490);
  expected.kind = DungeonEntranceRecordKind::kSpawnPoint;
  expected.dirty = true;
  spawn.room_id = expected.room_id;
  spawn.x_coordinate = expected.player_x;
  spawn.y_coordinate = expected.player_y;
  ApplyDungeonSpawnCameraState(expected, &spawn);

  ASSERT_TRUE(spawn.Save(&rom, /*spawn_id=*/0).ok());
  auto reloaded_or = zelda3::DungeonSpawnPoint::Load(rom, /*spawn_id=*/0);
  ASSERT_TRUE(reloaded_or.ok()) << reloaded_or.status();
  expected.dirty = false;
  EXPECT_EQ(CaptureDungeonSpawnCamera(*reloaded_or), expected);
}

}  // namespace
}  // namespace yaze::editor
