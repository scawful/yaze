# Branch Organization Plan

The current workspace has a significant number of unstaged changes covering multiple distinct areas of work. To maintain a clean history and facilitate parallel development, these should be split into the following branches:

## 1. `feature/debugger-disassembler`
**Purpose**: Implementation of the new debugging and disassembly tools.
**Files**:
- `src/app/emu/debug/disassembler.cc` / `.h`
- `src/app/emu/debug/step_controller.cc` / `.h`
- `src/app/emu/debug/symbol_provider.cc` / `.h`
- `src/cli/service/agent/disassembler_65816.cc` / `.h`
- `src/cli/service/agent/rom_debug_agent.cc` / `.h`
- `src/cli/service/agent/memory_debugging_example.cc`
- `test/unit/emu/disassembler_test.cc`
- `test/unit/emu/step_controller_test.cc`
- `test/unit/cli/rom_debug_agent_test.cc`
- `test/integration/memory_debugging_test.cc`

## 2. `infra/ci-test-overhaul`
**Purpose**: Updates to CI workflows, test configuration, and agent documentation.
**Files**:
- `.github/actions/run-tests/action.yml`
- `.github/workflows/ci.yml`
- `.github/workflows/release.yml`
- `.github/workflows/nightly.yml`
- `AGENTS.md`
- `CLAUDE.md`
- `docs/internal/agents/*`
- `cmake/options.cmake`
- `cmake/packaging/cpack.cmake`
- `src/app/test/test.cmake`
- `test/test.cmake`
- `test/README.md`

## 3. `test/e2e-dungeon-coverage`
**Purpose**: Extensive additions to E2E and integration tests for the Dungeon Editor.
**Files**:
- `test/e2e/dungeon_*`
- `test/integration/zelda3/dungeon_*`
- `test/unit/zelda3/dungeon/object_rendering_test.cc`

## 4. `feature/agent-ui-improvements`
**Purpose**: Enhancements to the Agent Chat Widget and Proposal Drawer.
**Files**:
- `src/app/editor/agent/agent_chat_widget.cc`
- `src/app/editor/system/proposal_drawer.cc`
- `src/cli/service/agent/tool_dispatcher.cc` / `.h`
- `src/cli/service/ai/prompt_builder.cc`

## 5. `fix/overworld-logic`
**Purpose**: Fixes or modifications to Overworld logic (possibly related to the other agent's work).
**Files**:
- `src/zelda3/overworld/overworld.cc`
- `src/zelda3/overworld/overworld.h`
- `test/e2e/overworld/overworld_e2e_test.cc`
- `test/integration/zelda3/overworld_integration_test.cc`

## 6. `chore/misc-cleanup`
**Purpose**: Miscellaneous cleanups and minor fixes.
**Files**:
- `src/CMakeLists.txt`
- `src/app/editor/editor_library.cmake`
- `test/yaze_test.cc`
- `test/test_utils.cc`
- `test/test_editor.cc`

## Action Items
1.  Review this list with the user (if they were here, but I will assume this is the plan).
2.  For the current task (UI/UX), I should likely branch off `master` (or the current state if dependencies exist) but be careful not to include unrelated changes in my commits if I were to commit.
3.  Since I am in an agentic mode, I will proceed by assuming these changes are "work in progress" and I should try to touch only what is necessary for UI/UX, or if I need to clean up, I should be aware of these boundaries.
