#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/strings/str_format.h"
#include "app/editor/graphics/graphics_palette_preview.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/editor/graphics/palette_controls_panel.h"
#include "app/editor/graphics/pixel_editor_panel.h"
#include "app/editor/graphics/sheet_browser_panel.h"
#include "app/editor/graphics/ui/usage/usage_preview_view.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/canvas/canvas_rendering.h"
#include "imgui/imgui_internal.h"
#include "zelda3/game_data.h"

namespace yaze::editor {
namespace {

class GraphicsWorkspaceLayoutTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1440, 900);
    io.DeltaTime = 1.0f / 60;
    io.ConfigInputTrickleEventQueue = false;
    // Match SDL/Metal's large-mesh support at high pixel-editor zoom levels.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID((ImTextureID)0x4321);
    bitmap_ = &gfx::Arena::Get().mutable_gfx_sheets()->at(0);
    saved_bitmap_ = std::move(*bitmap_);
    bitmap_->Create(128, 32, 8, std::vector<uint8_t>(4096, 1));
    bitmap_->SetPalette(gfx::SnesPalette(std::vector<uint16_t>(8, 0x7fff)));
    data_ = std::make_unique<zelda3::GameData>();
    data_->graphics_buffer.assign(223 * 4096, 1);
    data_->sheet_store.MarkAllSheetsChanged();
    data_->gfx_bitmaps[0] = *bitmap_;
    previous_owner_ = gfx::Arena::Get().gfx_sheets_owner();
    gfx::Arena::Get().set_gfx_sheets_owner(data_.get());
    AttachSheetStore(state_, data_.get());
    // The headless draw-list test never sends this sentinel to a renderer.
    bitmap_->set_texture(reinterpret_cast<gfx::TextureHandle>(0x1234));
  }

  void TearDown() override {
    bitmap_->set_texture(nullptr);
    *bitmap_ = std::move(saved_bitmap_);
    gfx::Arena::Get().set_gfx_sheets_owner(previous_owner_);
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_);
  }

  void Frame(const std::function<void()>& draw, float width = 640,
             float alpha = 1) {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, 700), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::Begin("GraphicsHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    draw();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();
  }

  ImGuiWindow* ActiveChild(const char* part) {
    for (ImGuiWindow* w : context_->Windows) {
      if (w->Active && std::string(w->Name).find(part) != std::string::npos)
        return w;
    }
    return nullptr;
  }

  std::optional<gui::WidgetIdRegistry::WidgetInfo> FindWidget(
      const char* label) {
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (widget.label == gui::WidgetIdRegistry::NormalizeLabel(label))
        return widget;
    }
    return std::nullopt;
  }

  void Click(const char* label, const std::function<void()>& frame) {
    const auto target = FindWidget(label);
    ASSERT_TRUE(target) << label;
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((target->bounds.min_x + target->bounds.max_x) / 2,
                        (target->bounds.min_y + target->bounds.max_y) / 2);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    frame();
  }

  // Inspect the actual swatch draw geometry, not a helper that could return the
  // right color while the production widget still renders a stale value.
  std::optional<ImU32> SwatchColor(const char* label) {
    const auto widget = FindWidget(label);
    auto* picker = ImGui::FindWindowByName("GraphicsHost");
    if (!widget || !picker)
      return std::nullopt;
    const ImVec2 point(widget->bounds.min_x + 6.5f,
                       widget->bounds.min_y + 6.5f);
    const auto& draw = *picker->DrawList;
    std::optional<ImU32> result;
    for (const auto& command : draw.CmdBuffer) {
      for (unsigned int i = 0; i + 2 < command.ElemCount; i += 3) {
        const auto& a = draw.VtxBuffer[command.VtxOffset +
                                       draw.IdxBuffer[command.IdxOffset + i]];
        const auto& b =
            draw.VtxBuffer[command.VtxOffset +
                           draw.IdxBuffer[command.IdxOffset + i + 1]];
        const auto& c =
            draw.VtxBuffer[command.VtxOffset +
                           draw.IdxBuffer[command.IdxOffset + i + 2]];
        if (a.col == b.col && b.col == c.col &&
            ImTriangleContainsPoint(a.pos, b.pos, c.pos, point)) {
          result = a.col;
        }
      }
    }
    return result;
  }

  ImGuiContext* previous_ = nullptr;
  ImGuiContext* context_ = nullptr;
  GraphicsEditorState state_;
  std::unique_ptr<zelda3::GameData> data_;
  const void* previous_owner_ = nullptr;
  gfx::Bitmap* bitmap_ = nullptr;
  gfx::Bitmap saved_bitmap_;
};

