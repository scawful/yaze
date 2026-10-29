#include "app/editor/overworld/entity/entity_workbench.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/overworld/canvas/overworld_canvas_renderer.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/registry/content_registry.h"
#include "app/gui/core/popup_id.h"
#include "core/features.h"
#include "core/project.h"
#include "core/rom_settings.h"
#include "fixtures/lyra/synthetic_fixture_data.h"
#include "framework/headless_editor_test.h"
#include "gtest/gtest.h"
#include "imgui/imgui_internal.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz/miniz.h"
#undef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "unique_temp_path.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

class OverworldConnectionWorkbenchTestPeer {
 public:
  static const OverworldConnectionDestination& DraftDestination(
      const OverworldEntityWorkbench& workbench) {
    return workbench.project_draft_.destination;
  }
};

class OverworldConnectionCanvasTestPeer {
 public:
  static bool Input(
      OverworldCanvasRenderer& renderer, const gui::CanvasRuntime& canvas,
      OverworldEntityWorkbench& workbench,
      std::optional<OverworldConnectionReference> hovered = std::nullopt) {
    return renderer.HandleProjectConnectionInput(canvas, workbench, hovered);
  }
  static bool Dragging(const OverworldCanvasRenderer& renderer) {
    return renderer.project_drag_before_.has_value();
  }
  static std::optional<OverworldConnectionView> Preview(
      const OverworldCanvasRenderer& renderer) {
    return renderer.project_drag_preview_;
  }
  static absl::Status CommitNative(OverworldCanvasRenderer& renderer,
                                   zelda3::GameEntity* target,
                                   const OverworldConnectionView& before) {
    return renderer.CommitNativeConnectionDrag(target, before);
  }
  static std::optional<OverworldConnectionReference> DrawProject(
      OverworldCanvasRenderer& renderer, const gui::CanvasRuntime& canvas) {
    return renderer.DrawProjectConnections(canvas);
  }
  static bool MapInteraction(OverworldCanvasRenderer& renderer,
                             const gui::CanvasRuntime& canvas) {
    return renderer.HandleMapInteraction(canvas);
  }
};

namespace {

class OverworldConnectionWorkbenchTest : public test::HeadlessEditorTest {
 protected:
  void SetUp() override {
    test::HeadlessEditorTest::SetUp();
    ImGui::GetIO().ConfigInputTrickleEventQueue = false;
    previous_addresses_ = core::RomSettings::Get().address_overrides();
    previous_flags_ = core::FeatureFlags::get();
    core::RomSettings::Get().ClearOverrides();
    core::FeatureFlags::get().overworld.kLoadCustomOverworld = true;
    directory_ = test::UniqueTempPath("yaze_connection_workbench");
    ASSERT_TRUE(std::filesystem::create_directories(directory_));

    // The embedded fixture is generated from zeros and contains no game art.
    // Supply synthetic decoded pixels rather than invoking a game GFX decoder.
    const auto fixture = test::lyra::FindFixture("legacy_160_input.bin");
    ASSERT_NE(fixture.data, nullptr);
    std::vector<uint8_t> image(fixture.image_size);
    mz_ulong length = static_cast<mz_ulong>(image.size());
    ASSERT_EQ(mz_uncompress(image.data(), &length, fixture.data,
                            static_cast<mz_ulong>(fixture.size)),
              MZ_OK);
    ASSERT_EQ(length, image.size());
    image[zelda3::OverworldCustomTileGFXGroupEnabled] = 1;
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromData(image).ok());
    zelda3::LoadOptions options;
    options.load_graphics = false;
    options.expand_rom = false;
    ASSERT_TRUE(zelda3::LoadGameData(*rom_, *game_data_, options).ok());
    game_data_->graphics_buffer.assign(0x100000, 0);
    project_.name = "Synthetic connection project";
    project_.filepath = (directory_ / "Project.yaze").string();
    project_.rom_filename = (directory_ / "synthetic.sfc").string();
    ASSERT_NO_FATAL_FAILURE(CreateEditor(editor_, project_, 101));

    auto state = Document().authoring();
    state.maps[0].doors = {{41, 1, 2, 0x25}, {42, 4, 5, std::nullopt}};
    state.maps[31].exits = {{51, 0x105, 31, 31, 78}};
    state.maps[2].sprites.push_back({61, 0x42, 8, 9, 2});
    state.flight_points.push_back(
        {71, 0x27, 10, 11, "Unrelated flight point", std::nullopt});
    ASSERT_TRUE(editor_->EditProjectMapAuthoring(std::move(state)).ok());
    baseline_ = Document().authoring();
    rom_before_ = rom_->vector();
  }

  void TearDown() override {
    workbench_.ClearProjectConnection();
    ContentRegistry::Context::SetEditorWindowContext("Overworld",
                                                     previous_context_);
    editor_.reset();
    core::RomSettings::Get().SetAddressOverrides(previous_addresses_);
    core::FeatureFlags::get() = previous_flags_;
    std::error_code ec;
    std::filesystem::remove_all(directory_, ec);
    test::HeadlessEditorTest::TearDown();
  }

  void CreateEditor(std::unique_ptr<OverworldEditor>& editor,
                    project::YazeProject& project, size_t session_id) {
    EditorDependencies dependencies;
    dependencies.renderer = renderer_.get();
    dependencies.session_id = session_id;
    dependencies.project = &project;
    editor = std::make_unique<OverworldEditor>(rom_.get(), dependencies);
    editor->SetGameData(game_data_.get());
    editor->Initialize();
    const auto loaded = editor->overworld().Load(rom_.get());
    ASSERT_TRUE(loaded.ok()) << loaded;
    auto* donor = editor->overworld().mutable_overworld_map(0x3A);
    ASSERT_NE(donor, nullptr);
    donor->SetParent(0x3A);
    donor->ClearInheritedAreaProperties();
    const core::LyraMapProperties defaults;
    for (int slot = 0; slot < 8; ++slot) {
      donor->set_custom_tileset(slot, defaults.graphics_sheets[slot]);
    }
    donor->set_main_palette(defaults.main_palette);
    donor->set_area_palette(defaults.palette_set);
    donor->set_animated_gfx(defaults.animated_sheet);
    const auto created =
        editor->lyra_canvas_.Create(&project, editor->overworld());
    ASSERT_TRUE(created.ok()) << created;
    ASSERT_NE(editor->lyra_canvas_.document(), nullptr);
    ASSERT_NE(editor->entity_mutation_service(), nullptr);
  }

