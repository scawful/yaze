#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/editor_manager.h"
#include "app/editor/message/message_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "core/palette_catalog.h"
#include "core/source_artifact_publisher.h"
#include "editor_test_support.h"
#include "test_utils.h"
#include "testing.h"
#include "zelda3/resource_labels.h"

#include "imgui/imgui.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {
namespace {

struct ScopedImGuiContext {
  ImGuiContext* ctx = nullptr;
  ScopedImGuiContext() {
    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  ~ScopedImGuiContext() {
    if (ctx) {
      ImGui::DestroyContext(ctx);
    }
  }
};

struct ScopedTempDir {
  std::filesystem::path path;
  bool retain_on_success = false;
  explicit ScopedTempDir(std::filesystem::path p) : path(std::move(p)) {
    std::filesystem::create_directories(path);
  }
  ~ScopedTempDir() {
    if (retain_on_success && !::testing::Test::HasFailure() &&
        !::testing::Test::IsSkipped()) {
      ::testing::Test::RecordProperty("lyra_authoring_retained_root",
                                      path.string());
      return;
    }
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

std::filesystem::path MakeTempDir(const std::string& stem) {
  const auto nonce = static_cast<uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() /
         (stem + "_" + std::to_string(nonce));
}

void WriteRomFile(const std::filesystem::path& path,
                  const std::string& title = "YAZE TEST ROM") {
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  for (size_t i = 0; i < title.size() && (0x7FC0 + i) < rom_data.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  ASSERT_TRUE(out.good());
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open());
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

std::string ProjectFileContents(const std::string& name,
                                const std::filesystem::path& rom_path) {
  return "[project]\nname=" + name + "\n\n" +
         "[files]\nrom_filename=" + rom_path.string() + "\n" +
         "rom_backup_folder=backups\n" +
         "code_folder=\nassets_folder=\npatches_folder=\n" +
         "labels_filename=labels.txt\n" +
         "symbols_filename=symbols.txt\noutput_folder=build\n\n" +
         "[workspace]\nautosave_enabled=false\n" +
         "autosave_interval_secs=300\nbackup_on_save=false\n";
}

void WriteProjectFile(const std::filesystem::path& path,
                      const std::string& name,
                      const std::filesystem::path& rom_path) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out << ProjectFileContents(name, rom_path);
  ASSERT_TRUE(out.good());
}

ProjectFileEditorState MakeModifiedProjectDraft(ProjectFileEditor* editor,
                                                project::YazeProject* project,
                                                const std::string& contents) {
  ProjectFileEditorState state = editor->CaptureState();
  state.text = contents;
  state.initialized = true;
  state.modified = true;
  state.active = true;
  editor->RestoreState(state, project);
  return state;
}

class LyraActionCallbackEditor : public Editor {
 public:
  explicit LyraActionCallbackEditor(std::function<absl::Status()> callback)
      : callback_(std::move(callback)) {
    type_ = EditorType::kDungeon;
    active_ = true;
  }
  void Initialize() override {}
  absl::Status Load() override { return absl::OkStatus(); }
  absl::Status Save() override { return absl::OkStatus(); }
  absl::Status Update() override { return callback_(); }
  absl::Status Cut() override { return absl::OkStatus(); }
  absl::Status Copy() override { return absl::OkStatus(); }
  absl::Status Paste() override { return absl::OkStatus(); }
  absl::Status Undo() override { return absl::OkStatus(); }
  absl::Status Redo() override { return absl::OkStatus(); }
  absl::Status Find() override { return absl::OkStatus(); }

 private:
  std::function<absl::Status()> callback_;
};

TEST(EditorManagerProjectActionsTest,
     LyraTerrainActionsRequireSavedProjectAndLoadedRom) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir directory(MakeTempDir("yaze_lyra_project_gate"));
  const auto rom_path = directory.path / "game.sfc";
  const auto project_path = directory.path / "game.yaze";
  const auto source_path = directory.path / "terrain.json";
  auto expect_refused = [&](const char* reason) {
    const auto create = manager->CreateLyraTerrain();
    EXPECT_TRUE(absl::IsFailedPrecondition(create)) << create;
    EXPECT_NE(create.message().find(reason), absl::string_view::npos) << create;
    const auto open = manager->OpenLyraTerrainFromPath(source_path.string());
    EXPECT_TRUE(absl::IsFailedPrecondition(open)) << open;
    EXPECT_NE(open.message().find(reason), absl::string_view::npos) << open;
    EXPECT_TRUE(manager->GetCurrentProject()->lyra_terrain_file.empty());
    EXPECT_FALSE(manager->IsCurrentProjectDirty());
    EXPECT_FALSE(std::filesystem::exists(source_path));
  };

  ASSERT_OK(manager->OpenLyraTerrainFromPath(""));
  expect_refused("Save a Yaze project");

  WriteRomFile(rom_path);
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  ASSERT_FALSE(manager->GetCurrentProject()->project_opened());
  expect_refused("Save a Yaze project");
  EXPECT_FALSE(manager->GetCurrentRom()->dirty());
  manager->CloseRom();

  WriteProjectFile(project_path, "Saved Project", rom_path);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  ASSERT_TRUE(manager->GetCurrentProject()->project_opened());
  manager->GetCurrentRom()->Close();
  ASSERT_FALSE(manager->GetCurrentRom()->is_loaded());
  expect_refused("Load the project's ROM");
}

TEST(EditorManagerProjectActionsTest,
     CancelLyraTerrainOpenPreservesDirtyProjectReference) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir directory(MakeTempDir("yaze_lyra_open_cancel"));
  const auto rom_path = directory.path / "game.sfc";
  const auto project_path = directory.path / "game.yaze";
  WriteRomFile(rom_path);
  WriteProjectFile(project_path, "Cancel Project", rom_path);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  const auto descriptor_before = ReadFile(project_path);
  const auto session_id = manager->GetCurrentSessionId();
  const auto reference = (directory.path / "pending.json").string();
  manager->GetCurrentProject()->lyra_terrain_file = reference;
  manager->MarkCurrentProjectDirty();

  ASSERT_OK(manager->OpenLyraTerrainFromPath(""));
  EXPECT_EQ(manager->GetCurrentSessionId(), session_id);
  EXPECT_EQ(manager->GetCurrentProject()->lyra_terrain_file, reference);
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_FALSE(manager->GetCurrentRom()->dirty());
  EXPECT_EQ(ReadFile(project_path), descriptor_before);
}

TEST(EditorManagerProjectActionsTest,
     FailedLyraTerrainOpenPreservesProjectReferenceAndDirtyState) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir directory(MakeTempDir("yaze_lyra_open_failure"));
  const auto rom_path = directory.path / "game.sfc";
  const auto project_path = directory.path / "game.yaze";
  WriteRomFile(rom_path);
  WriteProjectFile(project_path, "Failed Open Project", rom_path);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  const auto descriptor_before = ReadFile(project_path);
  const auto reference = (directory.path / "original.json").string();
  manager->GetCurrentProject()->lyra_terrain_file = reference;
  // Invalid-extension validation needs no native assets. Mark the synthetic
  // session ready so this test reaches the manager's real failure propagation.
  auto* session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  session->game_data_loaded = true;
  session->editor_initialized[EditorTypeIndex(EditorType::kOverworld)] = true;
  session->editor_assets_loaded[EditorTypeIndex(EditorType::kOverworld)] = true;

  for (bool dirty : {false, true}) {
    SCOPED_TRACE(dirty);
    if (dirty)
      manager->MarkCurrentProjectDirty();
    const auto failed = manager->OpenLyraTerrainFromPath(
        (directory.path / "invalid.txt").string());
    EXPECT_TRUE(absl::IsInvalidArgument(failed)) << failed;
    EXPECT_EQ(manager->GetCurrentProject()->lyra_terrain_file, reference);
    EXPECT_EQ(manager->IsCurrentProjectDirty(), dirty);
    EXPECT_FALSE(manager->GetCurrentRom()->dirty());
    EXPECT_EQ(ReadFile(project_path), descriptor_before);
  }
}

TEST(EditorManagerProjectActionsTest,
     TransientSessionCannotCreateOrOpenAnotherOwnersLyraTerrain) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir directory(MakeTempDir("yaze_lyra_transient_owner"));
  const auto rom_a = directory.path / "a.sfc";
  const auto rom_b = directory.path / "b.sfc";
  const auto project_a = directory.path / "a.yaze";
  const auto project_b = directory.path / "b.yaze";
  const auto source = directory.path / "terrain.json";
  WriteRomFile(rom_a, "LYRA OWNER A");
  WriteRomFile(rom_b, "LYRA OWNER B");
  WriteProjectFile(project_a, "Owner A", rom_a);
  WriteProjectFile(project_b, "Owner B", rom_b);
  ASSERT_OK(manager->OpenRomOrProject(project_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(project_b.string()));
  manager->SwitchToSession(0);
  auto* session_a =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(0));
  auto* session_b =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(1));
  ASSERT_NE(session_a, nullptr);
  ASSERT_NE(session_b, nullptr);
  ASSERT_TRUE(
      manager->IsCurrentProjectContextOwnedBySession(session_a->session_id()));
  const auto descriptor_a = ReadFile(project_a);
  const auto descriptor_b = ReadFile(project_b);
  absl::Status create = absl::UnknownError("callback did not run");
  absl::Status open = absl::UnknownError("callback did not run");
  LyraActionCallbackEditor callback([&]() {
    EXPECT_EQ(manager->GetCurrentSessionId(), session_b->session_id());
    EXPECT_FALSE(manager->IsCurrentProjectContextOwnedBySession(
        session_b->session_id()));
    create = manager->CreateLyraTerrain();
    open = manager->OpenLyraTerrainFromPath(source.string());
    return absl::OkStatus();
  });
  session_b->editors.active_editors_.push_back(&callback);
  absl::Cleanup remove_callback = [&]() {
    auto& editors = session_b->editors.active_editors_;
    editors.erase(std::remove(editors.begin(), editors.end(), &callback),
                  editors.end());
  };
  auto& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280.0f, 720.0f);
  io.DeltaTime = 1.0f / 60.0f;
  ImGui::NewFrame();
  manager->session_coordinator()->UpdateSessions();
  ImGui::EndFrame();

  EXPECT_TRUE(absl::IsFailedPrecondition(create)) << create;
  EXPECT_TRUE(absl::IsFailedPrecondition(open)) << open;
  EXPECT_NE(create.message().find("project session"), absl::string_view::npos)
      << create;
  EXPECT_NE(open.message().find("project session"), absl::string_view::npos)
      << open;
  EXPECT_EQ(manager->GetCurrentSessionId(), session_a->session_id());
  EXPECT_TRUE(manager->GetCurrentProject()->lyra_terrain_file.empty());
  ASSERT_TRUE(session_b->project_context.has_value());
  EXPECT_TRUE(session_b->project_context->lyra_terrain_file.empty());
  EXPECT_FALSE(session_a->project_dirty);
  EXPECT_FALSE(session_b->project_dirty);
  EXPECT_FALSE(session_a->rom.dirty());
  EXPECT_FALSE(session_b->rom.dirty());
  EXPECT_EQ(ReadFile(project_a), descriptor_a);
  EXPECT_EQ(ReadFile(project_b), descriptor_b);
  EXPECT_FALSE(std::filesystem::exists(source));
}

TEST(EditorManagerProjectActionsTest,
     NormalSavePersistsProjectWorkAndRetainsExplicitRomWritePolicy) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir directory(MakeTempDir("yaze_normal_project_save"));
  const auto rom = directory.path / "private.sfc";
  const auto descriptor = directory.path / "private.yaze";
  WriteRomFile(rom);
  WriteProjectFile(descriptor, "Read-only ROM project", rom);
  project::YazeProject project;
  ASSERT_OK(project.Open(descriptor.string()));
  project.rom_metadata.write_policy = project::RomWritePolicy::kReadOnly;
  ASSERT_OK(project.Save());
  const auto rom_before = ReadFile(rom);
  ASSERT_OK(manager->OpenRomOrProject(descriptor.string()));
  const auto descriptor_before = ReadFile(descriptor);
  ASSERT_OK(manager->SaveCurrentSession());
  EXPECT_EQ(ReadFile(descriptor), descriptor_before);
  manager->GetCurrentProject()->metadata.author = "Project edit";
  manager->MarkCurrentProjectDirty();
  ASSERT_OK(manager->SaveCurrentSession());
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  ASSERT_OK(project.Open(descriptor.string()));
  EXPECT_EQ(project.metadata.author, "Project edit");
  EXPECT_EQ(manager->SaveRom().code(), absl::StatusCode::kPermissionDenied);
  EXPECT_EQ(ReadFile(rom), rom_before);
}

TEST(EditorManagerProjectActionsTest,
     NormalSaveRefusesUnpublishedMessageSourceWithoutWritingRom) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  const auto initial_flags = core::FeatureFlags::get();
  absl::Cleanup cleanup = [&]() {
    gfx::Arena::Get().ClearTextureQueue();
    manager.reset();
    gfx::PaletteManager::Get().ResetForTesting();
    gfx::Arena::Get().ClearTextureQueue();
    core::FeatureFlags::get() = initial_flags;
  };
  test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir directory(MakeTempDir("yaze_normal_message_source_save"));
  const auto rom = directory.path / "private.sfc";
  const auto descriptor = directory.path / "private.yaze";
  const auto canonical = directory.path / "messages.json";
  WriteRomFile(rom);
  WriteProjectFile(descriptor, "Message source project", rom);
  std::ofstream(canonical)
      << R"json({"format":"yaze-message-bundle","version":1,
    "counts":{"vanilla":0,"expanded":1},
    "messages":[{"id":0,"bank":"expanded","text":"Alpha"}]})json";
  std::ofstream(directory.path / "hack_manifest.json") << R"json({
    "manifest_version":3,"hack_name":"Oracle of Secrets",
    "messages":{"data_start":"0x2F8026","data_end":"0x2F80FF",
      "vanilla_count":1,
      "expanded_range":{"first":"0x18D","last":"0x18D","count":1},
      "source":{"format":"yaze-message-bundle","version":1,
        "canonical_bundle_path":"messages.json",
        "generated_asm_include_path":"expanded_messages.asm"}}})json";
  project::YazeProject project;
  ASSERT_OK(project.Open(descriptor.string()));
  project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  ASSERT_OK(project.Save());
  ASSERT_OK(manager->OpenRomOrProject(descriptor.string()));
  auto* message = manager->GetCurrentEditorSet()->GetEditorAs<MessageEditor>(
      EditorType::kMessage);
  ASSERT_NE(message, nullptr);
  ASSERT_OK(message->LoadCanonicalExpandedSource());
  ASSERT_TRUE(message->OpenMessageById(0x18D));
  EXPECT_FALSE(message->HasUnpublishedMessageSourceChanges());
  ASSERT_OK(manager->SaveCurrentSession());

  const auto descriptor_before = ReadFile(descriptor);
  const auto source_before = ReadFile(canonical);
  const auto rom_before = ReadFile(rom);
  message->UpdateCurrentMessageFromText("Alphb");
  ASSERT_TRUE(message->HasUnpublishedMessageSourceChanges());
  ASSERT_FALSE(manager->GetCurrentRom()->dirty());
  manager->GetCurrentProject()->metadata.author = "Pending project edit";
  manager->MarkCurrentProjectDirty();
  const auto status = manager->SaveCurrentSession();

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("Preview and Publish"),
            std::string::npos);
  auto* save_shortcut = manager->shortcut_manager()->FindShortcut("Save");
  ASSERT_NE(save_shortcut, nullptr);
  ASSERT_TRUE(save_shortcut->callback);
  save_shortcut->callback();
  const auto& notifications = manager->toast_manager()->GetHistory();
  ASSERT_FALSE(notifications.empty());
  EXPECT_EQ(notifications.front().type, ToastType::kError);
  EXPECT_NE(notifications.front().message.find("Preview and Publish"),
            std::string::npos);
  EXPECT_TRUE(message->HasUnpublishedMessageSourceChanges());
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_FALSE(manager->GetCurrentRom()->dirty());
  EXPECT_EQ(ReadFile(descriptor), descriptor_before);
  EXPECT_EQ(ReadFile(canonical), source_before);
  EXPECT_EQ(ReadFile(rom), rom_before);
  EXPECT_FALSE(
      std::filesystem::exists(directory.path / "expanded_messages.asm"));
}

