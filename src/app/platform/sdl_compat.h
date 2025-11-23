#ifndef YAZE_APP_PLATFORM_SDL_COMPAT_H_
#define YAZE_APP_PLATFORM_SDL_COMPAT_H_

/**
 * @file sdl_compat.h
 * @brief SDL2/SDL3 compatibility layer
 *
 * This header provides cross-version compatibility between SDL2 and SDL3.
 * It defines type aliases, macros, and wrapper functions that allow
 * application code to work with both versions.
 */

#ifdef YAZE_USE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

namespace yaze {
namespace platform {

// ============================================================================
// Type Aliases
// ============================================================================

#ifdef YAZE_USE_SDL3
// SDL3 uses bool* for keyboard state
using KeyboardState = const bool*;
#else
// SDL2 uses Uint8* for keyboard state
using KeyboardState = const Uint8*;
#endif

// ============================================================================
// Event Type Constants
// ============================================================================

#ifdef YAZE_USE_SDL3
constexpr auto kEventKeyDown = SDL_EVENT_KEY_DOWN;
constexpr auto kEventKeyUp = SDL_EVENT_KEY_UP;
constexpr auto kEventMouseMotion = SDL_EVENT_MOUSE_MOTION;
constexpr auto kEventMouseButtonDown = SDL_EVENT_MOUSE_BUTTON_DOWN;
constexpr auto kEventMouseButtonUp = SDL_EVENT_MOUSE_BUTTON_UP;
constexpr auto kEventMouseWheel = SDL_EVENT_MOUSE_WHEEL;
constexpr auto kEventQuit = SDL_EVENT_QUIT;
constexpr auto kEventDropFile = SDL_EVENT_DROP_FILE;
constexpr auto kEventWindowCloseRequested = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
constexpr auto kEventWindowResized = SDL_EVENT_WINDOW_RESIZED;
constexpr auto kEventGamepadAdded = SDL_EVENT_GAMEPAD_ADDED;
constexpr auto kEventGamepadRemoved = SDL_EVENT_GAMEPAD_REMOVED;
#else
constexpr auto kEventKeyDown = SDL_KEYDOWN;
constexpr auto kEventKeyUp = SDL_KEYUP;
constexpr auto kEventMouseMotion = SDL_MOUSEMOTION;
constexpr auto kEventMouseButtonDown = SDL_MOUSEBUTTONDOWN;
constexpr auto kEventMouseButtonUp = SDL_MOUSEBUTTONUP;
constexpr auto kEventMouseWheel = SDL_MOUSEWHEEL;
constexpr auto kEventQuit = SDL_QUIT;
constexpr auto kEventDropFile = SDL_DROPFILE;
// SDL2 uses SDL_WINDOWEVENT with sub-types, not individual events
// These are handled specially in window code
constexpr auto kEventWindowEvent = SDL_WINDOWEVENT;
constexpr auto kEventControllerDeviceAdded = SDL_CONTROLLERDEVICEADDED;
constexpr auto kEventControllerDeviceRemoved = SDL_CONTROLLERDEVICEREMOVED;
#endif

// ============================================================================
// Keyboard Helpers
// ============================================================================

/**
 * @brief Get keyboard state from SDL event
 * @param event The SDL event
 * @return The keycode from the event
 */
inline SDL_Keycode GetKeyFromEvent(const SDL_Event& event) {
#ifdef YAZE_USE_SDL3
  return event.key.key;
#else
  return event.key.keysym.sym;
#endif
}

/**
 * @brief Check if a key is pressed using the keyboard state
 * @param state The keyboard state from SDL_GetKeyboardState
 * @param scancode The scancode to check
 * @return True if the key is pressed
 */
inline bool IsKeyPressed(KeyboardState state, SDL_Scancode scancode) {
#ifdef YAZE_USE_SDL3
  // SDL3 returns bool*
  return state[scancode];
#else
  // SDL2 returns Uint8*, non-zero means pressed
  return state[scancode] != 0;
#endif
}

// ============================================================================
// Gamepad/Controller Helpers
// ============================================================================

#ifdef YAZE_USE_SDL3
// SDL3 uses SDL_Gamepad instead of SDL_GameController
using GamepadHandle = SDL_Gamepad*;

inline GamepadHandle OpenGamepad(int index) {
  SDL_JoystickID* joysticks = SDL_GetGamepads(nullptr);
  if (joysticks && index < 4) {
    SDL_JoystickID id = joysticks[index];
    SDL_free(joysticks);
    return SDL_OpenGamepad(id);
  }
  if (joysticks) SDL_free(joysticks);
  return nullptr;
}

inline void CloseGamepad(GamepadHandle gamepad) {
  if (gamepad) SDL_CloseGamepad(gamepad);
}

inline bool GetGamepadButton(GamepadHandle gamepad, SDL_GamepadButton button) {
  return SDL_GetGamepadButton(gamepad, button);
}

inline int16_t GetGamepadAxis(GamepadHandle gamepad, SDL_GamepadAxis axis) {
  return SDL_GetGamepadAxis(gamepad, axis);
}

inline bool IsGamepadConnected(int index) {
  int count = 0;
  SDL_JoystickID* joysticks = SDL_GetGamepads(&count);
  if (joysticks) {
    SDL_free(joysticks);
  }
  return index < count;
}
#else
// SDL2 uses SDL_GameController
using GamepadHandle = SDL_GameController*;

inline GamepadHandle OpenGamepad(int index) {
  if (SDL_IsGameController(index)) {
    return SDL_GameControllerOpen(index);
  }
  return nullptr;
}

inline void CloseGamepad(GamepadHandle gamepad) {
  if (gamepad) SDL_GameControllerClose(gamepad);
}

inline bool GetGamepadButton(GamepadHandle gamepad,
                             SDL_GameControllerButton button) {
  return SDL_GameControllerGetButton(gamepad, button) != 0;
}

inline int16_t GetGamepadAxis(GamepadHandle gamepad,
                              SDL_GameControllerAxis axis) {
  return SDL_GameControllerGetAxis(gamepad, axis);
}

inline bool IsGamepadConnected(int index) {
  return SDL_IsGameController(index);
}
#endif

// ============================================================================
// Button/Axis Type Aliases
// ============================================================================

#ifdef YAZE_USE_SDL3
using GamepadButton = SDL_GamepadButton;
using GamepadAxis = SDL_GamepadAxis;

constexpr auto kGamepadButtonA = SDL_GAMEPAD_BUTTON_SOUTH;
constexpr auto kGamepadButtonB = SDL_GAMEPAD_BUTTON_EAST;
constexpr auto kGamepadButtonX = SDL_GAMEPAD_BUTTON_WEST;
constexpr auto kGamepadButtonY = SDL_GAMEPAD_BUTTON_NORTH;
constexpr auto kGamepadButtonBack = SDL_GAMEPAD_BUTTON_BACK;
constexpr auto kGamepadButtonStart = SDL_GAMEPAD_BUTTON_START;
constexpr auto kGamepadButtonLeftShoulder = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
constexpr auto kGamepadButtonRightShoulder = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
constexpr auto kGamepadButtonDpadUp = SDL_GAMEPAD_BUTTON_DPAD_UP;
constexpr auto kGamepadButtonDpadDown = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
constexpr auto kGamepadButtonDpadLeft = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
constexpr auto kGamepadButtonDpadRight = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;

constexpr auto kGamepadAxisLeftX = SDL_GAMEPAD_AXIS_LEFTX;
constexpr auto kGamepadAxisLeftY = SDL_GAMEPAD_AXIS_LEFTY;
#else
using GamepadButton = SDL_GameControllerButton;
using GamepadAxis = SDL_GameControllerAxis;

constexpr auto kGamepadButtonA = SDL_CONTROLLER_BUTTON_A;
constexpr auto kGamepadButtonB = SDL_CONTROLLER_BUTTON_B;
constexpr auto kGamepadButtonX = SDL_CONTROLLER_BUTTON_X;
constexpr auto kGamepadButtonY = SDL_CONTROLLER_BUTTON_Y;
constexpr auto kGamepadButtonBack = SDL_CONTROLLER_BUTTON_BACK;
constexpr auto kGamepadButtonStart = SDL_CONTROLLER_BUTTON_START;
constexpr auto kGamepadButtonLeftShoulder = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
constexpr auto kGamepadButtonRightShoulder = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
constexpr auto kGamepadButtonDpadUp = SDL_CONTROLLER_BUTTON_DPAD_UP;
constexpr auto kGamepadButtonDpadDown = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
constexpr auto kGamepadButtonDpadLeft = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
constexpr auto kGamepadButtonDpadRight = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;

constexpr auto kGamepadAxisLeftX = SDL_CONTROLLER_AXIS_LEFTX;
constexpr auto kGamepadAxisLeftY = SDL_CONTROLLER_AXIS_LEFTY;
#endif

// ============================================================================
// Renderer Helpers
// ============================================================================

/**
 * @brief Create a renderer with default settings.
 *
 * SDL2: SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED)
 * SDL3: SDL_CreateRenderer(window, nullptr)
 */
inline SDL_Renderer* CreateRenderer(SDL_Window* window) {
#ifdef YAZE_USE_SDL3
  return SDL_CreateRenderer(window, nullptr);
#else
  return SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
#endif
}

/**
 * @brief Set vertical sync for the renderer.
 *
 * SDL2: VSync is set at renderer creation time via flags
 * SDL3: SDL_SetRenderVSync(renderer, interval)
 */
inline void SetRenderVSync(SDL_Renderer* renderer, int interval) {
#ifdef YAZE_USE_SDL3
  SDL_SetRenderVSync(renderer, interval);
#else
  // SDL2 sets vsync at creation time, this is a no-op
  (void)renderer;
  (void)interval;
#endif
}

/**
 * @brief Render a texture to the current render target.
 *
 * SDL2: SDL_RenderCopy(renderer, texture, srcrect, dstrect)
 * SDL3: SDL_RenderTexture(renderer, texture, srcrect, dstrect)
 *
 * Note: This version handles the int to float conversion for SDL3.
 */
inline bool RenderTexture(SDL_Renderer* renderer, SDL_Texture* texture,
                          const SDL_Rect* srcrect, const SDL_Rect* dstrect) {
#ifdef YAZE_USE_SDL3
  SDL_FRect src_frect, dst_frect;
  SDL_FRect* src_ptr = nullptr;
  SDL_FRect* dst_ptr = nullptr;

  if (srcrect) {
    src_frect.x = static_cast<float>(srcrect->x);
    src_frect.y = static_cast<float>(srcrect->y);
    src_frect.w = static_cast<float>(srcrect->w);
    src_frect.h = static_cast<float>(srcrect->h);
    src_ptr = &src_frect;
  }

  if (dstrect) {
    dst_frect.x = static_cast<float>(dstrect->x);
    dst_frect.y = static_cast<float>(dstrect->y);
    dst_frect.w = static_cast<float>(dstrect->w);
    dst_frect.h = static_cast<float>(dstrect->h);
    dst_ptr = &dst_frect;
  }

  return SDL_RenderTexture(renderer, texture, src_ptr, dst_ptr);
#else
  return SDL_RenderCopy(renderer, texture, srcrect, dstrect) == 0;
#endif
}

// ============================================================================
// Surface Helpers
// ============================================================================

/**
 * @brief Free/destroy a surface.
 *
 * SDL2: SDL_FreeSurface(surface)
 * SDL3: SDL_DestroySurface(surface)
 */
inline void FreeSurface(SDL_Surface* surface) {
  if (!surface) return;
#ifdef YAZE_USE_SDL3
  SDL_DestroySurface(surface);
#else
  SDL_FreeSurface(surface);
#endif
}

/**
 * @brief Convert a surface to a specific pixel format.
 *
 * SDL2: SDL_ConvertSurfaceFormat(surface, format, flags)
 * SDL3: SDL_ConvertSurface(surface, format)
 */
inline SDL_Surface* ConvertSurfaceFormat(SDL_Surface* surface, uint32_t format,
                                         uint32_t flags = 0) {
  if (!surface) return nullptr;
#ifdef YAZE_USE_SDL3
  (void)flags;  // SDL3 removed flags parameter
  return SDL_ConvertSurface(surface, format);
#else
  return SDL_ConvertSurfaceFormat(surface, format, flags);
#endif
}

/**
 * @brief Get bits per pixel from a surface.
 *
 * SDL2: surface->format->BitsPerPixel
 * SDL3: SDL_GetPixelFormatDetails(surface->format)->bits_per_pixel
 */
inline int GetSurfaceBitsPerPixel(SDL_Surface* surface) {
  if (!surface) return 0;
#ifdef YAZE_USE_SDL3
  const SDL_PixelFormatDetails* details =
      SDL_GetPixelFormatDetails(surface->format);
  return details ? details->bits_per_pixel : 0;
#else
  return surface->format ? surface->format->BitsPerPixel : 0;
#endif
}

/**
 * @brief Get bytes per pixel from a surface.
 *
 * SDL2: surface->format->BytesPerPixel
 * SDL3: SDL_GetPixelFormatDetails(surface->format)->bytes_per_pixel
 */
inline int GetSurfaceBytesPerPixel(SDL_Surface* surface) {
  if (!surface) return 0;
#ifdef YAZE_USE_SDL3
  const SDL_PixelFormatDetails* details =
      SDL_GetPixelFormatDetails(surface->format);
  return details ? details->bytes_per_pixel : 0;
#else
  return surface->format ? surface->format->BytesPerPixel : 0;
#endif
}

// ============================================================================
// Window Event Compatibility Macros
// These macros allow code to handle window events consistently across SDL2/SDL3
// ============================================================================

#ifdef YAZE_USE_SDL3

// SDL3 has individual window events at the top level
#define YAZE_SDL_QUIT SDL_EVENT_QUIT
#define YAZE_SDL_WINDOWEVENT 0  // Placeholder - SDL3 has no combined event

// SDL3 window events are individual event types
#define YAZE_SDL_WINDOW_CLOSE SDL_EVENT_WINDOW_CLOSE_REQUESTED
#define YAZE_SDL_WINDOW_RESIZED SDL_EVENT_WINDOW_RESIZED
#define YAZE_SDL_WINDOW_SIZE_CHANGED SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
#define YAZE_SDL_WINDOW_MINIMIZED SDL_EVENT_WINDOW_MINIMIZED
#define YAZE_SDL_WINDOW_MAXIMIZED SDL_EVENT_WINDOW_MAXIMIZED
#define YAZE_SDL_WINDOW_RESTORED SDL_EVENT_WINDOW_RESTORED
#define YAZE_SDL_WINDOW_SHOWN SDL_EVENT_WINDOW_SHOWN
#define YAZE_SDL_WINDOW_HIDDEN SDL_EVENT_WINDOW_HIDDEN
#define YAZE_SDL_WINDOW_EXPOSED SDL_EVENT_WINDOW_EXPOSED
#define YAZE_SDL_WINDOW_FOCUS_GAINED SDL_EVENT_WINDOW_FOCUS_GAINED
#define YAZE_SDL_WINDOW_FOCUS_LOST SDL_EVENT_WINDOW_FOCUS_LOST

// SDL3 has no nested window events
#define YAZE_SDL_HAS_INDIVIDUAL_WINDOW_EVENTS 1

#else  // SDL2

// SDL2 event types
#define YAZE_SDL_QUIT SDL_QUIT
#define YAZE_SDL_WINDOWEVENT SDL_WINDOWEVENT

// SDL2 window events are nested under SDL_WINDOWEVENT
#define YAZE_SDL_WINDOW_CLOSE SDL_WINDOWEVENT_CLOSE
#define YAZE_SDL_WINDOW_RESIZED SDL_WINDOWEVENT_RESIZED
#define YAZE_SDL_WINDOW_SIZE_CHANGED SDL_WINDOWEVENT_SIZE_CHANGED
#define YAZE_SDL_WINDOW_MINIMIZED SDL_WINDOWEVENT_MINIMIZED
#define YAZE_SDL_WINDOW_MAXIMIZED SDL_WINDOWEVENT_MAXIMIZED
#define YAZE_SDL_WINDOW_RESTORED SDL_WINDOWEVENT_RESTORED
#define YAZE_SDL_WINDOW_SHOWN SDL_WINDOWEVENT_SHOWN
#define YAZE_SDL_WINDOW_HIDDEN SDL_WINDOWEVENT_HIDDEN
#define YAZE_SDL_WINDOW_EXPOSED SDL_WINDOWEVENT_EXPOSED
#define YAZE_SDL_WINDOW_FOCUS_GAINED SDL_WINDOWEVENT_FOCUS_GAINED
#define YAZE_SDL_WINDOW_FOCUS_LOST SDL_WINDOWEVENT_FOCUS_LOST

// SDL2 uses nested window events
#define YAZE_SDL_HAS_INDIVIDUAL_WINDOW_EVENTS 0

#endif  // YAZE_USE_SDL3

// ============================================================================
// Window Event Helper Functions
// ============================================================================

/**
 * @brief Check if an event is a window close event
 * Works correctly for both SDL2 (nested) and SDL3 (individual) events
 */
inline bool IsWindowCloseEvent(const SDL_Event& event) {
#ifdef YAZE_USE_SDL3
  return event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED;
#else
  return event.type == SDL_WINDOWEVENT &&
         event.window.event == SDL_WINDOWEVENT_CLOSE;
#endif
}

/**
 * @brief Check if an event is a window resize event
 */
inline bool IsWindowResizeEvent(const SDL_Event& event) {
#ifdef YAZE_USE_SDL3
  return event.type == SDL_EVENT_WINDOW_RESIZED ||
         event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
#else
  return event.type == SDL_WINDOWEVENT &&
         (event.window.event == SDL_WINDOWEVENT_RESIZED ||
          event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED);
#endif
}

/**
 * @brief Check if an event is a window minimize event
 */
inline bool IsWindowMinimizedEvent(const SDL_Event& event) {
#ifdef YAZE_USE_SDL3
  return event.type == SDL_EVENT_WINDOW_MINIMIZED;
#else
  return event.type == SDL_WINDOWEVENT &&
         event.window.event == SDL_WINDOWEVENT_MINIMIZED;
#endif
}

/**
 * @brief Check if an event is a window restore event
 */
inline bool IsWindowRestoredEvent(const SDL_Event& event) {
#ifdef YAZE_USE_SDL3
  return event.type == SDL_EVENT_WINDOW_RESTORED;
#else
  return event.type == SDL_WINDOWEVENT &&
         event.window.event == SDL_WINDOWEVENT_RESTORED;
#endif
}

/**
 * @brief Get window width from resize event data
 */
inline int GetWindowEventWidth(const SDL_Event& event) {
  return event.window.data1;
}

/**
 * @brief Get window height from resize event data
 */
inline int GetWindowEventHeight(const SDL_Event& event) {
  return event.window.data2;
}

// ============================================================================
// Initialization Helpers
// ============================================================================

/**
 * @brief Check if SDL initialization succeeded.
 *
 * SDL2: Returns 0 on success
 * SDL3: Returns true (non-zero) on success
 */
inline bool InitSucceeded(int result) {
#ifdef YAZE_USE_SDL3
  // SDL3 returns bool (non-zero for success)
  return result != 0;
#else
  // SDL2 returns 0 for success
  return result == 0;
#endif
}

/**
 * @brief Get recommended init flags.
 *
 * SDL3 removed SDL_INIT_TIMER (timer is always available).
 */
inline uint32_t GetDefaultInitFlags() {
#ifdef YAZE_USE_SDL3
  return SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS;
#else
  return SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS;
#endif
}

}  // namespace platform
}  // namespace yaze

#endif  // YAZE_APP_PLATFORM_SDL_COMPAT_H_
