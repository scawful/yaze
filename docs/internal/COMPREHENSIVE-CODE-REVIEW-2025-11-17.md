# Build Stabilization Review — 2025-11-17

## Summary
- Diagnosed Windows CI fallout caused by Abseil include dirs not propagating when gRPC was enabled; added `yaze_refresh_absl_usage_metadata()` plus refreshed helper functions in `cmake/dependencies/grpc_stack.cmake`.
- Swapped the gRPC zlib provider to `module` so the CPM build fetches its own copy instead of relying on system zlib (fixes Windows preset configuration).
- Added a first-class cpp-httplib dependency module and linked it via `yaze_httplib` anywhere JSON-powered networking is compiled.
- Ensured `.github/actions/build-project` always creates the `build/` directory before caching to stop “Path Validation Error” warnings in CI.
- Verified `ci-windows` feature coverage locally by configuring with gRPC/JSON enabled and compiling `yaze_util` (Abseil headers now resolve).

## Outstanding Risks / Follow-ups
- `src/lib/SDL/android-project` still expects an Android SDK when lint tools sweep the vendor tree; treat as third-party noise for now.
- macOS/Linux cache steps should be rechecked on the next workflow run to confirm the directory bootstrap fixes the warnings.

## Next Steps
1. Push these CMake and workflow fixes.
2. Re-run the full `CI/CD Pipeline` workflow (both build + test matrices).
3. If macOS/Linux caches still warn, consider scoping cache paths (e.g., `build/${{matrix.platform}}`) per job.
4. Schedule manual editor smoke tests once agent networking is exercised in CI again.

