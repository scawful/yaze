# YAZE v0.4.0 Roadmap: Editor Stability & OOS Support

**Version:** 2.0.0
**Date:** November 23, 2025
**Status:** Active
**Current:** v0.3.9 release fix in progress
**Next:** v0.4.0 (first feature release after CI/CD stabilization)

---

## Context

**v0.3.3-v0.3.9** was consumed by CI/CD and release workflow fixes (~90% of development time). v0.4.0 marks the return to feature development, focusing on unblocking Oracle of Secrets (OOS) workflows.

---

## Priority Tiers

### Tier 1: Critical (Blocks OOS Development)

#### 1. Dungeon Editor - Full Workflow Verification

**Problem:** `SaveDungeon()` is a **STUB** that returns OkStatus() without saving anything.

| Task | File | Status |
|------|------|--------|
| Implement `SaveDungeon()` | `src/zelda3/dungeon/dungeon_editor_system.cc:44-50` | STUB |
| Implement `SaveRoom()` properly | `src/zelda3/dungeon/dungeon_editor_system.cc:905-934` | Partial |
| Free space validation | `src/zelda3/dungeon/room.cc:957` | Missing |
| Load room from ROM | `src/zelda3/dungeon/dungeon_editor_system.cc:86` | TODO |
| Room create/delete/duplicate | `src/zelda3/dungeon/dungeon_editor_system.cc:92-105` | TODO |
| Undo/redo system | `src/app/editor/dungeon/dungeon_editor_v2.h:60-62` | Stubbed |
| Validation system | `src/zelda3/dungeon/dungeon_editor_system.cc:683-717` | TODO |

**Success Criteria:** Load dungeon room → Edit objects/sprites/tiles → Save to ROM → Test in emulator → Changes persist

---

#### 2. Message Editor - Expanded Messages Support

**Problem:** OOS uses expanded dialogue. BIN file saving is broken, no JSON export for version control.

| Task | File | Priority |
|------|------|----------|
| Fix BIN file saving | `src/app/editor/message/message_editor.cc:497-508` | P0 |
| Add file save dialog for expanded | `src/app/editor/message/message_editor.cc:317-352` | P0 |
| JSON export/import | `src/app/editor/message/message_data.h/cc` (new) | P0 |
| Visual vanilla vs expanded separation | `src/app/editor/message/message_editor.cc:205-252` | P1 |
| Address management for new messages | `src/app/editor/message/message_data.cc:435-451` | P1 |
| Complete search & replace | `src/app/editor/message/message_editor.cc:574-600` (TODO line 590) | P2 |

**Existing Plans:** `docs/internal/plans/message_editor_implementation_roadmap.md` (773 lines)

**Success Criteria:** Load ROM → Edit expanded messages → Export BIN file → Import JSON → Changes work in OOS

---

#### 3. ZSCOW Full Audit

**Problem:** Code lifted from ZScream, works for most cases but needs audit for vanilla + OOS workflows.

| Component | Status | File | Action |
|-----------|--------|------|--------|
| Version detection | Working (0x00 edge case) | `src/zelda3/overworld/overworld_version_helper.h:51-71` | Clarify 0x00 |
| Area sizing | Working | `src/zelda3/overworld/overworld.cc:267-422` | Integration tests |
| Map rendering | Working (2 TODOs) | `src/zelda3/overworld/overworld_map.cc:590,422` | Death Mountain GFX |
| Custom palettes v3 | Working | `src/zelda3/overworld/overworld_map.cc:806-826` | UI feedback |

**Success Criteria:** Vanilla ROM, ZSCustom v1/v2/v3 ROMs all load and render correctly with full feature support

---

### Tier 2: High Priority (Development Quality)

#### 4. Testing Infrastructure

**Goal:** E2E GUI tests + ROM validation for automated verification

| Test Suite | Description | Status |
|------------|-------------|--------|
| Dungeon E2E workflow | Load → Edit → Save → Validate ROM | New |
| Message editor E2E | Load → Edit expanded → Export BIN | New |
| ROM validation suite | Verify saved ROMs boot in emulator | New |
| ZSCOW regression tests | Test vanilla, v1, v2, v3 ROMs | Partial |

**Framework:** ImGui Test Engine (`test/e2e/`)

