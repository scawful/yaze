# CLAUDE.md

_Extends: ~/AGENTS.md, ~/CLAUDE.md_

C++23 ROM editor for Zelda: A Link to the Past. GUI editor + SNES emulator + AI CLI (`z3ed`).

## Build & Test

```bash
cmake --preset mac-dbg && cmake --build build -j8   # Build
ctest --test-dir build -L stable -j4                 # Test
./yaze --rom_file=zelda3.sfc --editor=Dungeon        # Run
```

Presets: `mac-dbg`/`lin-dbg`/`win-dbg`, `mac-ai`/`win-ai`, `*-rel`. See `docs/public/build/quick-reference.md`.

## Architecture

| Component | Location | Purpose |
|-----------|----------|---------|
| Rom | `src/app/rom.h` | ROM data access, transactions |
| Editors | `src/app/editor/` | Overworld, Dungeon, Graphics, Palette |
| Graphics | `src/app/gfx/` | Bitmap, Arena (async loading), Tiles |
| Zelda3 | `src/zelda3/` | Overworld (160 maps), Dungeon (296 rooms) |
| Canvas | `src/app/gui/canvas.h` | ImGui canvas with pan/zoom |
| CLI | `src/cli/z3ed.cc` | AI-powered ROM hacking tool |

## Key Patterns

**Graphics refresh**: Update model → `Load*()` → `Renderer::Get().RenderBitmap()`

**Async loading**: `Arena::Get().QueueDeferredTexture(bitmap, priority)` + process in `Update()`

**Bitmap sync**: Use `set_data()` for bulk updates (syncs `data_` and `surface_->pixels`)

**Theming**: Always use `AgentUI::GetTheme()`, never hardcoded colors

**Multi-area maps**: Always use `Overworld::ConfigureMultiAreaMap()`, never set `area_size` directly

## Naming

- **Load**: ROM → memory
- **Render**: Data → bitmap (CPU)
- **Draw**: Bitmap → screen (GPU)

## Pitfalls

1. Use `set_data()` not `mutable_data()` assignment for bitmap bulk updates
2. Call `ProcessTextureQueue()` every frame
3. Pass `0x800` to `DecompressV2`, never `0`
4. SMC header: `size % 1MB == 512`, not `size % 32KB`
5. Check `rom_->is_loaded()` before ROM operations

## Code Style

Google C++ Style. Use `absl::Status`/`StatusOr<T>` with `RETURN_IF_ERROR()`/`ASSIGN_OR_RETURN()`.

Format: `cmake --build build --target format`

## Docs

- Architecture: `docs/internal/architecture/`
- Build issues: `docs/BUILD-TROUBLESHOOTING.md`
- Tests: `test/README.md`
