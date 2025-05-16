#include "bitmap.h"

#include <SDL.h>
#if YAZE_LIB_PNG == 1
#include <png.h>
#endif

#include <cstdint>

#include "app/gfx/arena.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace gfx {

#if YAZE_LIB_PNG == 1

namespace png_internal {

void PngWriteCallback(png_structp png_ptr, png_bytep data, png_size_t length) {
  std::vector<uint8_t> *p = (std::vector<uint8_t> *)png_get_io_ptr(png_ptr);
  p->insert(p->end(), data, data + length);
}

void PngReadCallback(png_structp png_ptr, png_bytep outBytes,
                     png_size_t byteCountToRead) {
  png_voidp io_ptr = png_get_io_ptr(png_ptr);
  if (!io_ptr) return;

  std::vector<uint8_t> *png_data =
      reinterpret_cast<std::vector<uint8_t> *>(io_ptr);
  static size_t pos = 0;  // Position to read from

  if (pos + byteCountToRead <= png_data->size()) {
    memcpy(outBytes, png_data->data() + pos, byteCountToRead);
    pos += byteCountToRead;
  } else {
    png_error(png_ptr, "Read error in PngReadCallback");
  }
}

}  // namespace png_internal

bool ConvertSurfaceToPng(SDL_Surface *surface, std::vector<uint8_t> &buffer) {
  png_structp png_ptr = png_create_write_struct("1.6.40", NULL, NULL, NULL);
  if (!png_ptr) {
    SDL_Log("Failed to create PNG write struct");
    return false;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    SDL_Log("Failed to create PNG info struct");
    return false;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    SDL_Log("Error during PNG write");
    return false;
  }

  png_set_write_fn(png_ptr, &buffer, png_internal::PngWriteCallback, NULL);

  png_colorp pal_ptr;

  /* Prepare chunks */
  int colortype = PNG_COLOR_MASK_COLOR;
  int i = 0;
  SDL_Palette *pal;
  if (surface->format->BytesPerPixel > 0 &&
      surface->format->BytesPerPixel <= 8 && (pal = surface->format->palette)) {
    SDL_Log("Writing PNG image with palette");
    colortype |= PNG_COLOR_MASK_PALETTE;
    pal_ptr = (png_colorp)malloc(pal->ncolors * sizeof(png_color));
    for (i = 0; i < pal->ncolors; i++) {
      pal_ptr[i].red = pal->colors[i].r;
      pal_ptr[i].green = pal->colors[i].g;
      pal_ptr[i].blue = pal->colors[i].b;
    }
    png_set_PLTE(png_ptr, info_ptr, pal_ptr, pal->ncolors);
    free(pal_ptr);
  }

  if (surface->format->Amask) {  // Check for alpha channel
    colortype |= PNG_COLOR_MASK_ALPHA;
  }

  auto depth = surface->format->BitsPerPixel;

  // Set image attributes.
  png_set_IHDR(png_ptr, info_ptr, surface->w, surface->h, depth, colortype,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_set_bgr(png_ptr);

  // Write the image data.
  std::vector<png_bytep> row_pointers(surface->h);
  for (int y = 0; y < surface->h; ++y) {
    row_pointers[y] = (png_bytep)(surface->pixels) + y * surface->pitch;
  }

  png_set_rows(png_ptr, info_ptr, row_pointers.data());

  SDL_Log("Writing PNG image...");
  png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
  SDL_Log("PNG image write complete");

  png_destroy_write_struct(&png_ptr, &info_ptr);

  return true;
}

void ConvertPngToSurface(const std::vector<uint8_t> &png_data,
                         SDL_Surface **outSurface) {
  std::vector<uint8_t> data(png_data);
  png_structp png_ptr = png_create_read_struct("1.6.40", NULL, NULL, NULL);
  if (!png_ptr) {
    throw std::runtime_error("Failed to create PNG read struct");
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    throw std::runtime_error("Failed to create PNG info struct");
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    throw std::runtime_error("Error during PNG read");
  }

  // Set our custom read function
  png_set_read_fn(png_ptr, &data, png_internal::PngReadCallback);

  // Read the PNG info
  png_read_info(png_ptr, info_ptr);

  uint32_t width = png_get_image_width(png_ptr, info_ptr);
  uint32_t height = png_get_image_height(png_ptr, info_ptr);
  png_byte color_type = png_get_color_type(png_ptr, info_ptr);
  png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

  // Apply necessary transformations...
  // (Same as in your existing code)

  // Update info structure with transformations
  png_read_update_info(png_ptr, info_ptr);

  // Read the file
  std::vector<uint8_t> raw_data(width * height *
                                4);  // Assuming 4 bytes per pixel (RGBA)
  std::vector<png_bytep> row_pointers(height);
  for (size_t y = 0; y < height; y++) {
    row_pointers[y] = raw_data.data() + y * width * 4;
  }

  png_read_image(png_ptr, row_pointers.data());
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

  // Create an SDL_Surface
  *outSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                               SDL_PIXELFORMAT_RGBA32);
  if (*outSurface == nullptr) {
    SDL_Log("SDL_CreateRGBSurfaceWithFormat failed: %s\n", SDL_GetError());
    return;
  }

  // Copy the raw data into the SDL_Surface
  SDL_LockSurface(*outSurface);
  memcpy((*outSurface)->pixels, raw_data.data(), raw_data.size());
  SDL_UnlockSurface(*outSurface);

  SDL_Log("Successfully created SDL_Surface from PNG data");
}