TEST_F(GraphicsWorkspaceLayoutTest,
       SourceColorsFollowPaletteUndoWithoutReplacingPendingPixels) {
  auto& manager = gfx::PaletteManager::Get();
  manager.ResetForTesting();
  absl::Cleanup cleanup = [&] {
    manager.ResetForTesting();
    gfx::Arena::Get().ClearTextureQueue();
  };
  data_->palette_groups.dungeon_main.AddPalette(
      gfx::SnesPalette(std::vector<uint16_t>(90, 0x001F)));
  manager.Initialize(data_.get());
  GraphicsEditContext source;
  source.owner = data_.get();
  source.targets.push_back(
      {.sheet_id = 0,
       .tile_index = 0,
       .palette = gfx::SnesPalette(std::vector<uint16_t>(8, 0x001F))});
  int refreshes = 0;
  source.refresh_palettes = [&](GraphicsEditContext& context) {
    ++refreshes;
    auto fresh = context;
    fresh.targets[0].palette = gfx::SnesPalette(std::vector<uint16_t>(
        8, data_->palette_groups.dungeon_main.palette_ref(0)[0].snes()));
    return UpdateGraphicsContextPalettes(context, std::move(fresh));
  };
  state_.source_context = std::move(source);
  state_.source_context_revision++;
  state_.open_sheets.insert(0);
  auto painted = bitmap_->vector();
  painted[16] = 5;
  ASSERT_TRUE(CommitSheetPixels(state_, 0, painted).ok());
  const auto sheet_revision = data_->sheet_store.Revision(0);
  state_.selection.is_floating = true;
  state_.selection.pixel_data = {2, 3};
  PixelEditorPanel pixel(&state_, nullptr);
  const auto frame = [&] {
    RefreshGraphicsPreviewPalettes(state_);
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(255, 0, 0, 255));
  EXPECT_EQ(refreshes, 1);
  ASSERT_TRUE(
      manager.SetColor("dungeon_main", 0, 0, gfx::SnesColor(0x03E0)).ok());
  frame();
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(0, 255, 0, 255));
  manager.Undo();
  frame();
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(255, 0, 0, 255));
  manager.Redo();
  frame();
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(0, 255, 0, 255));
  manager.DiscardAllChanges();
  frame();
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(255, 0, 0, 255));
  EXPECT_EQ(bitmap_->vector(), painted);
  EXPECT_EQ(data_->sheet_store.Revision(0), sheet_revision);
  EXPECT_TRUE(state_.modified_sheets.contains(0));
  EXPECT_TRUE(state_.selection.is_floating);
  EXPECT_EQ(state_.selection.pixel_data, (std::vector<uint8_t>{2, 3}));

  zelda3::GameData other;
  other.palette_groups.dungeon_main.AddPalette(
      gfx::SnesPalette(std::vector<uint16_t>(90, 0x7C00)));
  manager.Initialize(&other);
  ASSERT_TRUE(
      manager.SetColor("dungeon_main", 0, 0, gfx::SnesColor(0x001F)).ok());
  const int previous_refreshes = refreshes;
  frame();
  EXPECT_EQ(refreshes, previous_refreshes);
  EXPECT_EQ(state_.source_context->targets[0].palette[1].snes(), 0x001F);
  manager.ReleaseSession(&other);
  manager.ActivateSession(data_.get());

  auto changed_source = *state_.source_context;
  changed_source.targets[0].sheet_id = 1;
  changed_source.targets[0].palette =
      gfx::SnesPalette(std::vector<uint16_t>(8, 0x7C00));
  EXPECT_FALSE(UpdateGraphicsContextPalettes(*state_.source_context,
                                             std::move(changed_source))
                   .ok());
  EXPECT_EQ(state_.source_context->targets[0].sheet_id, 0);
  EXPECT_EQ(state_.source_context->targets[0].palette[1].snes(), 0x001F);
}

