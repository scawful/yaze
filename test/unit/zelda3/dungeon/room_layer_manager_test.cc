#include "zelda3/dungeon/room_layer_manager.h"
#include "gtest/gtest.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {

class RoomLayerManagerTest : public ::testing::Test {
 protected:
  void PrepareColorMathRoom(Room& room) {
    std::vector<SDL_Color> palette(256, {0, 0, 0, 255});
    // Expanded five-bit red values: 8, 4, 12, 6, 16, 24, and 31. The
    // independent expected full-add/half-add results exist in this bank, so
    // nearest-palette quantization cannot hide the arithmetic difference.
    palette[33] = {66, 0, 0, 255};
    palette[34] = {33, 0, 0, 255};
    palette[35] = {99, 0, 0, 255};
    palette[36] = {49, 0, 0, 255};
    palette[37] = {132, 0, 0, 255};
    palette[38] = {198, 0, 0, 255};
    palette[39] = {255, 0, 0, 255};
    for (auto* buffer :
         {&room.bg1_buffer(), &room.bg2_buffer(), &room.object_bg1_buffer(),
          &room.object_bg2_buffer()}) {
      buffer->EnsureBitmapInitialized();
      buffer->bitmap().Fill(255);
      buffer->bitmap().SetPalette(palette);
      buffer->ClearPriorityBuffer();
      buffer->ClearCoverageBuffer();
      buffer->ClearBG1RevealMask();
    }
  }

  RoomLayerManager manager_;
};

// =============================================================================
// Layer Visibility Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, DefaultVisibilityAllLayersVisible) {
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG1_Layout));
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG1_Objects));
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG2_Layout));
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG2_Objects));
}

TEST_F(RoomLayerManagerTest, SetLayerVisibleWorks) {
  manager_.SetLayerVisible(LayerType::BG1_Objects, false);
  EXPECT_FALSE(manager_.IsLayerVisible(LayerType::BG1_Objects));
  EXPECT_TRUE(
      manager_.IsLayerVisible(LayerType::BG1_Layout));  // Others unchanged

  manager_.SetLayerVisible(LayerType::BG1_Objects, true);
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG1_Objects));
}

TEST_F(RoomLayerManagerTest, ResetRestoresDefaults) {
  manager_.SetLayerVisible(LayerType::BG1_Layout, false);
  manager_.SetLayerVisible(LayerType::BG2_Objects, false);
  manager_.SetLayerBlendMode(LayerType::BG2_Layout,
                             LayerBlendMode::Translucent);

  manager_.Reset();

  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG1_Layout));
  EXPECT_TRUE(manager_.IsLayerVisible(LayerType::BG2_Objects));
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout),
            LayerBlendMode::Normal);
}

// =============================================================================
// Blend Mode Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, DefaultBlendModeIsNormal) {
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG1_Layout),
            LayerBlendMode::Normal);
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout),
            LayerBlendMode::Normal);
}

TEST_F(RoomLayerManagerTest, SetBlendModeUpdatesAlpha) {
  manager_.SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Normal);
  EXPECT_EQ(manager_.GetLayerAlpha(LayerType::BG2_Layout), 255);

  manager_.SetLayerBlendMode(LayerType::BG2_Layout,
                             LayerBlendMode::Translucent);
  EXPECT_EQ(manager_.GetLayerAlpha(LayerType::BG2_Layout), 180);

  manager_.SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Off);
  EXPECT_EQ(manager_.GetLayerAlpha(LayerType::BG2_Layout), 0);
}

// =============================================================================
// Draw Order Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, DefaultDrawOrderBG2First) {
  manager_.SetBG2OnTop(false);
  auto order = manager_.GetDrawOrder();

  // BG2 should be drawn first (background)
  EXPECT_EQ(order[0], LayerType::BG2_Layout);
  EXPECT_EQ(order[1], LayerType::BG2_Objects);
  EXPECT_EQ(order[2], LayerType::BG1_Layout);
  EXPECT_EQ(order[3], LayerType::BG1_Objects);
}

TEST_F(RoomLayerManagerTest, BG2OnTopDrawOrderBG1First) {
  manager_.SetBG2OnTop(true);
  auto order = manager_.GetDrawOrder();

  // Draw order remains BG2 then BG1; "BG2 on top" only affects color math
  EXPECT_EQ(order[0], LayerType::BG2_Layout);
  EXPECT_EQ(order[1], LayerType::BG2_Objects);
  EXPECT_EQ(order[2], LayerType::BG1_Layout);
  EXPECT_EQ(order[3], LayerType::BG1_Objects);
}

// =============================================================================
// Per-Object Translucency Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, DefaultObjectsNotTranslucent) {
  EXPECT_FALSE(manager_.IsObjectTranslucent(0));
  EXPECT_FALSE(manager_.IsObjectTranslucent(10));
  EXPECT_EQ(manager_.GetObjectAlpha(0), 255);
}