TEST(EditorManagerProjectActionsTest,
     LyraTerrainAutomaticProjectSaveAsReopenAndAdvancedImport) {
  const auto immutable_rom =
      test::TestRomManager::GetRomPath(test::RomRole::kExpanded);
  if (immutable_rom.empty() || !std::filesystem::exists(immutable_rom)) {
    GTEST_SKIP() << test::TestRomManager::GetRomRoleHint(
        test::RomRole::kExpanded);
  }
  ASSERT_TRUE(std::filesystem::is_regular_file(immutable_rom));
  const auto immutable_hash =
      core::ComputeSourceArtifactSha256(ReadFile(immutable_rom));
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  ScopedTempDir directory(MakeTempDir("yaze_lyra_fresh_project"));
  const auto primary = directory.path / "primary";
  const auto relocated = directory.path / "relocated";
  const auto consumer = directory.path / "consumer";
  std::filesystem::create_directories(primary);
  std::filesystem::create_directories(relocated);
  std::filesystem::create_directories(consumer);
  const auto primary_rom = primary / "source.sfc";
  const auto consumer_rom = consumer / "source.sfc";
  const auto primary_project = primary / "fresh.yaze";
  const auto relocated_project = relocated / "saved-as.yaze";
  const auto consumer_project = consumer / "consumer.yaze";
  const auto terrain = primary / "fresh.lyra-terrain.json";
  const auto relocated_terrain = relocated / "saved-as.lyra-terrain.json";
  const auto replacement = consumer / "replacement.lyra-terrain.json";
  std::filesystem::copy_file(immutable_rom, primary_rom);
  std::filesystem::copy_file(immutable_rom, consumer_rom);
  auto write_project = [&](const std::filesystem::path& path,
                           const std::filesystem::path& rom) {
    WriteProjectFile(path, "Fresh Lyra Project", rom);
    project::YazeProject descriptor;
    ASSERT_OK(descriptor.Open(path.string()));
    descriptor.feature_flags.overworld.kLoadCustomOverworld = true;
    descriptor.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
    ASSERT_OK(descriptor.Save());
  };
  ASSERT_NO_FATAL_FAILURE(write_project(primary_project, primary_rom));
  ASSERT_NO_FATAL_FAILURE(write_project(consumer_project, consumer_rom));
  const auto manifest = primary / "hack_manifest.json";
  std::ofstream(manifest)
      << R"json({"manifest_version":2,"hack_name":"Oracle of Secrets"})json";
  project::YazeProject oracle;
  ASSERT_OK(oracle.Open(primary_project.string()));
  ASSERT_TRUE(oracle.IsOracleCompatible());
  ASSERT_OK(oracle.Save());
  const auto initial_descriptor = ReadFile(primary_project);
  const auto initial_flags = core::FeatureFlags::get();
  std::unique_ptr<EditorManager> manager;
  absl::Cleanup cleanup = [&]() {
    gfx::Arena::Get().ClearTextureQueue();
    manager.reset();
    gfx::PaletteManager::Get().ResetForTesting();
    gfx::Arena::Get().ClearTextureQueue();
    core::FeatureFlags::get() = initial_flags;
  };
  auto open_fresh_manager = [&](const std::filesystem::path& path) {
    gfx::Arena::Get().ClearTextureQueue();
    manager.reset();
    gfx::PaletteManager::Get().ResetForTesting();
    gfx::Arena::Get().ClearTextureQueue();
    manager = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*manager, renderer.get());
    manager->SetAssetLoadMode(AssetLoadMode::kLazy);
    ASSERT_OK(manager->OpenRomOrProject(path.string()));
  };
  std::ofstream(terrain) << "malformed managed terrain";
  ASSERT_NO_FATAL_FAILURE(open_fresh_manager(primary_project));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  auto* rejected = manager->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
      EditorType::kOverworld);
  ASSERT_NE(rejected, nullptr);
  EXPECT_FALSE(rejected->lyra_canvas_status_.ok());
  EXPECT_FALSE(rejected->lyra_canvas_.loaded());
  EXPECT_TRUE(manager->GetCurrentProject()->lyra_terrain_file.empty());
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  EXPECT_EQ(ReadFile(terrain), "malformed managed terrain");
  EXPECT_EQ(ReadFile(primary_project), initial_descriptor);
  std::filesystem::remove(terrain);
  ASSERT_NO_FATAL_FAILURE(open_fresh_manager(primary_project));
  ASSERT_FALSE(manager->IsCurrentProjectDirty());
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  auto* editor = manager->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
      EditorType::kOverworld);
  ASSERT_NE(editor, nullptr);
  ASSERT_TRUE(editor->lyra_canvas_.loaded());
  EXPECT_EQ(editor->current_map_id(), 0xA0);
  EXPECT_EQ(editor->current_world_id(), 2);
  EXPECT_EQ(manager->GetCurrentProject()->lyra_terrain_file, terrain.string());
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_TRUE(editor->HasPendingProjectDraftChanges());
  EXPECT_FALSE(manager->GetCurrentRom()->dirty());
  EXPECT_FALSE(std::filesystem::exists(terrain));
  EXPECT_EQ(ReadFile(primary_project), initial_descriptor);

  const auto initial_authoring = editor->lyra_canvas_.document()->authoring();
  EXPECT_TRUE(initial_authoring.gameplay_areas.empty());
  UndoManager area_history;
  auto& canvas = editor->lyra_canvas_;
  using AreaSize = core::LyraGameplayAreaSize;
  ASSERT_OK(canvas.SetGameplayArea(0xA0, AreaSize::kLarge, editor->overworld(),
                                   area_history));
  ASSERT_NE(canvas.document()->GameplayAreaForMap(0xA9), nullptr);
  EXPECT_EQ(canvas.document()->GameplayAreaForMap(0xA9)->parent_map_id, 0xA0);
  const auto assigned_area = canvas.document()->authoring();
  const auto area_history_size = area_history.UndoStackSize();
  // A child cannot silently steal ownership, and a root cannot wrap a row.
  EXPECT_FALSE(canvas
                   .SetGameplayArea(0xA1, AreaSize::kSmall, editor->overworld(),
                                    area_history)
                   .ok());
  EXPECT_FALSE(canvas
                   .SetGameplayArea(0xA7, AreaSize::kWide, editor->overworld(),
                                    area_history)
                   .ok());
  EXPECT_EQ(canvas.document()->authoring(), assigned_area);
  EXPECT_EQ(area_history.UndoStackSize(), area_history_size);
  ASSERT_OK(canvas.SetGameplayArea(0xA9, std::nullopt, editor->overworld(),
                                   area_history));
  EXPECT_TRUE(canvas.document()->authoring().gameplay_areas.empty());
  ASSERT_OK(area_history.Undo());
  EXPECT_EQ(canvas.document()->authoring(), assigned_area);
  ASSERT_OK(area_history.Redo());
  EXPECT_EQ(canvas.document()->authoring(), initial_authoring);
  auto selected_scene = initial_authoring;
  selected_scene.maps[1].properties.message_id = 0x42;
  selected_scene.gameplay_areas.push_back(
      {0xA0, AreaSize::kWide, {0xA0, 0xA1}});
  selected_scene.edge_connections.push_back(
      {1, 0x99, 0xA1, core::LyraEdgePassage{15, 16}});
  ASSERT_OK(editor->EditProjectMapAuthoring(selected_scene));
  // The editor binds changed map properties to native assets. Account only for
  // these derived fingerprints; every requested and unrelated field stays exact.
  const auto& bindings =
      editor->lyra_canvas_.document()->authoring().maps[1].asset_bindings;
  for (const auto& binding : bindings) {
    ASSERT_EQ(binding.size(), 64u);
    ASSERT_EQ(binding.find_first_not_of("0123456789abcdef"), std::string::npos);
  }
  selected_scene.maps[1].asset_bindings = bindings;
  ASSERT_EQ(editor->lyra_canvas_.document()->authoring(), selected_scene);
  ASSERT_OK(editor->Undo());
  EXPECT_EQ(editor->lyra_canvas_.document()->authoring(), initial_authoring);
  ASSERT_OK(editor->Redo());
  EXPECT_EQ(editor->lyra_canvas_.document()->authoring(), selected_scene);

  // The normal Save command persists project-only work despite ROM writes
  // being blocked, and never enlists an unnecessary ROM serializer.
  ASSERT_OK(manager->SaveCurrentSession());
  ASSERT_TRUE(std::filesystem::is_regular_file(terrain));
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  EXPECT_FALSE(editor->HasPendingProjectDraftChanges());
  const auto terrain_bytes = ReadFile(terrain);
  auto saved_descriptor = ReadFile(primary_project);
  project::YazeProject saved;
  ASSERT_OK(saved.Open(primary_project.string()));
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      saved.GetAbsolutePath(saved.lyra_terrain_file), terrain.string()));

  // A project made before the terrain reference was recorded discovers its
  // managed file without requiring a separate Open Lyra Terrain operation.
  std::ofstream(primary_project) << initial_descriptor;
  ASSERT_NO_FATAL_FAILURE(open_fresh_manager(primary_project));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  editor = manager->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
      EditorType::kOverworld);
  ASSERT_TRUE(editor->lyra_canvas_.loaded());
  EXPECT_EQ(editor->lyra_canvas_.document()->Serialize(), terrain_bytes);
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  ASSERT_OK(manager->SaveCurrentSession());
  saved_descriptor = ReadFile(primary_project);

  auto save_as_scene = selected_scene;
  save_as_scene.maps[0].properties.message_id = 0x71;
  save_as_scene.gameplay_areas[0] = {
      0xA0, AreaSize::kLarge, {0xA0, 0xA1, 0xA8, 0xA9}};
  ASSERT_OK(editor->EditProjectMapAuthoring(save_as_scene));
  save_as_scene = editor->lyra_canvas_.document()->authoring();
  const auto save_as_bytes = editor->lyra_canvas_.document()->Serialize();
  const auto* saved_document = editor->lyra_canvas_.document();

  const auto refused_project = relocated / "occupied.yaze";
  const auto refused_terrain = relocated / "occupied.lyra-terrain.json";
  std::ofstream(refused_project) << "unrelated project";
  std::ofstream(refused_terrain) << "unrelated terrain";
  EXPECT_FALSE(manager->SaveProjectAs(refused_project.string()).ok());
  EXPECT_EQ(manager->GetCurrentProject()->filepath, primary_project.string());
  EXPECT_EQ(manager->GetCurrentProject()->lyra_terrain_file, terrain.string());
  EXPECT_EQ(editor->lyra_canvas_.document(), saved_document);
  EXPECT_EQ(editor->lyra_canvas_.document()->Serialize(), save_as_bytes);
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_EQ(ReadFile(refused_project), "unrelated project");
  EXPECT_EQ(ReadFile(refused_terrain), "unrelated terrain");
  EXPECT_EQ(ReadFile(primary_project), saved_descriptor);
  EXPECT_EQ(ReadFile(terrain), terrain_bytes);

  ASSERT_OK(manager->SaveProjectAs(relocated_project.string()));
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  EXPECT_EQ(ReadFile(primary_project), saved_descriptor);
  EXPECT_EQ(ReadFile(terrain), terrain_bytes);
  EXPECT_EQ(ReadFile(relocated_terrain), save_as_bytes);
  EXPECT_EQ(editor->lyra_canvas_.document(), saved_document);
  ASSERT_OK(editor->Undo());
  EXPECT_EQ(editor->lyra_canvas_.document()->authoring(), selected_scene);
  ASSERT_OK(editor->Redo());
  EXPECT_EQ(editor->lyra_canvas_.document()->authoring(), save_as_scene);
  ASSERT_OK(saved.Open(relocated_project.string()));
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      saved.GetAbsolutePath(saved.lyra_terrain_file),
      relocated_terrain.string()));
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      saved.GetAbsolutePath(saved.rom_filename), primary_rom.string()));
  // Save As publishes the new owner's receipt binding its own descriptor,
  // its terrain sidecar and the unchanged ROM.
  const auto relocated_receipt = nlohmann::json::parse(
      ReadFile(relocated_project.string() + ".native-save.json"));
  EXPECT_EQ(relocated_receipt.at("operation"), "save_as");
  // The fixture names label/symbol files it never creates; only those may be
  // reported unbound.
  for (const auto& error : relocated_receipt.at("binding_errors")) {
    EXPECT_EQ(error.get<std::string>().rfind("dependency.", 0), 0u)
        << relocated_receipt.dump(2);
  }
  for (const auto& [role, path] :
       std::vector<std::pair<std::string, std::filesystem::path>>{
           {"project", relocated_project},
           {"sidecar", relocated_terrain},
           {"rom", primary_rom}}) {
    const auto& artifacts = relocated_receipt.at("artifacts");
    const auto found =
        std::find_if(artifacts.begin(), artifacts.end(),
                     [&](const auto& item) { return item.at("role") == role; });
    ASSERT_NE(found, artifacts.end()) << role;
    EXPECT_EQ(found->at("path"), std::filesystem::canonical(path).string());
    EXPECT_EQ(found->at("sha256"),
              core::ComputeSourceArtifactSha256(ReadFile(path)));
  }

  ASSERT_NO_FATAL_FAILURE(open_fresh_manager(relocated_project));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  editor = manager->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
      EditorType::kOverworld);
  ASSERT_NE(editor, nullptr);
  ASSERT_TRUE(editor->lyra_canvas_.loaded());
  EXPECT_EQ(editor->lyra_canvas_.document()->Serialize(), save_as_bytes);
  EXPECT_EQ(editor->lyra_canvas_.document()->authoring(), save_as_scene);
  EXPECT_EQ(editor->current_map_id(), 0xA0);
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  ASSERT_OK(manager->OpenLyraTerrainFromPath(relocated_terrain.string()));
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  EXPECT_FALSE(editor->HasPendingProjectDraftChanges());

  ASSERT_OK(manager->OpenRomOrProject(consumer_project.string()));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  ASSERT_TRUE(manager->GetCurrentProject()->lyra_terrain_file.empty());
  editor = manager->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
      EditorType::kOverworld);
  ASSERT_NE(editor, nullptr);
  EXPECT_FALSE(editor->lyra_canvas_.loaded());
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  ASSERT_OK(manager->OpenLyraTerrainFromPath(terrain.string()));
  editor = manager->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
      EditorType::kOverworld);
  ASSERT_NE(editor, nullptr);
  ASSERT_TRUE(editor->lyra_canvas_.loaded());
  EXPECT_EQ(editor->lyra_canvas_.document()->Serialize(), terrain_bytes);
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_TRUE(editor->HasPendingProjectDraftChanges());
  ASSERT_OK(manager->SaveProject());
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  ASSERT_OK(manager->OpenLyraTerrainFromPath(terrain.string()));
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  EXPECT_FALSE(editor->HasPendingProjectDraftChanges());

  auto changed = *editor->lyra_canvas_.document();
  auto authoring = changed.authoring();
  authoring.maps[1].properties.message_id = 0x123;
  ASSERT_OK(changed.SetAuthoring(changed.authoring(), authoring));
  std::ofstream(replacement) << changed.Serialize();
  ASSERT_OK(manager->OpenLyraTerrainFromPath(replacement.string()));
  EXPECT_EQ(manager->GetCurrentProject()->lyra_terrain_file,
            replacement.string());
  EXPECT_EQ(editor->lyra_canvas_.document()->authoring(), authoring);
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_TRUE(editor->HasPendingProjectDraftChanges());
  EXPECT_FALSE(manager->GetCurrentRom()->dirty());
  ASSERT_OK(manager->SaveProject());
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  EXPECT_FALSE(editor->HasPendingProjectDraftChanges());
  ASSERT_OK(saved.Open(consumer_project.string()));
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      saved.GetAbsolutePath(saved.lyra_terrain_file), replacement.string()));
  EXPECT_EQ(ReadFile(terrain), terrain_bytes);
  EXPECT_EQ(core::ComputeSourceArtifactSha256(ReadFile(primary_rom)),
            immutable_hash);
  EXPECT_EQ(core::ComputeSourceArtifactSha256(ReadFile(consumer_rom)),
            immutable_hash);
  EXPECT_EQ(core::ComputeSourceArtifactSha256(ReadFile(immutable_rom)),
            immutable_hash);

  const char* retain = std::getenv("YAZE_LYRA_AUTHORING_RETAIN");
  if (retain != nullptr && std::string(retain) == "1" &&
      !::testing::Test::HasFailure() && !::testing::Test::IsSkipped()) {
    directory.retain_on_success = true;
  }
}

