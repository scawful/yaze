#include "app/editor/dungeon/dungeon_entrance_edit_policy.h"

#include <array>
#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/ui/window/dungeon_entrances_panel.h"
#include "app/gui/automation/widget_id_registry.h"
#include "core/features.h"
#include "dungeon_workbench_test_peer.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "rom/rom.h"

namespace yaze::editor {

// Exercise the same callback configuration as the production shared panel,
// while keeping all entrance models and history inside one real editor.
class DungeonEditorEntrancePropertiesTestPeer {
 public:
  static std::unique_ptr<DungeonEntrancesPanel> MakePanel(
      DungeonEditorV2& editor) {
    auto panel = std::make_unique<DungeonEntrancesPanel>(
        &editor.entrances_, &editor.spawn_points_, &editor.current_entrance_id_,
        [](int) {});
    editor.ConfigureEntrancePropertyEditCallbacks(panel.get());
    return panel;
  }

  static absl::Status LoadRecords(DungeonEditorV2& editor) {
    for (int id : {0, 1}) {
      editor.entrances_[zelda3::kNumDungeonSpawnPoints + id] =
          zelda3::RoomEntrance(editor.rom_, id, false);
      auto spawn = zelda3::DungeonSpawnPoint::Load(*editor.rom_, id);
      if (!spawn.ok()) {
        return spawn.status();
      }
      editor.spawn_points_[id] = *spawn;
    }
    editor.current_room_id_ = 0;
    editor.rooms_[0].SetLoaded(true);
    editor.rooms_[0].ClearSaveDirtyState();
    return absl::OkStatus();
  }

  static zelda3::RoomEntrance& Regular(DungeonEditorV2& editor, int id = 0) {
    return editor.entrances_[zelda3::kNumDungeonSpawnPoints + id];
  }
  static zelda3::DungeonSpawnPoint& Spawn(DungeonEditorV2& editor, int id = 0) {
    return editor.spawn_points_[id];
  }
  static bool LegacySpawnDirty(const DungeonEditorV2& editor, int id = 0) {
    return editor.entrances_[id].dirty();
  }
  static void Select(DungeonEditorV2& editor, int slot) {
    editor.current_entrance_id_ = slot;
    editor.automatic_entrance_preview_ = false;
  }
  static size_t UndoCount(const DungeonEditorV2& editor) {
    return editor.undo_manager_.UndoStackSize();
  }
  static size_t RedoCount(const DungeonEditorV2& editor) {
    return editor.undo_manager_.RedoStackSize();
  }
  static int* RoomId(DungeonEditorV2& editor) {
    return &editor.current_room_id_;
  }
  static DungeonRoomStore* Rooms(DungeonEditorV2& editor) {
    return &editor.rooms_;
  }
};

namespace {

TEST(DungeonEntranceEditPolicyTest, SpawnPropertyChangeCannotMarkRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::RoomEntrance spawn(&rom, 0, true);

  EXPECT_FALSE(CanEditDungeonEntrance(0, spawn));
  EXPECT_FALSE(MarkDungeonEntranceDirtyIfEditable(0, spawn, true));
  EXPECT_FALSE(spawn.dirty());
}

TEST(DungeonEntranceEditPolicyTest,
     SpawnSlotCannotMarkDefaultModelDirtyBeforeLoad) {
  zelda3::RoomEntrance default_model;

  EXPECT_FALSE(CanEditDungeonEntrance(0, default_model));
  EXPECT_FALSE(MarkDungeonEntranceDirtyIfEditable(0, default_model, true));
  EXPECT_FALSE(default_model.dirty());
}

TEST(DungeonEntranceEditPolicyTest, RegularPropertyChangeMarksRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::RoomEntrance entrance(&rom, 0, false);

  constexpr int kRegularSlot = zelda3::kNumDungeonSpawnPoints;
  EXPECT_TRUE(CanEditDungeonEntrance(kRegularSlot, entrance));
  EXPECT_FALSE(
      MarkDungeonEntranceDirtyIfEditable(kRegularSlot, entrance, false));
  EXPECT_FALSE(entrance.dirty());
  EXPECT_TRUE(MarkDungeonEntranceDirtyIfEditable(kRegularSlot, entrance, true));
  EXPECT_TRUE(entrance.dirty());
}

TEST(DungeonEntranceEditPolicyTest,
     LoadedSpawnPropertyChangeMarksMatchingRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  for (int slot = 0; slot < zelda3::kNumDungeonSpawnPoints; ++slot) {
    auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, slot);
    ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
    auto spawn = *spawn_or;

    EXPECT_TRUE(CanEditDungeonSpawnPoint(slot, spawn));
    EXPECT_FALSE(MarkDungeonSpawnPointDirtyIfEditable(slot, spawn, false));
    EXPECT_FALSE(spawn.dirty());
    EXPECT_TRUE(MarkDungeonSpawnPointDirtyIfEditable(slot, spawn, true));
    EXPECT_TRUE(spawn.dirty());
  }
}

TEST(DungeonEntranceEditPolicyTest,
     DedicatedSpawnSlotCannotMarkDefaultModelDirtyBeforeLoad) {
  zelda3::DungeonSpawnPoint default_model;

  EXPECT_FALSE(CanEditDungeonSpawnPoint(0, default_model));
  EXPECT_FALSE(MarkDungeonSpawnPointDirtyIfEditable(0, default_model, true));
  EXPECT_FALSE(default_model.dirty());
}

TEST(DungeonEntranceEditPolicyTest, SpawnSlotCannotMarkMismatchedRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, 1);
  ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
  auto spawn = *spawn_or;