TEST_F(RoomLayerManagerTest, SetObjectTranslucencyWorks) {
  manager_.SetObjectTranslucency(5, true, 128);
  EXPECT_TRUE(manager_.IsObjectTranslucent(5));
  EXPECT_EQ(manager_.GetObjectAlpha(5), 128);

  // Other objects unaffected
  EXPECT_FALSE(manager_.IsObjectTranslucent(4));
  EXPECT_EQ(manager_.GetObjectAlpha(4), 255);
}

TEST_F(RoomLayerManagerTest, ClearObjectTranslucencyWorks) {
  manager_.SetObjectTranslucency(5, true, 128);
  manager_.SetObjectTranslucency(10, true, 64);
  manager_.ClearObjectTranslucency();

  EXPECT_FALSE(manager_.IsObjectTranslucent(5));
  EXPECT_FALSE(manager_.IsObjectTranslucent(10));
}

// =============================================================================
// LayerMergeType Integration Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, ApplyLayerMergingNormal) {
  // LayerMergeType(id, name, see, top, trans)
  // "Normal" mode: visible=true, on_top=false, translucent=false
  LayerMergeType merge{0x06, "Normal", true, false, false};
  manager_.ApplyLayerMerging(merge);

  EXPECT_FALSE(manager_.IsBG2OnTop());  // Normal has Layer2OnTop=false
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout),
            LayerBlendMode::Normal);
}

TEST_F(RoomLayerManagerTest, ApplyLayerMergingTranslucent) {
  LayerMergeType merge{0x04, "Translucent", true, true, true};
  manager_.ApplyLayerMerging(merge);

  EXPECT_TRUE(manager_.IsBG2OnTop());
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout),
            LayerBlendMode::Translucent);
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Objects),
            LayerBlendMode::Translucent);
}

TEST_F(RoomLayerManagerTest, ApplyLayerMergingOff) {
  LayerMergeType merge{0x00, "Off", false, false, false};
  manager_.ApplyLayerMerging(merge);

  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout),
            LayerBlendMode::Normal);
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Objects),
            LayerBlendMode::Normal);
}

TEST_F(RoomLayerManagerTest, SetBg2SynchronizesDecodedRenderingState) {
  Room room(/*room_id=*/0, /*rom=*/nullptr);

  room.SetBg2(static_cast<background2>(6));
  EXPECT_EQ(room.bg2(), static_cast<background2>(6));
  EXPECT_EQ(room.layer2_mode(), 6);
  EXPECT_EQ(room.layer_merging(), LayerMerge06);
  EXPECT_FALSE(room.IsLight());

  room.SetBg2(static_cast<background2>(4));
  EXPECT_EQ(room.bg2(), static_cast<background2>(4));
  EXPECT_EQ(room.layer2_mode(), 4);
  EXPECT_EQ(room.layer_merging(), LayerMerge04);

  // The dark-room flag occupies a separate header bit. Entering that editor
  // state must retain the three-bit BG2 mode that will be serialized with it.
  room.SetBg2(background2::DarkRoom);
  EXPECT_EQ(room.bg2(), background2::DarkRoom);
  EXPECT_EQ(room.layer2_mode(), 4);
  EXPECT_EQ(room.layer_merging(), LayerMerge08);
  EXPECT_TRUE(room.IsLight());

  // Leaving DarkRoom clears the flag and restores the selected BG2 mode as the
  // active compositor state without requiring a ROM reload.
  room.SetBg2(static_cast<background2>(6));
  EXPECT_EQ(room.bg2(), static_cast<background2>(6));
  EXPECT_EQ(room.layer2_mode(), 6);
  EXPECT_EQ(room.layer_merging(), LayerMerge06);
  EXPECT_FALSE(room.IsLight());
}

TEST_F(RoomLayerManagerTest, SetLayer2ModeSynchronizesNonDarkBg2State) {
  Room room(/*room_id=*/0, /*rom=*/nullptr);

  room.SetLayer2Mode(6);
  EXPECT_EQ(room.bg2(), static_cast<background2>(6));
  EXPECT_EQ(room.layer2_mode(), 6);
  EXPECT_EQ(room.layer_merging(), LayerMerge06);

  room.SetBg2(background2::DarkRoom);
  room.SetLayer2Mode(4);
  EXPECT_EQ(room.bg2(), background2::DarkRoom);
  EXPECT_EQ(room.layer2_mode(), 4);
  EXPECT_EQ(room.layer_merging(), LayerMerge08);
}

