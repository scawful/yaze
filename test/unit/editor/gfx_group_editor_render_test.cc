#include "app/editor/graphics/gfx_group_editor_internal.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
#include "absl/cleanup/cleanup.h"
#include "app/editor/graphics/gfx_group_editor.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/automation/widget_id_registry.h"
#include "imgui/imgui_internal.h"

#include "app/editor/graphics/screen_editor_internal.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "framework/mock_renderer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

constexpr int kSheetWidth = 128;
constexpr int kSheetHeight = 32;
constexpr int kSheetDepth = 8;

gfx::Bitmap MakeSheetBitmap() {
  gfx::Bitmap bmp;
  std::vector<uint8_t> pixels(kSheetWidth * kSheetHeight, 0);
  bmp.Create(kSheetWidth, kSheetHeight, kSheetDepth, pixels);
  return bmp;
}

class GfxGroupAssignmentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000)).ok());
    data.gfx_groups_loaded = true;
    data.graphics_buffer.assign(223 * 4096, 1);
    data.palette_groups.dungeon_main.AddPalette(
        gfx::SnesPalette(std::vector<uint16_t>(90, 0x3DEF)));
    panel.SetRom(&rom);
    panel.SetGameData(&data);
    panel.SetUndoManager(&undo);
    panel.SetMainUsageCallback([](int, int) {
      zelda3::MainGroupUsageReport usage;
      usage.explicit_preview_rooms = {5};
      usage.ow_areas = {0};
      return absl::StatusOr<zelda3::MainGroupUsageReport>(usage);
    });
    rom.set_dirty(false);
  }
  void TearDown() override { gfx::Arena::Get().ClearTextureQueue(); }
  Rom rom;
  zelda3::GameData data;
  UndoManager undo;
  GfxGroupEditor panel;
};

TEST_F(GfxGroupAssignmentTest, UndoTargetsOriginalGroupAfterSelectionChanges) {
  ASSERT_TRUE(panel.EditMainGraphicsSheet(0, 0, 1).ok());
  EXPECT_TRUE(rom.dirty());
  panel.SetSelectedBlockset(1);
  ASSERT_TRUE(undo.Undo().ok());
  EXPECT_EQ(data.main_blockset_ids[0][0], 0);
  EXPECT_EQ(data.main_blockset_ids[1][0], 0);
  ASSERT_TRUE(undo.Redo().ok());
  EXPECT_EQ(data.main_blockset_ids[0][0], 1);
  ASSERT_TRUE(panel.EditMainGraphicsSheet(0, 0, 1).ok());
  EXPECT_EQ(undo.UndoStackSize(), 1);
  EXPECT_FALSE(panel.EditMainGraphicsSheet(0, 8, 1).ok());
  EXPECT_FALSE(panel.EditMainGraphicsSheet(0, 0, 113).ok());
  EXPECT_FALSE(panel.EditMainGraphicsSheet(0, 0, 223).ok());
  EXPECT_FALSE(panel.EditMainGraphicsSheet(-1, 0, 1).ok());
  EXPECT_EQ(undo.UndoStackSize(), 1);
  ASSERT_TRUE(panel.EditMainGraphicsSheet(0, 0, 0).ok());
  EXPECT_EQ(data.main_blockset_ids[0][0], 0);
}

TEST_F(GfxGroupAssignmentTest, OtherHostAssignmentSurvivesStaleUndo) {
  GfxGroupEditor other;
  UndoManager other_undo;
  other.SetRom(&rom);
  other.SetGameData(&data);
  other.SetUndoManager(&other_undo);
  ASSERT_TRUE(panel.EditMainGraphicsSheet(0, 0, 1).ok());
  ASSERT_TRUE(other.EditMainGraphicsSheet(0, 0, 2).ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(undo.Undo()));
  EXPECT_EQ(data.main_blockset_ids[0][0], 2);
  ASSERT_TRUE(other_undo.Undo().ok());
  ASSERT_TRUE(undo.Undo().ok());
  EXPECT_EQ(data.main_blockset_ids[0][0], 0);
}

