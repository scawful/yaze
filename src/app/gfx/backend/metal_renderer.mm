#include "app/gfx/backend/metal_renderer.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#import <CoreFoundation/CoreFoundation.h>
#endif

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#endif

#include "app/gfx/core/bitmap.h"
#include "app/platform/sdl_compat.h"
#include "util/log.h"
#include "util/sdl_deleter.h"
#include <algorithm>

namespace yaze {
namespace gfx {

namespace {

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
MTLPixelFormat GetMetalPixelFormatForSDL(uint32_t format) {
  switch (format) {
    case SDL_PIXELFORMAT_ARGB8888:
    case SDL_PIXELFORMAT_BGRA8888:
      return MTLPixelFormatBGRA8Unorm;
    case SDL_PIXELFORMAT_RGBA8888:
    case SDL_PIXELFORMAT_ABGR8888:
      return MTLPixelFormatRGBA8Unorm;
    default:
      return MTLPixelFormatRGBA8Unorm;
  }
}

int BytesPerPixel(MTLPixelFormat format) {
  switch (format) {
    case MTLPixelFormatBGRA8Unorm:
    case MTLPixelFormatRGBA8Unorm:
      return 4;
    default:
      return 4;
  }
}
#endif

}  // namespace

MetalRenderer::~MetalRenderer() {
  Shutdown();
}

bool MetalRenderer::Initialize(SDL_Window* window) {
  (void)window;
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (!metal_view_) {
    LOG_WARN("MetalRenderer", "Metal view not attached");
    return false;
  }
  auto* view = static_cast<MTKView*>(metal_view_);
  id<MTLDevice> device = view.device;
  if (!device) {
    device = MTLCreateSystemDefaultDevice();
    view.device = device;
  }
  if (!device) {
    LOG_WARN("MetalRenderer", "Failed to create Metal device");
    return false;
  }
  if (!command_queue_) {
    id<MTLCommandQueue> queue = [device newCommandQueue];
    command_queue_ = (__bridge_retained void*)queue;
  }
  return true;
#else
  return false;
#endif
}

void MetalRenderer::Shutdown() {
  if (command_queue_) {
    CFRelease(command_queue_);
    command_queue_ = nullptr;
  }
  render_target_ = nullptr;
}

TextureHandle MetalRenderer::CreateTexture(int width, int height) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (!metal_view_) {
    return nullptr;
  }

  auto* view = static_cast<MTKView*>(metal_view_);
  id<MTLDevice> device = view.device;
  if (!device) {
    device = MTLCreateSystemDefaultDevice();
    view.device = device;
  }
  if (!device) {
    return nullptr;
  }

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  MTLPixelFormat default_format = MTLPixelFormatRGBA8Unorm;
#else
  MTLPixelFormat default_format = MTLPixelFormatBGRA8Unorm;
#endif
  MTLTextureDescriptor* descriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:default_format
                                                         width:width
                                                        height:height
                                                     mipmapped:NO];
  descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
  descriptor.storageMode = MTLStorageModeShared;

  id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
  return texture ? (__bridge_retained void*)texture : nullptr;
#else
  (void)width;
  (void)height;
  return nullptr;
#endif
}

TextureHandle MetalRenderer::CreateTextureWithFormat(int width, int height,
                                                     uint32_t format,
                                                     int access) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (!metal_view_) {
    return nullptr;
  }
  (void)access;

  auto* view = static_cast<MTKView*>(metal_view_);
  id<MTLDevice> device = view.device;
  if (!device) {
    device = MTLCreateSystemDefaultDevice();
    view.device = device;
  }
  if (!device) {
    return nullptr;
  }

  MTLPixelFormat pixel_format = GetMetalPixelFormatForSDL(format);
  MTLTextureDescriptor* descriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixel_format
                                                         width:width
                                                        height:height
                                                     mipmapped:NO];
  descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
  descriptor.storageMode = MTLStorageModeShared;

  id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
  return texture ? (__bridge_retained void*)texture : nullptr;
#else
  (void)format;
  (void)access;
  return CreateTexture(width, height);
#endif
}

void MetalRenderer::UpdateTexture(TextureHandle texture, const Bitmap& bitmap) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (!texture) {
    return;
  }

  SDL_Surface* surface = bitmap.surface();
  if (!surface || !surface->pixels || surface->w <= 0 || surface->h <= 0) {
    return;
  }

  id<MTLTexture> metal_texture = (__bridge id<MTLTexture>)texture;
  const MTLPixelFormat pixel_format = metal_texture.pixelFormat;
  uint32_t target_format = SDL_PIXELFORMAT_RGBA8888;
  if (pixel_format == MTLPixelFormatBGRA8Unorm) {
    target_format = SDL_PIXELFORMAT_BGRA8888;
  }

  auto converted_surface =
      std::unique_ptr<SDL_Surface, util::SDL_Surface_Deleter>(
          platform::ConvertSurfaceFormat(surface, target_format));
  if (!converted_surface || !converted_surface->pixels) {
    return;
  }

  MTLRegion region = {
      {0, 0, 0},
      {static_cast<NSUInteger>(converted_surface->w),
       static_cast<NSUInteger>(converted_surface->h),
       1}};
  [metal_texture replaceRegion:region
                   mipmapLevel:0
                     withBytes:converted_surface->pixels
                   bytesPerRow:converted_surface->pitch];
