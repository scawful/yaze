#include "bitmap.h"

#include <SDL.h>
#include <png.h>

#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "app/core/constants.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {
namespace gfx {

namespace {
void GrayscalePalette(SDL_Palette *palette) {
  for (int i = 0; i < 8; i++) {
    palette->colors[i].r = i * 31;
    palette->colors[i].g = i * 31;
    palette->colors[i].b = i * 31;
  }
}

void PngWriteCallback(png_structp png_ptr, png_bytep data, png_size_t length) {
  std::vector<uint8_t> *p = (std::vector<uint8_t> *)png_get_io_ptr(png_ptr);
  p->insert(p->end(), data, data + length);
}

bool ConvertSurfaceToPNG(SDL_Surface *surface, std::vector<uint8_t> &buffer) {
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

  png_set_write_fn(png_ptr, &buffer, PngWriteCallback, NULL);

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

void PngReadCallback(png_structp png_ptr, png_bytep outBytes,
                     png_size_t byteCountToRead) {
  png_voidp io_ptr = png_get_io_ptr(png_ptr);
  if (!io_ptr) return;

  std::vector<uint8_t> *png_data =
      reinterpret_cast<std::vector<uint8_t> *>(io_ptr);
  size_t pos = png_data->size() - byteCountToRead;
  memcpy(outBytes, png_data->data() + pos, byteCountToRead);
  png_data->resize(pos);  // Reduce the buffer size
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
  png_set_read_fn(png_ptr, &data, PngReadCallback);

  // Read the PNG info
  png_read_info(png_ptr, info_ptr);

  uint32_t width = png_get_image_width(png_ptr, info_ptr);
  uint32_t height = png_get_image_height(png_ptr, info_ptr);
  png_byte color_type = png_get_color_type(png_ptr, info_ptr);
  png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

  // Set up transformations, e.g., strip 16-bit PNGs down to 8-bit, expand
  // palettes, etc.
  if (bit_depth == 16) {
    png_set_strip_16(png_ptr);
  }

  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(png_ptr);
  }

  // PNG files pack pixels, expand them
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    png_set_expand_gray_1_2_4_to_8(png_ptr);
  }

  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png_ptr);
  }

  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
  }

  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png_ptr);
  }

  // Update info structure with transformations
  png_read_update_info(png_ptr, info_ptr);

  // Read the file
  std::vector<png_bytep> row_pointers(height);
  std::vector<uint8_t> raw_data(width * height *
                                4);  // Assuming 4 bytes per pixel (RGBA)
  for (size_t y = 0; y < height; y++) {
    row_pointers[y] = &raw_data[y * width * 4];
  }

  png_read_image(png_ptr, row_pointers.data());
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

  // Create SDL_Surface from raw pixel data
  *outSurface = SDL_CreateRGBSurfaceWithFormatFrom(
      raw_data.data(), width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32);
  if (*outSurface == nullptr) {
    SDL_Log("SDL_CreateRGBSurfaceWithFormatFrom failed: %s\n", SDL_GetError());
  } else {
    SDL_Log("Successfully created SDL_Surface from PNG data");
  }
}
}  // namespace

Bitmap::Bitmap(int width, int height, int depth, int data_size) {
  Create(width, height, depth, data_size);
}

// Reserves data to later draw to surface via pointer
void Bitmap::Create(int width, int height, int depth, int size) {
  active_ = true;
  width_ = width;
  height_ = height;
  depth_ = depth;
  data_size_ = size;
  data_.reserve(size);
  pixel_data_ = data_.data();
  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width, height, depth,
                                     SDL_PIXELFORMAT_INDEX8),
      SDL_Surface_Deleter());
  surface_->pixels = pixel_data_;
  GrayscalePalette(surface_->format->palette);
}

void Bitmap::Create(int width, int height, int depth, const Bytes &data) {
  active_ = true;
  width_ = width;
  height_ = height;
  depth_ = depth;
  data_ = data;
  pixel_data_ = data_.data();
  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width_, height_, depth_,
                                     SDL_PIXELFORMAT_INDEX8),
      SDL_Surface_Deleter());
  surface_->pixels = pixel_data_;
  GrayscalePalette(surface_->format->palette);
}

// Creates the texture that will be displayed to the screen.
void Bitmap::CreateTexture(SDL_Renderer *renderer) {
  // Ensure width and height are non-zero
  if (width_ <= 0 || height_ <= 0) {
    SDL_Log("Invalid texture dimensions: width=%d, height=%d\n", width_,
            height_);
    return;
  }

  texture_ = std::shared_ptr<SDL_Texture>{
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                        SDL_TEXTUREACCESS_STREAMING, width_, height_),
      SDL_Texture_Deleter{}};
  if (texture_ == nullptr) {
    SDL_Log("SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
  }

  SDL_Surface *converted_surface =
      SDL_ConvertSurfaceFormat(surface_.get(), SDL_PIXELFORMAT_ARGB8888, 0);
  if (converted_surface) {
    // Create texture from the converted surface
    converted_surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
        converted_surface, SDL_Surface_Deleter());
  } else {
    // Handle the error
    SDL_Log("SDL_ConvertSurfaceFormat failed: %s\n", SDL_GetError());
  }

  SDL_LockTexture(texture_.get(), nullptr, (void **)&texture_pixels,
                  &converted_surface_->pitch);

  memcpy(texture_pixels, converted_surface_->pixels,
         converted_surface_->h * converted_surface_->pitch);

  SDL_UnlockTexture(texture_.get());
}