  EXPECT_FALSE(CanEditDungeonSpawnPoint(0, spawn));
  EXPECT_FALSE(MarkDungeonSpawnPointDirtyIfEditable(0, spawn, true));
  EXPECT_FALSE(spawn.dirty());
}

TEST(DungeonEntranceEditPolicyTest,
     InvalidSpawnSlotsCannotMarkLoadedRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, 0);
  ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();

  for (const int invalid_slot : {-1, zelda3::kNumDungeonSpawnPoints}) {
    auto spawn = *spawn_or;
    EXPECT_FALSE(CanEditDungeonSpawnPoint(invalid_slot, spawn));
    EXPECT_FALSE(
        MarkDungeonSpawnPointDirtyIfEditable(invalid_slot, spawn, true));
    EXPECT_FALSE(spawn.dirty());
  }
}

TEST(DungeonEntranceEditPolicyTest,
     SharedEntranceNavigatorRendersNarrowAndSplitLayouts) {
  ImGuiContext* context = ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
  unsigned char* pixels = nullptr;
  int atlas_width = 0;
  int atlas_height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width,
                                           &atlas_height);

  std::array<zelda3::RoomEntrance, zelda3::kNumDungeonEntranceSlots> entrances;
  std::array<zelda3::DungeonSpawnPoint, zelda3::kNumDungeonSpawnPoints>
      spawn_points;
  int selected = 0;
  DungeonEntrancesPanel panel(&entrances, &spawn_points, &selected, nullptr);
  const auto derived = DeriveDungeonEntranceCamera(
      /*room_id=*/0, /*player_x=*/0x78, /*player_y=*/0x78);
  const DungeonEntranceCameraState camera_state = {
      .kind = DungeonEntranceRecordKind::kSpawnPoint,
      .room_id = 0,
      .player_x = 0x78,
      .player_y = 0x78,
      .camera_x = derived.camera_x,
      .camera_y = derived.camera_y,
      .trigger_x = derived.trigger_x,
      .trigger_y = derived.trigger_y,
      .boundaries = derived.boundaries,
      .quadrant = derived.quadrant,
  };
  panel.SetCameraTools(
      [camera_state](int) { return std::optional(camera_state); },
      [](int) { return absl::OkStatus(); });

  for (float scale : {1.0f, 1.5f}) {
    for (float width : {280.0f, 800.0f}) {
      SCOPED_TRACE(absl::StrFormat("width %.0f scale %.1f", width, scale));
      ImGui::GetIO().FontGlobalScale = scale;
      ImGui::GetIO().DisplaySize = ImVec2(width, 720.0f);
      ImGui::NewFrame();
      ImGui::SetNextWindowSize(ImVec2(width, 680.0f));
      ImGui::SetNextWindowFocus();
      ImGui::Begin("EntranceNavigatorTest");
      EXPECT_NO_FATAL_FAILURE(panel.Draw(nullptr));
      EXPECT_TRUE(panel.OwnsNavigationShortcutFocus());
      ImGui::End();
      EXPECT_EQ(ImGui::GetCurrentContext()->StyleVarStack.Size, 0);
      EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
      ImGui::EndFrame();
      ImGui::Render();
    }
  }
  ImGui::DestroyContext(context);
}