  const core::LyraTerrainDocument& Document() const {
    return *editor_->lyra_canvas_.document();
  }

  std::optional<OverworldConnectionView> Select(
      OverworldConnectionReference reference = ProjectEntranceReference{41}) {
    const auto status = workbench_.SelectProjectConnection(*editor_, reference);
    EXPECT_TRUE(status.ok()) << status;
    return workbench_.SelectedProjectConnection(*editor_);
  }

  static ProjectConnectionEdit EditAt(
      int map, int x, int y, OverworldConnectionDestination destination) {
    return {map, (map % 8) * 512 + x * 16, (map % 64 / 8) * 512 + y * 16,
            std::move(destination)};
  }

  void ExpectProtectedData(const core::LyraAuthoringState& current) {
    EXPECT_EQ(current.maps[2], baseline_.maps[2]);
    EXPECT_EQ(current.flight_points, baseline_.flight_points);
    EXPECT_EQ(rom_->vector(), rom_before_);
  }

  bool CanvasInput(
      OverworldCanvasRenderer& renderer, ImVec2 mouse, bool down,
      std::optional<OverworldConnectionReference> hovered = std::nullopt,
      bool escape = false, float scale = 1.0f) {
    // HeadlessEditorTest owns an open frame; finish it before queuing input.
    ImGui::EndFrame();
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(mouse.x, mouse.y);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, down);
    io.AddKeyEvent(ImGuiKey_Escape, escape);
    ImGui::NewFrame();
    gui::CanvasRuntime canvas;
    canvas.hovered = true;
    canvas.scale = scale;
    return OverworldConnectionCanvasTestPeer::Input(renderer, canvas,
                                                    workbench_, hovered);
  }

  void PrepareProjectDrag() {
    editor_->SetCurrentMap(0xA0);
    editor_->SetEditingMode(EditingMode::MOUSE);
    ASSERT_TRUE(editor_->entities_visible());
  }

  std::filesystem::path directory_;
  project::YazeProject project_;
  std::unique_ptr<OverworldEditor> editor_;
  OverworldEntityWorkbench workbench_;
  core::LyraAuthoringState baseline_;
  std::vector<uint8_t> rom_before_;
  core::RomAddressOverrides previous_addresses_;
  core::FeatureFlags::Flags previous_flags_;
  Editor* previous_context_ =
      ContentRegistry::Context::editor_window_context("Overworld");
};