TEST_F(RoomLayerManagerTest,
       ApplyRoomEffectMovingWaterPromotesBG2Translucency) {
  manager_.SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Normal);
  manager_.SetLayerBlendMode(LayerType::BG2_Objects, LayerBlendMode::Normal);

  manager_.ApplyRoomEffect(EffectKey::Moving_Water);

  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout),
            LayerBlendMode::Translucent);
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Objects),
            LayerBlendMode::Translucent);
}

TEST_F(RoomLayerManagerTest, ApplyRoomEffectPreservesExplicitBlendModes) {
  manager_.SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Addition);
  manager_.SetLayerBlendMode(LayerType::BG2_Objects, LayerBlendMode::Off);

  manager_.ApplyRoomEffect(EffectKey::Moving_Water);

  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Layout),
            LayerBlendMode::Addition);
  EXPECT_EQ(manager_.GetLayerBlendMode(LayerType::BG2_Objects),
            LayerBlendMode::Off);
}

// =============================================================================
// Static Helper Tests
// =============================================================================

TEST_F(RoomLayerManagerTest, GetLayerNameReturnsCorrectStrings) {
  EXPECT_STREQ(RoomLayerManager::GetLayerName(LayerType::BG1_Layout),
               "BG1 Layout");
  EXPECT_STREQ(RoomLayerManager::GetLayerName(LayerType::BG1_Objects),
               "BG1 Objects");
  EXPECT_STREQ(RoomLayerManager::GetLayerName(LayerType::BG2_Layout),
               "BG2 Layout");
  EXPECT_STREQ(RoomLayerManager::GetLayerName(LayerType::BG2_Objects),
               "BG2 Objects");
}

TEST_F(RoomLayerManagerTest, GetBlendModeNameReturnsCorrectStrings) {
  EXPECT_STREQ(RoomLayerManager::GetBlendModeName(LayerBlendMode::Normal),
               "Normal");
  EXPECT_STREQ(RoomLayerManager::GetBlendModeName(LayerBlendMode::Translucent),
               "Translucent");
  EXPECT_STREQ(RoomLayerManager::GetBlendModeName(LayerBlendMode::Off), "Off");
}

TEST_F(RoomLayerManagerTest, CompositeToOutputUsesBackdropWhenLayersAreEmpty) {
  Room room(/*room_id=*/0, /*rom=*/nullptr);

  // Ensure all layer buffers are initialized and empty (255 == transparent fill).
  room.bg1_buffer().EnsureBitmapInitialized();
  room.bg2_buffer().EnsureBitmapInitialized();
  room.object_bg1_buffer().EnsureBitmapInitialized();
  room.object_bg2_buffer().EnsureBitmapInitialized();
  room.bg1_buffer().bitmap().Fill(255);
  room.bg2_buffer().bitmap().Fill(255);
  room.object_bg1_buffer().bitmap().Fill(255);
  room.object_bg2_buffer().bitmap().Fill(255);

  gfx::Bitmap output;
  manager_.CompositeToOutput(room, output);

  ASSERT_TRUE(output.is_active());
  ASSERT_EQ(output.width(), 512);
  ASSERT_EQ(output.height(), 512);
  ASSERT_GT(output.size(), 0u);

  // With no visible pixels in any layer, the composite should remain at the
  // backdrop value (0), not the transparent fill (255).
  EXPECT_EQ(output.data()[0], 0);
}

TEST_F(RoomLayerManagerTest, PriorityCompositing_BG2Priority1OverBG1Priority0) {
  // This matches SNES Mode 1 behavior: BG2 tiles with priority=1 can appear
  // above BG1 tiles with priority=0.
  manager_.SetPriorityCompositing(true);

  Room room(/*room_id=*/0, /*rom=*/nullptr);
  room.bg1_buffer().EnsureBitmapInitialized();
  room.bg2_buffer().EnsureBitmapInitialized();
  room.object_bg1_buffer().EnsureBitmapInitialized();
  room.object_bg2_buffer().EnsureBitmapInitialized();
  room.bg1_buffer().bitmap().Fill(255);
  room.bg2_buffer().bitmap().Fill(255);
  room.object_bg1_buffer().bitmap().Fill(255);
  room.object_bg2_buffer().bitmap().Fill(255);
  room.bg1_buffer().ClearPriorityBuffer();
  room.bg2_buffer().ClearPriorityBuffer();
  room.object_bg1_buffer().ClearPriorityBuffer();
  room.object_bg2_buffer().ClearPriorityBuffer();

  // Put an opaque pixel in BG1 (priority 0) and a competing pixel in BG2
  // (priority 1) at the same location.
  room.bg1_buffer().bitmap().mutable_data()[0] = 10;
  room.bg1_buffer().mutable_priority_data()[0] = 0;
  room.bg2_buffer().bitmap().mutable_data()[0] = 20;
  room.bg2_buffer().mutable_priority_data()[0] = 1;

  gfx::Bitmap output;
  manager_.CompositeToOutput(room, output);
  ASSERT_TRUE(output.is_active());
  EXPECT_EQ(output.data()[0], 20);
}