#else
  (void)texture;
  (void)bitmap;
#endif
}

void MetalRenderer::DestroyTexture(TextureHandle texture) {
  if (!texture) {
    return;
  }
  if (render_target_ == texture) {
    render_target_ = nullptr;
  }
  staging_buffers_.erase(texture);
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  CFRelease(texture);
#else
  (void)texture;
#endif
}

bool MetalRenderer::LockTexture(TextureHandle texture, SDL_Rect* rect,
                                void** pixels, int* pitch) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (!texture || !pixels || !pitch) {
    return false;
  }

  id<MTLTexture> metal_texture = (__bridge id<MTLTexture>)texture;
  const int width = static_cast<int>(metal_texture.width);
  const int height = static_cast<int>(metal_texture.height);
  const int bytes_per_pixel = BytesPerPixel(metal_texture.pixelFormat);
  const int row_pitch = width * bytes_per_pixel;
  if (row_pitch <= 0 || height <= 0) {
    return false;
  }

  auto& staging = staging_buffers_[texture];
  staging.width = width;
  staging.height = height;
  staging.pitch = row_pitch;
  staging.data.resize(static_cast<size_t>(row_pitch) *
                      static_cast<size_t>(height));

  if (rect) {
    rect->x = 0;
    rect->y = 0;
    rect->w = width;
    rect->h = height;
  }

  *pixels = staging.data.data();
  *pitch = row_pitch;
  return true;
#else
  (void)texture;
  (void)rect;
  (void)pixels;
  (void)pitch;
  return false;
#endif
}

void MetalRenderer::UnlockTexture(TextureHandle texture) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (!texture) {
    return;
  }
  auto it = staging_buffers_.find(texture);
  if (it == staging_buffers_.end()) {
    return;
  }

  id<MTLTexture> metal_texture = (__bridge id<MTLTexture>)texture;
  auto& staging = it->second;
  if (staging.data.empty()) {
    return;
  }

  MTLRegion region = {
      {0, 0, 0},
      {static_cast<NSUInteger>(staging.width),
       static_cast<NSUInteger>(staging.height),
       1}};
  [metal_texture replaceRegion:region
                   mipmapLevel:0
                     withBytes:staging.data.data()
                   bytesPerRow:staging.pitch];
#else
  (void)texture;
#endif
}

void MetalRenderer::Clear() {
}

void MetalRenderer::Present() {
}

void MetalRenderer::RenderCopy(TextureHandle texture, const SDL_Rect* srcrect,
                               const SDL_Rect* dstrect) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (!texture || !render_target_ || !command_queue_) {
    return;
  }

  id<MTLTexture> source = (__bridge id<MTLTexture>)texture;
  id<MTLTexture> dest = (__bridge id<MTLTexture>)render_target_;

  int src_x = srcrect ? srcrect->x : 0;
  int src_y = srcrect ? srcrect->y : 0;
  int src_w = srcrect ? srcrect->w : static_cast<int>(source.width);
  int src_h = srcrect ? srcrect->h : static_cast<int>(source.height);

  int dst_x = dstrect ? dstrect->x : 0;
  int dst_y = dstrect ? dstrect->y : 0;

  src_w = std::min(src_w, static_cast<int>(source.width) - src_x);
  src_h = std::min(src_h, static_cast<int>(source.height) - src_y);

  if (src_w <= 0 || src_h <= 0) {
    return;
  }

  MTLOrigin src_origin = {static_cast<NSUInteger>(src_x),
                          static_cast<NSUInteger>(src_y),
                          0};
  MTLSize src_size = {static_cast<NSUInteger>(src_w),
                      static_cast<NSUInteger>(src_h),
                      1};
  MTLOrigin dst_origin = {static_cast<NSUInteger>(dst_x),
                          static_cast<NSUInteger>(dst_y),
                          0};

  id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)command_queue_;
  id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
  id<MTLBlitCommandEncoder> blit = [command_buffer blitCommandEncoder];
  [blit copyFromTexture:source
            sourceSlice:0
            sourceLevel:0
           sourceOrigin:src_origin
             sourceSize:src_size
              toTexture:dest
       destinationSlice:0
       destinationLevel:0
      destinationOrigin:dst_origin];
  [blit endEncoding];
  [command_buffer commit];
  [command_buffer waitUntilCompleted];
#else
  (void)texture;
  (void)srcrect;
  (void)dstrect;
#endif
}

void MetalRenderer::SetRenderTarget(TextureHandle texture) {
  render_target_ = texture;
}

void MetalRenderer::SetDrawColor(SDL_Color color) {
  (void)color;
}

void* MetalRenderer::GetBackendRenderer() {
  return command_queue_;
}

void MetalRenderer::SetMetalView(void* view) {
  metal_view_ = view;
}

}  // namespace gfx
}  // namespace yaze
