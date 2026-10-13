#include "app/editor/dungeon/dungeon_canvas_viewer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_canvas_transform.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/ui/window/dungeon_map_panel.h"
#include "app/editor/dungeon/ui_constants.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas_pipelines.h"
#include "app/gui/core/icons.h"
#include "core/features.h"
#include "core/project.h"
#include "dungeon_workbench_test_peer.h"
#include "gtest/gtest.h"
#include "imgui/imgui_internal.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/palette_debug.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

struct PingRectSnapshot {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

struct PendingScrollFrameSnapshot {
  ImVec2 canvas_origin;
  ImVec2 applied_scroll;
  ImVec2 background_origin;
  ImVec2 overlay_origin;
  std::pair<int, int> background_origin_room_pixel;
};

struct IssueReportPopupSnapshot {
  bool found = false;
  ImVec2 position;
  ImVec2 size;
  ImGuiWindowFlags flags = ImGuiWindowFlags_None;
  bool outer_scrollbar_y = false;
  bool body_found = false;
  bool body_scrollbar_y = false;
  ImRect body_bounds;
  ImRect inner_bounds;
  ImVec2 content_end;
};

class DungeonMapPanelTestPeer {
 public:
  static size_t RoomCount(const DungeonMapPanel& panel) {
    return panel.dungeon_room_ids_.size();
  }

  static bool HasRoomType(const DungeonMapPanel& panel, int room_id) {
    return panel.room_types_.contains(room_id);
  }

  static size_t StairConnectionCount(const DungeonMapPanel& panel) {
    return panel.stair_connections_.size();
  }

  static size_t HolewarpConnectionCount(const DungeonMapPanel& panel) {
    return panel.holewarp_connections_.size();
  }

  static void SeedCompositeOutput(DungeonMapPanel& panel, int room_id) {
    panel.room_composite_outputs_[room_id] =
        std::make_unique<RoomCompositeOutput>();
  }

  static size_t CompositeOutputCount(const DungeonMapPanel& panel) {
    return panel.room_composite_outputs_.size();
  }
};

class DungeonCanvasViewerTestPeer {
 public:
  static const gfx::Bitmap& PrimaryComposite(
      const DungeonCanvasViewer& viewer) {
    return viewer.primary_composite_output_.bitmap();
  }
  static gfx::Bitmap* PrepareConnectedRoom(DungeonCanvasViewer& viewer,
                                           int room_id) {
    return viewer.PrepareConnectedRoomCompositeBitmap(room_id);
  }
  static std::span<const uint8_t> ExternalSpriteGraphics(
      DungeonCanvasViewer& viewer) {
    const auto* project = viewer.project();
    viewer.sprite_preview_resources_.SetContext(
        project->filepath, project->GetAbsolutePath(project->assets_folder),
        project->hack_manifest.hack_name());
    return viewer.sprite_preview_resources_.GetGraphics(
        zelda3::SpriteOamRegistry::GetPreviewOverride(0x88,
                                                      "Oracle of Secrets"));
  }

  static std::array<DungeonCanvasViewer*, 3> SeedAssetRefreshViewers(
      DungeonEditorV2& editor, const project::YazeProject* project) {
    auto room_viewer = std::make_unique<DungeonCanvasViewer>();
    auto* room_viewer_ptr = room_viewer.get();
    editor.room_viewers_.Insert(0, std::move(room_viewer));
    editor.workbench_viewer_ = std::make_unique<DungeonCanvasViewer>();
    editor.workbench_compare_viewer_ = std::make_unique<DungeonCanvasViewer>();
    std::array<DungeonCanvasViewer*, 3> viewers = {
        room_viewer_ptr, editor.workbench_viewer_.get(),
        editor.workbench_compare_viewer_.get()};
    for (auto* viewer : viewers) {
      viewer->SetProject(project);
    }
    return viewers;
  }

  static void SynchronizeProjectAssets(DungeonEditorV2& editor) {
    editor.SynchronizeCustomObjectAssets();
  }

  static void RenderSprites(DungeonCanvasViewer& viewer,
                            const gui::CanvasRuntime& runtime,
                            const zelda3::Room& room) {
    viewer.RenderSprites(runtime, room);
  }

  static void RenderChestContents(DungeonCanvasViewer& viewer,
                                  const gui::CanvasRuntime& runtime,
                                  const zelda3::Room& room) {
    viewer.RenderChestContents(runtime, room);
  }

  static ImRect DrawMetadataHud(DungeonCanvasViewer& viewer, int room_id,
                                float canvas_width) {
    gui::CanvasFrameOptions options;
    options.canvas_size = ImVec2(canvas_width, 400);
    options.draw_context_menu = false;
    options.draw_grid = false;
    options.draw_overlay = false;
    options.render_popups = false;
    auto runtime = gui::BeginCanvas(viewer.canvas_, options);
    viewer.DrawHeaderHiddenMetadataHud(room_id);
    gui::EndCanvas(viewer.canvas_, runtime, options);
    return ImRect(runtime.canvas_p0,
                  ImVec2(runtime.canvas_p0.x + runtime.canvas_sz.x,
                         runtime.canvas_p0.y + runtime.canvas_sz.y));
  }

  static auto* CachedSpritePreview(DungeonCanvasViewer& viewer,
                                   const zelda3::Room& room,
                                   const zelda3::Sprite& sprite) {
    return viewer.sprite_preview_cache_.Peek({room.graphics_revision(),
                                              sprite.id(), sprite.subtype(),
                                              sprite.IsOverlord()});
  }

  static size_t SpritePreviewCacheSize(const DungeonCanvasViewer& viewer) {
    return viewer.sprite_preview_cache_.Size();
  }

  static void UpdateRoomCanvasShortcutFocus(DungeonCanvasViewer& viewer,
                                            bool hovered, bool pointer_pressed,
                                            int frame_index) {
    viewer.UpdateRoomCanvasShortcutFocus(hovered, pointer_pressed, frame_index);
  }

  static bool HasRoomCanvasShortcutFocusForFrame(
      const DungeonCanvasViewer& viewer, int frame_index) {
    return viewer.HasRoomCanvasShortcutFocusForFrame(frame_index);
  }

  static absl::Status PrepareIssueReportPopup(
      DungeonCanvasViewer& viewer, const std::string& title,
      const std::string& summary, const std::string& kind_label,
      const std::string& diagnostics, int room_id, int default_category_index) {
    return viewer.PrepareIssueReportPopup(title, summary, kind_label,
                                          diagnostics, room_id,
                                          default_category_index);
  }

  static const std::string& last_log_path(const DungeonCanvasViewer& viewer) {
    return viewer.issue_report_popup_last_log_path_;
  }

  static absl::Status SaveIssueReport(DungeonCanvasViewer& viewer) {
    return viewer.EnsureIssueReportPersisted();
  }

  static void OpenIssueReportPopup(DungeonCanvasViewer& viewer) {
    viewer.OpenIssueReportPopup(
        "Room 001 rendering", "Wall overlaps north door",
        "Dungeon Room Issue Report",
        "Room 0x001\nObject stream diagnostics\nDoor diagnostics", 0x01,
        ToIssueCategoryIndex(DungeonIssueCategory::ObjectDrawMismatch));
  }

  static void RenderIssueReportPopup(DungeonCanvasViewer& viewer) {
    viewer.canvas_.RenderPersistentPopups();
  }

  static void SetIssueReportCaptureState(DungeonCanvasViewer& viewer,
                                         bool captured) {
    viewer.issue_report_popup_screenshot_path_ =
        captured ? "/tmp/room-001.png" : "";
  }

  static void SetIssueReportStatus(DungeonCanvasViewer& viewer,
                                   const std::string& message, bool is_error) {
    viewer.SetIssueReportPopupStatus(message, is_error);
  }

  static ImRect DrawIssueReportStatus(DungeonCanvasViewer& viewer) {
    viewer.DrawIssueReportStatusMessage();
    return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
  }

  static ImGuiWindow* IssueReportBody(const DungeonCanvasViewer& viewer) {
    ImGuiWindow* window =
        ImGui::FindWindowByName(viewer.issue_report_popup_id_.c_str());
    if (window != nullptr) {
      for (ImGuiWindow* child : window->DC.ChildWindows) {
        if (std::string_view(child->Name).find("##DungeonIssueReportBody") !=
            std::string_view::npos) {
          return child;
        }
      }
    }
    return nullptr;
  }

  static void ExpandIssueReportSections(const DungeonCanvasViewer& viewer) {
    if (ImGuiWindow* body = IssueReportBody(viewer)) {
      body->StateStorage.SetInt(body->GetID("Local files"), 1);
      body->StateStorage.SetInt(body->GetID("Diagnostics"), 1);
    }
  }

  static IssueReportPopupSnapshot CaptureIssueReportPopup(
      const DungeonCanvasViewer& viewer) {
    IssueReportPopupSnapshot snapshot;
    ImGuiWindow* window =
        ImGui::FindWindowByName(viewer.issue_report_popup_id_.c_str());
    if (window == nullptr) {
      return snapshot;
    }
    snapshot.found = true;
    snapshot.position = window->Pos;
    snapshot.size = window->Size;
    snapshot.flags = window->Flags;
    snapshot.outer_scrollbar_y = window->ScrollbarY;
    snapshot.inner_bounds = window->InnerRect;
    snapshot.content_end = window->DC.CursorMaxPos;
    if (ImGuiWindow* body = IssueReportBody(viewer)) {
      snapshot.body_found = true;
      snapshot.body_scrollbar_y = body->ScrollbarY;
      snapshot.body_bounds = ImRect(
          body->Pos,
          ImVec2(body->Pos.x + body->Size.x, body->Pos.y + body->Size.y));
    }
    return snapshot;
  }

  static std::string BuildDrawIssueReport(DungeonCanvasViewer& viewer,
                                          const zelda3::Room& room,
                                          int room_id) {
    return viewer.BuildDrawIssueReport(room, room_id);
  }

  static std::string BuildSelectionIssueReport(
      const DungeonCanvasViewer& viewer, const zelda3::Room& room,
      int room_id) {
    return viewer.BuildSelectionIssueReport(room, room_id);
  }

  static DungeonCanvasViewer::ConnectedRoomGraphData BuildConnectedRoomGraph(
      DungeonCanvasViewer& viewer, int start_room_id) {
    return viewer.BuildConnectedRoomGraph(start_room_id);
  }

  static int ApplyConnectedStaircaseIssueAutoFixes(DungeonCanvasViewer& viewer,
                                                   int start_room_id) {
    return viewer.ApplyConnectedStaircaseIssueAutoFixes(start_room_id);
  }

  static size_t ChangePingRectCount(const DungeonCanvasViewer& viewer) {
    return viewer.change_ping_rects_.size();
  }

  static PingRectSnapshot ChangePingRectAt(const DungeonCanvasViewer& viewer,
                                           size_t index) {
    const auto& rect = viewer.change_ping_rects_.at(index);
    return PingRectSnapshot{rect.x, rect.y, rect.w, rect.h};
  }

  static double ChangePingStartTime(const DungeonCanvasViewer& viewer) {
    return viewer.change_ping_start_time_;
  }

  static bool InvokeDisplayContextMenuItem(DungeonCanvasViewer& viewer,
                                           int room_id, const char* label) {
    viewer.canvas_.ClearContextMenuItems();
    viewer.AddLoadedRoomContextMenuItems(room_id);
    // Copy before dispatch: a Fit callback can refresh the room composite.
    const auto menu = viewer.canvas_.editor_menu();
    for (const auto& section : menu.sections) {
      for (const auto& item : section.items) {
        if (item.label != "Display")
          continue;
        for (const auto& action : item.subitems) {
          if (action.label == label && action.callback) {
            action.callback();
            return true;
          }
        }
      }
    }
    return false;
  }

  static std::vector<std::string> InteractionContextMenuLabels(
      DungeonCanvasViewer& viewer, int room_id) {
    viewer.canvas_.ClearContextMenuItems();
    viewer.AddInteractionContextMenuItems(room_id);
    std::vector<std::string> labels;
    for (const auto& section : viewer.canvas_.editor_menu().sections) {
      for (const auto& item : section.items) {
        labels.push_back(item.label);
      }
    }
    return labels;
  }

  static std::vector<std::string> SelectionContextMenuLabels(
      DungeonCanvasViewer& viewer, int room_id) {
    std::vector<std::string> labels;
    const auto collect =
        [&](auto&& self,
            const std::vector<gui::CanvasMenuItem>& items) -> void {
      for (const auto& item : items) {
        labels.push_back(item.label);
        self(self, item.subitems);
      }
    };
    collect(collect, viewer.BuildSelectionContextMenuItems(room_id));
    return labels;
  }

  static std::vector<gui::CanvasMenuItem> SelectionContextMenuItems(
      DungeonCanvasViewer& viewer, int room_id,
      std::optional<zelda3::RoomObject> context_object) {
    return viewer.BuildSelectionContextMenuItems(room_id,
                                                 std::move(context_object));
  }

  static bool InvokeSelectionContextMenuItem(DungeonCanvasViewer& viewer,
                                             int room_id,
                                             const std::string& label) {
    for (auto& item : viewer.BuildSelectionContextMenuItems(room_id)) {
      if (item.label == label && item.callback) {
        item.callback();
        return true;
      }
    }
    return false;
  }

  static PendingScrollFrameSnapshot ConsumePendingScrollAndCaptureTransforms(
      DungeonCanvasViewer& viewer, int tile_x, int tile_y, float scale,
      ImVec2 initial_scroll, ImVec2 canvas_size) {
    viewer.canvas_.set_global_scale(scale);
    viewer.canvas_.set_scrolling(initial_scroll);
    viewer.ScrollToTile(tile_x, tile_y);

    gui::CanvasFrameOptions frame_opts;
    frame_opts.canvas_size = canvas_size;
    frame_opts.draw_context_menu = false;
    frame_opts.draw_grid = false;
    frame_opts.draw_overlay = false;
    frame_opts.render_popups = false;

    auto canvas_rt = gui::BeginCanvas(viewer.canvas_, frame_opts);
    viewer.ConsumePendingCanvasScroll(canvas_rt);
    canvas_rt.scrolling = viewer.canvas_.scrolling();

    gfx::Bitmap bitmap(16, 16, 8, std::vector<uint8_t>(16 * 16, 0));
    bitmap.set_texture(&bitmap);
    const int bitmap_vertex_start = canvas_rt.draw_list->VtxBuffer.Size;
    viewer.canvas_.DrawBitmap(bitmap, 0, 0, scale, 255);

    PendingScrollFrameSnapshot snapshot;
    snapshot.canvas_origin = canvas_rt.canvas_p0;
    snapshot.applied_scroll = canvas_rt.scrolling;
    if (canvas_rt.draw_list->VtxBuffer.Size >= bitmap_vertex_start + 4) {
      snapshot.background_origin =
          canvas_rt.draw_list->VtxBuffer[bitmap_vertex_start].pos;
    }

    const DungeonCanvasTransform overlay_and_input_transform(
        canvas_rt.canvas_p0, canvas_rt.scrolling, canvas_rt.scale);
    snapshot.overlay_origin =
        overlay_and_input_transform.RoomPixelsToScreen(ImVec2(0, 0));
    snapshot.background_origin_room_pixel =
        overlay_and_input_transform.ScreenToRoomPixelCoordinates(
            snapshot.background_origin);

    gui::EndCanvas(viewer.canvas_, canvas_rt, frame_opts);
    return snapshot;
  }
};

namespace {

class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* name, const std::string& value) : name_(name) {
    const char* existing = std::getenv(name_.c_str());
    if (existing) {
      had_original_ = true;
      original_value_ = existing;
    }
    SetEnv(value);
  }

  ~ScopedEnvVar() {
    if (had_original_) {
      SetEnv(original_value_);
    } else {
      UnsetEnv();
    }
  }

 private:
  void SetEnv(const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name_.c_str(), value.c_str());
#else
    setenv(name_.c_str(), value.c_str(), 1);
#endif
  }

  void UnsetEnv() {
#if defined(_WIN32)
    _putenv_s(name_.c_str(), "");
#else
    unsetenv(name_.c_str());
#endif
  }

  std::string name_;
  std::string original_value_;
  bool had_original_ = false;
};

std::filesystem::path MakeTempHomeRoot() {
  return std::filesystem::temp_directory_path() /
         std::filesystem::path(
             "yaze_dungeon_canvas_viewer_test_" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
}

struct ScopedImGuiContext {
  ImGuiContext* context = nullptr;

  ScopedImGuiContext() {
    context = ImGui::CreateContext();
    ImGui::GetIO().DisplaySize = ImVec2(1024, 768);
  }

  ~ScopedImGuiContext() {
    if (context != nullptr) {
      ImGui::DestroyContext(context);
    }
  }
};

// Exercise wheel routing through the real host hierarchy. A direct interaction
// call after NewFrame cannot detect ImGui scrolling an ancestor first.
class DungeonCanvasWheelRoutingTest : public ::testing::Test {
 protected:
  enum class Host { Workbench, Compare, Standalone };
  struct Snapshot {
    ImRect viewport;
    ImRect canvas_bounds;
    ImRect bitmap;
    ImVec2 pan;
    ImVec2 remaining_wheel;
    std::vector<std::pair<std::string, ImVec2>> ancestor_scrolls;
  };

  void SetUp() override {
    old_custom_objects_ = core::FeatureFlags::get().kEnableCustomObjects;
    core::FeatureFlags::get().kEnableCustomObjects = false;
    imgui_ = std::make_unique<ScopedImGuiContext>();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigInputTrickleEventQueue = false;
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels;
    int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    gfx::Arena::Get().ClearTextureQueue();
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    const int layout = SnesToPc(zelda3::kRoomLayoutPointers.front());
    ASSERT_TRUE(rom_.WriteWord(layout, 0xFFFF).ok());
    ASSERT_TRUE(rom_.WriteWord(zelda3::kRoomsSpritePointer, 0x8000).ok());
    ASSERT_TRUE(rom_.WriteVector(0x49000, {0x00, 0xFF}).ok());
    ASSERT_TRUE(rom_.WriteWord(0xFF00, 0xFFFF).ok());
    data_.graphics_buffer.assign(zelda3::kNumGfxSheets * 4096, 0);
    data_.palette_groups.dungeon_main.AddPalette(gfx::SnesPalette(
        std::vector<gfx::SnesColor>(90, gfx::SnesColor(64, 96, 128))));
    data_.palette_groups.hud.AddPalette(gfx::SnesPalette(
        std::vector<gfx::SnesColor>(32, gfx::SnesColor(0, 0, 0))));
    for (int id : {0, 1}) {
      ASSERT_TRUE(rom_.WriteWord(0x48000 + id * 2, 0x9000).ok());
      ASSERT_TRUE(
          rom_.WriteWord(zelda3::kRoomItemsPointers + id * 2, 0xFF00).ok());
      auto& room = rooms_[id];
      room.SetLoaded(true);
      room.SetTileObjects({zelda3::RoomObject(0x01, 5, 5, 3, 0)});
      room.LoadSprites();
      room.LoadPotItems();
      room.ClearSaveDirtyState();
    }
    primary_ = std::make_unique<DungeonCanvasViewer>();
    compare_ = std::make_unique<DungeonCanvasViewer>();
    for (auto* viewer : {primary_.get(), compare_.get()}) {
      const int room = viewer == primary_.get() ? 0 : 1;
      viewer->RefreshRomBackedState(&rom_, &data_, &rooms_, room);
      viewer->SetHeaderVisible(false);
      viewer->SetHeaderHiddenMetadataHudVisible(false);
      viewer->set_show_coordinate_overlay(false);
      viewer->set_show_grid(false);
      viewer->SetSpritesVisible(false);
      viewer->SetPotItemsVisible(false);
    }
    compare_->SetObjectInteractionEnabled(false);
    room_selector_.SetGameData(&data_);
    room_selector_.set_rooms(&rooms_);
    content_ = std::make_unique<DungeonWorkbenchContent>(
        &room_selector_, &room_id_, [](int) {}, [](int, RoomSelectionIntent) {},
        [](int) {}, [] {}, [this] { return primary_.get(); },
        [this](int) { return compare_.get(); },
        [this]() -> const std::deque<int>& { return recent_; }, [](int) {},
        [](bool) {}, &rom_);
  }

  void TearDown() override {
    content_.reset();
    primary_.reset();
    compare_.reset();
    rooms_.Clear();
    gfx::Arena::Get().ClearTextureQueue();
    core::FeatureFlags::get().kEnableCustomObjects = old_custom_objects_;
    imgui_.reset();
  }

  Snapshot Capture(DungeonCanvasViewer& viewer) {
    Snapshot result;
    const auto& canvas = viewer.canvas();
    auto* draw_list = canvas.draw_list();
    EXPECT_NE(draw_list, nullptr);
    if (!draw_list)
      return result;
    auto* owner = ImGui::FindWindowByName(draw_list->_OwnerName);
    EXPECT_NE(owner, nullptr);
    if (!owner)
      return result;
    result.viewport = owner->InnerClipRect;
    result.pan = canvas.scrolling();
    const auto& composite =
        DungeonCanvasViewerTestPeer::PrimaryComposite(viewer);
    EXPECT_EQ(composite.width(), 512);
    EXPECT_EQ(composite.height(), 512);
    EXPECT_NE(composite.surface(), nullptr);
    const auto origin = canvas.zero_point();
    result.canvas_bounds = ImRect(
        origin,
        ImVec2(origin.x + canvas.canvas_size().x * canvas.global_scale(),
               origin.y + canvas.canvas_size().y * canvas.global_scale()));
    const float size = 512 * canvas.global_scale();
    result.bitmap = ImRect(
        ImVec2(origin.x + result.pan.x, origin.y + result.pan.y),
        ImVec2(origin.x + result.pan.x + size, origin.y + result.pan.y + size));
    for (auto* window = owner; window; window = window->ParentWindow)
      result.ancestor_scrolls.emplace_back(window->Name, window->Scroll);
    result.remaining_wheel =
        ImVec2(ImGui::GetIO().MouseWheelH, ImGui::GetIO().MouseWheel);
    return result;
  }

  Snapshot Frame(DungeonCanvasViewer* target = nullptr) {
    if (!target)
      target = primary_.get();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(host_size_, ImGuiCond_Always);
    ImGui::Begin("CanvasWheelHost", nullptr, host_flags_);
    if (host_ == Host::Standalone)
      primary_->Draw(room_id_);
    else if (product_host_)
      content_->Draw(nullptr);
    else
      DungeonWorkbenchContentTestPeer::DrawCanvasPane(*content_, *primary_, 760,
                                                      460);
    Snapshot result = Capture(*target);
    // A scrollable ancestor makes accidental wheel propagation observable even
    // when the canvas itself happens to fit at the selected scale.
    if (synthetic_overflow_)
      ImGui::Dummy(synthetic_content_size_);
    if (after_canvas_)
      after_canvas_();
    ImGui::End();
    ImGui::Render();
    return result;
  }

  void SetHost(Host host, float scale = 0.0f) {
    host_ = host;
    DungeonWorkbenchContentTestPeer::SetCompare(*content_,
                                                host == Host::Compare);
    primary_->SetHeaderVisible(host == Host::Standalone);
    for (auto* viewer : {primary_.get(), compare_.get()}) {
      if (scale > 0)
        viewer->canvas().set_global_scale(scale);
      viewer->canvas().set_scrolling(ImVec2(-40, -40));
    }
    Frame();
    Frame();
  }

  Snapshot Wheel(DungeonCanvasViewer& viewer, ImVec2 point, float x, float y,
                 bool shift = false) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(point.x, point.y);
    io.AddKeyEvent(ImGuiMod_Shift, shift);
    Frame(&viewer);
    Frame(&viewer);
    io.AddMouseWheelEvent(x, y);
    const auto event = Frame(&viewer);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    Frame(&viewer);
    return event;
  }

  Snapshot EnterWithWheel(DungeonCanvasViewer& viewer, float x, float y) {
    auto& io = ImGui::GetIO();
    // Establish an outside pointer and release prior-frame item ownership.
    // There is deliberately no hovered frame between entry and this wheel.
    io.AddMousePosEvent(1000, 740);
    Frame(&viewer);
    const auto before = Frame(&viewer);
    const auto point = before.viewport.GetCenter();
    io.AddMousePosEvent(point.x, point.y);
    io.AddMouseWheelEvent(x, y);
    return Frame(&viewer);
  }

  void ExpectViewportFits(const Snapshot& frame) {
    EXPECT_GE(frame.canvas_bounds.Min.x, frame.viewport.Min.x - 1);
    EXPECT_GE(frame.canvas_bounds.Min.y, frame.viewport.Min.y - 1);
    EXPECT_LE(frame.canvas_bounds.Max.x, frame.viewport.Max.x + 1);
    EXPECT_LE(frame.canvas_bounds.Max.y, frame.viewport.Max.y + 1);
  }

  void ExpectCentered(const Snapshot& frame) {
    EXPECT_NEAR(frame.bitmap.GetCenter().x, frame.canvas_bounds.GetCenter().x,
                1);
    EXPECT_NEAR(frame.bitmap.GetCenter().y, frame.canvas_bounds.GetCenter().y,
                1);
  }

  void ExpectRoomFits(const Snapshot& frame) {
    ExpectViewportFits(frame);
    ExpectCentered(frame);
    EXPECT_GE(frame.bitmap.Min.x, frame.viewport.Min.x - 1);
    EXPECT_GE(frame.bitmap.Min.y, frame.viewport.Min.y - 1);
    EXPECT_LE(frame.bitmap.Max.x, frame.viewport.Max.x + 1);
    EXPECT_LE(frame.bitmap.Max.y, frame.viewport.Max.y + 1);
    EXPECT_NEAR(frame.bitmap.GetWidth(),
                std::min(frame.canvas_bounds.GetWidth(),
                         frame.canvas_bounds.GetHeight()),
                1);
  }

  void ExpectAttached(const Snapshot& frame) {
    EXPECT_GT(frame.viewport.GetWidth(), 0);
    EXPECT_GT(frame.viewport.GetHeight(), 0);
    EXPECT_GT(frame.bitmap.Max.x, frame.viewport.Min.x);
    EXPECT_GT(frame.bitmap.Max.y, frame.viewport.Min.y);
    EXPECT_LT(frame.bitmap.Min.x, frame.viewport.Max.x);
    EXPECT_LT(frame.bitmap.Min.y, frame.viewport.Max.y);
    for (const auto& [name, scroll] : frame.ancestor_scrolls) {
      EXPECT_FLOAT_EQ(scroll.x, 0) << name;
      EXPECT_FLOAT_EQ(scroll.y, 0) << name;
    }
    EXPECT_FLOAT_EQ(frame.remaining_wheel.x, 0);
    EXPECT_FLOAT_EQ(frame.remaining_wheel.y, 0);
  }

  bool old_custom_objects_ = false;
  std::unique_ptr<ScopedImGuiContext> imgui_;
  Rom rom_;
  zelda3::GameData data_;
  DungeonRoomStore rooms_{&rom_, &data_};
  DungeonRoomSelector room_selector_{&rom_};
  std::unique_ptr<DungeonCanvasViewer> primary_, compare_;
  int room_id_ = 0;
  std::deque<int> recent_{0, 1};
  std::unique_ptr<DungeonWorkbenchContent> content_;
  Host host_ = Host::Workbench;
  ImVec2 host_size_{900, 660};
  ImGuiWindowFlags host_flags_ = ImGuiWindowFlags_NoSavedSettings;
  ImVec2 synthetic_content_size_{0, 800};
  bool product_host_ = false;
  bool synthetic_overflow_ = true;
  std::function<void()> after_canvas_;
};