TEST_F(GfxGroupAssignmentTest, ForeignSessionAndRebindingBackRefuseOldUndo) {
  ASSERT_TRUE(panel.EditMainGraphicsSheet(0, 0, 1).ok());
  zelda3::GameData other;
  panel.SetGameData(&other);
  EXPECT_TRUE(absl::IsFailedPrecondition(undo.Undo()));
  panel.SetGameData(&data);
  EXPECT_TRUE(absl::IsFailedPrecondition(undo.Undo()));
  EXPECT_EQ(data.main_blockset_ids[0][0], 1);
  EXPECT_EQ(other.main_blockset_ids[0][0], 0);
  panel.DetachRuntimeContext();
  EXPECT_TRUE(absl::IsFailedPrecondition(undo.Undo()));
}

TEST_F(GfxGroupAssignmentTest, SameAddressRebindingInvalidatesOldUndo) {
  ASSERT_TRUE(panel.EditMainGraphicsSheet(0, 0, 1).ok());
  panel.SetGameData(&data);
  EXPECT_TRUE(absl::IsFailedPrecondition(undo.Undo()));
  EXPECT_EQ(data.main_blockset_ids[0][0], 1);
}

TEST_F(GfxGroupAssignmentTest, DestroyedPanelRefusesOutstandingUndo) {
  {
    GfxGroupEditor temporary;
    temporary.SetRom(&rom);
    temporary.SetGameData(&data);
    temporary.SetUndoManager(&undo);
    ASSERT_TRUE(temporary.EditMainGraphicsSheet(0, 0, 1).ok());
  }
  EXPECT_TRUE(absl::IsFailedPrecondition(undo.Undo()));
  EXPECT_EQ(data.main_blockset_ids[0][0], 1);
}

TEST_F(GfxGroupAssignmentTest, ThumbnailChoiceAppliesAndNavigationCancels) {
  ImGuiContext* previous = ImGui::GetCurrentContext();
  ImGuiContext* context = ImGui::CreateContext();
  const absl::Cleanup cleanup = [&] {
    ImGui::DestroyContext(context);
    ImGui::SetCurrentContext(previous);
    gui::WidgetIdRegistry::Instance().Clear();
  };
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(1280, 1600);
  io.DeltaTime = 1.0f / 60.0f;
  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  std::string rendered_text;
  auto draw = [&] {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(900, 1500), ImGuiCond_Always);
    ImGui::Begin("MainAssignmentTest", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    {
      gui::AutoWidgetScope scope("MainAssignmentTest");
      ImGui::LogToBuffer(0);
      panel.DrawBlocksetViewer();
      rendered_text = ImGui::GetCurrentContext()->LogBuffer.c_str();
      ImGui::LogFinish();
    }
    ImGui::End();
    ImGui::Render();
    gfx::Arena::Get().ClearTextureQueue();
  };
  auto click = [&](const std::string& label) {
    bool found = false;
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (widget.label != label || !widget.visible || !widget.enabled)
        continue;
      const auto b = widget.bounds;
      io.AddMousePosEvent((b.min_x + b.max_x) / 2, (b.min_y + b.max_y) / 2);
      found = true;
      break;
    }
    ASSERT_TRUE(found) << label;
    draw();
    io.AddMouseButtonEvent(0, true);
    draw();
    io.AddMouseButtonEvent(0, false);
    draw();
    draw();
  };
  draw();
  draw();
  EXPECT_NE(rendered_text.find("Affects 1 room previews and 1 map screens"),
            std::string::npos);
  click("Main slot 4");
  click("Choose replacement sheet");
  click("Main sheet 01");
  EXPECT_FALSE(undo.CanUndo());
  click("Apply to shared main group");
  EXPECT_EQ(data.main_blockset_ids[0][4], 1);
  ASSERT_TRUE(undo.Undo().ok());
  EXPECT_EQ(data.main_blockset_ids[0][4], 0);
  click("Choose replacement sheet");
  click("Main sheet 02");
  data.main_blockset_ids[0][4] = 3;
  click("Apply to shared main group");
  EXPECT_EQ(data.main_blockset_ids[0][4], 3);
  EXPECT_NE(rendered_text.find("Assignment changed while choosing"),
            std::string::npos);
  panel.SetSelectedBlockset(1);
  draw();
  for (const auto& [path, widget] :
       gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
    EXPECT_NE(widget.label, "Apply to shared main group");
  }
}

