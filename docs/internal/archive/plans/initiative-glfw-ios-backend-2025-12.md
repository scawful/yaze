# Initiative: GLFW Backend + iOS Rebuild + Lab Orchestration

Status: DRAFT  
Owner: imgui-frontend-engineer  
Created: 2025-12-23  
Last Reviewed: 2025-12-23  
Next Review: 2026-01-06  
Validation/Exit Criteria:
- GLFW backend can run the editor shell with ImGui viewports enabled and stable.
- iOS app boots, loads a ROM via native document picker, and renders the editor UI.
- Lab target supports backend selection and layout import/export for rapid UX iteration.
- Editor orchestration is separated from editor implementations and reusable across desktop/iOS/lab.

## Goals
- Add a GLFW + OpenGL backend option to unlock ImGui viewports for experimentation.
- Rebuild the iOS host app with native iOS integrations and the ImGui editor system.
- Use the lab target as the sandbox for layout and orchestration experiments.
- Decouple UX orchestration from editor logic for fine-grained control.

## Scope
- Window backend abstraction updates needed for non-SDL windows.
- OpenGL renderer path that satisfies `IRenderer` requirements (textures + blits).
- iOS host refactor (metal + imgui backend, document picker, app lifecycle hooks).
- Lab target improvements (backend selection, layout import/export, viewport stress tests).

## Non-Goals
- Full emulator input parity on GLFW (defer to follow-up once input backend is defined).
- Complete SDL removal (GLFW remains optional).
- Shipping viewports as the default on desktop.

## Phased Plan
### Phase 1: Backend Abstraction and GLFW Entry Point
- Define a backend-neutral native window handle and remove SDL-only assumptions in the renderer init path.
- Introduce `WindowBackendType::GLFW` and `RendererBackendType::OpenGL`.
- Add `GLFWWindowBackend` using `imgui_impl_glfw` + `imgui_impl_opengl3`.
- Add OpenGL renderer that can handle `CreateTexture`, `UpdateTexture`, and `RenderCopy`.

### Phase 2: Lab Target Expansion
- Add backend selection flags to the lab target for quick viewport testing.
- Add layout import/export round-trip and preset management.
- Add viewport stress scenes (multi-dock + texture-heavy panels).

### Phase 3: iOS App Rebuild
- New iOS host app with Metal-backed ImGui renderer and full app lifecycle support.
- Native integrations: document picker, share sheet, background/foreground safe handling.
- Mobile layout presets and a touch-first panel navigation surface.

### Phase 4: Orchestration Decoupling
- Extract `UIOrchestrator` that owns dockspace/menu/panel visibility.
- Keep editors focused on data/model and panel registration.
- Expose per-panel visibility and orchestration state for fine-grained control.

## Risks / Constraints
- `IRenderer` currently assumes SDL types; refactor must avoid breaking existing SDL2/SDL3 paths.
- OpenGL renderer must handle palette-indexed textures sourced from SDL surfaces.
- iOS input and file access require security-scoped bookmarks and careful threading.

## Dependencies
- GLFW + OpenGL toolchain availability on macOS.
- ImGui OpenGL backend support (no GL loader assumed, use platform headers).
- iOS signing and entitlements for file access.

## Milestones
- M1: GLFW backend runs lab target with viewports enabled.
- M2: OpenGL renderer draws tilemaps and editor textures.
- M3: iOS app boots with ROM load and core UI flow.
- M4: Orchestration decoupling merged and used by lab/iOS.