TEST_F(GraphicsWorkspaceLayoutTest, UsageSourceRendersAfterColorOnlyChange) {
  auto& manager = gfx::PaletteManager::Get();
  manager.ResetForTesting();
  absl::Cleanup cleanup = [&] {
    manager.ResetForTesting();
    gfx::Arena::Get().ClearTextureQueue();
  };
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  data_->palette_groups.dungeon_main.AddPalette(
      gfx::SnesPalette(std::vector<uint16_t>(90, 0x001F)));
  manager.Initialize(data_.get());
  GraphicsEditContext source;
  source.owner = data_.get();
  int renders = 0;
  uint16_t rendered_color = 0;
  source.render_preview = [&]() -> absl::StatusOr<usage_preview::PreviewImage> {
    ++renders;
    rendered_color = manager.GetColor("dungeon_main", 0, 0).snes();
    usage_preview::PreviewImage image;
    image.width = image.height = 1;
    image.pixels = {1};
    return image;
  };
  state_.source_context = std::move(source);
  state_.source_context_revision++;
  UsagePreviewView view(&state_);
  view.SetRom(&rom);
  view.SetGameData(data_.get());
  const auto frame = [&] {
    Frame([&] { view.Draw(); });
  };
  frame();
  frame();
  frame();
  ASSERT_GT(renders, 0);
  EXPECT_EQ(rendered_color, 0x001F);
  const int before = renders;
  ASSERT_TRUE(
      manager.SetColor("dungeon_main", 0, 0, gfx::SnesColor(0x03E0)).ok());
  frame();
  frame();
  EXPECT_GT(renders, before);
  EXPECT_EQ(rendered_color, 0x03E0);
  manager.Undo();
  frame();
  EXPECT_EQ(rendered_color, 0x001F);
  gfx::Arena::Get().ClearTextureQueue();
}