TEST_F(RoomLayerManagerTest, PriorityCompositing_BG1Priority0OverBG2Priority0) {
  // BG1 with priority=0 should still be above BG2 with priority=0.
  manager_.SetPriorityCompositing(true);

  Room room(/*room_id=*/0, /*rom=*/nullptr);
  room.bg1_buffer().EnsureBitmapInitialized();
  room.bg2_buffer().EnsureBitmapInitialized();
  room.object_bg1_buffer().EnsureBitmapInitialized();
  room.object_bg2_buffer().EnsureBitmapInitialized();
  room.bg1_buffer().bitmap().Fill(255);
  room.bg2_buffer().bitmap().Fill(255);
  room.object_bg1_buffer().bitmap().Fill(255);
  room.object_bg2_buffer().bitmap().Fill(255);
  room.bg1_buffer().ClearPriorityBuffer();
  room.bg2_buffer().ClearPriorityBuffer();

  room.bg1_buffer().bitmap().mutable_data()[0] = 11;
  room.bg1_buffer().mutable_priority_data()[0] = 0;
  room.bg2_buffer().bitmap().mutable_data()[0] = 22;
  room.bg2_buffer().mutable_priority_data()[0] = 0;

  gfx::Bitmap output;
  manager_.CompositeToOutput(room, output);
  ASSERT_TRUE(output.is_active());
  EXPECT_EQ(output.data()[0], 11);
}

TEST_F(RoomLayerManagerTest, ModeSevenUsesFullAddWithOrWithoutTilePriority) {
  // USDASM Underworld_HandleTranslucencyAndPalettes ($02:A20C) selects
  // CGADSUB=$32 for mode 7: add BG2 + subscreen without the half-color bit.
  for (const bool priority : {false, true}) {
    SCOPED_TRACE(priority);
    manager_.Reset();
    manager_.SetPriorityCompositing(priority);
    manager_.ApplyLayerMerging(LayerMerge07);
    Room room(/*room_id=*/0, /*rom=*/nullptr);
    room.SetLayer2Mode(0x07);
    PrepareColorMathRoom(room);
    auto& upper = room.bg1_buffer().bitmap().mutable_data();
    auto& lower = room.bg2_buffer().bitmap().mutable_data();
    upper[0] = 33;
    lower[0] = 34;  // 8 + 4 = 12, not (8 + 4) / 2 = 6.
    upper[1] = 33;
    lower[1] = 33;  // Identical colors still add: 8 + 8 = 16.
    upper[2] = 38;
    lower[2] = 37;  // Saturate 24 + 16 to 31, never wrap.
    upper[3] = 33;  // Transparent lower: retain the upper color.
    lower[4] = 34;  // Transparent upper: retain the lower color.
    upper[5] = 33;
    lower[5] = 34;
    room.object_bg1_buffer().mutable_coverage_data()[5] = 1;
    // Transparent object coverage replaces its layout, so only lower remains.

    gfx::Bitmap output;
    manager_.CompositeToOutput(room, output);
    ASSERT_TRUE(output.is_active());
    EXPECT_EQ(output.data()[0], 35);
    EXPECT_EQ(output.data()[1], 37);
    EXPECT_EQ(output.data()[2], 39);
    EXPECT_EQ(output.data()[3], 33);
    EXPECT_EQ(output.data()[4], 34);
    EXPECT_EQ(output.data()[5], 34);

    manager_.SetLayerVisible(LayerType::BG2_Layout, false);
    manager_.SetLayerVisible(LayerType::BG2_Objects, false);
    manager_.CompositeToOutput(room, output);
    EXPECT_EQ(output.data()[0], 33) << "Hidden lower layers cannot add color";
  }
}

TEST_F(RoomLayerManagerTest, ModeFourRetainsExistingHalfAddAndFallback) {
  // USDASM $02:A212 selects CGADSUB=$62 for mode 4, including the half bit.
  // The priority-off fallback remains its existing simple upper overwrite.
  for (const bool priority : {false, true}) {
    SCOPED_TRACE(priority);
    manager_.Reset();
    manager_.SetPriorityCompositing(priority);
    manager_.ApplyLayerMerging(LayerMerge04);
    Room room(/*room_id=*/0, /*rom=*/nullptr);
    room.SetLayer2Mode(0x04);
    PrepareColorMathRoom(room);
    room.bg1_buffer().bitmap().mutable_data()[0] = 33;
    room.bg2_buffer().bitmap().mutable_data()[0] = 34;

    gfx::Bitmap output;
    manager_.CompositeToOutput(room, output);
    ASSERT_TRUE(output.is_active());
    EXPECT_EQ(output.data()[0], priority ? 36 : 33);
  }
}