TEST_F(OverworldConnectionWorkbenchTest,
       EditCommitsThroughEditorSharedUndoRedoAndRetainsStableSelection) {
  auto selected = Select();
  ASSERT_TRUE(selected.has_value());
  ASSERT_TRUE(workbench_
                  .ApplyProjectConnectionEdit(
                      *editor_, *selected,
                      EditAt(0xBF, 29, 28, DungeonEntranceDestination{0xFE}))
                  .ok());
  const auto after = Document().authoring();
  ASSERT_EQ(after.maps[31].doors.size(), 1);
  EXPECT_EQ(after.maps[31].doors[0],
            (core::LyraDoorPlacement{41, 29, 28, 0xFE}));
  EXPECT_EQ(after.maps[0].doors.size(), 1);
  ExpectProtectedData(after);
  auto moved = workbench_.SelectedProjectConnection(*editor_);
  ASSERT_TRUE(moved.has_value());
  EXPECT_EQ(moved->physical_map_id, 0xBF);
  EXPECT_EQ(moved->reference, selected->reference);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  auto restored = workbench_.SelectedProjectConnection(*editor_);
  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->physical_map_id, 0xA0);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Document().authoring(), after);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       ReturnEditUsesRealHistoryAndRetainsUnassignedRowOnFreshParse) {
  auto selected = Select(ProjectReturnReference{51});
  ASSERT_TRUE(selected.has_value());
  ASSERT_TRUE(workbench_
                  .ApplyProjectConnectionEdit(
                      *editor_, *selected,
                      EditAt(0xA0, 30, 29,
                             DungeonReturnDestination{0x127, std::nullopt}))
                  .ok());
  const auto after = Document().authoring();
  EXPECT_EQ(after.maps[0].exits[0],
            (core::LyraExitPlacement{51, 0x127, 30, 29, std::nullopt}));
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Document().authoring(), after);
  auto fresh = core::LyraTerrainDocument::Parse(Document().Serialize(),
                                                Document().profile());
  ASSERT_TRUE(fresh.ok()) << fresh.status();
  EXPECT_EQ(fresh->authoring(), after);
  EXPECT_EQ(fresh->authoring().maps[0].exits[0].id, 51u);
  ExpectProtectedData(fresh->authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       InsertionAndDeletionEachCreateOneSharedUndoStep) {
  ASSERT_TRUE(
      workbench_.InsertProjectConnection(*editor_, "entrance", 0xBF, 4080, 4080)
          .ok());
  auto selected = workbench_.SelectedProjectConnection(*editor_);
  ASSERT_TRUE(selected.has_value());
  const auto inserted_reference = selected->reference;
  const auto after_insert = Document().authoring();
  EXPECT_TRUE(std::holds_alternative<std::monostate>(selected->destination));
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  EXPECT_FALSE(workbench_.SelectedProjectConnection(*editor_).has_value());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Document().authoring(), after_insert);
  selected = Select(inserted_reference);
  ASSERT_TRUE(selected.has_value());
  ASSERT_TRUE(workbench_.DeleteProjectConnection(*editor_, *selected).ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  EXPECT_FALSE(workbench_.SelectedProjectConnection(*editor_).has_value());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), after_insert);
  selected = Select(inserted_reference);
  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(std::get<core::LyraDoorPlacement>(selected->record).id,
            std::get<ProjectEntranceReference>(inserted_reference).id);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       StaleDraftAndChangedSelectionRefuseWithoutPushingUndo) {
  auto stale = Select();
  ASSERT_TRUE(stale.has_value());
  auto changed = baseline_;
  changed.maps[0].doors[0].entrance_id = 0x27;
  ASSERT_TRUE(editor_->EditProjectMapAuthoring(changed).ok());
  const auto edit = EditAt(0xBF, 29, 28, DungeonEntranceDestination{0xFE});
  EXPECT_TRUE(absl::IsFailedPrecondition(
      workbench_.ApplyProjectConnectionEdit(*editor_, *stale, edit)));
  EXPECT_TRUE(absl::IsFailedPrecondition(
      workbench_.DeleteProjectConnection(*editor_, *stale)));
  EXPECT_EQ(Document().authoring(), changed);
  auto other = Select(ProjectEntranceReference{42});
  ASSERT_TRUE(other.has_value());
  EXPECT_TRUE(absl::IsFailedPrecondition(
      workbench_.ApplyProjectConnectionEdit(*editor_, *stale, edit)));
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       InvalidDestinationAndCollisionRefuseBeforeHistoryOrNativeMutation) {
  auto selected = Select();
  ASSERT_TRUE(selected.has_value());
  EXPECT_TRUE(absl::IsFailedPrecondition(workbench_.ApplyProjectConnectionEdit(
      *editor_, *selected, EditAt(0xA0, 1, 2, WorldLinkDestination{0x183}))));
  EXPECT_TRUE(absl::IsInvalidArgument(workbench_.ApplyProjectConnectionEdit(
      *editor_, *selected,
      EditAt(0xA0, 4, 5, DungeonEntranceDestination{0x25}))));
  EXPECT_TRUE(absl::IsInvalidArgument(workbench_.InsertProjectConnection(
      *editor_, "entrance", 0xA0, 16, 2080)));
  EXPECT_TRUE(absl::IsInvalidArgument(
      workbench_.InsertProjectConnection(*editor_, "hole", 0xA0, 0, 2048)));
  EXPECT_EQ(Document().authoring(), baseline_);
  // Only the fixture's initial authoring transaction exists in shared history.
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(Document().authoring().maps[0].doors.empty());
  EXPECT_TRUE(Document().authoring().maps[31].exits.empty());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       EditorOwnershipRefusesAnotherSessionEvenWithSameProjectId) {
  auto selected = Select();
  ASSERT_TRUE(selected.has_value());
  auto project = project_;
  project.filepath = (directory_ / "Other.yaze").string();
  project.lyra_terrain_file.clear();
  std::unique_ptr<OverworldEditor> other;
  ASSERT_NO_FATAL_FAILURE(CreateEditor(other, project, 202));
  ASSERT_TRUE(other->EditProjectMapAuthoring(baseline_).ok());
  EXPECT_FALSE(workbench_.SelectedProjectConnection(*other).has_value());
  EXPECT_TRUE(absl::IsFailedPrecondition(workbench_.ApplyProjectConnectionEdit(
      *other, *selected, EditAt(0xBF, 1, 2, DungeonEntranceDestination{1}))));
  EXPECT_TRUE(absl::IsFailedPrecondition(
      workbench_.DeleteProjectConnection(*other, *selected)));
  EXPECT_EQ(other->lyra_canvas_.document()->authoring(), baseline_);
  EXPECT_EQ(Document().authoring(), baseline_);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       DocumentReplacementClearsSelectionAndRefusesOldDraft) {
  auto selected = Select();
  ASSERT_TRUE(selected.has_value());
  const auto* old_document = editor_->lyra_canvas_.document();
  // The existing history retains the old document, preventing pointer reuse.
  editor_->lyra_canvas_.Reset();
  auto replacement = project_;
  replacement.filepath = (directory_ / "Replacement.yaze").string();
  replacement.lyra_terrain_file.clear();
  ASSERT_TRUE(
      editor_->lyra_canvas_.Create(&replacement, editor_->overworld()).ok());
  ASSERT_NE(editor_->lyra_canvas_.document(), old_document);
  ASSERT_TRUE(editor_->EditProjectMapAuthoring(baseline_).ok());
  EXPECT_FALSE(workbench_.SelectedProjectConnection(*editor_).has_value());
  EXPECT_TRUE(absl::IsFailedPrecondition(workbench_.ApplyProjectConnectionEdit(
      *editor_, *selected, EditAt(0xBF, 1, 2, DungeonEntranceDestination{1}))));
  EXPECT_TRUE(absl::IsFailedPrecondition(
      workbench_.DeleteProjectConnection(*editor_, *selected)));
  EXPECT_EQ(Document().authoring(), baseline_);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       SelectionResolvesStableIdAfterUnrelatedVectorReorder) {
  auto selected = Select();
  ASSERT_TRUE(selected.has_value());
  auto reordered = baseline_;
  std::reverse(reordered.maps[0].doors.begin(), reordered.maps[0].doors.end());
  ASSERT_TRUE(editor_->EditProjectMapAuthoring(reordered).ok());
  auto current = workbench_.SelectedProjectConnection(*editor_);
  ASSERT_TRUE(current.has_value());
  EXPECT_EQ(current->reference, selected->reference);
  EXPECT_EQ(std::get<core::LyraDoorPlacement>(current->record).id, 41u);
  ASSERT_TRUE(workbench_
                  .ApplyProjectConnectionEdit(
                      *editor_, *current,
                      EditAt(0xA0, 2, 3, DungeonEntranceDestination{0x26}))
                  .ok());
  EXPECT_EQ(Document().authoring().maps[0].doors[0].id, 42u);
  EXPECT_EQ(Document().authoring().maps[0].doors[1],
            (core::LyraDoorPlacement{41, 2, 3, 0x26}));
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), reordered);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       CanvasDragPreviewsWhileHeldAndReleaseCommitsExactlyOneUndoStep) {
  PrepareProjectDrag();
  OverworldCanvasRenderer canvas(editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  EXPECT_TRUE(
      CanvasInput(canvas, {100, 100}, true, ProjectEntranceReference{41}));
  ASSERT_TRUE(OverworldConnectionCanvasTestPeer::Dragging(canvas));
  EXPECT_TRUE(CanvasInput(canvas, {612, 116}, true));
  auto preview = OverworldConnectionCanvasTestPeer::Preview(canvas);
  ASSERT_TRUE(preview.has_value());
  EXPECT_EQ(preview->physical_map_id, 0xA1);
  EXPECT_EQ(preview->world_x, 528);
  EXPECT_EQ(preview->world_y, 2096);
  EXPECT_EQ(Document().authoring(), baseline_);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
  ExpectProtectedData(Document().authoring());

  EXPECT_TRUE(CanvasInput(canvas, {612, 116}, false));
  EXPECT_FALSE(OverworldConnectionCanvasTestPeer::Dragging(canvas));
  EXPECT_FALSE(OverworldConnectionCanvasTestPeer::Preview(canvas).has_value());
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before + 1);
  const auto after = Document().authoring();
  ASSERT_EQ(after.maps[1].doors.size(), 1);
  EXPECT_EQ(after.maps[1].doors[0], (core::LyraDoorPlacement{41, 1, 3, 0x25}));
  auto selected = workbench_.SelectedProjectConnection(*editor_);
  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->reference,
            OverworldConnectionReference(ProjectEntranceReference{41}));
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Document().authoring(), after);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       CanvasPressThenMoveAndReleaseCommitsFinalTileWithOneUndoStep) {
  PrepareProjectDrag();
  OverworldCanvasRenderer canvas(editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  EXPECT_TRUE(
      CanvasInput(canvas, {100, 100}, true, ProjectEntranceReference{41}));
  EXPECT_EQ(Document().authoring(), baseline_);
  // The next frame delivers both movement and release; no held-move frame
  // precedes it. The final displacement still belongs to this gesture.
  EXPECT_TRUE(CanvasInput(canvas, {124, 132}, false));
  EXPECT_FALSE(OverworldConnectionCanvasTestPeer::Dragging(canvas));
  EXPECT_EQ(Document().authoring().maps[0].doors[0],
            (core::LyraDoorPlacement{41, 2, 4, 0x25}));
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before + 1);
  const auto after = Document().authoring();
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Document().authoring(), after);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       CanvasLastMovementOnReleaseIsIncludedAfterHeldPreview) {
  PrepareProjectDrag();
  OverworldCanvasRenderer canvas(editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  EXPECT_TRUE(
      CanvasInput(canvas, {100, 100}, true, ProjectEntranceReference{41}));
  EXPECT_TRUE(CanvasInput(canvas, {116, 116}, true));
  auto preview = OverworldConnectionCanvasTestPeer::Preview(canvas);
  ASSERT_TRUE(preview.has_value());
  EXPECT_EQ(preview->world_x, 32);
  EXPECT_EQ(preview->world_y, 2096);
  EXPECT_EQ(Document().authoring(), baseline_);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
  EXPECT_TRUE(CanvasInput(canvas, {132, 132}, false));
  EXPECT_EQ(Document().authoring().maps[0].doors[0],
            (core::LyraDoorPlacement{41, 3, 4, 0x25}));
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before + 1);
  EXPECT_FALSE(OverworldConnectionCanvasTestPeer::Dragging(canvas));
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       CanvasDragRespectsScaleAndRetainsReturnDestination) {
  PrepareProjectDrag();
  OverworldCanvasRenderer canvas(editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  EXPECT_TRUE(CanvasInput(canvas, {200, 200}, true, ProjectReturnReference{51},
                          false, 2.0f));
  EXPECT_TRUE(CanvasInput(canvas, {168, 168}, true, std::nullopt, false, 2.0f));
  EXPECT_EQ(Document().authoring(), baseline_);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
  EXPECT_TRUE(
      CanvasInput(canvas, {168, 168}, false, std::nullopt, false, 2.0f));
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before + 1);
  EXPECT_EQ(Document().authoring().maps[31].exits[0],
            (core::LyraExitPlacement{51, 0x105, 30, 30, 78}));
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
}