TEST_F(GraphicsWorkspaceLayoutTest,
       CurrentSwatchFollowsSelectionAndPaletteRow) {
  PixelEditorPanel pixel(&state_, nullptr);
  state_.open_sheets.insert(0);
  // The sheet displays one seven-color slice with transparent index zero.
  // The source palette intentionally has different colors at the same indices.
  std::vector<uint16_t> colors(14, 0x001f);
  colors[1] = 0x7c00;
  colors[7] = 0x03e0;
  colors[8] = 0x7c00;
  data_->palette_groups.overworld_main.AddPalette(gfx::SnesPalette(colors));
  ASSERT_TRUE(
      SetGraphicsPreviewPalette(state_, 0, GraphicsPaletteSelection{"ow_main"})
          .ok());
  const auto pixels_before = bitmap_->vector();
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); });
  };
  const auto expect_display_color = [&](ImU32 expected) {
    const auto color = bitmap_->ReadColor(0, 0);  // This pixel is index one.
    EXPECT_EQ(IM_COL32(color.r, color.g, color.b, 255), expected);
    EXPECT_EQ(SwatchColor("Palette index 1"), expected);
    EXPECT_EQ(SwatchColor("Current color"), expected);
  };
  frame();
  frame();
  EXPECT_EQ(state_.current_color_index, 1);
  expect_display_color(IM_COL32(255, 0, 0, 255));
  Click("Next palette row", frame);
  EXPECT_EQ(state_.PreviewPalette(0)->row, 1);
  EXPECT_EQ(state_.current_color_index, 1);
  expect_display_color(IM_COL32(0, 255, 0, 255));
  Click("Palette index 2", frame);
  EXPECT_EQ(state_.current_color_index, 2);
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(0, 0, 255, 255));
  Click("Palette index 1", frame);
  Click("Previous palette row", frame);
  EXPECT_EQ(state_.PreviewPalette(0)->row, 0);
  expect_display_color(IM_COL32(255, 0, 0, 255));
  // A complete three-bit palette may store only seven source colors.
  bitmap_->SetPaletteWithTransparent(
      gfx::SnesPalette(std::vector<uint16_t>(7, 0x03e0)), 0, 7);
  frame();
  Click("Palette index 7", frame);
  EXPECT_EQ(state_.current_color_index, 7);
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(0, 255, 0, 255));
  EXPECT_EQ(bitmap_->vector(), pixels_before);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(GraphicsWorkspaceLayoutTest,
       TransparentAndUnavailableSlotsAreNotColors) {
  PixelEditorPanel pixel(&state_, nullptr);
  state_.open_sheets.insert(0);
  bitmap_->SetPalette(gfx::SnesPalette(std::vector<uint16_t>(8, 0x001f)));
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();
  Click("Palette index 0", frame);
  ASSERT_EQ(state_.current_color_index, 0);
  const auto color = SwatchColor("Current color");
  ASSERT_TRUE(color);
  // A transparent index displays the neutral checkerboard, not palette0 red.
  EXPECT_EQ((*color >> IM_COL32_R_SHIFT) & 255,
            (*color >> IM_COL32_G_SHIFT) & 255);
  EXPECT_EQ((*color >> IM_COL32_G_SHIFT) & 255,
            (*color >> IM_COL32_B_SHIFT) & 255);
  state_.current_color_index = 8;  // Outside the sheet's three-bit indices.
  frame();
  EXPECT_FALSE(FindWidget("Current color"));
  EXPECT_EQ(state_.current_color_index, 8);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(GraphicsWorkspaceLayoutTest,
       CurrentSwatchUsesOnlyTheOwningSourcePalette) {
  zelda3::GameData owner;
  zelda3::GameData other;
  PixelEditorPanel pixel(&state_, nullptr);
  state_.open_sheets.insert(0);
  state_.game_data = &owner;
  GraphicsEditContext context;
  context.owner = &owner;
  context.targets.push_back(
      {.sheet_id = 0,
       .label = "Source tile",
       .palette = gfx::SnesPalette(std::vector<uint16_t>(8, 0x001f))});
  state_.source_context = std::move(context);
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(255, 0, 0, 255));
  state_.source_context->targets[0].palette =
      gfx::SnesPalette(std::vector<uint16_t>(8, 0x03e0));
  frame();
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(0, 255, 0, 255));
  state_.game_data = &other;
  frame();
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(255, 255, 255, 255));
  EXPECT_EQ(state_.current_color_index, 1);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(GraphicsWorkspaceLayoutTest, PresetsApplyTheirNamedPaletteImmediately) {
  struct Preset {
    const char* button;
    const char* group;
    uint16_t color;
    int size;
  };
  const Preset presets[] = {{"Overworld preview", "ow_main", 0x001f, 14},
                            {"Dungeon preview", "dungeon_main", 0x03e0, 30},
                            {"Sprites preview", "sprites_aux1", 0x7c00, 7},
                            {"Link preview", "armors", 0x03ff, 15},
                            {"HUD preview", "hud", 0x7c1f, 16}};
  for (const auto& preset : presets) {
    data_->palette_groups.get_group(preset.group)
        ->AddPalette(
            gfx::SnesPalette(std::vector<uint16_t>(preset.size, preset.color)));
  }
  PaletteControlsPanel palettes(&state_);
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(palettes.Update().ok()); });
  };
  const auto authored = data_->graphics_buffer;
  const auto pixels = bitmap_->vector();
  const auto revision = data_->sheet_store.Revision(0);
  frame();
  frame();
  EXPECT_EQ(state_.PreviewPalette(0), nullptr);
  for (const auto& preset : presets) {
    Click(preset.button, frame);
    ASSERT_NE(state_.PreviewPalette(0), nullptr);
    EXPECT_EQ(state_.PreviewPalette(0)->group, preset.group);
    const auto expected = gfx::SnesColor(preset.color).rgb();
    const auto actual = bitmap_->ReadColor(0, 0);
    EXPECT_EQ(actual.r, expected.x) << preset.button;
    EXPECT_EQ(actual.g, expected.y) << preset.button;
    EXPECT_EQ(actual.b, expected.z) << preset.button;
  }
  Click("Restore loaded colors", frame);
  EXPECT_EQ(state_.PreviewPalette(0), nullptr);
  EXPECT_EQ(bitmap_->ReadColor(0, 0).r, 255);
  EXPECT_EQ(bitmap_->ReadColor(0, 0).g, 255);
  EXPECT_EQ(bitmap_->ReadColor(0, 0).b, 255);
  EXPECT_EQ(data_->graphics_buffer, authored);
  EXPECT_EQ(bitmap_->vector(), pixels);
  EXPECT_EQ(data_->sheet_store.Revision(0), revision);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(GraphicsWorkspaceLayoutTest,
       PixelPaletteChoiceIsExplicitAndRowsAreBounded) {
  data_->palette_groups.overworld_main.AddPalette(
      gfx::SnesPalette(std::vector<uint16_t>(14, 0x001f)));
  PixelEditorPanel pixel(&state_, nullptr);
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();
  EXPECT_EQ(state_.PreviewPalette(0), nullptr);
  EXPECT_FALSE(FindWidget("Next palette row"));
  Click("Preview palette", frame);
  Click("Preview group ow_main", frame);
  ASSERT_NE(state_.PreviewPalette(0), nullptr);
  EXPECT_EQ(state_.PreviewPalette(0)->group, "ow_main");
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(255, 0, 0, 255));
  Click("Next palette row", frame);
  EXPECT_EQ(state_.PreviewPalette(0)->row, 1);
  Click("Next palette row", frame);  // Disabled at the actual final row.
  EXPECT_EQ(state_.PreviewPalette(0)->row, 1);
  EXPECT_FALSE(SetGraphicsPreviewPalette(
                   state_, 0, GraphicsPaletteSelection{"ow_main", 0, 2})
                   .ok());
  EXPECT_FALSE(SetGraphicsPreviewPalette(
                   state_, 0, GraphicsPaletteSelection{"missing", 0, 0})
                   .ok());
  EXPECT_EQ(state_.PreviewPalette(0)->row, 1);
  Click("Preview palette", frame);
  Click("Loaded colors", frame);
  EXPECT_EQ(state_.PreviewPalette(0), nullptr);
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(255, 255, 255, 255));
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(GraphicsWorkspaceLayoutTest,
       PreviewChoicesSurviveSheetNavigationAndRespectArenaOwner) {
  auto& arena = gfx::Arena::Get();
  auto saved_second = std::move(arena.mutable_gfx_sheets()->at(1));
  auto& second = arena.mutable_gfx_sheets()->at(1);
  second.Create(128, 32, 8, std::vector<uint8_t>(4096, 1));
  second.SetPalette(gfx::SnesPalette(std::vector<uint16_t>(8, 0x7fff)));
  data_->gfx_bitmaps[1] = second;
  data_->palette_groups.overworld_main.AddPalette(
      gfx::SnesPalette(std::vector<uint16_t>(7, 0x001f)));
  data_->palette_groups.dungeon_main.AddPalette(
      gfx::SnesPalette(std::vector<uint16_t>(15, 0x03e0)));
  EXPECT_TRUE(
      SetGraphicsPreviewPalette(state_, 0, GraphicsPaletteSelection{"ow_main"})
          .ok());
  state_.SelectSheet(1);
  EXPECT_EQ(state_.PreviewPalette(1), nullptr);
  EXPECT_TRUE(SetGraphicsPreviewPalette(
                  state_, 1, GraphicsPaletteSelection{"dungeon_main"})
                  .ok());
  state_.SelectSheet(0);
  EXPECT_EQ(state_.PreviewPalette(0)->group, "ow_main");
  EXPECT_EQ(bitmap_->ReadColor(0, 0).r, 255);
  EXPECT_EQ(second.ReadColor(0, 0).g, 255);

  zelda3::GameData other;
  other.palette_groups.overworld_main.AddPalette(
      gfx::SnesPalette(std::vector<uint16_t>(7, 0x7c00)));
  GraphicsEditorState other_state;
  AttachSheetStore(other_state, &other);
  arena.set_gfx_sheets_owner(&other);
  EXPECT_FALSE(SetGraphicsPreviewPalette(
                   state_, 0, GraphicsPaletteSelection{"dungeon_main"})
                   .ok());
  EXPECT_TRUE(SetGraphicsPreviewPalette(other_state, 0,
                                        GraphicsPaletteSelection{"ow_main"})
                  .ok());
  RefreshGraphicsPreviewPalettes(
      state_);  // Must not overwrite the other session.
  EXPECT_EQ(bitmap_->ReadColor(0, 0).b, 255);
  arena.set_gfx_sheets_owner(data_.get());
  RefreshGraphicsPreviewPalettes(state_);
  EXPECT_EQ(bitmap_->ReadColor(0, 0).r, 255);
  EXPECT_EQ(bitmap_->ReadColor(0, 0).b, 0);
  EXPECT_EQ(state_.PreviewPalette(1)->group, "dungeon_main");
  // A source palette edit changes the preview without changing its identity.
  data_->palette_groups.overworld_main.SetColor(
      0, 0, gfx::SnesColor(uint16_t{0x03ff}));
  RefreshGraphicsPreviewPalettes(state_);
  EXPECT_EQ(bitmap_->ReadColor(0, 0).g, 255);
  AttachSheetStore(state_,
                   &other);  // Replacement, not activation of an editor.
  EXPECT_TRUE(state_.preview_palettes.empty());
  EXPECT_EQ(other_state.PreviewPalette(0)->group, "ow_main");
  second = std::move(saved_second);
}

