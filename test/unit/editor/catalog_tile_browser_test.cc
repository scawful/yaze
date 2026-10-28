#include "app/editor/graphics/catalog_tile_browser.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/editor/registry/undo_manager.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/platform/sdl_compat.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics_sheet_store.h"

namespace yaze::editor {
namespace {
class CatalogTileBrowserTest : public testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1000, 1800);
    io.DeltaTime = 1.f / 60.f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    data_ = std::make_unique<zelda3::GameData>();
    data_->graphics_buffer.assign(223 * 4096, 1);
    data_->sheet_store.MarkAllSheetsChanged();
    auto& arena = gfx::Arena::Get();
    previous_owner_ = arena.gfx_sheets_owner();
    previous_bitmap_ = std::move(arena.mutable_gfx_sheets()->at(0x20));
    arena.mutable_gfx_sheets()->at(0x20).Create(128, 32, 8,
                                                std::vector<uint8_t>(4096, 1));
    arena.set_gfx_sheets_owner(data_.get());
    AttachSheetStore(state_, data_.get());
    state_.game_data = data_.get();
    state_.current_sheet_id = 0x20;
    auto& bitmap = data_->gfx_bitmaps[0x20];
    bitmap.Create(128, 32, 8, std::vector<uint8_t>(4096, 1));
    std::vector<SDL_Color> colors(8, SDL_Color{0, 0, 0, 255});
    colors[1] = {248, 0, 0, 255};
    bitmap.SetPalette(colors);
    browser_.sources.push_back({"source", "Source sheet", "read-only.sfc",
                                std::vector<uint8_t>(4096, 1)});
    browser_.source = 0;
    browser_.palette = -2;
  }
  void TearDown() override {
    CancelCatalogArtPreview(browser_);
    gfx::Arena::Get().ClearTextureQueue();
    gfx::Arena::Get().mutable_gfx_sheets()->at(0x20) =
        std::move(previous_bitmap_);
    gfx::Arena::Get().set_gfx_sheets_owner(previous_owner_);
    gui::WidgetIdRegistry::Instance().Clear();
    data_.reset();
    ImGui::DestroyContext(context_);
  }
  int DrawAndCountColor(ImU32 color) {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(950, 1750), ImGuiCond_Always);
    ImGui::Begin("Catalog browser host");
    auto* window = ImGui::GetCurrentWindow();
    const int depth = context_->CurrentWindowStack.Size;
    DrawCatalogTileBrowser(browser_, state_, &undo_, "");
    EXPECT_EQ(window, ImGui::GetCurrentWindow());
    EXPECT_EQ(depth, context_->CurrentWindowStack.Size);
    const auto& vertices = window->DrawList->VtxBuffer;
    int count = 0;
    for (const auto& vertex : vertices)
      if (vertex.col == color)
        ++count;
    ImGui::End();
    ImGui::Render();
    gfx::Arena::Get().ClearTextureQueue();
    return count;
  }
  std::vector<uint8_t> Pixels() const {
    const auto pixels = data_->sheet_store.Sheet(0x20);
    return {pixels.begin(), pixels.end()};
  }
  void BindObjectArt() {
    auto& source = browser_.sources.front();
    source.asset_id = source.id;
    source.indexed_pixels.assign(4096, 3);
    source.pixel_width = source.pixel_height = 8;
    source.assembly = {{0, 0, 0, 0}};
    source.palette = {0, 0, 0x03e0, 0x7c00, 0, 0, 0, 0};
    browser_.palette = -3;
    browser_.asset_cache_id.clear();
    browser_.asset_preview.reset();
    live_source_ = std::make_unique<GraphicsEditContext>();
    live_source_->owner = data_.get();
    live_source_->label = "Fixture selected object";
    live_source_->pattern_width = live_source_->pattern_height = 8;
    live_source_->targets.resize(1);
    live_source_->targets[0].sheet_id = 0x20;
    live_source_->targets[0].tile_index = 5;
    live_source_->pattern_cells = {{0, 0, 0}};
    live_source_->resolve_live_source = [this](GraphicsEditContext& fresh) {
      ++resolve_count_;
      fresh = *live_source_;
      return absl::OkStatus();
    };
    live_source_->preview_revision = [this]() -> absl::StatusOr<uint64_t> {
      return preview_revision_;
    };
    live_source_->render_staged_preview =
        [this](const std::map<uint16_t, std::vector<uint8_t>>& overrides)
        -> absl::StatusOr<usage_preview::PreviewImage> {
      ++render_count_;
      last_overrides_ = overrides;
      usage_preview::PreviewImage image;
      image.width = image.height = 8;
      image.index0_transparent = true;
      image.colors[3] = {0, 0, 248, 255};
      for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
          image.pixels.push_back(overrides.at(0x20)[y * 128 + 40 + x]);
      return image;
    };
    state_.source_context = *live_source_;
    ++state_.source_context_revision;
  }
  bool HasWidget(const std::string& label) {
    const auto widgets = gui::WidgetIdRegistry::Instance().GetAllWidgets();
    return std::any_of(widgets.begin(), widgets.end(), [&](const auto& item) {
      return item.second.label == label;
    });
  }
  void ClickWidget(const std::string& label) {
    const auto widgets = gui::WidgetIdRegistry::Instance().GetAllWidgets();
    auto found =
        std::find_if(widgets.begin(), widgets.end(), [&](const auto& item) {
          return item.second.label == label && item.second.visible &&
                 item.second.enabled;
        });
    ASSERT_NE(found, widgets.end()) << label;
    const auto& bounds = found->second.bounds;
    const auto* host = ImGui::FindWindowByName("Catalog browser host");
    ASSERT_NE(host, nullptr);
    const ImVec2 center((bounds.min_x + bounds.max_x) / 2,
                        (bounds.min_y + bounds.max_y) / 2);
    ASSERT_TRUE(host->InnerClipRect.Contains(center))
        << label << " is outside the host viewport at " << center.x << ","
        << center.y;
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(center.x, center.y);
    DrawAndCountColor(0);
    io.AddMouseButtonEvent(0, true);
    DrawAndCountColor(0);
    io.AddMouseButtonEvent(0, false);
    DrawAndCountColor(0);
    DrawAndCountColor(0);
  }
  void EnterWidgetValue(const std::string& label, const char* value) {
    const auto widgets = gui::WidgetIdRegistry::Instance().GetAllWidgets();
    const auto found =
        std::find_if(widgets.begin(), widgets.end(), [&](const auto& item) {
          return item.second.label == label && item.second.visible &&
                 item.second.enabled;
        });
    ASSERT_NE(found, widgets.end()) << label;
    auto& io = ImGui::GetIO();
    // InputInt's registered bounds include its step buttons and label. Focus
    // the editable text at the left before sending actual keyboard events.
    io.AddMousePosEvent(
        found->second.bounds.min_x + 10,
        (found->second.bounds.min_y + found->second.bounds.max_y) / 2);
    DrawAndCountColor(0);
    io.AddMouseButtonEvent(0, true);
    DrawAndCountColor(0);
    io.AddMouseButtonEvent(0, false);
    DrawAndCountColor(0);
    const ImGuiKey command =
        io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
    io.AddKeyEvent(command, true);
    io.AddKeyEvent(ImGuiKey_A, true);
    DrawAndCountColor(0);
    io.AddKeyEvent(ImGuiKey_A, false);
    io.AddKeyEvent(command, false);
    DrawAndCountColor(0);
    io.AddInputCharactersUTF8(value);
    DrawAndCountColor(0);
    io.AddKeyEvent(ImGuiKey_Enter, true);
    DrawAndCountColor(0);
    io.AddKeyEvent(ImGuiKey_Enter, false);
    DrawAndCountColor(0);
  }
  ImGuiContext* context_;
  std::unique_ptr<zelda3::GameData> data_;
  GraphicsEditorState state_;
  UndoManager undo_;
  CatalogTileBrowserState browser_;
  gfx::Bitmap previous_bitmap_;
  const void* previous_owner_ = nullptr;
  std::unique_ptr<GraphicsEditContext> live_source_;
  uint64_t preview_revision_ = 7;
  int render_count_ = 0;
  int resolve_count_ = 0;
  std::map<uint16_t, std::vector<uint8_t>> last_overrides_;
};