TEST_F(DungeonCanvasWheelRoutingTest,
       UnselectedWheelKeepsRoomFixedInEveryHost) {
  for (Host host : {Host::Workbench, Host::Compare, Host::Standalone}) {
    SCOPED_TRACE(static_cast<int>(host));
    SetHost(host);
    std::vector<DungeonCanvasViewer*> targets{primary_.get()};
    if (host == Host::Compare)
      targets.push_back(compare_.get());
    for (auto* target : targets) {
      const auto before = Frame(target);
      const auto point = before.viewport.GetCenter();
      const auto other_before = Frame(
          target == primary_.get() && host == Host::Compare ? compare_.get()
                                                            : primary_.get());
      const auto modest = Wheel(*target, point, -1, -1);
      ExpectAttached(modest);
      ExpectRoomFits(modest);
      EXPECT_FLOAT_EQ(modest.pan.x, before.pan.x);
      EXPECT_FLOAT_EQ(modest.pan.y, before.pan.y);
      for (const ImVec2 delta : {ImVec2(-1000, -1000), ImVec2(1000, 1000),
                                 ImVec2(-1000, 0), ImVec2(1000, 0)}) {
        const auto event = Wheel(*target, point, delta.x, delta.y);
        ExpectAttached(event);
        ExpectRoomFits(event);
        EXPECT_FLOAT_EQ(event.pan.x, before.pan.x);
        EXPECT_FLOAT_EQ(event.pan.y, before.pan.y);
        ExpectAttached(Frame(target));
      }
      EXPECT_EQ(
          rooms_[target == primary_.get() ? 0 : 1].GetTileObjects()[0].size_,
          3);
      if (host == Host::Compare) {
        auto* other =
            target == primary_.get() ? compare_.get() : primary_.get();
        const auto after = Frame(other);
        // Input in one Compare viewport never changes the other.
        EXPECT_FLOAT_EQ(after.pan.x, other_before.pan.x);
        EXPECT_FLOAT_EQ(after.pan.y, other_before.pan.y);
      }
    }
  }
}

TEST_F(DungeonCanvasWheelRoutingTest, BlankViewportMarginOwnsTrackpadWheel) {
  for (Host host : {Host::Workbench, Host::Compare, Host::Standalone}) {
    SCOPED_TRACE(static_cast<int>(host));
    SetHost(host, 0.5f);
    primary_->canvas().set_scrolling(ImVec2(0, 0));
    const auto before = Frame();
    const ImVec2 margin(before.viewport.Max.x - 8, before.viewport.Max.y - 8);
    ASSERT_FALSE(before.bitmap.Contains(margin));
    const auto event = Wheel(*primary_, margin, -1, -1);
    ExpectAttached(event);
    ExpectCentered(event);
    EXPECT_FLOAT_EQ(event.pan.x, before.pan.x);
    EXPECT_FLOAT_EQ(event.pan.y, before.pan.y);
    ExpectAttached(Frame());
  }
}

TEST_F(DungeonCanvasWheelRoutingTest,
       SelectedAndPlacementWheelResizeWithoutPan) {
  for (Host host : {Host::Workbench, Host::Compare, Host::Standalone}) {
    SCOPED_TRACE(static_cast<int>(host));
    SetHost(host);
    rooms_[0].GetTileObjects()[0].size_ = 3;
    auto& interaction = primary_->object_interaction();
    interaction.SetSelectedObjects({0});
    const auto before = Frame();
    const auto after = Wheel(*primary_, before.viewport.GetCenter(), 0, 1);
    ExpectAttached(after);
    EXPECT_EQ(rooms_[0].GetTileObjects()[0].size_, 4);
    EXPECT_FLOAT_EQ(after.pan.x, before.pan.x);
    EXPECT_FLOAT_EQ(after.pan.y, before.pan.y);
    const auto shifted =
        Wheel(*primary_, before.viewport.GetCenter(), 1, 0, true);
    ExpectAttached(shifted);
    EXPECT_EQ(rooms_[0].GetTileObjects()[0].size_, 5);
    EXPECT_FLOAT_EQ(shifted.pan.x, before.pan.x);
    EXPECT_FLOAT_EQ(shifted.pan.y, before.pan.y);
    interaction.ClearSelection();
    primary_->SetPreviewObject(zelda3::RoomObject(0x01, 0, 0, 3, 0));
    const auto preview = Wheel(*primary_, before.viewport.GetCenter(), 0, 1);
    ExpectAttached(preview);
    ASSERT_NE(interaction.GetPlacementPreview(), nullptr);
    EXPECT_EQ(interaction.GetPlacementPreview()->size_, 4);
    EXPECT_EQ(rooms_[0].GetTileObjects()[0].size_, 5);
    EXPECT_FLOAT_EQ(preview.pan.x, before.pan.x);
    EXPECT_FLOAT_EQ(preview.pan.y, before.pan.y);
    interaction.CancelPlacement();
  }
}

// This intentionally tests arbitrary scrollable embedding separately from the
// real product host below: a synthetic ancestor must not be mistaken for a
// native Workbench sizing failure.
TEST_F(DungeonCanvasWheelRoutingTest,
       FirstEntryWheelDoesNotScrollSyntheticAncestor) {
  SetHost(Host::Workbench);
  auto* host = ImGui::FindWindowByName("CanvasWheelHost");
  ASSERT_NE(host, nullptr);
  ASSERT_GT(host->ScrollMax.y, 0);
  const auto before = Frame();
  const auto event = EnterWithWheel(*primary_, 0, -1);
  ExpectAttached(event);
  EXPECT_FLOAT_EQ(event.pan.x, before.pan.x);
  EXPECT_FLOAT_EQ(event.pan.y, before.pan.y);
  ExpectAttached(Frame());
}

TEST_F(DungeonCanvasWheelRoutingTest,
       WheelOutsideViewportStillScrollsSyntheticAncestor) {
  SetHost(Host::Workbench);
  auto* host = ImGui::FindWindowByName("CanvasWheelHost");
  ASSERT_NE(host, nullptr);
  ASSERT_GT(host->ScrollMax.y, 0);
  const auto before = Frame();
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(before.viewport.GetCenter().x,
                      before.viewport.GetCenter().y);
  Frame();
  Frame();
  const ImVec2 outside(host->InnerRect.Max.x - 24,
                       before.viewport.GetCenter().y);
  ASSERT_FALSE(before.viewport.Contains(outside));
  ASSERT_TRUE(host->InnerRect.Contains(outside));
  io.AddMousePosEvent(outside.x, outside.y);
  // Release ordinary previous-frame item ownership before checking the hook's
  // outside-viewport behavior. First-entry ownership is tested separately.
  Frame();
  Frame();
  ASSERT_EQ(ImGui::GetCurrentContext()->HoveredWindow, host);
  io.AddMouseWheelEvent(0, -1);
  const auto event = Frame();
  EXPECT_GT(host->Scroll.y, 0);
  EXPECT_FLOAT_EQ(event.pan.x, before.pan.x);
  EXPECT_FLOAT_EQ(event.pan.y, before.pan.y);
  const float native_scroll = host->Scroll.y;
  Frame();
  EXPECT_FLOAT_EQ(host->Scroll.y, native_scroll);
}

TEST_F(DungeonCanvasWheelRoutingTest,
       FirstEntryWheelPreservesUnrelatedNativeScrollTarget) {
  host_flags_ |= ImGuiWindowFlags_HorizontalScrollbar;
  synthetic_content_size_ = ImVec2(1400, 800);
  SetHost(Host::Workbench);
  auto* host = ImGui::FindWindowByName("CanvasWheelHost");
  ASSERT_NE(host, nullptr);
  ASSERT_GT(host->ScrollMax.x, 0);
  ASSERT_GT(host->ScrollMax.y, 64);
  auto& io = ImGui::GetIO();
  io.ConfigMacOSXBehaviors = false;  // Exercise ImGui's Shift/Y-to-X mapping.
  for (int mode : {0, 1, 2}) {
    SCOPED_TRACE(mode);  // Native X, Shift+Y, diagonal with dominant X.
    io.AddKeyEvent(ImGuiMod_Shift, false);
    io.AddMousePosEvent(1000, 740);
    ImGui::SetScrollX(host, 0);
    ImGui::SetScrollY(host, 0);
    Frame();
    const auto before = Frame();
    const auto point = before.viewport.GetCenter();
    // This intentional Y target must survive cancellation of native X wheel.
    ImGui::SetScrollY(host, 64);
    io.AddKeyEvent(ImGuiMod_Shift, mode == 1);
    io.AddMousePosEvent(point.x, point.y);
    const ImVec2 wheel = mode == 0   ? ImVec2(-1, 0)
                         : mode == 1 ? ImVec2(0, -1)
                                     : ImVec2(-4, -1);
    io.AddMouseWheelEvent(wheel.x, wheel.y);
    const auto event = Frame();
    EXPECT_EQ(ImGui::GetCurrentContext()->WheelingWindowScrolledFrame,
              ImGui::GetFrameCount());
    EXPECT_FLOAT_EQ(host->Scroll.x, 0);
    EXPECT_FLOAT_EQ(host->Scroll.y, 64);
    EXPECT_FLOAT_EQ(event.remaining_wheel.x, 0);
    EXPECT_FLOAT_EQ(event.remaining_wheel.y, 0);
    EXPECT_FLOAT_EQ(event.pan.x, before.pan.x);
    EXPECT_FLOAT_EQ(event.pan.y, before.pan.y);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    Frame();
    EXPECT_FLOAT_EQ(host->Scroll.x, 0);
    EXPECT_FLOAT_EQ(host->Scroll.y, 64);
  }
}

TEST_F(DungeonCanvasWheelRoutingTest, ProductHostsFitWithoutSyntheticOverflow) {
  product_host_ = true;
  synthetic_overflow_ = false;
  for (Host kind : {Host::Workbench, Host::Compare, Host::Standalone}) {
    SCOPED_TRACE(static_cast<int>(kind));
    SetHost(kind);
    auto* host = ImGui::FindWindowByName("CanvasWheelHost");
    ASSERT_NE(host, nullptr);
    EXPECT_FLOAT_EQ(host->ScrollMax.x, 0);
    EXPECT_FLOAT_EQ(host->ScrollMax.y, 0);
    std::vector<DungeonCanvasViewer*> targets{primary_.get()};
    if (kind == Host::Compare)
      targets.push_back(compare_.get());
    for (auto* target : targets) {
      ExpectRoomFits(Frame(target));
      const auto event = EnterWithWheel(*target, 0, -1);
      ExpectRoomFits(event);
      ExpectAttached(event);
      ExpectAttached(Frame(target));
    }
  }
}

TEST_F(DungeonCanvasWheelRoutingTest, ShortProductCompareUsesVisibleViewport) {
  product_host_ = true;
  synthetic_overflow_ = false;
  host_size_ = ImVec2(900, 260);
  SetHost(Host::Compare);
  for (auto* target : {primary_.get(), compare_.get()}) {
    const auto before = Frame(target);
    ASSERT_GT(before.viewport.GetHeight(), 0);
    ASSERT_LT(before.viewport.GetHeight(), 200);
    ExpectRoomFits(before);
    for (float delta : {-1000.0f, 1000.0f}) {
      const auto event = Wheel(*target, before.viewport.GetCenter(), 0, delta);
      ExpectAttached(event);
      ExpectRoomFits(event);
      EXPECT_FLOAT_EQ(event.pan.x, before.pan.x);
      EXPECT_FLOAT_EQ(event.pan.y, before.pan.y);
    }
  }
}

TEST_F(DungeonCanvasWheelRoutingTest, AutomaticFitTracksPaneAndCompareResize) {
  product_host_ = true;
  synthetic_overflow_ = false;
  for (Host kind : {Host::Workbench, Host::Compare, Host::Standalone}) {
    SCOPED_TRACE(static_cast<int>(kind));
    SetHost(kind);
    for (const ImVec2 size :
         {ImVec2(900, 660), ImVec2(800, 520), ImVec2(960, 720)}) {
      host_size_ = size;
      Frame();
      ExpectRoomFits(Frame());
      if (kind == Host::Compare)
        ExpectRoomFits(Frame(compare_.get()));
    }
  }
}

TEST_F(DungeonCanvasWheelRoutingTest,
       ExplicitZoomStaysCenteredAndFitRestoresAutomaticSizing) {
  product_host_ = true;
  synthetic_overflow_ = false;
  auto& io = ImGui::GetIO();
  for (Host kind : {Host::Workbench, Host::Compare, Host::Standalone}) {
    SCOPED_TRACE(static_cast<int>(kind));
    host_size_ = ImVec2(900, 660);
    // Also proves a non-default explicit zoom supplied before the first draw
    // is preserved rather than being mistaken for the Canvas 1x default.
    SetHost(kind, 2.0f);
    const auto zoomed = Frame();
    EXPECT_FLOAT_EQ(primary_->canvas().global_scale(), 2.0f);
    EXPECT_GT(zoomed.bitmap.GetHeight(), zoomed.viewport.GetHeight());
    ExpectCentered(zoomed);
    const auto wheel = Wheel(*primary_, zoomed.viewport.GetCenter(), -3, -3);
    EXPECT_FLOAT_EQ(wheel.pan.x, zoomed.pan.x);
    EXPECT_FLOAT_EQ(wheel.pan.y, zoomed.pan.y);
    for (ImGuiMouseButton button :
         {ImGuiMouseButton_Middle, ImGuiMouseButton_Right}) {
      const auto center = zoomed.viewport.GetCenter();
      io.AddMousePosEvent(center.x, center.y);
      Frame();
      Frame();
      io.AddMouseButtonEvent(button, true);
      Frame();
      io.AddMousePosEvent(center.x + 45, center.y + 25);
      const auto dragged = Frame();
      EXPECT_FLOAT_EQ(dragged.pan.x, zoomed.pan.x);
      EXPECT_FLOAT_EQ(dragged.pan.y, zoomed.pan.y);
      ExpectCentered(dragged);
      io.AddMouseButtonEvent(button, false);
      Frame();
    }
    host_size_ = ImVec2(840, 600);
    Frame();
    ExpectCentered(Frame());
    EXPECT_FLOAT_EQ(primary_->canvas().global_scale(), 2.0f);
    EXPECT_FALSE(primary_->canvas().GetConfig().show_builtin_context_menu);
    ASSERT_TRUE(DungeonCanvasViewerTestPeer::InvokeDisplayContextMenuItem(
        *primary_, primary_->current_room_id(), "Fit room"));
    ExpectRoomFits(Frame());
    host_size_ = ImVec2(960, 720);
    Frame();
    ExpectRoomFits(Frame());
  }
}

TEST_F(DungeonCanvasWheelRoutingTest, RepeatedZoomOutKeepsRoomVisible) {
  product_host_ = true;
  synthetic_overflow_ = false;
  SetHost(Host::Workbench, 2.0f);
  Frame();
  for (int i = 0; i < 12; ++i) {
    const float before = primary_->canvas().global_scale();
    ASSERT_TRUE(DungeonCanvasViewerTestPeer::InvokeDisplayContextMenuItem(
        *primary_, primary_->current_room_id(), "Zoom out"));
    EXPECT_GT(primary_->canvas().global_scale(), 0.0f);
    EXPECT_LE(primary_->canvas().global_scale(), before);
    EXPECT_FLOAT_EQ(primary_->canvas().GetConfig().global_scale,
                    primary_->canvas().global_scale());
    ExpectCentered(Frame());
  }
  ASSERT_TRUE(DungeonCanvasViewerTestPeer::InvokeDisplayContextMenuItem(
      *primary_, primary_->current_room_id(), "Fit room"));
  ExpectRoomFits(Frame());

  // Short Compare panes can fit below the regular 0.25 zoom floor.
  // Zoom Out must not make such a fitted room larger or invert its geometry.
  host_size_ = ImVec2(900, 220);
  SetHost(Host::Compare);
  ExpectRoomFits(Frame());
  const float fitted = primary_->canvas().global_scale();
  ASSERT_GT(fitted, 0.0f);
  ASSERT_LT(fitted, 0.25f);
  primary_->canvas().ZoomOut();
  EXPECT_FLOAT_EQ(primary_->canvas().global_scale(), fitted);
  ExpectRoomFits(Frame());
}

TEST_F(DungeonCanvasWheelRoutingTest, SmallPaneZoomIsNotMistakenForFit) {
  product_host_ = true;
  synthetic_overflow_ = false;
  host_size_ = ImVec2(900, 220);
  SetHost(Host::Compare);
  ExpectRoomFits(Frame());
  ASSERT_LT(primary_->canvas().global_scale(), 0.25f);
  ASSERT_TRUE(DungeonCanvasViewerTestPeer::InvokeDisplayContextMenuItem(
      *primary_, primary_->current_room_id(), "Zoom in"));
  Frame();
  ASSERT_TRUE(DungeonCanvasViewerTestPeer::InvokeDisplayContextMenuItem(
      *primary_, primary_->current_room_id(), "Zoom out"));
  ExpectCentered(Frame());
  EXPECT_FLOAT_EQ(primary_->canvas().global_scale(), 0.25f);
  host_size_ = ImVec2(900, 660);
  Frame();
  ExpectCentered(Frame());
  EXPECT_FLOAT_EQ(primary_->canvas().global_scale(), 0.25f);
  ASSERT_TRUE(DungeonCanvasViewerTestPeer::InvokeDisplayContextMenuItem(
      *primary_, primary_->current_room_id(), "Fit room"));
  ExpectRoomFits(Frame());
  host_size_ = ImVec2(960, 720);
  Frame();
  ExpectRoomFits(Frame());
}

