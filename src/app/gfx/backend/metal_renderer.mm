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

  MTLTextureDescriptor* descriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
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
  (void)format;
  (void)access;
  return CreateTexture(width, height);
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

  auto converted_surface =
      std::unique_ptr<SDL_Surface, util::SDL_Surface_Deleter>(
          platform::ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_BGRA8888));
  if (!converted_surface || !converted_surface->pixels) {
    return;
  }

  id<MTLTexture> metal_texture = (__bridge id<MTLTexture>)texture;
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
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  CFRelease(texture);
#else
  (void)texture;
#endif
}

bool MetalRenderer::LockTexture(TextureHandle texture, SDL_Rect* rect,
                                void** pixels, int* pitch) {
  (void)texture;
  (void)rect;
  (void)pixels;
  (void)pitch;
  return false;
}

void MetalRenderer::UnlockTexture(TextureHandle texture) {
  (void)texture;
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