enum class EntrancePropertyHost { kWorkbench, kStandalone };

class DungeonEntrancePropertiesUiTest
    : public ::testing::TestWithParam<EntrancePropertyHost> {
 protected:
  using Peer = DungeonEditorEntrancePropertiesTestPeer;

  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1200, 2000);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigInputTrickleEventQueue = false;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    for (int id : {0, 1}) {
      zelda3::RoomEntrance regular(&rom_, id, false);
      regular.room_ = 0x10 + id;
      regular.x_position_ = 0x0080 + id * 0x0200;
      regular.y_position_ = 0x0290;
      regular.blockset_ = 4;
      ApplyDungeonEntranceCameraState(
          BuildRepairedDungeonEntranceCamera(
              CaptureDungeonEntranceCamera(regular)),
          &regular);
      ASSERT_TRUE(regular.Save(&rom_, id).ok());

      auto loaded = zelda3::DungeonSpawnPoint::Load(rom_, id);
      ASSERT_TRUE(loaded.ok()) << loaded.status();
      auto spawn = *loaded;
      spawn.room_id = 0x21 + id;
      spawn.x_coordinate = 0x0350 + id * 0x0200;
      spawn.y_coordinate = 0x0490;
      spawn.main_gfx = 3;
      ApplyDungeonSpawnCameraState(
          BuildRepairedDungeonEntranceCamera(CaptureDungeonSpawnCamera(spawn)),
          &spawn);
      ASSERT_TRUE(spawn.Save(&rom_, id).ok());
    }

    editor_ = std::make_unique<DungeonEditorV2>(&rom_);
    ASSERT_TRUE(Peer::LoadRecords(*editor_).ok());
    Peer::Select(*editor_, zelda3::kNumDungeonSpawnPoints);
    panel_ = Peer::MakePanel(*editor_);
    viewer_.RefreshRomBackedState(&rom_, nullptr, Peer::Rooms(*editor_), 0);
    viewer_.SetObjectInteractionEnabled(true);
    content_ = std::make_unique<DungeonWorkbenchContent>(
        nullptr, Peer::RoomId(*editor_), [](int) {},
        [](int, RoomSelectionIntent) {}, [](int) {}, []() {},
        [this]() { return &viewer_; }, [this](int) { return &viewer_; },
        [this]() -> const std::deque<int>& { return recent_; }, [](int) {},
        [](bool) {});
    content_->SetNavigationPanels(nullptr, panel_.get());

    auto& flags = core::FeatureFlags::get().dungeon;
    flags.kSaveObjects = false;
    flags.kSaveSprites = false;
    flags.kSaveRoomHeaders = false;
    flags.kSaveTorches = false;
    flags.kSavePits = false;
    flags.kSaveBlocks = false;
    flags.kSaveCollision = false;
    flags.kSaveWaterFillZones = false;
    flags.kSaveChests = false;
    flags.kSavePotItems = false;
    flags.kSavePalettes = false;
    flags.kSaveEntrances = true;
    DrawFrame();
    DrawFrame();
  }

  void TearDown() override {
    content_.reset();
    panel_.reset();
    editor_.reset();
    core::FeatureFlags::get().dungeon = saved_flags_;
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
    if (!temp_dir_.empty()) {
      std::error_code error;
      std::filesystem::remove_all(temp_dir_, error);
    }
  }

  void DrawFrame(const std::function<void(ImGuiIO&)>& events = {}) {
    if (events) {
      events(ImGui::GetIO());
    }
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1100, 1900), ImGuiCond_Always);
    ImGui::Begin("EntrancePropertyHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    if (GetParam() == EntrancePropertyHost::kWorkbench) {
      DungeonWorkbenchContentTestPeer::DrawRoomInspector(*content_, viewer_);
    } else {
      panel_->Draw(nullptr);
    }
    ImGui::End();
    ImGui::Render();
  }

  void Click(const char* label) {
    const auto normalized = gui::WidgetIdRegistry::NormalizeLabel(label);
    std::optional<gui::WidgetIdRegistry::WidgetInfo> widget;
    for (const auto& [path, info] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (info.label == normalized) {
        widget = info;
        break;
      }
    }
    ASSERT_TRUE(widget.has_value()) << label;
    ASSERT_TRUE(widget->visible) << label;
    ASSERT_TRUE(widget->enabled) << label;
    ASSERT_TRUE(widget->bounds.valid) << label;
    // InputHex groups include the +/- buttons; click inside the text box.
    const float x = widget->type == "input"
                        ? widget->bounds.min_x + 15
                        : (widget->bounds.min_x + widget->bounds.max_x) / 2;
    const float y = (widget->bounds.min_y + widget->bounds.max_y) / 2;
    DrawFrame([&](ImGuiIO& io) { io.AddMousePosEvent(x, y); });
    DrawFrame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, true); });
    DrawFrame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, false); });
  }

  void Type(const char* label, const char* value, bool commit = true) {
    ASSERT_NO_FATAL_FAILURE(Click(label));
    ASSERT_NE(ImGui::GetCurrentContext()->ActiveId, 0u) << label;
    DrawFrame([&](ImGuiIO& io) { io.AddInputCharactersUTF8(value); });
    if (commit) {
      Enter();
    }
  }

  void TypeCharacters(const char* label, const char* value,
                      bool commit = true) {
    ASSERT_NO_FATAL_FAILURE(Click(label));
    ASSERT_NE(ImGui::GetCurrentContext()->ActiveId, 0u) << label;
    for (const char* character = value; *character != '\0'; ++character) {
      const char event[] = {*character, '\0'};
      DrawFrame([&](ImGuiIO& io) { io.AddInputCharactersUTF8(event); });
    }
    if (commit) {
      Enter();
    }
  }

  void Enter() {
    DrawFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Enter, true); });
    DrawFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Enter, false); });
  }

  void Select(int slot) {
    Peer::Select(*editor_, slot);
    DrawFrame();
    DrawFrame();
  }

  void SaveToDisposableFile() {
    if (temp_dir_.empty()) {
      const auto nonce =
          std::chrono::steady_clock::now().time_since_epoch().count();
      temp_dir_ = std::filesystem::temp_directory_path() /
                  absl::StrFormat("yaze-entrance-properties-%lld",
                                  static_cast<long long>(nonce));
      ASSERT_TRUE(std::filesystem::create_directory(temp_dir_));
    }
    const auto status = editor_->SaveRoom(0);
    ASSERT_TRUE(status.ok()) << status;
    Rom::SaveSettings settings;
    settings.filename = (temp_dir_ / "entrance-copy.sfc").string();
    ASSERT_TRUE(rom_.SaveToFile(settings).ok());
  }

  void ExpectReopenedRoom(int expected, bool spawn) {
    Rom reopened;
    ASSERT_TRUE(
        reopened.LoadFromFile((temp_dir_ / "entrance-copy.sfc").string()).ok());
    if (spawn) {
      const auto record = zelda3::DungeonSpawnPoint::Load(reopened, 0);
      ASSERT_TRUE(record.ok()) << record.status();
      EXPECT_EQ(record->room_id, expected);
    } else {
      const zelda3::RoomEntrance record(&reopened, 0, false);
      EXPECT_EQ(record.room_, expected);
    }
  }

  void ExpectReopenedCamera(DungeonEntranceCameraState expected) {
    Rom reopened;
    ASSERT_TRUE(
        reopened.LoadFromFile((temp_dir_ / "entrance-copy.sfc").string()).ok());
    const zelda3::RoomEntrance record(&reopened, 0, false);
    expected.dirty = false;
    EXPECT_EQ(CaptureDungeonEntranceCamera(record), expected);
  }

  void CheckSaveUndoRedo(bool spawn) {
    Select(spawn ? 0 : zelda3::kNumDungeonSpawnPoints);
    const int before =
        spawn ? Peer::Spawn(*editor_).room_id : Peer::Regular(*editor_).room_;
    const auto dirty = [this, spawn]() {
      return spawn ? Peer::Spawn(*editor_).dirty()
                   : Peer::Regular(*editor_).dirty();
    };
    ASSERT_NO_FATAL_FAILURE(
        Type(spawn ? "SpawnRoomId" : "RegularRoomId", "002B"));
    ASSERT_NO_FATAL_FAILURE(SaveToDisposableFile());
    ASSERT_NO_FATAL_FAILURE(ExpectReopenedRoom(0x2B, spawn));
    EXPECT_FALSE(dirty());
    EXPECT_TRUE(editor_->CollectWriteRanges().empty());
    EXPECT_FALSE(editor_->HasPendingDungeonChanges());

    ASSERT_TRUE(editor_->Undo().ok());
    EXPECT_TRUE(dirty());
    EXPECT_FALSE(editor_->CollectWriteRanges().empty());
    EXPECT_TRUE(editor_->HasPendingDungeonChanges());
    ASSERT_NO_FATAL_FAILURE(SaveToDisposableFile());
    ASSERT_NO_FATAL_FAILURE(ExpectReopenedRoom(before, spawn));
    EXPECT_FALSE(dirty());

    ASSERT_TRUE(editor_->Redo().ok());
    EXPECT_TRUE(dirty());
    EXPECT_FALSE(editor_->CollectWriteRanges().empty());
    EXPECT_TRUE(editor_->HasPendingDungeonChanges());
    ASSERT_NO_FATAL_FAILURE(SaveToDisposableFile());
    ASSERT_NO_FATAL_FAILURE(ExpectReopenedRoom(0x2B, spawn));
    EXPECT_FALSE(dirty());
  }

  decltype(core::FeatureFlags::get().dungeon) saved_flags_ =
      core::FeatureFlags::get().dungeon;
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  Rom rom_;
  std::unique_ptr<DungeonEditorV2> editor_;
  std::unique_ptr<DungeonEntrancesPanel> panel_;
  DungeonCanvasViewer viewer_;
  std::deque<int> recent_;
  std::unique_ptr<DungeonWorkbenchContent> content_;
  std::filesystem::path temp_dir_;
};

