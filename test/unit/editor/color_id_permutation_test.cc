#include "app/editor/graphics/color_id_permutation.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_scratch_asset.h"
#include "app/editor/graphics/scratch_color_id_command.h"
#include "app/editor/graphics/ui/scratch/graphics_scratch_view.h"
#include "app/editor/registry/undo_manager.h"

namespace yaze::editor {
namespace {

std::vector<uint8_t> Pattern(int bpp, int shift = 0) {
  std::vector<uint8_t> pixels(4096);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 128; ++x)
      pixels[y * 128 + x] = (x + y + shift) % (1 << bpp);
  return pixels;
}

std::vector<uint8_t> Mapped(std::vector<uint8_t> pixels,
                            const ColorIdPermutation& permutation) {
  for (auto& pixel : pixels)
    pixel = permutation.ids[pixel];
  return pixels;
}

std::vector<uint8_t> Applied(const std::vector<uint8_t>& pixels,
                             const ColorIdEditPlan& plan, int bpp) {
  auto result = pixels;
  for (const auto& edit : plan.ranges)
    EXPECT_TRUE(ApplyColorIdRangeEdit(edit, bpp, true, result).ok());
  return result;
}

std::vector<ColorIdTarget> WholePage(uint32_t range) {
  std::vector<ColorIdTarget> targets;
  for (uint16_t tile = 0; tile < 64; ++tile)
    targets.push_back({range, tile});
  return targets;
}

TEST(ColorIdPermutationTest, ValidatesEveryIdAgainstTheDeclaredDepth) {
  auto two = IdentityColorIdPermutation(2);
  std::swap(two.ids[1], two.ids[3]);
  EXPECT_TRUE(ValidateColorIdPermutation(two).ok());
  auto beyond_two = IdentityColorIdPermutation(2);
  std::swap(beyond_two.ids[3], beyond_two.ids[4]);
  EXPECT_FALSE(ValidateColorIdPermutation(beyond_two).ok());

  const auto four = MakeColorIdSwap(4, 9, 15);
  ASSERT_TRUE(four.ok()) << four.status();
  EXPECT_EQ(four->ids[9], 15);
  EXPECT_EQ(four->ids[15], 9);
  EXPECT_TRUE(ValidateColorIdPermutation(*four).ok());
  EXPECT_TRUE(MakeColorIdSwap(4, 1, 8).ok());
  EXPECT_FALSE(MakeColorIdSwap(3, 1, 8).ok());
  EXPECT_FALSE(MakeColorIdSwap(2, 1, 4).ok());
  EXPECT_FALSE(MakeColorIdSwap(2, -1, 2).ok());
  EXPECT_FALSE(MakeColorIdSwap(1, 1, 0).ok());
  EXPECT_FALSE(MakeColorIdSwap(5, 1, 2).ok());

  auto duplicate = IdentityColorIdPermutation(4);
  duplicate.ids[2] = 1;
  EXPECT_FALSE(ValidateColorIdPermutation(duplicate).ok());
  auto bad_depth = IdentityColorIdPermutation(8);
  EXPECT_FALSE(ValidateColorIdPermutation(bad_depth).ok());
}

TEST(ColorIdPermutationTest, RefusesTransparencyChangesInsteadOfReplacing) {
  auto opaque_zero = IdentityColorIdPermutation(4);
  std::swap(opaque_zero.ids[0], opaque_zero.ids[5]);
  const auto status = ValidateColorIdPermutation(opaque_zero);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(status.message().find("transparent"), std::string::npos);
  EXPECT_FALSE(MakeColorIdSwap(4, 0, 5).ok());
  EXPECT_FALSE(MakeColorIdSwap(2, 3, 0).ok());
  const auto pixels = Pattern(4);
  EXPECT_FALSE(
      PlanColorIdEdit({{0, 4, pixels}}, WholePage(0), opaque_zero).ok());
}

TEST(ColorIdPermutationTest, BackingDepthAndStoredIdsMustMatch) {
  const auto four = MakeColorIdSwap(4, 1, 2);
  ASSERT_TRUE(four.ok());
  EXPECT_TRUE(ValidateColorIdPermutationForBacking(*four, 4).ok());
  EXPECT_FALSE(ValidateColorIdPermutationForBacking(*four, 3).ok());
  const auto two_pixels = Pattern(2);
  EXPECT_FALSE(PlanColorIdEdit({{0, 2, two_pixels}}, WholePage(0), *four).ok());

  auto overfull = Pattern(2);
  overfull[100] = 4;
  const auto two = MakeColorIdSwap(2, 1, 2);
  ASSERT_TRUE(two.ok());
  const auto refused = PlanColorIdEdit({{0, 2, overfull}}, WholePage(0), *two);
  EXPECT_EQ(refused.status().code(), absl::StatusCode::kFailedPrecondition);
  const std::vector<uint8_t> short_page(2048, 1);
  EXPECT_FALSE(PlanColorIdEdit({{0, 2, short_page}}, WholePage(0), *two).ok());
  EXPECT_FALSE(PlanColorIdEdit({{0, 2, two_pixels}}, {{1, 0}}, *two).ok());
  EXPECT_FALSE(PlanColorIdEdit({{0, 2, two_pixels}}, {{0, 64}}, *two).ok());
  EXPECT_FALSE(PlanColorIdEdit({{0, 2, two_pixels}}, {}, *two).ok());
  EXPECT_FALSE(
      PlanColorIdEdit({{0, 2, two_pixels}, {0, 2, two_pixels}}, {{0, 0}}, *two)
          .ok());
}

TEST(ColorIdPermutationTest, TwoAndFourBppSwapsPreserveIdZero) {
  for (const auto& [bpp, from, to] :
       std::vector<std::tuple<int, int, int>>{{2, 1, 3}, {4, 1, 15}}) {
    SCOPED_TRACE(bpp);
    const auto pixels = Pattern(bpp);
    const auto swap = MakeColorIdSwap(bpp, from, to);
    ASSERT_TRUE(swap.ok());
    const auto plan = PlanColorIdEdit({{7, bpp, pixels}}, WholePage(7), *swap);
    ASSERT_TRUE(plan.ok()) << plan.status();
    EXPECT_EQ(plan->physical_tiles, 64u);
    EXPECT_EQ(plan->selected_pixels, 4096u);
    ASSERT_EQ(plan->ranges.size(), 1u);
    const auto after = Applied(pixels, *plan, bpp);
    EXPECT_EQ(after, Mapped(pixels, *swap));
    for (size_t i = 0; i < pixels.size(); ++i)
      EXPECT_EQ(after[i] == 0, pixels[i] == 0) << i;
  }
}

TEST(ColorIdPermutationTest, FlippedAliasesAreUnionedAndMappedOnce) {
  const auto pixels = Pattern(4);
  const auto swap = MakeColorIdSwap(4, 1, 2);
  ASSERT_TRUE(swap.ok());
  const std::vector<ColorIdTarget> targets{
      {0, 0, uint64_t{1} << 1},
      {0, 0, uint64_t{1} << 6, true, false},
      {0, 0, uint64_t{1} << 57, false, true},
      {0, 0, uint64_t{1} << 62, true, true},
      {0, 0, uint64_t{1} << 2},
      {0, 0, uint64_t{1} << 2}};
  const auto plan = PlanColorIdEdit({{0, 4, pixels}}, targets, *swap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->physical_tiles, 1u);
  EXPECT_EQ(plan->selected_pixels, 2u);
  ASSERT_EQ(plan->targets.size(), 1u);
  EXPECT_FALSE(plan->targets[0].hflip);
  EXPECT_FALSE(plan->targets[0].vflip);
  EXPECT_EQ(plan->targets[0].pixel_mask,
            (uint64_t{1} << 1) | (uint64_t{1} << 2));
  // A swap applied twice would restore the original IDs.
  auto expected = pixels;
  expected[1] = 2;
  expected[2] = 1;
  EXPECT_EQ(Applied(pixels, *plan, 4), expected);
}

TEST(ColorIdPermutationTest, PlanningIsPureAndRangeEditsRefuseDrift) {
  const auto pixels = Pattern(4, 3);
  const auto swap = MakeColorIdSwap(4, 4, 11);
  ASSERT_TRUE(swap.ok());
  const auto plan = PlanColorIdEdit({{0, 4, pixels}}, WholePage(0), *swap);
  ASSERT_TRUE(plan.ok());
  EXPECT_EQ(pixels, Pattern(4, 3));
  ASSERT_EQ(plan->ranges.size(), 1u);
  const auto& edit = plan->ranges.front();
  auto drifted = pixels;
  drifted[edit.offsets.front()] = 9;
  const auto before_drift = drifted;
  EXPECT_FALSE(ApplyColorIdRangeEdit(edit, 4, true, drifted).ok());
  EXPECT_EQ(drifted, before_drift);
  auto round_trip = pixels;
  ASSERT_TRUE(ApplyColorIdRangeEdit(edit, 4, true, round_trip).ok());
  EXPECT_NE(round_trip, pixels);
  ASSERT_TRUE(ApplyColorIdRangeEdit(edit, 4, false, round_trip).ok());
  EXPECT_EQ(round_trip, pixels);
  EXPECT_FALSE(ApplyColorIdRangeEdit(edit, 2, true, round_trip).ok());
}

TEST(ColorIdPermutationTest, AbsentIdsProduceNoRangeEdits) {
  const std::vector<uint8_t> pixels(4096, 3);
  const auto swap = MakeColorIdSwap(4, 1, 2);
  ASSERT_TRUE(swap.ok());
  const auto plan = PlanColorIdEdit({{0, 4, pixels}}, WholePage(0), *swap);
  ASSERT_TRUE(plan.ok());
  EXPECT_TRUE(plan->ranges.empty());
  EXPECT_EQ(plan->selected_pixels, 4096u);
}

class ScratchColorIdTest : public ::testing::Test {
 protected:
  void SetUp() override {
    folder_ =
        std::filesystem::temp_directory_path() /
        ("yaze-scratch-color-id-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(folder_));
    library_ = folder_ / "library.json";
  }
  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(folder_, ec);
  }

