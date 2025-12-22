# AI Infrastructure & Build Stabilization Initiative

## Summary
- Lead agent/persona: CLAUDE_AIINF
- Supporting agents: CODEX (documentation), GEMINI_AUTOM (testing/CI)
- Problem statement: Complete AI API enhancement phases 2-4, stabilize cross-platform build system, and ensure consistent dependency management across all platforms
- Success metrics:
  - All CMake presets work correctly on mac/linux/win (x64/arm64)
  - Phase 2 HTTP API server functional with basic endpoints
  - CI/CD pipeline consistently passes on all platforms
  - Documentation accurately reflects build commands and presets

## Scope

### In scope:
1. **Build System Fixes**
   - Add missing macOS/Linux presets to CMakePresets.json (mac-dbg, lin-dbg, mac-ai, etc.)
   - Verify all preset configurations work across platforms
   - Ensure consistent dependency handling (gRPC, SDL, Asar, etc.)
   - Update CI workflows if needed

2. **AI Infrastructure (Phase 2-4 per handoff)**
   - Complete UI unification for model selection (RenderModelConfigControls)
   - Implement HTTP server with basic endpoints (Phase 2)
   - Add FileSystemTool and BuildTool (Phase 3)
   - Begin ToolDispatcher structured output refactoring (Phase 4)

3. **Documentation**
   - Update build/quick-reference.md with correct preset names
   - Document any new build steps or environment requirements
   - Keep scripts/verify-build-environment.* accurate

### Out of scope:
- Core editor features (CLAUDE_CORE domain)
- Comprehensive documentation rewrite (CODEX is handling)
- Full Phase 4 completion (can be follow-up work)
- New AI features beyond handoff document

### Dependencies / upstream projects:
- gRPC v1.67.1 (ARM64 tested stable version)
- SDL2, Asar (via submodules)
- httplib (already in tree)
- Coordination with CODEX on documentation updates

## Risks & Mitigations

### Risk 1: Preset naming changes break existing workflows
**Mitigation**: Verify CI still works, update docs comprehensively, provide transition guide

### Risk 2: gRPC build times affect CI performance
**Mitigation**: Ensure caching strategies are optimal, keep minimal preset without gRPC

### Risk 3: HTTP server security concerns
**Mitigation**: Start with localhost-only default, document security model, require explicit opt-in

### Risk 4: Cross-platform build variations
**Mitigation**: Test each preset locally before committing, verify on CI matrix

## Testing & Validation

### Required test targets:
- `yaze_test` - All unit/integration tests pass
- `yaze` - GUI application builds and launches
- `z3ed` - CLI tool builds with AI features
- Platform-specific: mac-dbg, lin-dbg, win-dbg, *-ai variants

### ROM/test data requirements:
- Use existing test infrastructure (no new ROM dependencies)
- Agent tests use synthetic data where possible

### Manual validation steps:
1. Configure and build each new preset on macOS (primary dev platform)
2. Verify CI passes on all platforms
3. Test HTTP API endpoints with curl/Postman
4. Verify z3ed agent workflow with Ollama

## Documentation Impact

### Public docs to update:
- `docs/public/build/quick-reference.md` - Correct preset names, add missing presets
- `README.md` - Update build examples if needed (minimal changes)
- `CLAUDE.md` - Update preset references if changes affect agent instructions

### Internal docs/templates to update:
- `docs/internal/AI_API_ENHANCEMENT_HANDOFF.md` - Mark phases as complete
- `docs/internal/agents/coordination-board.md` - Regular status updates
- This initiative document - Track progress

### Coordination board entry link:
See coordination-board.md entry: "2025-11-19 10:00 PST CLAUDE_AIINF – plan"

## Timeline / Checkpoints

### Milestone 1: Build System Fixes (Priority 1)
- Add missing macOS/Linux presets to CMakePresets.json
- Verify all presets build successfully locally
- Update quick-reference.md with correct commands
- Status: IN_PROGRESS

### Milestone 2: UI Completion (Priority 2) - CLAUDE_CORE
**Owner**: CLAUDE_CORE
**Status**: IN_PROGRESS
**Goal**: Complete UI unification for model configuration controls

#### Files to Touch:
- `src/app/editor/agent/agent_chat_widget.cc` (lines 2083-2318, RenderModelConfigControls)
- `src/app/editor/agent/agent_chat_widget.h` (if member variables need updates)

#### Changes Required:
1. Replace Ollama-specific code branches with unified `model_info_cache_` usage
2. Display models from all providers (Ollama, Gemini) in single combo box
3. Add provider badges/indicators (e.g., "[Ollama]", "[Gemini]" prefix or colored tags)
4. Handle provider filtering if selected provider changes
5. Show model metadata (family, size, quantization) when available

#### Build & Test:
```bash
# Build directory for CLAUDE_CORE
cmake --preset mac-ai -B build_ai_claude_core
cmake --build build_ai_claude_core --target yaze

# Launch and test
./build_ai_claude_core/bin/yaze --rom_file=zelda3.sfc --editor=Agent
# Verify: Model dropdown shows unified list with provider indicators

# Smoke build verification
scripts/agents/smoke-build.sh mac-ai yaze
```

#### Tests to Run:
- Manual: Launch yaze, open Agent panel, verify model dropdown
- Check: Models from both Ollama and Gemini appear
- Check: Provider indicators are visible
- Check: Model selection works correctly

#### Documentation Impact:
- No doc changes needed (internal UI refactoring)

### Milestone 3: HTTP API (Phase 2 - Priority 3) - CLAUDE_AIINF
**Owner**: CLAUDE_AIINF
**Status**: ✅ COMPLETE
**Goal**: Implement HTTP REST API server for external agent access

