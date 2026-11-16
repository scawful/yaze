#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/editor/graphics/graphics_undo_actions.h"
#include "app/editor/menu/status_bar.h"
#include "app/editor/shell/feedback/popup_manager.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "cli/handlers/rom/project_bundle_verify_commands.h"
#include "core/features.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "core/gfx_sheet_source.h"
#include "core/project.h"
#include "core/project_workspace.h"
#include "editor_test_support.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "testing.h"
#include "unique_temp_path.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "util/json.h"
#include "util/rom_hash.h"

namespace yaze::editor {

class ProjectOpenResilienceTestPeer {
 public:
  static void MarkSheetModified(GraphicsEditor* editor, uint16_t sheet_id) {
    gfx::Arena::Get().set_gfx_sheets_owner(editor->game_data());
    editor->state_.MarkSheetModified(sheet_id);
  }

  static absl::Status PaintPixel(GraphicsEditor* editor, uint8_t value,
                                 uint16_t sheet_id) {
    auto& state = editor->state_;
    const auto pixels = state.game_data->sheet_store.Sheet(sheet_id);
    std::vector<uint8_t> before(pixels.begin(), pixels.end());
    auto after = before;
    after[0] = value;
    auto status = CommitSheetPixels(state, sheet_id, after);
    if (!status.ok()) {
      return status;
    }
    editor->undo_manager_.Push(std::make_unique<GraphicsPixelEditAction>(
        &state, MakeSheetPixelDiff(sheet_id, before, after),
        "Resilience source pixel"));
    return absl::OkStatus();
  }
};

namespace {

namespace fs = std::filesystem;
using ::testing::HasSubstr;

class ScopedImGuiContext {
 public:
  ScopedImGuiContext() {
    ctx_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280, 800);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  ~ScopedImGuiContext() {
    if (ctx_ != nullptr) {
      ImGui::DestroyContext(ctx_);
    }
  }

 private:
  ImGuiContext* ctx_ = nullptr;
};

class ScopedEnvVar {
 public:
  explicit ScopedEnvVar(const char* name) : name_(name) {
    if (const char* current = std::getenv(name_)) {
      saved_ = current;
    }
  }
  ~ScopedEnvVar() { Restore(); }

  void Set(const std::string& value) {
#ifdef _WIN32
    _putenv_s(name_, value.c_str());
#else
    setenv(name_, value.c_str(), 1);
#endif
  }

  void Restore() {
#ifdef _WIN32
    _putenv_s(name_, saved_.has_value() ? saved_->c_str() : "");
#else
    if (saved_.has_value()) {
      setenv(name_, saved_->c_str(), 1);
    } else {
      unsetenv(name_);
    }
#endif
  }

 private:
  const char* name_;
  std::optional<std::string> saved_;
};

std::string ReadText(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  EXPECT_TRUE(input.is_open()) << path;
  return std::string(std::istreambuf_iterator<char>(input), {});
}

void WriteText(const fs::path& path, const std::string& bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output.is_open()) << path;
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  ASSERT_TRUE(output.good()) << path;
}

void WriteBytes(const fs::path& path, const std::vector<uint8_t>& bytes) {
  WriteText(path, std::string(bytes.begin(), bytes.end()));
}

std::vector<std::string> SplitLines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream iss(text);
  std::string line;
  while (std::getline(iss, line)) {
    lines.push_back(line);
  }
  return lines;
}

class ProjectOpenResilienceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const fs::path raw_root =
        yaze::test::UniqueTempPath("yaze_project_open_resilience");
    ASSERT_TRUE(fs::create_directories(raw_root));
    root_ = fs::canonical(raw_root);

    app_data_env_.Set((root_ / "appdata").string());
    proposal_env_.Set((root_ / "proposals").string());
    sandbox_env_.Set((root_ / "sandboxes").string());

    saved_flags_ = core::FeatureFlags::get();
    ResetManager();
  }

  void TearDown() override {
    manager_.reset();
    core::FeatureFlags::get() = saved_flags_;
    gfx::Arena::Get().ClearTextureQueue();
    std::error_code ec;
    fs::remove_all(root_, ec);
  }

  void ResetManager() {
    manager_.reset();
    manager_ = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*manager_, &renderer_);
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager_->user_settings().prefs().backup_before_save = false;
  }

  struct SyntheticBundle {
    fs::path bundle_dir;
    fs::path project_path;
    fs::path rom_path;
    fs::path manifest_path;
    fs::path spec_path;
    util::RomIdentityHashes rom_hashes;
    std::string spec_sha1;
  };

  SyntheticBundle CreateSourceBundle(
      const std::string& name, const std::string& expected_hash_override,
      bool adopt_hash_on_open, const std::string& spec_sha1_override = "") {
    SyntheticBundle out;
    out.bundle_dir = root_ / (name + ".yazeproj");
    out.project_path = out.bundle_dir / "project.yaze";
    out.rom_path = out.bundle_dir / "rom";
    out.manifest_path = out.bundle_dir / "manifest.json";
    fs::create_directories(out.bundle_dir / "Data/graphics");
    fs::create_directories(out.bundle_dir / "backups");

    auto fixture = test::BuildGfxSheetTestRom();
    fixture.bytes[0x7FD9] = 1;
    const std::string title = "RESILIENCE SYNTHETIC";
    std::copy(title.begin(), title.end(), fixture.bytes.begin() + 0x7FC0);
    WriteBytes(out.rom_path, fixture.bytes);
    out.rom_hashes = util::ComputeRomIdentityHashes(fixture.bytes.data(),
                                                    fixture.bytes.size());

    WriteText(out.bundle_dir / "backups/rom_backup_1.sfc", "backup");

    Json spec = {{"schema_version", 2},
                 {"start", "0x40D000"},
                 {"capacity", "0x3000"},
                 {"encoding", "lz2"},
                 {"sheets", Json::array()}};
    const std::vector<uint8_t> pixels(4096, 2);
    for (int id : {0xDF, 0xFE}) {
      const std::string txt_name = std::to_string(id) + ".txt";
      WriteText(out.bundle_dir / "Data/graphics" / txt_name,
                core::GfxSheetSource::EncodeText(pixels));
      spec["sheets"].push_back(
          {{"global_sheet", id},
           {"source", txt_name},
           {"decoded_sha256", core::GfxSheetSource::DecodedSha256(pixels)},
           {"why", "Project open resilience fixture"}});
    }
    out.spec_path = out.bundle_dir / "Data/graphics/source_sprite_sheets.json";
    WriteText(out.spec_path, spec.dump(2) + "\n");
    out.spec_sha1 = core::GfxSheetSource::SpecSha1(ReadText(out.spec_path));

    const std::string effective_hash = expected_hash_override.empty()
                                           ? out.rom_hashes.crc32
                                           : expected_hash_override;
    const std::string effective_spec_sha1 =
        spec_sha1_override.empty() ? out.spec_sha1 : spec_sha1_override;

    WriteText(out.manifest_path,
              absl::StrFormat(
                  "{\n  \"name\": \"%s\",\n  \"romChecksum\": \"%s\"\n}\n",
                  name, std::string(40, '1')));

    WriteText(out.project_path,
              absl::StrFormat(
                  "# yaze Project File\n"
                  "# Last Modified: 2026-11-16 13:24:06\n"
                  "\n"
                  "[project]\n"
                  "name=%s\n"
                  "last_modified=2026-11-16 13:24:06\n"
                  "project_id=resilience_%s\n"
                  "\n"
                  "[files]\n"
                  "rom_filename=rom\n"
                  "rom_backup_folder=backups\n"
                  "code_folder=.\n"
                  "\n"
                  "[rom]\n"
                  "role=dev\n"
                  "expected_hash=%s\n"
                  "write_policy=block\n"
                  "adopt_hash_on_open=%s\n"
                  "\n"
                  "[graphics_sheets]\n"
                  "source_spec=Data/graphics/source_sprite_sheets.json\n"
                  "source_spec_sha1=%s\n"
                  "# scawful 2026-09-25: gfx 0x7B/0x7C must stay empty.\n"
                  "reserved_sheets=0x7B,0x7C\n",
                  name, name, effective_hash,
                  adopt_hash_on_open ? "true" : "false", effective_spec_sha1));
    return out;
  }

  ScopedImGuiContext imgui_;
  ScopedEnvVar app_data_env_{project::kAppDataDirEnv};
  ScopedEnvVar proposal_env_{project::kProposalRootEnv};
  ScopedEnvVar sandbox_env_{project::kSandboxRootEnv};
  core::FeatureFlags::Flags saved_flags_;
  gfx::NullRenderer renderer_;
  std::unique_ptr<EditorManager> manager_;
  fs::path root_;
};

