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
}  // namespace

bool ConvertSurfaceToPNG(SDL_Surface *surface, std::vector<uint8_t> &buffer) {
    png_structp png_ptr = png_create_write_struct("1.6.40", NULL, NULL,
    NULL); if (!png_ptr) {
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
        surface->format->BytesPerPixel <= 8 && (pal =
        surface->format->palette)) {
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
    png_set_read_fn(png_ptr, &data, PngReadCallback);

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

Bitmap::Bitmap(int width, int height, int depth, int data_size) {
  Create(width, height, depth, Bytes(data_size, 0));
}

void Bitmap::Create(int width, int height, int depth, const Bytes &data) {
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

void Bitmap::CreateTexture(SDL_Renderer *renderer) {
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
    converted_surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
        converted_surface, SDL_Surface_Deleter());
  } else {
    SDL_Log("SDL_ConvertSurfaceFormat failed: %s\n", SDL_GetError());
  }

  SDL_LockTexture(texture_.get(), nullptr, (void **)&texture_pixels,
                  &converted_surface_->pitch);

  memcpy(texture_pixels, converted_surface_->pixels,
         converted_surface_->h * converted_surface_->pitch);

  SDL_UnlockTexture(texture_.get());
}

void Bitmap::UpdateTexture(SDL_Renderer *renderer, bool use_sdl_update) {
  SDL_Surface *converted_surface =
      SDL_ConvertSurfaceFormat(surface_.get(), SDL_PIXELFORMAT_ARGB8888, 0);
  if (converted_surface) {
    converted_surface_ = std::unique_ptr<SDL_Surface, SDL_Surface_Deleter>(
        converted_surface, SDL_Surface_Deleter());
  } else {
    SDL_Log("SDL_ConvertSurfaceFormat failed: %s\n", SDL_GetError());
  }

  SDL_LockTexture(texture_.get(), nullptr, (void **)&texture_pixels,
                  &converted_surface_->pitch);

  try {
    if (use_sdl_update) {
      SDL_UpdateTexture(texture_.get(), nullptr, converted_surface_->pixels,
                        converted_surface_->pitch);
    } else {
      memcpy(texture_pixels, converted_surface_->pixels,
             converted_surface_->h * converted_surface_->pitch);
    }
  } catch (const std::exception &e) {
    SDL_Log("Exception: %s\n", e.what());
  }

  SDL_UnlockTexture(texture_.get());
}

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
absl::Status Bitmap::ApplyPalette(const SnesPalette &palette) {
  if (surface_ == nullptr) {
    return absl::FailedPreconditionError("Surface is null");
  }

  if (surface_->format == nullptr || surface_->format->palette == nullptr) {
    return absl::FailedPreconditionError("Surface format or palette is null");
  }

  palette_ = palette;

  SDL_Palette *sdlPalette = surface_->format->palette;
  if (sdlPalette == nullptr) {
    return absl::InternalError("Failed to get SDL palette");
  }

  SDL_UnlockSurface(surface_.get());

  for (int i = 0; i < palette.size(); ++i) {
    ASSIGN_OR_RETURN(gfx::SnesColor pal_color, palette.GetColor(i));
    if (pal_color.is_transparent()) {
      sdlPalette->colors[i].r = 0;
      sdlPalette->colors[i].g = 0;
      sdlPalette->colors[i].b = 0;
      sdlPalette->colors[i].a = 0;
    } else {
      sdlPalette->colors[i].r = pal_color.rgb().x;
      sdlPalette->colors[i].g = pal_color.rgb().y;
      sdlPalette->colors[i].b = pal_color.rgb().z;
      sdlPalette->colors[i].a = pal_color.rgb().w;
    }
  }

  SDL_LockSurface(surface_.get());

  return absl::OkStatus();
}

absl::Status Bitmap::ApplyPaletteFromPaletteGroup(const SnesPalette &palette,
                                                  int palette_id) {
  auto start_index = palette_id * 8;
  palette_ = palette.sub_palette(start_index, start_index + 8);
  SDL_UnlockSurface(surface_.get());
  for (int i = 0; i < palette_.size(); ++i) {
    ASSIGN_OR_RETURN(auto pal_color, palette_.GetColor(i));
    if (pal_color.is_transparent()) {
      surface_->format->palette->colors[i].r = 0;
      surface_->format->palette->colors[i].g = 0;
      surface_->format->palette->colors[i].b = 0;
      surface_->format->palette->colors[i].a = 0;
    } else {
      surface_->format->palette->colors[i].r = pal_color.rgb().x;
      surface_->format->palette->colors[i].g = pal_color.rgb().y;
      surface_->format->palette->colors[i].b = pal_color.rgb().z;
      surface_->format->palette->colors[i].a = pal_color.rgb().w;
    }
  }
  SDL_LockSurface(surface_.get());
  return absl::OkStatus();
}

absl::Status Bitmap::ApplyPaletteWithTransparent(const SnesPalette &palette,
                                                 int index, int length) {
  auto start_index = index * 7;
  palette_ = palette.sub_palette(start_index, start_index + 7);
  std::vector<ImVec4> colors;
  colors.push_back(ImVec4(0, 0, 0, 0));
  for (int i = start_index; i < start_index + 7; ++i) {
    ASSIGN_OR_RETURN(auto pal_color, palette.GetColor(i));
    colors.push_back(pal_color.rgb());
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
  return absl::OkStatus();
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

}  // namespace gfx
}  // namespace app
}  // namespace yaze