TEST_F(OverworldConnectionWorkbenchTest,
       CanvasEscapeCancelsPreviewWithoutCommittingOrChangingHistory) {
  PrepareProjectDrag();
  OverworldCanvasRenderer canvas(editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  EXPECT_TRUE(
      CanvasInput(canvas, {100, 100}, true, ProjectEntranceReference{41}));
  EXPECT_TRUE(CanvasInput(canvas, {612, 116}, true));
  ASSERT_TRUE(OverworldConnectionCanvasTestPeer::Dragging(canvas));
  EXPECT_TRUE(CanvasInput(canvas, {612, 116}, true, std::nullopt, true));
  EXPECT_FALSE(OverworldConnectionCanvasTestPeer::Dragging(canvas));
  EXPECT_FALSE(OverworldConnectionCanvasTestPeer::Preview(canvas).has_value());
  EXPECT_FALSE(CanvasInput(canvas, {612, 116}, false));
  EXPECT_EQ(Document().authoring(), baseline_);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       CanvasToolChangeCancelsHeldDragBeforeRelease) {
  PrepareProjectDrag();
  OverworldCanvasRenderer canvas(editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  EXPECT_TRUE(
      CanvasInput(canvas, {100, 100}, true, ProjectEntranceReference{41}));
  EXPECT_TRUE(CanvasInput(canvas, {612, 116}, true));
  editor_->SetEditingMode(EditingMode::DRAW_TILE);
  EXPECT_FALSE(CanvasInput(canvas, {612, 116}, true));
  EXPECT_FALSE(OverworldConnectionCanvasTestPeer::Dragging(canvas));
  editor_->SetEditingMode(EditingMode::MOUSE);
  EXPECT_FALSE(CanvasInput(canvas, {612, 116}, false));
  EXPECT_EQ(Document().authoring(), baseline_);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
}

TEST_F(OverworldConnectionWorkbenchTest,
       CanvasReleaseRefusesChangedPreimageAndPreservesConcurrentEdit) {
  PrepareProjectDrag();
  OverworldCanvasRenderer canvas(editor_.get());
  EXPECT_TRUE(
      CanvasInput(canvas, {100, 100}, true, ProjectEntranceReference{41}));
  EXPECT_TRUE(CanvasInput(canvas, {612, 116}, true));
  auto concurrent = baseline_;
  concurrent.maps[0].doors[0].entrance_id = 0x27;
  ASSERT_TRUE(editor_->EditProjectMapAuthoring(concurrent).ok());
  const auto history_before_release = editor_->undo_manager().UndoStackSize();
  EXPECT_TRUE(CanvasInput(canvas, {612, 116}, false));
  EXPECT_FALSE(OverworldConnectionCanvasTestPeer::Dragging(canvas));
  EXPECT_EQ(Document().authoring(), concurrent);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before_release);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Document().authoring(), baseline_);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       CanvasReleaseRefusesSelectionOwnedByAnotherEditor) {
  PrepareProjectDrag();
  OverworldCanvasRenderer canvas(editor_.get());
  EXPECT_TRUE(
      CanvasInput(canvas, {100, 100}, true, ProjectEntranceReference{41}));
  EXPECT_TRUE(CanvasInput(canvas, {612, 116}, true));
  auto project = project_;
  project.filepath = (directory_ / "DragOther.yaze").string();
  project.lyra_terrain_file.clear();
  std::unique_ptr<OverworldEditor> other;
  ASSERT_NO_FATAL_FAILURE(CreateEditor(other, project, 303));
  ASSERT_TRUE(other->EditProjectMapAuthoring(baseline_).ok());
  ASSERT_TRUE(
      workbench_.SelectProjectConnection(*other, ProjectEntranceReference{41})
          .ok());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  EXPECT_TRUE(CanvasInput(canvas, {612, 116}, false));
  EXPECT_EQ(Document().authoring(), baseline_);
  EXPECT_EQ(other->lyra_canvas_.document()->authoring(), baseline_);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
}

