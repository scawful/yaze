# WASM Planning Documentation Archive

**Date Archived:** November 24, 2025
**Archived By:** Documentation Janitor

This directory contains WASM development planning documents that represent historical design decisions and feature roadmaps. Most content has been superseded by implementation and integration into `docs/internal/wasm_dev_status.md`.

## Archived Documents

### 1. wasm-network-support-plan.md
- **Original Purpose:** Detailed plan for implementing browser-compatible networking (Phases 1-5)
- **Status:** Superseded by implementation
- **Why Archived:** Network abstraction and WASM implementations have been completed; this planning document is no longer needed
- **Current Reference:** See `wasm_dev_status.md` Section 1.1 (ROM Loading & Initialization)

### 2. wasm-web-features-roadmap.md
- **Original Purpose:** Comprehensive feature roadmap (Phases 1-14)
- **Status:** Mostly completed or planning-stage
- **Why Archived:** Long-term planning document that predates actual implementation; many features are now in wasm_dev_status.md
- **Current Reference:** See `wasm_dev_status.md` Sections 1-4 for completed features

### 3. wasm-web-app-enhancements-plan.md
- **Original Purpose:** Detailed Phase 1-8 implementation plan
- **Status:** Most phases completed
- **Why Archived:** Highly structured planning document; actual implementations supersede these plans
- **Current Reference:** See `wasm_dev_status.md` for current status of all phases

### 4. wasm-ai-integration-summary.md
- **Original Purpose:** Summary of Phase 5 AI Service Integration implementation
- **Status:** Consolidated into main status document
- **Why Archived:** Content merged into `wasm_dev_status.md` AI Agent Integration section
- **Current Reference:** See `wasm_dev_status.md` Section 1 (Completed Features → AI Agent Integration)

### 5. wasm-widget-tracking-implementation.md
- **Original Purpose:** Detailed implementation notes for widget bounds tracking
- **Status:** Consolidated into main status document
- **Why Archived:** Implementation details merged into `wasm_dev_status.md` Control APIs section
- **Current Reference:** See `wasm_dev_status.md` Section 1.4 (WASM Control APIs → Widget Tracking Infrastructure)

## Content Consolidation

The following information from archived documents has been consolidated into active documentation:

| Original Document | Content Moved To | Location |
|---|---|---|
| wasm-network-support-plan.md | wasm_dev_status.md | Section 1.1, Section 4 (Key Files) |
| wasm-web-features-roadmap.md | wasm_dev_status.md | Section 1 (Completed Features) |
| wasm-web-app-enhancements-plan.md | wasm_dev_status.md | Section 1 (Completed Features) |
| wasm-ai-integration-summary.md | wasm_dev_status.md | Section 1 (AI Agent Integration) |
| wasm-widget-tracking-implementation.md | wasm_dev_status.md | Section 1.4 (Widget Tracking Infrastructure) |

## Active WASM Documentation

The following documents remain in `docs/internal/` and are actively maintained:

1. **wasm_dev_status.md** - CANONICAL STATUS DOCUMENT
   - Current implementation status (updated Nov 24, 2025)
   - All completed features with file references
   - Technical debt and known issues
   - Roadmap for next steps

2. **wasm-debug-infrastructure.md** - HIGH-LEVEL DEBUGGING OVERVIEW
   - Debugging architecture and philosophy
   - File system fixes with explanations
   - Known limitations
   - Cross-references to detailed API docs

3. **wasm-yazeDebug-api-reference.md** - DETAILED API REFERENCE
   - Complete JavaScript API reference for `window.yazeDebug`
   - Authoritative source for all debug functions
   - Usage examples for each API section
   - Palette, ROM, overworld, arena, emulator debugging

4. **wasm_dungeon_debugging.md** - QUICK REFERENCE GUIDE
   - Short, practical debugging tips
   - God mode console inspector usage
   - Command line bridge reference
   - Feature parity notes between WASM and native builds

5. **debugging-wasm-memory-errors.md** - TECHNICAL REFERENCE
   - Memory debugging techniques
   - SAFE_HEAP usage
   - Common pitfalls and fixes
   - Function mapping methods

## How to Use This Archive

If you need historical context about WASM development decisions:
1. Start with the relevant archived document
2. Check the "Current Reference" section for where the content moved
3. Consult the active documentation for implementation details

When searching for WASM documentation, use this hierarchy:
1. **wasm_dev_status.md** - Status and overview (start here)
2. **wasm-debug-infrastructure.md** - Debugging overview
3. **wasm-yazeDebug-api-reference.md** - Detailed debug API
4. **wasm_dungeon_debugging.md** - Quick reference for dungeon editor
5. **debugging-wasm-memory-errors.md** - Memory debugging specifics

## Rationale for Archival

The WASM codebase evolved rapidly from November 2024 to November 2025:

- **Planning Phase** (early 2024): Detailed roadmaps and enhancement plans created
- **Implementation Phase** (mid 2024 - Nov 2025): Features implemented incrementally
- **Integration Phase** (current): All systems working, focus on maintenance and refinement

Archived documents represented the planning phase. As implementation completed, their value shifted from prescriptive (what to build) to historical (how we decided to build it). Consolidating information into `wasm_dev_status.md` provides:

- Single source of truth for current status
- Easier maintenance (updates in one place)
- Clearer navigation for developers
- Better signal-to-noise ratio

## Future Archival Guidance

When new WASM documentation is created:
- Keep planning/roadmap docs current or archive them promptly
- Consolidate implementation summaries into main status document
- Use high-level docs (like wasm-debug-infrastructure.md) for architecture overview
- Use detailed reference docs (like wasm-yazeDebug-api-reference.md) for API details
- Maintain clear cross-references between related docs

---

**Archive Directory Structure:**
```
docs/internal/agents/archive/wasm-planning-2025/
├── README.md (this file)
├── wasm-network-support-plan.md
├── wasm-web-features-roadmap.md
├── wasm-web-app-enhancements-plan.md
├── wasm-ai-integration-summary.md
└── wasm-widget-tracking-implementation.md
```
