#include "app/editor/sprite/sprite_editor.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "app/editor/sprite/sprite_authoring.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "core/sprite_asset_json.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {
namespace fs = std::filesystem;

class SpriteCatalogSessionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = fs::temp_directory_path() /
            ("yaze_sprite_session_" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root_ / "source/Sprites/NPCs");
    const auto fixture = fs::path(__FILE__)
                             .parent_path()
                             .parent_path()
                             .parent_path()
                             .parent_path() /
                         "assets/sprite_catalogs/oracle_f0.json";
    fs::copy_file(fixture, root_ / "catalog.json");
    std::ofstream(root_ / "source/Sprites/NPCs/maple.asm")
        << "; fixture\nMapleHandler:\nRTL\n";
    project_.filepath = (root_ / "project.yaze").string();
    project_.sprite_catalog_file = "catalog.json";
    project_.sprite_source_root = "source";
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }
  fs::path root_;
  project::YazeProject project_;
};

TEST_F(SpriteCatalogSessionTest,
       ImportedDrawCopySupportsClipboardUndoWithoutSourceWrites) {
  const std::string content =
      "Test_Draw:\n{\n.start_index\ndb 0\n.nbr_of_tiles\ndb 0\n"
      ".x_offsets\ndw -8\n.y_offsets\ndw -16\n.chr\ndb $10\n"
      ".properties\ndb $39\n.sizes\ndb 2\n}\n";
  const auto path = root_ / "source/Sprites/NPCs/mermaid.asm";
  std::ofstream(path) << content;
  EditorDependencies deps;
  deps.project = &project_;
  SpriteEditor editor;
  editor.SetDependencies(deps);
  ASSERT_TRUE(
      editor
          .ImportCatalogDraw({"draw", "Sprites/NPCs/mermaid.asm", "Test_Draw"})
          .ok());
  ASSERT_NE(editor.current_custom_sprite(), nullptr);
  EXPECT_EQ(editor.current_custom_sprite()->editor.Frames.size(), 1);
  ASSERT_TRUE(editor.Copy().ok());
  ASSERT_TRUE(editor.Paste().ok());
  EXPECT_EQ(editor.current_custom_sprite()->editor.Frames.size(), 2);
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_EQ(editor.current_custom_sprite()->editor.Frames.size(), 1);
  ASSERT_TRUE(editor.Redo().ok());
  EXPECT_EQ(editor.current_custom_sprite()->editor.Frames.size(), 2);
  ASSERT_TRUE(editor.Cut().ok());
  EXPECT_EQ(editor.current_custom_sprite()->editor.Frames.size(), 1);
  EXPECT_FALSE(editor.Cut().ok());
  SpriteEditor independent;
  independent.SetDependencies(deps);
  EXPECT_FALSE(independent.Paste().ok());
  EXPECT_FALSE(
      editor.ImportCatalogDraw({"draw", "Sprites/NPCs/mermaid.asm", "Missing"})
          .ok());
  EXPECT_EQ(editor.current_custom_sprite()->editor.Frames.size(), 1);
  std::ifstream stream(path);
  EXPECT_EQ(std::string(std::istreambuf_iterator<char>(stream), {}), content);
}