TEST(EditorManagerProjectActionsTest,
     ReopenActiveProjectAliasPreservesDirtyRomAndProjectDraft) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir directory(MakeTempDir("yaze_reopen_project"));
  const auto rom = directory.path / "game.sfc";
  const auto project = directory.path / "project.yaze";
  const auto alias = directory.path / "alias.yaze";
  WriteRomFile(rom);
  WriteProjectFile(project, "Original", rom);
  std::filesystem::create_symlink(project, alias);
  ASSERT_OK(manager->OpenRomOrProject(project.string()));
  auto* original_rom = manager->GetCurrentRom();
  const auto original_id = manager->GetCurrentSessionId();
  ASSERT_OK(original_rom->WriteByte(0x100, 0xA5));
  manager->GetCurrentProject()->name = "Unsaved Settings";
  manager->MarkCurrentProjectDirty();
  manager->ShowProjectFileEditor();
  const auto draft = MakeModifiedProjectDraft(
      manager->project_file_editor(), manager->GetCurrentProject(),
      ProjectFileContents("Unsaved Text", rom));

  ASSERT_OK(manager->OpenRomOrProject(alias.string()));
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
  EXPECT_EQ(manager->GetCurrentSessionId(), original_id);
  EXPECT_EQ(manager->GetCurrentRom(), original_rom);
  EXPECT_EQ(original_rom->vector()[0x100], 0xA5);
  EXPECT_TRUE(original_rom->dirty());
  EXPECT_EQ(manager->GetCurrentProject()->name, "Unsaved Settings");
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_EQ(manager->project_file_editor()->CaptureState().text, draft.text);
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  const auto status = nlohmann::json::parse(
      manager->ExecuteEditorControl(R"({"command":"status"})"));
  const auto& state = status["state"];
  EXPECT_EQ(state["session_count"], 1);
  ASSERT_EQ(state["sessions"].size(), 1u);
  EXPECT_EQ(state["sessions"][0]["session_id"], original_id);
  EXPECT_EQ(state["sessions"][0]["project_path"], project.string());
  EXPECT_EQ(state["sessions"][0]["rom_path"], rom.string());
  EXPECT_EQ(state["sessions"][0]["rom_dirty"], true);
  EXPECT_EQ(state["sessions"][0]["unsaved"], true);
  EXPECT_EQ(state["sessions"][0]["active"], true);
}

TEST(EditorManagerProjectActionsTest,
     ReopenInactiveProjectActivatesOwnerAndKeepsDistinctProjects) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir directory(MakeTempDir("yaze_reopen_inactive_project"));
  const auto rom_a = directory.path / "a.sfc";
  const auto rom_b = directory.path / "b.sfc";
  const auto project_a = directory.path / "a.yaze";
  const auto project_b = directory.path / "b.yaze";
  const auto project_c = directory.path / "different_project_same_rom.yaze";
  WriteRomFile(rom_a, "ROM A");
  WriteRomFile(rom_b, "ROM B");
  WriteProjectFile(project_a, "Project A", rom_a);
  WriteProjectFile(project_b, "Project B", rom_b);
  WriteProjectFile(project_c, "Project C", rom_a);
  ASSERT_OK(manager->OpenRomOrProject(project_a.string()));
  auto* original_a = manager->GetCurrentRom();
  const auto id_a = manager->GetCurrentSessionId();
  ASSERT_OK(manager->OpenRomOrProject(project_b.string()));
  const auto id_b = manager->GetCurrentSessionId();
  ASSERT_NE(id_a, id_b);
  ASSERT_OK(original_a->WriteByte(0x100, 0xA5));

  ASSERT_OK(manager->OpenRomOrProject(project_a.string()));
  EXPECT_EQ(manager->GetCurrentSessionId(), id_a);
  EXPECT_EQ(manager->GetCurrentRom(), original_a);
  EXPECT_EQ(original_a->vector()[0x100], 0xA5);
  EXPECT_TRUE(original_a->dirty());
  EXPECT_EQ(manager->GetCurrentProject()->name, "Project A");
  EXPECT_EQ(manager->GetActiveSessionCount(), 2u);
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());

  ASSERT_OK(manager->OpenRomOrProject(project_b.string()));
  EXPECT_EQ(manager->GetCurrentSessionId(), id_a);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  EXPECT_EQ(manager->GetCurrentSessionId(), id_b);
  EXPECT_TRUE(original_a->dirty());
  EXPECT_EQ(manager->GetCurrentProject()->name, "Project B");
  const auto conflict = manager->OpenRomOrProject(project_c.string());
  EXPECT_EQ(conflict.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_EQ(manager->GetCurrentSessionId(), id_b);
  EXPECT_EQ(manager->GetCurrentProject()->name, "Project B");
  EXPECT_EQ(manager->GetActiveSessionCount(), 2u);
}

TEST(EditorManagerProjectActionsTest,
     ExplicitStartupProjectOpensOnlyOneSessionWithRestorePreference) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ScopedTempDir directory(MakeTempDir("yaze_explicit_startup_project"));
  const auto rom = directory.path / "explicit.sfc";
  const auto project = directory.path / "explicit.yaze";
  const auto previous_rom = directory.path / "previous.sfc";
  const auto previous_project = directory.path / "previous.yaze";
  WriteRomFile(rom);
  WriteRomFile(previous_rom);
  WriteProjectFile(project, "Explicit", rom);
  WriteProjectFile(previous_project, "Previous", previous_rom);
  manager->user_settings().SetSettingsFilePathForTesting(
      (directory.path / "settings.json").string());
  auto& preferences = manager->user_settings().prefs();
  preferences.restore_last_session = true;
  preferences.last_rom_path = previous_rom.string();
  preferences.last_project_path = previous_project.string();
  ASSERT_OK(manager->user_settings().Save());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->Initialize(renderer.get(), project.string());
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
  EXPECT_EQ(manager->GetCurrentProject()->filepath, project.string());
  ASSERT_NE(manager->GetCurrentRom(), nullptr);
  EXPECT_EQ(manager->GetCurrentRom()->filename(), rom.string());
  EXPECT_TRUE(manager->user_settings().prefs().restore_last_session);
}

TEST(EditorManagerProjectActionsTest,
     SaveProjectAsWarnsWhenPaletteCatalogLeavesNewProjectFolder) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_catalog_save_as"));
  const auto original_folder = temp_dir.path / "original";
  const auto sibling_folder = temp_dir.path / "sibling";
  const auto child_folder = original_folder / "child";
  std::filesystem::create_directories(sibling_folder);
  std::filesystem::create_directories(child_folder);
  const auto rom_path = original_folder / "source.sfc";
  const auto project_path = original_folder / "source.yaze";
  const auto catalog_path = original_folder / "catalog.json";
  WriteRomFile(rom_path);
  WriteProjectFile(project_path, "Catalog Save As", rom_path);
  const std::string catalog = R"({"schema_version":1,"game_id":"oracle",
"entries":[{"key":"sprites","name":"Sprites","group":"global_sprites",
"index":0,"world":"light_world","use":"mixed","description":"Sprites",
"provenance":"test","ownership":"rom","usages":[]}]})";
  std::ofstream(catalog_path) << catalog;
  {
    project::YazeProject project;
    ASSERT_OK(project.Open(project_path.string()));
    project.palette_catalog_file = catalog_path.string();
    ASSERT_OK(project.Save());
  }
  const auto original_descriptor = ReadFile(project_path);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  ASSERT_TRUE(core::PaletteCatalog::LoadForProject(
                  project_path.string(),
                  manager->GetCurrentProject()->palette_catalog_file)
                  .ok());

  const std::vector<std::pair<std::filesystem::path, bool>> destinations = {
      {original_folder / "same-folder.yaze", false},
      {sibling_folder / "sibling.yaze", true},
      {child_folder / "child.yaze", true},
      {temp_dir.path / "ancestor.yaze", false}};
  for (const auto& [destination, expect_warning] : destinations) {
    SCOPED_TRACE(destination.string());
    const auto toast_begin = manager->toast_manager()->pending_count();
    ASSERT_OK(manager->SaveProjectAs(destination.string()));
    bool warned = false;
    for (size_t i = toast_begin; i < manager->toast_manager()->pending_count();
         ++i) {
      const auto* toast = manager->toast_manager()->GetPendingToast(i);
      ASSERT_NE(toast, nullptr);
      if (toast->type == ToastType::kWarning &&
          toast->message.find("palette catalog") != std::string::npos) {
        warned = true;
        EXPECT_NE(toast->message.find("Copy the catalog"), std::string::npos);
        EXPECT_NE(toast->message.find("palette_catalog_file"),
                  std::string::npos);
      }
    }
    EXPECT_EQ(warned, expect_warning);
    project::YazeProject reopened;
    ASSERT_OK(reopened.Open(destination.string()));
    EXPECT_EQ(core::PaletteCatalog::LoadForProject(
                  reopened.filepath, reopened.palette_catalog_file)
                  .ok(),
              !expect_warning);
    EXPECT_EQ(ReadFile(catalog_path), catalog);
    EXPECT_EQ(ReadFile(project_path), original_descriptor);
  }
}

TEST(EditorManagerProjectActionsTest,
     ReadOnlyProjectDisablesSyncAndQueuedBuilds) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  ScopedTempDir dir(MakeTempDir("yaze_readonly_build"));
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "ReadOnlyReference";
  project->filepath = (dir.path / "project.yaze").string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kReadOnly;
  project->build_script = "must-not-execute";
  EXPECT_EQ(manager->BuildCurrentProject().code(),
            absl::StatusCode::kPermissionDenied);
  manager->QueueBuildCurrentProject();
  const auto status = ContentRegistry::Context::build_workflow_status();
  EXPECT_EQ(status.summary, "Build unavailable");
  EXPECT_NE(status.detail.find("read-only"), std::string::npos);
}

TEST(EditorManagerProjectActionsTest,
     BuildCurrentProjectUsesProjectBuildScript) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_build_project_script"));
  const auto project_path = temp_dir.path / "project.yaze";
  const auto marker_path = temp_dir.path / "build.marker";
  const auto script_path = temp_dir.path / "build_ok.sh";

  {
    std::ofstream script(script_path);
    ASSERT_TRUE(script.is_open());
    script << "#!/bin/sh\n";
    script << "printf 'project-build' > build.marker\n";
  }
  std::filesystem::permissions(script_path,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::add);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "BuildProjectScript";
  project->filepath = project_path.string();
  project->build_script = "./build_ok.sh";

  ASSERT_OK(manager->BuildCurrentProject());
  ASSERT_TRUE(std::filesystem::exists(marker_path));
  EXPECT_EQ(ReadFile(marker_path), "project-build");
#endif  // _WIN32
}