TEST(BitmapTileExtractionTest,
     Get16x16TileWritesCurrentOffsetAndAdvancesByTileSize) {
  std::vector<uint8_t> pixels(16 * 16);
  for (size_t i = 0; i < pixels.size(); ++i) {
    pixels[i] = static_cast<uint8_t>(i);
  }
  gfx::Bitmap bitmap(16, 16, 8, pixels);

  constexpr int kInitialOffset = 3;
  std::vector<uint8_t> tile_data(kInitialOffset + pixels.size() + 1, 0xEE);
  int tile_data_offset = kInitialOffset;

  bitmap.Get16x16Tile(0, 0, tile_data, tile_data_offset);

  EXPECT_EQ(tile_data[kInitialOffset], pixels.front());
  EXPECT_EQ(tile_data[kInitialOffset + pixels.size() - 1], pixels.back());
  EXPECT_EQ(tile_data[kInitialOffset - 1], 0xEE);
  EXPECT_EQ(tile_data[kInitialOffset + pixels.size()], 0xEE);
  EXPECT_EQ(tile_data_offset, kInitialOffset + static_cast<int>(pixels.size()));
}

class GfxGroupEditorRenderTest : public ::testing::Test {
 protected:
  void SetUp() override { gfx::Arena::Get().ClearTextureQueue(); }
  void TearDown() override { gfx::Arena::Get().ClearTextureQueue(); }
};

TEST_F(GfxGroupEditorRenderTest, PreviewPalettesDoNotModifySharedSource) {
  auto source = MakeSheetBitmap();
  const gfx::SnesPalette source_palette(std::vector<uint16_t>{0, 0x001F});
  const gfx::SnesPalette first_palette(std::vector<uint16_t>{0, 0x03E0});
  const gfx::SnesPalette second_palette(std::vector<uint16_t>{0, 0x7C00});
  source.SetPalette(source_palette);
  const auto purpose = source.metadata().purpose;
  gfx::Bitmap first, second;
  internal::SyncSheetPreview(source, first_palette, first);
  internal::SyncSheetPreview(source, second_palette, second);
  EXPECT_EQ(source.palette(), source_palette);
  EXPECT_EQ(source.metadata().purpose, purpose);
  EXPECT_EQ(first.palette(), first_palette);
  EXPECT_EQ(second.palette(), second_palette);
  EXPECT_EQ(first.vector(), source.vector());
  source.Fill(7);
  internal::SyncSheetPreview(source, first_palette, first);
  EXPECT_EQ(first.vector(), source.vector());
  EXPECT_EQ(second.vector().front(), 0);
}

TEST_F(GfxGroupEditorRenderTest, UnchangedPreviewDoesNotQueueTextureUpdate) {
  auto source = MakeSheetBitmap();
  gfx::Bitmap preview;
  internal::SyncSheetPreview(source, source.palette(), preview);
  gfx::Arena::Get().ClearTextureQueue();
  int texture_tag = 0;
  preview.set_texture(&texture_tag);
  internal::SyncSheetPreview(source, source.palette(), preview);
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 0u);
  preview.set_texture(nullptr);
}

TEST_F(GfxGroupEditorRenderTest, EmptyBitmapIsNoOp) {
  gfx::Bitmap empty;  // No surface, no texture.

  internal::EnsureSheetTextureQueued(empty);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 0u);
  EXPECT_FALSE(empty.is_active());
}

TEST_F(GfxGroupEditorRenderTest, TexturelessSheetGetsCreateQueued) {
  gfx::Bitmap sheet = MakeSheetBitmap();
  ASSERT_NE(sheet.surface(), nullptr)
      << "Bitmap::Create should allocate surface";
  ASSERT_EQ(sheet.texture(), nullptr)
      << "No renderer is wired in unit tests, so texture must remain null";

  const size_t before = gfx::Arena::Get().texture_command_queue_size();
  internal::EnsureSheetTextureQueued(sheet);
  const size_t after = gfx::Arena::Get().texture_command_queue_size();

  EXPECT_EQ(after, before + 1)
      << "Helper must queue exactly one CREATE for a textureless sheet";
  EXPECT_TRUE(sheet.is_active());
  EXPECT_EQ(sheet.metadata().purpose, gfx::Bitmap::BitmapPurpose::kPreview)
      << "Gfx-group viewer sheets are read-only previews; the helper must "
         "stamp purpose so consumers can tell preview bitmaps from editable "
         "ones without consulting the caller.";
}