TEST_P(DungeonEntrancePropertiesUiTest,
       RegularTypedPropertyIsOneUndoableDirtyTransition) {
  const int before = Peer::Regular(*editor_).room_;
  ASSERT_NO_FATAL_FAILURE(Type("RegularRoomId", "002B"));
  EXPECT_EQ(Peer::Regular(*editor_).room_, 0x2B);
  EXPECT_TRUE(Peer::Regular(*editor_).dirty());
  EXPECT_FALSE(Peer::Spawn(*editor_).dirty());
  EXPECT_EQ(Peer::UndoCount(*editor_), 1u);

  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Peer::Regular(*editor_).room_, before);
  EXPECT_FALSE(Peer::Regular(*editor_).dirty());
  EXPECT_EQ(Peer::UndoCount(*editor_), 0u);
  EXPECT_EQ(Peer::RedoCount(*editor_), 1u);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Peer::Regular(*editor_).room_, 0x2B);
  EXPECT_TRUE(Peer::Regular(*editor_).dirty());
}

TEST_P(DungeonEntrancePropertiesUiTest,
       SpawnTypedPropertyIsOneActionAndLeavesRegularModelsClean) {
  Select(0);
  const int before = Peer::Spawn(*editor_).room_id;
  ASSERT_NO_FATAL_FAILURE(Type("SpawnRoomId", "002B"));
  EXPECT_EQ(Peer::Spawn(*editor_).room_id, 0x2B);
  EXPECT_TRUE(Peer::Spawn(*editor_).dirty());
  EXPECT_FALSE(Peer::Regular(*editor_).dirty());
  EXPECT_FALSE(Peer::LegacySpawnDirty(*editor_));
  EXPECT_EQ(Peer::UndoCount(*editor_), 1u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Peer::Spawn(*editor_).room_id, before);
  EXPECT_FALSE(Peer::Spawn(*editor_).dirty());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Peer::Spawn(*editor_).room_id, 0x2B);
  EXPECT_TRUE(Peer::Spawn(*editor_).dirty());
  EXPECT_FALSE(Peer::LegacySpawnDirty(*editor_));
}