TEST_F(SpriteCatalogSessionTest,
       AssetBindingsSurviveProjectReopenAndDetectSourceDrift) {
  zsprite::ZSprite fixture;
  fixture.Reset();
  fixture.editor.Frames.resize(1);
  fixture.editor.Frames[0].Tiles.emplace_back(128, 112, false, false, 0x13, 4,
                                              true, 3);
  auto tables = sprite_authoring::ExportDrawTables(fixture);
  ASSERT_TRUE(tables.ok());
  const auto source_path = root_ / "source/Sprites/NPCs/mermaid.asm";
  const std::string source = "Test_Draw:\n{\n" + *tables + "}\n";
  std::ofstream(source_path, std::ios::binary) << source;
  EditorDependencies deps;
  deps.project = &project_;
  SpriteEditor editor;
  editor.SetDependencies(deps);
  ASSERT_TRUE(
      editor
          .ImportCatalogDraw({"draw", "Sprites/NPCs/mermaid.asm", "Test_Draw"},
                             "oracle.test")
          .ok());
  ASSERT_TRUE(editor.CheckCurrentSpriteSource().ok());
  const auto original_hash = core::ComputeSourceArtifactSha256(source);
  EXPECT_EQ(editor.current_sprite_binding()->source_sha256, original_hash);
  auto binding = *editor.current_sprite_binding();
  binding.sheets = {1, 2, 3, 4, 5, 6, 7, 8};
  binding.palette_rows[4] = {"sprites_aux2", 7};
  ASSERT_TRUE(
      editor.SetSpriteGraphics(binding.sheets, binding.palette_rows).ok());
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_NE(editor.current_sprite_binding()->sheets, binding.sheets);
  ASSERT_TRUE(editor.Redo().ok());
  const auto asset_path = root_ / "test sprite.zsm";
  ASSERT_TRUE(editor.SaveSpriteAsset(asset_path.string()).ok());
  ASSERT_EQ(project_.sprite_assets.size(), 1);
  ASSERT_TRUE(project_.Save().ok());
  project::YazeProject reopened;
  ASSERT_TRUE(reopened.Open(project_.filepath).ok());
  ASSERT_EQ(reopened.sprite_assets.size(), 1);
  EXPECT_EQ(reopened.sprite_assets[0].zsm_path, asset_path.string());
  EXPECT_EQ(reopened.sprite_assets[0].catalog_key, "oracle.test");
  SpriteEditor next;
  deps.project = &reopened;
  next.SetDependencies(deps);
  ASSERT_TRUE(next.Load().ok());
  ASSERT_NE(next.current_custom_sprite(), nullptr);
  ASSERT_NE(next.current_sprite_binding(), nullptr);
  EXPECT_EQ(next.current_sprite_binding()->sheets, binding.sheets);
  EXPECT_EQ(next.current_sprite_binding()->palette_rows[4].group,
            "sprites_aux2");
  EXPECT_EQ(next.current_sprite_binding()->palette_rows[4].index, 7);
  ASSERT_TRUE(next.CheckCurrentSpriteSource().ok());
  EXPECT_TRUE(next.ExportCurrentSpriteDraw().ok());
  std::ofstream(source_path, std::ios::app) << "; source changed\n";
  EXPECT_FALSE(next.ExportCurrentSpriteDraw().ok());
  EXPECT_FALSE(next.CheckCurrentSpriteSource().ok());
  EXPECT_EQ(next.current_sprite_binding()->source_sha256, original_hash);
  EXPECT_EQ(next.current_custom_sprite()->editor.Frames[0].Tiles[0].id, 0x13);
  ASSERT_TRUE(next.SaveSpriteAsset(asset_path.string()).ok());
  EXPECT_EQ(reopened.sprite_assets[0].source_sha256, original_hash);
  fs::remove(source_path);
  EXPECT_FALSE(next.CheckCurrentSpriteSource().ok());
  SpriteEditor offline;
  offline.SetDependencies(deps);
  ASSERT_TRUE(offline.Load().ok());
  ASSERT_NE(offline.current_custom_sprite(), nullptr);
  EXPECT_EQ(offline.current_sprite_binding()->sheets, binding.sheets);
}

