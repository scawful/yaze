// Optional real-Working-ROM product scenario. The caller supplies a disposable
// project/ROM/source clone; this test never prepares or selects a shared fixture.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "app/editor/editor_manager.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gfx/resource/arena.h"
#include "core/features.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "editor_test_support.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/tile16_renderer.h"

namespace yaze::editor {

class ProjectGraphicsImportWorkflowTestPeer {
 public:
  static ProjectGraphicsSource* Source(OverworldEditor& editor) {
    return editor.graphics_source_.get();
  }
  static absl::StatusOr<ProjectGraphicsImportContext> Context(
      OverworldEditor& editor) {
    return editor.GraphicsImportContext();
  }
};

namespace {
namespace fs = std::filesystem;
using Peer = ProjectGraphicsImportWorkflowTestPeer;
constexpr int kParent = 0x30;
constexpr int kOwnedDefinitions = 96;

std::string Read(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

std::vector<uint16_t> StampPixels(const ProjectGraphicsImportContext& context,
                                  int x, int y, int width, int height) {
  std::vector<uint16_t> result;
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx) {
      const auto id = context.grid[(y + yy) * context.grid_width + x + xx];
      const auto pixels = zelda3::RenderTile16PixelsFromMetadata(
          context.definitions[id], context.characters);
      for (auto pixel : pixels)
        result.push_back(context.palette[pixel]);
    }
  }
  return result;
}

int UnplacedRange(zelda3::Overworld& overworld) {
  std::set<int> placed;
  for (int world = 0; world < 3; ++world) {
    for (const auto& column : overworld.GetMapTiles(world)) {
      placed.insert(column.begin(), column.end());
    }
  }
  // Preserve the authored native dock kit and its reserved FDC hole. A found
  // range is a disclosed test fixture, never a production allocation grant.
  const int end = std::min<int>(0xFC0, overworld.tiles16().size());
  for (int first = 0x100; first + kOwnedDefinitions <= end; ++first) {
    bool free = true;
    for (int i = 0; i < kOwnedDefinitions; ++i) {
      if (placed.contains(first + i)) {
        free = false;
        break;
      }
    }
    if (free)
      return first;
  }
  return -1;
}

class ProjectGraphicsImportWorkflowTest : public testing::Test {
 protected:
  void SetUp() override {
    const char* project = std::getenv("YAZE_BOAT_IMPORT_PROJECT");
    const char* pack = std::getenv("YAZE_BOAT_IMPORT_PACK");
    if (!project && !pack)
      GTEST_SKIP() << "No selected boat-import fixture";
    ASSERT_NE(project, nullptr);
    ASSERT_NE(pack, nullptr);
    ASSERT_NE(*project, '\0');
    ASSERT_NE(*pack, '\0');
    project_ = fs::absolute(project);
    pack_ = fs::absolute(pack);
    ASSERT_TRUE(fs::is_regular_file(project_));
    ASSERT_TRUE(fs::is_regular_file(pack_));
    original_flags_ = core::FeatureFlags::get();
    original_custom_ = zelda3::CustomObjectManager::Get().SnapshotState();
    original_custom_context_ =
        zelda3::CustomObjectManager::Get().active_runtime_context_id();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    initialized_ = true;
    Open(project_);
  }

  void TearDown() override {
    gfx::Arena::Get().ClearTextureQueue();
    manager_.reset();
    gfx::Arena::Get().ClearTextureQueue();
    if (initialized_) {
      core::FeatureFlags::get() = original_flags_;
      auto& custom = zelda3::CustomObjectManager::Get();
      if (original_custom_context_)
        custom.ActivateRuntimeContext(*original_custom_context_,
                                      original_custom_);
      else {
        custom.ActivateStandaloneContext();
        custom.RestoreState(original_custom_);
      }
      zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    }
    if (context_)
      ImGui::DestroyContext(context_);
    // Caller-owned fixtures and first failures are deliberately retained.
  }

  void Open(const fs::path& project) {
    editor_ = nullptr;
    gfx::Arena::Get().ClearTextureQueue();
    manager_ = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*manager_, &renderer_);
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    auto status = manager_->OpenRomOrProject(project.string());
    ASSERT_TRUE(status.ok()) << status;
    ASSERT_NE(manager_->GetCurrentProject(), nullptr);
    status = manager_->EnsureEditorAssetsLoaded(EditorType::kOverworld);
    ASSERT_TRUE(status.ok()) << status;
    editor_ = manager_->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
        EditorType::kOverworld);
    ASSERT_NE(editor_, nullptr);
    editor_->SetCurrentMap(kParent);
    status = editor_->OpenProjectGraphicsSource();
    ASSERT_TRUE(status.ok()) << status;
  }

  fs::path project_, pack_;
  ImGuiContext* context_ = nullptr;
  gfx::NullRenderer renderer_;
  std::unique_ptr<EditorManager> manager_;
  OverworldEditor* editor_ = nullptr;
  bool initialized_ = false;
  core::FeatureFlags::Flags original_flags_{};
  zelda3::CustomObjectManager::State original_custom_;
  std::optional<uint64_t> original_custom_context_;
};