TEST_F(OverworldConnectionWorkbenchTest,
       NativeEntranceDragUsesExistingEditTransactionAndSharedUndo) {
  auto& native = editor_->overworld();
  native.mutable_entrances()->clear();
  zelda3::OverworldEntrance entrance;
  entrance.map_id_ = 0;
  entrance.x_ = 16;
  entrance.y_ = 32;
  entrance.entrance_id_ = 0x25;
  entrance.UpdateMapProperties(0, &native);
  native.mutable_entrances()->push_back(entrance);
  auto* target = &native.mutable_entrances()->at(0);
  auto before = ResolveOverworldConnection(
      {*native.mutable_entrances(), {}, {}, nullptr},
      NativeEntranceReference{NativeEntranceTable::kEntrances, 0});
  ASSERT_TRUE(before.ok()) << before.status();
  OverworldCanvasRenderer canvas(editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  target->x_ = 48;
  target->y_ = 64;
  ASSERT_TRUE(
      OverworldConnectionCanvasTestPeer::CommitNative(canvas, target, *before)
          .ok());
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before + 1);
  EXPECT_EQ(target->x_, 48);
  EXPECT_EQ(target->y_, 64);
  EXPECT_EQ(target->entrance_id_, 0x25);
  const auto new_map_pos = target->map_pos_;
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(target->x_, entrance.x_);
  EXPECT_EQ(target->y_, entrance.y_);
  EXPECT_EQ(target->map_pos_, entrance.map_pos_);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(target->x_, 48);
  EXPECT_EQ(target->y_, 64);
  EXPECT_EQ(target->map_pos_, new_map_pos);
  EXPECT_EQ(Document().authoring(), baseline_);
}

