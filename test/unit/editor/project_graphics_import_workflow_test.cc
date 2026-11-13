// Optional real-Working-ROM product scenario. The caller supplies a disposable
// project/ROM/source clone; this test never prepares or selects a shared fixture.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
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
  static zelda3::GameData* Data(OverworldEditor& editor) {
    return editor.game_data_;
  }
  static absl::StatusOr<ProjectGraphicsImportContext> Context(
      OverworldEditor& editor) {
    return editor.GraphicsImportContext();
  }
  static absl::StatusOr<ProjectGraphicsImportContext> NativeContext(
      OverworldEditor& editor, int id) {
    auto& ow = editor.overworld();
    const auto status = ow.EnsureMapBuilt(id);
    if (!status.ok())
      return status;
    const auto* map = ow.overworld_map(id);
    const auto size = map->area_size();
    const int width = size == zelda3::AreaSizeEnum::LargeArea ||
                              size == zelda3::AreaSizeEnum::WideArea
                          ? 64
                          : 32;
    const int height = size == zelda3::AreaSizeEnum::LargeArea ||
                               size == zelda3::AreaSizeEnum::TallArea
                           ? 64
                           : 32;
    const int parent = map->parent();
    const int ox = (parent % 8) * 32, oy = ((parent % 64) / 8) * 32;
    const auto& tiles = ow.GetMapTiles(parent / 64);
    std::vector<uint16_t> grid;
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
        grid.push_back(tiles[ox + x][oy + y]);
    return CaptureProjectGraphicsImportContext(*map, ow.tiles16(), grid, width,
                                               height);
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

std::vector<std::vector<uint16_t>> PaletteWords(const gfx::PaletteGroup& group,
                                                int except = -1) {
  std::vector<std::vector<uint16_t>> result;
  for (int p = 0; p < static_cast<int>(group.size()); ++p) {
    if (p == except)
      continue;
    std::vector<uint16_t> colors;
    const auto& palette = group.palette_ref(p);
    for (size_t c = 0; c < palette.size(); ++c)
      colors.push_back(palette[c].snes());
    result.push_back(std::move(colors));
  }
  return result;
}

std::vector<int> SpriteFields(const zelda3::Overworld& overworld) {
  std::vector<int> result;
  for (const auto& map : overworld.overworld_maps()) {
    for (int state = 0; state < 3; ++state) {
      result.push_back(map.sprite_graphics(state));
      result.push_back(map.sprite_palette(state));
    }
  }
  return result;
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
    if (const char* isolated =
            std::getenv("YAZE_BOAT_IMPORT_ISOLATED_PALETTE")) {
      ASSERT_EQ(std::string_view(isolated), "1");
      isolated_palette_ = true;
    }
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
    Open(project_, !isolated_palette_);
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

  void Open(const fs::path& project, bool open_source = true) {
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
    if (open_source) {
      status = editor_->OpenProjectGraphicsSource();
      ASSERT_TRUE(status.ok()) << status;
    }
  }

  fs::path project_, pack_;
  ImGuiContext* context_ = nullptr;
  gfx::NullRenderer renderer_;
  std::unique_ptr<EditorManager> manager_;
  OverworldEditor* editor_ = nullptr;
  bool initialized_ = false;
  bool isolated_palette_ = false;
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
  auto* data = Peer::Data(*editor_);
  ASSERT_NE(data, nullptr);
  const auto original_main = PaletteWords(data->palette_groups.overworld_main);
  const auto original_aux =
      PaletteWords(data->palette_groups.overworld_aux, 14);
  const auto original_sets = data->paletteset_ids;
  const auto original_sprites = SpriteFields(editor_->overworld());
  if (isolated_palette_) {
    // Private reservation fixture: native OW set $1D aliases model row 70.
    // Clone the existing appearance through ordinary models and SaveROM before
    // reopening the palette loader, which currently reads set bytes from ROM.
    auto appearance = Peer::NativeContext(*editor_, kParent);
    ASSERT_TRUE(appearance.ok()) << appearance.status();
    ASSERT_GE(data->palette_groups.overworld_aux.size(), 17u);
    ASSERT_EQ(original_sets[69], (std::array<uint8_t, 4>{16, 13, 6, 255}));
    std::vector<gfx::PaletteColorChange> clone;
    const auto& aux = data->palette_groups.overworld_aux;
    ASSERT_EQ(aux.palette_ref(14).size(), 21u);
    ASSERT_EQ(aux.palette_ref(16).size(), 21u);
    for (int c = 0; c < 21; ++c)
      clone.push_back(
          {"ow_aux", 14, c, aux.palette_ref(14)[c], aux.palette_ref(16)[c], 0});
    auto status = gfx::PaletteManager::Get().ApplyExternalChanges(
        data, clone, [] { return absl::OkStatus(); });
    ASSERT_TRUE(status.ok()) << status;
    data->paletteset_ids[70] = data->paletteset_ids[69];
    data->paletteset_ids[70][0] = 14;
    editor_->overworld().mutable_overworld_map(kParent)->set_area_palette(0x1D);
    status = manager_->SaveRom();
    ASSERT_TRUE(status.ok()) << status;
    status = editor_->Load();
    ASSERT_TRUE(status.ok()) << status;
    editor_->SetCurrentMap(kParent);
    auto rebound = Peer::NativeContext(*editor_, kParent);
    ASSERT_TRUE(rebound.ok()) << rebound.status();
    ASSERT_EQ(rebound->palette, appearance->palette);
    ASSERT_EQ(rebound->grid, appearance->grid);
    ASSERT_EQ(rebound->definitions, appearance->definitions);
    ASSERT_EQ(SpriteFields(editor_->overworld()), original_sprites);
    status = editor_->OpenProjectGraphicsSource();
    ASSERT_TRUE(status.ok()) << status;
    RecordProperty("private_isolated_palette_fixture", "Aux14/OWset1D/model70");
  }
  const auto* source = Peer::Source(*editor_);
  ASSERT_NE(source, nullptr);
  ASSERT_NE(source->selected_resource(), nullptr);
  const auto fixture_root = fs::weakly_canonical(source->source_root());
  ASSERT_NE(fixture_root,
            fs::path("/Users/scawful/src/hobby/oracle-of-secrets"));
  RecordProperty("private_source_fixture", fixture_root.string());
  const auto asset = LoadProjectGraphicsImport(pack_);
  ASSERT_TRUE(asset.ok()) << asset.status();
  ASSERT_EQ(asset->palettes.size(), isolated_palette_ ? 3u : 2u);
  ProjectGraphicsImportBindings binding;
  binding.target_resource = source->selected_resource()->pending_bytes;
  binding.resource_first_character = 0xC0;
  if (isolated_palette_) {
    binding.palettes = {{asset->palettes[0].id, 3, 0x100, 64},
                        {asset->palettes[1].id, 4, 0x180, 32},
                        {asset->palettes[2].id, 2, 0x140, 64}};
  } else {
    binding.palettes = {{asset->palettes[0].id, 5, 0x100, 128},
                        {asset->palettes[1].id, 6, 0x180, 32}};
  }
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
  if (isolated_palette_) {
    for (const auto& change : plan->palette_changes) {
      ASSERT_EQ(change.group_name, "ow_aux");
      ASSERT_EQ(change.palette_index, 14);
      ASSERT_TRUE((change.color_index >= 0 && change.color_index <= 3) ||
                  (change.color_index >= 7 && change.color_index <= 13));
    }
  }
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
  const auto expected_aux14 =
      PaletteWords(Peer::Data(*editor_)->palette_groups.overworld_aux);
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
  if (isolated_palette_) {
    data = Peer::Data(*editor_);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(PaletteWords(data->palette_groups.overworld_main), original_main);
    EXPECT_EQ(PaletteWords(data->palette_groups.overworld_aux, 14),
              original_aux);
    EXPECT_EQ(PaletteWords(data->palette_groups.overworld_aux), expected_aux14);
    for (size_t set = 0; set < original_sets.size(); ++set) {
      if (set != 70)
        EXPECT_EQ(data->paletteset_ids[set], original_sets[set]) << set;
    }
    EXPECT_EQ(data->paletteset_ids[70],
              (std::array<uint8_t, 4>{14, 13, 6, 255}));
    EXPECT_EQ(editor_->overworld().overworld_map(kParent)->area_palette(),
              0x1D);
    EXPECT_EQ(SpriteFields(editor_->overworld()), original_sprites);
  }
  RecordProperty("proof_scope",
                 "Private Working44f46 product "
                 "import/publish/SaveROM/SaveAs/fresh-reopen; not global "
                 "palette allocation or native gameplay");
}

}  // namespace
}  // namespace yaze::editor