TEST_F(DungeonCanvasWheelRoutingTest, OffscreenSelectionFocusReturnsToFit) {
  product_host_ = true;
  synthetic_overflow_ = false;
  for (Host kind : {Host::Workbench, Host::Compare, Host::Standalone}) {
    SCOPED_TRACE(static_cast<int>(kind));
    host_size_ = ImVec2(900, 660);
    SetHost(kind, 2.0f);
    primary_->ScrollToTile(32, 32);
    const auto central = Frame();
    EXPECT_FALSE(primary_->HasPendingScrollTarget());
    EXPECT_FLOAT_EQ(primary_->canvas().global_scale(), 2.0f);
    ExpectCentered(central);

    primary_->object_interaction().SetSelectedObjects({0});
    const auto& object = rooms_[0].GetTileObjects()[0];
    const ImVec2 before_target(central.bitmap.Min.x + object.x_ * 8 * 2,
                               central.bitmap.Min.y + object.y_ * 8 * 2);
    ASSERT_FALSE(central.viewport.Contains(before_target));
    primary_->ScrollToTile(object.x_, object.y_);
    const auto focused = Frame();
    EXPECT_FALSE(primary_->HasPendingScrollTarget());
    ExpectRoomFits(focused);
    EXPECT_EQ(primary_->object_interaction().GetSelectedObjectIndices(),
              (std::vector<size_t>{0}));
    const float tile_size = 8 * primary_->canvas().global_scale();
    const ImVec2 target(focused.bitmap.Min.x + object.x_ * tile_size,
                        focused.bitmap.Min.y + object.y_ * tile_size);
    EXPECT_TRUE(focused.viewport.Contains(target));
    EXPECT_TRUE(focused.viewport.Contains(
        ImVec2(target.x + tile_size, target.y + tile_size)));
    host_size_ = ImVec2(960, 720);
    Frame();
    ExpectRoomFits(Frame());
    primary_->object_interaction().ClearSelection();
  }
}

TEST_F(DungeonCanvasWheelRoutingTest,
       FitReturnsToAutomaticSizingBelowCanvasFloor) {
  product_host_ = true;
  synthetic_overflow_ = false;
  host_size_ = ImVec2(900, 220);
  SetHost(Host::Compare, 2.0f);
  const auto zoomed = Frame();
  ASSERT_LT(std::min(zoomed.canvas_bounds.GetWidth(),
                     zoomed.canvas_bounds.GetHeight()),
            128);
  ASSERT_TRUE(DungeonCanvasViewerTestPeer::InvokeDisplayContextMenuItem(
      *primary_, primary_->current_room_id(), "Fit room"));
  EXPECT_FLOAT_EQ(primary_->canvas().global_scale(), 0.25f);
  ExpectRoomFits(Frame());
  EXPECT_LT(primary_->canvas().global_scale(), 0.25f);
  host_size_ = ImVec2(900, 660);
  Frame();
  ExpectRoomFits(Frame());
}

TEST_F(DungeonCanvasWheelRoutingTest,
       ZoomToFitUsesRenderedViewportFromAnotherWindow) {
  product_host_ = true;
  synthetic_overflow_ = false;
  SetHost(Host::Workbench, 2.0f);
  const auto before = Frame();
  const auto expected = gui::ComputeZoomToFit(
      ImVec2(512, 512), before.canvas_bounds.GetSize(), 0);
  ASSERT_GT(expected.scale, 0.25f);
  ImVec2 popup_available;
  after_canvas_ = [&] {
    ImGui::SetNextWindowPos(ImVec2(900, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(120, 100), ImGuiCond_Always);
    ImGui::Begin("UnrelatedFitContext", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    popup_available = ImGui::GetContentRegionAvail();
    primary_->canvas().SetZoomToFit(
        DungeonCanvasViewerTestPeer::PrimaryComposite(*primary_));
    ImGui::End();
  };
  Frame();
  after_canvas_ = {};
  EXPECT_LT(popup_available.y, before.canvas_bounds.GetHeight());
  const auto& canvas = primary_->canvas();
  EXPECT_FLOAT_EQ(canvas.global_scale(), expected.scale);
  EXPECT_FLOAT_EQ(canvas.scrolling().x, expected.scroll.x);
  EXPECT_FLOAT_EQ(canvas.scrolling().y, expected.scroll.y);
  EXPECT_FLOAT_EQ(canvas.GetConfig().scrolling.x, expected.scroll.x);
  EXPECT_FLOAT_EQ(canvas.GetConfig().scrolling.y, expected.scroll.y);
  const auto fitted = Frame();
  ExpectAttached(fitted);
  ExpectViewportFits(fitted);
  EXPECT_NEAR(fitted.bitmap.GetCenter().x, fitted.canvas_bounds.GetCenter().x,
              1);
  EXPECT_NEAR(fitted.bitmap.GetCenter().y, fitted.canvas_bounds.GetCenter().y,
              1);
  EXPECT_GE(fitted.bitmap.Min.x, fitted.viewport.Min.x - 1);
  EXPECT_GE(fitted.bitmap.Min.y, fitted.viewport.Min.y - 1);
  EXPECT_LE(fitted.bitmap.Max.x, fitted.viewport.Max.x + 1);
  EXPECT_LE(fitted.bitmap.Max.y, fitted.viewport.Max.y + 1);
}

TEST(DungeonCanvasFitTest, LegacyCanvasFitsHostFromPopupWithoutZoomGrowth) {
  ScopedImGuiContext imgui;
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DeltaTime = 1.0f / 60.0f;
  unsigned char* font_pixels = nullptr;
  int font_width = 0, font_height = 0;
  io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
  gui::Canvas canvas;
  canvas.Init("LegacyOversizedCanvas", ImVec2(1024, 512));
  canvas.set_global_scale(2.0f);
  gfx::Bitmap bitmap(1024, 512, 8, std::vector<uint8_t>(1024 * 512, 0));
  ASSERT_TRUE(bitmap.is_active());
  ImVec2 visible_size;
  ImVec2 popup_size;
  const auto draw_frame = [&](bool fit) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(640, 360), ImGuiCond_Always);
    ImGui::Begin("LegacyCanvasHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse);
    const auto origin = ImGui::GetCursorScreenPos();
    const auto available = ImGui::GetContentRegionAvail();
    ImRect visible(origin,
                   ImVec2(origin.x + available.x, origin.y + available.y));
    visible.ClipWith(ImGui::GetCurrentWindow()->InnerClipRect);
    visible_size = visible.GetSize();
    // No optional fixed viewport: the canvas item uses oversized content.
    canvas.DrawBackground(ImVec2(1024, 512));
    if (fit) {
      ImGui::OpenPopup("UnrelatedLegacyFitPopup");
      ImGui::SetNextWindowPos(ImVec2(700, 20), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(120, 100), ImGuiCond_Always);
      const bool opened = ImGui::BeginPopup("UnrelatedLegacyFitPopup");
      EXPECT_TRUE(opened);
      if (opened) {
        popup_size = ImGui::GetContentRegionAvail();
        canvas.SetZoomToFit(bitmap);
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
      }
    }
    ImGui::End();
    ImGui::Render();
  };
  draw_frame(false);
  draw_frame(false);
  ASSERT_LT(visible_size.x, 1024);
  ASSERT_LT(visible_size.y, 512);
  const float expected = std::min(visible_size.x / 1024, visible_size.y / 512);
  ASSERT_GT(expected, 0.25f);
  for (int iteration = 0; iteration < 3; ++iteration) {
    SCOPED_TRACE(iteration);
    draw_frame(true);
    EXPECT_LT(popup_size.x, visible_size.x);
    EXPECT_LT(popup_size.y, visible_size.y);
    EXPECT_FLOAT_EQ(canvas.GetGlobalScale(), expected);
    EXPECT_FLOAT_EQ(canvas.GetConfig().global_scale, expected);
    const ImVec2 centered((visible_size.x - 1024 * expected) * 0.5f,
                          (visible_size.y - 512 * expected) * 0.5f);
    EXPECT_FLOAT_EQ(canvas.scrolling().x, centered.x);
    EXPECT_FLOAT_EQ(canvas.scrolling().y, centered.y);
    EXPECT_FLOAT_EQ(canvas.GetConfig().scrolling.x, centered.x);
    EXPECT_FLOAT_EQ(canvas.GetConfig().scrolling.y, centered.y);
    // These existing commands read the legacy scale: both scale paths must
    // agree after Fit, and a zoom-in/out round trip must preserve that fit.
    canvas.ZoomIn();
    EXPECT_NEAR(canvas.GetGlobalScale(), expected + 0.25f, 0.00001f);
    canvas.ZoomOut();
    EXPECT_NEAR(canvas.GetGlobalScale(), expected, 0.00001f);
  }
}

size_t FindLabelIndex(const std::vector<std::string>& labels,
                      const std::string& label) {
  const auto it = std::find(labels.begin(), labels.end(), label);
  return it == labels.end() ? labels.size()
                            : static_cast<size_t>(it - labels.begin());
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

zelda3::Room::Door MakeDoor(zelda3::DoorDirection direction,
                            zelda3::DoorType type) {
  zelda3::Room::Door door{};
  door.direction = direction;
  door.type = type;
  door.position = 0;
  door.byte1 = 0;
  door.byte2 = static_cast<uint8_t>(type);
  return door;
}

std::vector<gfx::TileInfo> MakeObjectTiles(int count) {
  std::vector<gfx::TileInfo> tiles;
  tiles.reserve(count);
  for (int i = 0; i < count; ++i) {
    tiles.emplace_back(static_cast<uint16_t>(0x120 + i),
                       static_cast<uint8_t>(i % 8), false, (i % 2) == 0,
                       (i % 3) == 0);
  }
  return tiles;
}

void ClearRoomLinks(zelda3::Room* room) {
  for (int i = 0; i < 4; ++i) {
    room->SetStaircaseRoom(i, 0);
  }
  room->SetHolewarp(0);
}

}  // namespace

TEST(DungeonMapPanelTest, LoadFromDungeonEntryReplacesAllRoomDerivedState) {
  int current_room_id = 0x10;
  ImVector<int> active_rooms;
  DungeonMapPanel panel(&current_room_id, &active_rooms, nullptr);

  core::DungeonEntry first;
  first.name = "First";
  core::DungeonRoom first_room{};
  first_room.id = 0x10;
  first_room.type = "boss";
  first.rooms.push_back(first_room);
  core::DungeonConnection stair{};
  stair.from_room = 0x10;
  stair.to_room = 0x11;
  first.stairs.push_back(stair);
  core::DungeonConnection holewarp{};
  holewarp.from_room = 0x10;
  holewarp.to_room = 0x12;
  first.holewarps.push_back(holewarp);
  panel.LoadFromDungeonEntry(first);

  ASSERT_EQ(DungeonMapPanelTestPeer::RoomCount(panel), 1u);
  ASSERT_TRUE(DungeonMapPanelTestPeer::HasRoomType(panel, 0x10));
  ASSERT_EQ(DungeonMapPanelTestPeer::StairConnectionCount(panel), 1u);
  ASSERT_EQ(DungeonMapPanelTestPeer::HolewarpConnectionCount(panel), 1u);
  DungeonMapPanelTestPeer::SeedCompositeOutput(panel, 0x10);
  ASSERT_EQ(DungeonMapPanelTestPeer::CompositeOutputCount(panel), 1u);

  core::DungeonEntry second;
  second.name = "Second";
  core::DungeonRoom second_room{};
  second_room.id = 0x20;
  second_room.type = "entrance";
  second.rooms.push_back(second_room);
  panel.LoadFromDungeonEntry(second);

  EXPECT_EQ(DungeonMapPanelTestPeer::RoomCount(panel), 1u);
  EXPECT_FALSE(DungeonMapPanelTestPeer::HasRoomType(panel, 0x10));
  EXPECT_TRUE(DungeonMapPanelTestPeer::HasRoomType(panel, 0x20));
  EXPECT_EQ(DungeonMapPanelTestPeer::StairConnectionCount(panel), 0u);
  EXPECT_EQ(DungeonMapPanelTestPeer::HolewarpConnectionCount(panel), 0u);
  EXPECT_EQ(DungeonMapPanelTestPeer::CompositeOutputCount(panel), 0u);
}

TEST(DungeonMapPanelTest, DistantRoomRemainsClickableInsideSmallScrolledMap) {
  ScopedImGuiContext imgui;
  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = 1.0f / 60.0f;
  io.Fonts->AddFontDefault();
  unsigned char* font_pixels = nullptr;
  int font_width = 0;
  int font_height = 0;
  io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

  int current_room_id = 0x10;
  int selected_room_id = -1;
  int selection_count = 0;
  ImVector<int> active_rooms;
  DungeonMapPanel panel(&current_room_id, &active_rooms, [&](int room_id) {
    selected_room_id = room_id;
    ++selection_count;
  });
  panel.SetDungeonRooms({0x10, 0x11});
  panel.SetRoomPosition(0x10, 0, 0);
  panel.SetRoomPosition(0x11, 12, 12);

  struct MapFrame {
    ImGuiWindow* child = nullptr;
    ImRect clip;
    ImRect footer;
    ImRect host_content;
    ImVec2 room_center;
  };
  auto draw_frame = [&]() -> MapFrame {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 260.0f), ImGuiCond_Always);
    ImGui::Begin("SmallDungeonMapHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    auto* host = ImGui::GetCurrentWindow();
    panel.Draw(nullptr);
    MapFrame result;
    result.host_content = host->InnerRect;
    result.footer = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    for (auto* child : host->DC.ChildWindows) {
      if (std::string(child->Name).find("##DungeonMapCanvas") !=
          std::string::npos) {
        result.child = child;
        result.clip = child->InnerClipRect;
        // The room buttons are positioned from the child content origin.
        constexpr float kDistantRoomCenter = 8.0f + 12.0f * 72.0f + 32.0f;
        result.room_center =
            ImVec2(child->DC.CursorStartPos.x + kDistantRoomCenter,
                   child->DC.CursorStartPos.y + kDistantRoomCenter);
        break;
      }
    }
    ImGui::End();
    ImGui::Render();
    return result;
  };

  draw_frame();  // ImGui resolves the child scroll range from prior content.
  const MapFrame initial = draw_frame();
  ASSERT_NE(initial.child, nullptr);
  EXPECT_GT(initial.child->ScrollMax.x, 500.0f);
  EXPECT_GT(initial.child->ScrollMax.y, 500.0f);
  EXPECT_LE(initial.footer.Max.y, initial.host_content.Max.y + 1.0f);
  EXPECT_GT(initial.room_center.x, initial.clip.Max.x);
  EXPECT_GT(initial.room_center.y, initial.clip.Max.y);
  EXPECT_EQ(selection_count, 0);

  ImGui::SetScrollX(initial.child, initial.child->ScrollMax.x);
  ImGui::SetScrollY(initial.child, initial.child->ScrollMax.y);
  const MapFrame scrolled = draw_frame();
  ASSERT_NE(scrolled.child, nullptr);
  EXPECT_GT(scrolled.child->Scroll.x, 500.0f);
  EXPECT_GT(scrolled.child->Scroll.y, 500.0f);
  EXPECT_GT(scrolled.room_center.x, scrolled.clip.Min.x);
  EXPECT_LT(scrolled.room_center.x, scrolled.clip.Max.x);
  EXPECT_GT(scrolled.room_center.y, scrolled.clip.Min.y);
  EXPECT_LT(scrolled.room_center.y, scrolled.clip.Max.y);
  EXPECT_LE(scrolled.footer.Max.y, scrolled.host_content.Max.y + 1.0f);

  io.AddMousePosEvent(scrolled.room_center.x, scrolled.room_center.y);
  draw_frame();  // Establish hover at the scrolled room button.
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  draw_frame();
  EXPECT_EQ(selected_room_id, 0x11);
  EXPECT_EQ(selection_count, 1);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  draw_frame();
}

TEST(DungeonCanvasViewerNavigationTest, CanNavigateRoomsReflectsCallbacks) {
  DungeonCanvasViewer viewer;

  EXPECT_FALSE(viewer.CanNavigateRooms());

  viewer.SetRoomNavigationCallback([](int) {});
  EXPECT_TRUE(viewer.CanNavigateRooms());

  viewer.SetRoomNavigationCallback(nullptr);
  EXPECT_FALSE(viewer.CanNavigateRooms());

  viewer.SetRoomSwapCallback([](int, int) {});
  EXPECT_TRUE(viewer.CanNavigateRooms());
}

TEST(DungeonCanvasViewerLayerTest,
     GameLayerVisibilityFollowsObjectEditsWithoutResettingOverrides) {
  DungeonRoomStore rooms;
  auto& room = rooms[0];
  room.SetLayerMerging(zelda3::LayerMerge01);
  room.SetTileObjects({});
  DungeonCanvasViewer viewer;
  viewer.SetRooms(&rooms);
  auto& manager = viewer.GetRoomLayerManager(0);
  ASSERT_FALSE(manager.GameHidesLowerTilemap());
  manager.SetLayerBlendMode(zelda3::LayerType::BG1_Objects,
                            zelda3::LayerBlendMode::Off);
  manager.SetLayerVisible(zelda3::LayerType::BG2_Objects, false);
  manager.SetShowHiddenLayers(true);

  room.SetTileObjects({zelda3::RoomObject(0xDA, 8, 8, 0, 1)});
  EXPECT_TRUE(viewer.GetRoomLayerManager(0).GameHidesLowerTilemap());
  room.SetTileObjects({});
  EXPECT_FALSE(viewer.GetRoomLayerManager(0).GameHidesLowerTilemap());

  EXPECT_EQ(manager.GetLayerBlendMode(zelda3::LayerType::BG1_Objects),
            zelda3::LayerBlendMode::Off);
  EXPECT_FALSE(manager.IsLayerVisible(zelda3::LayerType::BG2_Objects));
  EXPECT_TRUE(manager.ShowHiddenLayers());
}

TEST(DungeonCanvasViewerLayerTest, GameLayerVisibilityFollowsTagEdits) {
  DungeonRoomStore rooms;
  auto& room = rooms[0];
  room.SetLayerMerging(zelda3::LayerMerge01);
  room.SetTileObjects({zelda3::RoomObject(0x133, 8, 8, 0, 0)});
  room.SetTag2(static_cast<zelda3::TagKey>(0));
  DungeonCanvasViewer viewer;
  viewer.SetRooms(&rooms);
  ASSERT_FALSE(viewer.GetRoomLayerManager(0).GameHidesLowerTilemap());

  room.SetTag2(static_cast<zelda3::TagKey>(0x1B));
  EXPECT_TRUE(viewer.GetRoomLayerManager(0).GameHidesLowerTilemap());
  room.SetTag2(static_cast<zelda3::TagKey>(0));
  EXPECT_FALSE(viewer.GetRoomLayerManager(0).GameHidesLowerTilemap());
}

TEST(DungeonCanvasViewerNavigationTest, NavigateToRoomIgnoresInvalidTargets) {
  DungeonCanvasViewer viewer;
  int nav_calls = 0;
  int swap_calls = 0;
  viewer.SetRoomNavigationCallback([&](int) { ++nav_calls; });
  viewer.SetRoomSwapCallback([&](int, int) { ++swap_calls; });

  viewer.NavigateToRoom(-1);
  viewer.NavigateToRoom(zelda3::kNumberOfRooms);

  EXPECT_EQ(nav_calls, 0);
  EXPECT_EQ(swap_calls, 0);
}

TEST(DungeonCanvasViewerNavigationTest, NavigateToRoomPrefersSwapCallback) {
  DungeonCanvasViewer viewer;

  int nav_target = -1;
  int swap_old = -2;
  int swap_new = -2;
  viewer.SetRoomNavigationCallback([&](int target) { nav_target = target; });
  viewer.SetRoomSwapCallback([&](int old_room, int new_room) {
    swap_old = old_room;
    swap_new = new_room;
  });

  viewer.NavigateToRoom(0xA8);

  EXPECT_EQ(nav_target, -1);
  EXPECT_EQ(swap_old, -1);
  EXPECT_EQ(swap_new, 0xA8);
}

TEST(DungeonCanvasViewerNavigationTest, NavigateToRoomUsesNavigationFallback) {
  DungeonCanvasViewer viewer;

  int nav_target = -1;
  viewer.SetRoomNavigationCallback([&](int target) { nav_target = target; });

  viewer.NavigateToRoom(0xDA);

  EXPECT_EQ(nav_target, 0xDA);
}

TEST(DungeonCanvasViewerNavigationTest, ScrollToTileStoresPendingTarget) {
  DungeonCanvasViewer viewer;

  EXPECT_FALSE(viewer.HasPendingScrollTarget());

  viewer.ScrollToTile(12, 34);

  ASSERT_TRUE(viewer.HasPendingScrollTarget());
  ASSERT_TRUE(viewer.GetPendingScrollTarget().has_value());
  EXPECT_EQ(viewer.GetPendingScrollTarget()->first, 12);
  EXPECT_EQ(viewer.GetPendingScrollTarget()->second, 34);
}

TEST(DungeonCanvasViewerShortcutFocusTest,
     DeleteEligibilityFollowsCanvasFocusAndCurrentDrawFrame) {
  DungeonCanvasViewer viewer;

  DungeonCanvasViewerTestPeer::UpdateRoomCanvasShortcutFocus(
      viewer, /*hovered=*/true, /*pointer_pressed=*/false,
      /*frame_index=*/10);
  EXPECT_FALSE(DungeonCanvasViewerTestPeer::HasRoomCanvasShortcutFocusForFrame(
      viewer, 10));

  DungeonCanvasViewerTestPeer::UpdateRoomCanvasShortcutFocus(
      viewer, /*hovered=*/true, /*pointer_pressed=*/true,
      /*frame_index=*/11);
  EXPECT_TRUE(DungeonCanvasViewerTestPeer::HasRoomCanvasShortcutFocusForFrame(
      viewer, 11));
  EXPECT_FALSE(DungeonCanvasViewerTestPeer::HasRoomCanvasShortcutFocusForFrame(
      viewer, 12));

  DungeonCanvasViewerTestPeer::UpdateRoomCanvasShortcutFocus(
      viewer, /*hovered=*/false, /*pointer_pressed=*/true,
      /*frame_index=*/12);
  EXPECT_FALSE(DungeonCanvasViewerTestPeer::HasRoomCanvasShortcutFocusForFrame(
      viewer, 12));
}

TEST(DungeonCanvasViewerNavigationTest,
     ConnectedPreviewResolvesEachRoomWithoutChangingPrimaryContext) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  for (const int id : {0x89, 0x90}) {
    rooms[id].SetTileObjects({});
    rooms[id].SetLoaded(true);
  }
  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  viewer.SetEntranceRenderContext(0x27 + zelda3::kNumDungeonSpawnPoints, 4);
  bool automatic = true;
  viewer.SetEntranceBlocksetResolver([&](int room_id) -> uint8_t {
    return automatic && room_id == 0x90 ? 1 : 4;
  });
  DungeonCanvasViewerTestPeer::PrepareConnectedRoom(viewer, 0x89);
  DungeonCanvasViewerTestPeer::PrepareConnectedRoom(viewer, 0x90);
  EXPECT_EQ(rooms[0x89].render_entrance_blockset(), 4);
  EXPECT_EQ(rooms[0x90].render_entrance_blockset(), 1);
  EXPECT_EQ(viewer.current_entrance_blockset(), 4);
  EXPECT_EQ(viewer.current_entrance_id(),
            0x27 + zelda3::kNumDungeonSpawnPoints);
  automatic = false;
  DungeonCanvasViewerTestPeer::PrepareConnectedRoom(viewer, 0x90);
  EXPECT_EQ(rooms[0x90].render_entrance_blockset(), 4);
}