TEST_F(ProjectOpenResilienceTest,
       StaleExpectedHashWithAdoptOffStagesAttentionThenAdoptsAndOpens) {
  auto b = CreateSourceBundle("StaleHash", "deadbeef",
                              /*adopt_hash_on_open=*/false);
  const std::string before_yaze = ReadText(b.project_path);

  ASSERT_OK(manager_->OpenRomOrProject(b.bundle_dir.string()));

  EXPECT_EQ(manager_->GetActiveSessionCount(), 0u);
  ASSERT_TRUE(manager_->has_pending_project_open_attention());
  EXPECT_TRUE(
      manager_->popup_manager()->IsVisible(PopupID::kProjectNeedsAttention));

  const auto& att = *manager_->pending_project_open_attention();
  EXPECT_EQ(att.kind,
            EditorManager::ProjectOpenAttention::Kind::kRomIdentityMismatch);
  EXPECT_EQ(att.rom_mismatch.expected_hash, "deadbeef");
  EXPECT_EQ(att.rom_mismatch.loaded_hash, b.rom_hashes.crc32);
  EXPECT_NE(att.rom_mismatch.rom_mtime, "unknown");
  EXPECT_THAT(att.rom_mismatch.newest_backup, HasSubstr("rom_backup_1.sfc ("));
  EXPECT_TRUE(att.can_adopt);
  EXPECT_EQ(att.adopt_rom_hash, b.rom_hashes.crc32);

  // Project file was not touched before user confirmation.
  EXPECT_EQ(ReadText(b.project_path), before_yaze);

  ASSERT_OK(manager_->AdoptPendingProjectOpenAttention());

  EXPECT_FALSE(manager_->has_pending_project_open_attention());
  EXPECT_FALSE(
      manager_->popup_manager()->IsVisible(PopupID::kProjectNeedsAttention));
  EXPECT_EQ(manager_->GetActiveSessionCount(), 1u);
  EXPECT_EQ(manager_->GetCurrentProject()->rom_metadata.expected_hash,
            b.rom_hashes.crc32);

  // Verify ONLY expected_hash changed in project.yaze, preserving comments and
  // last_modified timestamps verbatim.
  const auto before_lines = SplitLines(before_yaze);
  const auto after_lines = SplitLines(ReadText(b.project_path));
  ASSERT_EQ(before_lines.size(), after_lines.size());
  int diff_count = 0;
  for (size_t i = 0; i < before_lines.size(); ++i) {
    if (before_lines[i] != after_lines[i]) {
      ++diff_count;
      EXPECT_EQ(before_lines[i], "expected_hash=deadbeef");
      EXPECT_EQ(after_lines[i], "expected_hash=" + b.rom_hashes.crc32);
    }
  }
  EXPECT_EQ(diff_count, 1);

  // Bundle manifest romChecksum was also synced to the ROM SHA-1.
  const auto manifest_json = Json::parse(ReadText(b.manifest_path));
  EXPECT_EQ(manifest_json.at("romChecksum"), b.rom_hashes.sha1);
}

TEST_F(ProjectOpenResilienceTest,
       CancelAttentionLeavesClosedWithToastAndStatusBarReason) {
  auto b = CreateSourceBundle("CancelHash", "00000000",
                              /*adopt_hash_on_open=*/false);
  const std::string before_yaze = ReadText(b.project_path);

  ASSERT_OK(manager_->OpenRomOrProject(b.project_path.string()));
  ASSERT_TRUE(manager_->has_pending_project_open_attention());
  EXPECT_EQ(manager_->GetActiveSessionCount(), 0u);

  manager_->CancelPendingProjectOpenAttention();

  EXPECT_FALSE(manager_->has_pending_project_open_attention());
  EXPECT_FALSE(
      manager_->popup_manager()->IsVisible(PopupID::kProjectNeedsAttention));
  EXPECT_EQ(manager_->GetActiveSessionCount(), 0u);
  EXPECT_EQ(ReadText(b.project_path), before_yaze);

  EXPECT_TRUE(manager_->status_bar()->has_notification_for_test());
  EXPECT_THAT(manager_->status_bar()->notification_for_test(),
              HasSubstr("expected_hash=00000000"));
  ASSERT_FALSE(manager_->toast_manager()->GetHistory().empty());
  EXPECT_THAT(manager_->toast_manager()->GetHistory().front().message,
              HasSubstr("expected_hash=00000000"));
}