TEST_F(GraphicsWorkspaceLayoutTest,
       BrowserStartsWithSheetsAndDetailsRemainReachable) {
  SheetBrowserPanel browser(&state_);
  browser.Initialize();
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(browser.Update().ok()); }, 280);
  };
  frame();
  frame();
  auto* grid = ActiveChild("##SheetGridChild");
  ASSERT_NE(grid, nullptr);
  EXPECT_GT(grid->Size.y, 560);
  EXPECT_EQ(ActiveChild("##SheetDetails"), nullptr);
  auto* thumbnail = ActiveChild("/##Sheet00");
  ASSERT_NE(thumbnail, nullptr);
  EXPECT_LE(thumbnail->Pos.x + thumbnail->Size.x, grid->InnerRect.Max.x);
  Click("Details", frame);
  EXPECT_NE(ActiveChild("##SheetDetails"), nullptr);
  EXPECT_EQ(ActiveChild("##SheetGridChild"), nullptr);
  Click("PNG", frame);
  EXPECT_NE(ActiveChild("##SheetTransfer"), nullptr);
  Click("Sheets", frame);
  Click("Sheet filters", frame);
  EXPECT_TRUE(ImGui::IsPopupOpen("Sheet filters", ImGuiPopupFlags_AnyPopupId));
}