TEST_P(DungeonEntrancePropertiesUiTest,
       CharacterPerFrameTypingIsOneUndoableGestureForBothRecordKinds) {
  for (bool spawn : {false, true}) {
    SCOPED_TRACE(spawn ? "Spawn" : "Regular");
    Select(spawn ? 0 : zelda3::kNumDungeonSpawnPoints);
    const int before =
        spawn ? Peer::Spawn(*editor_).room_id : Peer::Regular(*editor_).room_;
    const auto undo_before = Peer::UndoCount(*editor_);
    const auto room_id = [&]() {
      return spawn ? Peer::Spawn(*editor_).room_id
                   : Peer::Regular(*editor_).room_;
    };
    ASSERT_NO_FATAL_FAILURE(
        TypeCharacters(spawn ? "SpawnRoomId" : "RegularRoomId", "002B"));
    EXPECT_EQ(room_id(), 0x2B);
    EXPECT_EQ(Peer::UndoCount(*editor_), undo_before + 1);
    ASSERT_TRUE(editor_->Undo().ok());
    EXPECT_EQ(room_id(), before);
    EXPECT_EQ(Peer::UndoCount(*editor_), undo_before);
    ASSERT_TRUE(editor_->Redo().ok());
    EXPECT_EQ(room_id(), 0x2B);
  }
}