TEST_F(RoomLayerManagerTest,
       HiddenTranslucentBG2SourceDoesNotBlendVisibleNormalSource) {
  for (const LayerType hidden_source :
       {LayerType::BG2_Layout, LayerType::BG2_Objects}) {
    SCOPED_TRACE(RoomLayerManager::GetLayerName(hidden_source));
    manager_.Reset();
    Room room(/*room_id=*/0, /*rom=*/nullptr);
    PrepareColorMathRoom(room);
    room.bg1_buffer().bitmap().mutable_data()[0] = 33;
    room.bg2_buffer().bitmap().mutable_data()[0] = 34;
    room.object_bg2_buffer().bitmap().mutable_data()[0] = 34;
    room.object_bg2_buffer().mutable_coverage_data()[0] = 1;

    manager_.SetLayerBlendMode(hidden_source, LayerBlendMode::Translucent);
    manager_.SetLayerVisible(hidden_source, false);
    gfx::Bitmap output;
    manager_.CompositeToOutput(room, output);
    EXPECT_EQ(output.data()[0], 33)
        << "A hidden source's blend setting cannot affect the visible source";

    manager_.SetLayerVisible(hidden_source, true);
    manager_.SetLayerBlendMode(hidden_source, LayerBlendMode::Off);
    manager_.CompositeToOutput(room, output);
    EXPECT_EQ(output.data()[0], 33);
  }
}

TEST_F(RoomLayerManagerTest, TranslucencyFollowsSelectedBG2SourceAtEachPixel) {
  for (const bool full_add : {false, true}) {
    SCOPED_TRACE(full_add ? "full add" : "half add");
    for (const bool translucent_layout : {false, true}) {
      SCOPED_TRACE(translucent_layout ? "translucent layout"
                                      : "translucent objects");
      manager_.Reset();
      if (full_add) {
        manager_.ApplyLayerMerging(LayerMerge07);
      }
      manager_.SetLayerBlendMode(LayerType::BG2_Layout,
                                 translucent_layout
                                     ? LayerBlendMode::Translucent
                                     : LayerBlendMode::Normal);
      manager_.SetLayerBlendMode(LayerType::BG2_Objects,
                                 translucent_layout
                                     ? LayerBlendMode::Normal
                                     : LayerBlendMode::Translucent);
      Room room(/*room_id=*/0, /*rom=*/nullptr);
      room.SetLayer2Mode(full_add ? 7 : 0);
      PrepareColorMathRoom(room);
      auto& upper = room.bg1_buffer().bitmap().mutable_data();
      auto& lower_layout = room.bg2_buffer().bitmap().mutable_data();
      auto& lower_objects = room.object_bg2_buffer().bitmap().mutable_data();
      for (int index = 0; index < 4; ++index) {
        upper[index] = 33;
      }
      lower_layout[0] = 34;   // No object: use the layout's blend setting.
      lower_objects[1] = 34;  // No layout: use the object's blend setting.
      lower_layout[2] = 34;
      lower_objects[2] = 34;  // Object replaces the layout and its blend mode.
      lower_layout[3] = 34;
      // A covered transparent object also replaces the translucent layout.
      room.object_bg2_buffer().mutable_coverage_data()[3] = 1;

      gfx::Bitmap output;
      manager_.CompositeToOutput(room, output);
      const uint8_t blended = full_add ? 35 : 36;
      EXPECT_EQ(output.data()[0], translucent_layout ? blended : 33);
      EXPECT_EQ(output.data()[1], translucent_layout ? 33 : blended);
      EXPECT_EQ(output.data()[2], translucent_layout ? 33 : blended);
      EXPECT_EQ(output.data()[3], 33);
    }
  }
}

TEST_F(RoomLayerManagerTest,
       ModeSixUpperMainScreenWinsRegardlessOfTilePriority) {
  manager_.ApplyLayerMerging(LayerMerge06);

  Room room(/*room_id=*/0, /*rom=*/nullptr);
  room.SetLayer2Mode(0x06);
  for (auto* buffer : {&room.bg1_buffer(), &room.bg2_buffer(),
                       &room.object_bg1_buffer(), &room.object_bg2_buffer()}) {
    buffer->EnsureBitmapInitialized();
    buffer->bitmap().Fill(255);
    buffer->ClearPriorityBuffer();
    buffer->ClearCoverageBuffer();
  }

  room.bg1_buffer().bitmap().mutable_data()[0] = 11;
  room.bg1_buffer().mutable_priority_data()[0] = 0;
  room.bg2_buffer().bitmap().mutable_data()[0] = 22;
  room.bg2_buffer().mutable_priority_data()[0] = 1;

  gfx::Bitmap output;
  manager_.CompositeToOutput(room, output);

  ASSERT_TRUE(output.is_active());
  EXPECT_EQ(output.data()[0], 11)
      << "Mode 6 places the upper tilemap on the main screen; lower-tilemap "
         "priority cannot cover it";
}