TEST_F(ProjectOpenResilienceTest,
       AdoptHashOnOpenTrueAndNonInteractiveOpenStayUnchanged) {
  auto auto_bundle = CreateSourceBundle("AutoAdopt", std::string(40, '2'),
                                        /*adopt_hash_on_open=*/true);
  ASSERT_OK(manager_->OpenRomOrProject(auto_bundle.project_path.string()));
  EXPECT_FALSE(manager_->has_pending_project_open_attention());
  EXPECT_FALSE(
      manager_->popup_manager()->IsVisible(PopupID::kProjectNeedsAttention));
  EXPECT_EQ(manager_->GetActiveSessionCount(), 1u);
  EXPECT_EQ(manager_->GetCurrentProject()->rom_metadata.expected_hash,
            auto_bundle.rom_hashes.sha1);

  ResetManager();
  auto manual_bundle = CreateSourceBundle("ManualControl", "deadbeef",
                                          /*adopt_hash_on_open=*/false);
  const std::string before_yaze = ReadText(manual_bundle.project_path);
  const auto response = Json::parse(manager_->ExecuteEditorControl(
      Json({{"command", "project.open"},
            {"path", manual_bundle.project_path.string()}})
          .dump()));
  EXPECT_EQ(response.at("status"), "error");
  EXPECT_EQ(response.at("http_status"), 422);
  EXPECT_THAT(response.at("error").get<std::string>(),
              HasSubstr("expected_hash=deadbeef"));
  EXPECT_THAT(response.at("error").get<std::string>(),
              HasSubstr("loaded_hash=" + manual_bundle.rom_hashes.crc32));
  EXPECT_THAT(response.at("error").get<std::string>(), HasSubstr("rom_mtime="));
  EXPECT_THAT(response.at("error").get<std::string>(),
              HasSubstr("newest_backup=rom_backup_1.sfc ("));
  EXPECT_FALSE(manager_->has_pending_project_open_attention());
  EXPECT_EQ(ReadText(manual_bundle.project_path), before_yaze);
}

TEST_F(ProjectOpenResilienceTest,
       StaleSpecSha1WithValidSpecAdoptsOnlyThatKeyAndOpens) {
  const std::string stale_sha1(40, '0');
  auto b = CreateSourceBundle("StaleSpec", /*expected_hash_override=*/"",
                              /*adopt_hash_on_open=*/false, stale_sha1);
  const std::string before_yaze = ReadText(b.project_path);

  ASSERT_OK(manager_->OpenRomOrProject(b.bundle_dir.string()));

  EXPECT_EQ(manager_->GetActiveSessionCount(), 0u);
  ASSERT_TRUE(manager_->has_pending_project_open_attention());
  EXPECT_TRUE(
      manager_->popup_manager()->IsVisible(PopupID::kProjectNeedsAttention));

  const auto& att = *manager_->pending_project_open_attention();
  EXPECT_EQ(
      att.kind,
      EditorManager::ProjectOpenAttention::Kind::kGraphicsSpecSha1Mismatch);
  EXPECT_EQ(att.spec_mismatch.recorded_sha1, stale_sha1);
  EXPECT_EQ(att.spec_mismatch.on_disk_sha1, b.spec_sha1);
  EXPECT_EQ(att.spec_mismatch.spec_path, b.spec_path.string());
  EXPECT_TRUE(att.spec_mismatch.spec_valid);
  EXPECT_TRUE(att.can_adopt);
  EXPECT_EQ(ReadText(b.project_path), before_yaze);

  ASSERT_OK(manager_->AdoptPendingProjectOpenAttention());

  EXPECT_FALSE(manager_->has_pending_project_open_attention());
  EXPECT_EQ(manager_->GetActiveSessionCount(), 1u);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* gfx = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
  ASSERT_NE(gfx, nullptr);
  EXPECT_EQ(gfx->game_data()->sheet_store.Sheet(0xFE)[0], 2);

  // Verify ONLY source_spec_sha1 changed in project.yaze.
  const auto before_lines = SplitLines(before_yaze);
  const auto after_lines = SplitLines(ReadText(b.project_path));
  ASSERT_EQ(before_lines.size(), after_lines.size());
  int diff_count = 0;
  for (size_t i = 0; i < before_lines.size(); ++i) {
    if (before_lines[i] != after_lines[i]) {
      ++diff_count;
      EXPECT_EQ(before_lines[i], "source_spec_sha1=" + stale_sha1);
      EXPECT_EQ(after_lines[i], "source_spec_sha1=" + b.spec_sha1);
    }
  }
  EXPECT_EQ(diff_count, 1);
}

