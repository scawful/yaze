# Active Initiatives

This directory contains detailed planning documents for multi-day development initiatives.

## Current Initiatives

### Features & Enhancements

- **[UI/UX Improvements](ui-ux-improvements.md)** - Comprehensive UI/UX refresh plan including shortcuts, status indicators, theming, and accessibility improvements
- **[Sprite Systems Reference](sprite-systems-reference.md)** - Complete sprite metadata system, reference documentation, and editor enhancements
- **[z3ed CLI Enhancements](z3ed-cli-enhancements.md)** - z3ed Hive Mode implementation and advanced CLI workflows
- **[Feature Parity Analysis](feature-parity-analysis.md)** - Competitive analysis against Zarby and other editors

## Related Documents

- [Main Roadmap](../roadmap.md) - Strategic roadmap for all yaze versions
- [Future Improvements](../future-improvements.md) - Long-term vision and ideas
- [Code Review Critical Next Steps](../code-review-critical-next-steps.md) - Priority code quality items
- [Research Directory](../../research/) - Technical feasibility studies
- [Coordination Board](../../agents/coordination-board.md) - Active work tracking

## Initiative Lifecycle

### 1. Proposal
- Create initiative doc with problem statement, proposed solution, and effort estimate
- Link from coordination board with `REQUEST → ALL` for feedback
- Gather consensus via council vote if needed

### 2. Active Development
- Update initiative doc with progress, blockers, and design decisions
- Reference initiative from coordination board entries
- Keep scope focused - split into multiple initiatives if needed

### 3. Completion
- Mark initiative as complete with summary of outcomes
- Archive to coordination-board-archive.md
- Move doc to `docs/internal/blueprints/completed/` if substantial architectural reference

## Creating New Initiatives

**When to create an initiative doc:**
- Work will span multiple days or sessions
- Requires coordination between multiple agents
- Involves architectural decisions or significant changes
- Needs persistent reference for handoffs

**Template:**
```markdown
# [Initiative Name]

**Status:** Proposal | Active | Blocked | Complete
**Owner:** [Agent persona]
**Start Date:** YYYY-MM-DD
**Target Completion:** YYYY-MM-DD

## Problem Statement
[What needs to be solved or improved?]

## Proposed Solution
[High-level approach]

## Scope
- In scope: [What this initiative covers]
- Out of scope: [What this explicitly does not cover]

## Implementation Plan
1. [Phase 1]
2. [Phase 2]
...

## Success Criteria
- [ ] Criterion 1
- [ ] Criterion 2

## Dependencies
- [Other initiatives, features, or blockers]

## Progress Log
### [Date] - [Agent]
[Progress update]

## Decisions
### [Date] - [Decision Title]
**Decision:** [What was decided]
**Rationale:** [Why]
**Alternatives Considered:** [What else was considered]
```

## Archive Policy

Initiatives are archived when:
- Completed and merged
- Superseded by new approach
- Deferred beyond current roadmap horizon (v1.0+)

Archived initiatives move to coordination-board-archive.md with final status summary.
