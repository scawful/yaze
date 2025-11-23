# Initiative: YAZE v0.4.0 - SDL3 Modernization & Emulator Accuracy

**Created**: 2025-11-23
**Owner**: Multi-agent coordination
**Status**: ACTIVE
**Target Release**: Q1 2026

---

## Executive Summary

YAZE v0.4.0 represents a major release focusing on two pillars:
1. **Emulator Accuracy** - Implementing cycle-accurate PPU rendering and AI integration
2. **SDL3 Modernization** - Migrating from SDL2 to SDL3 with backend abstractions

This initiative coordinates 7 specialized agents across 5 parallel workstreams.

---

## Background

### Current State (v0.3.8-hotfix1)
- AI agent infrastructure complete (z3ed CLI)
- Card-based UI system functional
- Emulator debugging framework established
- CI/CD pipeline stabilized with nightly testing
- Known issues: Tile16 palette, overworld sprite movement, emulator audio

### Uncommitted Work Ready for Integration
- PPU JIT catch-up system (`ppu.cc` - 29 lines added)
- Dungeon room sprite encoding/saving (`room.cc` - 82 lines added)
- Dungeon editor system improvements (133 lines added)
- Test suite configuration updates

---

## Milestones

### Milestone 1: Emulator Accuracy (Weeks 1-6)

#### 1.1 PPU JIT Catch-up Completion
**Agent**: `snes-emulator-expert`
**Status**: IN_PROGRESS (uncommitted work exists)
**Files**: `src/app/emu/video/ppu.cc`, `src/app/emu/video/ppu.h`

**Tasks**:
- [x] Add `last_rendered_x_` tracking
- [x] Implement `StartLine()` method
- [x] Implement `CatchUp(h_pos)` method
- [ ] Integrate `CatchUp()` calls into `Snes::WriteBBus`
- [ ] Add unit tests for mid-scanline register writes
- [ ] Verify with raster-effect test ROMs

**Success Criteria**: Games with H-IRQ effects (Tales of Phantasia, Star Ocean) render correctly

#### 1.2 Semantic Inspection API
**Agent**: `ai-infra-architect`
**Status**: PLANNED
**Files**: New `src/app/emu/debug/semantic_introspection.h/cc`

**Tasks**:
- [ ] Create `SemanticIntrospectionEngine` class
- [ ] Connect to `Memory` and `SymbolProvider`
- [ ] Implement `GetPlayerState()` using ALTTP RAM offsets
- [ ] Implement `GetSpriteState()` for sprite tracking
- [ ] Add JSON export for AI consumption
- [ ] Create debug overlay rendering for vision models

**Success Criteria**: AI agents can query game state semantically via JSON API

#### 1.3 State Injection API
**Agent**: `snes-emulator-expert`
**Status**: PLANNED
**Files**: `src/app/emu/emulator.h/cc`, new `src/app/emu/state_patch.h`

**Tasks**:
- [ ] Define `GameStatePatch` structure
- [ ] Implement `Emulator::InjectState(patch)`
- [ ] Add fast-boot capability (skip intro sequences)
- [ ] Create ALTTP-specific presets (Dungeon Test, Overworld Test)
- [ ] Integrate with z3ed CLI for "test sprite" workflow

**Success Criteria**: Editors can teleport emulator to any game state programmatically

#### 1.4 Audio System Fix
**Agent**: `snes-emulator-expert`
**Status**: PLANNED
**Files**: `src/app/emu/audio/`, `src/app/emu/apu/`

**Tasks**:
- [ ] Diagnose SDL2 audio device initialization
- [ ] Fix SPC700 → SDL2 format conversion
- [ ] Verify APU handshake timing
- [ ] Add audio debugging tools to UI
- [ ] Test with music playback in ALTTP

**Success Criteria**: Audio plays correctly during emulation

---

### Milestone 2: SDL3 Migration (Weeks 3-8)

#### 2.1 Directory Restructure
**Agent**: `backend-infra-engineer`
**Status**: PLANNED
**Scope**: Move `src/lib/` + `third_party/` → `external/`

**Tasks**:
- [ ] Create `external/` directory structure
- [ ] Move SDL2 (to be replaced), imgui, etc.
- [ ] Update CMakeLists.txt references
- [ ] Update submodule paths
- [ ] Validate builds on all platforms

#### 2.2 SDL3 Core Integration
**Agent**: `imgui-frontend-engineer`
**Status**: PLANNED
**Files**: `src/app/platform/`, `CMakeLists.txt`

**Tasks**:
- [ ] Add SDL3 as dependency
- [ ] Create `GraphicsBackend` abstraction interface
- [ ] Implement SDL3 backend for window/rendering
- [ ] Update ImGui to SDL3 backend
- [ ] Port window creation and event handling

#### 2.3 SDL3 Audio Backend
**Agent**: `snes-emulator-expert`
**Status**: PLANNED (after audio fix)
**Files**: `src/app/emu/audio/sdl3_audio_backend.h/cc`