TEST_F(SpriteCatalogSessionTest,
       AssetBindingsSwitchWithDocumentsAndLegacyProjectClearsList) {
  zsprite::ZSprite fixture;
  fixture.Reset();
  fixture.editor.Frames.resize(1);
  fixture.animations.emplace_back(0, 0, 6, "Idle");
  const auto first = (root_ / "first.zsm").string();
  const auto second = (root_ / "second.zsm").string();
  ASSERT_TRUE(fixture.Save(first).ok());
  ASSERT_TRUE(fixture.Save(second).ok());
  project::SpriteAssetBinding a, b;
  a.zsm_path = first;
  a.sheets[0] = 10;
  b.zsm_path = second;
  b.sheets[0] = 40;
  project_.sprite_assets = {a, b};
  EditorDependencies deps;
  deps.project = &project_;
  SpriteEditor editor;
  editor.SetDependencies(deps);
  ASSERT_TRUE(editor.Load().ok());
  EXPECT_EQ(editor.current_sprite_binding()->sheets[0], 40);
  ASSERT_TRUE(editor.OpenSpriteAsset(first).ok());
  EXPECT_EQ(editor.current_sprite_binding()->sheets[0], 10);
  EXPECT_TRUE(
      project_.LoadFromString("[project]\nname=Legacy\n", project_.filepath)
          .ok());
  EXPECT_TRUE(project_.sprite_assets.empty());
}

TEST_F(SpriteCatalogSessionTest,
       InvalidAssetBindingsFailInsteadOfWrappingValues) {
  project::SpriteAssetBinding asset;
  asset.zsm_path = "actor.zsm";
  auto json = project::SpriteAssetToJson(asset);
  ASSERT_TRUE(project::SpriteAssetFromJson(json).ok());
  json["sheets"][0] = 256;
  EXPECT_FALSE(project::SpriteAssetFromJson(json).ok());
  json = project::SpriteAssetToJson(asset);
  json["palette_rows"][0]["index"] = -1;
  EXPECT_FALSE(project::SpriteAssetFromJson(json).ok());
  json = project::SpriteAssetToJson(asset);
  json["unexpected"] = "unsupported";
  EXPECT_FALSE(project::SpriteAssetFromJson(json).ok());
  asset.source_path = "Sprites/NPCs/mermaid.asm";
  EXPECT_FALSE(
      project::SpriteAssetFromJson(project::SpriteAssetToJson(asset)).ok());
  auto duplicate =
      project::SpriteAssetToJson(project::SpriteAssetBinding{}).dump();
  duplicate.insert(1, "\"version\":1,");
  EXPECT_FALSE(project::ParseSpriteAssetBinding(duplicate).ok());
}

TEST_F(SpriteCatalogSessionTest,
       CorruptAssetDoesNotPreventOtherProjectAssetsOpening) {
  const auto corrupt = root_ / "corrupt.zsm";
  std::ofstream(corrupt, std::ios::binary) << "\xff\xff\xff\x7f";
  zsprite::ZSprite good;
  good.Reset();
  good.editor.Frames.resize(1);
  const auto valid = root_ / "valid.zsm";
  ASSERT_TRUE(good.Save(valid.string()).ok());
  project::SpriteAssetBinding a, b;
  a.zsm_path = corrupt.string();
  b.zsm_path = valid.string();
  project_.sprite_assets = {a, b};
  EditorDependencies deps;
  deps.project = &project_;
  SpriteEditor editor;
  editor.SetDependencies(deps);
  EXPECT_FALSE(editor.ReloadProjectSpriteAssets().ok());
  ASSERT_NE(editor.current_sprite_binding(), nullptr);
  EXPECT_EQ(editor.current_sprite_binding()->zsm_path, valid.string());
}

TEST_F(SpriteCatalogSessionTest, InvalidZsmSavePreservesExistingFile) {
  const auto path = root_ / "preserved.zsm";
  std::ofstream(path) << "original";
  zsprite::ZSprite sprite;
  sprite.Reset();
  sprite.editor.Frames.resize(257);
  EXPECT_FALSE(sprite.Save(path.string()).ok());
  sprite.editor.Frames.resize(1);
  sprite.property_health.Text = "invalid";
  EXPECT_FALSE(sprite.Save(path.string()).ok());
  std::ifstream input(path);
  EXPECT_EQ(std::string(std::istreambuf_iterator<char>(input), {}), "original");
}

