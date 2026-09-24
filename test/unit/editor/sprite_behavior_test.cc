#include "app/editor/sprite/sprite_behavior.h"
#include "app/editor/sprite/sprite_editor.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "core/asar_wrapper.h"
#include "core/project.h"
#include "core/sprite_asset_json.h"
#include "gtest/gtest.h"
#include "test_utils.h"

namespace yaze::editor {
namespace {
project::SpriteBehavior ExampleBehavior() {
  project::SpriteBehavior result;
  result.profile = "oracle_actions_v1";
  result.source_sha256.fill(std::string(64, 'a'));
  result.actions.resize(2);
  result.actions[0].name = "Talk";
  result.actions[0].block_player = true;
  result.actions[0].message_id = 0x1B3;
  result.actions[0].message_next = 1;
  auto& move = result.actions[1];
  move.name = "Walk";
  move.animation = 1;
  move.move = true;
  move.x_speed = -8;
  move.bounce_tiles = true;
  move.timer_ticks = 40;
  move.timer_next = 0;
  return result;
}

zsprite::ZSprite ExampleSprite() {
  zsprite::ZSprite sprite;
  sprite.Reset();
  sprite.editor.Frames.resize(3);
  for (auto& frame : sprite.editor.Frames)
    frame.Tiles.emplace_back(128, 112, false, false, 0x113, 4, true, 3);
  sprite.animations.emplace_back(0, 0, 20, "Idle");
  sprite.animations.emplace_back(1, 2, 14, "Walk");
  return sprite;
}

TEST(SpriteBehaviorTest,
     GeneratesExplicitEntriesAndSingleMovementWithImmediateCarryBranch) {
  auto generated = sprite_behavior::GenerateCandidate(
      ExampleBehavior(), ExampleSprite(), "Test_Actor");
  ASSERT_TRUE(generated.ok()) << generated.status();
  EXPECT_NE(generated->find("%ShowSolicitedMessage($01B3)\n  BCC .no_message\n "
                            " JMP Test_Actor_Enter1"),
            std::string::npos);
  EXPECT_NE(generated->find(
                "JSL Sprite_BounceFromTileCollision\n  JSL Sprite_Move\n  RTS"),
            std::string::npos);
  EXPECT_NE(generated->find("LDA.b #$F8 : STA.w SprXSpeed, X"),
            std::string::npos);
  EXPECT_NE(generated->find("LDA.b #$0E : STA.w SprTimerB, X"),
            std::string::npos);
  EXPECT_NE(generated->find("LDA.b #$28 : STA.w SprTimerA, X"),
            std::string::npos);
  EXPECT_EQ(generated->find("%GotoAction"), std::string::npos);
  EXPECT_EQ(generated->find("%MoveTowardPlayer"), std::string::npos);
  EXPECT_EQ(generated->find("Set_Sprite_Properties"), std::string::npos);
}

TEST(SpriteBehaviorTest,
     RejectsInvalidTargetsProfilesAndMacroEndpointOverflow) {
  auto model = ExampleBehavior();
  auto sprite = ExampleSprite();
  model.actions[0].message_next = 2;
  EXPECT_FALSE(sprite_behavior::Validate(model, sprite).ok());
  model = ExampleBehavior();
  model.actions[0].animation = 2;
  EXPECT_FALSE(sprite_behavior::Validate(model, sprite).ok());
  model = ExampleBehavior();
  sprite.editor.Frames.resize(256);
  sprite.animations[0].frame_end = 255;
  EXPECT_FALSE(sprite_behavior::Validate(model, sprite).ok());
  sprite = ExampleSprite();
  EXPECT_FALSE(
      sprite_behavior::GenerateCandidate(model, sprite, "Label:\norg $008000")
          .ok());
  model.profile = "vanilla";
  EXPECT_FALSE(sprite_behavior::Validate(model, sprite).ok());
}

TEST(SpriteBehaviorTest, DoesNotSilentlyRetargetDeletedActions) {
  auto model = ExampleBehavior();
  EXPECT_FALSE(sprite_behavior::RemoveAction(model, 1).ok());
  model.actions[0].message_id = model.actions[0].message_next = -1;
  ASSERT_TRUE(sprite_behavior::RemoveAction(model, 1).ok());
  EXPECT_EQ(model.actions.size(), 1);
  EXPECT_FALSE(sprite_behavior::RemoveAction(model, 0).ok());
}

TEST(SpriteBehaviorTest, BehaviorAssetsUseV2AndKeepV1Compatible) {
  project::SpriteAssetBinding asset;
  asset.zsm_path = "sprite.zsm";
  auto old = project::SpriteAssetToJson(asset);
  EXPECT_EQ(old["version"], 1);
  ASSERT_TRUE(project::SpriteAssetFromJson(old).ok());
  asset.behavior = ExampleBehavior();
  auto json = project::SpriteAssetToJson(asset);
  EXPECT_EQ(json["version"], 2);
  auto parsed = project::SpriteAssetFromJson(json);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->behavior.actions[1].x_speed, -8);
  EXPECT_EQ(parsed->behavior.actions[0].message_next, 1);
  json["behavior"]["actions"][0]["extra"] = true;
  EXPECT_FALSE(project::SpriteAssetFromJson(json).ok());
  json = project::SpriteAssetToJson(asset);
  json["behavior"]["actions"][0]["animation"] = 0.5;
  EXPECT_FALSE(project::SpriteAssetFromJson(json).ok());
}

class SpriteBehaviorSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* source = std::getenv("YAZE_ORACLE_SOURCE_ROOT");
    if (!source)
      GTEST_SKIP() << "Set YAZE_ORACLE_SOURCE_ROOT for reviewed-source tests";
    source_root_ = source;
    root_ = std::filesystem::temp_directory_path() /
            ("yaze_behavior_" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root_ / "Core");
    for (const auto* path : sprite_behavior::kSourcePaths)
      std::filesystem::copy_file(source_root_ / path, root_ / path);
  }
  void TearDown() override {
    if (!root_.empty()) {
      std::error_code ec;
      std::filesystem::remove_all(root_, ec);
    }
  }
  std::filesystem::path source_root_, root_;
};