TEST_P(DungeonEntrancePropertiesUiTest,
       CharacterPerFrameEditsToDifferentFieldsRemainTwoActions) {
  const auto before = CaptureDungeonEntranceCamera(Peer::Regular(*editor_));
  ASSERT_NO_FATAL_FAILURE(TypeCharacters("RegularRoomId", "002B"));
  ASSERT_NO_FATAL_FAILURE(TypeCharacters("RegularPlayerX", "0088"));
  EXPECT_EQ(Peer::UndoCount(*editor_), 2u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Peer::Regular(*editor_).room_, 0x2B);
  EXPECT_EQ(Peer::Regular(*editor_).x_position_, before.player_x);
  EXPECT_TRUE(Peer::Regular(*editor_).dirty());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(CaptureDungeonEntranceCamera(Peer::Regular(*editor_)), before);
}

TEST_P(DungeonEntrancePropertiesUiTest,
       RefocusingTheSameFieldStartsAnotherUndoGesture) {
  const int before = Peer::Regular(*editor_).room_;
  ASSERT_NO_FATAL_FAILURE(TypeCharacters("RegularRoomId", "002B"));
  ASSERT_NO_FATAL_FAILURE(TypeCharacters("RegularRoomId", "002C"));
  EXPECT_EQ(Peer::UndoCount(*editor_), 2u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Peer::Regular(*editor_).room_, 0x2B);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Peer::Regular(*editor_).room_, before);
  EXPECT_FALSE(Peer::Regular(*editor_).dirty());
}

TEST_P(DungeonEntrancePropertiesUiTest,
       SaveDuringActiveTypingStartsAnotherUndoGesture) {
  const int before = Peer::Regular(*editor_).room_;
  ASSERT_NO_FATAL_FAILURE(TypeCharacters("RegularRoomId", "002", false));
  ASSERT_EQ(Peer::Regular(*editor_).room_, 2);
  ASSERT_EQ(Peer::UndoCount(*editor_), 1u);
  ASSERT_NO_FATAL_FAILURE(SaveToDisposableFile());
  ASSERT_NO_FATAL_FAILURE(ExpectReopenedRoom(2, false));
  EXPECT_FALSE(Peer::Regular(*editor_).dirty());

  DrawFrame([](ImGuiIO& io) { io.AddInputCharactersUTF8("B"); });
  Enter();
  ASSERT_EQ(Peer::Regular(*editor_).room_, 0x2B);
  EXPECT_EQ(Peer::UndoCount(*editor_), 2u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Peer::Regular(*editor_).room_, 2);
  EXPECT_FALSE(Peer::Regular(*editor_).dirty());
  EXPECT_TRUE(editor_->CollectWriteRanges().empty());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Peer::Regular(*editor_).room_, before);
  EXPECT_TRUE(Peer::Regular(*editor_).dirty());
  EXPECT_FALSE(editor_->CollectWriteRanges().empty());
}

