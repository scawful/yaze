# Claude Test Handoff Document

**Date**: 2024-11-22
**Prepared by**: Claude (Sonnet 4.5)
**Previous agents**: Gemini 3, Claude 4.5 (build fixes)
**Status**: Build passing, ready for testing

## TL;DR

All 6 feature branches from Gemini3's work have been merged to master and build issues are resolved. The codebase needs comprehensive testing across multiple areas: Overworld fixes, Dungeon E2E tests, Agent UI improvements, CI infrastructure, and debugger/disassembler features.

## Current State

```
Commit: ed980625d7 fix: resolve build errors from Gemini3 handoff
Branch: master (13 commits ahead of origin)
Build: PASSING (mac-dbg preset)
```

### Merged Branches (in order)
1. `infra/ci-test-overhaul` - CI/CD and test infrastructure
2. `test/e2e-dungeon-coverage` - Dungeon editor E2E tests
3. `feature/agent-ui-improvements` - Agent UI and dev tools
4. `fix/overworld-logic` - Overworld test fixes
5. `chore/misc-cleanup` - Documentation and cleanup
6. `feature/debugger-disassembler` - Debugger and disassembler support

### Build Fixes Applied
- Added `memory_inspector_tool.cc` to `agent.cmake` (was missing, caused vtable linker errors)
- Fixed API mismatches in `memory_inspector_tool.cc`:
  - `GetArg()` → `GetString().value_or()`
  - `OutputMap()`/`OutputTable()` → `BeginObject()`/`AddField()`/`EndObject()` pattern

---

## Testing Areas

### 1. Overworld Fixes (`fix/overworld-logic`)

**Files Changed**:
- `test/integration/zelda3/overworld_integration_test.cc`
- `test/unit/zelda3/overworld_test.cc`

**Test Commands**:
```bash
# Run overworld tests
ctest --test-dir build -R "overworld" --output-on-failure

# Specific test binaries
./build/bin/Debug/yaze_test_stable --gtest_filter="*Overworld*"
```

**What to Verify**:
- [ ] Overworld unit tests pass
- [ ] Overworld integration tests pass
- [ ] No regressions in overworld map loading
- [ ] Multi-area map configuration works correctly

**Manual Testing**:
```bash
./build/bin/Debug/yaze.app/Contents/MacOS/yaze --rom_file=<path_to_rom> --editor=Overworld
```
- Load a ROM and verify overworld renders correctly
- Test map switching between Light World, Dark World, Special areas
- Verify entity visibility (entrances, exits, items, sprites)

---

### 2. Dungeon E2E Tests (`test/e2e-dungeon-coverage`)

**New Test Files**:
| File | Purpose |
|------|---------|
| `dungeon_canvas_interaction_test.cc/.h` | Canvas click/drag tests |
| `dungeon_e2e_tests.cc/.h` | Full workflow E2E tests |
| `dungeon_layer_rendering_test.cc/.h` | Layer visibility tests |
| `dungeon_object_drawing_test.cc/.h` | Object rendering tests |
| `dungeon_visual_verification_test.cc/.h` | Visual regression tests |

**Test Commands**:
```bash
# Run all dungeon tests
ctest --test-dir build -R "dungeon" -L stable --output-on-failure

# Run dungeon E2E specifically
ctest --test-dir build -R "dungeon_e2e" --output-on-failure

# GUI tests (requires display)
./build/bin/Debug/yaze_test_gui --gtest_filter="*Dungeon*"
```

**What to Verify**:
- [ ] All new dungeon test files compile
- [ ] Canvas interaction tests pass
- [ ] Layer rendering tests pass
- [ ] Object drawing tests pass
- [ ] Visual verification tests pass (may need baseline images)
- [ ] Integration tests with ROM pass (if ROM available)

**Manual Testing**:
```bash
./build/bin/Debug/yaze.app/Contents/MacOS/yaze --rom_file=<path_to_rom> --editor=Dungeon
```
- Load dungeon rooms and verify rendering
- Test object selection and manipulation
- Verify layer toggling works
- Check room switching

---

### 3. Agent UI Improvements (`feature/agent-ui-improvements`)

**Key Files**:
| File | Purpose |
|------|---------|
| `src/cli/service/agent/dev_assist_agent.cc/.h` | Dev assistance agent |
| `src/cli/service/agent/tools/build_tool.cc/.h` | Build system integration |
| `src/cli/service/agent/tools/filesystem_tool.cc/.h` | File operations |
| `src/cli/service/agent/tools/memory_inspector_tool.cc/.h` | Memory debugging |
| `src/app/editor/agent/agent_editor.cc` | Agent editor UI |

**Test Commands**:
```bash
# Run agent-related tests
ctest --test-dir build -R "tool_dispatcher" --output-on-failure
ctest --test-dir build -R "agent" --output-on-failure

# Test z3ed CLI
./build/bin/Debug/z3ed --help
./build/bin/Debug/z3ed memory regions
./build/bin/Debug/z3ed memory analyze 0x7E0000
```

**What to Verify**:
- [ ] Tool dispatcher tests pass
- [ ] Memory inspector tools work:
  - `memory regions` - lists known ALTTP memory regions
  - `memory analyze <addr>` - analyzes memory at address
  - `memory search <pattern>` - searches for patterns
  - `memory compare <addr>` - compares memory values
  - `memory check [region]` - checks for anomalies
- [ ] Build tool integration works
- [ ] Filesystem tool operations work
- [ ] Agent editor UI renders correctly

**Manual Testing**:
```bash
./build/bin/Debug/yaze.app/Contents/MacOS/yaze --rom_file=<path_to_rom> --editor=Agent
```
- Open Agent editor and verify chat UI
- Test proposal drawer functionality
- Verify theme colors are consistent (no hardcoded colors)