TEST(DungeonCanvasViewerNavigationTest,
     PendingScrollAlignsBackgroundOverlayAndInputInSameFrame) {
  ScopedImGuiContext imgui;
  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = 1.0f / 60.0f;
  io.Fonts->AddFontDefault();
  unsigned char* font_pixels = nullptr;
  int font_width = 0;
  int font_height = 0;
  io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(20.0f, 30.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(1300.0f, 1100.0f), ImGuiCond_Always);
  ImGui::Begin("PendingScrollCanvasHost", nullptr,
               ImGuiWindowFlags_NoSavedSettings);

  DungeonCanvasViewer viewer;
  const PendingScrollFrameSnapshot snapshot =
      DungeonCanvasViewerTestPeer::ConsumePendingScrollAndCaptureTransforms(
          viewer, 48, 40, 2.0f, ImVec2(-64.0f, -32.0f), ImVec2(512.0f, 512.0f));

  ImGui::End();
  ImGui::Render();

  EXPECT_FALSE(viewer.HasPendingScrollTarget());
  EXPECT_NEAR(snapshot.applied_scroll.x, -512.0f, 0.01f);
  EXPECT_NEAR(snapshot.applied_scroll.y, -384.0f, 0.01f);
  EXPECT_NEAR(snapshot.background_origin.x,
              snapshot.canvas_origin.x + snapshot.applied_scroll.x, 0.01f);
  EXPECT_NEAR(snapshot.background_origin.y,
              snapshot.canvas_origin.y + snapshot.applied_scroll.y, 0.01f);
  EXPECT_NEAR(snapshot.overlay_origin.x, snapshot.background_origin.x, 0.01f);
  EXPECT_NEAR(snapshot.overlay_origin.y, snapshot.background_origin.y, 0.01f);
  EXPECT_EQ(snapshot.background_origin_room_pixel, std::make_pair(0, 0));
}

namespace {

struct SpritePreviewCanvasCase {
  uint8_t id;
  SDL_Rect expected_art_bounds;
  const char* name;
};

class DungeonCanvasAssetRefreshTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("yaze_canvas_asset_refresh_" + std::to_string(nonce));
    ASSERT_TRUE(std::filesystem::create_directories(root_ / "Sprites/Bosses"));
    project_.filepath = (root_ / "project.yaze").string();
    project_.assets_folder = "Sprites";
    ASSERT_TRUE(project_.hack_manifest
                    .LoadFromString(R"json({
      "manifest_version": 1, "hack_name": "Oracle of Secrets"
    })json")
                    .ok());
    auto& manager = zelda3::CustomObjectManager::Get();
    previous_state_ = manager.SnapshotState();
    previous_context_ = manager.active_runtime_context_id();
    previous_enabled_ = core::FeatureFlags::get().kEnableCustomObjects;
    core::FeatureFlags::get().kEnableCustomObjects = false;
    manager.ActivateRuntimeContext(kContext, {});
  }

  void TearDown() override {
    auto& manager = zelda3::CustomObjectManager::Get();
    manager.RemoveRuntimeContext(kContext);
    manager.RemoveRuntimeContext(kOtherContext);
    if (previous_context_) {
      manager.ActivateRuntimeContext(*previous_context_, previous_state_);
    } else {
      manager.ActivateStandaloneContext();
      manager.RestoreState(previous_state_);
    }
    core::FeatureFlags::get().kEnableCustomObjects = previous_enabled_;
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }

  void WriteSprite(uint8_t value, size_t size = 0x2000) {
    const std::vector<uint8_t> bytes(size, value);
    std::ofstream file(root_ / "Sprites/Bosses/manhandla.bin",
                       std::ios::binary);
    ASSERT_TRUE(file.is_open());
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    ASSERT_TRUE(file.good());
  }

  void ExpectSprite(DungeonCanvasViewer& viewer, uint8_t value) {
    const auto bytes =
        DungeonCanvasViewerTestPeer::ExternalSpriteGraphics(viewer);
    ASSERT_EQ(bytes.size(), 0x2000u);
    EXPECT_EQ(bytes.front(), value);
  }

  static constexpr uint64_t kContext = 0xA55E7001;
  static constexpr uint64_t kOtherContext = 0xA55E7002;
  std::filesystem::path root_;
  project::YazeProject project_;
  zelda3::CustomObjectManager::State previous_state_;
  std::optional<uint64_t> previous_context_;
  bool previous_enabled_ = false;
};

TEST_F(DungeonCanvasAssetRefreshTest,
       SameProjectRebindKeepsCacheUntilExplicitRefresh) {
  DungeonCanvasViewer viewer;
  viewer.SetProject(&project_);
  WriteSprite(0x11);
  ExpectSprite(viewer, 0x11);

  DungeonRoomStore rooms;
  rooms[0].GetTileObjects().emplace_back(0x01, 10, 10, 0, 0);
  viewer.SetRooms(&rooms);
  viewer.object_interaction().SetCurrentRoom(&rooms, 0);
  viewer.object_interaction().SetSelectedObjects({0});
  WriteSprite(0x22);
  viewer.SetProject(&project_);
  ExpectSprite(viewer, 0x11);

  viewer.InvalidateExternalSpriteResources();
  ExpectSprite(viewer, 0x22);
  EXPECT_EQ(viewer.object_interaction().GetSelectedObjectIndices(),
            std::vector<size_t>{0});
  EXPECT_EQ(rooms[0].GetTileObjects().front().x_, 10);
}

TEST_F(DungeonCanvasAssetRefreshTest,
       ExplicitRefreshRetriesMissingAndBadFiles) {
  DungeonCanvasViewer viewer;
  viewer.SetProject(&project_);
  EXPECT_TRUE(
      DungeonCanvasViewerTestPeer::ExternalSpriteGraphics(viewer).empty());
  WriteSprite(0x33);
  EXPECT_TRUE(
      DungeonCanvasViewerTestPeer::ExternalSpriteGraphics(viewer).empty());
  viewer.InvalidateExternalSpriteResources();
  ExpectSprite(viewer, 0x33);
  WriteSprite(0x44, 5);
  viewer.InvalidateExternalSpriteResources();
  EXPECT_TRUE(
      DungeonCanvasViewerTestPeer::ExternalSpriteGraphics(viewer).empty());
  WriteSprite(0x55);
  EXPECT_TRUE(
      DungeonCanvasViewerTestPeer::ExternalSpriteGraphics(viewer).empty());
  viewer.InvalidateExternalSpriteResources();
  ExpectSprite(viewer, 0x55);
}

TEST_F(DungeonCanvasAssetRefreshTest,
       FullRomRefreshAlsoInvalidatesExternalArt) {
  Rom rom;
  DungeonRoomStore rooms(&rom);
  DungeonCanvasViewer viewer(&rom);
  viewer.SetProject(&project_);
  WriteSprite(0x11);
  ExpectSprite(viewer, 0x11);
  WriteSprite(0x22);
  viewer.RefreshRomBackedState(&rom, nullptr, &rooms, 0);
  ExpectSprite(viewer, 0x22);
}

TEST_F(DungeonCanvasAssetRefreshTest,
       GenerationRefreshesAllSessionViewersOnceWithCustomObjectsDisabled) {
  DungeonEditorV2 editor(nullptr);
  const auto viewers =
      DungeonCanvasViewerTestPeer::SeedAssetRefreshViewers(editor, &project_);
  DungeonCanvasViewerTestPeer::SynchronizeProjectAssets(editor);
  WriteSprite(0x11);
  for (auto* viewer : viewers) {
    ExpectSprite(*viewer, 0x11);
  }

  auto& manager = zelda3::CustomObjectManager::Get();
  manager.ActivateRuntimeContext(kOtherContext, {});
  DungeonEditorV2 other_editor(nullptr);
  const auto other_viewers =
      DungeonCanvasViewerTestPeer::SeedAssetRefreshViewers(other_editor,
                                                           &project_);
  DungeonCanvasViewerTestPeer::SynchronizeProjectAssets(other_editor);
  for (auto* viewer : other_viewers) {
    ExpectSprite(*viewer, 0x11);
  }

  manager.ActivateRuntimeContext(kContext, {});
  WriteSprite(0x22);
  manager.ReloadAll();
  DungeonCanvasViewerTestPeer::SynchronizeProjectAssets(editor);
  for (auto* viewer : viewers) {
    ExpectSprite(*viewer, 0x22);
  }
  WriteSprite(0x33);
  DungeonCanvasViewerTestPeer::SynchronizeProjectAssets(editor);
  for (auto* viewer : viewers) {
    ExpectSprite(*viewer, 0x22);
  }

  manager.ActivateRuntimeContext(kOtherContext, {});
  DungeonCanvasViewerTestPeer::SynchronizeProjectAssets(other_editor);
  for (auto* viewer : other_viewers) {
    ExpectSprite(*viewer, 0x11);
  }
}

// Opt-in CPU benchmark: no frame-time threshold and no display/GPU requirement.
TEST_F(DungeonCanvasAssetRefreshTest, DISABLED_ProfileSpritePreviewFrames) {
  const char* assets = std::getenv("YAZE_TEST_ORACLE_SPRITE_ASSETS");
  ASSERT_NE(assets, nullptr);
  const auto source = std::filesystem::path(assets) / "Bosses/manhandla.bin";
  ASSERT_TRUE(std::filesystem::copy_file(
      source, root_ / "Sprites/Bosses/manhandla.bin"));
  ScopedImGuiContext imgui;
  auto& io = ImGui::GetIO();
  io.Fonts->AddFontDefault();
  unsigned char* font_pixels = nullptr;
  int font_width = 0, font_height = 0;
  io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::GameData game_data;
  game_data.graphics_buffer.assign(4096, 1);
  game_data.palette_groups.global_sprites.AddPalette(gfx::SnesPalette(
      std::vector<gfx::SnesColor>(60, gfx::SnesColor(12, 180, 90))));
  zelda3::Room room(0, &rom, &game_data);
  room.mutable_blocks().fill(0);
  room.CopyRoomGraphicsToBuffer();

  struct Case {
    const char* name;
    uint8_t id;
    int count;
    bool visible;
  };
  for (const auto& scenario :
       {Case{"ordinary_1", 0x00, 1, true}, Case{"manhandla_1", 0x88, 1, true},
        Case{"manhandla_8", 0x88, 8, true}, Case{"hidden_8", 0x88, 8, false}}) {
    DungeonCanvasViewer viewer;
    viewer.SetProject(&project_);
    viewer.SetGameData(&game_data);
    viewer.SetSpritesVisible(scenario.visible);
    room.GetSprites().clear();
    for (int i = 0; i < scenario.count; ++i) {
      room.GetSprites().emplace_back(scenario.id, 2 + i * 3, 8, 0, 0);
    }
    auto frame = [&]() {
      ImGui::NewFrame();
      ImGui::SetNextWindowSize(ImVec2(900, 700));
      ImGui::Begin("SpriteProfile", nullptr, ImGuiWindowFlags_NoSavedSettings);
      gui::CanvasRuntime runtime;
      runtime.draw_list = ImGui::GetWindowDrawList();
      runtime.canvas_p0 = ImVec2(10, 10);
      runtime.canvas_sz = ImVec2(800, 640);
      runtime.scale = 1.0f;
      const auto start = std::chrono::steady_clock::now();
      DungeonCanvasViewerTestPeer::RenderSprites(viewer, runtime, room);
      const double elapsed = std::chrono::duration<double, std::micro>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();
      ImGui::End();
      ImGui::Render();
      return elapsed;
    };
    const double cold = frame();
    std::array<double, 7> batches{};
    for (auto& batch : batches) {
      for (int i = 0; i < 200; ++i) {
        batch += frame();
      }
      batch /= 200;
    }
    std::sort(batches.begin(), batches.end());
    std::cout << "SPRITE_PROFILE " << scenario.name << " cold_us=" << cold
              << " warm_median_us=" << batches[3] << '\n';
    RecordProperty(std::string(scenario.name) + "_warm_us",
                   std::to_string(batches[3]));
  }
}

class DungeonCanvasSpritePreviewCacheTest
    : public DungeonCanvasAssetRefreshTest {
 protected:
  void SetUp() override {
    DungeonCanvasAssetRefreshTest::SetUp();
    imgui_ = std::make_unique<ScopedImGuiContext>();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* font_pixels = nullptr;
    int font_width = 0, font_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    game_data_.graphics_buffer.assign(4096, 1);
    SetSpriteColor(gfx::SnesColor(11, 197, 83));
    viewer_.SetGameData(&game_data_);
    viewer_.SetProject(&project_);
  }

  void SetSpriteColor(const gfx::SnesColor& color) {
    auto& palettes = game_data_.palette_groups.global_sprites;
    if (palettes.empty()) {
      palettes.AddPalette(
          gfx::SnesPalette(std::vector<gfx::SnesColor>(60, color)));
    } else {
      palettes.palette_ref(0) =
          gfx::SnesPalette(std::vector<gfx::SnesColor>(60, color));
    }
  }

  zelda3::Room MakeRoom(int room_id, uint8_t sprite_id = 0x00) {
    zelda3::Room room(room_id, &rom_, &game_data_);
    room.mutable_blocks().fill(0);
    room.CopyRoomGraphicsToBuffer();
    room.GetSprites().emplace_back(sprite_id, 8, 8, 0, 0);
    return room;
  }

  std::vector<ImDrawVert> Frame(const zelda3::Room& room, float scale = 1.0f,
                                ImVec2 scroll = ImVec2(0, 0),
                                bool hovered = false) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(950, 720), ImGuiCond_Always);
    ImGui::Begin("SpriteCacheHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    gui::CanvasRuntime runtime;
    runtime.draw_list = ImGui::GetWindowDrawList();
    runtime.canvas_p0 = ImVec2(40, 50);
    runtime.canvas_sz = ImVec2(850, 650);
    runtime.scrolling = scroll;
    runtime.scale = scale;
    runtime.hovered = hovered;
    const int first_vertex = runtime.draw_list->VtxBuffer.Size;
    DungeonCanvasViewerTestPeer::RenderSprites(viewer_, runtime, room);
    std::vector<ImDrawVert> vertices(
        runtime.draw_list->VtxBuffer.begin() + first_vertex,
        runtime.draw_list->VtxBuffer.end());
    ImGui::End();
    ImGui::Render();
    return vertices;
  }

  void ExpectUncachedArt(const zelda3::Room& room,
                         std::span<const uint8_t> resource = {}) {
    const auto& sprite = room.GetSprites().front();
    const auto* cached =
        DungeonCanvasViewerTestPeer::CachedSpritePreview(viewer_, room, sprite);
    ASSERT_NE(cached, nullptr);
    auto expected = sprite;
    const auto* layout = zelda3::SpriteOamRegistry::GetPreviewOverride(
        sprite.id(), project_.hack_manifest.hack_name());
    expected.RenderPreviewGraphics(room.get_gfx_buffer(), layout, resource);
    EXPECT_EQ(cached->pixels, *expected.preview_graphics());
    const auto bounds = expected.preview_bounds();
    EXPECT_EQ(cached->bounds.x, bounds.x);
    EXPECT_EQ(cached->bounds.y, bounds.y);
    EXPECT_EQ(cached->bounds.w, bounds.w);
    EXPECT_EQ(cached->bounds.h, bounds.h);
  }

  static std::optional<ImRect> ArtBounds(
      const std::vector<ImDrawVert>& vertices, ImU32 color) {
    std::optional<ImRect> bounds;
    for (const auto& vertex : vertices) {
      if (vertex.col != color) {
        continue;
      }
      if (bounds) {
        bounds->Add(vertex.pos);
      } else {
        bounds = ImRect(vertex.pos, vertex.pos);
      }
    }
    return bounds;
  }

  std::unique_ptr<ScopedImGuiContext> imgui_;
  Rom rom_;
  zelda3::GameData game_data_;
  DungeonCanvasViewer viewer_;
};

TEST_F(DungeonCanvasSpritePreviewCacheTest,
       RepeatedFramesAndDuplicateSpritesReuseDecodedPixels) {
  auto room = MakeRoom(0, 0x7E);
  room.GetSprites().emplace_back(0x7E, 15, 12, 0, 1);
  Frame(room);
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 1u);
  ExpectUncachedArt(room);
  auto* cached = DungeonCanvasViewerTestPeer::CachedSpritePreview(
      viewer_, room, room.GetSprites().front());
  ASSERT_NE(cached, nullptr);
  ASSERT_FALSE(cached->pixels.empty());
  const auto* storage = cached->pixels.data();
  // A test-only marker proves the next frame consumes this cache entry, even
  // if an allocator would reuse the same address after an accidental rebuild.
  cached->pixels.front() = 0x37;
  Frame(room);
  cached = DungeonCanvasViewerTestPeer::CachedSpritePreview(
      viewer_, room, room.GetSprites().front());
  ASSERT_NE(cached, nullptr);
  EXPECT_EQ(cached->pixels.data(), storage);
  EXPECT_EQ(cached->pixels.front(), 0x37);
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 1u);
  EXPECT_EQ(room.GetSprites().front().x(), 8);
  EXPECT_EQ(room.GetSprites().back().layer(), 1);
}

TEST_F(DungeonCanvasSpritePreviewCacheTest,
       GraphicsAssemblyAndAnimatedReloadCreateFreshPreviews) {
  auto room = MakeRoom(0);
  Frame(room);
  const auto old_pixels = DungeonCanvasViewerTestPeer::CachedSpritePreview(
                              viewer_, room, room.GetSprites().front())
                              ->pixels;
  const auto old_revision = room.graphics_revision();
  game_data_.graphics_buffer.assign(223 * 4096, 2);
  room.CopyRoomGraphicsToBuffer();
  ASSERT_NE(room.graphics_revision(), old_revision);
  Frame(room);
  ExpectUncachedArt(room);
  EXPECT_NE(DungeonCanvasViewerTestPeer::CachedSpritePreview(
                viewer_, room, room.GetSprites().front())
                ->pixels,
            old_pixels);
  const auto assembled_revision = room.graphics_revision();
  room.LoadAnimatedGraphics();
  ASSERT_NE(room.graphics_revision(), assembled_revision);
  Frame(room);
  ExpectUncachedArt(room);
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 3u);
}

TEST_F(DungeonCanvasSpritePreviewCacheTest,
       MultipleRoomsKeepIndependentWarmPreviews) {
  auto first = MakeRoom(0);
  game_data_.graphics_buffer.assign(4096, 2);
  auto second = MakeRoom(1);
  ASSERT_NE(first.graphics_revision(), second.graphics_revision());
  Frame(first);
  Frame(second);
  ExpectUncachedArt(first);
  ExpectUncachedArt(second);
  auto* first_cached = DungeonCanvasViewerTestPeer::CachedSpritePreview(
      viewer_, first, first.GetSprites().front());
  auto* second_cached = DungeonCanvasViewerTestPeer::CachedSpritePreview(
      viewer_, second, second.GetSprites().front());
  ASSERT_NE(first_cached, nullptr);
  ASSERT_NE(second_cached, nullptr);
  EXPECT_NE(first_cached->pixels, second_cached->pixels);
  first_cached->pixels.front() = 0x37;
  second_cached->pixels.front() = 0x38;
  Frame(first);
  Frame(second);
  EXPECT_EQ(first_cached->pixels.front(), 0x37);
  EXPECT_EQ(second_cached->pixels.front(), 0x38);
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 2u);
}

TEST_F(DungeonCanvasSpritePreviewCacheTest,
       UnassembledRoomsDoNotShareRevisionZeroPreviews) {
  zelda3::Room first(0, &rom_, &game_data_);
  zelda3::Room second(1, &rom_, &game_data_);
  // These non-const fixture rooms deliberately bypass graphics assembly.
  // Initialize their backing arrays without publishing a graphics revision.
  auto& first_pixels =
      const_cast<std::array<uint8_t, 0x10000>&>(first.get_gfx_buffer());
  auto& second_pixels =
      const_cast<std::array<uint8_t, 0x10000>&>(second.get_gfx_buffer());
  first_pixels.fill(1);
  second_pixels.fill(0);
  first.GetSprites().emplace_back(0x00, 8, 8, 0, 0);
  second.GetSprites().emplace_back(0x00, 8, 8, 0, 0);
  ASSERT_EQ(first.graphics_revision(), 0u);
  ASSERT_EQ(second.graphics_revision(), 0u);
  EXPECT_TRUE(ArtBounds(Frame(first), IM_COL32(11, 197, 83, 255)).has_value());
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 0u);
  EXPECT_FALSE(
      ArtBounds(Frame(second), IM_COL32(11, 197, 83, 255)).has_value());
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 0u);
  EXPECT_TRUE(ArtBounds(Frame(first), IM_COL32(11, 197, 83, 255)).has_value());
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 0u);
}

TEST_F(DungeonCanvasSpritePreviewCacheTest,
       PaletteMovementLayerAndCanvasTransformStayLive) {
  auto room = MakeRoom(0);
  const auto before = ArtBounds(Frame(room), IM_COL32(11, 197, 83, 255));
  ASSERT_TRUE(before.has_value());
  auto* cached = DungeonCanvasViewerTestPeer::CachedSpritePreview(
      viewer_, room, room.GetSprites().front());
  ASSERT_NE(cached, nullptr);
  const auto* storage = cached->pixels.data();
  SetSpriteColor(gfx::SnesColor(211, 37, 109));
  auto& sprite = room.GetSprites().front();
  sprite.set_x(10);
  sprite.set_y(11);
  sprite.set_layer(1);
  const auto vertices = Frame(room, 1.5f, ImVec2(7, 9));
  const auto after = ArtBounds(vertices, IM_COL32(211, 37, 109, 255));
  ASSERT_TRUE(after.has_value());
  EXPECT_FALSE(ArtBounds(vertices, IM_COL32(11, 197, 83, 255)).has_value());
  EXPECT_FLOAT_EQ(after->Min.x, 40 + 7 + 10 * 16 * 1.5f);
  EXPECT_FLOAT_EQ(after->Min.y, 50 + 9 + 11 * 16 * 1.5f);
  EXPECT_FLOAT_EQ(after->GetWidth(), before->GetWidth() * 1.5f);
  EXPECT_FLOAT_EQ(after->GetHeight(), before->GetHeight() * 1.5f);
  EXPECT_EQ(
      DungeonCanvasViewerTestPeer::CachedSpritePreview(viewer_, room, sprite)
          ->pixels.data(),
      storage);
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 1u);
}