TEST(EditorManagerProjectActionsTest,
     SyncAndQueuedBuildsUseRelocatedProjectContextWithoutChangingRequest) {
#if defined(_WIN32) || defined(__EMSCRIPTEN__) || \
    (defined(__APPLE__) && TARGET_OS_IPHONE)
  GTEST_SKIP() << "Background commands require a POSIX desktop";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir temp(MakeTempDir("yaze_project_build_context"));
  const auto original = temp.path / "original";
  const auto relocated = temp.path / "copy with 'quotes'";
  std::filesystem::create_directories(original / "code folder");
  std::filesystem::create_directories(relocated);
  const auto original_project = original / "original.yaze";
  const auto copied_project = relocated / "copy.yaze";
  const auto rom = original / "source.sfc";
  const auto terrain = original / "terrain.json";
  const auto request = original / "request.json";
  WriteRomFile(rom);
  std::ofstream(terrain) << "terrain context canary";
  std::ofstream(request) << "request context canary";
  auto* project = manager->GetCurrentProject();
  project->name = "Build context";
  project->filepath = original_project.string();
  project->rom_filename = rom.string();
  project->code_folder = (original / "code folder").string();
  project->lyra_terrain_file = terrain.string();
  project->lyra_build_request_file = request.string();
  project->build_script = "./build_context.sh";
  ASSERT_OK(project->Save());
  ASSERT_OK(project->Open(original_project.string()));
  const auto original_bytes = ReadFile(original_project);
  ASSERT_OK(project->SaveAs(copied_project.string()));
  ASSERT_EQ(project->lyra_build_request_file, request.string());

  const auto script_path = relocated / "build_context.sh";
  {
    std::ofstream script(script_path);
    ASSERT_TRUE(script.is_open());
    script << R"sh(#!/bin/sh
printf '%s\n' "$YAZE_PROJECT_FILE" "$YAZE_PROJECT_CODE_FOLDER" \
  "$YAZE_PROJECT_ROM" "$YAZE_LYRA_TERRAIN_FILE" "$OOS_LYRA_PROJECT" \
  "$OOS_LYRA_REQUEST" > build.context
printf '%s\n' "${YAZE_PROJECT_FILE+set}" "${YAZE_PROJECT_CODE_FOLDER+set}" \
  "${YAZE_PROJECT_ROM+set}" "${YAZE_LYRA_TERRAIN_FILE+set}" \
  "${OOS_LYRA_PROJECT+set}" "${OOS_LYRA_REQUEST+set}" > build.presence
)sh";
    ASSERT_TRUE(script.good());
  }
  std::filesystem::permissions(
      script_path,
      std::filesystem::perms::owner_exec | std::filesystem::perms::owner_read,
      std::filesystem::perm_options::add);
  const std::string expected =
      copied_project.string() + "\n" + (original / "code folder").string() +
      "\n" + rom.string() + "\n" + terrain.string() + "\n" +
      copied_project.string() + "\n" + request.string() + "\n";
  for (bool queued : {false, true}) {
    SCOPED_TRACE(queued ? "queued" : "synchronous");
    std::filesystem::remove(relocated / "build.context");
    if (queued) {
      manager->QueueBuildCurrentProject();
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (ContentRegistry::Context::build_workflow_status().state ==
                 ProjectWorkflowState::kRunning &&
             std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ImGui::GetIO().DisplaySize = ImVec2(1280, 720);
        ImGui::NewFrame();
        const auto status = manager->Update();
        ImGui::Render();
        ASSERT_OK(status);
      }
      const auto status = ContentRegistry::Context::build_workflow_status();
      ASSERT_EQ(status.state, ProjectWorkflowState::kSuccess) << status.detail;
    } else {
      ASSERT_OK(manager->BuildCurrentProject());
    }
    EXPECT_EQ(ReadFile(relocated / "build.context"), expected);
    EXPECT_EQ(ReadFile(relocated / "build.presence"),
              "set\nset\nset\nset\nset\nset\n");
  }

  // Every known name remains an explicit override when its binding is absent.
  project->code_folder.clear();
  project->rom_filename.clear();
  project->lyra_terrain_file.clear();
  project->lyra_build_request_file.clear();
  ASSERT_OK(project->Save());
  ASSERT_OK(manager->BuildCurrentProject());
  EXPECT_EQ(ReadFile(relocated / "build.context"),
            copied_project.string() + "\n\n\n\n\n\n");
  EXPECT_EQ(ReadFile(relocated / "build.presence"),
            "set\nset\nset\nset\nset\nset\n");
  EXPECT_EQ(ReadFile(original_project), original_bytes);
  EXPECT_EQ(ReadFile(request), "request context canary");
  EXPECT_EQ(ReadFile(terrain), "terrain context canary");
#endif
}

TEST(EditorManagerProjectActionsTest,
     BuildCurrentProjectFallsBackToManifestBuildScript) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_build_manifest_script"));
  const auto project_path = temp_dir.path / "project.yaze";
  const auto marker_path = temp_dir.path / "manifest.marker";
  const auto script_path = temp_dir.path / "manifest_build.sh";

  {
    std::ofstream script(script_path);
    ASSERT_TRUE(script.is_open());
    script << "#!/bin/sh\n";
    script << "printf 'manifest-build' > manifest.marker\n";
  }
  std::filesystem::permissions(script_path,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::add);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "BuildManifestScript";
  project->filepath = project_path.string();
  project->build_script.clear();
  ASSERT_TRUE(project->hack_manifest
                  .LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Generic Hack",
  "build_pipeline": {
    "build_script": "./manifest_build.sh"
  }
}
)json")
                  .ok());

  ASSERT_OK(manager->BuildCurrentProject());
  ASSERT_TRUE(std::filesystem::exists(marker_path));
  EXPECT_EQ(ReadFile(marker_path), "manifest-build");
#endif  // _WIN32
}

TEST(EditorManagerProjectActionsTest,
     BuildRefusesUnsavedRomBeforeStartingConfiguredScript) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_build_unsaved_rom"));
  const auto rom_path = temp_dir.path / "source.sfc";
  const auto marker_path = temp_dir.path / "build.marker";
  const auto script_path = temp_dir.path / "build_ok.sh";
  WriteRomFile(rom_path);
  {
    std::ofstream script(script_path);
    ASSERT_TRUE(script.is_open());
    script << "#!/bin/sh\nprintf 'built' > build.marker\n";
  }
  std::filesystem::permissions(
      script_path,
      std::filesystem::perms::owner_exec | std::filesystem::perms::owner_read,
      std::filesystem::perm_options::add);

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "BuildUnsavedRom";
  project->filepath = (temp_dir.path / "project.yaze").string();
  project->rom_filename = rom_path.string();
  project->build_script = "./build_ok.sh";
  ASSERT_NE(manager->GetCurrentRom(), nullptr);
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));

  const auto status = manager->BuildCurrentProject();
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("Save project and ROM changes"),
            std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(marker_path));
#endif
}

TEST(EditorManagerProjectActionsTest,
     BuildRefusesSavedCopyWhenProjectStillNamesOriginalRom) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  const auto original_flags = core::FeatureFlags::get();
  const auto restore_flags =
      absl::MakeCleanup([&] { core::FeatureFlags::get() = original_flags; });
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_build_saved_copy"));
  const auto rom_path = temp_dir.path / "source.sfc";
  const auto copy_path = temp_dir.path / "saved-copy.sfc";
  const auto marker_path = temp_dir.path / "build.marker";
  const auto script_path = temp_dir.path / "build_ok.sh";
  WriteRomFile(rom_path);
  {
    std::ofstream script(script_path);
    ASSERT_TRUE(script.is_open());
    script << "#!/bin/sh\nprintf 'built' > build.marker\n";
  }
  std::filesystem::permissions(
      script_path,
      std::filesystem::perms::owner_exec | std::filesystem::perms::owner_read,
      std::filesystem::perm_options::add);

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "BuildSavedCopy";
  project->filepath = (temp_dir.path / "project.yaze").string();
  project->rom_filename = rom_path.string();
  project->build_script = "./build_ok.sh";
  ASSERT_OK(manager->BuildCurrentProject());
  ASSERT_TRUE(std::filesystem::exists(marker_path));
  ASSERT_TRUE(std::filesystem::remove(marker_path));

  // This synthetic ROM tests Save As binding, not asset serialization. The
  // Oracle acceptance tests below exercise the actual build inputs separately.
  auto& flags = core::FeatureFlags::get();
  flags.kSaveDungeonMaps = flags.kSaveGraphicsSheet = flags.kSaveMessages =
      false;
  flags.kSaveAllPalettes = flags.kSaveGfxGroups = false;
  flags.dungeon = {.kSaveObjects = false,
                   .kSaveSprites = false,
                   .kSaveRoomHeaders = false,
                   .kSaveTorches = false,
                   .kSavePits = false,
                   .kSaveBlocks = false,
                   .kSaveCollision = false,
                   .kSaveWaterFillZones = false,
                   .kSaveChests = false,
                   .kSavePotItems = false,
                   .kSaveEntrances = false,
                   .kSavePalettes = false};
  flags.overworld.kSaveOverworldMaps = false;
  flags.overworld.kSaveOverworldEntrances = false;
  flags.overworld.kSaveOverworldExits = false;
  flags.overworld.kSaveOverworldItems = false;
  flags.overworld.kSaveOverworldProperties = false;
  flags.overworld.kSaveOverworldSprites = false;
  manager->user_settings().prefs().backup_before_save = false;
  ASSERT_OK(manager->SaveRomAs(copy_path.string()));
  ASSERT_TRUE(std::filesystem::exists(copy_path));
  const auto status = manager->BuildCurrentProject();
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("Build source mismatch"),
            std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(marker_path));
#endif
}

TEST(EditorManagerProjectActionsTest,
     BuildRefusesMissingManifestFallbackBeforeStartingProcess) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_build_missing_manifest_script"));
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "EditorOnlyProject";
  project->filepath = (temp_dir.path / "project.yaze").string();
  project->build_script.clear();
  ASSERT_OK(project->hack_manifest.LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Editor-only fixture",
  "build_pipeline": {
    "build_script": "Scripts/Build/build_rom.sh"
  }
}
)json"));

  manager->QueueBuildCurrentProject();
  const auto status = ContentRegistry::Context::build_workflow_status();
  EXPECT_EQ(status.summary, "Build unavailable");
  EXPECT_NE(status.detail.find("Manifest build script is unavailable"),
            std::string::npos);
}

TEST(EditorManagerProjectActionsTest,
     BuildRefusesManifestDevelopmentRomMismatchBeforeProcess) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_build_manifest_source"));
  const auto marker_path = temp_dir.path / "build.marker";
  const auto script_path = temp_dir.path / "build_ok.sh";
  {
    std::ofstream script(script_path);
    ASSERT_TRUE(script.is_open());
    script << "#!/bin/sh\nprintf 'built' > build.marker\n";
  }
  std::filesystem::permissions(
      script_path,
      std::filesystem::perms::owner_exec | std::filesystem::perms::owner_read,
      std::filesystem::perm_options::add);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "ManifestSourceMismatch";
  project->filepath = (temp_dir.path / "project.yaze").string();
  project->rom_filename = "source.sfc";
  project->build_script.clear();
  ASSERT_OK(project->hack_manifest.LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Manifest source fixture",
  "build_pipeline": {
    "dev_rom": "other.sfc",
    "build_script": "./build_ok.sh"
  }
}
)json"));

  const auto status = manager->BuildCurrentProject();
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("manifest development ROM"),
            std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(marker_path));
#endif
}

TEST(EditorManagerProjectActionsTest,
     BuildRefusesLiveProjectPanelAndRawProjectFileDraft) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_build_live_project_drafts"));
  const auto rom_path = temp_dir.path / "source.sfc";
  const auto project_path = temp_dir.path / "project.yaze";
  const auto marker_path = temp_dir.path / "build.marker";
  const auto script_path = temp_dir.path / "build_ok.sh";
  WriteRomFile(rom_path);
  {
    std::ofstream project(project_path);
    ASSERT_TRUE(project.is_open());
    project << ProjectFileContents("Build Draft", rom_path)
            << "[build]\nbuild_script=./build_ok.sh\n";
  }
  {
    std::ofstream script(script_path);
    ASSERT_TRUE(script.is_open());
    script << "#!/bin/sh\nprintf 'built' > build.marker\n";
  }
  std::filesystem::permissions(
      script_path,
      std::filesystem::perms::owner_exec | std::filesystem::perms::owner_read,
      std::filesystem::perm_options::add);

  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  auto* session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  auto* panel = manager->project_management_panel();
  ASSERT_NE(panel, nullptr);
  EXPECT_FALSE(session->project_dirty);
  panel->SetProjectDirty(true);
  EXPECT_FALSE(session->project_dirty);

  const auto panel_status = manager->BuildCurrentProject();
  EXPECT_EQ(panel_status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(
      std::string(panel_status.message()).find("unsaved project settings"),
      std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(marker_path));

  panel->SetProjectDirty(false);
  manager->ShowProjectFileEditor();
  auto* raw_editor = manager->project_file_editor();
  ASSERT_NE(raw_editor, nullptr);
  MakeModifiedProjectDraft(raw_editor, manager->GetCurrentProject(),
                           ProjectFileContents("Unsaved Raw Draft", rom_path));
  EXPECT_FALSE(session->project_file_editor_state.modified);

  manager->QueueBuildCurrentProject();
  const auto build_status = ContentRegistry::Context::build_workflow_status();
  EXPECT_EQ(build_status.summary, "Build unavailable");
  EXPECT_NE(build_status.detail.find("unsaved project-file draft"),
            std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(marker_path));
#endif
}

TEST(EditorManagerProjectActionsTest,
     RunCurrentProjectPrefersManifestPatchedRomAndReloadsEmulator) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_run_project_output"));
  const auto project_path = temp_dir.path / "project.yaze";
  const auto valid_rom = temp_dir.path / "patched_valid.sfc";
  WriteRomFile(valid_rom, "RUN TARGET ROM");

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "RunProjectOutput";
  project->filepath = project_path.string();
  project->build_target = "missing_output.sfc";
  ASSERT_TRUE(project->hack_manifest
                  .LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Generic Hack",
  "build_pipeline": {
    "patched_rom": "patched_valid.sfc"
  }
}
)json")
                  .ok());

  ASSERT_OK(manager->RunCurrentProject());
  EXPECT_TRUE(manager->emulator().is_snes_initialized());
  EXPECT_TRUE(manager->emulator().running());
  ASSERT_NE(manager->ui_coordinator(), nullptr);
  EXPECT_TRUE(manager->ui_coordinator()->IsEmulatorVisible());

  const auto& history = manager->toast_manager()->GetHistory();
  ASSERT_FALSE(history.empty());
  EXPECT_NE(history.front().message.find("patched_valid.sfc"),
            std::string::npos);
#endif  // _WIN32
}