  // Imports one 64-tile BIN through the existing scratch BIN path.
  uint64_t Import(GraphicsScratchView& view, int bpp,
                  const std::vector<uint8_t>& pixels) {
    GraphicsScratchAsset asset;
    asset.name = "fixture";
    asset.bpp = bpp;
    asset.pixels = pixels;
    const auto bytes = EncodeScratchBin(asset);
    EXPECT_TRUE(bytes.ok()) << bytes.status();
    const auto path = folder_ / ("page-" + std::to_string(++bins_) + ".bin");
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    output.close();
    EXPECT_TRUE(view.ImportBin(path.string(), bpp, 0).ok());
    return view.library().assets.back().id;
  }

  const std::vector<uint8_t>& PixelsOf(const GraphicsScratchView& view,
                                       uint64_t id) {
    for (const auto& asset : view.library().assets)
      if (asset.id == id)
        return asset.pixels;
    static const std::vector<uint8_t> kMissing;
    ADD_FAILURE() << "missing scratch asset " << id;
    return kMissing;
  }

  static std::vector<ScratchColorIdTarget> Whole(
      std::initializer_list<uint64_t> ids) {
    std::vector<ScratchColorIdTarget> targets;
    for (const auto id : ids) {
      const auto whole = WholeScratchAssetSelection(id);
      targets.insert(targets.end(), whole.begin(), whole.end());
    }
    return targets;
  }