TEST_F(DungeonCanvasSpritePreviewCacheTest,
       ExplicitRefreshAndSamePointerContextChangesReplaceExternalArt) {
  auto room = MakeRoom(0, 0x88);
  WriteSprite(0x11);
  Frame(room);
  ExpectUncachedArt(room, std::vector<uint8_t>(0x2000, 0x11));
  WriteSprite(0x22);
  Frame(room);
  ExpectUncachedArt(room, std::vector<uint8_t>(0x2000, 0x11));
  viewer_.InvalidateExternalSpriteResources();
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 0u);
  Frame(room);
  ExpectUncachedArt(room, std::vector<uint8_t>(0x2000, 0x22));

  WriteSprite(0x33);
  Frame(room);
  ExpectUncachedArt(room, std::vector<uint8_t>(0x2000, 0x22));
  project_.filepath = (root_ / "renamed.yaze").string();
  Frame(room);
  ExpectUncachedArt(room, std::vector<uint8_t>(0x2000, 0x33));
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 1u);

  ASSERT_TRUE(std::filesystem::create_directories(root_ / "Other/Bosses"));
  WriteSprite(0x44);
  ASSERT_TRUE(std::filesystem::copy_file(root_ / "Sprites/Bosses/manhandla.bin",
                                         root_ / "Other/Bosses/manhandla.bin"));
  project_.assets_folder = "Other";
  Frame(room);
  ExpectUncachedArt(room, std::vector<uint8_t>(0x2000, 0x44));
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 1u);

  ASSERT_TRUE(project_.hack_manifest
                  .LoadFromString(R"json({
      "manifest_version": 1, "hack_name": "Vanilla"
    })json")
                  .ok());
  Frame(room);
  ExpectUncachedArt(room);
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 1u);
}

TEST_F(DungeonCanvasSpritePreviewCacheTest,
       MissingAndMalformedExternalArtRemainEmptyUntilRefresh) {
  auto room = MakeRoom(0, 0x88);
  Frame(room);
  ExpectUncachedArt(room);
  WriteSprite(0x11);
  Frame(room);
  ExpectUncachedArt(room);
  viewer_.InvalidateExternalSpriteResources();
  Frame(room);
  ExpectUncachedArt(room, std::vector<uint8_t>(0x2000, 0x11));
  WriteSprite(0x22, 5);
  viewer_.InvalidateExternalSpriteResources();
  Frame(room);
  ExpectUncachedArt(room);
  WriteSprite(0x33);
  Frame(room);
  ExpectUncachedArt(room);
  viewer_.InvalidateExternalSpriteResources();
  Frame(room);
  ExpectUncachedArt(room, std::vector<uint8_t>(0x2000, 0x33));
}

TEST_F(DungeonCanvasSpritePreviewCacheTest,
       ChangingSpriteIdentityUsesSeparateEntriesAndCacheIsBounded) {
  auto room = MakeRoom(0);
  auto& sprite = room.GetSprites().front();
  Frame(room);
  sprite.set_id(0x7E);
  Frame(room);
  ExpectUncachedArt(room);
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 2u);
  viewer_.InvalidateExternalSpriteResources();
  for (int subtype = 0; subtype < 129; ++subtype) {
    sprite.set_subtype(subtype);
    Frame(room);
  }
  EXPECT_EQ(DungeonCanvasViewerTestPeer::SpritePreviewCacheSize(viewer_), 128u);
  ExpectUncachedArt(room);
  sprite.set_subtype(0);
  EXPECT_EQ(
      DungeonCanvasViewerTestPeer::CachedSpritePreview(viewer_, room, sprite),
      nullptr);
  sprite.set_subtype(1);
  EXPECT_NE(
      DungeonCanvasViewerTestPeer::CachedSpritePreview(viewer_, room, sprite),
      nullptr);
}

TEST_F(DungeonCanvasSpritePreviewCacheTest,
       EntityLabelsFollowHoverAndSelection) {
  DungeonRoomStore rooms;
  rooms[0] = MakeRoom(0);
  auto& room = rooms[0];
  viewer_.object_interaction().SetCurrentRoom(&rooms, 0);
  ImGui::GetIO().AddMousePosEvent(800, 600);
  Frame(room);
  const auto quiet = Frame(room);
  ImGui::GetIO().AddMousePosEvent(171, 181);
  Frame(room, 1, ImVec2(0, 0), true);
  const auto hovered = Frame(room, 1, ImVec2(0, 0), true);
  EXPECT_GT(hovered.size(), quiet.size());
  ImGui::GetIO().AddMousePosEvent(800, 600);
  viewer_.object_interaction().SelectEntity(EntityType::Sprite, 0);
  const auto selected = Frame(room);
  EXPECT_GT(selected.size(), quiet.size());
  viewer_.object_interaction().entity_coordinator().ClearAllEntitySelections();
  EXPECT_EQ(Frame(room).size(), quiet.size());
}

class DungeonCanvasSpritePreviewBoundsTest
    : public ::testing::TestWithParam<SpritePreviewCanvasCase> {};

TEST_P(DungeonCanvasSpritePreviewBoundsTest,
       PreservesArtBoundsAtRoomAnchorWithScaleAndPan) {
  ScopedImGuiContext imgui;
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DeltaTime = 1.0f / 60.0f;
  io.Fonts->AddFontDefault();
  unsigned char* font_pixels = nullptr;
  int font_width = 0;
  int font_height = 0;
  io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::GameData game_data;
  game_data.graphics_buffer.assign(4096, 1);
  // All art has a distinctive color, separate from selection outlines and
  // labels. The global rows cover firebars; auxiliary row 14 covers Helmasaur.
  constexpr ImU32 kArtColor = IM_COL32(11, 197, 83, 255);
  const gfx::SnesColor art_color(11, 197, 83);
  game_data.palette_groups.global_sprites.AddPalette(
      gfx::SnesPalette(std::vector<gfx::SnesColor>(60, art_color)));
  game_data.palette_groups.sprites_aux3.AddPalette(
      gfx::SnesPalette(std::vector<gfx::SnesColor>(7, art_color)));
  zelda3::Room room(0, &rom, &game_data);
  room.mutable_blocks().fill(0);
  room.CopyRoomGraphicsToBuffer();
  room.GetSprites().emplace_back(GetParam().id, 12, 10, 0, 0);
  DungeonCanvasViewer viewer;
  viewer.SetGameData(&game_data);

  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(10, 20), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(950, 720), ImGuiCond_Always);
  ImGui::Begin("SpritePreviewBoundsHost", nullptr,
               ImGuiWindowFlags_NoSavedSettings);
  gui::CanvasRuntime runtime;
  runtime.draw_list = ImGui::GetWindowDrawList();
  runtime.canvas_p0 = ImVec2(81, 67);
  runtime.canvas_sz = ImVec2(800, 640);
  runtime.scrolling = ImVec2(-31, 23);
  runtime.scale = 1.5f;

  const int first_vertex = runtime.draw_list->VtxBuffer.Size;
  DungeonCanvasViewerTestPeer::RenderSprites(viewer, runtime, room);
  ImRect art_bounds;
  bool found_art = false;
  for (int i = first_vertex; i < runtime.draw_list->VtxBuffer.Size; ++i) {
    const auto& vertex = runtime.draw_list->VtxBuffer[i];
    if (vertex.col == kArtColor) {
      if (found_art) {
        art_bounds.Add(vertex.pos);
      } else {
        art_bounds = ImRect(vertex.pos, vertex.pos);
      }
      found_art = true;
    }
  }
  ImGui::End();
  ImGui::Render();

  ASSERT_TRUE(found_art);
  const auto expected = GetParam().expected_art_bounds;
  const float anchor_x =
      runtime.canvas_p0.x + runtime.scrolling.x + 12 * 16 * runtime.scale;
  const float anchor_y =
      runtime.canvas_p0.y + runtime.scrolling.y + 10 * 16 * runtime.scale;
  EXPECT_FLOAT_EQ(art_bounds.Min.x, anchor_x + expected.x * runtime.scale);
  EXPECT_FLOAT_EQ(art_bounds.Min.y, anchor_y + expected.y * runtime.scale);
  EXPECT_FLOAT_EQ(art_bounds.Max.x,
                  anchor_x + (expected.x + expected.w) * runtime.scale);
  EXPECT_FLOAT_EQ(art_bounds.Max.y,
                  anchor_y + (expected.y + expected.h) * runtime.scale);
  EXPECT_EQ(room.GetSprites().front().x(), 12);
  EXPECT_EQ(room.GetSprites().front().y(), 10);
  EXPECT_FALSE(rom.dirty());
}

INSTANTIATE_TEST_SUITE_P(
    ExtendedArt, DungeonCanvasSpritePreviewBoundsTest,
    ::testing::Values(
        SpritePreviewCanvasCase{0x00, {0, 0, 16, 16}, "StandardSprite"},
        SpritePreviewCanvasCase{0x7E, {0, -56, 16, 58}, "ClockwiseFirebar"},
        SpritePreviewCanvasCase{0x7F,
                                {0, -56, 16, 58},
                                "CounterclockwiseFirebar"},
        SpritePreviewCanvasCase{0x80, {-56, 0, 72, 16}, "HorizontalFirebar"},
        SpritePreviewCanvasCase{0xC7, {0, -30, 16, 46}, "Pokey"},
        SpritePreviewCanvasCase{0x92, {0, 32, 48, 48}, "HelmasaurKing"}),
    [](const ::testing::TestParamInfo<SpritePreviewCanvasCase>& info) {
      return info.param.name;
    });

}  // namespace

TEST(DungeonCanvasViewerContextMenuTest,
     EmptySelectionKeepsPasteAndInsertAtRoot) {
  ScopedImGuiContext imgui;
  DungeonCanvasViewer viewer;
  viewer.SetShowObjectPanelCallback([]() {});
  viewer.SetShowSpritePanelCallback([]() {});
  viewer.SetShowItemPanelCallback([]() {});

  const auto labels =
      DungeonCanvasViewerTestPeer::InteractionContextMenuLabels(viewer, 0);

  ASSERT_EQ(labels.size(), 4u);
  EXPECT_EQ(labels[0], "Paste");
  EXPECT_EQ(labels[1], "Delete");
  EXPECT_EQ(labels[2], "Use Object as Brush");
  EXPECT_EQ(labels[3], "Insert");
}

TEST(DungeonCanvasViewerContextMenuTest,
     ObjectBrushActionKeepsItsSlotAndUsesCapturedObject) {
  ScopedImGuiContext imgui;
  DungeonCanvasViewer viewer;

  auto items = DungeonCanvasViewerTestPeer::SelectionContextMenuItems(
      viewer, 0, std::nullopt);
  auto brush = std::find_if(items.begin(), items.end(), [](const auto& item) {
    return item.label == "Use Object as Brush";
  });
  ASSERT_NE(brush, items.end());
  EXPECT_FALSE(brush->enabled_condition());

  const zelda3::RoomObject captured_object{0x34, 12, 9, 0x05, 1};
  items = DungeonCanvasViewerTestPeer::SelectionContextMenuItems(
      viewer, 0, captured_object);
  brush = std::find_if(items.begin(), items.end(), [](const auto& item) {
    return item.label == "Use Object as Brush";
  });
  ASSERT_NE(brush, items.end());
  ASSERT_TRUE(brush->enabled_condition());
  ASSERT_TRUE(brush->callback);
  brush->callback();

  EXPECT_TRUE(viewer.object_interaction().IsObjectLoaded());
  const auto& preview = viewer.object_interaction()
                            .entity_coordinator()
                            .tile_handler()
                            .GetPreviewObject();
  EXPECT_EQ(preview.id_, captured_object.id_);
  EXPECT_EQ(preview.size_, captured_object.size_);
  EXPECT_EQ(preview.layer_, captured_object.layer_);
}

TEST(DungeonCanvasViewerContextMenuTest,
     SelectedObjectKeepsCommonActionsDirectAndGroupsArrangement) {
  ScopedImGuiContext imgui;
  DungeonRoomStore rooms;
  rooms[0].GetTileObjects().push_back(
      zelda3::RoomObject{0x01, 10, 10, 0x00, 0});

  DungeonCanvasViewer viewer;
  viewer.SetRooms(&rooms);
  viewer.object_interaction().SetCurrentRoom(&rooms, 0);
  viewer.object_interaction().SetSelectedObjects({0});

  const auto labels =
      DungeonCanvasViewerTestPeer::SelectionContextMenuLabels(viewer, 0);

  const auto root =
      DungeonCanvasViewerTestPeer::InteractionContextMenuLabels(viewer, 0);
  ASSERT_GE(root.size(), 7u);
  EXPECT_EQ(root[0], "Cut");
  EXPECT_EQ(root[1], "Copy");
  EXPECT_EQ(root[2], "Paste");
  EXPECT_EQ(root[3], "Duplicate");
  EXPECT_EQ(root[4], "Delete");
  EXPECT_EQ(root[5], "Arrange");
  EXPECT_EQ(root[6], "Placement");
  EXPECT_EQ(FindLabelIndex(root, "Bring to Front"), root.size());
  EXPECT_EQ(FindLabelIndex(root, "Object Stream: Primary"), root.size());

  const size_t bring_front = FindLabelIndex(labels, "Bring to Front");
  const size_t send_back = FindLabelIndex(labels, "Send to Back");
  const size_t z_order = FindLabelIndex(labels, "Arrange");
  const size_t primary = FindLabelIndex(labels, "Object Stream: Primary");
  const size_t bg1_overlay =
      FindLabelIndex(labels, "Object Stream: BG1 Overlay");

  ASSERT_LT(bring_front, labels.size());
  ASSERT_LT(send_back, labels.size());
  ASSERT_LT(z_order, labels.size());
  ASSERT_LT(primary, labels.size());
  ASSERT_LT(bg1_overlay, labels.size());
  EXPECT_LT(z_order, bring_front);
  EXPECT_LT(z_order, send_back);
  EXPECT_LT(z_order, primary);
  EXPECT_LT(primary, bg1_overlay);
  EXPECT_EQ(FindLabelIndex(labels, "Increase Z"), labels.size());
  EXPECT_EQ(FindLabelIndex(labels, "Send to Layer 1"), labels.size());
}

TEST(DungeonCanvasViewerContextMenuTest,
     SpecialObjectDoesNotOfferThirdStreamSelector) {
  ScopedImGuiContext imgui;
  DungeonRoomStore rooms;
  zelda3::RoomObject torch{0x150, 10, 10, 0x00, 0};
  torch.set_options(zelda3::ObjectOption::Torch);
  rooms[0].GetTileObjects().push_back(torch);

  DungeonCanvasViewer viewer;
  viewer.SetRooms(&rooms);
  viewer.object_interaction().SetCurrentRoom(&rooms, 0);
  viewer.object_interaction().SetSelectedObjects({0});

  const auto labels =
      DungeonCanvasViewerTestPeer::SelectionContextMenuLabels(viewer, 0);

  EXPECT_LT(FindLabelIndex(labels, "Upper Layer (BG1)"), labels.size());
  EXPECT_LT(FindLabelIndex(labels, "Lower Layer (BG2)"), labels.size());
  EXPECT_EQ(FindLabelIndex(labels, "Object Stream: BG1 Overlay"),
            labels.size());
}

TEST(DungeonCanvasViewerContextMenuTest,
     MixedObjectSelectionDoesNotOfferThirdStreamSelector) {
  ScopedImGuiContext imgui;
  DungeonRoomStore rooms;
  rooms[0].GetTileObjects().push_back(
      zelda3::RoomObject{0x01, 10, 10, 0x00, 0});
  zelda3::RoomObject block{0x150, 12, 12, 0x00, 0};
  block.set_options(zelda3::ObjectOption::Block);
  rooms[0].GetTileObjects().push_back(block);

  DungeonCanvasViewer viewer;
  viewer.SetRooms(&rooms);
  viewer.object_interaction().SetCurrentRoom(&rooms, 0);
  viewer.object_interaction().SetSelectedObjects({0, 1});

  const auto labels =
      DungeonCanvasViewerTestPeer::SelectionContextMenuLabels(viewer, 0);

  EXPECT_LT(FindLabelIndex(labels, "Placement 0: Primary / Upper Layer (BG1)"),
            labels.size());
  EXPECT_LT(
      FindLabelIndex(labels, "Placement 1: BG2 Overlay / Lower Layer (BG2)"),
      labels.size());
  EXPECT_EQ(FindLabelIndex(labels, "Object Stream: BG1 Overlay"),
            labels.size());
}

TEST(DungeonCanvasViewerContextMenuTest,
     SingleAllowlistedObjectOffersObjectTileEditorAction) {
  ScopedImGuiContext imgui;
  DungeonRoomStore rooms;
  rooms[0].GetTileObjects().push_back(
      zelda3::RoomObject{0x11F, 10, 10, 0x00, 0});

  int opened_room = -1;
  int opened_object = -1;
  DungeonCanvasViewer viewer;
  viewer.SetRooms(&rooms);
  viewer.object_interaction().SetCurrentRoom(&rooms, 0);
  viewer.object_interaction().SetSelectedObjects({0});
  viewer.SetEditObjectTilesCallback(
      [&](int room_id, const zelda3::RoomObject& object) {
        opened_room = room_id;
        opened_object = object.id_;
      });

  const auto labels =
      DungeonCanvasViewerTestPeer::SelectionContextMenuLabels(viewer, 0);
  EXPECT_LT(FindLabelIndex(labels, "Edit Object Tiles..."), labels.size());
  EXPECT_TRUE(DungeonCanvasViewerTestPeer::InvokeSelectionContextMenuItem(
      viewer, 0, "Edit Object Tiles..."));
  EXPECT_EQ(opened_room, 0);
  EXPECT_EQ(opened_object, 0x11F);
}

TEST(DungeonCanvasViewerContextMenuTest,
     ObjectTileEditorActionRequiresOneAllowlistedSelection) {
  ScopedImGuiContext imgui;
  DungeonRoomStore rooms;
  rooms[0].GetTileObjects().push_back(
      zelda3::RoomObject{0x11F, 10, 10, 0x00, 0});
  rooms[0].GetTileObjects().push_back(
      zelda3::RoomObject{0x01, 12, 12, 0x00, 0});

  rooms[0].GetTileObjects().push_back(
      zelda3::RoomObject{0x05, 14, 14, 0x00, 0});

  DungeonCanvasViewer viewer;
  viewer.SetRooms(&rooms);
  viewer.object_interaction().SetCurrentRoom(&rooms, 0);
  viewer.SetEditObjectTilesCallback([](int, const zelda3::RoomObject&) {});

  viewer.object_interaction().SetSelectedObjects({1});
  auto labels =
      DungeonCanvasViewerTestPeer::SelectionContextMenuLabels(viewer, 0);
  EXPECT_LT(FindLabelIndex(labels, "Edit Object Tiles..."), labels.size());

  viewer.object_interaction().SetSelectedObjects({2});
  labels = DungeonCanvasViewerTestPeer::SelectionContextMenuLabels(viewer, 0);
  EXPECT_EQ(FindLabelIndex(labels, "Edit Object Tiles..."), labels.size());

  viewer.object_interaction().SetSelectedObjects({0, 1});
  labels = DungeonCanvasViewerTestPeer::SelectionContextMenuLabels(viewer, 0);
  EXPECT_EQ(FindLabelIndex(labels, "Edit Object Tiles..."), labels.size());
}

TEST(DungeonCanvasViewerPingTest, TriggerCanvasPingRectClampsToCanvasBounds) {
  DungeonCanvasViewer viewer;

  viewer.TriggerCanvasPingRect(510, -8, 64, 0);

  ASSERT_EQ(DungeonCanvasViewerTestPeer::ChangePingRectCount(viewer), 1u);
  const PingRectSnapshot rect =
      DungeonCanvasViewerTestPeer::ChangePingRectAt(viewer, 0);
  EXPECT_EQ(rect.x, 510);
  EXPECT_EQ(rect.y, 0);
  EXPECT_EQ(rect.w, 2);
  EXPECT_EQ(rect.h, 1);
  EXPECT_GE(DungeonCanvasViewerTestPeer::ChangePingStartTime(viewer), 0.0);
}

TEST(DungeonCanvasViewerPingTest,
     TriggerObjectChangePingIgnoresUnchangedSnapshots) {
  DungeonCanvasViewer viewer;
  const std::vector<zelda3::RoomObject> objects = {
      zelda3::RoomObject{0x01, 2, 3, 0x00, 0},
      zelda3::RoomObject{0x02, 4, 5, 0x00, 0},
  };

  viewer.TriggerObjectChangePing(objects, objects);

  EXPECT_EQ(DungeonCanvasViewerTestPeer::ChangePingRectCount(viewer), 0u);
  EXPECT_LT(DungeonCanvasViewerTestPeer::ChangePingStartTime(viewer), 0.0);
}

TEST(DungeonCanvasViewerPingTest,
     TriggerObjectChangePingCapturesPreviousAndNextChangedBounds) {
  DungeonCanvasViewer viewer;
  const std::vector<zelda3::RoomObject> before = {
      zelda3::RoomObject{0x01, 2, 3, 0x00, 0},
      zelda3::RoomObject{0x02, 4, 5, 0x00, 0},
  };
  const std::vector<zelda3::RoomObject> after = {
      zelda3::RoomObject{0x01, 2, 3, 0x00, 0},
      zelda3::RoomObject{0x02, 6, 5, 0x00, 0},
  };

  viewer.TriggerObjectChangePing(before, after);

  ASSERT_EQ(DungeonCanvasViewerTestPeer::ChangePingRectCount(viewer), 2u);
  const PingRectSnapshot before_rect =
      DungeonCanvasViewerTestPeer::ChangePingRectAt(viewer, 0);
  const PingRectSnapshot after_rect =
      DungeonCanvasViewerTestPeer::ChangePingRectAt(viewer, 1);
  EXPECT_GE(before_rect.w, 8);
  EXPECT_GE(before_rect.h, 8);
  EXPECT_GE(after_rect.w, 8);
  EXPECT_GE(after_rect.h, 8);
  EXPECT_GE(DungeonCanvasViewerTestPeer::ChangePingStartTime(viewer), 0.0);
}

TEST(DungeonCanvasViewerNavigationTest, EntranceRenderContextRoundTrips) {
  DungeonCanvasViewer viewer;

  EXPECT_EQ(viewer.current_entrance_id(), -1);
  EXPECT_EQ(viewer.current_entrance_blockset(), 0xFF);

  viewer.SetEntranceRenderContext(/*entrance_id=*/0x12, /*blockset=*/0x2A);

  EXPECT_EQ(viewer.current_entrance_id(), 0x12);
  EXPECT_EQ(viewer.current_entrance_blockset(), 0x2A);

  viewer.ClearEntranceRenderContext();

  EXPECT_EQ(viewer.current_entrance_id(), -1);
  EXPECT_EQ(viewer.current_entrance_blockset(), 0xFF);
}