TEST_F(OverworldConnectionWorkbenchTest,
       NativeInvalidDragRestoresBeforeRecordWithoutPushingHistory) {
  auto& native = editor_->overworld();
  native.mutable_entrances()->clear();
  zelda3::OverworldEntrance entrance;
  entrance.map_id_ = 0;
  entrance.x_ = 16;
  entrance.y_ = 32;
  entrance.entrance_id_ = 0x25;
  entrance.UpdateMapProperties(0, &native);
  native.mutable_entrances()->push_back(entrance);
  auto* target = &native.mutable_entrances()->at(0);
  auto before = ResolveOverworldConnection(
      {*native.mutable_entrances(), {}, {}, nullptr},
      NativeEntranceReference{NativeEntranceTable::kEntrances, 0});
  ASSERT_TRUE(before.ok()) << before.status();
  OverworldCanvasRenderer canvas(editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  target->x_ = 17;
  target->y_ = 33;
  const auto status =
      OverworldConnectionCanvasTestPeer::CommitNative(canvas, target, *before);
  EXPECT_TRUE(absl::IsInvalidArgument(status));
  EXPECT_EQ(target->x_, entrance.x_);
  EXPECT_EQ(target->y_, entrance.y_);
  EXPECT_EQ(target->map_pos_, entrance.map_pos_);
  EXPECT_EQ(target->entrance_id_, entrance.entrance_id_);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
  EXPECT_EQ(Document().authoring(), baseline_);
  EXPECT_EQ(rom_->vector(), rom_before_);
}

TEST_F(OverworldConnectionWorkbenchTest,
       NativeReturnDragPreservesBeforePayloadThroughRealUndoRedo) {
  auto& native = editor_->overworld();
  native.mutable_exits()->clear();
  zelda3::OverworldExit exit;
  exit.map_id_ = 0;
  exit.room_id_ = 0x105;
  exit.x_ = exit.x_player_ = 17;
  exit.y_ = exit.y_player_ = 33;
  exit.x_scroll_ = 3;
  exit.y_scroll_ = 5;
  exit.x_camera_ = 0x91;
  exit.y_camera_ = 0xB3;
  exit.map_pos_ = 0x422;
  exit.scroll_mod_x_ = 7;
  exit.scroll_mod_y_ = 9;
  exit.door_type_1_ = 0x8123;
  exit.door_type_2_ = 0x4567;
  exit.is_automatic_ = false;
  native.mutable_exits()->push_back(exit);
  auto* target = &native.mutable_exits()->at(0);
  const auto fields = [](const zelda3::OverworldExit& row) {
    return std::make_tuple(
        row.room_id_, row.map_id_, row.x_, row.y_, row.x_player_, row.y_player_,
        row.x_scroll_, row.y_scroll_, row.x_camera_, row.y_camera_,
        row.map_pos_, row.scroll_mod_x_, row.scroll_mod_y_, row.door_type_1_,
        row.door_type_2_, row.is_automatic_);
  };
  const auto original = fields(exit);
  auto before = ResolveOverworldConnection(
      {{}, {}, *native.mutable_exits(), nullptr}, NativeReturnReference{0});
  ASSERT_TRUE(before.ok()) << before.status();
  OverworldCanvasRenderer canvas(editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  target->x_ += 16;
  target->y_ += 16;
  ASSERT_TRUE(
      OverworldConnectionCanvasTestPeer::CommitNative(canvas, target, *before)
          .ok());
  const auto after = fields(*target);
  EXPECT_NE(after, original);
  EXPECT_EQ(target->x_, 33);
  EXPECT_EQ(target->y_, 49);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before + 1);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(fields(*target), original);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(fields(*target), after);
  EXPECT_EQ(Document().authoring(), baseline_);
}

TEST_F(OverworldConnectionWorkbenchTest,
       NativeDragRefusesDifferentRowBeforeRestoringAnyPayload) {
  auto& rows = *editor_->overworld().mutable_entrances();
  rows.clear();
  zelda3::OverworldEntrance entrance;
  entrance.map_id_ = 0;
  entrance.x_ = 16;
  entrance.y_ = 32;
  entrance.entrance_id_ = 0x25;
  rows.push_back(entrance);
  entrance.x_ = 80;
  entrance.entrance_id_ = 0x27;
  rows.push_back(entrance);
  auto before = ResolveOverworldConnection(
      {rows, {}, {}, nullptr},
      NativeEntranceReference{NativeEntranceTable::kEntrances, 0});
  ASSERT_TRUE(before.ok()) << before.status();
  OverworldCanvasRenderer canvas(editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  EXPECT_FALSE(
      OverworldConnectionCanvasTestPeer::CommitNative(canvas, &rows[1], *before)
          .ok());
  EXPECT_EQ(rows[0].x_, 16);
  EXPECT_EQ(rows[0].entrance_id_, 0x25);
  EXPECT_EQ(rows[1].x_, 80);
  EXPECT_EQ(rows[1].entrance_id_, 0x27);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
}

TEST_F(OverworldConnectionWorkbenchTest,
       NativeDragRefusesRemovedRowBeforeDereferencingOldTarget) {
  auto& rows = *editor_->overworld().mutable_entrances();
  rows.clear();
  zelda3::OverworldEntrance entrance;
  entrance.map_id_ = 0;
  entrance.x_ = 16;
  entrance.y_ = 32;
  rows.push_back(entrance);
  auto before = ResolveOverworldConnection(
      {rows, {}, {}, nullptr},
      NativeEntranceReference{NativeEntranceTable::kEntrances, 0});
  ASSERT_TRUE(before.ok()) << before.status();
  zelda3::GameEntity* old_target = &rows[0];
  rows.clear();
  OverworldCanvasRenderer canvas(editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  EXPECT_FALSE(OverworldConnectionCanvasTestPeer::CommitNative(
                   canvas, old_target, *before)
                   .ok());
  EXPECT_TRUE(rows.empty());
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
}

TEST_F(OverworldConnectionWorkbenchTest,
       OrdinaryEntranceAndExitPopupIdsOpenAndCancelWithoutMutation) {
  ContentRegistry::Context::SetEditorWindowContext("Overworld", editor_.get());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  for (const auto& reference :
       {OverworldConnectionReference(ProjectEntranceReference{41}),
        OverworldConnectionReference(ProjectReturnReference{51})}) {
    const bool entrance =
        std::holds_alternative<ProjectEntranceReference>(reference);
    const auto id =
        gui::MakePopupId(gui::EditorNames::kOverworld,
                         entrance ? "Entrance Editor" : "Exit Editor");
    ASSERT_TRUE(
        workbench_.OpenProjectConnectionEditor(*editor_, reference).ok());
    const auto draw_popup = [&]() {
      ImGui::EndFrame();
      ImGui::NewFrame();
      ImGui::Begin("ConnectionPopupHost");
      workbench_.DrawPopups();
      const bool open = ImGui::IsPopupOpen(id.c_str());
      ImGui::End();
      return open;
    };
    ASSERT_TRUE(draw_popup());
    ASSERT_TRUE(draw_popup());
    auto* popup = ImGui::FindWindowByName(id.c_str());
    ASSERT_NE(popup, nullptr);
    EXPECT_EQ(std::string(popup->Name), id);
    EXPECT_NE(popup->Flags & ImGuiWindowFlags_Modal, 0);
    // Exercise the actual ImGui Cancel button's activation, as the existing
    // empty-state UI tests do, rather than clearing the Workbench manually.
    ImGui::ActivateItemByID(popup->GetID("Cancel"));
    EXPECT_FALSE(draw_popup());
    EXPECT_EQ(Document().authoring(), baseline_);
    EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
  }
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       ModalDraftCannotSaveIntoRetargetedEditorWithIdenticalIdAndPreimage) {
  auto project = project_;
  project.filepath = (directory_ / "ModalOther.yaze").string();
  project.lyra_terrain_file.clear();
  std::unique_ptr<OverworldEditor> other;
  ASSERT_NO_FATAL_FAILURE(CreateEditor(other, project, 404));
  ASSERT_TRUE(other->EditProjectMapAuthoring(baseline_).ok());
  const auto history_a = editor_->undo_manager().UndoStackSize();
  const auto history_b = other->undo_manager().UndoStackSize();
  const auto id =
      gui::MakePopupId(gui::EditorNames::kOverworld, "Entrance Editor");
  const auto draw_popup = [&]() {
    ImGui::EndFrame();
    ImGui::NewFrame();
    ImGui::Begin("ModalOwnershipHost");
    workbench_.DrawPopups();
    const bool open = ImGui::IsPopupOpen(id.c_str());
    ImGui::End();
    return open;
  };
  ContentRegistry::Context::SetEditorWindowContext("Overworld", editor_.get());
  ASSERT_TRUE(
      workbench_
          .OpenProjectConnectionEditor(*editor_, ProjectEntranceReference{41})
          .ok());
  ASSERT_TRUE(draw_popup());
  ASSERT_TRUE(draw_popup());
  auto* popup = ImGui::FindWindowByName(id.c_str());
  ASSERT_NE(popup, nullptr);
  ImGui::ActivateItemByID(popup->GetID("Assign entrance ID"));
  ASSERT_TRUE(draw_popup());
  ASSERT_TRUE(std::holds_alternative<std::monostate>(
      OverworldConnectionWorkbenchTestPeer::DraftDestination(workbench_)));
  EXPECT_EQ(Document().authoring(), baseline_);

  // The second editor has the same ID and complete record preimage. A stale
  // modal therefore needs document ownership in addition to record equality.
  ASSERT_TRUE(
      workbench_.SelectProjectConnection(*other, ProjectEntranceReference{41})
          .ok());
  ContentRegistry::Context::SetEditorWindowContext("Overworld", other.get());
  ImGui::ActivateItemByID(popup->GetID("Save"));
  EXPECT_FALSE(draw_popup());
  EXPECT_EQ(Document().authoring(), baseline_);
  EXPECT_EQ(other->lyra_canvas_.document()->authoring(), baseline_);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_a);
  EXPECT_EQ(other->undo_manager().UndoStackSize(), history_b);

  // An explicit new opening in the current editor creates its own draft.
  ASSERT_TRUE(
      workbench_
          .OpenProjectConnectionEditor(*other, ProjectEntranceReference{41})
          .ok());
  ASSERT_TRUE(draw_popup());
  ASSERT_TRUE(draw_popup());
  popup = ImGui::FindWindowByName(id.c_str());
  ASSERT_NE(popup, nullptr);
  ImGui::ActivateItemByID(popup->GetID("Assign entrance ID"));
  ASSERT_TRUE(draw_popup());
  ASSERT_TRUE(std::holds_alternative<std::monostate>(
      OverworldConnectionWorkbenchTestPeer::DraftDestination(workbench_)));
  ImGui::ActivateItemByID(popup->GetID("Save"));
  EXPECT_FALSE(draw_popup());
  EXPECT_EQ(
      other->lyra_canvas_.document()->authoring().maps[0].doors[0].entrance_id,
      std::nullopt);
  EXPECT_EQ(other->undo_manager().UndoStackSize(), history_b + 1);
  EXPECT_EQ(Document().authoring(), baseline_);
  ASSERT_TRUE(other->Undo().ok());
  EXPECT_EQ(other->lyra_canvas_.document()->authoring(), baseline_);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       ModalDraftCannotSaveIntoReplacementDocumentWithIdenticalRecord) {
  ContentRegistry::Context::SetEditorWindowContext("Overworld", editor_.get());
  const auto id =
      gui::MakePopupId(gui::EditorNames::kOverworld, "Entrance Editor");
  const auto draw_popup = [&]() {
    ImGui::EndFrame();
    ImGui::NewFrame();
    ImGui::Begin("ModalDocumentHost");
    workbench_.DrawPopups();
    const bool open = ImGui::IsPopupOpen(id.c_str());
    ImGui::End();
    return open;
  };
  ASSERT_TRUE(
      workbench_
          .OpenProjectConnectionEditor(*editor_, ProjectEntranceReference{41})
          .ok());
  ASSERT_TRUE(draw_popup());
  ASSERT_TRUE(draw_popup());
  auto* popup = ImGui::FindWindowByName(id.c_str());
  ASSERT_NE(popup, nullptr);
  ImGui::ActivateItemByID(popup->GetID("Assign entrance ID"));
  ASSERT_TRUE(draw_popup());
  ASSERT_TRUE(std::holds_alternative<std::monostate>(
      OverworldConnectionWorkbenchTestPeer::DraftDestination(workbench_)));
  const auto* old_document = editor_->lyra_canvas_.document();
  editor_->lyra_canvas_.Reset();
  auto replacement = project_;
  replacement.filepath = (directory_ / "ModalReplacement.yaze").string();
  replacement.lyra_terrain_file.clear();
  ASSERT_TRUE(
      editor_->lyra_canvas_.Create(&replacement, editor_->overworld()).ok());
  ASSERT_NE(editor_->lyra_canvas_.document(), old_document);
  ASSERT_TRUE(editor_->EditProjectMapAuthoring(baseline_).ok());
  ASSERT_TRUE(
      workbench_.SelectProjectConnection(*editor_, ProjectEntranceReference{41})
          .ok());
  const auto history_before = editor_->undo_manager().UndoStackSize();
  ImGui::ActivateItemByID(popup->GetID("Save"));
  EXPECT_FALSE(draw_popup());
  EXPECT_EQ(Document().authoring(), baseline_);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
  ExpectProtectedData(Document().authoring());
}

TEST_F(OverworldConnectionWorkbenchTest,
       CanvasExitsModeSelectsReturnOverOverlappingEntrance) {
  auto overlapping = baseline_;
  overlapping.maps[0].exits.push_back({52, 0x127, 1, 2, std::nullopt});
  ASSERT_TRUE(editor_->EditProjectMapAuthoring(overlapping).ok());
  PrepareProjectDrag();
  editor_->SetEntityEditMode(EntityEditMode::EXITS);
  ContentRegistry::Context::SetEditorWindowContext("Overworld", editor_.get());
  OverworldCanvasRenderer renderer(editor_.get());
  ImGui::EndFrame();
  ImGui::GetIO().AddMousePosEvent(24, 2088);
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  ImGui::NewFrame();
  ImGui::Begin("ConnectionOverlapHost");
  gui::CanvasRuntime canvas;
  canvas.hovered = true;
  canvas.scale = 1.0f;
  canvas.draw_list = ImGui::GetWindowDrawList();
  auto hovered =
      OverworldConnectionCanvasTestPeer::DrawProject(renderer, canvas);
  EXPECT_TRUE(hovered.has_value());
  if (hovered)
    EXPECT_EQ(*hovered,
              OverworldConnectionReference(ProjectReturnReference{52}));
  EXPECT_FALSE(
      OverworldConnectionCanvasTestPeer::MapInteraction(renderer, canvas));
  EXPECT_TRUE(OverworldConnectionCanvasTestPeer::Input(renderer, canvas,
                                                       workbench_, hovered));
  ImGui::End();
  auto selected = workbench_.SelectedProjectConnection(*editor_);
  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->reference,
            OverworldConnectionReference(ProjectReturnReference{52}));
  EXPECT_EQ(Document().authoring(), overlapping);

  // Escape clears the left gesture before the right-click context request.
  EXPECT_TRUE(CanvasInput(renderer, {24, 2088}, false, std::nullopt, true));
  ImGui::EndFrame();
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  ImGui::NewFrame();
  ImGui::Begin("ConnectionOverlapHost");
  canvas.draw_list = ImGui::GetWindowDrawList();
  hovered = OverworldConnectionCanvasTestPeer::DrawProject(renderer, canvas);
  EXPECT_TRUE(hovered.has_value());
  EXPECT_FALSE(
      OverworldConnectionCanvasTestPeer::MapInteraction(renderer, canvas));
  EXPECT_TRUE(OverworldConnectionCanvasTestPeer::Input(renderer, canvas,
                                                       workbench_, hovered));
  workbench_.DrawPopups();
  EXPECT_TRUE(ImGui::IsPopupOpen("##OverworldEntityContextMenu"));
  ImGui::End();
  selected = workbench_.SelectedProjectConnection(*editor_);
  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->reference,
            OverworldConnectionReference(ProjectReturnReference{52}));
  EXPECT_EQ(Document().authoring(), overlapping);
}