TEST(EditorManagerProjectActionsTest,
     WorkflowCallbacksTriggerBuildRunAndOpenOutputPanel) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_workflow_callbacks"));
  const auto project_path = temp_dir.path / "project.yaze";
  const auto marker_path = temp_dir.path / "build.marker";
  const auto script_path = temp_dir.path / "build_ok.sh";
  const auto valid_rom = temp_dir.path / "patched_valid.sfc";

  {
    std::ofstream script(script_path);
    ASSERT_TRUE(script.is_open());
    script << "#!/bin/sh\n";
    script << "printf 'callback-build' > build.marker\n";
  }
  std::filesystem::permissions(script_path,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::add);
  WriteRomFile(valid_rom, "CALLBACK RUN ROM");

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "WorkflowCallbacks";
  project->filepath = project_path.string();
  project->build_script = "./build_ok.sh";
  ASSERT_TRUE(project->hack_manifest
                  .LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Generic Hack",
  "build_pipeline": {
    "patched_rom": "patched_valid.sfc"
  }
}
)json")
                  .ok());

  auto show_output = ContentRegistry::Context::show_workflow_output_callback();
  ASSERT_TRUE(show_output);
  show_output();
  EXPECT_TRUE(manager->window_manager().IsWindowOpen(
      manager->GetCurrentSessionId(), "workflow.output"));

  auto start_build = ContentRegistry::Context::start_build_workflow_callback();
  ASSERT_TRUE(start_build);
  start_build();

  const auto build_status = ContentRegistry::Context::build_workflow_status();
  EXPECT_TRUE(build_status.visible);
  EXPECT_TRUE(build_status.can_cancel);
  EXPECT_EQ(build_status.state, ProjectWorkflowState::kRunning);

  bool build_completed = false;
  for (int i = 0; i < 40; ++i) {
    if (std::filesystem::exists(marker_path)) {
      build_completed = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  ASSERT_TRUE(build_completed);
  EXPECT_EQ(ReadFile(marker_path), "callback-build");

  auto run_project = ContentRegistry::Context::run_project_workflow_callback();
  ASSERT_TRUE(run_project);
  run_project();

  EXPECT_TRUE(manager->emulator().is_snes_initialized());
  EXPECT_TRUE(manager->emulator().running());
  ASSERT_FALSE(ContentRegistry::Context::workflow_history().empty());
  EXPECT_EQ(ContentRegistry::Context::workflow_history().front().kind, "Run");
#endif  // _WIN32
}

TEST(EditorManagerProjectActionsTest,
     ProjectPanelOnlyClearsDirtyStateAfterSuccessfulSave) {
  ProjectManagementPanel panel;
  project::YazeProject project;
  project.name = "Panel Save";
  project.filepath = "panel-save.yaze";
  panel.SetProject(&project, /*dirty=*/true);

  panel.SetSaveProjectCallback(
      []() { return absl::InternalError("expected save failure"); });
  EXPECT_FALSE(panel.SaveProjectEdits().ok());
  EXPECT_TRUE(panel.IsProjectDirty());

  panel.SetSaveProjectCallback([]() { return absl::OkStatus(); });
  EXPECT_TRUE(panel.SaveProjectEdits().ok());
  EXPECT_FALSE(panel.IsProjectDirty());
}

TEST(EditorManagerProjectActionsTest,
     ProjectFileDraftStateRoundTripsWithoutClearingModification) {
  ProjectFileEditor editor;
  project::YazeProject project;
  editor.SetProject(&project);
  ASSERT_OK(editor.NewFile());
  editor.set_active(true);

  const ProjectFileEditorState draft = editor.CaptureState();
  ASSERT_TRUE(draft.initialized);
  ASSERT_TRUE(draft.modified);
  ASSERT_TRUE(draft.active);
  ASSERT_FALSE(draft.text.empty());

  editor.ResetForProject(nullptr);
  EXPECT_FALSE(editor.IsInitialized());
  EXPECT_FALSE(editor.IsModified());

  editor.RestoreState(draft, &project);
  const ProjectFileEditorState restored = editor.CaptureState();
  EXPECT_EQ(restored.filepath, draft.filepath);
  EXPECT_EQ(restored.text, draft.text);
  EXPECT_TRUE(restored.initialized);
  EXPECT_TRUE(restored.modified);
  EXPECT_TRUE(restored.active);
}

TEST(EditorManagerProjectActionsTest, ProjectFileSaveIsByteStable) {
  ScopedTempDir temp_dir(MakeTempDir("yaze_project_file_roundtrip"));
  const auto project_path = temp_dir.path / "roundtrip.yaze";
  const std::string original = "[project]\nname=Round Trip\n\n";
  {
    std::ofstream out(project_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out << original;
  }

  ProjectFileEditor editor;
  ASSERT_OK(editor.LoadFile(project_path.string()));
  ASSERT_OK(editor.SaveFile());
  EXPECT_EQ(ReadFile(project_path), original);
}

TEST(EditorManagerProjectActionsTest,
     ProjectFileValidationAcceptsSerializedProject) {
  ScopedTempDir temp_dir(MakeTempDir("yaze_project_validation"));
  const auto project_path = temp_dir.path / "serialized.yaze";
  project::YazeProject project;
  project.name = "Serialized Project";
  ASSERT_OK(project.SaveAs(project_path.string()));
  ASSERT_NE(ReadFile(project_path).find("[sprite_assets]"), std::string::npos);

  ProjectFileEditor editor;
  ASSERT_OK(editor.LoadFile(project_path.string()));
  EXPECT_TRUE(editor.CaptureState().validation_errors.empty())
      << ::testing::PrintToString(editor.CaptureState().validation_errors);
  EXPECT_FALSE(editor.IsModified());
}

TEST(EditorManagerProjectActionsTest,
     ProjectFileValidationAcceptsSerializedOptionalSections) {
  ScopedTempDir temp_dir(MakeTempDir("yaze_project_optional_validation"));
  const auto project_path = temp_dir.path / "optional.yaze";
  project::YazeProject project;
  project.name = "Optional Sections";
  project.graphics_sheets.reserved_sheets = {0x73};
  project.atlas.section_present = true;
  project.zscream_project_file = "original.zsproj";
  project.resource_labels["sprite"]["0x01"] = "Test Sprite";
  ASSERT_OK(project.SaveAs(project_path.string()));
  const std::string serialized = ReadFile(project_path);
  for (const auto* section : {"[graphics_sheets]", "[atlas]",
                              "[zscream_compatibility]", "[labels_sprite]"}) {
    ASSERT_NE(serialized.find(section), std::string::npos) << section;
  }

  ProjectFileEditor editor;
  ASSERT_OK(editor.LoadFile(project_path.string()));
  EXPECT_TRUE(editor.CaptureState().validation_errors.empty())
      << ::testing::PrintToString(editor.CaptureState().validation_errors);
  EXPECT_EQ(editor.CaptureState().text, serialized);
}

TEST(EditorManagerProjectActionsTest,
     ProjectFileValidationRetainsUnknownAndMalformedDiagnostics) {
  ScopedTempDir temp_dir(MakeTempDir("yaze_project_validation_errors"));
  const auto project_path = temp_dir.path / "invalid.yaze";
  const std::string original =
      "[workspace_settings]\nshow_grid=true\n[unknown_extension]\n"
      "enabled=true\nmissing_equals\n";
  {
    std::ofstream out(project_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out << original;
    ASSERT_TRUE(out.good());
  }

  ProjectFileEditor editor;
  ASSERT_OK(editor.LoadFile(project_path.string()));
  const std::vector<std::string> expected = {
      "Line 3: Unknown section [unknown_extension]",
      "Line 5: Invalid format, expected key=value"};
  EXPECT_EQ(editor.CaptureState().validation_errors, expected);
  EXPECT_EQ(editor.CaptureState().text, original);
}

TEST(EditorManagerProjectActionsTest,
     ProjectFileSaveAsRejectsExistingUnrelatedDescriptor) {
  ScopedTempDir temp_dir(MakeTempDir("yaze_project_file_save_as_collision"));
  const auto project_path = temp_dir.path / "existing.yaze";
  const std::string original = "unrelated project descriptor\n";
  {
    std::ofstream out(project_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out << original;
  }

  ProjectFileEditor editor;
  ASSERT_OK(editor.NewFile());
  const auto status = editor.SaveFileAs(project_path.string());

  EXPECT_EQ(status.code(), absl::StatusCode::kAlreadyExists);
  EXPECT_TRUE(editor.IsModified());
  EXPECT_TRUE(editor.filepath().empty());
  EXPECT_EQ(ReadFile(project_path), original);

  const std::string temp_prefix = project_path.filename().string() + ".tmp.";
  for (const auto& entry : std::filesystem::directory_iterator(temp_dir.path)) {
    EXPECT_NE(entry.path().filename().string().find(temp_prefix), 0u);
  }
}

TEST(EditorManagerProjectActionsTest,
     ProjectFileSaveGuardPreservesDraftOnFailure) {
  ScopedTempDir temp_dir(MakeTempDir("yaze_project_file_guard"));
  const auto project_path = temp_dir.path / "guarded.yaze";
  ProjectFileEditor editor;
  ASSERT_OK(editor.NewFile());
  editor.SetSaveGuardCallback([](const std::string&, const std::string&) {
    return absl::FailedPreconditionError("conflicting draft");
  });

  auto status = editor.SaveFileAs(project_path.string());
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(editor.IsModified());
  EXPECT_FALSE(std::filesystem::exists(project_path));
}

TEST(EditorManagerProjectActionsTest,
     ProjectDirtyStateIsSessionOwnedAndSavedBeforeSwitch) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_dirty_sessions"));
  const auto rom_a = temp_dir.path / "a.sfc";
  const auto rom_b = temp_dir.path / "b.sfc";
  const auto project_a = temp_dir.path / "a.yaze";
  WriteRomFile(rom_a, "PROJECT DIRTY A");
  WriteRomFile(rom_b, "PROJECT DIRTY B");

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  manager->SwitchToSession(0);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "Session A Project";
  project->filepath = project_a.string();
  manager->MarkCurrentProjectDirty();

  manager->SwitchToSession(1);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  EXPECT_NE(manager->GetPendingUnsavedSessionActionPrompt().find(
                "unsaved project settings"),
            std::string::npos);
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 1u);

  manager->SwitchToSession(0);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);
  EXPECT_TRUE(manager->IsCurrentProjectDirty());

  manager->SwitchToSession(1);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionSaveAndContinue();
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 1u);
  EXPECT_TRUE(std::filesystem::exists(project_a));

  manager->SwitchToSession(0);
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
}

TEST(EditorManagerProjectActionsTest,
     SaveAndContinueRefusesProjectDraftWithoutDestination) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_draft_guard"));
  const auto rom_a = temp_dir.path / "a.sfc";
  const auto rom_b = temp_dir.path / "b.sfc";
  WriteRomFile(rom_a, "PROJECT DRAFT A");
  WriteRomFile(rom_b, "PROJECT DRAFT B");

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  manager->SwitchToSession(0);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);

  auto* file_editor = manager->project_file_editor();
  ASSERT_NE(file_editor, nullptr);
  ASSERT_OK(file_editor->NewFile());
  file_editor->set_active(true);

  manager->SwitchToSession(1);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  EXPECT_NE(manager->GetPendingUnsavedSessionActionPrompt().find(
                "unsaved project-file draft"),
            std::string::npos);
  manager->ConfirmPendingUnsavedSessionActionSaveAndContinue();

  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 0u);
  EXPECT_TRUE(file_editor->IsModified());
  ASSERT_FALSE(manager->toast_manager()->GetHistory().empty());
  EXPECT_NE(manager->toast_manager()->GetHistory().front().message.find(
                "use Save As first"),
            std::string::npos);
}

TEST(EditorManagerProjectActionsTest,
     ProjectFileNewAndOpenRefuseToDiscardModifiedDraft) {
  ScopedTempDir temp_dir(MakeTempDir("yaze_project_draft_replace_guard"));
  const auto other_path = temp_dir.path / "other.yaze";
  WriteProjectFile(other_path, "Other Project", temp_dir.path / "other.sfc");

  ProjectFileEditor editor;
  ASSERT_OK(editor.NewFile());
  auto draft = editor.CaptureState();
  draft.text = "sentinel modified draft";
  draft.modified = true;
  editor.RestoreState(draft, nullptr);
  EXPECT_EQ(editor.NewFile().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(editor.LoadFile(other_path.string()).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_TRUE(editor.IsModified());
  EXPECT_TRUE(editor.filepath().empty());
  EXPECT_EQ(editor.CaptureState().text, "sentinel modified draft");
}

TEST(EditorManagerProjectActionsTest,
     RawProjectSaveRebindsSessionBeforeStructuredSave) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_raw_project_rebind"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "game.yaze";
  WriteRomFile(rom_path, "RAW REBIND ROM");
  WriteProjectFile(project_path, "Before Raw Save", rom_path);

  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  manager->ShowProjectFileEditor();
  auto* editor = manager->project_file_editor();
  auto* session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(editor, nullptr);
  ASSERT_NE(session, nullptr);
  auto* version_manager = session->version_manager.get();

  MakeModifiedProjectDraft(editor, manager->GetCurrentProject(),
                           ProjectFileContents("After Raw Save", rom_path));
  ASSERT_OK(editor->SaveFile());
  EXPECT_EQ(manager->GetCurrentProject()->name, "After Raw Save");
  ASSERT_TRUE(session->project_context.has_value());
  EXPECT_EQ(session->project_context->name, "After Raw Save");
  EXPECT_EQ(session->version_manager.get(), version_manager);

  ASSERT_OK(manager->SaveProject());
  project::YazeProject reopened;
  ASSERT_OK(reopened.Open(project_path.string()));
  EXPECT_EQ(reopened.name, "After Raw Save");
}

TEST(EditorManagerProjectActionsTest, RawProjectSaveRejectsBackingFileChanges) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_raw_project_backing_guard"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto other_rom_path = temp_dir.path / "other.sfc";
  const auto project_path = temp_dir.path / "game.yaze";
  WriteRomFile(rom_path, "RAW BACKING ROM");
  WriteRomFile(other_rom_path, "OTHER BACKING ROM");
  WriteProjectFile(project_path, "Backing Guard", rom_path);
  const std::string descriptor_before = ReadFile(project_path);

  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  manager->ShowProjectFileEditor();
  auto* editor = manager->project_file_editor();
  MakeModifiedProjectDraft(
      editor, manager->GetCurrentProject(),
      ProjectFileContents("Backing Guard", other_rom_path));

  EXPECT_EQ(editor->SaveFile().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_TRUE(editor->IsModified());
  EXPECT_EQ(ReadFile(project_path), descriptor_before);
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      manager->GetCurrentProject()->rom_filename, rom_path.string()));
}

