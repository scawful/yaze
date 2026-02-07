# Handoff: HackManifest Guardrails + Save Conflict Checks (2026-02-06)

**Owner (Agent ID):** `ai-infra-architect`  
**Repo:** `yaze` (`/Users/scawful/src/hobby/yaze`)  
**Status:** WIP (link blocker resolved; build + unit tests green; pending manual UX validation + commit split)

## Goal / Why
Tighten Oracle-of-Secrets “HackManifest” integration so yaze can:
- Show ASM-provided labels (room tags) consistently in UI.
- Detect editor save writes that collide with ASM-owned/protected regions.
- Avoid false negatives caused by PC-offset vs SNES-address mismatches.
- Make the guardrails easy to relax via existing project `rom_metadata.write_policy` (`allow|warn|block`) so it does not block iteration.

## Current Git State
`git status -sb` shows `master` ahead of `origin/master` (23 commits) and a dirty worktree.

Modified:
- `src/CMakeLists.txt`
- `src/app/editor/dungeon/dungeon_canvas_viewer.cc`
- `src/app/editor/dungeon/dungeon_editor_v2.cc`
- `src/app/editor/message/message_editor.cc`
- `src/app/editor/overworld/overworld_editor.cc`
- `src/app/editor/ui/selection_properties_panel.cc`
- `src/core/hack_manifest.cc`
- `src/core/hack_manifest.h`
- `src/core/project.cc`
- `src/zelda3/overworld/overworld.cc`
- `src/zelda3/overworld/overworld.h`
- `src/zelda3/resource_labels.cc`
- `src/zelda3/resource_labels.h`
- `src/zelda3/zelda3_labels.cc`
- `test/CMakeLists.txt`
- `test/test.cmake`
- `test/unit/core/hack_manifest_test.cc`

Untracked:
- `docs/internal/architecture/hack_manifest.md`
- `test/unit/zelda3/resource_labels_test.cc`

## Key Changes (Uncommitted)

### 1) HackManifest: fix PC-vs-SNES write conflict detection
Problem:
- Oracle emits `hack_manifest.json` addresses in SNES LoROM space (e.g. `0x01CC14`).
- yaze editors compute write targets as PC ROM offsets.
- Passing PC ranges into SNES-based analysis silently breaks conflict detection.

Changes:
- `src/core/hack_manifest.h`: add `HackManifest::AnalyzePcWriteRanges(...)`.
- `src/core/hack_manifest.cc`: implement `AnalyzePcWriteRanges` by converting PC ranges via `PcToSnes(...)` and splitting at `0x8000` LoROM boundaries.

### 2) HackManifest: normalize mirrored bank addresses consistently
Problem:
- Manifest may reference FastROM mirrors (banks `0x80-0xFF`) while yaze classification uses canonical banks (`0x00-0x7F`).

Changes:
- `src/core/hack_manifest.cc`: add `NormalizeSnesAddress()` and apply it broadly.
- Normalize at load time too (protected regions, owned banks, room tag addresses, message layout addresses).
- Normalize owned bank IDs (`bank &= 0x7F`) so `0x9E` and `0x1E` behave equivalently.

### 3) Project: avoid stale manifest pointers across reloads
Problem:
- `ResourceLabelProvider` keeps a raw pointer to `HackManifest` owned by `YazeProject`.
- Reloading/switching projects can leave the provider pointing at stale state unless explicitly cleared.

Changes:
- `src/core/hack_manifest.h`: add `HackManifest::Clear()` (public) to reset internal state.
- `src/core/project.cc`:
  - `TryLoadHackManifest()` now clears previous manifest state and sets provider manifest pointer to `nullptr` before attempting load.
  - On successful load, sets provider manifest pointer to `&hack_manifest`; on failure, keeps it cleared.
  - `InitializeResourceLabelProvider()` sets manifest pointer to `&hack_manifest` when loaded, else `nullptr`.

### 4) Save guardrails: use project `write_policy` to allow warn/block behavior
Added conflict checks before writes:
- `src/app/editor/dungeon/dungeon_editor_v2.cc`:
  - Compute projected write ranges for room headers + objects (PC offsets).
  - Run `AnalyzePcWriteRanges`.
  - Apply policy:
    - `allow`: log only (debug)
    - `warn`: toast warning + log (warn) but allow save
    - `block`: toast error + log (warn) and abort save
- `src/app/editor/overworld/overworld_editor.cc`:
  - Same pattern, but only when `FeatureFlags::overworld.kSaveOverworldMaps` is enabled (to avoid unrelated saves being blocked).

### 5) Overworld projected write ranges
Added `Overworld::GetProjectedWriteRanges()` used by OverworldEditor conflict checks:
- `src/zelda3/overworld/overworld.h`
- `src/zelda3/overworld/overworld.cc`

