# Documentation Cleanup Summary - November 2025

**Date:** 2025-11-24
**Action:** Cleanup of speculative planning documents and AI-generated bloat from `/docs/internal/plans/`
**Rationale:** Planning documents should live in GitHub issues and coordination board, not as static markdown. Only actionable, actively-tracked plans belong in the codebase.

## Files Deleted (Pure AI-Generated Bloat)

These files were entirely speculative with no corresponding implementation:

1. **asm-debug-prompt-engineering.md** (45KB)
   - Extensive prompt templates for 65816 debugging
   - No evidence of integration or use
   - Classified as AI-generated reference material

2. **ai-assisted-development-plan.md** (17KB)
   - Workflow proposal for AI-assisted development
   - All features marked "Active" with "Next Review" dates but never implemented
   - Generic architecture diagrams with no corresponding code

3. **app-dev-agent-tools.md** (21KB)
   - 824 lines specifying 16 agent tools (build, code analysis, debug, editor integration)
   - All tools in Phase 1-3 (theoretical)
   - No implementation in codebase or recent commits

4. **EDITOR_ROADMAPS_2025-11.md** (25KB)
   - Multi-agent analysis document referencing "imgui-frontend-engineer" agent analysis
   - Generic roadmap format with estimated effort hours
   - Duplicate content with dungeon_editor_ui_refactor.md

5. **message_system_improvement_plan.md** (2KB)
   - Duplicate of sections in message_editor_implementation_roadmap.md
   - Generic feature wishlist (JSON export, translation workspace, search & replace)
   - No distinct value

6. **graphics_system_improvement_plan.md** (2.8KB)
   - Feature wishlist (unified editor, palette management, sprite assembly)
   - No concrete deliverables or implementation plan
   - Superseded by architecture documentation

7. **ui_modernization.md** (3.3KB)
   - Describes patterns already documented in CLAUDE.md
   - Marked "Active" but content is obsolete (already implemented)
   - Redundant with existing guidelines

## Files Archived (Partially Implemented / Historical Reference)

These files have some value as reference but are not actively tracked work:

1. **emulator-debug-api-design.md** → `archive/plans-2025-11/`
   - Design document for emulator debugging API
   - Some features implemented (breakpoints, memory inspection)
   - Watchpoints and symbol loading still planned but deprioritized
   - Value: Technical reference for future work

2. **message_editor_implementation_roadmap.md** → `archive/plans-2025-11/`
   - References actual code (MessageData, MessagePreview classes)
   - Documents what's completed vs. what's missing (JSON import/export)
   - Some ongoing development but should be tracked in coordination board
   - Value: Implementation reference

3. **hex_editor_enhancements.md** → `archive/plans-2025-11/`
   - Phase 1 (Data Inspector) concept defined
   - Phases 2-4 unimplemented
   - Better tracked as GitHub issue than static plan
   - Value: Technical spike reference

4. **dungeon_editor_ui_refactor.md** → `archive/plans-2025-11/`
   - Actually referenced in git commit acab491a1f (component was removed)
   - Concrete refactoring steps with clear deliverables
   - Now completed/obsolete
   - Value: Historical record of refactoring

## Files Retained (Actively Tracked)

1. **web_port_strategy.md**
   - Strategic milestone document for WASM port
   - Multiple recent commits show active ongoing work (52b1a99, 56e05bf, 206e926, etc.)
   - Clear milestones with deliverables
   - Actively referenced in CI/build processes
   - Status: KEEP - actively developed feature

2. **ai-infra-improvements.md**
   - Structured phase-based plan (gRPC server, emulator RPCs, breakpoints, symbols)
   - More specific than other plans with concrete files to modify
   - Tracks infrastructure gaps with file references
   - Status: KEEP - tracks ongoing infrastructure work

3. **README.md**
   - Directory governance and guidelines
   - Actively enforces plan organization standards
   - Status: KEEP - essential for directory management

## Rationale for This Cleanup

### Problem
- 14 planning files (400KB) in `/docs/internal/plans/`
- Most marked "Active" with forward-looking dates (Nov 25 → Dec 2)
- Little correlation with actual work in recent commits
- Directory becoming a repository of speculative AI-generated content

### Solution
- Keep only plans with active ongoing work (2 files)
- Archive reference documents with partial implementation (4 files)
- Delete pure speculation and AI-generated bloat (7 files)
- Directory size reduced from 400KB to 13KB at root level

### Principle
**Planning belongs in GitHub issues and coordination board, not in markdown files.**

Static plan documents should only exist for:
1. Strategic initiatives (like WASM web port) with active commits
2. Infrastructure work with concrete phases and file references
3. Historical reference after completion

Speculative planning should use:
- GitHub Discussions for RFCs
- Issues with labels for feature requests
- Coordination board for multi-agent work tracking

## Files in Archive

```
docs/internal/agents/archive/plans-2025-11/
├── CLEANUP_SUMMARY.md
├── dungeon_editor_ui_refactor.md
├── emulator-debug-api-design.md
├── hex_editor_enhancements.md
└── message_editor_implementation_roadmap.md
```

These are available if needed as historical context but should not be referenced for active development.
