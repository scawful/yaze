# Agent Documentation Audit Report

**Audit Date**: 2025-11-23
**Auditor**: CLAUDE_DOCS (Documentation Janitor)
**Total Files Reviewed**: 30 markdown files
**Total Size**: 9,149 lines, ~175KB

---

## Executive Summary

The `/docs/internal/agents/` directory contains valuable agent collaboration infrastructure but has accumulated task-specific documentation that should be **archived** (5 files), **consolidated** (3 file groups), and one **template** file that should remain. The coordination-board.md is oversized (83KB) and needs archival strategy.

**Key Findings**:
- **3 Gemini-specific task prompts** (gemini-master, gemini3-overworld-fix, gemini-task-checklist) are completed or superseded
- **2 Onboarding documents** (COLLABORATION_KICKOFF, CODEX_ONBOARDING) are one-time setup docs for past kickoffs
- **1 Handoff document** (CLAUDE_AIINF_HANDOFF) documents a completed session handoff
- **Core infrastructure documents** (coordination-board, personas, agent-architecture) should remain
- **System reference documents** (gemini-overworld-system-reference, gemini-dungeon-system-reference) are valuable for context
- **Initiative documents** (initiative-v040, initiative-test-slimdown) are active/in-progress

---

## File-by-File Audit Table

| File | Size | Relevance (1-5) | Status | Recommended Action | Justification |
|------|------|-----------------|--------|-------------------|---------------|
| **ACTIVE CORE** |
| coordination-board.md | 83KB | 5 | ACTIVE | ARCHIVE OLD ENTRIES | Live coordination hub; archive entries >2 weeks old to separate file |
| personas.md | 1.8KB | 5 | ACTIVE | KEEP | Defines CLAUDE_CORE, CLAUDE_AIINF, CLAUDE_DOCS, GEMINI_AUTOM personas |
| agent-architecture.md | 14KB | 5 | ACTIVE | KEEP | Foundational reference for agent roles, capabilities, interaction patterns |
| **ACTIVE INITIATIVES** |
| initiative-v040.md | 8.4KB | 5 | ACTIVE | KEEP | Ongoing v0.4.0 development (SDL3, emulator accuracy); linked from coordination-board |
| initiative-test-slimdown.md | 2.3KB | 4 | IN_PROGRESS | KEEP | Scoped test infrastructure work; referenced in coordination-board |
| initiative-template.md | 1.3KB | 4 | ACTIVE | KEEP | Reusable template for future initiatives |
| **ACTIVE COLLABORATION FRAMEWORK** |
| claude-gemini-collaboration.md | 12KB | 4 | ACTIVE | KEEP | Documents Claude-Gemini teamwork structure; still relevant |
| agent-leaderboard.md | 10KB | 3 | SEMI-ACTIVE | CONSIDER ARCHIVING | Gamification artifact from 2025-11-20; update score tracking to coordination-board |
| **GEMINI TASK-SPECIFIC (COMPLETED/SUPERSEDED)** |
| gemini-build-setup.md | 2.2KB | 3 | COMPLETED | ARCHIVE | Build guide for Gemini; superseded by docs/public/build/quick-reference.md |
| gemini-master-prompt.md | 7.1KB | 2 | COMPLETED | ARCHIVE | Session context doc for Gemini session; work is complete (fixed ASM version checks) |
| gemini3-overworld-fix-prompt.md | 5.4KB | 2 | COMPLETED | ARCHIVE | Specific bug fix prompt for overworld regression; issue resolved (commit aed7967e29) |
| gemini-task-checklist.md | 6.5KB | 2 | COMPLETED | ARCHIVE | Gemini task checklist from 2025-11-20 session; all items completed or handed off |
| gemini-overworld-reference.md | 7.0KB | 3 | REFERENCE | CONSOLIDATE | Duplicate info from gemini-overworld-system-reference.md; merge into system-reference |
| **DUNGEON/OVERWORLD SYSTEM REFERENCES** |
| gemini-overworld-system-reference.md | 11KB | 4 | REFERENCE | KEEP | Technical deep-dive for overworld system; valuable ongoing reference |
| gemini-dungeon-system-reference.md | 14KB | 4 | REFERENCE | KEEP | Technical deep-dive for dungeon system; valuable ongoing reference |
| **HANDOFF DOCUMENTS** |
| CLAUDE_AIINF_HANDOFF.md | 7.3KB | 2 | COMPLETED | ARCHIVE | Session handoff from 2025-11-20; work documented in coordination-board |
| **ONE-TIME SETUP/KICKOFF** |
| COLLABORATION_KICKOFF.md | 5.3KB | 2 | COMPLETED | ARCHIVE | Kickoff for Claude-Gemini collaboration; framework now in place |
| CODEX_ONBOARDING.md | 5.9KB | 2 | COMPLETED | ARCHIVE | Onboarding guide for Codex agent; role is now established |
| **DEVELOPMENT GUIDES** |
| overworld-agent-guide.md | 14KB | 3 | REFERENCE | CONSOLIDATE | Overlaps with gemini-overworld-system-reference.md; merge content |
| ai-agent-debugging-guide.md | 22KB | 4 | ACTIVE | KEEP | Comprehensive debug reference for AI agents working on yaze |
| ai-development-tools.md | 17KB | 4 | ACTIVE | KEEP | Development tools reference for AI agents |
| ai-infrastructure-initiative.md | 11KB | 3 | REFERENCE | CONSIDER ARCHIVING | Infrastructure planning doc; mostly superseded by active initiatives |
| **Z3ED DOCUMENTATION** |
| z3ed-command-abstraction.md | 15KB | 3 | REFERENCE | CONSOLIDATE | CLI refactoring reference; merge with z3ed-refactoring.md |
| z3ed-refactoring.md | 9.7KB | 3 | REFERENCE | CONSOLIDATE | CLI refactoring summary; merge with command-abstraction.md |
| **INFRASTRUCTURE** |
| CI-TEST-AUDIT-REPORT.md | 5.6KB | 2 | COMPLETED | ARCHIVE | Test audit from 2025-11-20; findings incorporated into CI/test docs |
| filesystem-tool.md | 6.0KB | 2 | REFERENCE | ARCHIVE | Tool documentation; likely obsolete or incorporated elsewhere |
| dev-assist-agent.md | 8.4KB | 3 | REFERENCE | REVIEW | Development assistant design doc; check if still relevant |
| ai-modularity.md | 6.6KB | 3 | REFERENCE | REVIEW | Modularity initiative doc; check completion status |
| gh-actions-remote.md | 1.6KB | 1 | REFERENCE | DELETE | GitHub Actions remote reference; likely outdated tool documentation |