TEST_F(SpriteCatalogSessionTest, SeparateEditorsDoNotLeakCatalogOrSource) {
  EditorDependencies deps;
  deps.project = &project_;
  deps.session_id = 1;
  SpriteEditor oracle;
  oracle.SetDependencies(deps);
  ASSERT_TRUE(oracle.sprite_catalog());
  auto maple = oracle.sprite_catalog()->Resolve(0xF0, 1);
  ASSERT_TRUE(oracle.OpenCatalogSource(maple.variant->sources[1]).ok());
  ASSERT_TRUE(oracle.catalog_source());
  EXPECT_EQ(oracle.catalog_source()->label_line, 2);

  project::YazeProject vanilla_project;
  deps.project = &vanilla_project;
  deps.session_id = 2;
  SpriteEditor vanilla;
  vanilla.SetDependencies(deps);
  EXPECT_FALSE(vanilla.sprite_catalog());
  EXPECT_FALSE(vanilla.catalog_source());
  ASSERT_TRUE(oracle.sprite_catalog());
  EXPECT_EQ(oracle.sprite_catalog()->Resolve(0xF0, 1).variant->name, "Maple");

  // Rebinding an editor must discard source text as well as catalog identity.
  oracle.SetDependencies(deps);
  EXPECT_FALSE(oracle.sprite_catalog());
  EXPECT_FALSE(oracle.catalog_source());
}

TEST_F(SpriteCatalogSessionTest, MissingSourceDoesNotBlockCatalogOrEditorLoad) {
  project_.sprite_source_root = "unavailable";
  EditorDependencies deps;
  deps.project = &project_;
  SpriteEditor editor;
  editor.SetDependencies(deps);
  ASSERT_TRUE(editor.Load().ok());
  ASSERT_TRUE(editor.sprite_catalog());
  auto maple = editor.sprite_catalog()->Resolve(0xF0, 1);
  EXPECT_FALSE(editor.OpenCatalogSource(maple.variant->sources[1]).ok());
  EXPECT_FALSE(editor.catalog_source());
  EXPECT_TRUE(editor.sprite_catalog());
}

TEST_F(SpriteCatalogSessionTest, FailedReloadClearsStaleCatalogAndSource) {
  EditorDependencies deps;
  deps.project = &project_;
  SpriteEditor editor;
  editor.SetDependencies(deps);
  ASSERT_TRUE(editor.sprite_catalog());
  ASSERT_TRUE(
      editor
          .OpenCatalogSource(
              editor.sprite_catalog()->Resolve(0xF0, 1).variant->sources[1])
          .ok());
  std::ofstream(root_ / "catalog.json")
      << R"({"schema_version":2,"profile":"oracle","families":[]})";
  EXPECT_FALSE(editor.ReloadSpriteCatalog().ok());
  EXPECT_FALSE(editor.sprite_catalog());
  EXPECT_FALSE(editor.catalog_source());
  EXPECT_TRUE(editor.Load().ok());
}

TEST_F(SpriteCatalogSessionTest, BrowseAndReloadDoNotModifyRomOrSource) {
  Rom rom;
  rom.Expand(0x100000);
  const auto before = rom.vector();
  EditorDependencies deps;
  deps.project = &project_;
  deps.rom = &rom;
  SpriteEditor editor(&rom);
  editor.SetDependencies(deps);
  ASSERT_TRUE(editor.ReloadSpriteCatalog().ok());
  ASSERT_TRUE(
      editor
          .OpenCatalogSource(
              editor.sprite_catalog()->Resolve(0xF0, 1).variant->sources[1])
          .ok());
  EXPECT_TRUE(editor.Save().ok());
  EXPECT_EQ(rom.vector(), before);
  std::ifstream stream(root_ / "source/Sprites/NPCs/maple.asm");
  EXPECT_EQ(std::string(std::istreambuf_iterator<char>(stream), {}),
            "; fixture\nMapleHandler:\nRTL\n");
}