TEST_F(GraphicsWorkspaceLayoutTest,
       PixelToolbarFitsAndViewOptionsAreReachable) {
  PixelEditorPanel pixel(&state_, nullptr);
  for (float width : {440.0f, 640.0f, 980.0f}) {
    const auto frame = [&] {
      Frame([&] { EXPECT_TRUE(pixel.Update().ok()); }, width);
    };
    frame();
    frame();
    auto* host = ImGui::FindWindowByName("GraphicsHost");
    ASSERT_NE(host, nullptr);
    EXPECT_LE(host->ContentSize.x, host->InnerRect.GetWidth() + 1) << width;
    auto* content = ActiveChild("##PixelEditorContent");
    ASSERT_NE(content, nullptr);
    EXPECT_GT(content->Size.y, 580) << width;
    auto* canvas = ActiveChild("##CanvasArea");
    ASSERT_NE(canvas, nullptr);
    EXPECT_GT(canvas->Size.x, width - 40) << width;
  }
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); });
  };
  Click("View options", frame);
  EXPECT_TRUE(ImGui::IsPopupOpen("View options", ImGuiPopupFlags_AnyPopupId));
}

TEST_F(GraphicsWorkspaceLayoutTest,
       LaptopWorkspaceKeepsToolsColorsAndZoomVisibleAtFontScales) {
  for (int i = 0; i < 2; ++i) {
    data_->palette_groups.overworld_main.AddPalette(
        gfx::SnesPalette(std::vector<uint16_t>(14, 0x001f)));
  }
  SheetBrowserPanel browser(&state_);
  browser.Initialize();
  PixelEditorPanel pixel(&state_, nullptr);
  const ImGuiStyle original_style = ImGui::GetStyle();
  auto& io = ImGui::GetIO();
  for (const ImVec2 viewport :
       {ImVec2(1280, 800), ImVec2(1440, 900), ImVec2(1920, 1080)}) {
    for (float scale : {1.0f, 1.25f, 1.5f}) {
      SCOPED_TRACE(absl::StrFormat("%.0fx%.0f scale %.2f", viewport.x,
                                   viewport.y, scale));
      io.DisplaySize = viewport;
      io.FontGlobalScale = scale;
      ImGui::GetStyle() = original_style;
      ImGui::GetStyle().ScaleAllSizes(scale);
      const auto frame = [&] {
        gui::WidgetIdRegistry::Instance().Clear();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(48, 48), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(viewport.x - 64, viewport.y - 100),
                                 ImGuiCond_Always);
        ImGui::Begin("Workspace", nullptr, ImGuiWindowFlags_NoSavedSettings);
        ImGui::BeginChild("Browser", ImVec2(viewport.x * 0.28f, 0));
        EXPECT_TRUE(browser.Update().ok());
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("Pixel", ImVec2(0, 0));
        EXPECT_TRUE(pixel.Update().ok());
        ImGui::EndChild();
        ImGui::End();
        ImGui::Render();
      };
      frame();
      frame();
      const auto first = FindWidget("Select (M)");
      const auto last = FindWidget("Eyedropper (I)");
      const auto undo = FindWidget("Undo pixels");
      const auto options = FindWidget("Edit options");
      ASSERT_TRUE(first && last && undo && options);
      EXPECT_FLOAT_EQ(first->bounds.min_y, last->bounds.min_y);
      EXPECT_FLOAT_EQ(first->bounds.min_y, options->bounds.min_y);
      EXPECT_FLOAT_EQ(first->bounds.max_y, undo->bounds.max_y);
      auto* panel = ActiveChild("/Pixel_");
      auto* canvas = ActiveChild("##CanvasArea");
      ASSERT_NE(panel, nullptr);
      ASSERT_NE(canvas, nullptr);
      EXPECT_LE(panel->ContentSize.x, panel->InnerRect.GetWidth() + 1);
      EXPECT_GT(canvas->Size.x, panel->Size.x - 40 * scale);
      EXPECT_GT(canvas->Size.y, (viewport.y - 100) * 0.60f);
      for (const auto* label :
           {"Palette index 0", "Palette index 7", "Current color", "Zoom out",
            "Zoom in", "Fit sheet"}) {
        const auto widget = FindWidget(label);
        ASSERT_TRUE(widget) << label;
        EXPECT_GE(widget->bounds.min_x, panel->InnerRect.Min.x) << label;
        EXPECT_LE(widget->bounds.max_x, panel->InnerRect.Max.x) << label;
        EXPECT_LE(widget->bounds.max_y, panel->InnerRect.Max.y) << label;
      }
      Click("Brush (P)", frame);
      EXPECT_EQ(state_.current_tool, PixelTool::kBrush);
      Click("Palette index 6", frame);
      EXPECT_EQ(state_.current_color_index, 6);
      ASSERT_TRUE(SetGraphicsPreviewPalette(state_, 0,
                                            GraphicsPaletteSelection{"ow_main"})
                      .ok());
      frame();
      frame();
      for (const auto* label : {"Preview palette index", "Next palette row",
                                "Palette index 7", "Current color"}) {
        const auto widget = FindWidget(label);
        ASSERT_TRUE(widget) << label;
        EXPECT_LE(widget->bounds.max_x, panel->InnerRect.Max.x) << label;
        EXPECT_LE(widget->bounds.max_y, canvas->Pos.y) << label;
      }
      EXPECT_LE(panel->ContentSize.x, panel->InnerRect.GetWidth() + 1);
      Click("Next palette row", frame);
      EXPECT_EQ(state_.PreviewPalette(0)->row, 1);
      ASSERT_TRUE(SetGraphicsPreviewPalette(state_, 0, std::nullopt).ok());
      state_.SetTool(PixelTool::kPencil);
    }
  }
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(GraphicsWorkspaceLayoutTest,
       FitIsExplicitIntegerZoomAndPreservesPendingPixelsAndSelection) {
  PixelEditorPanel pixel(&state_, nullptr);
  auto authored = bitmap_->vector();
  authored[16] = 5;
  ASSERT_TRUE(CommitSheetPixels(state_, 0, authored).ok());
  const auto revision = data_->sheet_store.Revision(0);
  state_.open_sheets.insert(0);
  state_.selection.is_active = true;
  state_.selection.x = 1;
  state_.selection.y = 2;
  state_.selection.width = 8;
  state_.selection.height = 8;
  state_.SetZoom(16);
  float width = 640;
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); }, width);
  };
  frame();
  frame();
  auto* canvas = ActiveChild("##CanvasArea");
  ASSERT_NE(canvas, nullptr);
  canvas->Scroll.x = 200;
  frame();
  Click("Fit sheet", frame);
  EXPECT_FLOAT_EQ(state_.zoom_level, 4);
  EXPECT_FLOAT_EQ(canvas->Scroll.x, 0);
  EXPECT_FLOAT_EQ(canvas->Scroll.y, 0);
  EXPECT_EQ(bitmap_->vector(), authored);
  EXPECT_EQ(data_->sheet_store.Revision(0), revision);
  EXPECT_TRUE(state_.modified_sheets.contains(0));
  EXPECT_TRUE(state_.selection.is_active);
  EXPECT_EQ(state_.selection.x, 1);
  EXPECT_EQ(state_.selection.y, 2);
  width = 980;
  frame();
  frame();
  EXPECT_FLOAT_EQ(state_.zoom_level, 4);  // Window resize does not refit.
  Click("Zoom in", frame);
  EXPECT_FLOAT_EQ(state_.zoom_level, 5);
  Click("Zoom out", frame);
  EXPECT_FLOAT_EQ(state_.zoom_level, 4);
  Click("Fit sheet", frame);
  EXPECT_FLOAT_EQ(state_.zoom_level, 7);
}