TEST_F(RoomLayerManagerTest,
       ModeSixRevealsLowerOnlyThroughTransparentUpperTilemap) {
  manager_.ApplyLayerMerging(LayerMerge06);

  Room room(/*room_id=*/0, /*rom=*/nullptr);
  room.SetLayer2Mode(0x06);
  for (auto* buffer : {&room.bg1_buffer(), &room.bg2_buffer(),
                       &room.object_bg1_buffer(), &room.object_bg2_buffer()}) {
    buffer->EnsureBitmapInitialized();
    buffer->bitmap().Fill(255);
    buffer->ClearPriorityBuffer();
    buffer->ClearCoverageBuffer();
    buffer->ClearBG1RevealMask();
  }

  // A historic reveal bit must not erase an opaque upper tile in mode 6.
  room.bg1_buffer().bitmap().mutable_data()[0] = 11;
  room.bg2_buffer().bitmap().mutable_data()[0] = 21;
  room.bg1_buffer().SetBG1RevealMaskRect(gfx::BG1RevealMaskSource::kBG2Objects,
                                         0, 0, 1, 1);

  // A transparent object write still replaces its own tilemap layout entry,
  // making the lower tilemap visible at this pixel.
  room.bg1_buffer().bitmap().mutable_data()[1] = 12;
  room.object_bg1_buffer().bitmap().mutable_data()[1] = 255;
  room.object_bg1_buffer().mutable_coverage_data()[1] = 1;
  room.bg2_buffer().bitmap().mutable_data()[1] = 22;

  // Opaque objects replace layout within a tilemap before main/sub resolution.
  room.bg1_buffer().bitmap().mutable_data()[2] = 13;
  room.object_bg1_buffer().bitmap().mutable_data()[2] = 14;
  room.object_bg1_buffer().mutable_coverage_data()[2] = 1;
  room.bg2_buffer().bitmap().mutable_data()[2] = 23;

  gfx::Bitmap output;
  manager_.CompositeToOutput(room, output);

  ASSERT_TRUE(output.is_active());
  EXPECT_EQ(output.data()[0], 11);
  EXPECT_EQ(output.data()[1], 22);
  EXPECT_EQ(output.data()[2], 14);
}

TEST_F(RoomLayerManagerTest,
       ModeSixUsesSerializedModeWhenDarkMergeOverridesMergeId) {
  manager_.ApplyLayerMerging(LayerMerge08);

  Room room(/*room_id=*/0, /*rom=*/nullptr);
  room.SetLayer2Mode(0x06);
  for (auto* buffer : {&room.bg1_buffer(), &room.bg2_buffer(),
                       &room.object_bg1_buffer(), &room.object_bg2_buffer()}) {
    buffer->EnsureBitmapInitialized();
    buffer->bitmap().Fill(255);
    buffer->ClearPriorityBuffer();
    buffer->ClearCoverageBuffer();
  }

  room.bg1_buffer().bitmap().mutable_data()[0] = 11;
  room.bg1_buffer().mutable_priority_data()[0] = 0;
  room.bg2_buffer().bitmap().mutable_data()[0] = 22;
  room.bg2_buffer().mutable_priority_data()[0] = 1;

  gfx::Bitmap output;
  manager_.CompositeToOutput(room, output);

  ASSERT_TRUE(output.is_active());
  EXPECT_EQ(output.data()[0], 11);
  EXPECT_EQ(manager_.GetMergeTypeId(), LayerMerge08.ID);
}

TEST_F(RoomLayerManagerTest, Coverage_ObjectTransparentWriteClearsLayout) {
  manager_.SetPriorityCompositing(true);

  Room room(/*room_id=*/0, /*rom=*/nullptr);
  room.bg1_buffer().EnsureBitmapInitialized();
  room.bg2_buffer().EnsureBitmapInitialized();
  room.object_bg1_buffer().EnsureBitmapInitialized();
  room.object_bg2_buffer().EnsureBitmapInitialized();

  room.bg1_buffer().bitmap().Fill(255);
  room.bg2_buffer().bitmap().Fill(255);
  room.object_bg1_buffer().bitmap().Fill(255);
  room.object_bg2_buffer().bitmap().Fill(255);

  room.bg1_buffer().ClearPriorityBuffer();
  room.bg2_buffer().ClearPriorityBuffer();
  room.object_bg1_buffer().ClearPriorityBuffer();
  room.object_bg2_buffer().ClearPriorityBuffer();
  room.object_bg1_buffer().ClearCoverageBuffer();
  room.object_bg2_buffer().ClearCoverageBuffer();

  // BG1 layout is opaque, BG2 layout is a different opaque pixel.
  room.bg1_buffer().bitmap().mutable_data()[0] = 11;
  room.bg2_buffer().bitmap().mutable_data()[0] = 22;

  // Object layer writes a transparent pixel at this location. With coverage,
  // it must override the BG1 layout and reveal BG2 (hole/clear semantics).
  room.object_bg1_buffer().bitmap().mutable_data()[0] = 255;
  room.object_bg1_buffer().mutable_coverage_data()[0] = 1;

  gfx::Bitmap output;
  manager_.CompositeToOutput(room, output);
  ASSERT_TRUE(output.is_active());
  EXPECT_EQ(output.data()[0], 22);
}