TEST(EditorManagerProjectActionsTest,
     RawSaveAsResolvesStructuredAndRawDraftConflict) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_draft_conflict"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "game.yaze";
  const auto raw_copy_path = temp_dir.path / "raw-copy.yaze";
  WriteRomFile(rom_path, "DRAFT CONFLICT ROM");
  WriteProjectFile(project_path, "Original Project", rom_path);

  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  manager->ShowProjectFileEditor();
  auto* editor = manager->project_file_editor();
  MakeModifiedProjectDraft(editor, manager->GetCurrentProject(),
                           ProjectFileContents("Raw Draft", rom_path));
  manager->GetCurrentProject()->name = "Structured Draft";
  manager->MarkCurrentProjectDirty();

  EXPECT_EQ(editor->SaveFile().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(manager->SaveProject().code(),
            absl::StatusCode::kFailedPrecondition);
  ASSERT_OK(editor->SaveFileAs(raw_copy_path.string()));
  EXPECT_FALSE(editor->IsModified());
  ASSERT_OK(manager->SaveProject());

  project::YazeProject structured;
  project::YazeProject raw_copy;
  ASSERT_OK(structured.Open(project_path.string()));
  ASSERT_OK(raw_copy.Open(raw_copy_path.string()));
  EXPECT_EQ(structured.name, "Structured Draft");
  EXPECT_EQ(raw_copy.name, "Raw Draft");
}

TEST(EditorManagerProjectActionsTest, AutosavePersistsProjectOnlyWork) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_only_autosave"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "game.yaze";
  WriteRomFile(rom_path, "PROJECT AUTOSAVE ROM");
  WriteProjectFile(project_path, "Before Autosave", rom_path);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  manager->ShowProjectFileEditor();

  manager->GetCurrentProject()->name = "After Autosave";
  manager->MarkCurrentProjectDirty();
  ASSERT_OK(manager->AutosaveActiveSession());
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  EXPECT_NE(manager->project_file_editor()->CaptureState().text.find(
                "name=After Autosave"),
            std::string::npos);

  project::YazeProject reopened;
  ASSERT_OK(reopened.Open(project_path.string()));
  EXPECT_EQ(reopened.name, "After Autosave");
}

TEST(EditorManagerProjectActionsTest,
     DungeonRoomRenameMarksAuthoritativeProjectAndSurvivesSave) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_room_rename"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "game.yaze";
  const auto other_rom = temp_dir.path / "other.sfc";
  WriteRomFile(rom_path, "ROOM RENAME ROM");
  WriteRomFile(other_rom, "ROOM RENAME OTHER");
  WriteProjectFile(project_path, "Room Rename", rom_path);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  ASSERT_FALSE(manager->IsCurrentProjectDirty());

  auto* dungeon = manager->GetCurrentEditorSet()->GetDungeonEditor();
  ASSERT_NE(dungeon, nullptr);
  ASSERT_OK(dungeon->ApplyRoomLabelEdit(0x25, std::string("Flooded Hall")));
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_EQ(
      dungeon_project_labels::GetRoomLabel(manager->GetCurrentProject(), 0x25),
      "Flooded Hall");

  // Save Project serializes EditorManager's project, not the session copy.
  ASSERT_OK(manager->SaveProject());
  EXPECT_FALSE(manager->IsCurrentProjectDirty());
  {
    project::YazeProject reopened;
    ASSERT_OK(reopened.Open(project_path.string()));
    EXPECT_EQ(dungeon_project_labels::GetRoomLabel(&reopened, 0x25),
              "Flooded Hall");
  }

  ASSERT_OK(dungeon->ApplyRoomLabelEdit(0x25, std::nullopt));
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  ASSERT_OK(manager->SaveProject());
  {
    project::YazeProject reopened;
    ASSERT_OK(reopened.Open(project_path.string()));
    EXPECT_FALSE(dungeon_project_labels::HasCustomRoomLabel(&reopened, 0x25));
  }

  // A dungeon editor whose session is not active must not edit the project
  // that another session owns.
  ASSERT_OK(manager->OpenRomOrProject(other_rom.string()));
  const auto refused =
      dungeon->ApplyRoomLabelEdit(0x25, std::string("Wrong Session"));
  EXPECT_TRUE(absl::IsFailedPrecondition(refused)) << refused;
  EXPECT_FALSE(dungeon_project_labels::HasCustomRoomLabel(
      manager->GetCurrentProject(), 0x25));
}

TEST(EditorManagerProjectActionsTest,
     DungeonRoomRenameAndResetRefreshGlobalRoomLabel) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_room_label_refresh"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "game.yaze";
  const auto other_rom = temp_dir.path / "other.sfc";
  WriteRomFile(rom_path, "ROOM LABEL REFRESH ROM");
  WriteRomFile(other_rom, "ROOM LABEL REFRESH OTHER");
  WriteProjectFile(project_path, "Room Label Refresh", rom_path);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  ASSERT_FALSE(manager->IsCurrentProjectDirty());
  const std::string default_label = zelda3::GetRoomLabel(0x25);
  ASSERT_NE(default_label, "Lyra Native Rename");

  auto* dungeon = manager->GetCurrentEditorSet()->GetDungeonEditor();
  ASSERT_NE(dungeon, nullptr);

  // Matrix captions, search, and navigation read the global provider, so a
  // rename must be visible there without reopening the project.
  ASSERT_OK(
      dungeon->ApplyRoomLabelEdit(0x25, std::string("Lyra Native Rename")));
  EXPECT_EQ(zelda3::GetRoomLabel(0x25), "Lyra Native Rename");
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_FALSE(manager->GetCurrentRom()->dirty());

  ASSERT_OK(dungeon->ApplyRoomLabelEdit(0x25, std::nullopt));
  EXPECT_EQ(zelda3::GetRoomLabel(0x25), default_label);
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_FALSE(manager->GetCurrentRom()->dirty());

  // Opening another ROM over unsaved project work only queues a prompt.
  ASSERT_OK(manager->SaveProject());
  ASSERT_FALSE(manager->IsCurrentProjectDirty());
  const size_t project_session_id = manager->GetCurrentSessionId();
  ASSERT_OK(manager->OpenRomOrProject(other_rom.string()));
  ASSERT_NE(manager->GetCurrentSessionId(), project_session_id);
  const std::string other_session_label = zelda3::GetRoomLabel(0x25);

  const auto refused =
      dungeon->ApplyRoomLabelEdit(0x25, std::string("Wrong Session"));
  EXPECT_TRUE(absl::IsFailedPrecondition(refused)) << refused;
  EXPECT_EQ(zelda3::GetRoomLabel(0x25), other_session_label);
}

TEST(EditorManagerProjectActionsTest,
     StructuredSaveRebasesRawDraftBeforeLaterRawSave) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_structured_save_raw_rebase"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "game.yaze";
  WriteRomFile(rom_path, "STRUCTURED REBASE ROM");
  WriteProjectFile(project_path, "Before Structured Save", rom_path);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  manager->ShowProjectFileEditor();

  manager->GetCurrentProject()->name = "After Structured Save";
  manager->MarkCurrentProjectDirty();
  ASSERT_OK(manager->SaveProject());

  auto* editor = manager->project_file_editor();
  auto raw_state = editor->CaptureState();
  EXPECT_NE(raw_state.text.find("name=After Structured Save"),
            std::string::npos);
  const auto autosave_pos = raw_state.text.find("autosave_enabled=false");
  ASSERT_NE(autosave_pos, std::string::npos);
  raw_state.text.replace(autosave_pos,
                         std::string("autosave_enabled=false").size(),
                         "autosave_enabled=true");
  raw_state.modified = true;
  editor->RestoreState(raw_state, manager->GetCurrentProject());
  ASSERT_OK(editor->SaveFile());

  project::YazeProject reopened;
  ASSERT_OK(reopened.Open(project_path.string()));
  EXPECT_EQ(reopened.name, "After Structured Save");
  EXPECT_TRUE(reopened.workspace_settings.autosave_enabled);
}

TEST(EditorManagerProjectActionsTest,
     StructuredSaveAsRebasesCleanRawProjectEditor) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_structured_save_as_rebase"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto old_project_path = temp_dir.path / "old.yaze";
  const auto new_project_path = temp_dir.path / "new.yaze";
  WriteRomFile(rom_path, "SAVE AS REBASE ROM");
  WriteProjectFile(old_project_path, "Save As Project", rom_path);

  ASSERT_OK(manager->OpenRomOrProject(old_project_path.string()));
  manager->ShowProjectFileEditor();
  ASSERT_FALSE(manager->project_file_editor()->IsModified());
  manager->GetCurrentProject()->name = "Updated Save As Project";
  manager->MarkCurrentProjectDirty();
  ASSERT_OK(manager->SaveProjectAs(new_project_path.string()));

  auto* session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      manager->project_file_editor()->filepath(), new_project_path.string()));
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      session->project_file_editor_state.filepath, new_project_path.string()));
  EXPECT_NE(manager->project_file_editor()->CaptureState().text.find(
                "name=Updated Save As Project"),
            std::string::npos);
  EXPECT_FALSE(manager->project_file_editor()->IsModified());
}

TEST(EditorManagerProjectActionsTest,
     CreateNewProjectFromRomPersistsAndAdoptsSessionContext) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_creation_adoption"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "oracle-test.yaze";
  WriteRomFile(rom_path, "CREATE PROJECT ROM");

  ASSERT_OK(manager->CreateNewProjectFromRom("Vanilla ROM Hack",
                                             rom_path.string(), "Oracle Test",
                                             project_path.string()));
  ASSERT_TRUE(std::filesystem::exists(project_path));
  auto* session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->project_context.has_value());
  EXPECT_EQ(manager->GetCurrentProject()->name, "Oracle Test");
  EXPECT_EQ(session->project_context->name, "Oracle Test");
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      session->project_context->filepath, project_path.string()));
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      session->project_context->rom_filename, rom_path.string()));
  EXPECT_EQ(manager->GetVersionManager(), session->version_manager.get());

  project::YazeProject reopened;
  ASSERT_OK(reopened.Open(project_path.string()));
  EXPECT_EQ(reopened.name, "Oracle Test");
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      reopened.rom_filename, rom_path.string()));
}

// Opening a project must not run `git init`. The project file carries
// git_repository=. the way a copied Oracle of Secrets folder does, so that
// value alone must not count as opting in.
TEST(EditorManagerProjectActionsTest,
     OpenProjectWithoutGitDoesNotCreateRepository) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_open_project_no_git"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "game.yaze";
  WriteRomFile(rom_path, "NO GIT ROM");
  {
    std::ofstream out(project_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out << ProjectFileContents("No Git", rom_path)
        << "\n[build]\ngit_repository=.\ntrack_changes=true\n";
    ASSERT_TRUE(out.good());
  }
  ASSERT_FALSE(std::filesystem::exists(temp_dir.path / ".git"));

  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));

  EXPECT_FALSE(std::filesystem::exists(temp_dir.path / ".git"));
  ASSERT_NE(manager->GetVersionManager(), nullptr);
  EXPECT_FALSE(manager->GetVersionManager()->IsGitInitialized());
}

TEST(EditorManagerProjectActionsTest,
     CreateNewProjectDoesNotCreateGitRepository) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_create_project_no_git"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "created.yaze";
  WriteRomFile(rom_path, "CREATE NO GIT ROM");

  ASSERT_OK(manager->CreateNewProjectFromRom(
      "Vanilla ROM Hack", rom_path.string(), "Created", project_path.string()));

  ASSERT_TRUE(std::filesystem::exists(project_path));
  EXPECT_FALSE(std::filesystem::exists(temp_dir.path / ".git"));
  ASSERT_NE(manager->GetVersionManager(), nullptr);
  EXPECT_FALSE(manager->GetVersionManager()->IsGitInitialized());
}

// A project folder that is already a repository keeps working: open records
// it in git_repository, and the check resolves against the project directory
// rather than the test's working directory.
TEST(EditorManagerProjectActionsTest, OpenProjectAdoptsExistingGitRepository) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_open_project_existing_git"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "game.yaze";
  // A .git entry is all the adopt check reads; no git process runs.
  std::filesystem::create_directories(temp_dir.path / ".git");
  WriteRomFile(rom_path, "EXISTING GIT ROM");
  WriteProjectFile(project_path, "Existing Git", rom_path);

  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));

  EXPECT_EQ(manager->GetCurrentProject()->git_repository, ".");
  auto* session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->project_context.has_value());
  EXPECT_EQ(session->project_context->git_repository, ".");
  ASSERT_NE(manager->GetVersionManager(), nullptr);
  EXPECT_TRUE(manager->GetVersionManager()->IsGitInitialized());
}

TEST(EditorManagerProjectActionsTest,
     CreateNewProjectRoutesToGuidedDialogWithoutMutatingSession) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_guided_project_creation"));
  const auto rom_path = temp_dir.path / "game.sfc";
  WriteRomFile(rom_path, "GUIDED PROJECT ROM");
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));

  Rom* const rom = manager->GetCurrentRom();
  const size_t session_id = manager->GetCurrentSessionId();
  const size_t session_count =
      manager->session_coordinator()->GetTotalSessionCount();
  const std::string project_name = manager->GetCurrentProject()->name;
  const std::string project_path = manager->GetCurrentProject()->filepath;
  ASSERT_NE(manager->ui_coordinator(), nullptr);
  ASSERT_FALSE(manager->ui_coordinator()->IsNewProjectDialogOpen());

  ASSERT_OK(manager->CreateNewProject("ZSCustomOverworld v3"));

  EXPECT_TRUE(manager->ui_coordinator()->IsNewProjectDialogOpen());
  EXPECT_EQ(manager->GetCurrentRom(), rom);
  EXPECT_EQ(manager->GetCurrentSessionId(), session_id);
  EXPECT_EQ(manager->session_coordinator()->GetTotalSessionCount(),
            session_count);
  EXPECT_EQ(manager->GetCurrentProject()->name, project_name);
  EXPECT_EQ(manager->GetCurrentProject()->filepath, project_path);
}

TEST(EditorManagerProjectActionsTest,
     CreateNewProjectReusesAlreadyLoadedRawRomSession) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_creation_raw_reuse"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "game.yaze";
  WriteRomFile(rom_path, "CREATE REUSE ROM");
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  const size_t session_count =
      manager->session_coordinator()->GetTotalSessionCount();

  ASSERT_OK(manager->CreateNewProjectFromRom("ZSCustomOverworld v3",
                                             rom_path.string(), "Reuse Project",
                                             project_path.string()));
  EXPECT_EQ(manager->session_coordinator()->GetTotalSessionCount(),
            session_count);
  EXPECT_EQ(manager->GetCurrentProject()->name, "Reuse Project");
  EXPECT_TRUE(std::filesystem::exists(project_path));
  EXPECT_TRUE(manager->GetCurrentProject()
                  ->feature_flags.overworld.kLoadCustomOverworld);
  EXPECT_TRUE(manager->GetCurrentProject()->feature_flags.kSaveAllPalettes);
  EXPECT_TRUE(manager->GetCurrentProject()->feature_flags.kSaveGfxGroups);
}

