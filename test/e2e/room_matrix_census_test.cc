#define IMGUI_DEFINE_MATH_OPERATORS

#include "e2e/room_matrix_census_test.h"

#include <string>

#include "absl/strings/str_format.h"
#include "app/controller.h"
#include "app/editor/dungeon/workspace/room_matrix_content.h"
#include "app/editor/editor_manager.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "imgui/imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "test_utils.h"
#include "zelda3/dungeon/room_census.h"

namespace {

using yaze::Controller;
using yaze::editor::EditorType;
using yaze::editor::RoomMatrixContent;
using yaze::editor::WindowDescriptor;
using yaze::editor::WorkspaceWindowManager;

constexpr const char* kMatrixId = "dungeon.room_matrix";

Controller* GetController(ImGuiTestContext* ctx) {
  return ctx && ctx->Test ? static_cast<Controller*>(ctx->Test->UserData)
                          : nullptr;
}

void E2ETest_RoomMatrixCensusToggleAndHoverFreeRoom(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  yaze::test::gui::LoadRomInTest(ctx,
                                 yaze::test::TestRomManager::GetTestRomPath());
  auto* rom = controller->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    ctx->LogWarning("RoomMatrixCensus: no test ROM loaded; skipping.");
    return;
  }
  auto* editor_manager = controller->editor_manager();
  editor_manager->SwitchToEditor(EditorType::kDungeon, true);
  ctx->Yield(10);

  WorkspaceWindowManager* wm = editor_manager->GetWindowManager();
  IM_CHECK(wm != nullptr);
  const size_t session = editor_manager->GetCurrentSessionId();
  wm->OpenWindow(session, kMatrixId);
  ctx->Yield(5);
  IM_CHECK(wm->IsWindowOpen(session, kMatrixId));
  auto* matrix = dynamic_cast<RoomMatrixContent*>(
      wm->GetWindowContent(session, kMatrixId));
  IM_CHECK(matrix != nullptr);
  const WindowDescriptor* descriptor =
      wm->GetWindowDescriptor(session, kMatrixId);
  IM_CHECK(descriptor != nullptr);
  const std::string title = descriptor->GetImGuiWindowName();

  // Real click on the toggle.
  auto& overlay = matrix->census_overlay();
  IM_CHECK(!overlay.enabled());
  ctx->SetRef(title.c_str());
  ctx->ItemClick("RoomMatrixCensus/Census overlay");
  ctx->Yield(2);
  IM_CHECK(overlay.enabled());

  // The census runs in the background on a ROM copy.
  for (int frame = 0; frame < 3000 && overlay.census() == nullptr; ++frame) {
    ctx->Yield();
  }
  const auto* census = overlay.census();
  IM_CHECK(census != nullptr);
  IM_CHECK_EQ(static_cast<int>(census->rooms.size()),
              yaze::zelda3::kRoomCensusRoomCount);
  IM_CHECK_GT(census->free_count, 0);
  ctx->LogInfo("Census: %d free, %d reclaimable, largest block: %s",
               census->free_count, census->reclaimable_count,
               census->free_clusters.empty()
                   ? "-"
                   : census->free_clusters.front().summary.c_str());

  int free_room = -1;
  for (const auto& entry : census->rooms) {
    if (entry.status == yaze::zelda3::RoomCensusStatus::kFree) {
      free_room = entry.room_id;
      break;
    }
  }
  IM_CHECK(free_room >= 0);

  // Legend chips are laid out and fit inside the window.
  ctx->Yield(2);
  ImGuiWindow* window = ImGui::FindWindowByName(title.c_str());
  IM_CHECK(window != nullptr);
  const ImGuiTestItemInfo chip = ctx->ItemInfo("RoomMatrixCensus/##chip_free");
  IM_CHECK(chip.ID != 0);
  IM_CHECK_LE(chip.RectFull.Max.x, window->InnerRect.Max.x + 1.0f);

  // Hover the free room's cell: the tooltip shows its census status.
  const std::string cell = absl::StrFormat("##room%d", free_room);
  ctx->ScrollToItemY(cell.c_str());
  ctx->MouseMove(cell.c_str());
  ctx->Yield(3);
  IM_CHECK(ctx->UiContext->HoveredIdPreviousFrame != 0);
  ImGuiWindow* tooltip = nullptr;
  for (ImGuiWindow* w : ctx->UiContext->Windows) {
    if (w->Active && (w->Flags & ImGuiWindowFlags_Tooltip)) {
      tooltip = w;
    }
  }
  IM_CHECK(tooltip != nullptr);
  IM_CHECK_EQ(census->rooms[free_room].status,
              yaze::zelda3::RoomCensusStatus::kFree);
  IM_CHECK(overlay.StatusOutline(free_room).has_value());

  // Free chip filter leaves the free room matching, an in-use room not.
  overlay.SetFilterFree(true);
  IM_CHECK(overlay.MatchesChips(free_room));
  int in_use_room = -1;
  for (const auto& entry : census->rooms) {
    if (entry.status == yaze::zelda3::RoomCensusStatus::kInUse) {
      in_use_room = entry.room_id;
      break;
    }
  }
  IM_CHECK(in_use_room >= 0);
  IM_CHECK(!overlay.MatchesChips(in_use_room));
  overlay.SetFilterFree(false);

  // Toggle back off with a real click.
  ctx->MouseMove("RoomMatrixCensus/Census overlay");
  ctx->ItemClick("RoomMatrixCensus/Census overlay");
  ctx->Yield(2);
  IM_CHECK(!overlay.enabled());
}

}  // namespace

namespace yaze {
namespace test {
namespace e2e {

void RegisterRoomMatrixCensusTests(ImGuiTestEngine* engine,
                                   Controller* controller) {
  ImGuiTest* t =
      IM_REGISTER_TEST(engine, "RoomMatrixCensus", "ToggleAndHoverFreeRoom");
  t->TestFunc = E2ETest_RoomMatrixCensusToggleAndHoverFreeRoom;
  t->UserData = controller;
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