It returns conservative PC-offset ranges covering:
- Map32 quadrant data
- Map16 tiles
- Compressed map pointer tables (low/high) and compressed data regions (bank 0B/0C + overflow)

### 6) Labels + UI integration cleanup
- `src/zelda3/resource_labels.h/.cc`: `ResourceLabelProvider` now consults HackManifest for `ResourceType::kRoomTag`.
- `src/zelda3/zelda3_labels.cc`: expanded room tag names list to cover all known vanilla tag IDs (0..64).
- `src/app/editor/dungeon/dungeon_canvas_viewer.cc`: removed local tag name array in favor of `zelda3::GetRoomTagLabel(...)` and show “disabled by feature flag” hints when manifest says so.
- `src/app/editor/ui/selection_properties_panel.cc`: replace placeholder “room properties” with actual editable `Room` fields and manifest-backed tag combos.
- `src/app/editor/message/message_editor.cc`: show expanded/vanilla type column and prefer manifest `message_layout.first_expanded_id` when available (fallback to `list_of_texts_.size()` if unset).

### 7) Tests + docs
- `test/unit/core/hack_manifest_test.cc`: add coverage for mirror normalization + `AnalyzePcWriteRanges`.
- `test/unit/zelda3/resource_labels_test.cc` + `test/CMakeLists.txt` + `test/test.cmake`: add unit coverage for manifest label precedence (project override > manifest > vanilla).
- `docs/internal/architecture/hack_manifest.md`: new internal doc explaining address spaces, conflict checks, and the write policy behavior.

## Build/Validation Status

### Build attempt (agent-safe dir)
Configured and built in `build_agent` with a “minimal-like” set of options (AI/agent/grpc off, GUI/tests on).

### Current blocker: yaze link failure (unrelated to HackManifest changes)
`cmake --build build_agent --config RelWithDebInfo` fails while linking the GUI executable:
- Undefined symbol: `yaze::cli::api::HttpServer::~HttpServer()`

Root cause:
- `src/app/main.cc` unconditionally includes `cli/service/api/http_server.h` and declares `std::unique_ptr<yaze::cli::api::HttpServer> api_server;` even when `YAZE_HTTP_API_ENABLED` is NOT defined.
- `HttpServer` implementation (`cli/service/api/http_server.cc`) is only compiled into `yaze_agent` via `src/cli/agent.cmake`.
- In minimal builds `yaze_agent` is stubbed out, so `http_server.cc` is not linked, but `main.cc` still references the destructor symbol through `unique_ptr`.

Fix options (pick one):
1. **Preferred quick fix:** in `src/app/main.cc`, guard the include + `std::unique_ptr<HttpServer>` declaration behind `#if defined(YAZE_HTTP_API_ENABLED)` so disabled builds don’t reference the type at all.
2. **More correct architecture:** move `cli/service/api/http_server.cc` (and handlers) out of `yaze_agent` and into a dedicated `yaze_http_api` library that is built+linked when `YAZE_ENABLE_HTTP_API=ON`. This prevents “HTTP API enabled but agent stubbed” from breaking.

Workaround to keep testing moving (until link is fixed):
- Build only the unit test binary:
  - `cmake --build build_agent --config RelWithDebInfo --target yaze_test_stable`
  - then run `ctest --test-dir build_agent -C RelWithDebInfo -R \"HackManifestTest|ResourceLabelsTest\" --output-on-failure`

### Update (2026-02-07)
- Implemented fix option (1): guard the `HttpServer` include + `std::unique_ptr<HttpServer>` under `YAZE_HTTP_API_ENABLED` in `src/app/main.cc`, so HTTP API disabled builds no longer reference the type.
- `cmake --build build_agent --config RelWithDebInfo` now links successfully.
- Ran `ctest --test-dir build_agent -C RelWithDebInfo -R "HackManifestTest|ResourceLabelsTest" --output-on-failure` (8/8 passed).

## Next Steps (Suggested)
1. Rebuild `build_agent` fully, then run:
   - `ctest --test-dir build_agent -C RelWithDebInfo -R \"HackManifestTest|ResourceLabelsTest\" --output-on-failure`
2. Re-check the save guardrails UX with an OOS project:
   - Verify `write_policy=warn` only warns and still saves.
   - Verify `write_policy=block` aborts save.
3. Split commits (once build+tests are green):
   - `ai-infra-architect: hack manifest write-conflict analysis (pc->snes) + mirror normalization`
   - `ai-infra-architect: manifest-aware save guardrails (respect rom write_policy)`
   - `ai-infra-architect: resource label provider uses hack manifest room tags + tests`
   - Optional: `ai-infra-architect: document hack manifest architecture`
