#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
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
#include "app/gui/core/icons.h"
#include "imgui/imgui_internal.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/game_data.h"

namespace yaze::editor {
class UsagePreviewViewTestPeer {
 public:
  static gfx::Bitmap& SourceBitmap(UsagePreviewView& view) {
    return view.source_bitmap_;
  }
  static bool SharedScanReady(const UsagePreviewView& view) {
    return view.shared_scan_ready_;
  }
  static const auto& SharedUses(const UsagePreviewView& view) {
    return view.shared_uses_;
  }
  static const auto& SharedTargets(const UsagePreviewView& view) {
    return view.shared_targets_;
  }
  static int SharedPreviewRoom(const UsagePreviewView& view) {
    return view.shared_preview_room_;
  }
  static size_t SharedOutlineCount(const UsagePreviewView& view) {
    return view.shared_room_preview_
        .ObjectRectsForObject(view.shared_preview_object_)
        .size();
  }
};
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

  std::optional<ImVec2> SheetOrigin() {
    auto* window = ActiveChild("##CanvasArea");
    if (!window)
      return std::nullopt;
    for (const auto& command : window->DrawList->CmdBuffer) {
      if (command.GetTexID() == (ImTextureID)0x1234 && command.ElemCount) {
        const auto index = window->DrawList->IdxBuffer[command.IdxOffset];
        return window->DrawList->VtxBuffer[command.VtxOffset + index].pos;
      }
    }
    return std::nullopt;
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

  void BindReorderSource() {
    data_->palette_groups.dungeon_main.AddPalette(gfx::SnesPalette(
        std::vector<uint16_t>{31, 992, 31744, 1023, 31775, 32736, 32767}));
    gfx::PaletteManager::Get().Initialize(data_.get());
    GraphicsEditContext source;
    source.owner = data_.get();
    source.label = "Two-tile source pattern";
    for (uint16_t tile = 0; tile < 2; ++tile) {
      GraphicsEditTarget target;
      target.tile_index = tile;
      target.label = absl::StrFormat("Tile %d", tile);
      target.palette = gfx::SnesPalette(std::vector<uint16_t>(8, 0));
      GraphicsPaletteAddressing binding;
      binding.owner = data_.get();
      binding.row = 2;
      for (int raw = 1; raw < 8; ++raw) {
        binding.colors[raw] = zelda3::PaletteColorAddress{
            zelda3::PaletteAddressGroup::kDungeonMain, 0, raw - 1};
        target.palette[raw] =
            data_->palette_groups.dungeon_main.palette_ref(0)[raw - 1];
      }
      target.writable_palette = binding;
      ASSERT_TRUE(ValidateGraphicsPaletteAddressing(target, data_.get()).ok());
      source.targets.push_back(target);
    }
    source.resolve_live_source = [this, source](GraphicsEditContext& fresh) {
      fresh = source;
      for (auto& target : fresh.targets)
        for (int raw = 1; raw < 8; ++raw)
          target.palette[raw] =
              data_->palette_groups.dungeon_main.palette_ref(0)[raw - 1];
      return absl::OkStatus();
    };
    state_.source_context = std::move(source);
    state_.open_sheets.insert(0);
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
       PixelToolbarCentersCompleteMaterialGlyphsAtFontScales) {
  // Match the shipped Karla + Material merge, including its inline-text
  // baseline offset. The default test font has no Material glyphs and cannot
  // expose the icon displacement seen in the application.
  const auto font_dir = std::filesystem::path(__FILE__)
                            .parent_path()
                            .parent_path()
                            .parent_path()
                            .parent_path() /
                        "assets/font";
  const auto text_font = font_dir / "Karla-Regular.ttf";
  const auto icon_font = font_dir / "MaterialIcons-Regular.ttf";
  ASSERT_TRUE(std::filesystem::is_regular_file(text_font));
  ASSERT_TRUE(std::filesystem::is_regular_file(icon_font));
  auto& io = ImGui::GetIO();
  io.Fonts->Clear();
  io.FontDefault = io.Fonts->AddFontFromFileTTF(text_font.string().c_str(), 16);
  ASSERT_NE(io.FontDefault, nullptr);
  ImFontConfig icons;
  icons.MergeMode = true;
  icons.GlyphOffset.y = 5;
  icons.GlyphMinAdvanceX = 13;
  icons.PixelSnapH = true;
  static const ImWchar ranges[] = {ICON_MIN_MD, 0xf900, 0};
  ASSERT_NE(io.Fonts->AddFontFromFileTTF(icon_font.string().c_str(), 18, &icons,
                                         ranges),
            nullptr);
  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  io.Fonts->SetTexID((ImTextureID)0x4321);

  const std::pair<const char*, const char*> controls[] = {
      {"Select (M)", ICON_MD_SELECT_ALL},
      {"Hand (H)", ICON_MD_PAN_TOOL},
      {"Pencil (B)", ICON_MD_DRAW},
      {"Brush (P)", ICON_MD_BRUSH},
      {"Eraser (E)", ICON_MD_AUTO_FIX_HIGH},
      {"Fill (G)", ICON_MD_FORMAT_COLOR_FILL},
      {"Line (L)", ICON_MD_HORIZONTAL_RULE},
      {"Rectangle (R)", ICON_MD_CROP_SQUARE},
      {"Eyedropper (I)", ICON_MD_COLORIZE},
      {"Undo pixels", ICON_MD_UNDO},
      {"Redo pixels", ICON_MD_REDO},
      {"View options", ICON_MD_TUNE},
      {"Edit options", ICON_MD_MORE_HORIZ},
      {"Zoom out", ICON_MD_ZOOM_OUT},
      {"Zoom in", ICON_MD_ZOOM_IN},
  };
  PixelEditorPanel pixel(&state_, nullptr);
  const ImGuiStyle original_style = ImGui::GetStyle();
  for (float scale : {1.0f, 1.25f, 1.5f}) {
    SCOPED_TRACE(scale);
    io.FontGlobalScale = scale;
    ImGui::GetStyle() = original_style;
    ImGui::GetStyle().ScaleAllSizes(scale);
    // Icon placement must not inherit an alignment chosen for text buttons.
    ImGui::GetStyle().ButtonTextAlign = ImVec2(0, 1);
    ImFontBaked* rendered_font = nullptr;
    auto frame = [&] {
      Frame(
          [&] {
            EXPECT_TRUE(pixel.Update().ok());
            rendered_font = ImGui::GetFontBaked();
          },
          1100);
    };
    frame();
    frame();
    auto* host = ImGui::FindWindowByName("GraphicsHost");
    ASSERT_NE(host, nullptr);
    const auto& vertices = host->DrawList->VtxBuffer;
    ASSERT_NE(rendered_font, nullptr);
    for (const auto& [label, icon] : controls) {
      SCOPED_TRACE(label);
      const auto widget = FindWidget(label);
      ASSERT_TRUE(widget);
      unsigned int codepoint = 0;
      ImTextCharFromUtf8(&codepoint, icon, nullptr);
      const auto* glyph = rendered_font->FindGlyphNoFallback(codepoint);
      ASSERT_NE(glyph, nullptr);
      ASSERT_TRUE(glyph->Visible);
      int complete_glyphs = 0;
      for (int i = 0; i + 3 < vertices.Size; ++i) {
        const auto& a = vertices[i];
        const auto& b = vertices[i + 1];
        const auto& c = vertices[i + 2];
        const auto& d = vertices[i + 3];
        if (a.uv.x != glyph->U0 || a.uv.y != glyph->V0 || b.uv.x != glyph->U1 ||
            b.uv.y != glyph->V0 || c.uv.x != glyph->U1 || c.uv.y != glyph->V1 ||
            d.uv.x != glyph->U0 || d.uv.y != glyph->V1 ||
            (a.col & IM_COL32_A_MASK) == 0)
          continue;
        ++complete_glyphs;
        const auto& bounds = widget->bounds;
        EXPECT_NEAR((a.pos.x + c.pos.x) * 0.5f,
                    (bounds.min_x + bounds.max_x) * 0.5f, 0.5f);
        EXPECT_NEAR((a.pos.y + c.pos.y) * 0.5f,
                    (bounds.min_y + bounds.max_y) * 0.5f, 0.5f);
        EXPECT_GE(a.pos.x, bounds.min_x + scale);
        EXPECT_GE(a.pos.y, bounds.min_y + scale);
        EXPECT_LE(c.pos.x, bounds.max_x - scale);
        EXPECT_LE(c.pos.y, bounds.max_y - scale);
        // Disabled history buttons retain the standard disabled alpha.
        const bool disabled =
            std::string(label).find("pixels") != std::string::npos;
        const int expected_alpha = static_cast<int>(
            255 * (disabled ? ImGui::GetStyle().DisabledAlpha : 1) + 0.5f);
        EXPECT_NEAR((a.col >> IM_COL32_A_SHIFT) & 255, expected_alpha, 1);
      }
      EXPECT_EQ(complete_glyphs, 1);
    }
    Click("Brush (P)", frame);
    EXPECT_EQ(state_.current_tool, PixelTool::kBrush);
    const float previous_zoom = state_.zoom_level;
    Click("Zoom in", frame);
    EXPECT_GT(state_.zoom_level, previous_zoom);
  }
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
  bool browser_visible = true;
  pixel.SetSheetBrowserCallbacks([&] { return browser_visible; },
                                 [&](bool value) { browser_visible = value; });
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
           {"Toggle Sheet Browser", "Palette index 0", "Palette index 7",
            "Current color", "Zoom out", "Zoom in", "Fit sheet"}) {
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

TEST_F(GraphicsWorkspaceLayoutTest,
       ReleaseOutsideCanvasFinishesStrokeBeforeToolbarUndo) {
  UndoManager history;
  PixelEditorPanel pixel(&state_, nullptr, &history);
  state_.SelectSheet(0);
  state_.SetTool(PixelTool::kPencil);
  state_.current_color_index = 3;
  const auto original = bitmap_->vector();
  const auto frame = [&] {
    Frame([&] { ASSERT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();
  const auto origin = SheetOrigin();
  ASSERT_TRUE(origin);
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(origin->x + 2.5f * state_.zoom_level,
                      origin->y + 2.5f * state_.zoom_level);
  frame();
  io.AddMouseButtonEvent(0, true);
  frame();
  ASSERT_TRUE(pixel.HasActiveStroke());
  ASSERT_EQ(data_->sheet_store.Sheet(0)[258], 3);
  io.AddMousePosEvent(1200, 850);
  frame();
  io.AddMouseButtonEvent(0, false);
  frame();
  EXPECT_FALSE(pixel.HasActiveStroke());
  ASSERT_EQ(history.UndoStackSize(), 1u);
  Click("Undo pixels", frame);
  EXPECT_EQ(bitmap_->vector(), original);
  ASSERT_EQ(history.RedoStackSize(), 1u);
  Click("Redo pixels", frame);
  EXPECT_EQ(data_->sheet_store.Sheet(0)[258], 3);
  EXPECT_EQ(history.UndoStackSize(), 1u);
}

TEST_F(GraphicsWorkspaceLayoutTest,
       ShapeReleaseWhileHiddenUsesLastCanvasPoint) {
  UndoManager history;
  PixelEditorPanel pixel(&state_, nullptr, &history);
  state_.SelectSheet(0);
  state_.SetTool(PixelTool::kLine);
  state_.current_color_index = 3;
  const auto original = bitmap_->vector();
  const auto frame = [&] {
    Frame([&] { ASSERT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();
  const auto origin = SheetOrigin();
  ASSERT_TRUE(origin);
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(origin->x + 2.5f * state_.zoom_level,
                      origin->y + 2.5f * state_.zoom_level);
  frame();
  io.AddMouseButtonEvent(0, true);
  frame();
  io.AddMousePosEvent(origin->x + 6.5f * state_.zoom_level,
                      origin->y + 2.5f * state_.zoom_level);
  frame();
  ASSERT_TRUE(pixel.HasActiveStroke());
  io.AddMousePosEvent(1200, 850);
  io.AddMouseButtonEvent(0, false);
  Frame([] {});  // The release frame occurs while this panel is hidden.
  frame();
  EXPECT_FALSE(pixel.HasActiveStroke());
  ASSERT_EQ(history.UndoStackSize(), 1u);
  auto expected = original;
  for (int x = 2; x <= 6; ++x)
    expected[256 + x] = 3;
  EXPECT_EQ(bitmap_->vector(), expected);
  ASSERT_TRUE(history.Undo().ok());
  EXPECT_EQ(bitmap_->vector(), original);
}

TEST_F(GraphicsWorkspaceLayoutTest,
       ChangingTabsFinishesGestureOnOriginalSheet) {
  auto& other = gfx::Arena::Get().mutable_gfx_sheets()->at(1);
  auto saved_other = std::move(other);
  auto restore = absl::MakeCleanup([&] { other = std::move(saved_other); });
  other.Create(128, 32, 8, std::vector<uint8_t>(4096, 1));
  other.SetPalette(gfx::SnesPalette(std::vector<uint16_t>(8, 0x7fff)));
  data_->gfx_bitmaps[1] = other;
  UndoManager history;
  PixelEditorPanel pixel(&state_, nullptr, &history);
  state_.SelectSheet(0);
  state_.SetTool(PixelTool::kPencil);
  state_.current_color_index = 3;
  const auto original = bitmap_->vector();
  const auto frame = [&] {
    Frame([&] { ASSERT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();
  const auto origin = SheetOrigin();
  ASSERT_TRUE(origin);
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(origin->x + 2.5f * state_.zoom_level,
                      origin->y + 2.5f * state_.zoom_level);
  frame();
  io.AddMouseButtonEvent(0, true);
  frame();
  ASSERT_TRUE(pixel.HasActiveStroke());
  // The same pending-tab request used by source/browser navigation.
  state_.open_sheets.insert(1);
  state_.requested_sheet_tab = 1;
  frame();
  frame();
  ASSERT_EQ(state_.current_sheet_id, 1);
  EXPECT_FALSE(pixel.HasActiveStroke());
  EXPECT_EQ(other.vector(), original);
  ASSERT_EQ(history.UndoStackSize(), 1u);
  io.AddMouseButtonEvent(0, false);
  frame();
  Click("Undo pixels", frame);
  EXPECT_EQ(data_->sheet_store.Sheet(0)[258], 1);
  EXPECT_EQ(other.vector(), original);
}

TEST_F(GraphicsWorkspaceLayoutTest, InsideShapeReleaseKeepsItsCurrentEndpoint) {
  for (const auto tool : {PixelTool::kLine, PixelTool::kRectangle}) {
    UndoManager history;
    PixelEditorPanel pixel(&state_, nullptr, &history);
    state_.SelectSheet(0);
    state_.SetTool(tool);
    state_.current_color_index = 3;
    const auto original = bitmap_->vector();
    const auto frame = [&] {
      Frame([&] { ASSERT_TRUE(pixel.Update().ok()); });
    };
    frame();
    frame();
    const auto origin = SheetOrigin();
    ASSERT_TRUE(origin);
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(origin->x + 2.5f * state_.zoom_level,
                        origin->y + 2.5f * state_.zoom_level);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMousePosEvent(origin->x + 6.5f * state_.zoom_level,
                        origin->y + 2.5f * state_.zoom_level);
    frame();
    io.AddMousePosEvent(origin->x + 8.5f * state_.zoom_level,
                        origin->y + 2.5f * state_.zoom_level);
    io.AddMouseButtonEvent(0, false);
    frame();
    EXPECT_FALSE(pixel.HasActiveStroke());
    ASSERT_EQ(history.UndoStackSize(), 1u);
    EXPECT_EQ(data_->sheet_store.Sheet(0)[264], 3);
    ASSERT_TRUE(history.Undo().ok());
    EXPECT_EQ(bitmap_->vector(), original);
  }
}

TEST_F(GraphicsWorkspaceLayoutTest,
       ReleasedShapeWaitsForItsOwnSessionBeforeFinishing) {
  UndoManager history;
  PixelEditorPanel pixel(&state_, nullptr, &history);
  state_.SelectSheet(0);
  state_.SetTool(PixelTool::kLine);
  state_.current_color_index = 3;
  const auto original = bitmap_->vector();
  const auto frame = [&] {
    Frame([&] { ASSERT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();
  const auto origin = SheetOrigin();
  ASSERT_TRUE(origin);
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(origin->x + 2.5f * state_.zoom_level,
                      origin->y + 2.5f * state_.zoom_level);
  frame();
  io.AddMouseButtonEvent(0, true);
  frame();
  io.AddMousePosEvent(origin->x + 6.5f * state_.zoom_level,
                      origin->y + 2.5f * state_.zoom_level);
  frame();
  ASSERT_TRUE(pixel.HasActiveStroke());
  zelda3::GameData foreign;
  gfx::Arena::Get().set_gfx_sheets_owner(&foreign);
  auto restore_owner = absl::MakeCleanup(
      [&] { gfx::Arena::Get().set_gfx_sheets_owner(data_.get()); });
  io.AddMousePosEvent(1200, 850);
  io.AddMouseButtonEvent(0, false);
  frame();
  EXPECT_TRUE(pixel.HasActiveStroke());
  EXPECT_EQ(history.UndoStackSize(), 0u);
  EXPECT_TRUE(state_.modified_sheets.empty());
  EXPECT_EQ(bitmap_->vector(), original);
  EXPECT_EQ(data_->sheet_store.Sheet(0)[258], 1);
  gfx::Arena::Get().set_gfx_sheets_owner(data_.get());
  frame();
  EXPECT_FALSE(pixel.HasActiveStroke());
  EXPECT_EQ(history.UndoStackSize(), 1u);
  EXPECT_EQ(data_->sheet_store.Sheet(0)[258], 3);
}

TEST_F(GraphicsWorkspaceLayoutTest,
       SharedUsesControlsRespectScopeAndRefuseStaleSource) {
  BindReorderSource();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  UsagePreviewView view(&state_);
  view.SetRom(&rom);
  view.SetGameData(data_.get());
  state_.source_context->render_preview =
      []() -> absl::StatusOr<usage_preview::PreviewImage> {
    usage_preview::PreviewImage image;
    image.width = image.height = 8;
    image.pixels.assign(64, 1);
    return image;
  };
  const absl::Cleanup cleanup = [&] {
    gfx::Arena::Get().ClearTextureQueue();
  };
  std::string text;
  const auto frame = [&] {
    Frame([&] {
      ImGui::LogToBuffer(2);
      view.DrawSourcePreview();
      text = context_->LogBuffer.c_str();
      ImGui::LogFinish();
    });
  };
  frame();
  Click("Shared uses in loaded rooms", frame);
  ASSERT_TRUE(FindWidget("Find shared uses"));
  Click("Find shared uses", frame);
  EXPECT_TRUE(UsagePreviewViewTestPeer::SharedScanReady(view));
  EXPECT_EQ(UsagePreviewViewTestPeer::SharedTargets(view).size(), 2u);
  EXPECT_TRUE(UsagePreviewViewTestPeer::SharedUses(view).empty());
  EXPECT_NE(text.find("does not establish exclusive use"), std::string::npos);
  EXPECT_NE(text.find("unloaded"), std::string::npos);
  Click("Shared uses current tile", frame);
  EXPECT_FALSE(UsagePreviewViewTestPeer::SharedScanReady(view));
  Click("Find shared uses", frame);
  ASSERT_EQ(UsagePreviewViewTestPeer::SharedTargets(view).size(), 1u);
  EXPECT_EQ(UsagePreviewViewTestPeer::SharedTargets(view)[0].tile_index, 0);

  state_.source_context->resolve_live_source = [](GraphicsEditContext&) {
    return absl::FailedPreconditionError("Source placement changed");
  };
  Click("Find shared uses", frame);
  EXPECT_FALSE(UsagePreviewViewTestPeer::SharedScanReady(view));
  EXPECT_TRUE(UsagePreviewViewTestPeer::SharedUses(view).empty());
  EXPECT_NE(text.find("Source placement changed"), std::string::npos);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(GraphicsWorkspaceLayoutTest,
       OracleSharedUsesFindsLoadedObjectsPreviewsAndInvalidates) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to an Oracle ROM copy";
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path).ok());
  ASSERT_TRUE(zelda3::LoadGameData(rom, *data_).ok());
  auto& custom = zelda3::CustomObjectManager::Get();
  const auto custom_state = custom.SnapshotState();
  const absl::Cleanup cleanup = [&] {
    gfx::Arena::Get().ClearTextureQueue();
    custom.RestoreState(custom_state);
  };
  custom.SetTilePatternsSource("");
  auto source_room = zelda3::LoadRoomFromRom(&rom, 0x89);
  auto neighbor = zelda3::LoadRoomFromRom(&rom, 0x88);
  source_room.SetGameData(data_.get());
  neighbor.SetGameData(data_.get());
  usage_preview::RoomUsagePreview trace;
  ASSERT_TRUE(trace.TraceLoadedRoom(&rom, data_.get(), &source_room).ok());
  ASSERT_FALSE(trace.tile_references().empty());
  const size_t object_index = trace.tile_references().front().object_index;
  auto source = MakeDungeonGraphicsEditContext(
      source_room, source_room.GetTileObjects()[object_index], data_.get());
  ASSERT_TRUE(source.ok()) << source.status();
  source->resolve_live_source = [&](GraphicsEditContext& fresh) {
    auto current = MakeDungeonGraphicsEditContext(
        source_room, source_room.GetTileObjects()[object_index], data_.get());
    if (!current.ok())
      return current.status();
    fresh = std::move(*current);
    return absl::OkStatus();
  };
  source->render_preview = []() -> absl::StatusOr<usage_preview::PreviewImage> {
    usage_preview::PreviewImage image;
    image.width = image.height = 8;
    image.pixels.assign(64, 1);
    return image;
  };
  state_.source_context = std::move(*source);
  ++state_.source_context_revision;
  state_.SelectSourceTarget(0);
  source_room.ClearSaveDirtyState();
  neighbor.ClearSaveDirtyState();
  source_room.MarkHeaderDirty();
  const auto before = rom.vector();
  UsagePreviewView view(&state_);
  view.SetRom(&rom);
  view.SetGameData(data_.get());
  view.SetLiveRoomProvider([&] {
    return usage_preview::LiveRoomContexts{
        {0x89, {&source_room, "Live source", true}},
        {0x88, {&neighbor, "Live neighbor", false}}};
  });
  const auto frame = [&] {
    Frame([&] { view.DrawSourcePreview(); }, 850);
  };
  frame();
  Click("Shared uses in loaded rooms", frame);
  Click("Find shared uses", frame);
  ASSERT_TRUE(UsagePreviewViewTestPeer::SharedScanReady(view));
  const auto& report = UsagePreviewViewTestPeer::SharedUses(view);
  ASSERT_EQ(report.size(), 2u);
  const auto matching =
      std::find_if(report.begin(), report.end(),
                   [](const auto& entry) { return !entry.objects.empty(); });
  ASSERT_NE(matching, report.end());
  const auto& object = matching->objects.front();
  const auto label = absl::StrFormat(
      "$%03X / object %03X #%zu: %s%s%s", matching->room_id, object.object_id,
      object.object_index, object.shares_pixels ? "Pixels" : "",
      object.shares_pixels && object.shares_palette ? " + " : "",
      object.shares_palette ? "Palette" : "");
  const int preview_room = matching->room_id;
  Click(label.c_str(), frame);
  EXPECT_EQ(UsagePreviewViewTestPeer::SharedPreviewRoom(view), preview_room);
  EXPECT_EQ(UsagePreviewViewTestPeer::SharedOutlineCount(view), 1u);
  const auto source_revision = source_room.composite_source_revision();
  const auto neighbor_revision = neighbor.composite_source_revision();
  frame();
  frame();
  EXPECT_TRUE(UsagePreviewViewTestPeer::SharedScanReady(view));
  EXPECT_EQ(source_room.composite_source_revision(), source_revision);
  EXPECT_EQ(neighbor.composite_source_revision(), neighbor_revision);
  EXPECT_TRUE(source_room.header_dirty());
  EXPECT_FALSE(neighbor.HasUnsavedChanges());
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(state_.modified_sheets.empty());

  neighbor.MarkGraphicsDirty();
  frame();
  EXPECT_FALSE(UsagePreviewViewTestPeer::SharedScanReady(view));
  EXPECT_EQ(UsagePreviewViewTestPeer::SharedPreviewRoom(view), -1);
  view.SetRom(&rom);  // Same-address session replacement clears the census too.
  EXPECT_TRUE(UsagePreviewViewTestPeer::SharedUses(view).empty());
}

TEST_F(GraphicsWorkspaceLayoutTest,
       SourceWorkspaceReflowsAndKeepsDetailsAndReturnReachable) {
  UsagePreviewView view(&state_);
  view.SetGameData(data_.get());
  PixelEditorPanel pixel(&state_, nullptr);
  pixel.SetSourcePreviewCallback([&] { view.DrawSourcePreview(); });
  GraphicsEditContext source;
  source.owner = data_.get();
  source.label = "Room 005 / ornate corner pillar / selected entrance 00";
  source.drawing_description = "Corner pattern with repeated horizontal beams";
  source.limitation = "Animated pixels are omitted";
  source.targets = {
      {.sheet_id = 0,
       .tile_index = 0,
       .label = "First source tile",
       .palette = gfx::SnesPalette(std::vector<uint16_t>(8, 0x001f)),
       .source_orientation_mask = 3},
      {.sheet_id = 0,
       .tile_index = 1,
       .label = "Second source tile",
       .palette = gfx::SnesPalette(std::vector<uint16_t>(8, 0x001f))}};
  int returns = 0;
  int renders = 0;
  source.return_to_source = [&] {
    ++returns;
  };
  source.render_preview = [&]() -> absl::StatusOr<usage_preview::PreviewImage> {
    ++renders;
    usage_preview::PreviewImage image;
    image.width = image.height = 512;
    image.pixels.assign(512 * 512, 1);
    image.colors[1] = {255, 0, 0, 255};
    return image;
  };
  state_.source_context = source;
  ++state_.source_context_revision;
  state_.SelectSourceTarget(0);
  absl::Cleanup cleanup = [&] {
    UsagePreviewViewTestPeer::SourceBitmap(view).set_texture(nullptr);
    gfx::Arena::Get().ClearTextureQueue();
  };
  float width = 440;
  std::string text;
  const auto frame = [&] {
    Frame(
        [&] {
          ImGui::LogToBuffer(2);
          EXPECT_TRUE(pixel.Update().ok());
          text = context_->LogBuffer.c_str();
          ImGui::LogFinish();
        },
        width);
  };
  frame();
  UsagePreviewViewTestPeer::SourceBitmap(view).set_texture(
      reinterpret_cast<gfx::TextureHandle>(0x2345));
  const auto original_style = ImGui::GetStyle();
  for (float scale : {1.0f, 1.5f}) {
    ImGui::GetIO().FontGlobalScale = scale;
    ImGui::GetStyle() = original_style;
    ImGui::GetStyle().ScaleAllSizes(scale);
    for (float target_width : {440.f, 1280.f}) {
      width = target_width;
      frame();
      frame();
      auto* canvas = ActiveChild("##CanvasArea");
      auto* preview = ActiveChild("##SourceUsagePreview");
      auto* host = ImGui::FindWindowByName("GraphicsHost");
      ASSERT_NE(canvas, nullptr);
      ASSERT_NE(preview, nullptr);
      ASSERT_NE(host, nullptr);
      EXPECT_LE(host->ContentSize.x, host->InnerRect.GetWidth() + 1);
      EXPECT_LE(host->ContentSize.y, host->InnerRect.GetHeight() + 1);
      if (width == 440) {
        EXPECT_GE(preview->Pos.y, canvas->Pos.y + canvas->Size.y);
      } else {
        EXPECT_GE(preview->Pos.x, canvas->Pos.x + canvas->Size.x);
      }
      // The actual source image fits both dimensions, even for a full room.
      int image_vertices = 0;
      for (const auto& command : preview->DrawList->CmdBuffer) {
        if (command.GetTexID() != (ImTextureID)0x2345)
          continue;
        for (unsigned i = 0; i < command.ElemCount; ++i) {
          const auto& vertex =
              preview->DrawList->VtxBuffer
                  [command.VtxOffset +
                   preview->DrawList->IdxBuffer[command.IdxOffset + i]];
          EXPECT_LE(vertex.pos.x, preview->InnerRect.Max.x + 1);
          EXPECT_LE(vertex.pos.y, preview->InnerRect.Max.y + 1);
          ++image_vertices;
        }
      }
      EXPECT_GE(image_vertices, 6);
      Click("Show source preview", frame);
      EXPECT_EQ(ActiveChild("##SourceUsagePreview"), nullptr);
      EXPECT_GT(canvas->Size.x, host->Size.x - 40);
      Click("Show source preview", frame);
      EXPECT_NE(ActiveChild("##SourceUsagePreview"), nullptr);
    }
  }
  EXPECT_EQ(renders, 1);  // Reflow/show/hide use the same cached source.
  Click("Source tile", frame);
  Click("Source tile 1", frame);
  EXPECT_EQ(state_.source_context->selected_target, 1);
  EXPECT_EQ(state_.current_sheet_id, 0);
  Click("Return to source", frame);
  EXPECT_EQ(returns, 1);
  Click("Source details", frame);
  EXPECT_NE(text.find(source.drawing_description), std::string::npos);
  EXPECT_NE(text.find(source.limitation), std::string::npos);
  state_.selection.is_floating = true;
  Click("Use manual palette", frame);
  EXPECT_TRUE(state_.source_context);
  state_.selection.is_floating = false;
  Click("Use manual palette", frame);
  EXPECT_FALSE(state_.source_context);
  EXPECT_EQ(ActiveChild("##SourceUsagePreview"), nullptr);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(GraphicsWorkspaceLayoutTest,
       FloatingPasteKeepsAdjacentPixelsHardEdgedAtFractionalZoom) {
  state_.open_sheets.insert(0);
  state_.SetZoom(1.5f);
  state_.selection.x = 1;
  state_.selection.y = 1;
  state_.selection.width = 2;
  state_.selection.height = 1;
  state_.selection.pixel_data = {1, 2};
  state_.selection.is_active = true;
  state_.selection.is_floating = true;
  PixelEditorPanel pixel(&state_, nullptr);
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();

  const auto origin = SheetOrigin();
  ASSERT_TRUE(origin);
  auto* canvas = ActiveChild("##CanvasArea");
  ASSERT_NE(canvas, nullptr);
  const ImU32 paste_color = IM_COL32(255, 255, 255, 230);
  std::vector<ImVec2> paste_vertices;
  for (const auto& vertex : canvas->DrawList->VtxBuffer) {
    if (vertex.col == paste_color) {
      paste_vertices.push_back(vertex.pos);
    }
  }
  ASSERT_EQ(paste_vertices.size(), 8u);
  const ImVec2 offsets[] = {{1.5f, 1.5f}, {3.0f, 1.5f}, {3.0f, 3.0f},
                            {1.5f, 3.0f}, {3.0f, 1.5f}, {4.5f, 1.5f},
                            {4.5f, 3.0f}, {3.0f, 3.0f}};
  for (size_t i = 0; i < paste_vertices.size(); ++i) {
    EXPECT_FLOAT_EQ(paste_vertices[i].x, origin->x + offsets[i].x);
    EXPECT_FLOAT_EQ(paste_vertices[i].y, origin->y + offsets[i].y);
  }
}

TEST_F(GraphicsWorkspaceLayoutTest,
       InlineAndComparisonShareSourceRefreshThroughPixelsPaletteAndErrors) {
  auto& palettes = gfx::PaletteManager::Get();
  palettes.ResetForTesting();
  UsagePreviewView view(&state_);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  view.SetRom(&rom);
  view.SetGameData(data_.get());
  data_->palette_groups.dungeon_main.AddPalette(
      gfx::SnesPalette(std::vector<uint16_t>(90, 0x001f)));
  palettes.Initialize(data_.get());
  absl::Cleanup cleanup = [&] {
    palettes.ResetForTesting();
    gfx::Arena::Get().ClearTextureQueue();
  };
  GraphicsEditContext source;
  source.owner = data_.get();
  int renders = 0;
  bool fail = false;
  source.render_preview = [&]() -> absl::StatusOr<usage_preview::PreviewImage> {
    ++renders;
    if (fail)
      return absl::FailedPreconditionError("Source render failed");
    usage_preview::PreviewImage image;
    image.width = image.height = 1;
    image.pixels = {bitmap_->GetPixel(0, 0)};
    return image;
  };
  state_.source_context = source;
  ++state_.source_context_revision;
  bool reverse_order = false;
  const auto frame = [&] {
    Frame([&] {
      if (reverse_order) {
        view.Draw();
        ImGui::PushID("Inline");
        view.DrawSourcePreview();
        ImGui::PopID();
      } else {
        ImGui::PushID("Inline");
        view.DrawSourcePreview();
        ImGui::PopID();
        view.Draw();
      }
    });
  };
  frame();
  frame();
  EXPECT_EQ(renders, 1);
  auto changed = bitmap_->vector();
  changed[0] = 6;
  ASSERT_TRUE(CommitSheetPixels(state_, 0, changed).ok());
  frame();
  EXPECT_EQ(renders, 2);
  EXPECT_EQ(UsagePreviewViewTestPeer::SourceBitmap(view).GetPixel(0, 0), 6);
  reverse_order = true;
  ASSERT_TRUE(
      palettes.SetColor("dungeon_main", 0, 0, gfx::SnesColor(0x03e0)).ok());
  frame();
  EXPECT_EQ(renders, 3);
  palettes.Undo();
  frame();
  EXPECT_EQ(renders, 4);
  state_.source_context->palette_error = "Source assignment changed";
  frame();
  EXPECT_EQ(renders, 4);
  state_.source_context->palette_error.clear();
  fail = true;
  frame();
  EXPECT_EQ(renders, 5);
  fail = false;
  Click("Refresh source preview", frame);
  EXPECT_EQ(renders, 6);
  zelda3::GameData other;
  view.SetGameData(&other);
  Frame([&] { view.DrawSourcePreview(); });
  EXPECT_EQ(renders, 6);  // Never render another session's captured callback.
  view.SetGameData(data_.get());
  frame();
  EXPECT_EQ(renders, 7);
}

TEST_F(GraphicsWorkspaceLayoutTest,
       PaletteReorderStagesScopeCancelApplyAndOneUndo) {
  BindReorderSource();
  auto cleanup = absl::MakeCleanup([&] {
    gfx::PaletteManager::Get().ReleaseSession(data_.get());
    gfx::Arena::Get().ClearTextureQueue();
  });
  UndoManager history;
  PixelEditorPanel pixel(&state_, nullptr, &history);
  pixel.SetPaletteReorderCallback([&](const auto& targets,
                                      const auto& permutation) {
    return ApplyPalettePixelPermutation(state_, history, targets, permutation);
  });
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); }, 460);
  };
  const auto before = data_->graphics_buffer;
  frame();
  frame();
  Click("Reorder source colors", frame);
  Click("Reorder after slot 1", frame);
  Click("Reorder after slot 2", frame);
  Click("Reorder source pattern", frame);
  EXPECT_EQ(data_->graphics_buffer, before);
  EXPECT_FALSE(gfx::PaletteManager::Get().HasUnsavedChanges());
  Click("Cancel palette reorder", frame);
  EXPECT_EQ(history.UndoStackSize(), 0u);
  EXPECT_EQ(data_->graphics_buffer, before);

  // Reopening resets to Current tile and the original order. Zero is fixed.
  Click("Reorder source colors", frame);
  Click("Reorder after slot 0", frame);
  Click("Reorder after slot 1", frame);
  Click("Reorder after slot 2", frame);
  Click("Apply palette reorder", frame);
  ASSERT_EQ(history.UndoStackSize(), 1u);
  auto pixels = data_->sheet_store.Sheet(0);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 16; ++x)
      EXPECT_EQ(pixels[y * 128 + x], x < 8 ? 2 : 1);
  EXPECT_EQ(gfx::PaletteManager::Get().GetColor("dungeon_main", 0, 1).snes(),
            31);  // Index two remains red after its pixels are remapped.
  ASSERT_TRUE(history.Undo().ok());
  EXPECT_EQ(data_->graphics_buffer, before);
  ASSERT_TRUE(history.Redo().ok());
  ASSERT_TRUE(history.Undo().ok());

  Click("Reorder source colors", frame);
  Click("Reorder source pattern", frame);
  Click("Reorder after slot 1", frame);
  Click("Reorder after slot 2", frame);
  Click("Apply palette reorder", frame);
  ASSERT_EQ(history.UndoStackSize(), 1u);
  pixels = data_->sheet_store.Sheet(0);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 16; ++x)
      EXPECT_EQ(pixels[y * 128 + x], 2);
  ASSERT_TRUE(history.Undo().ok());
  EXPECT_EQ(data_->graphics_buffer, before);
}

TEST_F(GraphicsWorkspaceLayoutTest,
       PaletteReorderRefusesChangedSourceAndUnmappedTargets) {
  BindReorderSource();
  auto cleanup = absl::MakeCleanup([&] {
    gfx::PaletteManager::Get().ReleaseSession(data_.get());
    gfx::Arena::Get().ClearTextureQueue();
  });
  PixelEditorPanel pixel(&state_, nullptr);
  int apply_count = 0;
  pixel.SetPaletteReorderCallback([&](const auto&, const auto&) {
    ++apply_count;
    return absl::OkStatus();
  });
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); }, 460);
  };
  frame();
  frame();
  Click("Reorder source colors", frame);
  Click("Reorder after slot 1", frame);
  Click("Reorder after slot 2", frame);
  ASSERT_TRUE(gfx::PaletteManager::Get()
                  .SetColor("dungeon_main", 0, 0, gfx::SnesColor(0x1234))
                  .ok());
  frame();
  EXPECT_FALSE(FindWidget("Apply palette reorder"));
  Click("Cancel palette reorder", frame);
  EXPECT_EQ(apply_count, 0);

  state_.source_context->targets[0].writable_palette.reset();
  Click("Reorder source colors", frame);
  Click("Reorder after slot 1", frame);
  Click("Reorder after slot 2", frame);
  Click("Apply palette reorder",
        frame);  // Disabled, despite nonidentity order.
  EXPECT_EQ(apply_count, 0);
  state_.ClearSourceContext();
  frame();
  EXPECT_EQ(ImGui::GetCurrentContext()->OpenPopupStack.Size, 0);
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