TEST(EditorManagerProjectActionsTest,
     CreateNewProjectRollsBackAndCanRetryAfterDescriptorFinalizeFailure) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_creation_retry"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto blocker = temp_dir.path / "not_a_directory";
  const auto failed_project_path = blocker / "cannot-create.yaze";
  const auto valid_project_path = temp_dir.path / "retry-succeeded.yaze";
  WriteRomFile(rom_path, "CREATE RETRY ROM");
  {
    std::ofstream out(blocker, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out << "blocker";
  }

  const auto first_status = manager->CreateNewProjectFromRom(
      "Vanilla ROM Hack", rom_path.string(), "Retry Project",
      failed_project_path.string());
  EXPECT_FALSE(first_status.ok());
  EXPECT_FALSE(std::filesystem::exists(failed_project_path));
  EXPECT_EQ(manager->session_coordinator()->GetTotalSessionCount(), 0u);
  EXPECT_EQ(manager->GetCurrentRom(), nullptr);
  EXPECT_FALSE(manager->GetCurrentProject()->project_opened());

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  Rom* const raw_rom = manager->GetCurrentRom();
  ASSERT_NE(raw_rom, nullptr);
  EXPECT_FALSE(manager
                   ->CreateNewProjectFromRom("ZSCustomOverworld v3",
                                             rom_path.string(), "Retry Project",
                                             failed_project_path.string())
                   .ok());
  EXPECT_EQ(manager->session_coordinator()->GetTotalSessionCount(), 1u);
  EXPECT_EQ(manager->GetCurrentRom(), raw_rom);
  EXPECT_FALSE(manager->GetCurrentProject()->project_opened());

  ASSERT_OK(manager->CreateNewProjectFromRom("ZSCustomOverworld v3",
                                             rom_path.string(), "Retry Project",
                                             valid_project_path.string()));
  EXPECT_EQ(manager->session_coordinator()->GetTotalSessionCount(), 1u);
  EXPECT_TRUE(std::filesystem::exists(valid_project_path));
  EXPECT_EQ(manager->GetCurrentProject()->name, "Retry Project");
  auto* session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->project_context.has_value());
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      session->project_context->filepath, valid_project_path.string()));
  EXPECT_TRUE(session->project_context->feature_flags.kSaveAllPalettes);
}

TEST(EditorManagerProjectActionsTest,
     RomLoadOptionsProjectCreationDoesNotReusePriorProjectState) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_sequential_project_creation"));
  const auto first_rom_path = temp_dir.path / "first.sfc";
  const auto second_rom_path = temp_dir.path / "second.sfc";
  const auto first_project_path = temp_dir.path / "first.yaze";
  const auto second_project_path = temp_dir.path / "second.yaze";
  WriteRomFile(first_rom_path, "FIRST PROJECT ROM");
  WriteRomFile(second_rom_path, "SECOND PROJECT ROM");

  ASSERT_OK(manager->CreateNewProjectFromRom(
      "Vanilla ROM Hack", first_rom_path.string(), "First Project",
      first_project_path.string()));
  EXPECT_EQ(manager->GetCurrentProject()->music_persistence.storage_key,
            "First_Project_music");

  // Loading another raw ROM is the state immediately before the load-options
  // dialog confirms its optional "Create associated project" action.
  ASSERT_OK(manager->OpenRomOrProject(second_rom_path.string()));
  ASSERT_OK(manager->FinalizeNewProject("Second Project",
                                        second_project_path.string()));

  project::YazeProject reopened;
  ASSERT_OK(reopened.Open(second_project_path.string()));
  EXPECT_EQ(reopened.music_persistence.storage_key, "Second_Project_music");
  EXPECT_NE(reopened.music_persistence.storage_key, "First_Project_music");
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      reopened.rom_filename, second_rom_path.string()));
}

TEST(EditorManagerProjectActionsTest,
     FailedProjectAssetLoadRestoresInitiatingSession) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_creation_asset_rollback"));
  const auto rom_a = temp_dir.path / "a.sfc";
  const auto rom_b = temp_dir.path / "b.sfc";
  const auto invalid_rom = temp_dir.path / "invalid-assets.sfc";
  const auto project_path = temp_dir.path / "must-not-exist.yaze";
  WriteRomFile(rom_a, "ROLLBACK SESSION A");
  WriteRomFile(rom_b, "ROLLBACK SESSION B");
  WriteRomFile(invalid_rom, "ROLLBACK INVALID");

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  manager->SwitchToSession(0);
  const size_t initiating_session_id = manager->GetCurrentSessionId();
  const size_t previous_session_count =
      manager->session_coordinator()->GetTotalSessionCount();

  manager->SetAssetLoadMode(AssetLoadMode::kFull);
  const auto status =
      manager->CreateNewProjectFromRom("Vanilla ROM Hack", invalid_rom.string(),
                                       "Must Roll Back", project_path.string());

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(manager->session_coordinator()->GetTotalSessionCount(),
            previous_session_count);
  EXPECT_EQ(manager->GetCurrentSessionId(), initiating_session_id);
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 0u);
  EXPECT_FALSE(std::filesystem::exists(project_path));
}

TEST(EditorManagerProjectActionsTest,
     FailedFirstAssetLoadCanReturnToZeroAndReopen) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());

  ScopedTempDir temp_dir(MakeTempDir("yaze_first_asset_rollback"));
  const auto invalid_rom = temp_dir.path / "invalid-assets.sfc";
  const auto later_rom = temp_dir.path / "later.sfc";
  WriteRomFile(invalid_rom, "FIRST INVALID");
  WriteRomFile(later_rom, "LATER VALID");

  manager->SetAssetLoadMode(AssetLoadMode::kFull);
  EXPECT_FALSE(manager->OpenRomOrProject(invalid_rom.string()).ok());
  EXPECT_EQ(manager->session_coordinator()->GetTotalSessionCount(), 0u);

  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(manager->OpenRomOrProject(later_rom.string()));
  EXPECT_EQ(manager->session_coordinator()->GetTotalSessionCount(), 1u);
  ASSERT_NE(manager->GetCurrentRom(), nullptr);
  EXPECT_NE(manager->GetCurrentRom()->title().find("LATER VALID"),
            std::string::npos);
}

TEST(EditorManagerProjectActionsTest,
     ProjectRomReloadReplacesActiveSessionInPlace) {
  ScopedImGuiContext imgui;
  gfx::PaletteManager::Get().ResetForTesting();
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_rom_reload"));
  const auto rom_path = temp_dir.path / "game.sfc";
  const auto project_path = temp_dir.path / "game.yaze";
  WriteRomFile(rom_path, "BEFORE RELOAD");
  WriteProjectFile(project_path, "Reload Project", rom_path);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));

  Rom* const rom_address = manager->GetCurrentRom();
  auto* const game_data_address = manager->GetCurrentGameData();
  ASSERT_TRUE(gfx::PaletteManager::Get().IsSessionActive(game_data_address));
  const size_t session_count =
      manager->session_coordinator()->GetTotalSessionCount();
  manager->MarkCurrentProjectDirty();
  ASSERT_TRUE(manager->IsCurrentProjectDirty());
  WriteRomFile(rom_path, "AFTER RELOAD");
  ASSERT_OK(manager->ReloadProjectRom());
  EXPECT_EQ(manager->GetCurrentRom(), rom_address);
  EXPECT_EQ(manager->GetCurrentGameData(), game_data_address);
  EXPECT_TRUE(gfx::PaletteManager::Get().IsSessionActive(game_data_address));
  EXPECT_EQ(manager->session_coordinator()->GetTotalSessionCount(),
            session_count);
  EXPECT_NE(manager->GetCurrentRom()->title().find("AFTER RELOAD"),
            std::string::npos);
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_TRUE(manager->project_management_panel()->IsProjectDirty());

  manager.reset();
  gfx::PaletteManager::Get().ResetForTesting();
}

TEST(EditorManagerProjectActionsTest,
     ProjectRomSwapRejectsAnotherSessionsBackingFile) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_rom_swap_owner"));
  const auto rom_a = temp_dir.path / "a.sfc";
  const auto rom_b = temp_dir.path / "b.sfc";
  const auto project_path = temp_dir.path / "a.yaze";
  WriteRomFile(rom_a, "SWAP OWNER A");
  WriteRomFile(rom_b, "SWAP OWNER B");
  WriteProjectFile(project_path, "Swap Owner Project", rom_a);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  manager->SwitchToSession(0);

  const std::string descriptor_before = ReadFile(project_path);
  EXPECT_EQ(manager->SwapProjectRom(rom_b.string()).code(),
            absl::StatusCode::kAlreadyExists);
  EXPECT_EQ(ReadFile(project_path), descriptor_before);
  EXPECT_TRUE(SessionCoordinator::PathsReferToSameBackingFile(
      manager->GetCurrentProject()->rom_filename, rom_a.string()));
}

TEST(EditorManagerProjectActionsTest,
     ReopeningProjectManagementPreservesPanelDirtyState) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_project_panel_dirty_reopen"));
  const auto rom_path = temp_dir.path / "game.sfc";
  WriteRomFile(rom_path, "PANEL DIRTY ROM");
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  auto* panel = manager->project_management_panel();
  ASSERT_NE(panel, nullptr);
  panel->SetProjectDirty(true);

  manager->ShowProjectManagement();
  EXPECT_TRUE(manager->IsCurrentProjectDirty());
  EXPECT_TRUE(panel->IsProjectDirty());
}

// Opt-in because this invokes the configured external build, not a fake script.
// The full authoring folder is copied; the supplied project is never written.
TEST(OracleProjectBuildAcceptance, SavedProjectBuildsFromProductCallback) {
  const char* input = std::getenv("YAZE_TEST_PROJECT_BUILD_FILE");
  if (!input || !*input) {
    GTEST_SKIP()
        << "Set YAZE_TEST_PROJECT_BUILD_FILE to a configured disposable "
           "Oracle authoring project";
  }
  ScopedImGuiContext imgui;
  ScopedTempDir temp(MakeTempDir("yaze_project_build_acceptance"));
  const std::filesystem::path source_project(input);
  const auto copy = temp.path / "authoring";
  std::filesystem::copy(source_project.parent_path(), copy,
                        std::filesystem::copy_options::recursive);
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(
      manager->OpenRomOrProject((copy / source_project.filename()).string()));
  const auto* project = manager->GetCurrentProject();
  const auto rom_path = project->GetAbsolutePath(project->rom_filename);
  const auto output_path = project->GetAbsolutePath(project->build_target);
  ASSERT_NE(rom_path, output_path);
  ASSERT_FALSE(std::filesystem::exists(output_path));
  const auto source_bytes = ReadFile(rom_path);
  auto start = ContentRegistry::Context::start_build_workflow_callback();
  ASSERT_TRUE(start);
  start();
  ASSERT_EQ(ContentRegistry::Context::build_workflow_status().state,
            ProjectWorkflowState::kRunning)
      << ContentRegistry::Context::build_workflow_status().detail;
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::minutes(3);
  while (std::chrono::steady_clock::now() < deadline &&
         ContentRegistry::Context::build_workflow_status().state ==
             ProjectWorkflowState::kRunning) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ImGui::GetIO().DisplaySize = ImVec2(1280, 720);
    ImGui::NewFrame();
    const auto update = manager->Update();
    ImGui::Render();
    ASSERT_OK(update);
  }
  const auto status = ContentRegistry::Context::build_workflow_status();
  std::cout << ContentRegistry::Context::build_workflow_log() << std::endl;
  ASSERT_EQ(status.state, ProjectWorkflowState::kSuccess)
      << status.detail << "\n"
      << status.output_tail;
  ASSERT_TRUE(std::filesystem::exists(output_path));
  EXPECT_EQ(ReadFile(rom_path), source_bytes);
  EXPECT_GT(std::filesystem::file_size(output_path), 512u * 1024u);
  Rom compiled;
  ASSERT_OK(compiled.LoadFromFile(output_path));
  EXPECT_NE(compiled.vector(), manager->GetCurrentRom()->vector());
}

TEST(EditorManagerProjectActionsTest,
     EditorControlReportsActualIdentityAndRejectsBadProject) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir temp_dir(MakeTempDir("yaze_editor_control_identity"));
  const auto rom = temp_dir.path / "game.sfc";
  const auto project = temp_dir.path / "game.yaze";
  WriteRomFile(rom);
  WriteProjectFile(project, "Control Project", rom);
  auto opened = nlohmann::json::parse(manager->ExecuteEditorControl(
      nlohmann::json({{"command", "project.open"}, {"path", project.string()}})
          .dump()));
  ASSERT_EQ(opened["status"], "ok") << opened.dump();
  EXPECT_EQ(opened["state"]["rom_path"], rom.string());
  EXPECT_EQ(opened["state"]["project_path"], project.string());
  auto* original_rom = manager->GetCurrentRom();
  const auto original_session = manager->GetCurrentSessionId();
  const auto bad_project = temp_dir.path / "missing-rom.yaze";
  WriteProjectFile(bad_project, "Bad Project", temp_dir.path / "missing.sfc");
  auto rejected = nlohmann::json::parse(manager->ExecuteEditorControl(
      nlohmann::json(
          {{"command", "project.open"}, {"path", bad_project.string()}})
          .dump()));
  EXPECT_EQ(rejected["status"], "error");
  EXPECT_EQ(rejected["state"]["project_path"], project.string());
  EXPECT_EQ(rejected["state"]["rom_path"], rom.string());
  EXPECT_EQ(manager->GetCurrentRom(), original_rom);
  EXPECT_EQ(manager->GetCurrentSessionId(), original_session);
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
}

TEST(EditorManagerProjectActionsTest,
     EditorControlRejectsDirtyRomWithoutDiscardOrDialog) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir temp_dir(MakeTempDir("yaze_editor_control_dirty"));
  const auto rom = temp_dir.path / "game.sfc";
  WriteRomFile(rom);
  ASSERT_OK(manager->OpenRomOrProject(rom.string()));
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  const auto before = manager->GetCurrentRom()->vector();
  auto rejected = nlohmann::json::parse(manager->ExecuteEditorControl(
      nlohmann::json({{"command", "project.open"},
                      {"path", (temp_dir.path / "next.yaze").string()}})
          .dump()));
  EXPECT_EQ(rejected["http_status"], 409);
  EXPECT_EQ(rejected["state"]["unsaved"], true);
  EXPECT_EQ(manager->GetCurrentRom()->vector(), before);
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
}