TEST(DungeonCanvasViewerNavigationTest,
     OpeningIssueReportKeepsDraftLocalUntilExplicitSave) {
  const std::filesystem::path temp_home = MakeTempHomeRoot();
  ASSERT_TRUE(std::filesystem::create_directories(temp_home));
  ScopedEnvVar scoped_home("HOME", temp_home.string());

  DungeonCanvasViewer viewer;
  const auto status = DungeonCanvasViewerTestPeer::PrepareIssueReportPopup(
      viewer, "Palette issue", "Room 001 palette mismatch",
      "Dungeon Palette Issue Report", "Room 0x001\nBG sheets: [1,2,3,4]", 0x01,
      0);
  ASSERT_TRUE(status.ok()) << status;

  EXPECT_TRUE(DungeonCanvasViewerTestPeer::last_log_path(viewer).empty());

  const auto save_status = DungeonCanvasViewerTestPeer::SaveIssueReport(viewer);
  ASSERT_TRUE(save_status.ok()) << save_status;

  const std::filesystem::path log_path =
      DungeonCanvasViewerTestPeer::last_log_path(viewer);
  ASSERT_FALSE(log_path.empty());
  ASSERT_TRUE(std::filesystem::exists(log_path));

  const std::string content = ReadFile(log_path);
  EXPECT_NE(content.find("Room 001 palette mismatch"), std::string::npos);
  EXPECT_NE(content.find("Dungeon Palette Issue Report"), std::string::npos);
  EXPECT_NE(content.find("Room 0x001\nBG sheets: [1,2,3,4]"),
            std::string::npos);

  std::filesystem::remove_all(temp_home);
}

TEST(DungeonCanvasViewerNavigationTest,
     IssueReportPopupFitsNarrowViewportWithoutOuterScrolling) {
  const std::filesystem::path temp_home = MakeTempHomeRoot();
  ASSERT_TRUE(std::filesystem::create_directories(temp_home));
  ScopedEnvVar scoped_home("HOME", temp_home.string());
  ScopedImGuiContext imgui;

  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(480.0f, 360.0f);
  io.DeltaTime = 1.0f / 60.0f;
  io.Fonts->AddFontDefault();
  unsigned char* font_pixels = nullptr;
  int font_width = 0;
  int font_height = 0;
  io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

  DungeonCanvasViewer viewer;
  bool opened = false;
  auto render_popup = [&]() {
    ImGui::NewFrame();
    ImGui::Begin("IssueReportHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    if (!opened) {
      DungeonCanvasViewerTestPeer::OpenIssueReportPopup(viewer);
      opened = true;
    }
    DungeonCanvasViewerTestPeer::RenderIssueReportPopup(viewer);
    const auto snapshot =
        DungeonCanvasViewerTestPeer::CaptureIssueReportPopup(viewer);
    ImGui::End();
    ImGui::Render();
    return snapshot;
  };
  IssueReportPopupSnapshot popup;
  for (int frame = 0; frame < 3; ++frame) {
    popup = render_popup();
  }

  ASSERT_TRUE(popup.found);
  EXPECT_LE(popup.size.x, 448.5f);
  EXPECT_LE(popup.size.y, 328.5f);
  EXPECT_GE(popup.position.x, 15.5f);
  EXPECT_GE(popup.position.y, 15.5f);
  EXPECT_EQ(popup.flags & ImGuiWindowFlags_AlwaysAutoResize, 0);
  EXPECT_NE(popup.flags & ImGuiWindowFlags_NoSavedSettings, 0);
  EXPECT_FALSE(popup.outer_scrollbar_y);
  ASSERT_TRUE(popup.body_found);
  const auto initial = popup;

  DungeonCanvasViewerTestPeer::ExpandIssueReportSections(viewer);
  const std::string error = "Could not write issue log: /a/very/long/" +
                            std::string(500, 'x') +
                            "\nPermission denied. Choose a writable location.";
  DungeonCanvasViewerTestPeer::SetIssueReportStatus(viewer, error, true);
  for (int frame = 0; frame < 3; ++frame) {
    popup = render_popup();
  }
  EXPECT_TRUE(popup.body_scrollbar_y);
  EXPECT_FALSE(popup.outer_scrollbar_y);
  EXPECT_FLOAT_EQ(popup.position.x, initial.position.x);
  EXPECT_FLOAT_EQ(popup.position.y, initial.position.y);
  EXPECT_FLOAT_EQ(popup.size.x, initial.size.x);
  EXPECT_FLOAT_EQ(popup.size.y, initial.size.y);
  EXPECT_FLOAT_EQ(popup.body_bounds.Max.y, initial.body_bounds.Max.y);
  EXPECT_LE(popup.content_end.x, popup.inner_bounds.Max.x);
  EXPECT_LE(popup.content_end.y, popup.inner_bounds.Max.y);

  std::filesystem::remove_all(temp_home);
}

TEST(DungeonCanvasViewerNavigationTest,
     IssueReportPopupFitsWorkAreaWithReservedMenuBar) {
  ScopedImGuiContext imgui;
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(480.0f, 360.0f);
  io.DeltaTime = 1.0f / 60.0f;
  io.Fonts->AddFontDefault();
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  const auto draw_menu_bar = []() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 14.0f));
    if (ImGui::BeginMainMenuBar()) {
      ImGui::TextUnformatted("File");
      ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar();
  };
  // Viewport work insets are committed on the following frame. Establish the
  // application's persistent menu before opening the dialog, as in the editor.
  for (int frame = 0; frame < 2; ++frame) {
    ImGui::NewFrame();
    draw_menu_bar();
    ImGui::Render();
  }

  DungeonCanvasViewer viewer;
  IssueReportPopupSnapshot popup;
  for (int frame = 0; frame < 3; ++frame) {
    ImGui::NewFrame();
    draw_menu_bar();
    ImGui::Begin("IssueReportHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    if (frame == 0) {
      DungeonCanvasViewerTestPeer::OpenIssueReportPopup(viewer);
    }
    DungeonCanvasViewerTestPeer::RenderIssueReportPopup(viewer);
    popup = DungeonCanvasViewerTestPeer::CaptureIssueReportPopup(viewer);
    ImGui::End();
    ImGui::Render();
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ASSERT_GT(viewport->WorkPos.y - viewport->Pos.y, 32.0f);
  ASSERT_TRUE(popup.found);
  EXPECT_GE(popup.position.x, viewport->WorkPos.x + 15.5f);
  EXPECT_GE(popup.position.y, viewport->WorkPos.y + 15.5f);
  EXPECT_LE(popup.position.x + popup.size.x,
            viewport->WorkPos.x + viewport->WorkSize.x - 15.5f);
  EXPECT_LE(popup.position.y + popup.size.y,
            viewport->WorkPos.y + viewport->WorkSize.y - 15.5f);
  EXPECT_FALSE(popup.outer_scrollbar_y);
}

#ifdef YAZE_WITH_GRPC
TEST(DungeonCanvasViewerNavigationTest,
     IssueReportCaptureKeepsFooterStableAtWrappingThreshold) {
  ScopedImGuiContext imgui;
  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = 1.0f / 60.0f;
  io.Fonts->AddFontDefault();
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  ImGui::NewFrame();
  const auto button_width = [](const char* label) {
    return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
  };
  const float fixed_width = button_width(ICON_MD_SAVE_AS " Save to Issue Log") +
                            button_width(ICON_MD_CONTENT_COPY " Copy Report") +
                            button_width(ICON_MD_CLOSE " Close") +
                            ImGui::GetStyle().ItemSpacing.x * 3;
  const float capture_width =
      button_width(ICON_MD_ADD_A_PHOTO " Capture Screenshot");
  const float recapture_width =
      button_width(ICON_MD_PHOTO_CAMERA " Re-capture Screenshot");
  // Choose a width where the old variable-width capture button added a row.
  io.DisplaySize =
      ImVec2(fixed_width + (capture_width + recapture_width) * 0.5f +
                 ImGui::GetStyle().WindowPadding.x * 2 + 32.0f,
             360.0f);
  ImGui::EndFrame();

  DungeonCanvasViewer viewer;
  bool opened = false;
  auto render_popup = [&]() {
    ImGui::NewFrame();
    ImGui::Begin("IssueReportHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    if (!opened) {
      DungeonCanvasViewerTestPeer::OpenIssueReportPopup(viewer);
      opened = true;
    }
    DungeonCanvasViewerTestPeer::RenderIssueReportPopup(viewer);
    const auto snapshot =
        DungeonCanvasViewerTestPeer::CaptureIssueReportPopup(viewer);
    ImGui::End();
    ImGui::Render();
    return snapshot;
  };
  IssueReportPopupSnapshot before;
  for (int frame = 0; frame < 3; ++frame) {
    before = render_popup();
  }
  ASSERT_TRUE(before.body_found);
  DungeonCanvasViewerTestPeer::SetIssueReportCaptureState(viewer, true);
  DungeonCanvasViewerTestPeer::SetIssueReportStatus(
      viewer, "Screenshot captured. Open Local files for its path.", false);
  for (int frame = 0; frame < 3; ++frame) {
    const auto after = render_popup();
    EXPECT_FALSE(after.outer_scrollbar_y);
    EXPECT_FLOAT_EQ(after.position.x, before.position.x);
    EXPECT_FLOAT_EQ(after.position.y, before.position.y);
    EXPECT_FLOAT_EQ(after.body_bounds.Max.y, before.body_bounds.Max.y);
    EXPECT_FLOAT_EQ(after.content_end.y, before.content_end.y);
    EXPECT_LE(after.content_end.x, after.inner_bounds.Max.x);
    EXPECT_LE(after.content_end.y, after.inner_bounds.Max.y);
  }
}
#endif

TEST(DungeonCanvasViewerNavigationTest,
     IssueReportStatusKeepsFullMultilineErrorInTooltip) {
  ScopedImGuiContext imgui;
  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = 1.0f / 60.0f;
  io.Fonts->AddFontDefault();
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  DungeonCanvasViewer viewer;
  const std::string error_path =
      "Could not save: /" + std::string(200, 'x') + ".json";
  const std::string error = error_path + "\nPermission denied.";
  DungeonCanvasViewerTestPeer::SetIssueReportStatus(viewer, error, true);
  ImRect status_bounds;
  std::string logged_text;
  for (int frame = 0; frame < 3; ++frame) {
    if (frame > 0) {
      io.AddMousePosEvent(status_bounds.Min.x + 2, status_bounds.Min.y + 2);
    }
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(360, 150), ImGuiCond_Always);
    ImGui::Begin("IssueStatusHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    status_bounds = DungeonCanvasViewerTestPeer::DrawIssueReportStatus(viewer);
    logged_text = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    EXPECT_FLOAT_EQ(status_bounds.GetHeight(), ImGui::GetTextLineHeight());
    EXPECT_LE(status_bounds.Max.x, ImGui::GetCurrentWindow()->InnerRect.Max.x);
    ImGui::End();
    ImGui::Render();
  }
  // ImGui's log uses native line endings even when the rendered text uses LF.
  // Keep checking both complete lines, including the untruncated path.
  EXPECT_NE(logged_text.find(error_path + IM_NEWLINE + "Permission denied."),
            std::string::npos);
}

TEST(DungeonCanvasViewerNavigationTest,
     DrawIssueReportIncludesRomContextAndSanePaletteEvents) {
  zelda3::PaletteDebugger::Get().Clear();

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());
  rom.set_filename("/tmp/palette_issue_test.sfc");

  DungeonCanvasViewer viewer(&rom);
  zelda3::Room room;

  zelda3::PaletteDebugger::Get().LogPaletteApplication("Test::Palette", 0x12,
                                                       true);

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x72);

  EXPECT_NE(report.find("ROM: palette_issue_test.sfc | CRC32:"),
            std::string::npos);
  EXPECT_NE(report.find("Path:/tmp/palette_issue_test.sfc"), std::string::npos);
  EXPECT_NE(report.find("[Test::Palette] palette=18"), std::string::npos);
  EXPECT_EQ(report.find("colors=1828354192"), std::string::npos);

  zelda3::PaletteDebugger::Get().Clear();
}

TEST(DungeonCanvasViewerNavigationTest,
     DrawIssueReportIncludesObjectTraceFootprint) {
  zelda3::PaletteDebugger::Get().Clear();

  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonCanvasViewer viewer(&rom);
  zelda3::Room room;
  zelda3::RoomObject obj(/*id=*/0xC8, /*x=*/4, /*y=*/5, /*size=*/0x11,
                         /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_ = MakeObjectTiles(8);
  room.GetTileObjects().push_back(obj);
  viewer.object_interaction().SetSelectedObjects({0});

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x72);

  EXPECT_NE(report.find("Descriptor tiles: count=8"), std::string::npos);
  EXPECT_NE(report.find("Object geometry: selection_bounds_px="),
            std::string::npos);
  EXPECT_NE(report.find("Drawer trace: status=ok"), std::string::npos);
  EXPECT_NE(report.find("context=room-stream-prefix+layout-tilewords"),
            std::string::npos);
  EXPECT_NE(report.find("bounds_tiles="), std::string::npos);
  EXPECT_NE(report.find("layer_counts BG1="), std::string::npos);
  EXPECT_NE(report.find("write[0] BG1 tile="), std::string::npos);

  zelda3::PaletteDebugger::Get().Clear();
}

TEST(DungeonCanvasViewerNavigationTest,
     DrawIssueReportReplaysConditionalEdgeAgainstRoomLayout) {
  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonCanvasViewer viewer(&rom);
  zelda3::Room room;
  zelda3::RoomObject edge(/*id=*/0x22, /*x=*/4, /*y=*/5, /*size=*/0,
                          /*layer=*/0);
  edge.tiles_loaded_ = true;
  edge.tiles_ = MakeObjectTiles(3);
  room.GetTileObjects().push_back(edge);
  room.bg1_buffer().SetTileAt(4, 5, 0x00E2);
  viewer.object_interaction().SetSelectedObjects({0});

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x72);

  EXPECT_NE(report.find("context=room-stream-prefix+layout-tilewords writes=3"),
            std::string::npos);
  EXPECT_NE(report.find("write[0] BG1 tile=(5,5) id=0x121"), std::string::npos);
  EXPECT_EQ(report.find("write[0] BG1 tile=(4,5) id=0x120"), std::string::npos);
}

TEST(DungeonCanvasViewerNavigationTest,
     DrawIssueReportPriorObjectCoverageOverridesMatchingLayout) {
  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonCanvasViewer viewer(&rom);
  zelda3::Room room;
  zelda3::RoomObject prior(/*id=*/0x11F, /*x=*/4, /*y=*/5, /*size=*/0,
                           /*layer=*/0);
  prior.tiles_loaded_ = true;
  prior.tiles_ = MakeObjectTiles(4);
  prior.tiles_[0].id_ = 0x0320;
  room.GetTileObjects().push_back(prior);

  zelda3::RoomObject edge(/*id=*/0x22, /*x=*/4, /*y=*/5, /*size=*/0,
                          /*layer=*/0);
  edge.tiles_loaded_ = true;
  edge.tiles_ = MakeObjectTiles(3);
  room.GetTileObjects().push_back(edge);
  room.bg1_buffer().SetTileAt(4, 5, 0x00E2);
  viewer.object_interaction().SetSelectedObjects({1});

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x72);

  EXPECT_NE(report.find("context=room-stream-prefix+layout-tilewords writes=4"),
            std::string::npos);
  EXPECT_NE(report.find("write[0] BG1 tile=(4,5) id=0x120"), std::string::npos);
}

TEST(DungeonCanvasViewerNavigationTest,
     RepeatedDrawIssueReportsReuseRetiredTraceSurfaces) {
  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonCanvasViewer viewer(&rom);
  zelda3::Room room;
  zelda3::RoomObject object(/*id=*/0x34, /*x=*/4, /*y=*/5, /*size=*/0,
                            /*layer=*/0);
  object.tiles_loaded_ = true;
  object.tiles_ = MakeObjectTiles(8);
  room.GetTileObjects().push_back(object);
  viewer.object_interaction().SetSelectedObjects({0});

  auto& arena = gfx::Arena::Get();
  arena.ClearTextureQueue();
  const size_t active_surfaces_before =
      arena.GetSurfaceCount() - arena.GetPooledSurfaceCount();
  const std::string first_report =
      DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x72);
  ASSERT_NE(first_report.find("Drawer trace: status=ok"), std::string::npos);
  ASSERT_EQ(arena.GetSurfaceCount() - arena.GetPooledSurfaceCount(),
            active_surfaces_before);
  const size_t surfaces_after_first_report = arena.GetSurfaceCount();

  for (int iteration = 0; iteration < 8; ++iteration) {
    const std::string report =
        DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x72);
    ASSERT_NE(report.find("Drawer trace: status=ok"), std::string::npos);
    ASSERT_EQ(arena.GetSurfaceCount() - arena.GetPooledSurfaceCount(),
              active_surfaces_before);
  }

  EXPECT_EQ(arena.GetSurfaceCount(), surfaces_after_first_report)
      << "Trace-only reports should reuse the two explicitly retired object "
         "surfaces instead of retaining two more per capture";
  EXPECT_EQ(arena.texture_command_queue_size(), 0u);
  EXPECT_EQ(arena.retired_texture_handle_count(), 0u);
}

TEST(DungeonCanvasViewerNavigationTest,
     DrawIssueReportDistinguishesFloorCopyDescriptorsAndEffectiveTiles) {
  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonCanvasViewer viewer(&rom);
  viewer.object_interaction().SetSelectedObjects({0});

  auto build_report = [&](int16_t object_id) {
    zelda3::Room room;
    room.set_floor1(6);
    room.set_floor2(11);
    zelda3::RoomObject object(object_id, /*x=*/4, /*y=*/5, /*size=*/0,
                              /*layer=*/0);
    object.tiles_loaded_ = true;
    object.tiles_ = MakeObjectTiles(8);
    room.GetTileObjects().push_back(object);
    return DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room,
                                                             0x72);
  };

  const std::string floor1_report = build_report(0x00C4);
  EXPECT_NE(floor1_report.find("Floor1:6 Floor2:11"), std::string::npos);
  EXPECT_NE(floor1_report.find("Descriptor tiles: count=8 [0]=0x120"),
            std::string::npos);
  EXPECT_NE(
      floor1_report.find("Floor copy source: Floor 1 (room.floor1) pattern=6"),
      std::string::npos);
  EXPECT_NE(floor1_report.find("Effective floor-copy tiles: count=8 "
                               "[0]=0x000"),
            std::string::npos);

  const std::string floor2_report = build_report(0x00DB);
  EXPECT_NE(floor2_report.find("Floor1:6 Floor2:11"), std::string::npos);
  EXPECT_NE(floor2_report.find("Descriptor tiles: count=8 [0]=0x120"),
            std::string::npos);
  EXPECT_NE(
      floor2_report.find("Floor copy source: Floor 2 (room.floor2) pattern=11"),
      std::string::npos);
  EXPECT_NE(floor2_report.find("Effective floor-copy tiles: count=8 "
                               "[0]=0x000"),
            std::string::npos);
}

TEST(DungeonCanvasViewerNavigationTest,
     DrawIssueReportSamplesPaletteFromObjectTraceFootprint) {
  auto& palette_debugger = zelda3::PaletteDebugger::Get();
  palette_debugger.Clear();

  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonCanvasViewer viewer(&rom);
  zelda3::Room room;
  zelda3::RoomObject obj(/*id=*/0xC8, /*x=*/4, /*y=*/5, /*size=*/0x11,
                         /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_ = MakeObjectTiles(8);
  room.GetTileObjects().push_back(obj);
  viewer.object_interaction().SetSelectedObjects({0});

  std::vector<uint8_t> pixels(512 * 512, 0);
  pixels[(5 * 8) * 512 + (4 * 8)] = 0x71;
  pixels[(5 * 8 + 4) * 512 + (4 * 8 + 4)] = 0x01;

  gfx::Bitmap bitmap;
  bitmap.Create(/*width=*/512, /*height=*/512, /*depth=*/8, pixels);
  std::vector<SDL_Color> render_palette(256);
  for (int i = 0; i < 256; ++i) {
    render_palette[i] =
        SDL_Color{static_cast<uint8_t>(i), static_cast<uint8_t>(255 - i),
                  static_cast<uint8_t>(i / 2), 255};
  }
  bitmap.SetPalette(render_palette);
  palette_debugger.SetCurrentRenderPalette(render_palette);
  palette_debugger.SetCurrentBitmap(&bitmap);

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x72);

  EXPECT_NE(report.find("Palette sample object-trace[0] tile=(4,5) pal=0 "
                        "(36,44): idx=1"),
            std::string::npos);
  EXPECT_EQ(report.find("Palette sample selection-origin"), std::string::npos);
  EXPECT_EQ(report.find("(32,40): idx=113"), std::string::npos);

  palette_debugger.Clear();
}

TEST(DungeonCanvasViewerNavigationTest,
     DrawIssueReportFlagsStaleObjectSelection) {
  DungeonCanvasViewer viewer;
  zelda3::Room room;
  viewer.object_interaction().SetSelectedObjects({7});

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x5A);

  EXPECT_NE(report.find("Selected objects: 1"), std::string::npos);
  EXPECT_NE(report.find("Selected object index 7 is stale"), std::string::npos);
  EXPECT_NE(report.find("room currently has 0 object(s)"), std::string::npos);
}