TEST_P(DungeonEntrancePropertiesUiTest,
       SavingCleanRecordDuringActiveTypingStillBreaksUndoMerging) {
  ASSERT_NO_FATAL_FAILURE(Type("RegularRoomId", "0011"));
  ASSERT_NO_FATAL_FAILURE(TypeCharacters("RegularRoomId", "0010", false));
  ASSERT_EQ(Peer::Regular(*editor_).room_, 0x10);
  ASSERT_EQ(Peer::UndoCount(*editor_), 2u);
  EXPECT_FALSE(Peer::Regular(*editor_).dirty());
  ASSERT_NO_FATAL_FAILURE(SaveToDisposableFile());
  ASSERT_NO_FATAL_FAILURE(ExpectReopenedRoom(0x10, false));

  DrawFrame([](ImGuiIO& io) { io.AddInputCharactersUTF8("1"); });
  Enter();
  ASSERT_EQ(Peer::Regular(*editor_).room_, 0x101);
  EXPECT_EQ(Peer::UndoCount(*editor_), 3u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Peer::Regular(*editor_).room_, 0x10);
  EXPECT_FALSE(Peer::Regular(*editor_).dirty());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Peer::Regular(*editor_).room_, 0x11);
  EXPECT_TRUE(Peer::Regular(*editor_).dirty());
}

TEST_P(DungeonEntrancePropertiesUiTest,
       CommittedEditUndoTargetsOriginalSlotAfterSelectionChanges) {
  const int other_before = Peer::Regular(*editor_, 1).room_;
  const int original_before = Peer::Regular(*editor_).room_;
  ASSERT_NO_FATAL_FAILURE(Type("RegularRoomId", "002B"));
  Select(zelda3::kNumDungeonSpawnPoints + 1);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Peer::Regular(*editor_).room_, original_before);
  EXPECT_EQ(Peer::Regular(*editor_, 1).room_, other_before);
  EXPECT_FALSE(Peer::Regular(*editor_, 1).dirty());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Peer::Regular(*editor_).room_, 0x2B);
  EXPECT_EQ(Peer::Regular(*editor_, 1).room_, other_before);
}

TEST_P(DungeonEntrancePropertiesUiTest,
       ActiveTextDoesNotCarryIntoAnotherSelectedSlot) {
  const int other_before = Peer::Regular(*editor_, 1).room_;
  ASSERT_NO_FATAL_FAILURE(Type("RegularRoomId", "002B", false));
  EXPECT_EQ(Peer::Regular(*editor_).room_, 0x2B);
  Select(zelda3::kNumDungeonSpawnPoints + 1);
  DrawFrame([](ImGuiIO& io) { io.AddInputCharactersUTF8("3"); });
  Enter();
  EXPECT_EQ(Peer::Regular(*editor_).room_, 0x2B);
  EXPECT_EQ(Peer::Regular(*editor_, 1).room_, other_before);
  EXPECT_FALSE(Peer::Regular(*editor_, 1).dirty());
  EXPECT_EQ(Peer::UndoCount(*editor_), 1u);
}

TEST_P(DungeonEntrancePropertiesUiTest,
       RegularUndoRedoAfterSaveRemainSaveableAndFreshlyReopen) {
  CheckSaveUndoRedo(false);
}

TEST_P(DungeonEntrancePropertiesUiTest,
       SpawnUndoRedoAfterSaveRemainSaveableAndFreshlyReopen) {
  CheckSaveUndoRedo(true);
}