TEST(EditorManagerProjectActionsTest,
     EditorControlCapturesLiveProjectDraftBeforeGuard) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ScopedTempDir temp_dir(MakeTempDir("yaze_editor_control_draft"));
  const auto rom = temp_dir.path / "game.sfc";
  const auto project = temp_dir.path / "game.yaze";
  WriteRomFile(rom);
  WriteProjectFile(project, "Original", rom);
  ASSERT_OK(manager->OpenRomOrProject(project.string()));
  manager->ShowProjectFileEditor();
  const auto draft = MakeModifiedProjectDraft(
      manager->project_file_editor(), manager->GetCurrentProject(),
      ProjectFileContents("Unsaved Draft", rom));
  auto rejected = nlohmann::json::parse(manager->ExecuteEditorControl(
      nlohmann::json({{"command", "project.open"},
                      {"path", (temp_dir.path / "next.yaze").string()}})
          .dump()));
  EXPECT_EQ(rejected["http_status"], 409);
  EXPECT_EQ(rejected["state"]["unsaved"], true);
  EXPECT_EQ(manager->project_file_editor()->CaptureState().text, draft.text);
  EXPECT_TRUE(manager->project_file_editor()->CaptureState().modified);
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
}

TEST(EditorManagerProjectActionsTest,
     EditorControlRejectsMalformedAndOutOfRangeCommands) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  for (const auto* request :
       {"{", R"({"command":"shell"})",
        R"({"command":"dungeon.select","room":296})",
        R"({"command":"dungeon.select","room":-1})",
        R"({"command":"dungeon.select","room":1.5})",
        R"({"command":"project.open","path":"relative.yaze"})"}) {
    const auto response =
        nlohmann::json::parse(manager->ExecuteEditorControl(request));
    EXPECT_EQ(response["http_status"], 400) << request;
    EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  }
}

class EditorControlRomSwapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ::yaze::test::InitializeWithIsolatedSettings(*manager_, renderer_.get());
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    WriteRomFile(rom_path_, "CURRENT ROM");
    WriteRomFile(candidate_path_, "CANDIDATE ROM");
    WriteProjectFile(project_path_, "Private Project", rom_path_);
    std::ofstream(project_path_, std::ios::app)
        << "\n[private_extension]\n# retain this comment\nunknown_key=value\n";
    ASSERT_OK(manager_->OpenRomOrProject(project_path_.string()));
    descriptor_before_ = ReadFile(project_path_);
    rom_before_ = manager_->GetCurrentRom()->vector();
  }

  nlohmann::json State() {
    return nlohmann::json::parse(
        manager_->ExecuteEditorControl(R"({"command":"status"})"))["state"];
  }
  nlohmann::json Request() {
    return {{"command", "project.rom.swap"},
            {"path", candidate_path_.string()},
            {"candidate_sha256",
             core::ComputeSourceArtifactSha256(ReadFile(candidate_path_))},
            {"expected", State()}};
  }
  nlohmann::json Execute(const nlohmann::json& request) {
    return nlohmann::json::parse(
        manager_->ExecuteEditorControl(request.dump()));
  }
  void ExpectUnchanged() {
    EXPECT_EQ(manager_->GetCurrentRom()->vector(), rom_before_);
    EXPECT_EQ(manager_->GetCurrentRom()->filename(), rom_path_.string());
    EXPECT_EQ(ReadFile(project_path_), descriptor_before_);
  }

  ScopedImGuiContext imgui_;
  ScopedTempDir temp_dir_{MakeTempDir("yaze_editor_control_swap")};
  const std::filesystem::path rom_path_ = temp_dir_.path / "current.sfc";
  const std::filesystem::path candidate_path_ =
      temp_dir_.path / "candidate.sfc";
  const std::filesystem::path project_path_ = temp_dir_.path / "private.yaze";
  std::unique_ptr<gfx::NullRenderer> renderer_ =
      std::make_unique<gfx::NullRenderer>();
  std::unique_ptr<EditorManager> manager_ = std::make_unique<EditorManager>();
  std::string descriptor_before_;
  std::vector<uint8_t> rom_before_;
};

TEST_F(EditorControlRomSwapTest, RejectsEachStaleIdentityAndMalformedArgument) {
  const auto request = Request();
  const std::vector<std::pair<std::string, nlohmann::json>> stale_fields = {
      {"pid", request["expected"]["pid"].get<int>() + 1},
      {"session_id", request["expected"]["session_id"].get<size_t>() + 1},
      {"project_path", (temp_dir_.path / "other.yaze").string()},
      {"rom_path", candidate_path_.string()},
      {"rom_sha256", std::string(64, '0')}};
  for (const auto& [field, value] : stale_fields) {
    SCOPED_TRACE(field);
    auto changed = request;
    changed["expected"][field] = value;
    const auto rejected = Execute(changed);
    EXPECT_EQ(rejected["http_status"], 409) << rejected.dump();
    EXPECT_EQ(rejected["state"], request["expected"]);
    ExpectUnchanged();
  }
  const std::vector<std::pair<std::string, nlohmann::json>> invalid_fields = {
      {"pid", true},
      {"session_id", 1.5},
      {"pid", -1},
      {"pid", 0},
      {"project_path", "relative.yaze"},
      {"rom_path", nullptr},
      {"rom_sha256", std::string(64, 'A')}};
  for (const auto& [field, value] : invalid_fields) {
    SCOPED_TRACE(field);
    auto changed = request;
    changed["expected"][field] = value;
    EXPECT_EQ(Execute(changed)["http_status"], 400);
    ExpectUnchanged();
  }
  for (const auto* field :
       {"pid", "session_id", "project_path", "rom_path", "rom_sha256"}) {
    auto changed = request;
    changed["expected"].erase(field);
    EXPECT_EQ(Execute(changed)["http_status"], 400) << field;
  }
  for (const auto* field : {"expected", "candidate_sha256", "path"}) {
    auto changed = request;
    changed.erase(field);
    EXPECT_EQ(Execute(changed)["http_status"], 400) << field;
  }
  auto changed = request;
  changed["candidate_sha256"] = "bad digest";
  EXPECT_EQ(Execute(changed)["http_status"], 400);
  changed = request;
  changed["path"] = "relative.sfc";
  EXPECT_EQ(Execute(changed)["http_status"], 400);
  ExpectUnchanged();
  EXPECT_FALSE(manager_->HasPendingUnsavedSessionAction());
}

TEST_F(EditorControlRomSwapTest, StencilRefusesBeforeTouchingTheEditor) {
  const auto stencil_path = temp_dir_.path / "stencil.json";
  const auto write_stencil = [&](const std::string& schema) {
    std::ofstream(stencil_path, std::ios::binary | std::ios::trunc)
        << nlohmann::json(
               {{"schema", schema},
                {"map_id", 0x98},
                {"cells",
                 {{{"x", 0}, {"y", 0}, {"expected", 1}, {"proposed", 2}}}}})
               .dump();
    return core::ComputeSourceArtifactSha256(ReadFile(stencil_path));
  };
  const nlohmann::json request = {
      {"command", "overworld.stencil.apply"},
      {"path", stencil_path.string()},
      {"stencil_sha256", write_stencil("yaze.overworld_tile16_stencil.v1")},
      {"expected", State()}};

  auto changed = request;
  changed["stencil_sha256"] = std::string(64, '0');
  EXPECT_EQ(Execute(changed)["http_status"], 409);
  changed = request;
  changed["expected"]["pid"] = request["expected"]["pid"].get<int>() + 1;
  EXPECT_EQ(Execute(changed)["http_status"], 409);
  changed = request;
  changed["path"] = "relative.json";
  EXPECT_EQ(Execute(changed)["http_status"], 400);
  changed = request;
  changed["path"] = (temp_dir_.path / "missing.json").string();
  EXPECT_EQ(Execute(changed)["http_status"], 422);
  changed = request;
  changed["stencil_sha256"] = write_stencil("other.schema");
  EXPECT_EQ(Execute(changed)["http_status"], 400);
  write_stencil("yaze.overworld_tile16_stencil.v1");

  manager_->ShowProjectFileEditor();
  const auto draft = MakeModifiedProjectDraft(
      manager_->project_file_editor(), manager_->GetCurrentProject(),
      ProjectFileContents("Unsaved Draft", rom_path_));
  EXPECT_EQ(Execute(request)["http_status"], 409);
  EXPECT_EQ(manager_->project_file_editor()->CaptureState().text, draft.text);
  ExpectUnchanged();
  EXPECT_FALSE(manager_->HasPendingUnsavedSessionAction());
}

TEST_F(EditorControlRomSwapTest, RejectsCandidateChangedAfterReceipt) {
  const auto request = Request();
  WriteRomFile(candidate_path_, "CHANGED AFTER RECEIPT");
  const auto rejected = Execute(request);
  EXPECT_EQ(rejected["http_status"], 409) << rejected.dump();
  EXPECT_EQ(rejected["state"], request["expected"]);
  ExpectUnchanged();
  // Status describes the loaded bytes even if their backing file has changed.
  WriteRomFile(rom_path_, "CHANGED ON DISK");
  EXPECT_EQ(State()["rom_sha256"], request["expected"]["rom_sha256"]);
}

TEST_F(EditorControlRomSwapTest, SwapsExactBytesAndReopensPreservedProject) {
  auto request = Request();
  const auto original_bytes = ReadFile(rom_path_);
  const auto candidate_bytes = ReadFile(candidate_path_);
  const auto session_id = manager_->GetCurrentSessionId();
  // Equivalent canonical identities are allowed; textual differences alone
  // must not turn a retained identity into a different editor.
  request["expected"]["project_path"] =
      (temp_dir_.path / "." / "private.yaze").string();
  request["expected"]["rom_path"] =
      (temp_dir_.path / "." / "current.sfc").string();
  const auto response = Execute(request);
  ASSERT_EQ(response["status"], "ok") << response.dump();
  EXPECT_EQ(response["state"]["rom_path"], candidate_path_.string());
  EXPECT_EQ(response["state"]["rom_sha256"], request["candidate_sha256"]);
  EXPECT_EQ(response["state"]["session_id"], session_id);
  EXPECT_EQ(response["state"]["project_path"], project_path_.string());
  EXPECT_EQ(response["state"]["unsaved"], false);
  const auto& commands = response["capabilities"]["commands"];
  EXPECT_NE(std::find(commands.begin(), commands.end(), "project.rom.swap"),
            commands.end());
  EXPECT_NE(ReadFile(project_path_).find("unknown_key=value"),
            std::string::npos);
  EXPECT_NE(ReadFile(project_path_).find("# retain this comment"),
            std::string::npos);
  EXPECT_EQ(ReadFile(rom_path_), original_bytes);
  EXPECT_EQ(ReadFile(candidate_path_), candidate_bytes);
  manager_.reset();
  manager_ = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager_, renderer_.get());
  manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(manager_->OpenRomOrProject(project_path_.string()));
  EXPECT_EQ(State()["rom_sha256"], request["candidate_sha256"]);
  EXPECT_EQ(State()["rom_path"], candidate_path_.string());
}

TEST_F(EditorControlRomSwapTest, RejectsLiveRawDraftAndPendingAction) {
  const auto request = Request();
  manager_->ShowProjectFileEditor();
  const auto draft = MakeModifiedProjectDraft(
      manager_->project_file_editor(), manager_->GetCurrentProject(),
      ProjectFileContents("Unsaved Draft", rom_path_));
  EXPECT_EQ(Execute(request)["http_status"], 409);
  EXPECT_EQ(manager_->project_file_editor()->CaptureState().text, draft.text);
  ExpectUnchanged();
  ASSERT_OK(manager_->OpenRomOrProject(candidate_path_.string()));
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  auto clean = draft;
  clean.modified = false;
  manager_->project_file_editor()->RestoreState(clean,
                                                manager_->GetCurrentProject());
  const auto rejected = Execute(request);
  EXPECT_EQ(rejected["http_status"], 409);
  EXPECT_EQ(rejected["state"]["pending_action"], true);
  EXPECT_TRUE(manager_->HasPendingUnsavedSessionAction());
  ExpectUnchanged();
}

TEST_F(EditorControlRomSwapTest, RejectsDraftInInactiveSession) {
  const auto other = temp_dir_.path / "other.sfc";
  WriteRomFile(other, "OTHER SESSION");
  ASSERT_OK(manager_->OpenRomOrProject(other.string()));
  const auto inactive_index = manager_->GetCurrentSessionIndex();
  manager_->SwitchToSession(0);
  ASSERT_EQ(manager_->GetCurrentProject()->filepath, project_path_.string());
  auto* inactive = static_cast<RomSession*>(
      manager_->session_coordinator()->GetSession(inactive_index));
  ASSERT_NE(inactive, nullptr);
  inactive->project_file_editor_state.initialized = true;
  inactive->project_file_editor_state.modified = true;
  inactive->project_file_editor_state.text = "retained inactive draft";
  const auto rejected = Execute(Request());
  EXPECT_EQ(rejected["http_status"], 409) << rejected.dump();
  EXPECT_EQ(inactive->project_file_editor_state.text,
            "retained inactive draft");
  EXPECT_TRUE(inactive->project_file_editor_state.modified);
  ExpectUnchanged();
}

TEST_F(EditorControlRomSwapTest, RejectsLyraDraftThroughRemoteAndGuiPaths) {
  const char* fixture = std::getenv("YAZE_TEST_ROM_OOS");
  if (!fixture || !*fixture)
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to the main-project canvas ROM";
  std::filesystem::copy_file(fixture, rom_path_,
                             std::filesystem::copy_options::overwrite_existing);
  ASSERT_OK(manager_->ReloadProjectRom());
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  auto* editor = manager_->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
      EditorType::kOverworld);
  ASSERT_NE(editor, nullptr);
  const auto before = manager_->GetCurrentRom()->vector();
  const auto request = Request();
  ASSERT_OK(editor->lyra_canvas_.Create(manager_->GetCurrentProject(),
                                        editor->overworld()));
  ASSERT_TRUE(editor->HasPendingProjectDraftChanges());
  ASSERT_FALSE(manager_->GetCurrentRom()->dirty());
  const auto* document = editor->lyra_canvas_.document();
  EXPECT_EQ(Execute(request)["http_status"], 409);
  EXPECT_EQ(manager_->SwapProjectRom(candidate_path_.string()).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(manager_->ReloadProjectRom().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(editor->lyra_canvas_.document(), document);
  EXPECT_TRUE(editor->HasPendingProjectDraftChanges());
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), before);
  EXPECT_EQ(ReadFile(project_path_), descriptor_before_);
  EXPECT_FALSE(std::filesystem::exists(
      manager_->GetCurrentProject()->lyra_terrain_file));
}

}  // namespace
}  // namespace yaze::editor