TEST_F(GfxGroupEditorRenderTest,
       RepeatedCallsQueueReplacementSafeCreatesWhileTextureMissing) {
  // The Arena drains commands in ProcessTextureQueue; until that runs the
  // sheet still has no texture, so calling the helper again still queues.
  // This is safe but not free: each CREATE replaces any texture installed by
  // an earlier CREATE, destroying that texture before allocating another.
  gfx::Bitmap sheet = MakeSheetBitmap();

  internal::EnsureSheetTextureQueued(sheet);
  internal::EnsureSheetTextureQueued(sheet);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 2u);
}

TEST_F(GfxGroupEditorRenderTest, InactiveButSurfacedSheetGetsActivated) {
  gfx::Bitmap sheet = MakeSheetBitmap();
  sheet.set_active(false);  // Simulate a sheet that lost its active flag.
  ASSERT_NE(sheet.surface(), nullptr);

  internal::EnsureSheetTextureQueued(sheet);

  EXPECT_TRUE(sheet.is_active());
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
}

TEST_F(GfxGroupEditorRenderTest,
       BitmapRecreationInvalidatesOldCommandsAndPreservesTextureForUpdate) {
  ::testing::StrictMock<yaze::test::MockRenderer> renderer;
  gfx::Bitmap bitmap = MakeSheetBitmap();
  int old_texture_storage = 0;
  auto old_texture = reinterpret_cast<gfx::TextureHandle>(&old_texture_storage);
  bitmap.set_texture(old_texture);

  const uint32_t old_generation = bitmap.generation();
  bitmap.UpdateTexture();  // Must become stale when Create() replaces data.
  std::vector<uint8_t> replacement_pixels(kSheetWidth * kSheetHeight, 7);
  bitmap.Create(kSheetWidth, kSheetHeight, kSheetDepth, replacement_pixels);
  bitmap.UpdateTexture();

  EXPECT_NE(bitmap.generation(), old_generation);
  ASSERT_EQ(bitmap.texture(), old_texture)
      << "Recreation must retain the texture so UPDATE can reuse it";
  EXPECT_CALL(renderer, DestroyTexture(::testing::_)).Times(0);
  EXPECT_CALL(renderer, CreateTexture(::testing::_, ::testing::_)).Times(0);
  EXPECT_CALL(renderer, UpdateTexture(old_texture, ::testing::Ref(bitmap)))
      .Times(1);

  // The pre-recreation UPDATE is stale; only the UPDATE stamped with the new
  // generation reaches the retained texture.
  gfx::Arena::Get().ProcessTextureQueue(&renderer);

  EXPECT_FALSE(gfx::Arena::Get().HasPendingTextures());
  EXPECT_EQ(bitmap.texture(), old_texture);
  EXPECT_EQ(bitmap.vector().front(), 7);
}

TEST_F(GfxGroupEditorRenderTest,
       BitmapRecreationCreateReplacesTextureInSingleQueuePath) {
  ::testing::StrictMock<yaze::test::MockRenderer> renderer;
  gfx::Bitmap bitmap = MakeSheetBitmap();
  int old_texture_storage = 0;
  int new_texture_storage = 0;
  auto old_texture = reinterpret_cast<gfx::TextureHandle>(&old_texture_storage);
  auto new_texture = reinterpret_cast<gfx::TextureHandle>(&new_texture_storage);
  bitmap.set_texture(old_texture);

  std::vector<uint8_t> replacement_pixels(kSheetWidth * kSheetHeight, 7);
  bitmap.Create(kSheetWidth, kSheetHeight, kSheetDepth, replacement_pixels);
  bitmap.CreateTexture();

  ASSERT_EQ(bitmap.texture(), old_texture)
      << "CREATE processing, not Bitmap::Create, owns texture replacement";

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
        .WillOnce(::testing::Return(new_texture));
    EXPECT_CALL(renderer, UpdateTexture(new_texture, ::testing::Ref(bitmap)));
    EXPECT_CALL(renderer, DestroyTexture(old_texture));
  }

  EXPECT_TRUE(gfx::Arena::Get().ProcessSingleTexture(&renderer));

  EXPECT_FALSE(gfx::Arena::Get().HasPendingTextures());
  EXPECT_EQ(bitmap.texture(), new_texture);
  EXPECT_EQ(bitmap.vector().front(), 7);
}

