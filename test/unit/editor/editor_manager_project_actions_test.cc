#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gfx/util/palette_manager.h"
#include "testing.h"

#include "imgui/imgui.h"

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
  explicit ScopedTempDir(std::filesystem::path p) : path(std::move(p)) {
    std::filesystem::create_directories(path);
  }
  ~ScopedTempDir() {
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

TEST(EditorManagerProjectActionsTest,
     BuildCurrentProjectUsesProjectBuildScript) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
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
     BuildCurrentProjectFallsBackToManifestBuildScript) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
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
     RunCurrentProjectPrefersManifestPatchedRomAndReloadsEmulator) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
     StructuredSaveRebasesRawDraftBeforeLaterRawSave) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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

TEST(EditorManagerProjectActionsTest,
     CreateNewProjectRoutesToGuidedDialogWithoutMutatingSession) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");

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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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
  manager->Initialize(renderer.get(), "");
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

}  // namespace
}  // namespace yaze::editor