TEST_F(GraphicsWorkspaceLayoutTest, PixelTextureUsesTheSameFadeAsItsWindow) {
  PixelEditorPanel pixel(&state_, nullptr);
  for (float alpha : {0.25f, 0.5f, 1.0f}) {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); }, 640, alpha);
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); }, 640, alpha);
    auto* canvas = ActiveChild("##CanvasArea");
    ASSERT_NE(canvas, nullptr);
    // Checkerboard, grid, and selection marks must not flash opaque either.
    for (const auto& vertex : canvas->DrawList->VtxBuffer) {
      EXPECT_LE((vertex.col >> IM_COL32_A_SHIFT) & 0xff, alpha * 255 + 1);
    }
    int samples = 0;
    for (const auto& command : canvas->DrawList->CmdBuffer) {
      if (command.GetTexID() != (ImTextureID)0x1234)
        continue;
      for (unsigned int i = 0; i < command.ElemCount; ++i) {
        const auto index = canvas->DrawList->IdxBuffer[command.IdxOffset + i];
        const auto color =
            canvas->DrawList->VtxBuffer[command.VtxOffset + index].col;
        EXPECT_NEAR((color >> IM_COL32_A_SHIFT) & 0xff, alpha * 255, 1);
        ++samples;
      }
    }
    EXPECT_GE(samples, 6);
  }
}