TEST_F(GfxGroupEditorRenderTest,
       DuplicateCreateCommandsDestroyEachReplacedTextureInBatchPath) {
  ::testing::StrictMock<yaze::test::MockRenderer> renderer;
  gfx::Bitmap bitmap = MakeSheetBitmap();
  int old_texture_storage = 0;
  int intermediate_texture_storage = 0;
  int final_texture_storage = 0;
  auto old_texture = reinterpret_cast<gfx::TextureHandle>(&old_texture_storage);
  auto intermediate_texture =
      reinterpret_cast<gfx::TextureHandle>(&intermediate_texture_storage);
  auto final_texture =
      reinterpret_cast<gfx::TextureHandle>(&final_texture_storage);
  bitmap.set_texture(old_texture);
  bitmap.CreateTexture();
  bitmap.CreateTexture();

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
        .WillOnce(::testing::Return(intermediate_texture));
    EXPECT_CALL(renderer,
                UpdateTexture(intermediate_texture, ::testing::Ref(bitmap)));
    EXPECT_CALL(renderer, DestroyTexture(old_texture));
    EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
        .WillOnce(::testing::Return(final_texture));
    EXPECT_CALL(renderer, UpdateTexture(final_texture, ::testing::Ref(bitmap)));
    EXPECT_CALL(renderer, DestroyTexture(intermediate_texture));
  }

  gfx::Arena::Get().ProcessTextureQueue(&renderer);

  EXPECT_FALSE(gfx::Arena::Get().HasPendingTextures());
  EXPECT_EQ(bitmap.texture(), final_texture);
}

TEST_F(GfxGroupEditorRenderTest,
       SingleCreateNullRetainsOldTextureAndRetriesThroughBudgetPath) {
  ::testing::StrictMock<yaze::test::MockRenderer> renderer;
  gfx::Bitmap bitmap = MakeSheetBitmap();
  int old_texture_storage = 0;
  int new_texture_storage = 0;
  auto old_texture = reinterpret_cast<gfx::TextureHandle>(&old_texture_storage);
  auto new_texture = reinterpret_cast<gfx::TextureHandle>(&new_texture_storage);
  bitmap.set_texture(old_texture);
  bitmap.CreateTexture();

  EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
      .WillOnce(::testing::Return(nullptr));
  EXPECT_FALSE(
      gfx::Arena::Get().ProcessTextureQueueWithBudget(&renderer, 100.0f));
  EXPECT_EQ(bitmap.texture(), old_texture);
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
        .WillOnce(::testing::Return(new_texture));
    EXPECT_CALL(renderer, UpdateTexture(new_texture, ::testing::Ref(bitmap)));
    EXPECT_CALL(renderer, DestroyTexture(old_texture));
  }
  EXPECT_TRUE(
      gfx::Arena::Get().ProcessTextureQueueWithBudget(&renderer, 100.0f));
  EXPECT_FALSE(gfx::Arena::Get().HasPendingTextures());
  EXPECT_EQ(bitmap.texture(), new_texture);
}