TEST_F(SpriteBehaviorSourceTest,
       ReviewedProfileDetectsDriftAndRejectsChangedContracts) {
  auto baseline = sprite_behavior::ReadReviewedProfile(root_.string());
  ASSERT_TRUE(baseline.ok()) << baseline.status();
  auto model = ExampleBehavior();
  model.source_sha256 = *baseline;
  ASSERT_TRUE(sprite_behavior::VerifyProfile(root_.string(), model).ok());
  std::ofstream(root_ / "Core/sprite_macros.asm", std::ios::app)
      << "\n; comment\n";
  EXPECT_FALSE(sprite_behavior::VerifyProfile(root_.string(), model).ok());
  EXPECT_TRUE(sprite_behavior::ReadReviewedProfile(root_.string()).ok());
  std::ofstream(root_ / "Core/sprite_macros.asm", std::ios::app) << "NOP\n";
  EXPECT_FALSE(sprite_behavior::ReadReviewedProfile(root_.string()).ok());
}

TEST_F(SpriteBehaviorSourceTest,
       EditorModelPersistsAndExportChecksFreshSources) {
  auto sprite = ExampleSprite();
  const auto asset_path = (root_ / "actor.zsm").string();
  ASSERT_TRUE(sprite.Save(asset_path).ok());
  project::YazeProject project;
  project.filepath = (root_ / "game.yaze").string();
  project.sprite_source_root = root_.string();
  SpriteEditor editor;
  EditorDependencies deps;
  deps.project = &project;
  editor.SetDependencies(deps);
  ASSERT_TRUE(editor.OpenSpriteAsset(asset_path).ok());
  ASSERT_TRUE(editor.BindOracleBehaviorProfile().ok());
  auto behavior = ExampleBehavior();
  behavior.source_sha256 =
      editor.current_sprite_binding()->behavior.source_sha256;
  ASSERT_TRUE(editor.SetSpriteBehavior(behavior).ok());
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_EQ(editor.current_sprite_binding()->behavior.actions.size(), 1);
  ASSERT_TRUE(editor.Redo().ok());
  ASSERT_TRUE(editor.SaveSpriteAsset(asset_path).ok());
  ASSERT_TRUE(project.Save().ok());
  project::YazeProject reopened;
  ASSERT_TRUE(reopened.Open(project.filepath).ok());
  SpriteEditor next;
  deps.project = &reopened;
  next.SetDependencies(deps);
  ASSERT_TRUE(next.Load().ok());
  ASSERT_NE(next.current_sprite_binding(), nullptr);
  ASSERT_EQ(next.current_sprite_binding()->behavior.actions.size(), 2);
  EXPECT_TRUE(next.ExportCurrentSpriteBehavior("Test_Reopened").ok());
  std::ofstream(root_ / "Core/sprite_functions.asm", std::ios::app)
      << "\n; changed\n";
  EXPECT_FALSE(next.ExportCurrentSpriteBehavior("Test_Reopened").ok());
  EXPECT_EQ(next.current_sprite_binding()->behavior.actions[0].message_id,
            0x1B3);
}