#endif  // YAZE_LIB_PNG

// Utility functions
Uint32 GetSnesPixelFormat(int format) {
  switch (format) {
    case 0:
      return SDL_PIXELFORMAT_INDEX8;
    case 1:
      return SNES_PIXELFORMAT_4BPP;
    case 2:
      return SNES_PIXELFORMAT_8BPP;
  }
  return SDL_PIXELFORMAT_INDEX8;
}

Bitmap::Bitmap(int width, int height, int depth,
               const std::vector<uint8_t> &data)
    : width_(width), height_(height), depth_(depth), data_(data) {
  Create(width, height, depth, data);
}

Bitmap::Bitmap(int width, int height, int depth,
               const std::vector<uint8_t> &data, const SnesPalette &palette)
    : width_(width),
      height_(height),
      depth_(depth),
      data_(data),
      palette_(palette) {
  Create(width, height, depth, data);
  SetPalette(palette);
}

void Bitmap::Create(int width, int height, int depth, std::span<uint8_t> data) {
  data_ = std::vector<uint8_t>(data.begin(), data.end());
  Create(width, height, depth, data_);
}

void Bitmap::Create(int width, int height, int depth,
                    const std::vector<uint8_t> &data) {
  Create(width, height, depth, static_cast<int>(BitmapFormat::kIndexed), data);
}

void Bitmap::Create(int width, int height, int depth, int format,
                    const std::vector<uint8_t> &data) {
  if (data.empty()) {
    SDL_Log("Bitmap data is empty\n");
    active_ = false;
    return;
  }
  active_ = true;
  width_ = width;
  height_ = height;
  depth_ = depth;
  if (data.size() == 0) {
    SDL_Log("Data provided to Bitmap is empty.\n");
    return;
  }
  data_.reserve(data.size());
  data_ = data;
  pixel_data_ = data_.data();
  surface_ = Arena::Get().AllocateSurface(width_, height_, depth_,
                                          GetSnesPixelFormat(format));
  if (surface_ == nullptr) {
    SDL_Log("Bitmap::Create.SDL_CreateRGBSurfaceWithFormat failed: %s\n",
            SDL_GetError());
    active_ = false;
    return;
  }
  surface_->pixels = pixel_data_;
  active_ = true;
}

void Bitmap::Reformat(int format) {
  surface_ = Arena::Get().AllocateSurface(width_, height_, depth_,
                                          GetSnesPixelFormat(format));
  surface_->pixels = pixel_data_;
  active_ = true;
  SetPalette(palette_);
}

void Bitmap::UpdateTexture(SDL_Renderer *renderer) {
  if (!texture_) {
    CreateTexture(renderer);
    return;
  }
  memcpy(surface_->pixels, data_.data(), data_.size());
  Arena::Get().UpdateTexture(texture_, surface_);
}

void Bitmap::CreateTexture(SDL_Renderer *renderer) {
  if (!renderer) {
    SDL_Log("Invalid renderer passed to CreateTexture");
    return;
  }

  if (width_ <= 0 || height_ <= 0) {
    SDL_Log("Invalid texture dimensions: width=%d, height=%d\n", width_,
            height_);
    return;
  }

  // Get a texture from the Arena
  texture_ = Arena::Get().AllocateTexture(renderer, width_, height_);
  if (!texture_) {
    SDL_Log("Bitmap::CreateTexture failed to allocate texture: %s\n",
            SDL_GetError());
    return;
  }

  UpdateTextureData();
}

void Bitmap::UpdateTextureData() {
  if (!texture_ || !surface_) {
    return;
  }

  Arena::Get().UpdateTexture(texture_, surface_);
  modified_ = false;
}