void Bitmap::UpdateTexture(SDL_Renderer *renderer) {
  SDL_Surface *converted_surface =
      SDL_ConvertSurfaceFormat(surface_.get(), SDL_PIXELFORMAT_ARGB8888, 0);
  if (converted_surface) {
    // Create texture from the converted surface
    converted_surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
        converted_surface, SDL_Surface_Deleter());
  } else {
    // Handle the error
    SDL_Log("SDL_ConvertSurfaceFormat failed: %s\n", SDL_GetError());
  }

  SDL_LockTexture(texture_.get(), nullptr, (void **)&texture_pixels,
                  &converted_surface_->pitch);

  memcpy(texture_pixels, converted_surface_->pixels,
         converted_surface_->h * converted_surface_->pitch);

  SDL_UnlockTexture(texture_.get());
}

// Creates the texture that will be displayed to the screen.
void Bitmap::CreateTexture(std::shared_ptr<SDL_Renderer> renderer) {
  texture_ = std::shared_ptr<SDL_Texture>{
      SDL_CreateTextureFromSurface(renderer.get(), surface_.get()),
      SDL_Texture_Deleter{}};
}

void Bitmap::UpdateTexture(std::shared_ptr<SDL_Renderer> renderer) {
  // SDL_DestroyTexture(texture_.get());
  // texture_ = nullptr;
  texture_ = std::shared_ptr<SDL_Texture>{
      SDL_CreateTextureFromSurface(renderer.get(), surface_.get()),
      SDL_Texture_Deleter{}};
}

void Bitmap::SaveSurfaceToFile(std::string_view filename) {
  SDL_SaveBMP(surface_.get(), filename.data());
}

void Bitmap::SetSurface(SDL_Surface *surface) {
  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      surface, SDL_Surface_Deleter());
}

std::vector<uint8_t> Bitmap::GetPngData() {
  ConvertSurfaceToPNG(surface_.get(), png_data_);
  return png_data_;
}

void Bitmap::LoadFromPngData(const std::vector<uint8_t> &png_data, int width,
                             int height) {
  width_ = width;
  height_ = height;
  SDL_Surface *surface = surface_.get();
  ConvertPngToSurface(png_data, &surface);
  surface_.reset(surface);
}

// Convert SNESPalette to SDL_Palette for surface.
void Bitmap::ApplyPalette(const SNESPalette &palette) {
  palette_ = palette;
  SDL_UnlockSurface(surface_.get());
  for (int i = 0; i < palette.size(); ++i) {
    if (palette.GetColor(i).IsTransparent()) {
      surface_->format->palette->colors[i].r = 0;
      surface_->format->palette->colors[i].g = 0;
      surface_->format->palette->colors[i].b = 0;
      surface_->format->palette->colors[i].a = 0;
    } else {
      surface_->format->palette->colors[i].r = palette.GetColor(i).GetRGB().x;
      surface_->format->palette->colors[i].g = palette.GetColor(i).GetRGB().y;
      surface_->format->palette->colors[i].b = palette.GetColor(i).GetRGB().z;
      surface_->format->palette->colors[i].a = palette.GetColor(i).GetRGB().w;
    }
  }
  SDL_LockSurface(surface_.get());
}

void Bitmap::ApplyPaletteWithTransparent(const SNESPalette &palette,
                                         int index) {
  palette_ = palette;
  auto start_index = index * 7;
  std::vector<ImVec4> colors;
  colors.push_back(ImVec4(0, 0, 0, 0));
  for (int i = start_index; i < start_index + 7; ++i) {
    colors.push_back(palette.GetColor(i).GetRGB());
  }

  SDL_UnlockSurface(surface_.get());
  int i = 0;
  for (auto &each : colors) {
    surface_->format->palette->colors[i].r = each.x;
    surface_->format->palette->colors[i].g = each.y;
    surface_->format->palette->colors[i].b = each.z;
    surface_->format->palette->colors[i].a = each.w;
    i++;
  }
  SDL_LockSurface(surface_.get());
}

void Bitmap::ApplyPalette(const std::vector<SDL_Color> &palette) {
  SDL_UnlockSurface(surface_.get());
  for (int i = 0; i < palette.size(); ++i) {
    surface_->format->palette->colors[i].r = palette[i].r;
    surface_->format->palette->colors[i].g = palette[i].g;
    surface_->format->palette->colors[i].b = palette[i].b;
    surface_->format->palette->colors[i].a = palette[i].a;
  }
  SDL_LockSurface(surface_.get());
}

void Bitmap::InitializeFromData(uint32_t width, uint32_t height, uint32_t depth,
                                const Bytes &data) {
  active_ = true;
  width_ = width;
  height_ = height;
  depth_ = depth;
  data_ = data;
  data_size_ = data.size();
  pixel_data_ = data_.data();

  surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
      SDL_CreateRGBSurfaceWithFormat(0, width_, height_, depth_,
                                     SDL_PIXELFORMAT_INDEX8),
      SDL_Surface_Deleter());

  surface_->pixels = pixel_data_;
  GrayscalePalette(surface_->format->palette);
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze
