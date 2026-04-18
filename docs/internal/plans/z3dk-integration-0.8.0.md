# z3dk Integration Proposal (target: v0.8.0)

**Status:** Planning draft. Not in 0.7.1 scope.
**Companion project:** `~/src/hobby/z3dk` (z3asm, z3lsp, z3disasm, z3dk_core).

## Motivation

yaze currently handles ROM bytes, UI, and ALTTP data structures. Custom ASM
authoring, assembly, symbol export, and Mesen2 debugging live in the separate
z3dk toolchain. Users round-trip through the shell. Integrating z3dk turns
yaze into a one-stop environment: edit ROM data, write ASM hooks, assemble,
see diagnostics inline, and debug against Mesen2 without leaving the editor.

z3dk is already a C++20 static library (`z3dk-core`) and a standalone LSP
binary, both built with compatible toolchains. The core was shaped with reuse
in mind — `Assembler`, opcode tables, linter, and `SnesKnowledgeBase` are all
library-style APIs.

## Integration surfaces

### 1. Embedded assembler (`z3dk-core` link)

- Link `z3dk-core` as a static dependency (like `zelda3_lib`).
- Expose an `AsmWorkbenchEditor` that accepts a patch path + current ROM bytes
  via `z3dk::Assembler::AssembleOptions` and renders the
  `AssembleResult.diagnostics` into the existing `Notifications` surface.
- Write patched bytes back through the existing ROM transaction/write-fence
  layer — do not let z3dk write the ROM file directly.

Value: ASM patches compile + apply without leaving yaze; diagnostics render
on the same Notifications channel the UI already trusts.

### 2. LSP features inside the editor

Two paths to evaluate:

- **In-process** (preferred): import the subset of z3lsp analysis that
  consumes `z3dk::Assembler` output (labels, defines, source map, hovers) and
  surface it in an ImGui text editor panel. No JSON-RPC overhead.
- **Out-of-process**: launch `z3lsp` as a sidecar, speak LSP over stdio.
  Heavier but keeps analysis decoupled.

Recommended first step: in-process hover / go-to-definition / diagnostics for
a single open patch file. Full workspace symbol index / rename can wait.

### 3. Mesen2 socket unification

- Both projects already speak the same protocol (`PING`, `READ`,
  `SYMBOLS_LOAD`, `BREAKPOINT`, `STEP`, `GAMESTATE`).
- Today: yaze has its own Mesen2 client (`src/cli/commands/mesen2_*`); z3dk has
  `MesenClient` in `src/z3lsp/mesen_client.cc`.
- Consolidate on one client. The yaze client is further along
  (CLI + editor panels), so preferred direction is to keep yaze's and have
  z3dk-in-yaze use it.

### 4. `.mlb` symbol export

- Hook yaze's "Build → Export" menu to `z3dk::SymbolsToMlb`.
- Useful even without the full assembler: any yaze-derived label set (ROM
  tables, user-named pointers) can be shipped to Mesen2.

### 5. Linter integration

- Run `z3dk::RunLint` on imported patches and project-owned ASM files.
- Route `LintResult` diagnostics into the existing validation/status system.
- Respect `prohibited_memory_ranges` per-project (read from `z3dk.toml` next
  to the project file, or yaze's own project settings).

## Non-goals (v0.8.0 cut)

- Full VS Code-style editor inside yaze. A usable text area with diagnostics
  + hover is enough.
- Multi-architecture (SPC700, SuperFX) support. 65816 only.
- z3disasm integration — pure read-side, lower priority.

## Risks / unknowns

- Build footprint: z3dk pulls in Asar-derived sources. Needs a size/time
  audit before linking as default.
- Include path ownership: `z3dk_core` uses `"src/..."` relative includes that
  may collide with yaze's include layout; a light namespace/install target
  refactor on the z3dk side may be required.
- License: z3asm inherits Asar's license; verify compatibility with yaze's
  packaging before shipping assembled output.
- Shared Mesen2 client refactor is a breaking change for CLI commands — plan
  a shim while downstream scripts migrate.

## Milestones

| Milestone | Description |
|-----------|-------------|
| M0 | Link `z3dk-core` as an optional target (`YAZE_ENABLE_Z3DK=ON/OFF`). Hello-world assemble behind a flag. |
| M1 | Diagnostics path: patch text buffer → assemble → surface errors in Notifications + inline gutter. |
| M2 | Symbol / label extraction: populate yaze project symbol table from assemble result. |
| M3 | Mesen2 client consolidation (single client shared by yaze CLI, editor, and embedded z3dk). |
| M4 | Hover + go-to-definition on ASM panels. |
| M5 | `.mlb` export menu. Linter runs on save. |

## References

- `~/src/hobby/z3dk/src/z3dk_core/assembler.h`
- `~/src/hobby/z3dk/src/z3dk_core/lint.h`
- `~/src/hobby/z3dk/src/z3dk_core/snes_knowledge_base.h`
- `~/src/hobby/z3dk/src/z3lsp/mesen_client.h`
- yaze skill: `/z3dk-toolchain`
- yaze Mesen2 panels: `src/app/editor/oracle/`, `src/cli/commands/mesen2_*`