  std::filesystem::path folder_, library_;
  int bins_ = 0;
  GraphicsEditorState state_;
  UndoManager history_;
};

TEST_F(ScratchColorIdTest, FourBppSwapAcrossTwoRangesIsOneUndoStep) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  const auto first = Import(view, 4, Pattern(4));
  const auto second = Import(view, 4, Pattern(4, 5));
  const auto history_before = history_.UndoStackSize();
  const auto swap = MakeColorIdSwap(4, 1, 12);
  ASSERT_TRUE(swap.ok());
  ASSERT_TRUE(view.SwapColorIds(Whole({first, second}), *swap).ok());
  EXPECT_EQ(history_.UndoStackSize(), history_before + 1);
  EXPECT_EQ(PixelsOf(view, first), Mapped(Pattern(4), *swap));
  EXPECT_EQ(PixelsOf(view, second), Mapped(Pattern(4, 5), *swap));
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_EQ(PixelsOf(view, first), Pattern(4));
  EXPECT_EQ(PixelsOf(view, second), Pattern(4, 5));
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_EQ(PixelsOf(view, first), Mapped(Pattern(4), *swap));
  EXPECT_EQ(PixelsOf(view, second), Mapped(Pattern(4, 5), *swap));
}

TEST_F(ScratchColorIdTest, TwoBppSwapKeepsTransparencyAndRefusesOtherDepths) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  const auto two = Import(view, 2, Pattern(2));
  const auto four = Import(view, 4, Pattern(4));
  const auto history_before = history_.UndoStackSize();
  const auto library_before = view.library().Serialize();

  const auto wrong_depth = MakeColorIdSwap(4, 1, 2);
  ASSERT_TRUE(wrong_depth.ok());
  EXPECT_FALSE(view.SwapColorIds(Whole({two}), *wrong_depth).ok());
  // A mixed-depth selection refuses before either range changes.
  EXPECT_FALSE(view.SwapColorIds(Whole({four, two}), *wrong_depth).ok());
  auto transparent = IdentityColorIdPermutation(2);
  std::swap(transparent.ids[0], transparent.ids[1]);
  EXPECT_FALSE(view.SwapColorIds(Whole({two}), transparent).ok());
  EXPECT_FALSE(view.SwapColorIds({{9999, 0}}, *wrong_depth).ok());
  EXPECT_EQ(view.library().Serialize(), library_before);
  EXPECT_EQ(history_.UndoStackSize(), history_before);

  const auto swap = MakeColorIdSwap(2, 1, 3);
  ASSERT_TRUE(swap.ok());
  ASSERT_TRUE(view.SwapColorIds(Whole({two}), *swap).ok());
  EXPECT_EQ(PixelsOf(view, two), Mapped(Pattern(2), *swap));
  EXPECT_EQ(PixelsOf(view, four), Pattern(4));
  EXPECT_EQ(history_.UndoStackSize(), history_before + 1);
}