TEST_F(ProjectGraphicsImportWorkflowTest,
       ImportPublishPlaceSaveAndFreshReopen) {
  ASSERT_NE(manager_, nullptr);
  const auto* rom = manager_->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  const std::string_view before(
      reinterpret_cast<const char*>(rom->vector().data()),
      rom->vector().size());
  ASSERT_EQ(rom->vector().size(), 0x400000u);
  ASSERT_EQ(core::ComputeSourceArtifactSha256(before),
            "44f46bb06f589a39275ff40d35e40a3a5652502a981205eaebb54f4c447a3ee2");
  const auto original_rom = fs::weakly_canonical(rom->filename());
  ASSERT_NE(original_rom,
            fs::path("/Users/scawful/Zelda/Projects/Oracle-of-Secrets/"
                     "Oracle-Working/oracle-working.sfc"));
  const auto destination = project_.parent_path() / "boat-import-save-as.sfc";
  const auto inspection = project_.parent_path() / "boat-import-reopen.yaze";
  ASSERT_FALSE(fs::exists(destination)) << "Refuse an existing SaveAs target";
  ASSERT_FALSE(fs::exists(inspection))
      << "Refuse an existing inspection project";
  const auto* source = Peer::Source(*editor_);
  ASSERT_NE(source, nullptr);
  ASSERT_NE(source->selected_resource(), nullptr);
  const auto fixture_root = fs::weakly_canonical(source->source_root());
  ASSERT_NE(fixture_root,
            fs::path("/Users/scawful/src/hobby/oracle-of-secrets"));
  RecordProperty("private_source_fixture", fixture_root.string());
  const auto asset = LoadProjectGraphicsImport(pack_);
  ASSERT_TRUE(asset.ok()) << asset.status();
  ASSERT_EQ(asset->palettes.size(), 2u);
  ProjectGraphicsImportBindings binding;
  binding.target_resource = source->selected_resource()->pending_bytes;
  binding.resource_first_character = 0xC0;
  binding.palettes = {{asset->palettes[0].id, 5, 0x100, 160},
                      {asset->palettes[1].id, 6, 0x1A0, 32}};
  binding.first_tile16 = UnplacedRange(editor_->overworld());
  ASSERT_GE(binding.first_tile16, 0)
      << "No unplaced96-ID fixture range; no destination was granted";
  binding.owned_tile16_count = kOwnedDefinitions;
  binding.anchor_x = 33;
  binding.anchor_y = 17;
  RecordProperty("private_fixture_tile16_first", binding.first_tile16);
  RecordProperty("private_fixture_tile16_count", kOwnedDefinitions);
  auto plan = editor_->PreviewProjectGraphicsImport(pack_.string(), binding);
  ASSERT_TRUE(plan.ok()) << "Actual source/background preflight refusal: "
                         << plan.status();
  auto status = editor_->ApplyProjectGraphicsImport(pack_.string(), binding);
  ASSERT_TRUE(status.ok()) << status;
  const int ox = (kParent % 8) * 32, oy = ((kParent % 64) / 8) * 32;
  for (int x : {33, 41}) {
    status = editor_->PlaceProjectGraphicsImport(ox + x, oy + 17);
    ASSERT_TRUE(status.ok())
        << "Actual placement(" << x << ",17) refusal: " << status;
  }
  auto snapshot = Peer::Context(*editor_);
  ASSERT_TRUE(snapshot.ok()) << snapshot.status();
  const auto first_pixels =
      StampPixels(*snapshot, 33, 17, asset->width, asset->height);
  const auto second_pixels =
      StampPixels(*snapshot, 41, 17, asset->width, asset->height);
  const auto expected_resource =
      Peer::Source(*editor_)->selected_resource()->pending_bytes;
  const auto expected_grid = snapshot->grid;
  const auto expected_definitions = snapshot->definitions;
  const auto expected_palette = snapshot->palette;
  status = editor_->PublishProjectGraphicsSource();
  ASSERT_TRUE(status.ok()) << status;
  ASSERT_FALSE(Peer::Source(*editor_)->any_dirty());
  status = manager_->SaveRom();
  ASSERT_TRUE(status.ok()) << status;
  status = manager_->SaveRomAs(destination.string());
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(Read(original_rom), Read(destination));
  // SaveAs does not retarget the original project. A separate inspection
  // descriptor changes only its ROM reference; catalog/source bindings persist.
  auto copy = *manager_->GetCurrentProject();
  copy.rom_filename = destination.string();
  status = copy.SaveAs(inspection.string());
  ASSERT_TRUE(status.ok()) << status;
  gfx::Arena::Get().ClearTextureQueue();
  manager_.reset();
  Open(inspection);
  ASSERT_FALSE(HasFatalFailure());
  ASSERT_NE(editor_, nullptr);
  auto reopened = Peer::Context(*editor_);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->grid, expected_grid);
  EXPECT_EQ(reopened->definitions, expected_definitions);
  EXPECT_EQ(reopened->palette, expected_palette);
  EXPECT_EQ(Peer::Source(*editor_)->selected_resource()->pending_bytes,
            expected_resource);
  EXPECT_EQ(StampPixels(*reopened, 33, 17, asset->width, asset->height),
            first_pixels);
  EXPECT_EQ(StampPixels(*reopened, 41, 17, asset->width, asset->height),
            second_pixels);
  RecordProperty("proof_scope",
                 "Private Working44f46 product "
                 "import/publish/SaveROM/SaveAs/fresh-reopen; not global "
                 "palette allocation or native gameplay");
}

}  // namespace
}  // namespace yaze::editor