TEST_F(ProjectOpenResilienceTest,
       StaleSpecSha1RefusesInvalidSpecMalformedJsonAndOpenSourceDraft) {
  // 1. Corrupted decoded_sha256 in the on-disk spec -> refused before Adopt.
  {
    const std::string stale_sha1(40, '0');
    auto b = CreateSourceBundle("BadSpecDigest", "", false, stale_sha1);
    auto spec_json = Json::parse(ReadText(b.spec_path));
    spec_json["sheets"][0]["decoded_sha256"] = std::string(64, 'f');
    WriteText(b.spec_path, spec_json.dump(2) + "\n");
    const std::string before_yaze = ReadText(b.project_path);

    ASSERT_OK(manager_->OpenRomOrProject(b.project_path.string()));
    ASSERT_TRUE(manager_->has_pending_project_open_attention());
    const auto& att = *manager_->pending_project_open_attention();
    EXPECT_FALSE(att.spec_mismatch.spec_valid);
    EXPECT_FALSE(att.can_adopt);
    EXPECT_FALSE(att.adopt_disabled_reason.empty());

    EXPECT_EQ(manager_->AdoptPendingProjectOpenAttention().code(),
              absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(ReadText(b.project_path), before_yaze);
    manager_->CancelPendingProjectOpenAttention();
    EXPECT_EQ(manager_->GetActiveSessionCount(), 0u);
  }

  // 2. Malformed JSON spec on disk -> refused cleanly.
  {
    ResetManager();
    const std::string stale_sha1(40, '0');
    auto b = CreateSourceBundle("MalformedSpec", "", false, stale_sha1);
    WriteText(b.spec_path, "{invalid json");
    const std::string before_yaze = ReadText(b.project_path);

    ASSERT_OK(manager_->OpenRomOrProject(b.project_path.string()));
    ASSERT_TRUE(manager_->has_pending_project_open_attention());
    EXPECT_FALSE(manager_->pending_project_open_attention()->can_adopt);
    EXPECT_FALSE(manager_->pending_project_open_attention()
                     ->adopt_disabled_reason.empty());
    EXPECT_EQ(manager_->AdoptPendingProjectOpenAttention().code(),
              absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(ReadText(b.project_path), before_yaze);
    manager_->CancelPendingProjectOpenAttention();
  }

  // 3. Another open session on the same spec holds an unsaved source draft ->
  //    adopting spec SHA-1 is refused so draft edits aren't orphaned.
  {
    ResetManager();
    auto session_a = CreateSourceBundle("SharedSpecA", "", false);
    ASSERT_OK(manager_->OpenRomOrProject(session_a.project_path.string()));
    ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
    auto* gfx_a = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
    ASSERT_NE(gfx_a, nullptr);

    // Open a second clean project session so MaybeGuardPendingSessionAction
    // does not prompt on the active session when opening peer_proj.
    const fs::path plain_rom = root_ / "plain_clean.sfc";
    WriteBytes(plain_rom, test::BuildGfxSheetTestRom().bytes);
    const fs::path plain_proj = root_ / "PlainClean.yaze";
    WriteText(plain_proj,
              "[project]\nname=PlainClean\nproject_id=plain_clean_1\n\n"
              "[files]\nrom_filename=plain_clean.sfc\n\n[rom]\nrole=dev\n");
    ASSERT_OK(manager_->OpenRomOrProject(plain_proj.string()));
    ASSERT_EQ(manager_->GetActiveSessionCount(), 2u);

    ProjectOpenResilienceTestPeer::MarkSheetModified(gfx_a, 0xFE);
    ASSERT_TRUE(gfx_a->HasPendingSourceChanges());

    const fs::path peer_dir = root_ / "peer_dir";
    ASSERT_TRUE(fs::create_directories(peer_dir));
    const fs::path peer_rom = peer_dir / "peer_rom.sfc";
    fs::copy_file(session_a.rom_path, peer_rom,
                  fs::copy_options::overwrite_existing);
    const fs::path peer_proj = peer_dir / "peer_stale_spec.yaze";
    std::string peer_text = ReadText(session_a.project_path);
    const auto rom_pos = peer_text.find("rom_filename=rom");
    ASSERT_NE(rom_pos, std::string::npos);
    peer_text.replace(rom_pos, std::string("rom_filename=rom").size(),
                      "rom_filename=peer_rom.sfc");
    const auto code_pos = peer_text.find("code_folder=.");
    ASSERT_NE(code_pos, std::string::npos);
    peer_text.replace(code_pos, std::string("code_folder=.").size(),
                      "code_folder=" + session_a.bundle_dir.string());
    peer_text.replace(peer_text.find(session_a.spec_sha1),
                      session_a.spec_sha1.size(), std::string(40, '0'));
    WriteText(peer_proj, peer_text);
    const std::string before_peer = ReadText(peer_proj);

    ASSERT_OK(manager_->OpenRomOrProject(peer_proj.string()));
    ASSERT_TRUE(manager_->has_pending_project_open_attention());
    const auto& att = *manager_->pending_project_open_attention();
    EXPECT_FALSE(att.can_adopt);
    EXPECT_THAT(att.adopt_disabled_reason,
                HasSubstr("unsaved graphics source draft"));
    EXPECT_EQ(manager_->AdoptPendingProjectOpenAttention().code(),
              absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(ReadText(peer_proj), before_peer);
    manager_->CancelPendingProjectOpenAttention();
  }
}

TEST_F(
    ProjectOpenResilienceTest,
    DescriptorSaveGuardRefusesExternalOverwriteAndHandlesReloadOverwriteCancel) {
  // 1. Non-source project: YazeProject::Save and SaveProject refuse external
  //    on-disk edits, support Cancel / Overwrite anyway (confirm) / Reload.
  {
    const fs::path plain_rom = root_ / "plain.sfc";
    auto fixture = test::BuildGfxSheetTestRom();
    WriteBytes(plain_rom, fixture.bytes);
    const fs::path proj_path = root_ / "PlainGuard.yaze";
    WriteText(proj_path,
              "[project]\n"
              "name=PlainGuard\n"
              "project_id=plain_guard_1\n\n"
              "[files]\n"
              "rom_filename=plain.sfc\n\n"
              "[rom]\n"
              "role=dev\n");

    // Direct YazeProject::Save guard.
    project::YazeProject direct;
    ASSERT_OK(direct.Open(proj_path.string()));
    const std::string external_direct =
        ReadText(proj_path) + "\n# external agent edit\n[notes]\ntag=keep\n";
    WriteText(proj_path, external_direct);
    direct.metadata.description = "stale memory write";
    EXPECT_EQ(direct.Save().code(), absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(ReadText(proj_path), external_direct);

    // EditorManager::SaveProject guard + modal actions.
    WriteText(proj_path,
              "[project]\n"
              "name=PlainGuard\n"
              "project_id=plain_guard_1\n\n"
              "[files]\n"
              "rom_filename=plain.sfc\n\n"
              "[rom]\n"
              "role=dev\n");
    ASSERT_OK(manager_->OpenRomOrProject(proj_path.string()));

    const std::string external_text =
        "[project]\n"
        "name=ExternalRenamed\n"
        "project_id=plain_guard_1\n\n"
        "[files]\n"
        "rom_filename=plain.sfc\n\n"
        "[rom]\n"
        "role=dev\n"
        "# external marker\n";
    WriteText(proj_path, external_text);

    // SaveProject must refuse and open kProjectDescriptorChanged without
    // touching disk.
    EXPECT_EQ(manager_->SaveProject().code(),
              absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(ReadText(proj_path), external_text);
    ASSERT_TRUE(manager_->has_pending_descriptor_save_conflict());
    EXPECT_TRUE(manager_->popup_manager()->IsVisible(
        PopupID::kProjectDescriptorChanged));

    // Cancel keeps disk untouched and records status bar + toast notification.
    manager_->CancelDescriptorConflict();
    EXPECT_FALSE(manager_->has_pending_descriptor_save_conflict());
    EXPECT_FALSE(manager_->popup_manager()->IsVisible(
        PopupID::kProjectDescriptorChanged));
    EXPECT_EQ(ReadText(proj_path), external_text);
    EXPECT_TRUE(manager_->status_bar()->has_notification_for_test());
    EXPECT_THAT(manager_->status_bar()->notification_for_test(),
                HasSubstr("project.yaze changed on disk"));

    // Dirty ROM blocks Reload project; SaveProject stages conflict with
    // can_reload == false.
    ASSERT_OK(manager_->GetCurrentRom()->WriteByte(0x200, 0x55));
    EXPECT_EQ(manager_->SaveProject().code(),
              absl::StatusCode::kFailedPrecondition);
    ASSERT_TRUE(manager_->has_pending_descriptor_save_conflict());
    EXPECT_FALSE(manager_->pending_descriptor_save_conflict()->can_reload);
    EXPECT_FALSE(manager_->pending_descriptor_save_conflict()
                     ->reload_disabled_reason.empty());
    EXPECT_EQ(manager_->ReloadProjectAfterDescriptorConflict().code(),
              absl::StatusCode::kFailedPrecondition);

    // Two-step overwrite confirm: Confirm fails before Request, succeeds after.
    EXPECT_EQ(manager_->ConfirmOverwriteDescriptorConflict().code(),
              absl::StatusCode::kFailedPrecondition);
    manager_->RequestOverwriteDescriptorConflict();
    ASSERT_TRUE(
        manager_->pending_descriptor_save_conflict()->confirming_overwrite);
    manager_->GetCurrentProject()->metadata.description = "confirmed overwrite";
    ASSERT_OK(manager_->ConfirmOverwriteDescriptorConflict());
    EXPECT_FALSE(manager_->has_pending_descriptor_save_conflict());
    EXPECT_THAT(ReadText(proj_path),
                HasSubstr("description=confirmed overwrite"));

    // Clear dirty ROM bit via SaveRom? Easier: reopen fresh on clean session
    // to test ReloadProjectAfterDescriptorConflict when clean!
    manager_->CancelDescriptorConflict();
  }

  // 2. Clean GfxSheetSource lazy project: SaveProject refuses external
  //    edit and ReloadProjectAfterDescriptorConflict loads the new disk state!
  {
    ResetManager();
    auto b = CreateSourceBundle("LazyReload", "", false);
    ASSERT_OK(manager_->OpenRomOrProject(b.project_path.string()));

    std::string updated_disk = ReadText(b.project_path);
    const auto name_pos = updated_disk.find("name=LazyReload");
    ASSERT_NE(name_pos, std::string::npos);
    updated_disk.replace(name_pos, std::string("name=LazyReload").size(),
                         "name=DiskUpdatedName");
    WriteText(b.project_path, updated_disk);

    // Even in lazy mode before GraphicsEditor materializes, SaveProject must
    // NOT re-read updated_disk and overwrite it!
    EXPECT_EQ(manager_->SaveProject().code(),
              absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(ReadText(b.project_path), updated_disk);
    ASSERT_TRUE(manager_->has_pending_descriptor_save_conflict());
    EXPECT_TRUE(manager_->pending_descriptor_save_conflict()->can_reload);

    ASSERT_OK(manager_->ReloadProjectAfterDescriptorConflict());
    EXPECT_FALSE(manager_->has_pending_descriptor_save_conflict());
    EXPECT_EQ(manager_->GetCurrentProject()->name, "DiskUpdatedName");
  }

  // 3. Pending graphics source draft keeps existing project.cc guard behavior
  //    ("Project descriptor changed on disk; source draft retained").
  {
    ResetManager();
    auto b = CreateSourceBundle("SourceDraftGuard", "", false);
    ASSERT_OK(manager_->OpenRomOrProject(b.project_path.string()));
    ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
    auto* gfx = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
    ASSERT_NE(gfx, nullptr);
    ASSERT_OK(ProjectOpenResilienceTestPeer::PaintPixel(gfx, 6, 0xFE));

    const std::string external_disk =
        ReadText(b.project_path) + "\n# external source-draft guard\n";
    WriteText(b.project_path, external_disk);

    const absl::Status status = manager_->SaveProject();
    EXPECT_FALSE(status.ok());
    EXPECT_THAT(
        std::string(status.message()),
        HasSubstr("Project descriptor changed on disk; source draft retained"));
    EXPECT_FALSE(manager_->has_pending_descriptor_save_conflict());
    EXPECT_FALSE(manager_->popup_manager()->IsVisible(
        PopupID::kProjectDescriptorChanged));
    EXPECT_EQ(ReadText(b.project_path), external_disk);
  }
}

TEST_F(ProjectOpenResilienceTest,
       CliParityHandlersReportMismatchFieldsWithoutModifyingProject) {
  const std::string stale_sha1(40, '0');
  auto b = CreateSourceBundle("CliParity", "deadbeef",
                              /*adopt_hash_on_open=*/false, stale_sha1);
  const std::string before_yaze = ReadText(b.project_path);

  project::YazeProject proj;
  ASSERT_OK(proj.Open(b.project_path.string()));
  Rom rom;
  ASSERT_OK(rom.LoadFromFile(b.rom_path.string()));

  const auto layout_status = core::ResolveGfxSheetLayout(rom, proj).status();
  ASSERT_FALSE(layout_status.ok());
  EXPECT_THAT(std::string(layout_status.message()),
              HasSubstr("expected_hash=deadbeef"));
  EXPECT_THAT(std::string(layout_status.message()),
              HasSubstr("loaded_hash=" + b.rom_hashes.crc32));
  EXPECT_THAT(std::string(layout_status.message()), HasSubstr("rom_mtime="));
  EXPECT_THAT(std::string(layout_status.message()),
              HasSubstr("newest_backup=rom_backup_1.sfc ("));

  const auto source_status = core::GfxSheetSource::Load(proj).status();
  ASSERT_FALSE(source_status.ok());
  EXPECT_THAT(std::string(source_status.message()),
              HasSubstr("recorded_sha1=" + stale_sha1));
  EXPECT_THAT(std::string(source_status.message()),
              HasSubstr("on_disk_sha1=" + b.spec_sha1));
  EXPECT_THAT(std::string(source_status.message()),
              HasSubstr("spec_path=" + b.spec_path.string()));

  cli::handlers::ProjectBundleVerifyCommandHandler verify_handler;
  std::string out;
  const absl::Status verify_status =
      verify_handler.Run({"--project=" + b.project_path.string(),
                          "--check-rom-hash", "--format=json"},
                         nullptr, &out);
  EXPECT_FALSE(verify_status.ok());
  const auto doc = Json::parse(out, nullptr, false);
  ASSERT_FALSE(doc.is_discarded()) << out;
  bool found = false;
  const auto& checks = doc.contains("Project Bundle Verify")
                           ? doc.at("Project Bundle Verify").at("checks")
                           : doc.at("checks");
  for (const auto& chk : checks) {
    if (chk.value("name", "") == "rom_hash_check" &&
        chk.value("status", "") == "fail") {
      found = true;
      EXPECT_THAT(chk.value("detail", ""), HasSubstr("rom_mtime="));
      EXPECT_THAT(chk.value("detail", ""),
                  HasSubstr("newest_backup=rom_backup_1.sfc ("));
    }
  }
  EXPECT_TRUE(found) << out;
  EXPECT_EQ(ReadText(b.project_path), before_yaze);
}

}  // namespace
}  // namespace yaze::editor