TEST_F(OverworldConnectionWorkbenchTest,
       ProjectDoubleClickOpensOrdinaryReturnPopupWithoutMapNavigation) {
  auto overlapping = baseline_;
  overlapping.maps[0].exits.push_back({52, 0x127, 1, 2, std::nullopt});
  ASSERT_TRUE(editor_->EditProjectMapAuthoring(overlapping).ok());
  PrepareProjectDrag();
  editor_->SetEntityEditMode(EntityEditMode::EXITS);
  ContentRegistry::Context::SetEditorWindowContext("Overworld", editor_.get());
  OverworldCanvasRenderer renderer(editor_.get());
  const auto id = gui::MakePopupId(gui::EditorNames::kOverworld, "Exit Editor");
  const auto history_before = editor_->undo_manager().UndoStackSize();
  const auto frame = [&](bool down) {
    ImGui::EndFrame();
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(24, 2088);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, down);
    ImGui::NewFrame();
    ImGui::Begin("ConnectionDoubleClickHost");
    gui::CanvasRuntime canvas;
    canvas.hovered = true;
    canvas.scale = 1.0f;
    canvas.draw_list = ImGui::GetWindowDrawList();
    const auto hovered =
        OverworldConnectionCanvasTestPeer::DrawProject(renderer, canvas);
    EXPECT_TRUE(hovered.has_value());
    EXPECT_FALSE(
        OverworldConnectionCanvasTestPeer::MapInteraction(renderer, canvas));
    const bool dragging_before =
        OverworldConnectionCanvasTestPeer::Dragging(renderer);
    EXPECT_EQ(OverworldConnectionCanvasTestPeer::Input(renderer, canvas,
                                                       workbench_, hovered),
              down || dragging_before);
    const bool double_click =
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    workbench_.DrawPopups();
    const bool open = ImGui::IsPopupOpen(id.c_str());
    ImGui::End();
    return std::make_pair(double_click, open);
  };
  EXPECT_FALSE(frame(true).first);
  EXPECT_FALSE(frame(false).second);
  const auto double_click = frame(true);
  EXPECT_TRUE(double_click.first);
  EXPECT_TRUE(double_click.second);
  EXPECT_FALSE(OverworldConnectionCanvasTestPeer::Dragging(renderer));
  const auto selected = workbench_.SelectedProjectConnection(*editor_);
  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->reference,
            OverworldConnectionReference(ProjectReturnReference{52}));
  EXPECT_EQ(Document().authoring(), overlapping);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history_before);
  auto* popup = ImGui::FindWindowByName(id.c_str());
  ASSERT_NE(popup, nullptr);
  ImGui::ActivateItemByID(popup->GetID("Cancel"));
  frame(false);
}

}  // namespace
}  // namespace yaze::editor
