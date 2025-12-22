# Documentation Cleanup - November 27, 2025

## Summary

Comprehensive review and update of YAZE documentation, focusing on public-facing docs, web app support, and organizational cleanup.

## Changes Made

### 1. Web App Documentation

**Created: `docs/public/usage/web-app.md`**
- Comprehensive guide for the WASM web application
- Clearly marked as **Preview** status (not production-ready)
- Detailed feature status table showing incomplete editors
- Browser requirements and compatibility
- Performance tips and troubleshooting
- Comparison table: Web vs Desktop
- Developer tools and API references
- Deployment instructions
- Privacy and storage information

**Key Points:**
- ⚠️ Emphasized preview/experimental status throughout
- Listed editor completeness accurately (Preview/Incomplete vs Working)
- Recommended desktop build for serious ROM hacking
- Linked to internal technical docs for developers

### 2. Main README Updates

**Updated: `README.md`**
- Added web preview mention in highlights section
- Added "Web App (Preview)" to Applications & Workflows
- Clearly linked to web-app.md guide
- Maintained focus on desktop as primary platform

### 3. Public Docs Index

**Updated: `docs/public/index.md`**
- Added Web App (Preview) to Usage Guides section
- Placed at top for visibility

### 4. Directory Organization

**Moved technical implementation docs to internal:**
- `docs/web/drag-drop-rom-loading.md` → `docs/internal/web-drag-drop-implementation.md`
- `docs/wasm/patch_export.md` → `docs/internal/wasm-patch-export-implementation.md`
- Removed empty `docs/web/` and `docs/wasm/` directories

**Organized format documentation:**
Moved to `docs/public/reference/` for better discoverability:
- `SAVE_STATE_FORMAT.md`
- `SNES_COMPRESSION.md`
- `SNES_GRAPHICS.md`
- `SYMBOL_FORMAT.md`
- `ZSM_FORMAT.md`

**Updated: `docs/public/reference/rom-reference.md`**
- Added "Additional Format Documentation" section
- Linked to all format specification docs
- Updated last modified date to November 27, 2025

### 5. Documentation Accuracy

**Updated: `docs/public/build/platform-compatibility.md`**
- Updated "Last Updated" from October 9, 2025 to November 27, 2025

**Reviewed for accuracy:**
- ✅ `docs/public/build/quick-reference.md` - Accurate
- ✅ `docs/public/build/build-from-source.md` - Accurate
- ✅ `docs/public/build/presets.md` - Accurate
- ✅ `docs/public/developer/architecture.md` - Accurate (updated Nov 2025)
- ✅ `docs/public/developer/testing-quick-start.md` - Accurate

### 6. Coordination Board

**Updated: `docs/internal/agents/coordination-board.md`**
- Added entry for docs-janitor work session
- Marked status as COMPLETE
- Listed all changes made

## File Structure After Cleanup

```
docs/
├── public/
│   ├── build/           [5 docs - build system]
│   ├── deployment/      [1 doc - collaboration server]
│   ├── developer/       [18 docs - developer guides]
│   ├── examples/        [1 doc - code examples]
│   ├── guides/          [1 doc - z3ed workflows]
│   ├── overview/        [1 doc - getting started]
│   ├── reference/       [8 docs - ROM & format specs] ⭐ IMPROVED
│   │   ├── rom-reference.md
│   │   ├── SAVE_STATE_FORMAT.md     ⬅️ MOVED HERE
│   │   ├── SNES_COMPRESSION.md      ⬅️ MOVED HERE
│   │   ├── SNES_GRAPHICS.md         ⬅️ MOVED HERE
│   │   ├── SYMBOL_FORMAT.md         ⬅️ MOVED HERE
│   │   └── ZSM_FORMAT.md            ⬅️ MOVED HERE
│   ├── usage/           [4 docs including web-app] ⭐ NEW
│   │   ├── web-app.md               ⬅️ NEW
│   │   ├── dungeon-editor.md
│   │   ├── overworld-loading.md
│   │   └── z3ed-cli.md
│   ├── index.md
│   └── README.md
├── internal/
│   ├── agents/          [Agent coordination & playbooks]
│   ├── architecture/    [System architecture docs]
│   ├── blueprints/      [Refactoring plans]
│   ├── plans/           [Implementation plans]
│   ├── reports/         [Investigation reports]
│   ├── roadmaps/        [Feature roadmaps]
│   ├── testing/         [Test infrastructure]
│   ├── web-drag-drop-implementation.md  ⬅️ MOVED HERE
│   ├── wasm-patch-export-implementation.md  ⬅️ MOVED HERE
│   └── [other internal docs]
├── examples/            [Code examples]
├── GIGALEAK_INTEGRATION.md
└── index.md
```

## Removed Directories

- ❌ `docs/web/` - consolidated into internal
- ❌ `docs/wasm/` - consolidated into internal

## Documentation Principles Applied

1. **Public vs Internal Separation**
   - Public: User-facing, stable, external developers
   - Internal: AI agents, implementation details, planning

2. **Accuracy & Honesty**
   - Web app clearly marked as preview/experimental
   - Editor status accurately reflects incomplete state
   - Recommended desktop for production work

3. **Organization**
   - Format docs in reference section for easy discovery
   - Technical implementation in internal for developers
   - Clear navigation through index files

4. **Currency**
   - Updated "Last Modified" dates
   - Removed outdated content
   - Consolidated duplicate information

## Impact

### For Users
- ✅ Clear understanding that web app is preview
- ✅ Easy access to format documentation
- ✅ Better organized public docs
- ✅ Honest feature status

### For Developers
- ✅ Technical docs in predictable locations
- ✅ Format specs easy to find in reference/
- ✅ Implementation details separated from user guides
- ✅ Clear documentation hierarchy

### For AI Agents
- ✅ Updated coordination board with session
- ✅ Clear doc hygiene maintained
- ✅ No doc sprawl in root directories

## Follow-up Actions

None required. Documentation is now:
- Organized
- Accurate
- Complete for web app preview
- Properly separated (public vs internal)
- Up to date

## Agent

**Agent ID:** docs-janitor  
**Session Date:** November 27, 2025  
**Duration:** Single session  
**Status:** Complete