TEST_F(SpriteCatalogSessionTest, ProjectPathsRoundTripAndAbsentSettingsReset) {
  ASSERT_TRUE(project_.Save().ok());
  std::ifstream original(project_.filepath);
  const std::string serialized(std::istreambuf_iterator<char>(original), {});
  project::YazeProject restored;
  ASSERT_TRUE(restored.LoadFromString(serialized, project_.filepath).ok());
  EXPECT_EQ(restored.sprite_catalog_file, (root_ / "catalog.json").string());
  EXPECT_EQ(restored.sprite_source_root, (root_ / "source").string());
  ASSERT_TRUE(restored.Save().ok());
  std::ifstream round_trip(restored.filepath);
  const std::string second(std::istreambuf_iterator<char>(round_trip), {});
  EXPECT_NE(second.find("sprite_catalog_file=catalog.json"), std::string::npos);
  EXPECT_NE(second.find("sprite_source_root=source"), std::string::npos);
  std::ofstream(root_ / "vanilla.yaze") << "[project]\nname=Vanilla\n";
  ASSERT_TRUE(restored.Open((root_ / "vanilla.yaze").string()).ok());
  EXPECT_TRUE(restored.sprite_catalog_file.empty());
  EXPECT_TRUE(restored.sprite_source_root.empty());
}

TEST_F(SpriteCatalogSessionTest, RegisteredPanelsDrawWithSessionLocalContent) {
  auto* context = ImGui::CreateContext();
  struct Cleanup {
    ImGuiContext* context;
    ~Cleanup() { ImGui::DestroyContext(context); }
  } cleanup{context};
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(1200, 1000);
  io.DeltaTime = 1.0f / 60;
  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  WorkspaceWindowManager manager;
  manager.RegisterSession(0);
  manager.SetActiveSession(0);
  EditorDependencies deps;
  deps.project = &project_;
  deps.window_manager = &manager;
  SpriteEditor oracle;
  oracle.SetDependencies(deps);
  oracle.Initialize();
  auto* oracle_panel = manager.GetWindowContent(0, "sprite.catalog");
  ASSERT_NE(oracle_panel, nullptr);
  ASSERT_TRUE(
      oracle
          .OpenCatalogSource(
              oracle.sprite_catalog()->Resolve(0xF0, 1).variant->sources[1])
          .ok());

  manager.RegisterSession(1);
  manager.SetActiveSession(1);
  project::YazeProject vanilla_project;
  deps.project = &vanilla_project;
  deps.session_id = 1;
  SpriteEditor vanilla;
  vanilla.SetDependencies(deps);
  vanilla.Initialize();
  auto* vanilla_panel = manager.GetWindowContent(1, "sprite.catalog");
  ASSERT_NE(vanilla_panel, nullptr);
  EXPECT_NE(oracle_panel, vanilla_panel);
  EXPECT_EQ(oracle_panel, manager.GetWindowContent(0, "sprite.catalog"));

  auto draw_and_read = [&](WindowContent* panel, const std::string& name) {
    const auto log = root_ / (name + ".txt");
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(1100, 950));
    ImGui::Begin(name.c_str());
    ImGui::LogToFile(-1, log.string().c_str());
    bool open = true;
    panel->Draw(&open);
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    std::ifstream stream(log);
    return std::string(std::istreambuf_iterator<char>(stream), {});
  };
  const auto oracle_text = draw_and_read(oracle_panel, "oracle_catalog");
  EXPECT_NE(oracle_text.find("Mermaid / Maple / Librarian"), std::string::npos);
  EXPECT_NE(oracle_text.find("Preview unavailable"), std::string::npos);
  EXPECT_NE(oracle_text.find("MapleHandler"), std::string::npos);
  const auto vanilla_text = draw_and_read(vanilla_panel, "vanilla_catalog");
  EXPECT_NE(vanilla_text.find("Choose a sprite catalog"), std::string::npos);
  EXPECT_EQ(vanilla_text.find("Librarian"), std::string::npos);
}
}  // namespace
}  // namespace yaze::editor