TEST_P(DungeonEntrancePropertiesUiTest,
       AdvancedCameraEditIsOneUndoableDirtyTransition) {
  const auto before = CaptureDungeonEntranceCamera(Peer::Regular(*editor_));
  ASSERT_NO_FATAL_FAILURE(Click("AdvancedCameraData"));
  ASSERT_NO_FATAL_FAILURE(Type("RegularCameraTriggerX", "0070"));
  const auto edited = CaptureDungeonEntranceCamera(Peer::Regular(*editor_));
  EXPECT_EQ(edited.trigger_x, 0x70);
  EXPECT_TRUE(edited.dirty);
  EXPECT_EQ(Peer::UndoCount(*editor_), 1u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(CaptureDungeonEntranceCamera(Peer::Regular(*editor_)), before);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(CaptureDungeonEntranceCamera(Peer::Regular(*editor_)), edited);
}

TEST_P(DungeonEntrancePropertiesUiTest,
       PlayerEditThenCameraRepairUndoInTheirOriginalOrder) {
  const auto before = CaptureDungeonEntranceCamera(Peer::Regular(*editor_));
  ASSERT_NO_FATAL_FAILURE(Type("RegularPlayerX", "0088"));
  const auto moved = CaptureDungeonEntranceCamera(Peer::Regular(*editor_));
  ASSERT_EQ(moved.player_x, 0x88);
  ASSERT_EQ(Peer::UndoCount(*editor_), 1u);
  ASSERT_NO_FATAL_FAILURE(Click("PreviewCameraRepair"));
  DrawFrame();
  ASSERT_NO_FATAL_FAILURE(Click("ApplyCameraRepair"));
  const auto repaired = CaptureDungeonEntranceCamera(Peer::Regular(*editor_));
  EXPECT_NE(repaired.trigger_x, moved.trigger_x);
  EXPECT_EQ(Peer::UndoCount(*editor_), 2u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(CaptureDungeonEntranceCamera(Peer::Regular(*editor_)), moved);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(CaptureDungeonEntranceCamera(Peer::Regular(*editor_)), before);
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(CaptureDungeonEntranceCamera(Peer::Regular(*editor_)), repaired);
}

TEST_P(DungeonEntrancePropertiesUiTest,
       CameraRepairUndoAfterSaveRemainsSaveableAndFreshlyReopens) {
  ASSERT_NO_FATAL_FAILURE(Type("RegularPlayerX", "0088"));
  const auto moved = CaptureDungeonEntranceCamera(Peer::Regular(*editor_));
  ASSERT_NO_FATAL_FAILURE(Click("PreviewCameraRepair"));
  DrawFrame();
  ASSERT_NO_FATAL_FAILURE(Click("ApplyCameraRepair"));
  const auto repaired = CaptureDungeonEntranceCamera(Peer::Regular(*editor_));
  ASSERT_NE(repaired.trigger_x, moved.trigger_x);
  ASSERT_NO_FATAL_FAILURE(SaveToDisposableFile());
  ASSERT_NO_FATAL_FAILURE(ExpectReopenedCamera(repaired));
  EXPECT_FALSE(Peer::Regular(*editor_).dirty());

  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(Peer::Regular(*editor_).dirty());
  EXPECT_FALSE(editor_->CollectWriteRanges().empty());
  ASSERT_NO_FATAL_FAILURE(SaveToDisposableFile());
  ASSERT_NO_FATAL_FAILURE(ExpectReopenedCamera(moved));
  EXPECT_FALSE(Peer::Regular(*editor_).dirty());

  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_TRUE(Peer::Regular(*editor_).dirty());
  EXPECT_FALSE(editor_->CollectWriteRanges().empty());
  ASSERT_NO_FATAL_FAILURE(SaveToDisposableFile());
  ASSERT_NO_FATAL_FAILURE(ExpectReopenedCamera(repaired));
  EXPECT_FALSE(Peer::Regular(*editor_).dirty());
}

INSTANTIATE_TEST_SUITE_P(
    SharedEntranceHosts, DungeonEntrancePropertiesUiTest,
    ::testing::Values(EntrancePropertyHost::kWorkbench,
                      EntrancePropertyHost::kStandalone),
    [](const ::testing::TestParamInfo<EntrancePropertyHost>& info) {
      return info.param == EntrancePropertyHost::kWorkbench ? "Workbench"
                                                            : "Standalone";
    });

}  // namespace
}  // namespace yaze::editor