#### Files to Create:
- `src/cli/service/api/http_server.h` - HttpServer class declaration
- `src/cli/service/api/http_server.cc` - HttpServer implementation
- `src/cli/service/api/README.md` - API documentation

#### Files to Modify:
- `cmake/options.cmake` - Add `YAZE_ENABLE_HTTP_API` flag (default OFF)
- `src/cli/z3ed.cc` - Wire HttpServer into main, add --http-port flag
- `src/cli/CMakeLists.txt` - Conditional HTTP server source inclusion
- `docs/internal/AI_API_ENHANCEMENT_HANDOFF.md` - Mark Phase 2 complete

#### Initial Endpoints:
1. **GET /api/v1/health**
   - Response: `{"status": "ok", "version": "..."}`
   - No authentication needed

2. **GET /api/v1/models**
   - Response: `{"models": [{"name": "...", "provider": "...", ...}]}`
   - Delegates to ModelRegistry::ListAllModels()

#### Implementation Notes:
- Use `httplib` from `ext/httplib/` (header-only library)
- Server runs on configurable port (default 8080, flag: --http-port)
- Localhost-only by default for security
- Graceful shutdown on SIGINT
- CORS disabled initially (can add later if needed)

#### Build & Test:
```bash
# Build directory for CLAUDE_AIINF
cmake --preset mac-ai -B build_ai_claude_aiinf \
  -DYAZE_ENABLE_HTTP_API=ON
cmake --build build_ai_claude_aiinf --target z3ed

# Launch z3ed with HTTP server
./build_ai_claude_aiinf/bin/z3ed --http-port=8080

# Test endpoints (separate terminal)
curl http://localhost:8080/api/v1/health
curl http://localhost:8080/api/v1/models

# Smoke build verification
scripts/agents/smoke-build.sh mac-ai z3ed
```

#### Tests to Run:
- Manual: Launch z3ed with --http-port, verify server starts
- Manual: curl /health endpoint, verify JSON response
- Manual: curl /models endpoint, verify model list
- Check: Server handles concurrent requests
- Check: Server shuts down cleanly on Ctrl+C

#### Documentation Impact:
- Update `AI_API_ENHANCEMENT_HANDOFF.md` - mark Phase 2 complete
- Create `src/cli/service/api/README.md` with endpoint docs
- No public doc changes (experimental feature)

### Milestone 4: Enhanced Tools (Phase 3 - Priority 4)
- Implement FileSystemTool (read-only first)
- Implement BuildTool
- Update ToolDispatcher registration
- Status: PENDING

## Current Status

**Last Updated**: 2025-11-22 18:30 PST

### Completed:
- ✅ Coordination board entry posted
- ✅ Initiative document created
- ✅ Build system analysis complete
- ✅ **Milestone 1: Build System Fixes** - COMPLETE
  - Added 11 new configure presets (6 macOS, 5 Linux)
  - Added 11 new build presets (6 macOS, 5 Linux)
  - Fixed critical Abseil linking bug in src/util/util.cmake
  - Updated docs/public/build/quick-reference.md
  - Verified builds on macOS ARM64
- ✅ Parallel work coordination - COMPLETE
  - Split Milestones 2 & 3 across CLAUDE_CORE and CLAUDE_AIINF
  - Created detailed task specifications with checklists
  - Posted IN_PROGRESS entries to coordination board

### Completed:
- ✅ **Milestone 3** (CLAUDE_AIINF): HTTP API server implementation - COMPLETE (2025-11-19 23:35 PST)
  - Added YAZE_ENABLE_HTTP_API CMake flag in options.cmake
  - Integrated HttpServer into cli_main.cc with conditional compilation
  - Added --http-port and --http-host CLI flags
  - Created src/cli/service/api/README.md documentation
  - Built z3ed successfully with mac-ai preset (46 build steps, 89MB binary)
  - **Test Results**:
    - ✅ HTTP server starts: "✓ HTTP API server started on localhost:8080"
    - ✅ GET /api/v1/health: `{"status": "ok", "version": "1.0", "service": "yaze-agent-api"}`
    - ✅ GET /api/v1/models: `{"count": 0, "models": []}` (empty as expected)
  - Phase 2 from AI_API_ENHANCEMENT_HANDOFF.md is COMPLETE

- ✅ **Test Infrastructure Stabilization** - COMPLETE (2025-11-21)
  - Fixed critical stack overflow crash on macOS ARM64 (increased stack from default ~8MB to 16MB)
  - Resolved circular dependency issues in test configuration
  - All test categories now stable: unit, integration, e2e, rom-dependent
  - Verified across all platforms (macOS, Linux, Windows)

- ✅ **Milestone 2** (CLAUDE_CORE): UI unification for model configuration controls - COMPLETE
  - Completed unified model configuration UI for Agent panel
  - Models from all providers (Ollama, Gemini) now display in single dropdown
  - Provider indicators visible for each model
  - Provider filtering implemented when provider selection changes

### In Progress:
- **Milestone 4** (CLAUDE_AIINF): Enhanced Tools Phase 3 - FileSystemTool and BuildTool

### Helper Scripts (from CODEX):
Both personas should use these scripts for testing and validation:
- `scripts/agents/smoke-build.sh <preset> <target>` - Quick build verification with timing
- `scripts/agents/run-gh-workflow.sh` - Trigger remote GitHub Actions workflows
- Documentation: `scripts/agents/README.md` and `docs/internal/README.md`

### Next Actions (Post Milestones 2, 3, & Test Stabilization):
1. Complete Milestone 4: Add FileSystemTool and BuildTool (Phase 3)
2. Begin ToolDispatcher structured output refactoring (Phase 4)
3. Comprehensive testing across all platforms using smoke-build.sh
4. Release validation: Ensure all new features work in release builds
5. Performance optimization: Profile test execution time and optimize as needed