---

### 4. CI Infrastructure (`infra/ci-test-overhaul`)

**Key Files**:
- `.github/workflows/ci.yml` - Main CI workflow
- `.github/workflows/release.yml` - Release workflow
- `.github/workflows/nightly.yml` - Nightly builds (NEW)
- `test/test.cmake` - Test configuration
- `test/README.md` - Test documentation

**What to Verify**:
- [ ] CI workflow YAML is valid:
  ```bash
  python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))"
  python3 -c "import yaml; yaml.safe_load(open('.github/workflows/nightly.yml'))"
  ```
- [ ] Test labels work correctly:
  ```bash
  ctest --test-dir build -L stable -N  # List stable tests
  ctest --test-dir build -L unit -N    # List unit tests
  ```
- [ ] Documentation is accurate in `test/README.md`

---

### 5. Debugger/Disassembler (`feature/debugger-disassembler`)

**Key Files**:
- `src/cli/service/agent/disassembler_65816.cc`
- `src/cli/service/agent/rom_debug_agent.cc`
- `src/app/emu/debug/` - Emulator debug components

**Test Commands**:
```bash
# Test disassembler
ctest --test-dir build -R "disassembler" --output-on-failure

# Manual disassembly test (requires ROM)
./build/bin/Debug/z3ed disasm <rom_file> 0x008000 20
```

**What to Verify**:
- [ ] 65816 disassembler produces correct output
- [ ] ROM debug agent works
- [ ] Emulator stepping (if integrated)

---

## Quick Test Matrix

| Area | Unit Tests | Integration | E2E/GUI | Manual |
|------|------------|-------------|---------|--------|
| Overworld | `overworld_test` | `overworld_integration_test` | - | Open editor |
| Dungeon | `object_rendering_test` | `dungeon_room_test` | `dungeon_e2e_tests` | Open editor |
| Agent Tools | `tool_dispatcher_test` | - | - | z3ed CLI |
| Memory Inspector | - | - | - | z3ed memory * |
| Disassembler | `disassembler_test` | - | - | z3ed disasm |

---

## Full Test Suite

```bash
# Quick smoke test (stable only, ~2 min)
ctest --test-dir build -L stable -j4 --output-on-failure

# All tests (may take longer)
ctest --test-dir build --output-on-failure

# ROM-dependent tests (if ROM available)
cmake --preset mac-dbg -DYAZE_ENABLE_ROM_TESTS=ON -DYAZE_TEST_ROM_PATH=~/zelda3.sfc
ctest --test-dir build -L rom_dependent --output-on-failure
```

---

## Known Issues / Watch For

1. **Linker warnings**: You may see warnings about duplicate libraries during linking - these are benign warnings, not errors.

2. **third_party/ directory**: There's an untracked `third_party/` directory that needs a decision:
   - Should it be added to `.gitignore`?
   - Should it be a git submodule?
   - For now, leave it untracked.

3. **Visual tests**: Some dungeon visual verification tests may fail if baseline images don't exist yet. These tests should generate baselines on first run.

4. **GUI tests**: Tests with `-L gui` require a display. On headless CI, these may need to be skipped or run with Xvfb.

5. **gRPC tests**: Some agent features require gRPC. Ensure `YAZE_ENABLE_REMOTE_AUTOMATION` is ON if testing those.

---

## Recommended Test Order

1. **First**: Run stable unit tests to catch obvious issues
   ```bash
   ctest --test-dir build -L stable -L unit -j4
   ```

2. **Second**: Run integration tests
   ```bash
   ctest --test-dir build -L stable -L integration -j4
   ```

3. **Third**: Manual testing of key editors (Overworld, Dungeon, Agent)

4. **Fourth**: E2E/GUI tests if display available
   ```bash
   ./build/bin/Debug/yaze_test_gui
   ```

5. **Fifth**: Full test suite
   ```bash
   ctest --test-dir build --output-on-failure
   ```

---

## Build Commands Reference

```bash
# Configure (if needed)
cmake --preset mac-dbg

# Build main executable
cmake --build build --target yaze -j4

# Build all (including tests)
cmake --build build -j4

# Build specific test binary
cmake --build build --target yaze_test_stable -j4
cmake --build build --target yaze_test_gui -j4
```

---

## Files Modified in Final Fix Commit

For reference, these files were modified in `ed980625d7`:

| File | Change |
|------|--------|
| `assets/asm/usdasm` | Submodule pointer update |
| `src/app/editor/agent/agent_editor.cc` | Gemini3's UI changes |
| `src/app/gui/style/theme.h` | Theme additions |
| `src/cli/agent.cmake` | Added memory_inspector_tool.cc |
| `src/cli/service/agent/emulator_service_impl.h` | Gemini3's changes |
| `src/cli/service/agent/rom_debug_agent.cc` | Gemini3's changes |
| `src/cli/service/agent/tools/memory_inspector_tool.cc` | API fixes |
| `src/protos/emulator_service.proto` | Gemini3's proto changes |

---

## Success Criteria

Testing is complete when:
- [ ] All stable tests pass (`ctest -L stable`)
- [ ] No regressions in Overworld editor
- [ ] No regressions in Dungeon editor
- [ ] Agent tools respond correctly to CLI commands
- [ ] Build succeeds on all platforms (via CI)

---

## Contact / Escalation

If you encounter issues:
1. Check `docs/BUILD-TROUBLESHOOTING.md` for common fixes
2. Review `GEMINI3_HANDOFF.md` for context on the original work
3. Check git log for related commits: `git log --oneline -20`

Good luck with testing!