TEST(DungeonCanvasViewerNavigationTest,
     SelectionIssueReportIncludesTraceSelectionDelta) {
  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonCanvasViewer viewer(&rom);
  zelda3::Room room;
  zelda3::RoomObject obj(/*id=*/0x71, /*x=*/29, /*y=*/14, /*size=*/0x04,
                         /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_ = MakeObjectTiles(1);
  room.GetTileObjects().push_back(obj);
  viewer.object_interaction().SetSelectedObjects({0});

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildSelectionIssueReport(viewer, room,
                                                             0x59);

  EXPECT_NE(report.find("geometry: selection_bounds_px=(232,112,8,64) "
                        "object_origin_px=(232,112)"),
            std::string::npos);
  EXPECT_NE(report.find("trace: writes=8 unique_cells=8 "
                        "bounds_px=(232,112,8,64) "
                        "delta_vs_selection_px=(+0,+0,+0,+0)"),
            std::string::npos);
}

TEST(DungeonCanvasViewerNavigationTest,
     SelectionIssueReportFlagsStaleObjectSelection) {
  DungeonCanvasViewer viewer;
  zelda3::Room room;
  viewer.object_interaction().SetSelectedObjects({11});

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildSelectionIssueReport(viewer, room,
                                                             0x5B);

  EXPECT_NE(report.find("Selected objects: 1"), std::string::npos);
  EXPECT_NE(report.find("- object[11] unavailable"), std::string::npos);
  EXPECT_NE(report.find("room currently has 0 object(s)"), std::string::npos);
}

TEST(DungeonCanvasViewerNavigationTest,
     SelectionIssueReportIncludesDoorGeometryAndEncoding) {
  DungeonCanvasViewer viewer;
  zelda3::Room room;
  auto door =
      MakeDoor(zelda3::DoorDirection::South, zelda3::DoorType::NormalDoor);
  door.position = 6;
  door.byte1 = 0x18;
  door.byte2 = 0x00;
  room.AddDoor(door);
  viewer.object_interaction().SelectEntity(EntityType::Door, 0);

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildSelectionIssueReport(viewer, room,
                                                             0x72);

  EXPECT_NE(report.find("Selected door[0]:"), std::string::npos);
  EXPECT_NE(report.find("- tile_coords=("), std::string::npos);
  EXPECT_NE(report.find("- dims_tiles=(4,3) editor_dims_tiles=(4,3)"),
            std::string::npos);
  EXPECT_NE(report.find("- bounds_px=("), std::string::npos);
  EXPECT_NE(report.find("- encoded=(0x"), std::string::npos);
  EXPECT_NE(report.find("original=(0x18,0x00)"), std::string::npos);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDungeonConnectedRoomLinksUsesReciprocalDoorsAndTransportTargets) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::North, zelda3::DoorType::NormalDoor));
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::West, zelda3::DoorType::BombableDoor));
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::East, zelda3::DoorType::CaveExit));
  room.SetStaircaseRoom(0, 0x28);
  room.SetHolewarp(0x6A);
  ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());

  const auto links = CollectDungeonConnectedRoomLinks(
      0x44, room, [](int neighbor_room_id, zelda3::DoorDirection dir) {
        return neighbor_room_id == 0x34 && dir == zelda3::DoorDirection::South;
      });

  ASSERT_EQ(links.size(), 3u);

  EXPECT_EQ(links[0].from_room_id, 0x44);
  EXPECT_EQ(links[0].to_room_id, 0x34);
  EXPECT_EQ(links[0].type, DungeonConnectedLinkType::Door);
  EXPECT_EQ(links[0].direction, zelda3::DoorDirection::North);

  EXPECT_EQ(links[1].from_room_id, 0x44);
  EXPECT_EQ(links[1].to_room_id, 0x28);
  EXPECT_EQ(links[1].type, DungeonConnectedLinkType::Staircase);

  EXPECT_EQ(links[2].from_room_id, 0x44);
  EXPECT_EQ(links[2].to_room_id, 0x6A);
  EXPECT_EQ(links[2].type, DungeonConnectedLinkType::Holewarp);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDungeonConnectedRoomLinksSkipsExitDoorsAndUnusedStairHeaders) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  for (const auto type : {
           zelda3::DoorType::ExitLower,
           zelda3::DoorType::FancyDungeonExit,
           zelda3::DoorType::FancyDungeonExitLower,
           zelda3::DoorType::CaveExit,
           zelda3::DoorType::LitCaveExitLower,
           zelda3::DoorType::DungeonSwapMarker,
           zelda3::DoorType::LayerSwapMarker,
       }) {
    room.AddDoor(MakeDoor(zelda3::DoorDirection::East, type));
  }
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::South, zelda3::DoorType::NormalDoor));
  room.SetStaircaseRoom(0, 0);
  room.SetStaircaseRoom(1, 0x17);
  room.SetHolewarp(0);

  const auto links = CollectDungeonConnectedRoomLinks(
      0x20, room, [](int neighbor_room_id, zelda3::DoorDirection dir) {
        return neighbor_room_id == 0x30 && dir == zelda3::DoorDirection::North;
      });

  ASSERT_EQ(links.size(), 1u);
  EXPECT_EQ(links[0].to_room_id, 0x30);
  EXPECT_EQ(links[0].type, DungeonConnectedLinkType::Door);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDungeonConnectedRoomLinksUsesStraightInterroomStairObjects) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  room.SetStaircaseRoom(0, 0x17);
  ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0xF9E, 4, 5, 0, 0)).ok());

  const auto links = CollectDungeonConnectedRoomLinks(0x20, room, nullptr);

  ASSERT_EQ(links.size(), 1u);
  EXPECT_EQ(links[0].from_room_id, 0x20);
  EXPECT_EQ(links[0].to_room_id, 0x17);
  EXPECT_EQ(links[0].type, DungeonConnectedLinkType::Staircase);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphKeepsCrossBlocksetStaircaseTargets) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetBlockset(0x01);
  start.SetStaircaseRoom(0, 0x40);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  start.SetLoaded(true);

  auto& stair_target = rooms[0x40];
  ClearRoomLinks(&stair_target);
  stair_target.SetBlockset(0x02);
  stair_target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_EQ(graph.room_count, 2);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_TRUE(graph.room_mask[0x40]);
  ASSERT_EQ(graph.links.size(), 1u);
  EXPECT_EQ(graph.links[0].from_room_id, 0x10);
  EXPECT_EQ(graph.links[0].to_room_id, 0x40);
  EXPECT_EQ(graph.links[0].type, DungeonConnectedLinkType::Staircase);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphReachesRoomZeroThroughStaircase) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x8000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  start.SetStaircaseRoom(0, 0);
  start.SetLoaded(true);
  auto& target = rooms[0];
  ClearRoomLinks(&target);
  target.SetLoaded(true);
  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);
  EXPECT_EQ(graph.room_count, 2);
  EXPECT_TRUE(graph.room_mask[0]);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_TRUE(graph.staircase_issues.empty());
  ASSERT_EQ(graph.links.size(), 1u);
  EXPECT_EQ(graph.links[0].to_room_id, 0);
  EXPECT_EQ(graph.links[0].type, DungeonConnectedLinkType::Staircase);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDiagnosticsTagsLinksWithProvenanceFields) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::North, zelda3::DoorType::NormalDoor));
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::West, zelda3::DoorType::BombableDoor));
  room.SetStaircaseRoom(0, 0x28);
  ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  room.SetHolewarp(0x6A);

  const auto diagnostics = CollectDungeonConnectedRoomLinkDiagnostics(
      0x44, room, [](int, zelda3::DoorDirection) { return true; });

  ASSERT_EQ(diagnostics.links.size(), 4u);
  EXPECT_TRUE(diagnostics.staircase_issues.empty());

  EXPECT_EQ(diagnostics.links[0].type, DungeonConnectedLinkType::Door);
  EXPECT_EQ(diagnostics.links[0].direction, zelda3::DoorDirection::North);
  EXPECT_EQ(diagnostics.links[0].door_index, 0);
  EXPECT_EQ(diagnostics.links[0].door_type, zelda3::DoorType::NormalDoor);
  EXPECT_EQ(diagnostics.links[0].slot_index, -1);

  EXPECT_EQ(diagnostics.links[1].type, DungeonConnectedLinkType::Door);
  EXPECT_EQ(diagnostics.links[1].direction, zelda3::DoorDirection::West);
  EXPECT_EQ(diagnostics.links[1].door_index, 1);
  EXPECT_EQ(diagnostics.links[1].door_type, zelda3::DoorType::BombableDoor);

  EXPECT_EQ(diagnostics.links[2].type, DungeonConnectedLinkType::Staircase);
  EXPECT_EQ(diagnostics.links[2].slot_index, 0);
  EXPECT_EQ(diagnostics.links[2].object_id, 0x138);
  EXPECT_EQ(diagnostics.links[2].to_room_id, 0x28);

  EXPECT_EQ(diagnostics.links[3].type, DungeonConnectedLinkType::Holewarp);
  EXPECT_EQ(diagnostics.links[3].to_room_id, 0x6A);
  EXPECT_EQ(diagnostics.links[3].slot_index, -1);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDiagnosticsEmitsStaleStaircaseEntriesForUnusedHeaderSlots) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  // Slot 0: header populated AND placed object → real link.
  // Slot 1: header populated but no second placed object → UnusedHeader.
  // Slot 2: header populated, also UnusedHeader.
  // Slot 3: header zero, no diagnostic.
  room.SetStaircaseRoom(0, 0x40);
  room.SetStaircaseRoom(1, 0x55);
  room.SetStaircaseRoom(2, 0x77);
  ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());

  const auto diagnostics =
      CollectDungeonConnectedRoomLinkDiagnostics(0x10, room, nullptr);

  ASSERT_EQ(diagnostics.links.size(), 1u);
  EXPECT_EQ(diagnostics.links[0].type, DungeonConnectedLinkType::Staircase);
  EXPECT_EQ(diagnostics.links[0].slot_index, 0);
  EXPECT_EQ(diagnostics.links[0].to_room_id, 0x40);

  ASSERT_EQ(diagnostics.staircase_issues.size(), 2u);
  EXPECT_EQ(diagnostics.staircase_issues[0].from_room_id, 0x10);
  EXPECT_EQ(diagnostics.staircase_issues[0].kind,
            DungeonStaircaseIssueKind::UnusedHeader);
  EXPECT_EQ(diagnostics.staircase_issues[0].slot_index, 1);
  EXPECT_EQ(diagnostics.staircase_issues[0].header_room_id, 0x55);
  EXPECT_EQ(diagnostics.staircase_issues[1].kind,
            DungeonStaircaseIssueKind::UnusedHeader);
  EXPECT_EQ(diagnostics.staircase_issues[1].slot_index, 2);
  EXPECT_EQ(diagnostics.staircase_issues[1].header_room_id, 0x77);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDiagnosticsPreservesPlacedStaircaseDestinationRoomZero) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  // A placed stair consumes slot 0 and targets valid room 000. The remaining
  // zero-filled, unconsumed header slots must not create links or diagnostics.
  ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());

  const auto diagnostics =
      CollectDungeonConnectedRoomLinkDiagnostics(0x10, room, nullptr);

  EXPECT_TRUE(diagnostics.staircase_issues.empty());
  ASSERT_EQ(diagnostics.links.size(), 1u);
  EXPECT_EQ(diagnostics.links[0].type, DungeonConnectedLinkType::Staircase);
  EXPECT_EQ(diagnostics.links[0].slot_index, 0);
  EXPECT_EQ(diagnostics.links[0].to_room_id, 0);
  EXPECT_EQ(diagnostics.links[0].object_id, 0x138);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDiagnosticsFlagsExcessPlacedStaircaseObjectsAsExtra) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  // 5 placed stair objects → only first 4 consume header slots, the 5th is
  // unreachable at runtime. Headers all valid so the first 4 produce real
  // links and slot indices 0..3 are exercised.
  room.SetStaircaseRoom(0, 0x21);
  room.SetStaircaseRoom(1, 0x22);
  room.SetStaircaseRoom(2, 0x23);
  room.SetStaircaseRoom(3, 0x24);
  for (int i = 0; i < 5; ++i) {
    ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0x138, i, 5, 0, 0)).ok());
  }

  const auto diagnostics =
      CollectDungeonConnectedRoomLinkDiagnostics(0x10, room, nullptr);

  ASSERT_EQ(diagnostics.links.size(), 4u);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(diagnostics.links[i].type, DungeonConnectedLinkType::Staircase);
    EXPECT_EQ(diagnostics.links[i].slot_index, static_cast<int>(i));
  }
  ASSERT_EQ(diagnostics.staircase_issues.size(), 1u);
  EXPECT_EQ(diagnostics.staircase_issues[0].kind,
            DungeonStaircaseIssueKind::ExtraPlacedObject);
  EXPECT_EQ(diagnostics.staircase_issues[0].object_id, 0x138);
  EXPECT_EQ(diagnostics.staircase_issues[0].slot_index, -1);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     FormatDungeonConnectedLinkDescriptionDescribesAllLinkTypes) {
  DungeonConnectedRoomLink door{};
  door.from_room_id = 0x10;
  door.to_room_id = 0x11;
  door.type = DungeonConnectedLinkType::Door;
  door.direction = zelda3::DoorDirection::South;
  door.door_index = 2;
  door.door_type = zelda3::DoorType::BombableDoor;
  EXPECT_EQ(FormatDungeonConnectedLinkDescription(door),
            "Door (South, type 0x2E) -> [011]");

  DungeonConnectedRoomLink stair{};
  stair.from_room_id = 0x10;
  stair.to_room_id = 0x40;
  stair.type = DungeonConnectedLinkType::Staircase;
  stair.slot_index = 1;
  stair.object_id = 0x138;
  EXPECT_EQ(FormatDungeonConnectedLinkDescription(stair),
            "Estimated staircase slot 1 obj 0x138 -> [040]");

  DungeonConnectedRoomLink warp{};
  warp.from_room_id = 0x10;
  warp.to_room_id = 0x6A;
  warp.type = DungeonConnectedLinkType::Holewarp;
  EXPECT_EQ(FormatDungeonConnectedLinkDescription(warp), "Holewarp -> [06A]");
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     FormatDungeonStaircaseIssueDescribesAllKinds) {
  DungeonStaircaseIssue unused{};
  unused.from_room_id = 0x10;
  unused.kind = DungeonStaircaseIssueKind::UnusedHeader;
  unused.slot_index = 2;
  unused.header_room_id = 0x55;
  EXPECT_EQ(
      FormatDungeonStaircaseIssueDescription(unused),
      "Possibly unused staircase slot 2 -> [055] (no placed interroom-stair "
      "object consumes this slot)");

  DungeonStaircaseIssue missing_invalid{};
  missing_invalid.kind = DungeonStaircaseIssueKind::MissingDestination;
  missing_invalid.slot_index = 1;
  missing_invalid.header_room_id = 0xFFFF;
  missing_invalid.object_id = 0x138;
  EXPECT_EQ(
      FormatDungeonStaircaseIssueDescription(missing_invalid),
      "Estimated missing staircase destination at slot 1 (placed object 0x138, "
      "header value 0xFFFF (out of range))");

  DungeonStaircaseIssue extra{};
  extra.kind = DungeonStaircaseIssueKind::ExtraPlacedObject;
  extra.object_id = 0x138;
  EXPECT_EQ(FormatDungeonStaircaseIssueDescription(extra),
            "Additional staircase object 0x138 beyond the estimated 4 slots "
            "(runtime mapping unverified)");
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphAggregatesStaircaseIssueEntries) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  start.SetStaircaseRoom(2, 0x55);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  start.SetLoaded(true);

  auto& target = rooms[0x40];
  ClearRoomLinks(&target);
  target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_EQ(graph.room_count, 2);
  EXPECT_FALSE(graph.dungeon_scope_active);
  ASSERT_EQ(graph.links.size(), 1u);
  EXPECT_EQ(graph.links[0].slot_index, 0);
  EXPECT_EQ(graph.links[0].object_id, 0x138);

  ASSERT_EQ(graph.staircase_issues.size(), 1u);
  EXPECT_EQ(graph.staircase_issues[0].from_room_id, 0x10);
  EXPECT_EQ(graph.staircase_issues[0].kind,
            DungeonStaircaseIssueKind::UnusedHeader);
  EXPECT_EQ(graph.staircase_issues[0].slot_index, 2);
  EXPECT_EQ(graph.staircase_issues[0].header_room_id, 0x55);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphIgnoresUnusedStaircaseHeaderTargets) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  start.SetLoaded(true);

  auto& stale_target = rooms[0x40];
  ClearRoomLinks(&stale_target);
  stale_target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_EQ(graph.room_count, 1);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_FALSE(graph.room_mask[0x40]);
  EXPECT_TRUE(graph.links.empty());

  ASSERT_EQ(graph.staircase_issues.size(), 1u);
  EXPECT_EQ(graph.staircase_issues[0].from_room_id, 0x10);
  EXPECT_EQ(graph.staircase_issues[0].kind,
            DungeonStaircaseIssueKind::UnusedHeader);
  EXPECT_EQ(graph.staircase_issues[0].slot_index, 0);
  EXPECT_EQ(graph.staircase_issues[0].header_room_id, 0x40);
}

namespace {

// Writes a minimal dungeons.json under root/Docs/Dev/Planning/ describing the
// passed-in dungeon entries. Returns the temp root for cleanup.
std::filesystem::path MakeProjectRegistryRoot(const std::string& json_body) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("yaze_connected_scope_test_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  const std::filesystem::path planning = root / "Docs" / "Dev" / "Planning";
  std::filesystem::create_directories(planning);
  std::ofstream(planning / "dungeons.json") << json_body;
  return root;
}

absl::Status LoadOracleProject(project::YazeProject* project,
                               const std::filesystem::path& root) {
  project->name = "Oracle of Secrets";
  project->filepath = (root / "Oracle-of-Secrets.yaze").string();
  if (auto s = project->hack_manifest.LoadFromString(R"json({
        "manifest_version": 1,
        "hack_name": "Oracle of Secrets"
      })json");
      !s.ok()) {
    return s;
  }
  return project->hack_manifest.LoadProjectRegistry(root.string());
}

}  // namespace

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphScopesToCurrentDungeonWhenProjectRegistryLoaded) {
  const std::filesystem::path root = MakeProjectRegistryRoot(R"json({
    "dungeons": [
      {
        "id": "D1",
        "name": "Test Dungeon",
        "rooms": [
          {"id": "0x10", "name": "Start"},
          {"id": "0x11", "name": "Adjacent"}
        ]
      }
    ]
  })json");

  project::YazeProject project;
  ASSERT_TRUE(LoadOracleProject(&project, root).ok());

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.AddDoor(
      MakeDoor(zelda3::DoorDirection::East, zelda3::DoorType::NormalDoor));
  start.SetLoaded(true);

  auto& neighbor_in_scope = rooms[0x11];
  ClearRoomLinks(&neighbor_in_scope);
  neighbor_in_scope.AddDoor(
      MakeDoor(zelda3::DoorDirection::West, zelda3::DoorType::NormalDoor));
  neighbor_in_scope.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  viewer.SetProject(&project);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_TRUE(graph.dungeon_scope_active);
  EXPECT_EQ(graph.room_count, 2);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_TRUE(graph.room_mask[0x11]);
  EXPECT_TRUE(graph.out_of_scope_links.empty());

  std::filesystem::remove_all(root);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphIncludesUnlinkedScopedRoomsWithFloorLabels) {
  const std::filesystem::path root = MakeProjectRegistryRoot(R"json({
    "dungeons": [
      {
        "id": "D6",
        "name": "Goron Mines",
        "rooms": [
          {"id": "0x10", "name": "F1 Start", "floor": "F1", "grid_row": 1, "grid_col": 1},
          {"id": "0x11", "name": "F1 East", "floor": "F1", "grid_row": 1, "grid_col": 2},
          {"id": "0x40", "name": "B1 Side", "floor": "B1", "grid_row": 4, "grid_col": 1}
        ]
      }
    ]
  })json");

  project::YazeProject project;
  ASSERT_TRUE(LoadOracleProject(&project, root).ok());

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.AddDoor(
      MakeDoor(zelda3::DoorDirection::East, zelda3::DoorType::NormalDoor));
  start.SetLoaded(true);

  auto& neighbor_in_scope = rooms[0x11];
  ClearRoomLinks(&neighbor_in_scope);
  neighbor_in_scope.AddDoor(
      MakeDoor(zelda3::DoorDirection::West, zelda3::DoorType::NormalDoor));
  neighbor_in_scope.SetLoaded(true);

  auto& unlinked_b1 = rooms[0x40];
  ClearRoomLinks(&unlinked_b1);
  unlinked_b1.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  viewer.SetProject(&project);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_TRUE(graph.dungeon_scope_active);
  EXPECT_EQ(graph.room_count, 3);
  EXPECT_EQ(graph.unlinked_room_count, 1);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_TRUE(graph.room_mask[0x11]);
  EXPECT_TRUE(graph.room_mask[0x40]);
  EXPECT_TRUE(graph.room_positions[0x10].connected_to_start);
  EXPECT_TRUE(graph.room_positions[0x11].connected_to_start);
  EXPECT_FALSE(graph.room_positions[0x40].connected_to_start);
  EXPECT_EQ(graph.room_positions[0x40].col, 1);
  EXPECT_EQ(graph.room_positions[0x40].row, 4);
  EXPECT_EQ(graph.room_floor_labels[0x10], "F1");
  EXPECT_EQ(graph.room_floor_labels[0x40], "B1");
  EXPECT_EQ(graph.floor_order, (std::vector<std::string>{"F1", "B1"}));

  std::filesystem::remove_all(root);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     UnmaterializedUnlinkedRoomLoadsFromRomBeforePreview) {
  const std::filesystem::path root = MakeProjectRegistryRoot(R"json({
    "dungeons": [{"id": "D6", "name": "Goron Mines", "rooms": [
      {"id": "0x10", "name": "Start"},
      {"id": "0x40", "name": "Unlinked", "floor": "B1"}
    ]}]
  })json");
  project::YazeProject project;
  ASSERT_TRUE(LoadOracleProject(&project, root).ok());

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto write_pointer = [&](int address, int pc) {
    const int snes = PcToSnes(pc);
    for (int byte = 0; byte < 3; ++byte) {
      rom.mutable_data()[address + byte] = (snes >> (8 * byte)) & 0xFF;
    }
  };
  constexpr int kHeaderTablePc = 0x0F6000;
  constexpr int kHeaderPc = 0x114000;
  constexpr int kObjectTablePc = 0x0F8000;
  constexpr int kObjectPc = 0x0F9000;
  write_pointer(zelda3::kRoomHeaderPointer, kHeaderTablePc);
  rom.mutable_data()[zelda3::kRoomHeaderPointerBank] =
      (PcToSnes(kHeaderPc) >> 16) & 0xFF;
  ASSERT_TRUE(
      rom.WriteWord(kHeaderTablePc + 0x40 * 2, PcToSnes(kHeaderPc) & 0xFFFF)
          .ok());
  std::vector<uint8_t> header(14, 0);
  header[1] = 0x12;  // Distinctive ROM palette, absent from a default Room.
  header[2] = 0x23;  // Distinctive ROM room blockset.
  ASSERT_TRUE(rom.WriteVector(kHeaderPc, header).ok());
  write_pointer(zelda3::kRoomObjectPointer, kObjectTablePc);
  write_pointer(kObjectTablePc + 0x40 * 3, kObjectPc);
  ASSERT_TRUE(rom.WriteVector(kObjectPc, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
                                          0xFF, 0xFF, 0xFF})
                  .ok());
  ASSERT_TRUE(
      rom.WriteWord(zelda3::kRoomItemsPointers + 0x40 * 2, 0xFF00).ok());
  ASSERT_TRUE(rom.WriteWord(0xFF00, 0xFFFF).ok());
  ASSERT_TRUE(
      rom.WriteWord(SnesToPc(zelda3::kRoomLayoutPointers.front()), 0xFFFF)
          .ok());

  zelda3::GameData game_data;
  game_data.graphics_buffer.assign(zelda3::kNumGfxSheets * 4096, 0);
  game_data.palette_groups.dungeon_main.AddPalette(gfx::SnesPalette(
      std::vector<gfx::SnesColor>(90, gfx::SnesColor(64, 96, 128))));
  DungeonRoomStore rooms(&rom, &game_data);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetTileObjects({zelda3::RoomObject(0x01, 5, 5, 0, 0)});
  start.SetLoaded(true);
  start.ClearSaveDirtyState();
  ASSERT_EQ(rooms.GetIfMaterialized(0x40), nullptr);

  DungeonCanvasViewer viewer(&rom);
  viewer.RefreshRomBackedState(&rom, &game_data, &rooms, 0x10);
  viewer.SetProject(&project);
  viewer.SetEntranceRenderContext(5, 4);
  viewer.SetEntranceBlocksetResolver(
      [](int room_id) -> uint8_t { return room_id == 0x40 ? 7 : 4; });
  viewer.object_interaction().SetSelectedObjects({0});

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);
  ASSERT_TRUE(graph.room_mask[0x40]);
  ASSERT_EQ(graph.unlinked_room_count, 1);
  auto* unlinked = rooms.GetIfLoaded(0x40);
  ASSERT_NE(unlinked, nullptr);
  EXPECT_EQ(unlinked->id(), 0x40);
  EXPECT_EQ(unlinked->palette(), 0x12);
  EXPECT_EQ(unlinked->blockset(), 0x23);
  EXPECT_FALSE(unlinked->HasUnsavedChanges());

  EXPECT_NE(DungeonCanvasViewerTestPeer::PrepareConnectedRoom(viewer, 0x40),
            nullptr);
  EXPECT_EQ(unlinked->render_entrance_blockset(), 7);
  EXPECT_EQ(viewer.current_room_id(), 0x10);
  EXPECT_EQ(viewer.current_entrance_blockset(), 4);
  EXPECT_EQ(viewer.object_interaction().GetSelectedObjectIndices(),
            (std::vector<size_t>{0}));
  EXPECT_FALSE(start.HasUnsavedChanges());
  EXPECT_FALSE(unlinked->HasUnsavedChanges());

  std::filesystem::remove_all(root);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphFlagsCrossDungeonStairLinksAsOutOfScope) {
  const std::filesystem::path root = MakeProjectRegistryRoot(R"json({
    "dungeons": [
      {"id": "D1", "name": "First", "rooms": [{"id": "0x10", "name": "Start"}]},
      {"id": "D2", "name": "Second", "rooms": [{"id": "0x40", "name": "Far"}]}
    ]
  })json");

  project::YazeProject project;
  ASSERT_TRUE(LoadOracleProject(&project, root).ok());

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  start.SetLoaded(true);

  auto& far_target = rooms[0x40];
  ClearRoomLinks(&far_target);
  far_target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  viewer.SetProject(&project);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_TRUE(graph.dungeon_scope_active);
  EXPECT_EQ(graph.room_count, 1);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_FALSE(graph.room_mask[0x40]);
  EXPECT_TRUE(graph.links.empty());

  ASSERT_EQ(graph.out_of_scope_links.size(), 1u);
  EXPECT_EQ(graph.out_of_scope_links[0].from_room_id, 0x10);
  EXPECT_EQ(graph.out_of_scope_links[0].to_room_id, 0x40);
  EXPECT_EQ(graph.out_of_scope_links[0].type,
            DungeonConnectedLinkType::Staircase);
  EXPECT_EQ(graph.out_of_scope_links[0].slot_index, 0);
  EXPECT_EQ(graph.out_of_scope_links[0].object_id, 0x138);

  std::filesystem::remove_all(root);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     ApplyConnectedStaircaseIssueAutoFixesPreservesUnverifiedHeaders) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  start.SetStaircaseRoom(1, 0x41);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  start.ClearHeaderDirty();
  start.SetLoaded(true);

  auto& linked_target = rooms[0x40];
  ClearRoomLinks(&linked_target);
  linked_target.SetLoaded(true);

  auto& stale_target = rooms[0x41];
  ClearRoomLinks(&stale_target);
  stale_target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  int batch_calls = 0;
  viewer.SetMetadataBatchEditCallback(
      [&](const std::vector<RoomMetadataRequest>& requests) {
        ++batch_calls;
        for (const auto& request : requests) {
          const auto status = ValidateRoomMetadataEdit(request.edit);
          if (!status.ok()) {
            return status;
          }
        }
        for (const auto& request : requests) {
          const auto status =
              ApplyRoomMetadataEdit(rooms[request.room_id], request.edit);
          if (!status.ok()) {
            return status;
          }
        }
        viewer.InvalidateConnectedRoomGraph();
        return absl::OkStatus();
      });

  const auto before =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);
  ASSERT_EQ(before.staircase_issues.size(), 1u);
  EXPECT_EQ(before.staircase_issues[0].kind,
            DungeonStaircaseIssueKind::UnusedHeader);

  EXPECT_EQ(DungeonCanvasViewerTestPeer::ApplyConnectedStaircaseIssueAutoFixes(
                viewer, 0x10),
            0);
  EXPECT_EQ(start.staircase_room(0), 0x40);
  EXPECT_EQ(start.staircase_room(1), 0x41);
  EXPECT_FALSE(start.header_dirty());
  EXPECT_EQ(batch_calls, 0);

  const auto after =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);
  ASSERT_EQ(after.staircase_issues.size(), before.staircase_issues.size());
  EXPECT_EQ(after.room_count, before.room_count);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphWithoutProjectFallsBackToFullTransitiveScope) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  start.SetLoaded(true);

  auto& target = rooms[0x40];
  ClearRoomLinks(&target);
  target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  // Intentionally no SetProject() call.

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_FALSE(graph.dungeon_scope_active);
  EXPECT_EQ(graph.room_count, 2);
  EXPECT_TRUE(graph.out_of_scope_links.empty());
}