TEST_F(CatalogTileBrowserTest, LoadedDestinationColorsAreUsedWithoutOverride) {
  DrawAndCountColor(IM_COL32(248, 0, 0, 255));
  EXPECT_GT(DrawAndCountColor(IM_COL32(248, 0, 0, 255)), 100);
  EXPECT_FALSE(state_.PreviewPalette(0x20));
  EXPECT_TRUE(state_.modified_sheets.empty());
  EXPECT_FALSE(undo_.CanUndo());
  EXPECT_EQ(browser_.sources[0].indexed_pixels, std::vector<uint8_t>(4096, 1));
}

TEST_F(CatalogTileBrowserTest, SwitchingDestinationOwnerDiscardsPendingCopy) {
  auto plan =
      PreviewCatalogTileTransfer(0x20, std::vector<uint8_t>(4096, 0),
                                 browser_.sources[0].indexed_pixels, {0}, 0);
  ASSERT_TRUE(plan.ok());
  browser_.pending = *plan;
  // This preview belongs to a prior session, even if its pixels were identical.
  browser_.destination_owner = nullptr;
  DrawAndCountColor(IM_COL32(248, 0, 0, 255));
  EXPECT_FALSE(browser_.pending);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(CatalogTileBrowserTest,
       FailedLoadRetainsLoadedLibrarySelectionAndCandidate) {
  browser_.loaded_path = "retained.catalog.json";
  browser_.selected = {2, 3};
  browser_.object_pending.emplace();
  const auto status =
      LoadCatalogTileBrowser(browser_, "missing-yaze-catalog.json");
  EXPECT_FALSE(status.ok());
  ASSERT_EQ(browser_.sources.size(), 1u);
  EXPECT_EQ(browser_.loaded_path, "retained.catalog.json");
  EXPECT_EQ(browser_.source, 0);
  EXPECT_EQ(browser_.selected, (std::set<int>{2, 3}));
  EXPECT_TRUE(browser_.object_pending);
  EXPECT_NE(browser_.status.find("previous library retained"),
            std::string::npos);
}

TEST_F(CatalogTileBrowserTest,
       CompleteMultiPageAssetUsesOwnColorsAndRegistersControls) {
  auto first = browser_.sources.front();
  first.id = "fixture.arch.page-0";
  first.asset_id = "fixture.arch";
  first.name = "Two-page fixture arch";
  first.group = "Dungeon";
  first.category = "arch";
  first.page_count = 2;
  first.pixel_width = 16;
  first.pixel_height = 8;
  first.indexed_pixels.assign(4096, 2);
  first.palette = {0, 0, 0x03e0, 0x7c00, 0, 0, 0, 0};
  first.assembly = {{0, 0, 0, 0}, {1, 0, 1, 0}};
  auto second = first;
  second.id = "fixture.arch.page-1";
  second.page = 1;
  second.indexed_pixels.assign(4096, 3);
  browser_.sources = {first, second};
  browser_.palette = -3;
  DrawAndCountColor(IM_COL32(0, 255, 0, 255));
  EXPECT_GT(DrawAndCountColor(IM_COL32(0, 255, 0, 255)), 10);
  ASSERT_TRUE(browser_.asset_preview);
  EXPECT_EQ(browser_.asset_preview->width, 16);
  EXPECT_EQ(browser_.asset_preview->indexed_pixels[0], 2);
  EXPECT_EQ(browser_.asset_preview->indexed_pixels[8], 3);
  EXPECT_EQ(browser_.art_selection.width, 16);
  EXPECT_TRUE(HasWidget("Logical asset pixels"));
  EXPECT_TRUE(HasWidget("Select whole asset"));
  EXPECT_TRUE(HasWidget("Try on selected object"));
  EXPECT_TRUE(HasWidget("Group"));
  EXPECT_TRUE(HasWidget("Category"));
  EXPECT_FALSE(state_.HasUnsavedChanges());
  EXPECT_FALSE(undo_.CanUndo());
}

TEST_F(CatalogTileBrowserTest,
       TryRendersDetachedOnceAndCancelPreservesDestination) {
  BindObjectArt();
  const auto before = Pixels();
  const auto revision = data_->sheet_store.Revision(0x20);
  ASSERT_TRUE(TryCatalogArtOnObject(browser_, state_).ok());
  EXPECT_EQ(render_count_, 1);
  ASSERT_TRUE(browser_.object_pending);
  EXPECT_EQ(last_overrides_.at(0x20)[40], 3);
  EXPECT_EQ(Pixels(), before);
  EXPECT_EQ(data_->sheet_store.Revision(0x20), revision);
  EXPECT_FALSE(state_.HasUnsavedChanges());
  EXPECT_FALSE(undo_.CanUndo());
  const auto resolved_after_try = resolve_count_;
  DrawAndCountColor(IM_COL32(0, 0, 255, 255));
  DrawAndCountColor(IM_COL32(0, 0, 255, 255));
  EXPECT_EQ(render_count_, 1);
  EXPECT_EQ(resolve_count_, resolved_after_try);
  EXPECT_TRUE(HasWidget("Apply imported art"));
  EXPECT_TRUE(HasWidget("Cancel imported art"));
  CancelCatalogArtPreview(browser_);
  EXPECT_FALSE(browser_.object_pending);
  EXPECT_FALSE(browser_.staged_bitmap.is_active());
  EXPECT_EQ(Pixels(), before);
  EXPECT_FALSE(undo_.CanUndo());
}

TEST_F(CatalogTileBrowserTest, ApplyUsesOneUndoAndKeepsDestinationPalette) {
  BindObjectArt();
  const auto before = Pixels();
  const auto* palette =
      platform::GetSurfacePalette(data_->gfx_bitmaps[0x20].surface());
  ASSERT_NE(palette, nullptr);
  const auto red = palette->colors[1];
  ASSERT_TRUE(TryCatalogArtOnObject(browser_, state_).ok());
  ASSERT_TRUE(ApplyCatalogArtToObject(browser_, state_, undo_).ok());
  EXPECT_EQ(undo_.UndoStackSize(), 1u);
  EXPECT_EQ(Pixels()[40], 3);
  EXPECT_FALSE(browser_.object_pending);
  EXPECT_EQ(palette->colors[1].r, red.r);
  EXPECT_EQ(palette->colors[1].g, red.g);
  EXPECT_EQ(palette->colors[1].b, red.b);
  ASSERT_TRUE(undo_.Undo().ok());
  EXPECT_EQ(Pixels(), before);
  ASSERT_TRUE(undo_.Redo().ok());
  EXPECT_EQ(Pixels()[40], 3);
}

TEST_F(CatalogTileBrowserTest,
       StoreContextAndLiveSourceChangesInvalidateCandidate) {
  BindObjectArt();
  ASSERT_TRUE(TryCatalogArtOnObject(browser_, state_).ok());
  data_->sheet_store.MarkAllSheetsChanged();
  EXPECT_FALSE(ValidateCatalogArtPreview(browser_, state_).ok());
  EXPECT_FALSE(browser_.object_pending);
  ASSERT_TRUE(TryCatalogArtOnObject(browser_, state_).ok());
  ++state_.source_context_revision;
  EXPECT_FALSE(ValidateCatalogArtPreview(browser_, state_).ok());
  EXPECT_FALSE(browser_.object_pending);
  ASSERT_TRUE(TryCatalogArtOnObject(browser_, state_).ok());
  ++preview_revision_;
  EXPECT_FALSE(ValidateCatalogArtPreview(browser_, state_).ok());
  EXPECT_FALSE(browser_.object_pending);
  ASSERT_TRUE(TryCatalogArtOnObject(browser_, state_).ok());
  live_source_->pattern_cells[0].hflip = true;
  EXPECT_FALSE(ValidateCatalogArtPreview(browser_, state_).ok());
  EXPECT_FALSE(browser_.object_pending);
  EXPECT_FALSE(undo_.CanUndo());
}

TEST_F(CatalogTileBrowserTest,
       CropChangeOversizeAndUnavailableRendererRefuseSafely) {
  BindObjectArt();
  ASSERT_TRUE(TryCatalogArtOnObject(browser_, state_).ok());
  browser_.art_selection.x = 8;
  EXPECT_FALSE(ValidateCatalogArtPreview(browser_, state_).ok());
  browser_.art_selection = {0, 0, 16, 8};
  const auto refused = TryCatalogArtOnObject(browser_, state_);
  EXPECT_FALSE(refused.ok());
  EXPECT_FALSE(browser_.object_pending);
  browser_.art_selection = {0, 0, 8, 8};
  state_.source_context->render_staged_preview = {};
  EXPECT_FALSE(TryCatalogArtOnObject(browser_, state_).ok());
  EXPECT_EQ(render_count_, 1);
  EXPECT_FALSE(undo_.CanUndo());
  EXPECT_EQ(Pixels(), std::vector<uint8_t>(4096, 1));
}

TEST_F(CatalogTileBrowserTest,
       KeyboardCropCommitsAlignedBoundsAndDiscardsCandidate) {
  BindObjectArt();
  auto& source = browser_.sources.front();
  source.pixel_width = source.pixel_height = 16;
  source.assembly = {{0, 0, 0, 0}, {1, 0, 0, 1}, {0, 1, 0, 16}, {1, 1, 0, 17}};
  live_source_->pattern_width = live_source_->pattern_height = 16;
  live_source_->targets.resize(4);
  for (int i = 0; i < 4; ++i) {
    live_source_->targets[i].sheet_id = 0x20;
    live_source_->targets[i].tile_index = 5 + i;
  }
  live_source_->pattern_cells = {{0, 0, 0}, {1, 0, 1}, {0, 1, 2}, {1, 1, 3}};
  state_.source_context = *live_source_;
  DrawAndCountColor(0);
  DrawAndCountColor(0);
  ASSERT_TRUE(TryCatalogArtOnObject(browser_, state_).ok());
  ASSERT_TRUE(browser_.object_pending);
  EnterWidgetValue("Crop width (tiles)", "1");
  EXPECT_EQ(browser_.art_selection.width, 8);
  EXPECT_EQ(browser_.art_selection.height, 16);
  EXPECT_FALSE(browser_.object_pending);
  EXPECT_FALSE(browser_.staged_bitmap.is_active());
  EnterWidgetValue("Crop X (tiles)", "999");
  EnterWidgetValue("Crop height (tiles)", "0");
  EnterWidgetValue("Crop Y (tiles)", "999");
  EnterWidgetValue("Crop width (tiles)", "999");
  EXPECT_EQ(browser_.art_selection.x, 8);
  EXPECT_EQ(browser_.art_selection.y, 8);
  EXPECT_EQ(browser_.art_selection.width, 8);
  EXPECT_EQ(browser_.art_selection.height, 8);
  EnterWidgetValue("Crop X (tiles)", "-5");
  EXPECT_EQ(browser_.art_selection.x, 0);
  EXPECT_EQ(render_count_, 1);
  EXPECT_EQ(Pixels(), std::vector<uint8_t>(4096, 1));
  EXPECT_FALSE(state_.HasUnsavedChanges());
  EXPECT_FALSE(undo_.CanUndo());
}

TEST_F(CatalogTileBrowserTest,
       MissingSourceColorsAndFourBppEightColorModesUseExplicitFallback) {
  browser_.palette = -3;
  DrawAndCountColor(0);
  EXPECT_EQ(browser_.palette, -1);
  BindObjectArt();
  auto& source = browser_.sources.front();
  source.bpp = 4;
  source.palette.resize(16);
  browser_.palette = -2;
  DrawAndCountColor(0);
  EXPECT_EQ(browser_.palette, -3);
  browser_.palette = 0;
  browser_.palettes.emplace_back();
  DrawAndCountColor(0);
  EXPECT_EQ(browser_.palette, -3);
  EXPECT_FALSE(state_.HasUnsavedChanges());
  EXPECT_FALSE(undo_.CanUndo());
}

TEST_F(CatalogTileBrowserTest, RegisteredTryCancelAndApplyDriveTheActualUi) {
  BindObjectArt();
  const auto before = Pixels();
  DrawAndCountColor(0);
  DrawAndCountColor(0);
  ClickWidget("Try on selected object");
  ASSERT_TRUE(browser_.object_pending);
  EXPECT_EQ(render_count_, 1);
  EXPECT_EQ(Pixels(), before);
  const auto preview = std::find_if(
      context_->Windows.begin(), context_->Windows.end(),
      [](const auto* window) {
        return std::string(window->Name).find("/CatalogDetachedPreview_") !=
               std::string::npos;
      });
  ASSERT_NE(preview, context_->Windows.end());
  EXPECT_LE((*preview)->Size.y, 330.f);
  ClickWidget("Cancel imported art");
  EXPECT_FALSE(browser_.object_pending);
  EXPECT_EQ(Pixels(), before);
  EXPECT_FALSE(undo_.CanUndo());
  ClickWidget("Try on selected object");
  ASSERT_TRUE(browser_.object_pending);
  ClickWidget("Apply imported art");
  EXPECT_FALSE(browser_.object_pending);
  EXPECT_EQ(Pixels()[40], 3);
  EXPECT_EQ(undo_.UndoStackSize(), 1u);
  ASSERT_TRUE(undo_.Undo().ok());
  EXPECT_EQ(Pixels(), before);
}

TEST_F(CatalogTileBrowserTest, RendererFailureLeavesNoCandidateOrPixelChanges) {
  BindObjectArt();
  state_.source_context->render_staged_preview =
      [](const std::map<uint16_t, std::vector<uint8_t>>&)
      -> absl::StatusOr<usage_preview::PreviewImage> {
    return absl::FailedPreconditionError("Fixture renderer refused");
  };
  const auto status = TryCatalogArtOnObject(browser_, state_);
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(status.message(), "Fixture renderer refused");
  EXPECT_FALSE(browser_.object_pending);
  EXPECT_FALSE(browser_.staged_bitmap.is_active());
  EXPECT_FALSE(state_.HasUnsavedChanges());
  EXPECT_FALSE(undo_.CanUndo());
  EXPECT_EQ(Pixels(), std::vector<uint8_t>(4096, 1));
}
}  // namespace
}  // namespace yaze::editor