---

## Consolidation Recommendations

### Group A: Gemini Overworld References (CONSOLIDATE)
**Files to Merge**:
- `gemini-overworld-reference.md` (7.0KB)
- `gemini-overworld-system-reference.md` (11KB)
- `overworld-agent-guide.md` (14KB)

**Action**: Keep `gemini-overworld-system-reference.md` as the authoritative reference. Archive the other two files.

**Reasoning**: All three files cover similar ground (overworld architecture, file structure, data models). System-reference is most comprehensive and is actively used by agents.

---

### Group B: z3ed Refactoring References (CONSOLIDATE)
**Files to Merge**:
- `z3ed-command-abstraction.md` (15KB)
- `z3ed-refactoring.md` (9.7KB)

**Action**: Keep `z3ed-refactoring.md` as the summary. Move detailed command abstraction specifics to a new `z3ed-implementation-details.md` in `docs/internal/` (not agents/).

**Reasoning**: Refactoring is complete, but CLI architecture docs are valuable for future CLI work. Separate implementation details from agent coordination.

---

### Group C: Gemini Task-Specific Prompts (ARCHIVE)
**Files to Archive**:
- `gemini-master-prompt.md` (7.1KB)
- `gemini3-overworld-fix-prompt.md` (5.4KB)
- `gemini-task-checklist.md` (6.5KB)
- `gemini-build-setup.md` (2.2KB)

**Action**: Move to `docs/internal/agents/archive/gemini-session-2025-11-20/` with a README explaining the session context.

**Reasoning**: These are session-specific task documents. The work they document is complete. They're valuable for understanding past sessions but shouldn't clutter the active agent docs directory.

---

### Group D: Session Handoffs & Kickoffs (ARCHIVE)
**Files to Archive**:
- `CLAUDE_AIINF_HANDOFF.md` (7.3KB)
- `COLLABORATION_KICKOFF.md` (5.3KB)
- `CODEX_ONBOARDING.md` (5.9KB)

**Action**: Move to `docs/internal/agents/archive/session-handoffs/` with dates in filenames.

**Reasoning**: One-time setup documents. The collaboration framework is now established and documented in `claude-gemini-collaboration.md`. Handoff information has been integrated into coordination-board.

---

### Group E: Completed Audits & Reports (ARCHIVE)
**Files to Archive**:
- `CI-TEST-AUDIT-REPORT.md` (5.6KB)

**Action**: Move to `docs/internal/agents/archive/reports/` with date.

**Reasoning**: Audit findings have been incorporated into active test documentation. The report itself is a historical artifact.

---

## Immediate Actions