TEST_F(GfxGroupEditorRenderTest,
       SingleCreateExceptionsCleanReplacementAndRetryThroughBudgetPath) {
  ::testing::StrictMock<yaze::test::MockRenderer> renderer;
  gfx::Bitmap bitmap = MakeSheetBitmap();
  int old_texture_storage = 0;
  int failed_texture_storage = 0;
  int new_texture_storage = 0;
  auto old_texture = reinterpret_cast<gfx::TextureHandle>(&old_texture_storage);
  auto failed_texture =
      reinterpret_cast<gfx::TextureHandle>(&failed_texture_storage);
  auto new_texture = reinterpret_cast<gfx::TextureHandle>(&new_texture_storage);
  bitmap.set_texture(old_texture);
  bitmap.CreateTexture();

  EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
      .WillOnce(::testing::Throw(std::runtime_error("create failed")));
  EXPECT_FALSE(
      gfx::Arena::Get().ProcessTextureQueueWithBudget(&renderer, 100.0f));
  EXPECT_EQ(bitmap.texture(), old_texture);
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
        .WillOnce(::testing::Return(failed_texture));
    EXPECT_CALL(renderer, UpdateTexture(failed_texture, ::testing::Ref(bitmap)))
        .WillOnce(::testing::Throw(std::runtime_error("update failed")));
    EXPECT_CALL(renderer, DestroyTexture(failed_texture));
  }
  EXPECT_FALSE(
      gfx::Arena::Get().ProcessTextureQueueWithBudget(&renderer, 100.0f));
  EXPECT_EQ(bitmap.texture(), old_texture);
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
        .WillOnce(::testing::Return(new_texture));
    EXPECT_CALL(renderer, UpdateTexture(new_texture, ::testing::Ref(bitmap)));
    EXPECT_CALL(renderer, DestroyTexture(old_texture))
        .WillOnce(::testing::Throw(std::runtime_error("destroy failed")));
  }
  EXPECT_TRUE(
      gfx::Arena::Get().ProcessTextureQueueWithBudget(&renderer, 100.0f));
  EXPECT_FALSE(gfx::Arena::Get().HasPendingTextures());
  EXPECT_EQ(bitmap.texture(), new_texture)
      << "Failure to destroy the old handle must not blank the replacement";
}

TEST_F(GfxGroupEditorRenderTest, BatchCreateNullRetainsOldTextureAndRetries) {
  ::testing::StrictMock<yaze::test::MockRenderer> renderer;
  gfx::Bitmap bitmap = MakeSheetBitmap();
  int old_texture_storage = 0;
  int new_texture_storage = 0;
  auto old_texture = reinterpret_cast<gfx::TextureHandle>(&old_texture_storage);
  auto new_texture = reinterpret_cast<gfx::TextureHandle>(&new_texture_storage);
  bitmap.set_texture(old_texture);
  bitmap.CreateTexture();

  EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
      .WillOnce(::testing::Return(nullptr));
  gfx::Arena::Get().ProcessTextureQueue(&renderer);
  EXPECT_EQ(bitmap.texture(), old_texture);
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
        .WillOnce(::testing::Return(new_texture));
    EXPECT_CALL(renderer, UpdateTexture(new_texture, ::testing::Ref(bitmap)));
    EXPECT_CALL(renderer, DestroyTexture(old_texture));
  }
  gfx::Arena::Get().ProcessTextureQueue(&renderer);
  EXPECT_FALSE(gfx::Arena::Get().HasPendingTextures());
  EXPECT_EQ(bitmap.texture(), new_texture);
}

TEST_F(GfxGroupEditorRenderTest,
       BatchCreateExceptionsCleanReplacementAndRetry) {
  ::testing::StrictMock<yaze::test::MockRenderer> renderer;
  gfx::Bitmap bitmap = MakeSheetBitmap();
  int old_texture_storage = 0;
  int failed_texture_storage = 0;
  int new_texture_storage = 0;
  auto old_texture = reinterpret_cast<gfx::TextureHandle>(&old_texture_storage);
  auto failed_texture =
      reinterpret_cast<gfx::TextureHandle>(&failed_texture_storage);
  auto new_texture = reinterpret_cast<gfx::TextureHandle>(&new_texture_storage);
  bitmap.set_texture(old_texture);
  bitmap.CreateTexture();

  EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
      .WillOnce(::testing::Throw(std::runtime_error("create failed")));
  gfx::Arena::Get().ProcessTextureQueue(&renderer);
  EXPECT_EQ(bitmap.texture(), old_texture);
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
        .WillOnce(::testing::Return(failed_texture));
    EXPECT_CALL(renderer, UpdateTexture(failed_texture, ::testing::Ref(bitmap)))
        .WillOnce(::testing::Throw(std::runtime_error("update failed")));
    EXPECT_CALL(renderer, DestroyTexture(failed_texture));
  }
  gfx::Arena::Get().ProcessTextureQueue(&renderer);
  EXPECT_EQ(bitmap.texture(), old_texture);
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
        .WillOnce(::testing::Return(new_texture));
    EXPECT_CALL(renderer, UpdateTexture(new_texture, ::testing::Ref(bitmap)));
    EXPECT_CALL(renderer, DestroyTexture(old_texture))
        .WillOnce(::testing::Throw(std::runtime_error("destroy failed")));
  }
  gfx::Arena::Get().ProcessTextureQueue(&renderer);
  EXPECT_FALSE(gfx::Arena::Get().HasPendingTextures());
  EXPECT_EQ(bitmap.texture(), new_texture)
      << "Failure to destroy the old handle must not blank the replacement";
}

