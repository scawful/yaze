#include "app/gfx/core/bitmap.h"

#include "app/gfx/resource/arena.h"
#include "gtest/gtest.h"
#include "zelda3/dungeon/palette_debug.h"

namespace yaze::gfx {
namespace {

TEST(BitmapColorTest, CopiesDisplayPaletteWithoutReplacingSourcePalette) {
  Bitmap original(3, 2, 8, std::vector<uint8_t>{1, 2, 1, 2, 1, 2});
  SnesPalette source;
  source.AddColor(SnesColor(static_cast<uint16_t>(0x7FFF)));
  original.SetPalette(source);
  std::vector<SDL_Color> colors(256, {0, 0, 0, 255});
  colors[1] = {17, 34, 51, 255};
  colors[2] = {68, 85, 102, 255};
  original.set_modified(false);
  original.SetPalette(colors);
  EXPECT_TRUE(original.modified());
  SDL_SetColorKey(original.surface(), SDL_TRUE, 2);
  original.metadata().purpose = Bitmap::BitmapPurpose::kCompositeOutput;
  Bitmap copy(original);
  Bitmap assigned;
  assigned = original;
  for (const auto* bitmap : {&copy, &assigned}) {
    EXPECT_EQ(bitmap->ToRgba(), original.ToRgba());
    EXPECT_EQ(bitmap->palette().size(), 1u);
    EXPECT_EQ(bitmap->palette()[0].snes(), source[0].snes());
    EXPECT_EQ(bitmap->metadata().purpose,
              Bitmap::BitmapPurpose::kCompositeOutput);
    EXPECT_EQ(bitmap->ReadColor(1, 0).a, 0);
    EXPECT_EQ(static_cast<uint8_t*>(
                  bitmap->surface()->pixels)[bitmap->surface()->pitch],
              2);
  }
  original.WriteToPixel(0, 1, 1);
  EXPECT_EQ(static_cast<uint8_t*>(
                original.surface()->pixels)[original.surface()->pitch],
            1);
  original.set_data({2, 1, 2, 1, 2, 1});
  EXPECT_EQ(static_cast<uint8_t*>(
                original.surface()->pixels)[original.surface()->pitch],
            1);
  EXPECT_TRUE(original.ValidateDataSurfaceSync());
  for (auto* bitmap : {&original, &copy, &assigned})
    Arena::Get().RetireBitmap(*bitmap);
}

TEST(BitmapColorTest, DirectColorInspectionDoesNotInventPaletteMismatch) {
  Bitmap bitmap(1, 1, 32, std::vector<uint8_t>{255, 0, 123, 255});
  auto& debug = zelda3::PaletteDebugger::Get();
  debug.Clear();
  debug.SetCurrentBitmap(&bitmap);
  const auto sample = debug.SamplePixelAt(0, 0);
  EXPECT_EQ(sample.palette_index, -1);
  EXPECT_EQ(sample.actual_r, 255);
  EXPECT_EQ(sample.actual_b, 123);
  debug.LogSurfaceState("test RGBA", bitmap.surface());
  ASSERT_EQ(debug.GetEvents().size(), 1u);
  EXPECT_EQ(debug.GetEvents().back().level, zelda3::PaletteDebugLevel::INFO);
  debug.Clear();
  Arena::Get().RetireBitmap(bitmap);
}

TEST(BitmapColorTest, RgbaSurvivesCopySurfaceConversionAndSourceWriteRefusal) {
  const std::vector<uint8_t> pixels{255, 0, 123, 255, 8, 16, 24, 0};
  Bitmap original(2, 1, 32, pixels);
  EXPECT_FALSE(original.IsIndexed());
  ASSERT_NE(original.surface(), nullptr);
  EXPECT_EQ(platform::GetSurfacePalette(original.surface()), nullptr);
  Bitmap copy = original;
  Bitmap assigned;
  assigned = original;
  for (auto* bitmap : {&original, &copy, &assigned}) {
    EXPECT_EQ(bitmap->ToRgba(), pixels);
    EXPECT_EQ(bitmap->ReadColor(0, 0).b, 123);
    EXPECT_EQ(bitmap->ReadColor(1, 0).a, 0);
    bitmap->WriteToPixel(0, static_cast<uint8_t>(5));
    bitmap->SetPixel(0, 0, SnesColor(static_cast<uint16_t>(0)));
    EXPECT_EQ(bitmap->vector(), pixels);
    auto* surface =
        SDL_ConvertSurfaceFormat(bitmap->surface(), SDL_PIXELFORMAT_RGBA32, 0);
    ASSERT_NE(surface, nullptr);
    EXPECT_EQ(std::vector<uint8_t>(static_cast<uint8_t*>(surface->pixels),
                                   static_cast<uint8_t*>(surface->pixels) + 8),
              pixels);
    SDL_FreeSurface(surface);
    Arena::Get().RetireBitmap(*bitmap);
  }
}

}  // namespace
}  // namespace yaze::gfx