TEST_F(RoomLayerManagerTest,
       CrossLayerRevealMasksFollowOwnerVisibilityAndTargetOrder) {
  for (const bool priority_compositing : {true, false}) {
    SCOPED_TRACE(priority_compositing ? "priority" : "simple");
    RoomLayerManager manager;
    manager.SetPriorityCompositing(priority_compositing);

    Room room(/*room_id=*/0, /*rom=*/nullptr);
    for (auto* buffer :
         {&room.bg1_buffer(), &room.bg2_buffer(), &room.object_bg1_buffer(),
          &room.object_bg2_buffer()}) {
      buffer->EnsureBitmapInitialized();
      buffer->bitmap().Fill(255);
      buffer->ClearPriorityBuffer();
      buffer->ClearCoverageBuffer();
      buffer->ClearBG1RevealMask();
    }

    // Layout-owned reveal at pixel 0.
    room.bg1_buffer().bitmap().mutable_data()[0] = 11;
    room.bg1_buffer().mutable_priority_data()[0] = 0;
    room.bg2_buffer().bitmap().mutable_data()[0] = 21;
    room.bg2_buffer().mutable_priority_data()[0] = 0;
    room.bg1_buffer().SetBG1RevealMaskRect(gfx::BG1RevealMaskSource::kBG2Layout,
                                           0, 0, 1, 1);

    // Object-owned reveal at pixel 1.
    room.object_bg1_buffer().bitmap().mutable_data()[1] = 12;
    room.object_bg1_buffer().mutable_priority_data()[1] = 0;
    room.object_bg1_buffer().mutable_coverage_data()[1] = 1;
    room.object_bg2_buffer().bitmap().mutable_data()[1] = 22;
    room.object_bg2_buffer().mutable_priority_data()[1] = 0;
    room.object_bg2_buffer().mutable_coverage_data()[1] = 1;
    room.object_bg1_buffer().SetBG1RevealMaskRect(
        gfx::BG1RevealMaskSource::kBG2Objects, 1, 0, 1, 1);

    // A layout reveal must not suppress a later BG1 object at pixel 2.
    room.bg1_buffer().bitmap().mutable_data()[2] = 13;
    room.bg2_buffer().bitmap().mutable_data()[2] = 23;
    room.bg1_buffer().SetBG1RevealMaskRect(gfx::BG1RevealMaskSource::kBG2Layout,
                                           2, 0, 1, 1);
    room.object_bg1_buffer().bitmap().mutable_data()[2] = 14;
    room.object_bg1_buffer().mutable_priority_data()[2] = 0;
    room.object_bg1_buffer().mutable_coverage_data()[2] = 1;

    // A later BG1 object has cleared its target's object-source bit at pixel 3;
    // the corresponding layout-target bit must not punch through that object.
    room.bg1_buffer().bitmap().mutable_data()[3] = 15;
    room.bg1_buffer().SetBG1RevealMaskRect(
        gfx::BG1RevealMaskSource::kBG2Objects, 3, 0, 1, 1);
    room.object_bg1_buffer().bitmap().mutable_data()[3] = 16;
    room.object_bg1_buffer().mutable_priority_data()[3] = 0;
    room.object_bg1_buffer().mutable_coverage_data()[3] = 1;
    room.object_bg2_buffer().bitmap().mutable_data()[3] = 24;
    room.object_bg2_buffer().mutable_coverage_data()[3] = 1;

    const auto raw_layout = room.bg1_buffer().bitmap().vector();
    const auto raw_objects = room.object_bg1_buffer().bitmap().vector();
    auto expect_pixels = [&](uint8_t pixel0, uint8_t pixel1) {
      const auto& composite = room.GetCompositeBitmap(manager);
      ASSERT_TRUE(composite.is_active());
      EXPECT_EQ(composite.data()[0], pixel0);
      EXPECT_EQ(composite.data()[1], pixel1);
      EXPECT_EQ(composite.data()[2], 14);
      EXPECT_EQ(composite.data()[3], 16);
    };

    expect_pixels(21, 22);

    manager.SetLayerVisible(LayerType::BG2_Layout, false);
    expect_pixels(11, 22);

    manager.SetLayerVisible(LayerType::BG2_Layout, true);
    manager.SetLayerVisible(LayerType::BG2_Objects, false);
    expect_pixels(21, 12);

    manager.SetLayerVisible(LayerType::BG2_Objects, true);
    expect_pixels(21, 22);

    EXPECT_EQ(room.bg1_buffer().bitmap().vector(), raw_layout);
    EXPECT_EQ(room.object_bg1_buffer().bitmap().vector(), raw_objects);
  }
}

