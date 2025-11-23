## Coordination Board

**Guidelines:** Keep entries concise (<=5 lines). Archive completed work weekly. Target <=40 active entries.

---

### 2025-11-24 CODEX – v0.3.9 release fix (IN PROGRESS)
- TASK: Fix failing release run 19609095482 for v0.3.9; validate artifacts and workflow
- SCOPE: .github/workflows/release.yml, packaging/CPack, release artifacts
- STATUS: IN_PROGRESS (another agent actively working)
- NOTES: Root causes identified (hashFiles() invalidation, Windows crash_handler POSIX macros)

---

### 2025-11-23 COORDINATOR - v0.4.0 Roadmap Refined
- TASK: v0.4.0 "Editor Stability & OOS Support" - first feature release after CI/CD stabilization
- SCOPE: Dungeon full workflow, Message editor expanded BIN, ZSCOW audit, E2E tests, AI inspection
- STATUS: ACTIVE
- ROADMAP: `docs/internal/roadmaps/2025-11-23-refined-roadmap.md`
- PRIORITIES: (1) Dungeon save is STUB, (2) Message BIN export broken, (3) ZSCOW needs audit

---

### 2025-11-23 CLAUDE_AIINF - Semantic Inspection API
- TASK: Implement Semantic Inspection API Phase 1 for AI agents
- SCOPE: src/app/emu/debug/semantic_introspection.{h,cc}
- STATUS: COMPLETE
- NOTES: Game state JSON serialization for AI agents. Phase 1 MVP complete.

---

### 2025-11-23 CLAUDE_CORE – SDL3 Backend Infrastructure
- TASK: Implement SDL3 backend infrastructure for v0.4.0 migration
- SCOPE: src/app/platform/, src/app/emu/audio/, src/app/emu/input/, src/app/gfx/backend/
- STATUS: COMPLETE (commit a5dc884612)
- NOTES: 17 new files, IWindowBackend/IAudioBackend/IInputBackend/IRenderer interfaces

---

### 2025-11-22 CLAUDE_CORE - CI Optimization
- TASK: Optimize CI for lean PR/push runs with comprehensive nightly testing
- SCOPE: .github/workflows/ci.yml, nightly.yml
- STATUS: COMPLETE
- NOTES: PR CI ~5-10 min (was 15-20), nightly runs comprehensive tests

---

### 2025-11-22 CLAUDE_AIINF - Test Suite Gating
- TASK: Gate optional test suites OFF by default (Test Slimdown Initiative)
- SCOPE: cmake/options.cmake, test/CMakeLists.txt
- STATUS: COMPLETE
- NOTES: AI/ROM/benchmark tests now require explicit enabling

---

### 2025-11-22 CLAUDE_AIINF - FileSystemTool
- TASK: Implement FileSystemTool for AI agents (Milestone 4, Phase 3)
- SCOPE: src/cli/service/agent/tools/filesystem_tool.{h,cc}
- STATUS: COMPLETE
- NOTES: Read-only filesystem exploration with security features

---

## Archived Sessions

Historical entries from 2025-11-20 to 2025-11-22 have been archived to:
`docs/internal/agents/archive/coordination-board-2025-11-20-to-22.md`

**Archived content includes:**
- 35+ "keep-chatting" morale rounds (Lightning Tips, bingo, haiku challenges)
- Agent onboarding sessions (GEMINI_FLASH_AUTOM, CODEX)
- AI-Powered Test Generation PoC research
- Real-Time Emulator Integration research
- gRPC/Linux build hang resolution
- Windows build workflow fixes
- Friendly rivalry documentation sprints

All key achievements are documented in respective feature/research documents.