### Priority 1: Resolve Coordination Board Size (CRITICAL)
**Current**: 83KB file makes it unwieldy
**Action**:
1. Create `coordination-board-archive.md` in same directory
2. Move entries older than 2 weeks to archive (keep last 60-80 entries, ~40KB max)
3. Update coordination-board.md header with note about archival strategy
4. Create a script to automate monthly archival

**Expected Result**: Faster file loads, easier to find current work

---

### Priority 2: Create Archive Structure
```
docs/internal/agents/
├── archive/
│   ├── gemini-session-2025-11-20/
│   │   ├── README.md (context)
│   │   ├── gemini-master-prompt.md
│   │   ├── gemini3-overworld-fix-prompt.md
│   │   ├── gemini-task-checklist.md
│   │   └── gemini-build-setup.md
│   ├── session-handoffs/
│   │   ├── 2025-11-20-CLAUDE_AIINF_HANDOFF.md
│   │   ├── 2025-11-20-COLLABORATION_KICKOFF.md
│   │   └── 2025-11-20-CODEX_ONBOARDING.md
│   └── reports/
│       └── 2025-11-20-CI-TEST-AUDIT-REPORT.md
```

---

### Priority 3: Consolidate Overlapping Documents
1. **Merge overworld guides**: Keep `gemini-overworld-system-reference.md`, archive others
2. **Merge z3ed docs**: Keep `z3ed-refactoring.md`, consolidate implementation details
3. **Review low-relevance files**: Check `dev-assist-agent.md`, `ai-modularity.md`, `ai-infrastructure-initiative.md`

---

## Files to Keep (Active Core)

These files should remain in `/docs/internal/agents/` with no changes:

| File | Reason |
|------|--------|
| `coordination-board.md` | Live coordination hub (after archival cleanup) |
| `personas.md` | Defines agent roles and responsibilities |
| `agent-architecture.md` | Foundational reference for agent systems |
| `initiative-v040.md` | Active development initiative |
| `initiative-test-slimdown.md` | Active development initiative |
| `initiative-template.md` | Reusable template for future work |
| `claude-gemini-collaboration.md` | Active team collaboration framework |
| `gemini-overworld-system-reference.md` | Technical deep-dive (widely used) |
| `gemini-dungeon-system-reference.md` | Technical deep-dive (widely used) |
| `ai-agent-debugging-guide.md` | Debugging reference for agents |
| `ai-development-tools.md` | Development tools reference |

---

## Files Requiring Further Review

These files need owner confirmation before archival:

| File | Status | Recommendation |
|------|--------|-----------------|
| `dev-assist-agent.md` | UNCLEAR | Contact owner to confirm if still active |
| `ai-modularity.md` | UNCLEAR | Check if modularity initiative is complete |
| `ai-infrastructure-initiative.md` | SEMI-ACTIVE | May be superseded by v0.4.0 initiative |
| `agent-leaderboard.md` | SEMI-ACTIVE | Consider moving gamification tracking to coordination-board |
| `gh-actions-remote.md` | LIKELY-OBSOLETE | Very small file; verify it's not actively referenced |

---

## Summary of Recommendations

### Files to Archive (7-8 files, ~45KB)
- Gemini task-specific prompts (4 files)
- Session handoffs and kickoffs (3 files)
- Completed audit reports (1 file)

### Files to Consolidate (3 groups)
- Overworld references: Keep system-reference.md, archive others
- Z3ed references: Merge into single document
- Review low-relevance infrastructure initiatives

### Files to Keep (11 files, ~80KB)
- Core coordination and architecture files
- Active initiatives
- Technical deep-dives used by agents

### Structure Improvement
- Create `archive/` subdirectory with documented substructure
- Establish coordination-board.md archival strategy
- Aim for <100KB total in active agents directory

---

## Implementation Timeline

**Week 1 (Nov 23-29)**:
- [ ] Create archive directory structure
- [ ] Move files to archive (priority: task-specific prompts)
- [ ] Update cross-references in remaining docs

**Week 2 (Nov 30-Dec 6)**:
- [ ] Archive coordination-board entries (manual or scripted)
- [ ] Consolidate overlapping system references
- [ ] Review unclear files with owners

**Ongoing**:
- [ ] Implement monthly coordination-board.md archival
- [ ] Keep active initiatives up-to-date

---

## Notes for Future Archival

When adding new agent documentation:

1. **Session-specific docs** (task prompts, handoffs, prompts) → Archive after completion
2. **One-time setup docs** (kickoffs, onboarding) → Archive after 2 weeks
3. **Active infrastructure** (coordination, personas, initiatives) → Keep in root
4. **System references** (architecture, debugging) → Keep if actively used by agents
5. **Completed work reports** → Archive with date in filename

**Target**: Keep `/docs/internal/agents/` under 100KB with ~15-20 active files. Move everything else to `archive/`.

