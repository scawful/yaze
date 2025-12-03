#ifndef YAZE_TEST_FRAMEWORK_MOCK_RENDERER_H_
#define YAZE_TEST_FRAMEWORK_MOCK_RENDERER_H_

#include "app/gfx/backend/irenderer.h"
#include "gmock/gmock.h"

namespace yaze {
namespace test {

class MockRenderer : public gfx::IRenderer {
 public:
  MOCK_METHOD(bool, Initialize, (SDL_Window* window), (override));
  MOCK_METHOD(void, Shutdown, (), (override));

  MOCK_METHOD(gfx::TextureHandle, CreateTexture, (int width, int height), (override));
  MOCK_METHOD(gfx::TextureHandle, CreateTextureWithFormat, 
              (int width, int height, uint32_t format, int access), (override));
  
  MOCK_METHOD(void, UpdateTexture, (gfx::TextureHandle texture, const gfx::Bitmap& bitmap), (override));
  MOCK_METHOD(void, DestroyTexture, (gfx::TextureHandle texture), (override));

  MOCK_METHOD(bool, LockTexture, (gfx::TextureHandle texture, SDL_Rect* rect, void** pixels, int* pitch), (override));
  MOCK_METHOD(void, UnlockTexture, (gfx::TextureHandle texture), (override));

  MOCK_METHOD(void, Clear, (), (override));
  MOCK_METHOD(void, Present, (), (override));
  MOCK_METHOD(void, RenderCopy, (gfx::TextureHandle texture, const SDL_Rect* srcrect, const SDL_Rect* dstrect), (override));
  
  MOCK_METHOD(void, SetRenderTarget, (gfx::TextureHandle texture), (override));
  MOCK_METHOD(void, SetDrawColor, (SDL_Color color), (override));
  
  MOCK_METHOD(void*, GetBackendRenderer, (), (override));
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_FRAMEWORK_MOCK_RENDERER_H_
