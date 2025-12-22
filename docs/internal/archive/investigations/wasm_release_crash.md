# WASM Release Crash Plan

Status: **ACTIVE**  
Owner: **backend-infra-engineer**  
Created: 2025-11-26  
Last Reviewed: 2025-11-26  
Next Review: 2025-12-03  
Coordination: [coordination-board entry](./coordination-board.md#2025-11-25-backend-infra-engineer--wasm-release-crash-triage)

## Context
Release WASM build crashes with `RuntimeError: memory access out of bounds` during ROM load; debug build succeeds. Stack maps to `std::unordered_map<int, gfx::Bitmap>` construction and sprite name static init, implying UB/heap corruption in bitmap/tile caching paths surfaced by `-O3`/no `SAFE_HEAP`/LTO.

## Goals / Exit Criteria
- Reproduce crash on a “sanitized release” build with symbolized stack (SAFE_HEAP/ASSERTIONS) and isolate the exact C++ site.  
- Implement a fix eliminating the heap corruption in release (likely bitmap/tile cache ownership/eviction) and verify both release/debug load ROM successfully.  
- Ship a safe release configuration (temporary SAFE_HEAP or LTO toggle acceptable) for GitHub Pages until root cause fix lands.  
- Remove/mitigate boot-time `Module` setter warning in `core/namespace.js` to reduce noise.  
- Document findings and updated build guidance in wasm playbook.

## Plan
1) **Repro + Instrumentation (today)**
   - Build “sanitized release” preset (O3 + `-s SAFE_HEAP=1 -s ASSERTIONS=2`, LTO off) and re-run ROM load via Playwright harness to capture precise stack.
   - Add temporary addr2line helper script for wasm (if needed) to map addresses quickly.

2) **Root Cause Fix (next)**
   - Inspect bitmap/tile cache ownership and eviction (`TileCache::CacheTile`, `BitmapTable`, `Arena::ProcessTextureQueue`, `Canvas::DrawBitmapTable`) for dangling surface/texture pointers or moved-from Bitmaps stored in unordered_map under optimization.
   - Patch to avoid storing moved-from Bitmap references (prefer emplace/move with clear ownership), ensure surface/texture pointers nulled before eviction, and guard palette/renderer access in release.
   - Rebuild release (standard flags) and verify ROM load succeeds without SAFE_HEAP.

3) **Mitigations & Cleanup (parallel/after fix)**
   - If fix needs longer: ship interim release with SAFE_HEAP/LTO-off to unblock Pages users.
   - Fix `window.yaze.core.Module` setter clash (define writable property) to remove boot warning.
   - Triage double `FS.syncfs` warning (low priority) while in code.
   - Update `wasm-antigravity-playbook` with debug steps + interim release flag guidance.

## Validation
- Automated ROM load (Playwright script) passes on release and debug builds, no runtime errors or aborts.
- Manual spot-check in browser confirms ROM loads and renders; no console OOB errors; yazeDebug ROM status shows loaded.
- GitHub Pages deployment built with chosen flags loads ROM without crash.
- No regressions in debug build (SAFE_HEAP path still works).

## Notes / Risks
- SAFE_HEAP in release increases bundle size/perf cost; acceptable as interim but not final.
- If root cause lives in SDL surface/texture lifetimes, need to validate on native as well (possible hidden UB masked by sanitizer).