TEST(
    DungeonCanvasViewerConnectedGraphTest,
    BuildConnectedRoomGraphPreservesDistinctStaircaseInstancesBetweenRoomPair) {
  // Two staircase objects in the same source room targeting the same
  // destination must each survive graph dedup with their own slot_index
  // / object_id provenance. Earlier the dedup keyed only on the
  // (minmax pair, type) tuple and silently collapsed both instances into
  // a single visible edge, hiding which slot was misconfigured.
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  start.SetStaircaseRoom(1, 0x40);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x139, 6, 5, 0, 0)).ok());
  start.SetLoaded(true);

  auto& target = rooms[0x40];
  ClearRoomLinks(&target);
  target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_EQ(graph.room_count, 2);
  ASSERT_EQ(graph.links.size(), 2u)
      << "Both staircase instances must be preserved in the graph; dedup "
         "should not collapse provenance-distinct edges";
  std::vector<int> seen_slots;
  std::vector<int16_t> seen_objects;
  for (const auto& link : graph.links) {
    EXPECT_EQ(link.type, DungeonConnectedLinkType::Staircase);
    EXPECT_EQ(link.from_room_id, 0x10);
    EXPECT_EQ(link.to_room_id, 0x40);
    seen_slots.push_back(link.slot_index);
    seen_objects.push_back(link.object_id);
  }
  std::sort(seen_slots.begin(), seen_slots.end());
  std::sort(seen_objects.begin(), seen_objects.end());
  EXPECT_EQ(seen_slots, (std::vector<int>{0, 1}));
  EXPECT_EQ(seen_objects, (std::vector<int16_t>{0x138, 0x139}));
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphScopesViaProjectOpenWithoutHackManifest) {
  // Real Oracle .yaze projects ship a project_registry without a
  // hack_manifest_v1.json (mirrors ProjectPathsTest::
  // OpenInjectsOracleDungeonRoomLabelsIntoProjectFile). Connected-mode
  // scoping must work via YazeProject::Open() in that configuration —
  // earlier code gated registry lookups on hack_manifest.loaded() and
  // silently fell back to full transitive BFS for the real Oracle path.
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("yaze_connected_scope_real_open_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  const std::filesystem::path planning = root / "Docs" / "Dev" / "Planning";
  std::filesystem::create_directories(planning);
  std::ofstream(planning / "dungeons.json") << R"json({
    "dungeons": [
      {
        "id": "D1",
        "name": "Real Dungeon",
        "rooms": [
          {"id": "0x10", "name": "Start"},
          {"id": "0x11", "name": "Adjacent"}
        ]
      }
    ]
  })json";

  const std::filesystem::path project_file = root / "Real.yaze";
  std::ofstream(project_file) << R"([project]
name=Real Oracle Project

[files]
code_folder=Core
hack_manifest_file=hack_manifest.json
)";

  project::YazeProject project;
  ASSERT_TRUE(project.Open(project_file.string()).ok());
  ASSERT_FALSE(project.hack_manifest.loaded());
  ASSERT_TRUE(project.hack_manifest.HasProjectRegistry());

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.AddDoor(
      MakeDoor(zelda3::DoorDirection::East, zelda3::DoorType::NormalDoor));
  start.SetLoaded(true);

  auto& adjacent = rooms[0x11];
  ClearRoomLinks(&adjacent);
  adjacent.AddDoor(
      MakeDoor(zelda3::DoorDirection::West, zelda3::DoorType::NormalDoor));
  adjacent.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  viewer.SetProject(&project);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_TRUE(graph.dungeon_scope_active)
      << "Scoping must activate from the project_registry alone, even when "
         "hack_manifest.loaded() is false";
  EXPECT_EQ(graph.room_count, 2);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_TRUE(graph.room_mask[0x11]);

  std::filesystem::remove_all(root);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     ShouldShowConnectedSidePanelAppliesHysteresisAroundMinWidth) {
  constexpr float kMin = dungeon_ui::kConnectedSidePanelApproxMinWidth;
  constexpr float kHyst = dungeon_ui::kConnectedSidePanelHideHysteresis;

  // From hidden: only reveal at or above the full min-threshold.
  EXPECT_FALSE(dungeon_ui::ShouldShowConnectedSidePanel(kMin - 0.001f, false));
  EXPECT_TRUE(dungeon_ui::ShouldShowConnectedSidePanel(kMin, false));

  // From visible: stay visible through the hysteresis band below min.
  EXPECT_TRUE(dungeon_ui::ShouldShowConnectedSidePanel(kMin - 0.001f, true));
  EXPECT_TRUE(dungeon_ui::ShouldShowConnectedSidePanel(kMin - kHyst, true));
  // Collapse only after crossing below (min - hysteresis).
  EXPECT_FALSE(
      dungeon_ui::ShouldShowConnectedSidePanel(kMin - kHyst - 0.001f, true));
}

class DungeonCanvasRoomInformationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    old_labels_ = zelda3::GetResourceLabels();
    imgui_ = std::make_unique<ScopedImGuiContext>();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    for (int id : {0, 1}) {
      rooms_[id].SetLoaded(true);
      rooms_[id].LoadChests();
      rooms_[id].SetTag1(zelda3::Nothing);
      rooms_[id].SetTag2(zelda3::Nothing);
      rooms_[id].ClearSaveDirtyState();
    }
    viewer_.RefreshRomBackedState(&rom_, nullptr, &rooms_, 0);
    viewer_.SetHeaderVisible(false);
    viewer_.SetHeaderHiddenMetadataHudVisible(false);
    viewer_.SetProject(&project_);
    ASSERT_TRUE(project_.hack_manifest
                    .LoadFromString(R"json({
      "version":1,"hack_name":"Oracle of Secrets",
      "room_tags":{"tags":[{"tag_id":"0x34","address":"0x01CC08",
      "name":"Crumble Floor","source":"Dungeons/crumblefloor_tag.asm"},
      {"tag_id":"0x37","name":"RoomTag_ShutterDoorRequiresCart",
      "enabled":false,"feature_flag":"!ENABLE_MINECART_CART_SHUTTERS"}]}
    })json")
                    .ok());
  }

  void TearDown() override {
    zelda3::GetResourceLabels() = old_labels_;
    imgui_.reset();
  }

  std::string HudFrame(int room_id, float width = 640.0f,
                       bool expected_hud = false) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width + 40, 500), ImGuiCond_Always);
    ImGui::Begin("RoomInformationHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    const auto canvas_bounds =
        DungeonCanvasViewerTestPeer::DrawMetadataHud(viewer_, room_id, width);
    const auto text =
        std::string(ImGui::GetCurrentContext()->LogBuffer.c_str());
    ImGui::LogFinish();
    const auto hud_id = absl::StrFormat("##MetadataHUD_%p", &viewer_);
    auto* hud = ImGui::FindWindowByName(hud_id.c_str());
    EXPECT_EQ(hud && hud->Active, expected_hud);
    if (hud && hud->Active) {
      EXPECT_EQ(hud->Flags & ImGuiWindowFlags_NoInputs,
                ImGuiWindowFlags_NoInputs);
      EXPECT_LE(hud->Pos.x + hud->Size.x, canvas_bounds.Max.x);
      EXPECT_LE(hud->DC.CursorMaxPos.x, hud->WorkRect.Max.x + 1.0f);
    }
    ImGui::End();
    ImGui::Render();
    return text;
  }

  static std::vector<float> GlyphUvs(const ImDrawList& draw_list, int first,
                                     int last) {
    std::vector<float> result;
    const auto white = ImGui::GetFontTexUvWhitePixel();
    for (int i = first; i < last; ++i) {
      const auto uv = draw_list.VtxBuffer[i].uv;
      if (uv.x != white.x || uv.y != white.y) {
        result.push_back(uv.x);
        result.push_back(uv.y);
      }
    }
    return result;
  }

  std::vector<ImDrawVert> BadgeFrame(const zelda3::Room& room,
                                     const std::string& expected_text,
                                     float scale = 1.0f,
                                     ImVec2 scroll = ImVec2(0, 0)) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(850, 700), ImGuiCond_Always);
    ImGui::Begin("ChestBadgeHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    gui::CanvasRuntime rt;
    rt.draw_list = ImGui::GetWindowDrawList();
    rt.canvas_p0 = ImVec2(40, 60);
    rt.canvas_sz = ImVec2(700, 560);
    rt.scale = scale;
    rt.scrolling = scroll;
    auto& draw_list = *rt.draw_list;
    // Keep line geometry on the white atlas pixel so GlyphUvs isolates actual
    // font glyphs. Production badge rendering retains its normal draw flags.
    draw_list.Flags &= ~ImDrawListFlags_AntiAliasedLinesUseTex;
    const auto last_item = ImGui::GetItemID();
    const int first = draw_list.VtxBuffer.Size;
    DungeonCanvasViewerTestPeer::RenderChestContents(viewer_, rt, room);
    const int last = draw_list.VtxBuffer.Size;
    EXPECT_EQ(ImGui::GetItemID(), last_item);
    const auto actual_uvs = GlyphUvs(draw_list, first, last);
    std::vector<ImDrawVert> result(draw_list.VtxBuffer.begin() + first,
                                   draw_list.VtxBuffer.begin() + last);
    // Compare the font-atlas coordinates actually submitted for the badge with
    // the expected receipt text. This detects wrong project/room/reward names
    // without relying on a GPU or adding a text-only production test seam.
    const int expected_first = draw_list.VtxBuffer.Size;
    draw_list.AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(40, 60),
                      IM_COL32_WHITE, expected_text.c_str(), nullptr, 180.0f);
    EXPECT_EQ(actual_uvs,
              GlyphUvs(draw_list, expected_first, draw_list.VtxBuffer.Size));
    ImGui::End();
    ImGui::Render();
    return result;
  }

  void AddChest(int room_id, int object_id, uint8_t item_id, int layer = 0) {
    rooms_[room_id].GetTileObjects().emplace_back(object_id, 20, 24, 0, layer);
    rooms_[room_id].GetChests().push_back({item_id, object_id == 0xFB1});
  }

  std::unique_ptr<ScopedImGuiContext> imgui_;
  Rom rom_;
  DungeonRoomStore rooms_{&rom_};
  DungeonCanvasViewer viewer_;
  project::YazeProject project_;
  zelda3::ResourceLabelProvider old_labels_;
};

TEST_F(DungeonCanvasRoomInformationTest,
       RoomTagsNeverCoverCanvasEvenWhenMetadataHudIsEnabled) {
  rooms_[0].SetTag1(static_cast<zelda3::TagKey>(0x34));
  rooms_[0].SetTag2(static_cast<zelda3::TagKey>(0x37));
  for (int frame = 0; frame < 3; ++frame) {
    EXPECT_TRUE(HudFrame(0).empty());
  }
  viewer_.SetHeaderHiddenMetadataHudVisible(true);
  const auto metadata = HudFrame(0, 260.0f, true);
  EXPECT_NE(metadata.find("B:"), std::string::npos);
  EXPECT_EQ(metadata.find("Tags:"), std::string::npos);
  EXPECT_EQ(metadata.find("Crumble Floor"), std::string::npos);
  EXPECT_EQ(metadata.find("RoomTag_ShutterDoorRequiresCart"),
            std::string::npos);
  viewer_.SetHeaderVisible(true);
  EXPECT_TRUE(HudFrame(0).empty());
  viewer_.SetHeaderVisible(false);
  viewer_.SetHeaderHiddenMetadataHudVisible(false);
  EXPECT_TRUE(HudFrame(0).empty());
}

TEST_F(DungeonCanvasRoomInformationTest,
       OptionalMetadataHudPreservesActiveControlOnAppearanceAndReappearance) {
  viewer_.SetHeaderHiddenMetadataHudVisible(true);
  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.AddMousePosEvent(-1000, -1000);
  std::array<char, 32> text{};
  ImGuiID control_id = 0;
  ImGuiWindow* host = nullptr;
  auto frame = [&](bool show_hud, bool focus_control = false) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(680, 520), ImGuiCond_Always);
    ImGui::Begin("TagHudFocusHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    host = ImGui::GetCurrentWindow();
    control_id = ImGui::GetID("Room note");
    if (focus_control) {
      ImGui::SetKeyboardFocusHere();
    }
    ImGui::InputText("Room note", text.data(), text.size());
    if (show_hud) {
      auto& context = *ImGui::GetCurrentContext();
      EXPECT_EQ(context.NavWindow, host);
      EXPECT_EQ(context.NavId, control_id);
      EXPECT_EQ(context.ActiveId, control_id);
      DungeonCanvasViewerTestPeer::DrawMetadataHud(viewer_, 0, 640.0f);
      EXPECT_EQ(context.NavWindow, host);
      EXPECT_EQ(context.NavId, control_id);
      EXPECT_EQ(context.ActiveId, control_id);
    }
    ImGui::End();
    ImGui::Render();
  };

  frame(false, true);
  frame(false);
  ASSERT_EQ(ImGui::GetCurrentContext()->NavWindow, host);
  ASSERT_EQ(ImGui::GetCurrentContext()->NavId, control_id);
  ASSERT_EQ(ImGui::GetCurrentContext()->ActiveId, control_id);
  frame(true);
  io.AddInputCharactersUTF8("a");
  frame(true);
  EXPECT_STREQ(text.data(), "a");
  frame(false);
  frame(false);
  frame(true);
  io.AddInputCharactersUTF8("b");
  frame(true);
  EXPECT_STREQ(text.data(), "ab");
}

TEST_F(DungeonCanvasRoomInformationTest,
       ChestBadgesUseCurrentReceiptsAndProjectNames) {
  AddChest(0, 0xF99, 0x24);
  project_.resource_labels["item_receipt"]["0x24"] = "Mine Key";
  EXPECT_FALSE(BadgeFrame(rooms_[0], "$24 Mine Key").empty());
  rooms_[0].GetChests()[0].id = 0x33;
  EXPECT_FALSE(BadgeFrame(rooms_[0], "$33 Map").empty());
  AddChest(1, 0xFB1, 0xF0);
  project_.resource_labels["item_receipt"]["240"] = "Custom Reward";
  EXPECT_FALSE(BadgeFrame(rooms_[1], "$F0 Custom Reward").empty());
  viewer_.SetProject(nullptr);
  EXPECT_FALSE(BadgeFrame(rooms_[1], "$F0 Unknown item F0").empty());
}

TEST_F(DungeonCanvasRoomInformationTest,
       ChestBadgesUseReceiptNamespaceAndViewerContext) {
  zelda3::ResourceLabelProvider::ProjectLabels foreign;
  foreign["item_receipt"]["37"] = "Wrong session";
  zelda3::GetResourceLabels().SetProjectLabels(&foreign);
  AddChest(0, 0xFB1, 0x25);
  project_.resource_labels["item"]["0x25"] = "Blue Potion";
  EXPECT_FALSE(BadgeFrame(rooms_[0], "$25 Compass").empty());
  project_.resource_labels["item_receipt"]["37"] = "Big chest key";
  EXPECT_FALSE(BadgeFrame(rooms_[0], "$25 Big chest key").empty());
  project::YazeProject second;
  second.resource_labels["item_receipt"]["0x25"] = "Second receipt";
  viewer_.SetProject(&second);
  EXPECT_FALSE(BadgeFrame(rooms_[0], "$25 Second receipt").empty());
  viewer_.SetProject(nullptr);
  EXPECT_FALSE(BadgeFrame(rooms_[0], "$25 Compass").empty());
  zelda3::GetResourceLabels().SetProjectLabels(nullptr);
}

TEST_F(DungeonCanvasRoomInformationTest,
       ChestBadgesRespectStreamOrderAndFixedOpenDecorations) {
  AddChest(0, 0xF99, 0x33, 1);
  AddChest(0, 0xFB1, 0x24, 0);
  rooms_[0].GetChests() = {{0x24, true}, {0x33, false}};
  rooms_[0].GetTileObjects().emplace_back(0xF9A, 8, 8, 0, 0);
  EXPECT_FALSE(BadgeFrame(rooms_[0], "$33 Map$24 Small Key").empty());
}

TEST_F(DungeonCanvasRoomInformationTest,
       ChestBadgesSkipAmbiguousMappingsAndHonorVisibility) {
  AddChest(0, 0xF99, 0x24);
  viewer_.SetChestContentsVisible(false);
  EXPECT_TRUE(BadgeFrame(rooms_[0], "").empty());
  viewer_.SetChestContentsVisible(true);
  rooms_[0].GetChests()[0].size = true;
  EXPECT_TRUE(BadgeFrame(rooms_[0], "").empty());
  rooms_[0].GetChests().clear();
  EXPECT_TRUE(BadgeFrame(rooms_[0], "").empty());
}

TEST_F(DungeonCanvasRoomInformationTest,
       ChestBadgeTracksZoomAndScrollAboveBigChestWithoutTakingInput) {
  AddChest(0, 0xFB1, 0x33);
  for (const float scale : {0.5f, 1.0f, 2.0f}) {
    const ImVec2 scroll(12, -7);
    const auto vertices = BadgeFrame(rooms_[0], "$33 Map", scale, scroll);
    ASSERT_FALSE(vertices.empty());
    float left = vertices.front().pos.x, right = left;
    float bottom = vertices.front().pos.y;
    for (const auto& vertex : vertices) {
      left = std::min(left, vertex.pos.x);
      right = std::max(right, vertex.pos.x);
      bottom = std::max(bottom, vertex.pos.y);
    }
    EXPECT_NEAR((left + right) / 2, 40 + scroll.x + (20 * 8 + 16) * scale,
                1.0f);
    EXPECT_LT(bottom, 60 + scroll.y + 24 * 8 * scale);
  }
}

}  // namespace yaze::editor