TEST_F(RoomLayerManagerTest,
       CompositeCacheInvalidatesWhenLayerManagerStateChanges) {
  Room room(/*room_id=*/0, /*rom=*/nullptr);
  room.bg1_buffer().EnsureBitmapInitialized();
  room.bg2_buffer().EnsureBitmapInitialized();
  room.object_bg1_buffer().EnsureBitmapInitialized();
  room.object_bg2_buffer().EnsureBitmapInitialized();

  room.bg1_buffer().bitmap().Fill(255);
  room.bg2_buffer().bitmap().Fill(255);
  room.object_bg1_buffer().bitmap().Fill(255);
  room.object_bg2_buffer().bitmap().Fill(255);

  room.bg1_buffer().bitmap().mutable_data()[0] = 11;
  room.bg2_buffer().bitmap().mutable_data()[0] = 22;

  auto& first = room.GetCompositeBitmap(manager_);
  ASSERT_TRUE(first.is_active());
  EXPECT_EQ(first.data()[0], 11);
  EXPECT_FALSE(room.IsCompositeDirty());

  manager_.SetLayerVisible(LayerType::BG1_Layout, false);

  auto& second = room.GetCompositeBitmap(manager_);
  ASSERT_TRUE(second.is_active());
  EXPECT_EQ(second.data()[0], 22)
      << "Changing layer visibility must invalidate cached composites";
}

// In rooms whose layer settings put hardware BG1 on neither screen (BGACT 0),
// the game never shows the lower tilemap, which yaze keeps in its BG2
// buffers. The composite hides those layers unless ShowHiddenLayers is on.
TEST_F(RoomLayerManagerTest, GameHiddenLowerTilemapIsOffUnlessShown) {
  RoomLayerManager manager;
  manager.ApplyGameLayerRegisters(DeriveRoomLayerRegisters(0, false, 0, 0, {}));
  EXPECT_TRUE(manager.GameHidesLowerTilemap());
  EXPECT_TRUE(manager.IsHiddenByGame(LayerType::BG2_Layout));
  EXPECT_TRUE(manager.IsHiddenByGame(LayerType::BG2_Objects));
  EXPECT_FALSE(manager.IsHiddenByGame(LayerType::BG1_Layout));

  manager.SetShowHiddenLayers(true);
  EXPECT_FALSE(manager.IsHiddenByGame(LayerType::BG2_Layout));

  RoomLayerManager shown;
  shown.ApplyGameLayerRegisters(DeriveRoomLayerRegisters(1, false, 0, 0, {}));
  EXPECT_FALSE(shown.GameHidesLowerTilemap());
  EXPECT_FALSE(shown.IsHiddenByGame(LayerType::BG2_Layout));
}

// Sub-screen-only lower tilemaps never cover opaque upper pixels; when both
// tilemaps share the main screen (BGACT 3) the lower one wins priority ties.
TEST_F(RoomLayerManagerTest, GameStackingFollowsLayerRegisters) {
  RoomLayerManager unapplied;
  EXPECT_TRUE(unapplied.UpperTilemapCoversLower(/*layer2_mode=*/6));
  EXPECT_FALSE(unapplied.UpperTilemapCoversLower(/*layer2_mode=*/1));
  EXPECT_FALSE(unapplied.LowerTilemapWinsTies());

  RoomLayerManager parallax;  // BGACT 1: sub screen, no blend
  parallax.ApplyGameLayerRegisters(
      DeriveRoomLayerRegisters(1, false, 0, 0, {}));
  EXPECT_TRUE(parallax.UpperTilemapCoversLower(1));
  EXPECT_FALSE(parallax.LowerTilemapWinsTies());

  RoomLayerManager translucent;  // BGACT 4: upper blends
  translucent.ApplyGameLayerRegisters(
      DeriveRoomLayerRegisters(4, false, 0, 0, {}));
  EXPECT_FALSE(translucent.UpperTilemapCoversLower(4));

  RoomLayerManager on_top;  // BGACT 3: both on the main screen
  on_top.ApplyGameLayerRegisters(DeriveRoomLayerRegisters(3, false, 0, 0, {}));
  EXPECT_FALSE(on_top.UpperTilemapCoversLower(3));
  EXPECT_TRUE(on_top.LowerTilemapWinsTies());
}

}  // namespace zelda3
}  // namespace yaze