**Tasks**:
- [ ] Implement `IAudioBackend` for SDL3
- [ ] Migrate audio initialization code
- [ ] Verify audio quality matches SDL2

#### 2.4 SDL3 Input Backend
**Agent**: `imgui-frontend-engineer`
**Status**: PLANNED
**Files**: `src/app/emu/ui/input_handler.cc`

**Tasks**:
- [ ] Implement SDL3 input backend
- [ ] Add gamepad support improvements
- [ ] Verify continuous key polling works

---

### Milestone 3: Editor Fixes (Weeks 2-4)

#### 3.1 Tile16 Palette System Fix
**Agent**: `zelda3-hacking-expert`
**Status**: PLANNED
**Files**: `src/app/editor/graphics/tile16_editor.cc`

**Tasks**:
- [ ] Fix Tile8 source canvas palette application
- [ ] Fix palette button 0-7 switching logic
- [ ] Ensure color alignment across canvases
- [ ] Add unit tests for palette operations

**Success Criteria**: Tile editing workflow fully functional

#### 3.2 Overworld Sprite Movement
**Agent**: `zelda3-hacking-expert`
**Status**: PLANNED
**Files**: `src/app/editor/overworld/overworld_editor.cc`

**Tasks**:
- [ ] Debug canvas interaction system
- [ ] Fix drag operation handling for sprites
- [ ] Test sprite placement workflow

**Success Criteria**: Sprites respond to drag operations

#### 3.3 Dungeon Sprite Save Integration
**Agent**: `zelda3-hacking-expert`
**Status**: IN_PROGRESS (uncommitted)
**Files**: `src/zelda3/dungeon/room.cc/h`

**Tasks**:
- [x] Implement `EncodeSprites()` method
- [x] Implement `SaveSprites()` method
- [ ] Integrate with dungeon editor UI
- [ ] Add unit tests
- [ ] Commit and verify CI

---

## Agent Assignments

| Agent | Primary Responsibilities | Workstream |
|-------|-------------------------|------------|
| `snes-emulator-expert` | PPU catch-up, audio fix, state injection, SDL3 audio | Stream 1 |
| `imgui-frontend-engineer` | SDL3 core, SDL3 input, UI updates | Stream 2 |
| `zelda3-hacking-expert` | Tile16 fix, sprite movement, dungeon save | Stream 3 |
| `ai-infra-architect` | Semantic API, multimodal context | Stream 4 |
| `backend-infra-engineer` | Directory restructure, CI updates | Stream 2 |
| `test-infrastructure-expert` | Test suite for new features | Support |
| `docs-janitor` | Documentation updates | Support |

---

## Parallel Workstreams

```
Week 1-2:
├── Stream 1: snes-emulator-expert → Complete PPU catch-up
├── Stream 3: zelda3-hacking-expert → Tile16 palette fix
└── Stream 4: ai-infra-architect → Semantic API design

Week 3-4:
├── Stream 1: snes-emulator-expert → Audio system fix
├── Stream 2: backend-infra-engineer → Directory restructure
├── Stream 3: zelda3-hacking-expert → Sprite movement fix
└── Stream 4: ai-infra-architect → Semantic API implementation

Week 5-6:
├── Stream 1: snes-emulator-expert → State injection API
├── Stream 2: imgui-frontend-engineer → SDL3 core integration
└── Stream 3: zelda3-hacking-expert → Dungeon sprite integration

Week 7-8:
├── Stream 1: snes-emulator-expert → SDL3 audio backend
├── Stream 2: imgui-frontend-engineer → SDL3 input backend
└── All: Integration testing and stabilization
```

---

## Success Criteria

### v0.4.0 Release Readiness
- [ ] PPU catch-up renders raster effects correctly
- [ ] Semantic API provides structured game state
- [ ] State injection enables "test sprite" workflow
- [ ] Audio system functional
- [ ] SDL3 builds pass on Windows, macOS, Linux
- [ ] No performance regression vs v0.3.x
- [ ] All known editor bugs fixed
- [ ] Documentation updated for new APIs

---

## Risk Mitigation

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| SDL3 breaking changes | Medium | High | Maintain SDL2 fallback branch |
| Audio system complexity | High | Medium | Prioritize diagnosis before migration |
| Cross-platform issues | Medium | Medium | CI validation on all platforms |
| Agent coordination conflicts | Low | Medium | Strict coordination board protocol |

---

## Communication

- **Daily**: Coordination board updates
- **Weekly**: Progress sync via initiative status
- **Blockers**: Post `BLOCKER` tag on coordination board immediately
- **Handoffs**: Use `REQUEST →` format for task transitions

---

## References

- [Emulator Accuracy Report](emulator_accuracy_report.md)
- [Roadmap](../roadmaps/roadmap.md)
- [Feature Parity Analysis](../roadmaps/feature-parity-analysis.md)
- [Code Review Next Steps](../roadmaps/code-review-critical-next-steps.md)
- [Coordination Board](coordination-board.md)