TEST_F(SpriteBehaviorSourceTest,
       CandidateAssemblesAgainstReviewedMacrosInSyntheticRom) {
  auto macros =
      zelda3::ReadSpriteSourceFile(root_.string(), "Core/sprite_macros.asm");
  ASSERT_TRUE(macros.ok());
  auto hashes = sprite_behavior::ReadReviewedProfile(root_.string());
  ASSERT_TRUE(hashes.ok()) << hashes.status();
  auto model = ExampleBehavior();
  model.source_sha256 = *hashes;
  // Include only the two reviewed macros used by the candidate. External engine
  // addresses are link symbols; this is assembly/encoding evidence, not execution.
  std::string definitions;
  bool copying = false;
  for (const auto& line : macros->lines) {
    if (line.rfind("macro PlayAnimation(", 0) == 0 ||
        line.rfind("macro ShowSolicitedMessage(", 0) == 0)
      copying = true;
    if (copying)
      definitions += line + "\n";
    if (line == "endmacro")
      copying = false;
  }
  auto candidate =
      sprite_behavior::GenerateCandidate(model, ExampleSprite(), "Test_Actor");
  ASSERT_TRUE(candidate.ok());
  const auto patch = root_ / "candidate.asm";
  std::ofstream(patch)
      << "lorom\norg $308000\n"
      << "SprAction = $0D80\nSprFrame = $0D90\nSprTimerA = $0DF0\nSprTimerB = "
         "$0E00\n"
      << "SprXSpeed = $0D50\nSprYSpeed = $0D40\nJumpTableLocal = $008781\n"
      << "Sprite_PlayerCantPassThrough = "
         "$1EF4F3\nSprite_ShowSolicitedMessageIfPlayerFacing = $05E1A7\n"
      << "Sprite_BounceFromTileCollision = $318000\nSprite_Move = $318100\n"
      << definitions << *candidate;
  core::AsarWrapper assembler;
  ASSERT_TRUE(assembler.Initialize().ok());
  auto rom = test::TestRomManager::CreateMinimalTestRom(4 * 1024 * 1024);
  auto result = assembler.ApplyPatch(patch.string(), rom);
  ASSERT_TRUE(result.ok()) << result.status();
  std::string errors;
  for (const auto& error : result->errors)
    errors += error + "\n";
  EXPECT_TRUE(result->success) << errors;
}
}  // namespace
}  // namespace yaze::editor