TEST_F(GraphicsWorkspaceLayoutTest,
       SharedCanvasPreservesLocalAlphaAndUvDuringFade) {
  Frame(
      [&] {
        auto* draw = ImGui::GetWindowDrawList();
        gui::CanvasGeometry geometry;
        geometry.canvas_p0 = ImVec2(50, 50);
        const int first = draw->VtxBuffer.Size;
        gui::RenderBitmapOnCanvas(draw, geometry, *bitmap_, 0, 1.0f);
        gui::RenderBitmapOnCanvas(draw, geometry, *bitmap_, 0, 0, 1.0f, 128);
        gui::RenderBitmapOnCanvas(draw, geometry, *bitmap_, ImVec2(0, 0),
                                  ImVec2(16, 16), ImVec2(8, 8), ImVec2(16, 16));
        ASSERT_EQ(draw->VtxBuffer.Size - first, 12);
        for (int i = 0; i < 12; ++i) {
          const int expected = i >= 4 && i < 8 ? 64 : 128;
          EXPECT_NEAR(
              (draw->VtxBuffer[first + i].col >> IM_COL32_A_SHIFT) & 255,
              expected, 1);
        }
        EXPECT_FLOAT_EQ(draw->VtxBuffer[first + 8].uv.x, 8.0f / 128);
        EXPECT_FLOAT_EQ(draw->VtxBuffer[first + 8].uv.y, 8.0f / 32);
      },
      640, 0.5f);
}

}  // namespace
}  // namespace yaze::editor