TEST_F(ScratchColorIdTest, NoOpSwapAddsNoHistory) {
  GraphicsScratchView view(&state_, &history_, library_.string());
  const auto blank = Import(view, 4, std::vector<uint8_t>(4096, 0));
  const auto history_before = history_.UndoStackSize();
  const auto swap = MakeColorIdSwap(4, 1, 2);
  ASSERT_TRUE(swap.ok());
  ASSERT_TRUE(view.SwapColorIds(Whole({blank}), *swap).ok());
  EXPECT_EQ(history_.UndoStackSize(), history_before);
  EXPECT_EQ(PixelsOf(view, blank), std::vector<uint8_t>(4096, 0));
}

TEST(ScratchColorIdCommandTest, ApplyRefusesDriftWithoutPartialWrites) {
  GraphicsScratchLibrary library;
  for (int shift : {0, 1}) {
    GraphicsScratchAsset asset;
    asset.name = "range";
    asset.bpp = 3;
    asset.pixels = Pattern(3, shift);
    ASSERT_TRUE(library.Add(std::move(asset)).ok());
  }
  const auto first = library.assets[0].id;
  const auto second = library.assets[1].id;
  auto targets = WholeScratchAssetSelection(first);
  const auto more = WholeScratchAssetSelection(second);
  targets.insert(targets.end(), more.begin(), more.end());
  const auto swap = MakeColorIdSwap(3, 2, 7);
  ASSERT_TRUE(swap.ok());
  const auto plan = PlanScratchColorIdPermutation(library, targets, *swap);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->edit.ranges.size(), 2u);
  library.assets[1].pixels[plan->edit.ranges[1].offsets.front()] = 5;
  const auto drifted = library.Serialize();
  EXPECT_FALSE(ApplyScratchColorIdPlan(library, *plan).ok());
  EXPECT_EQ(library.Serialize(), drifted);
}

}  // namespace
}  // namespace yaze::editor