void Bitmap::SetPalette(const SnesPalette &palette) {
  if (surface_ == nullptr) {
    throw std::runtime_error("Surface is null. Palette not applied");
  }
  if (surface_->format == nullptr || surface_->format->palette == nullptr) {
    throw std::runtime_error(
        "Surface format or palette is null. Palette not applied.");
  }
  palette_ = palette;

  SDL_Palette *sdl_palette = surface_->format->palette;
  if (sdl_palette == nullptr) {
    throw std::runtime_error("Failed to get SDL palette");
  }

  SDL_UnlockSurface(surface_);
  for (size_t i = 0; i < palette.size(); ++i) {
    auto pal_color = palette[i];
    sdl_palette->colors[i].r = pal_color.rgb().x;
    sdl_palette->colors[i].g = pal_color.rgb().y;
    sdl_palette->colors[i].b = pal_color.rgb().z;
    sdl_palette->colors[i].a = pal_color.rgb().w;
  }
  SDL_LockSurface(surface_);
}

void Bitmap::SetPaletteWithTransparent(const SnesPalette &palette, size_t index,
                                       int length) {
  if (index < 0 || index >= palette.size()) {
    throw std::invalid_argument("Invalid palette index");
  }

  if (length < 0 || length > palette.size()) {
    throw std::invalid_argument("Invalid palette length");
  }

  if (index + length > palette.size()) {
    throw std::invalid_argument("Palette index + length exceeds size");
  }

  if (surface_ == nullptr) {
    throw std::runtime_error("Surface is null. Palette not applied");
  }

  auto start_index = index * 7;
  palette_ = palette.sub_palette(start_index, start_index + 7);
  std::vector<ImVec4> colors;
  colors.push_back(ImVec4(0, 0, 0, 0));
  for (int i = start_index; i < start_index + 7; ++i) {
    auto pal_color = palette[i];
    colors.push_back(pal_color.rgb());
  }

  SDL_UnlockSurface(surface_);
  int i = 0;
  for (auto &each : colors) {
    surface_->format->palette->colors[i].r = each.x;
    surface_->format->palette->colors[i].g = each.y;
    surface_->format->palette->colors[i].b = each.z;
    surface_->format->palette->colors[i].a = each.w;
    i++;
  }
  SDL_LockSurface(surface_);
}

void Bitmap::SetPalette(const std::vector<SDL_Color> &palette) {
  SDL_UnlockSurface(surface_);
  for (size_t i = 0; i < palette.size(); ++i) {
    surface_->format->palette->colors[i].r = palette[i].r;
    surface_->format->palette->colors[i].g = palette[i].g;
    surface_->format->palette->colors[i].b = palette[i].b;
    surface_->format->palette->colors[i].a = palette[i].a;
  }
  SDL_LockSurface(surface_);
}

void Bitmap::WriteToPixel(int position, uint8_t value) {
  if (pixel_data_ == nullptr) {
    pixel_data_ = data_.data();
  }
  pixel_data_[position] = value;
  data_[position] = value;
  modified_ = true;
}

void Bitmap::WriteColor(int position, const ImVec4 &color) {
  // Convert ImVec4 (RGBA) to SDL_Color (RGBA)
  SDL_Color sdl_color;
  sdl_color.r = static_cast<Uint8>(color.x * 255);
  sdl_color.g = static_cast<Uint8>(color.y * 255);
  sdl_color.b = static_cast<Uint8>(color.z * 255);
  sdl_color.a = static_cast<Uint8>(color.w * 255);

  // Map SDL_Color to the nearest color index in the surface's palette
  Uint8 index =
      SDL_MapRGB(surface_->format, sdl_color.r, sdl_color.g, sdl_color.b);

  // Write the color index to the pixel data
  pixel_data_[position] = index;
  data_[position] = ConvertRgbToSnes(color);

  modified_ = true;
}

void Bitmap::Get8x8Tile(int tile_index, int x, int y,
                        std::vector<uint8_t> &tile_data,
                        int &tile_data_offset) {
  int tile_offset = tile_index * (width_ * height_);
  int tile_x = (x * 8) % width_;
  int tile_y = (y * 8) % height_;
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      int pixel_offset = tile_offset + (tile_y + i) * width_ + tile_x + j;
      int pixel_value = data_[pixel_offset];
      tile_data[tile_data_offset] = pixel_value;
      tile_data_offset++;
    }
  }
}

void Bitmap::Get16x16Tile(int tile_x, int tile_y,
                          std::vector<uint8_t> &tile_data,
                          int &tile_data_offset) {
  for (int ty = 0; ty < 16; ty++) {
    for (int tx = 0; tx < 16; tx++) {
      // Calculate the pixel position in the bitmap
      int pixel_x = tile_x + tx;
      int pixel_y = tile_y + ty;
      int pixel_offset = (pixel_y * width_) + pixel_x;
      int pixel_value = data_[pixel_offset];

      // Store the pixel value in the tile data
      tile_data[tile_data_offset++] = pixel_value;
    }
  }
}

#if YAZE_LIB_PNG == 1
std::vector<uint8_t> Bitmap::GetPngData() {
  std::vector<uint8_t> png_data;
  ConvertSurfaceToPng(surface_, png_data);
  return png_data;
}
#endif

}  // namespace gfx
}  // namespace yaze