---

#### 5. AI Integration - Agent Inspection

**Goal:** AI agents can query ROM/editor state with real data (not stubs)

| Task | File | Status |
|------|------|--------|
| Semantic Inspection API | `src/app/emu/debug/semantic_introspection.cc` | Complete |
| FileSystemTool | `src/cli/service/agent/tools/filesystem_tool.cc` | Complete |
| Overworld inspection | `src/cli/handlers/game/overworld_commands.cc:10-97` | Stub outputs |
| Dungeon inspection | `src/cli/handlers/game/dungeon_commands.cc` | Stub outputs |

**Success Criteria:** `z3ed overworld describe-map 0` returns real map data, not placeholder text

---

### Tier 3: Medium Priority (Polish)

#### 6. Editor Bug Fixes

| Issue | File | Status |
|-------|------|--------|
| Tile16 palette | `src/app/editor/overworld/tile16_editor.cc` | Uncommitted fixes |
| Sprite movement | `src/app/editor/overworld/entity.cc` | Uncommitted fixes |
| Entity colors | `src/app/editor/overworld/overworld_entity_renderer.cc:21-32` | Not per CLAUDE.md |
| Item deletion | `src/app/editor/overworld/entity.cc:352` | Hides, doesn't delete |

**CLAUDE.md Standards:**
- Entrances: Yellow-gold, 0.85f alpha
- Exits: Cyan-white (currently white), 0.85f alpha
- Items: Bright red, 0.85f alpha
- Sprites: Bright magenta, 0.85f alpha

---

#### 7. Uncommitted Work Review

Working tree changes need review and commit:

| File | Changes |
|------|---------|
| `tile16_editor.cc` | Texture queueing improvements |
| `entity.cc/h` | Sprite movement fixes, popup bugs |
| `overworld_editor.cc` | Entity rendering changes |
| `overworld_map.cc` | Map rendering updates |
| `object_drawer.cc/h` | Dungeon object drawing |

---

### Tier 4: Lower Priority (Future)

#### 8. Web Port (v0.5.0+)

**Strategy:** `docs/internal/plans/web_port_strategy.md`

- `wasm-release` CMake preset with Emscripten
- `#ifdef __EMSCRIPTEN__` guards (no codebase fork)
- MEMFS `/roms`, IDBFS `/saves`
- HTML shell with Upload/Download ROM
- Opt-in nightly CI job
- Position: "try-in-browser", desktop primary

---

#### 9. SDL3 Migration (v0.5.0)

**Status:** Infrastructure complete (commit a5dc884612)

| Component | Status |
|-----------|--------|
| IRenderer interface | Complete |
| SDL3Renderer | 50% done |
| SDL3 audio backend | Skeleton |
| SDL3 input backend | 80% done |
| CMake presets | Ready (mac-sdl3, win-sdl3, lin-sdl3) |

Defer full migration until editor stability achieved.

---

#### 10. Asar Assembler Restoration (v0.5.0+)

| Task | File | Status |
|------|------|--------|
| `AsarWrapper::Initialize()` | `src/core/asar_wrapper.cc` | Stub |
| `AsarWrapper::ApplyPatch()` | `src/core/asar_wrapper.cc` | Stub |
| Symbol extraction | `src/core/asar_wrapper.cc:103` | Stub |
| CLI command | `src/cli/handlers/tools/` (new) | Not started |

---

## v0.4.0 Success Criteria

- [ ] **Dungeon workflow**: Load → Edit → Save → Test in emulator works
- [ ] **Message editor**: Export/import expanded BIN files for OOS
- [ ] **ZSCOW audit**: All versions load and render correctly
- [ ] **E2E tests**: Automated verification of critical workflows
- [ ] **Agent inspection**: Returns real data, not stubs
- [ ] **Editor fixes**: Uncommitted changes reviewed and committed

---

## Version Timeline

| Version | Focus | Status |
|---------|-------|--------|
| v0.3.9 | Release workflow fix | In progress |
| **v0.4.0** | **Editor Stability & OOS Support** | **Next** |
| v0.5.0 | Web port + SDL3 migration | Future |
| v0.6.0 | Asar restoration + Agent editing | Future |
| v1.0.0 | GA - Documentation, plugins, parity | Future |