// EnsureCompositeBitmapTextureQueued mirrors EnsureSheetTextureQueued but
// stamps purpose=kCompositeOutput and additionally honors modified() to queue
// UPDATE for re-rendered composites. These tests pin the truth table that
// fixes the A3/A4 first-frame composite race; if the helper changes shape,
// these tests force the consumer to be re-validated.
constexpr int kCompositeWidth = 256;
constexpr int kCompositeHeight = 256;

gfx::Bitmap MakeCompositeBitmap() {
  gfx::Bitmap bmp;
  std::vector<uint8_t> pixels(kCompositeWidth * kCompositeHeight, 0);
  bmp.Create(kCompositeWidth, kCompositeHeight, 8, pixels);
  return bmp;
}

TEST_F(GfxGroupEditorRenderTest, EnsureCompositeBitmapEmptyIsNoOp) {
  gfx::Bitmap empty;  // No surface (TitleScreen::Create hasn't run yet).

  internal::EnsureCompositeBitmapTextureQueued(empty);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 0u);
  EXPECT_FALSE(empty.is_active());
}

TEST_F(GfxGroupEditorRenderTest,
       EnsureCompositeBitmapQueuesCreateOnFirstSight) {
  gfx::Bitmap composite = MakeCompositeBitmap();
  ASSERT_NE(composite.surface(), nullptr);
  ASSERT_EQ(composite.texture(), nullptr);

  internal::EnsureCompositeBitmapTextureQueued(composite);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u)
      << "First call on a textureless composite must queue exactly one CREATE";
  EXPECT_TRUE(composite.is_active());
  EXPECT_EQ(composite.metadata().purpose,
            gfx::Bitmap::BitmapPurpose::kCompositeOutput)
      << "Composites must be stamped as kCompositeOutput so canvas_rendering's "
         "diagnostic log identifies them as composite outputs (room renderer, "
         "title screen layer composite) rather than editable scratchpads.";
  EXPECT_FALSE(composite.modified())
      << "Helper clears modified after queueing CREATE so the next frame's "
         "modified() check doesn't re-queue UPDATE for the same pixels.";
}

TEST_F(GfxGroupEditorRenderTest,
       EnsureCompositeBitmapInactiveButSurfacedGetsActivated) {
  gfx::Bitmap composite = MakeCompositeBitmap();
  composite.set_active(false);
  ASSERT_NE(composite.surface(), nullptr);

  internal::EnsureCompositeBitmapTextureQueued(composite);

  EXPECT_TRUE(composite.is_active());
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
}

TEST_F(GfxGroupEditorRenderTest,
       EnsureCompositeBitmapDeduplicatesPendingCreateAndPinsPurpose) {
  // Repeated calls before the queue drains share one generation-keyed CREATE.
  // This avoids redundant texture allocation while each call still stamps the
  // bitmap purpose used by canvas diagnostics.
  gfx::Bitmap composite = MakeCompositeBitmap();

  internal::EnsureCompositeBitmapTextureQueued(composite);
  internal::EnsureCompositeBitmapTextureQueued(composite);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
  EXPECT_EQ(composite.metadata().purpose,
            gfx::Bitmap::BitmapPurpose::kCompositeOutput);
}

}  // namespace
}  // namespace yaze::editor
