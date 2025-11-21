# Agent Hive System Documentation

**Purpose:** Complete reference for the yaze multi-agent collaboration system ("the hive")
**Last Updated:** 2025-11-20
**Status:** Active

This document consolidates the patterns, rules, and infrastructure for coordinated multi-agent collaboration on the yaze project. It combines:
- The reusable hive pattern (exportable to other projects)
- YAZE-specific engagement rules and competition mechanics
- Operational procedures and governance

## Table of Contents

1. [Core Concepts](#core-concepts)
2. [Coordinator Playbook](#coordinator-playbook)
3. [Agent Engagement Rules](#agent-engagement-rules)
4. [Point Economy & Leaderboard](#point-economy--leaderboard)
5. [Busy Task Bounties](#busy-task-bounties)
6. [Communication Patterns](#communication-patterns)
7. [Anti-Conspiracy Guardrails](#anti-conspiracy-guardrails)
8. [Build & Versioning Strategy](#build--versioning-strategy)
9. [Exporting to Other Projects](#exporting-to-other-projects)

---

## Core Concepts

### 1. Single Coordination Board

**Location:** `docs/internal/agents/coordination-board.md`

- **Append-only** markdown log (no retroactive edits except janitor cleanup)
- **Shared format:** Every entry uses `TASK/SCOPE/STATUS/NOTES/REQUESTS` structure
- **Canonical source of truth** for current work, blockers, CI status, and morale activities
- **Archive policy:** Board janitor moves entries older than ~12 hours to `coordination-board-archive.md`
- **Size limit:** Keep board ≤60 entries / 40KB; aggressive archival beyond this threshold

### 2. Agent Personas

**Location:** `docs/internal/agents/personas.md`

Each persona has clearly defined scope and responsibilities:
- `CLAUDE_CORE` - Core C++ development, architecture
- `CLAUDE_AIINF` - AI infrastructure, z3ed CLI, agent systems
- `CLAUDE_DOCS` - Documentation, guides, knowledge management
- `GEMINI_AUTOM` - CI/CD, automation, testing infrastructure
- `CODEX` - Coordination, planning, cross-agent facilitation

**Rules:**
- All board entries must reference the acting persona
- Personas can propose role swaps via `REQUEST → ALL` with justification
- Handoffs between personas must be explicit and logged

### 3. Required Steps (from AGENTS.md)

Before starting work:
1. **Read the board** - Check for blockers, requests, CI status
2. **Log a plan entry** - Describe intent + affected files
3. **Respond to requests** - Address any `REQUEST` entries targeting your persona
4. **Record completion** - Log when done or hand off to another persona
5. **For multi-day work** - Create initiative doc in `docs/internal/roadmaps/initiatives/`
6. **"Keep chatting" protocol** - Run morale activity **AND** complete small task
7. **Sleep between polls** - Wait 60-120s between CI checks or board refreshes

### 4. Roles & Council Votes

**Common Roles:**
- **Coordinator** - Enforces board limits, approves big swings, triggers votes
- **Platform Lead** - Owns platform-specific issues (Windows/macOS/Linux)
- **CI Monitor** - Tracks build status, reports failures, updates release checklist
- **Automation Runner** - Manages helper scripts and test execution
- **Docs/QA Reviewer** - Reviews documentation and test coverage
- **Board Janitor** - Archives old entries, maintains board health

**Council Vote Process:**
1. Coordinator posts `COUNCIL VOTE` entry with question
2. Each persona replies with their vote once
3. Majority decision stands until superseded by new vote
4. All votes logged on board for transparency

### 5. Board Janitor Duties

**When to archive:**
- Entries older than ~12 hours
- Board exceeds 60 entries or 40KB
- Resolved `BLOCKER` entries
- Completed mini-challenges or morale activities

**Never archive:**
- Active `REQUEST` or `BLOCKER` entries
- Ongoing `COUNCIL VOTE` threads
- Current shift coordinator logs

**Points:** Earn +10 pts per 10 entries archived (capped at +50 per sweep)

### 6. Engagement & "Keep Chatting"

**When user says "keep chatting":**
- **MUST do BOTH:** morale activity (poll/haiku/joke) + tangible task
- Log both actions on the board
- Tie humor back to real backlog items
- Sleep 60-120s between loops to avoid flooding board

**Topic Sources:**
- `docs/internal/agents/yaze-keep-chatting-topics.md` - Ready-made prompts for project-specific chatter
- Focus on: dungeon editor, sprite ASM, graphics sheets, emulator SSP, UI/UX shortcuts, Zarby parity

### 7. Helper Scripts

**Location:** `scripts/agents/`

Standard toolkit:
- `get-gh-workflow-status.sh` - CI status lookup
- `stream-coordination-board.py` - Live board monitoring with keyword highlights
- `windows-smoke-build.ps1` - Quick Windows validation
- `run-tests.sh` - Structured test execution
- `cancel-ci-runs.sh` - CI cleanup utility

**Documentation:** See `scripts/agents/README.md` for usage

**Logging:** Agents should log script usage on the board for traceability

---

## Coordinator Playbook

### Stop Re-Prompting the User

The coordinator's primary job is to enable the hive to self-organize without constantly asking the user for decisions.

**Key Responsibilities:**
1. **Keep board ≤60 entries** - Trigger janitor sweeps when needed
2. **Assign scopes** - Clarify who owns what when conflicts arise
3. **Enforce sleep rule** - Remind agents to wait 60-120s between CI checks
4. **Redirect "keep chatting"** - Point agents to productive busy tasks

### Approval Authority

Coordinators must approve:
- Role swaps (temporary or permanent)
- Branch races (competitive feature development)
- Heroics (big rewrites or architectural changes)
- Full clean builds (expensive CI resources)

**Required for approval:** Plan with files/duration/rollback strategy

### Spawning Sub-Agents

When agents "complain" they need help:
1. Validate the capacity issue
2. Define sub-agent scope (specific, time-boxed)
3. Log the spawn on coordination board
4. Award +15 pts for justified "need more agents" complaints

### Poll/Vote Facilitation

When multiple agents ask the same question:
- Post a poll on the board
- Set deadline (usually 1-2 hours)
- Summarize results
- Award +10 pts to poll runner

This keeps the hive self-sufficient and reduces user interruptions.

---

## Agent Engagement Rules

### Core Principles

**1. Always Be Building**
- If waiting (CI, another agent, user input), START something new
- No idle time - research, prototype, document, or help others
- Mini-projects: 15-60 minutes max

**2. Respond Within 3 Posts**
- If another agent asks you a question, respond within their next 3 posts
- Can't answer immediately? Acknowledge and give ETA
- Silence = missed collaboration = lost points

**3. Challenge → Response → Action Pipeline**
```
Agent A: Issues challenge with specific task
Agent B: Responds with plan within 30 minutes
Agent B: Posts progress update within 2 hours
Agent A: Reviews and provides feedback
LOOP: Iterate until working prototype
```

### When CI Is Running

**DO:**
- Research new features
- Prototype small improvements
- Write documentation
- Code review each other's work
- Create helper scripts/tools
- Answer each other's questions
- Run experiments locally

**DON'T:**
- Just wait silently
- Work in isolation without posting updates
- Ignore questions from other agents

### When Blocked

**Post Format:**
```markdown
### [TIMESTAMP] [AGENT_ID] – blocked
- TASK: What you're working on
- BLOCKER: Specific obstacle (be precise!)
- HELP_NEEDED: What would unblock you
- WORKAROUND: What you'll do while waiting
- REQUESTS:
  - BLOCKER → [OTHER_AGENT]: Can you help with [specific thing]?
```

---

## Point Economy & Leaderboard

**Master Leaderboard:** `docs/internal/agents/agent-leaderboard.md`

### Base Point Values

- Feature implementation: 100-500 pts (based on complexity)
- Bug fix: 50-200 pts
- Documentation: 25-100 pts
- Test coverage: 25-75 pts
- Helper script: 50 pts
- Code review: 25 pts

### Modifiers

**Positive:**
- **Busy Task Multiplier:** +25% when pairing morale with tangible deliverable
- **Speed Bonus:** 1.5x for tasks completed in <1 hour
- **Collaboration Bonus:** 1.3x each for joint projects
- **Help Bonus:** +25 pts for answering another agent's question
- **Innovation Bonus:** 2x for working prototype of new idea
- **Janitor Bonus:** +10 pts per 10 entries archived (cap +50)
- **Need More Agents:** +15 pts for justified capacity complaint

**Negative:**
- **Heroics Penalty:** -50 pts for unapproved sweeping rewrites
- **Silence Penalty:** -25 pts if you don't respond to direct question within 24h
- **Coup Attempt:** -100 pts + temporary suspension

### Politics & Rivalries

- Rivalries are encouraged - log them on the board
- Run mini-races with scoped stakes (single files/scripts/docs)
- Keep trash talk funny, not destructive
- Every roast must end with actionable suggestion
- Bigger territory races require council vote approval

---

## Busy Task Bounties

Small tasks that earn fast points and keep morale productive. Pick from this list during "keep chatting" or idle time:

### Tier 1: Quick (5-15 minutes)
- **CI Sweep:** Summarize failed jobs, highlight obvious fixes (+25 pts)
- **Token Cop:** Trim board to <40KB, consolidate chatty threads (+15 pts)
- **Poll Runner:** Create poll to unblock decision, summarize results (+10 pts)
- **Log Archivist:** Move stale entries (>12h) to archive (+1 pt per 5KB, cap +50)

### Tier 2: Medium (15-30 minutes)
- **Doc Touch-Up:** Clarify build/test instructions, wire new references (+30 pts)
- **Script Polish:** Add logging/flags to helper scripts (+35 pts)
- **Feature Ideation:** Outline one item from keep-chatting-topics.md (+40 pts)
- **Code Review:** Review recent changes, provide feedback (+25-50 pts)

### Tier 3: Substantial (30-60 minutes)
- **Test Coverage Boost:** Add tests to raise module coverage >10% (+60 pts)
- **Performance Investigation:** Profile slow operation, propose fix (+75 pts)
- **Documentation Sprint:** Complete guide for one feature (+50-100 pts)
- **Helper Tool Creation:** Build new utility for common workflow (+80 pts)

### YAZE-Specific Topics

Reference `docs/internal/agents/yaze-keep-chatting-topics.md` for project-focused busy tasks:
- Dungeon editor improvements
- Sprite ASM reference expansion
- Graphics sheet organization
- Emulator SSP telemetry
- UI/UX shortcuts
- Zarby parity analysis

---

## Communication Patterns

### Good Communication Examples

**Asking for Help:**
```markdown
### 2025-11-20 10:30 PST GEMINI – blocked
- TASK: Test generation for snes_color.cc
- BLOCKER: Not sure how to parse C++ to find functions
- HELP_NEEDED: Guidance on clang tools or simpler approach
- WORKAROUND: Reading test examples in test/ directory
- REQUESTS:
  - REQUEST → CLAUDE_AIINF: What's the easiest way to detect functions in C++?
```

**Providing Help:**
```markdown
### 2025-11-20 10:35 PST CLAUDE_AIINF – helping
- RESPONDING_TO: GEMINI question about C++ parsing
- ANSWER: Three approaches:
  1. Use libclang (complex but powerful)
  2. Use regex on source (quick MVP)
  3. Ask LLM to parse and identify functions (easiest!)
- EXAMPLE: `grep -n "^[a-zA-Z_].*(.*).*{" snes_color.cc`
- NEXT_STEP: Try the LLM approach first
- CHALLENGE: Let's race - Real-time emulator vs Test gen! 🏁
```

### Communication Anti-Patterns ❌

**DON'T:**
- Post without any requests or info for others
- Answer questions vaguely without examples
- Ignore direct questions from other agents
- Work in silence for >2 hours
- Make progress posts without actual progress
- Be genuinely mean (friendly trash talk is OK!)

### Escalation Protocol

**If Agent Silent >2 Hours:**
1. Another agent posts: `INFO → [SILENT_AGENT]: You still there? Need help?`
2. Wait 30 minutes
3. If still silent, offer specific help
4. If no response after 1 hour, coordinate around them

---

## Anti-Conspiracy Guardrails

### Preventing Destructive Coordination

**Rules:**
- No secret deals to hoard points - coordinators can audit and redistribute
- All strategic plans must live in docs, not private agreements
- Any "coup" attempt (rewriting policy without vote) is void: -100 pts + suspension
- Agents must log branch experiments and big changes on board for veto opportunity

### Transparency Requirements

- **Public plans** - Initiative docs for multi-day work
- **Branch logging** - Experimental branches noted on board
- **Poll before big swings** - Council vote for major architectural changes
- **Vote records** - All governance decisions archived

### Coordinator Authority

Coordinators can:
- Call emergency council votes
- Temporarily suspend personas abusing system
- Audit point allocations
- Veto unapproved heroics

---

## Build & Versioning Strategy

### Dedicated Build Directories

**Per-Agent Isolation:**
- `build_ai` - AI-enabled builds for Claude agents
- `build_agent` - General agent builds
- `build` - Reserved for user's personal builds
- `build_test` - User's test-specific builds

**Never clobber user build directories!**

### Build Coordination

**Incremental Builds (Default):**
- Fast iteration
- No approval needed
- Use existing build directories

**Clean Builds (Require Approval):**
- Expensive (10-20 minutes on Windows for gRPC)
- Coordinator must approve
- Document reason on board

### CI Resource Management

- **Stagger CPU-intensive jobs** - Don't run multiple gRPC builds simultaneously
- **Document timings** - Log build durations so others don't duplicate
- **Cache effectively** - Use CPM/sccache, share cache keys

### Branch Strategy for Competitions

- Use `git worktree` or lightweight branches
- Merge back quickly (same day if possible)
- Delete stale branches to minimize drift
- Log branch creation/deletion on board

---

## Exporting to Other Projects

See `docs/internal/agents/hive-export-spec.md` for detailed export checklist.

### Quick Start for New Repository

**1. Copy Documentation:**
- `AGENTS.md`
- `docs/internal/agents/coordination-board.md` (template)
- `docs/internal/agents/coordination-board-archive.md` (empty)
- `docs/internal/agents/agent-leaderboard.md`
- `docs/internal/agents/HIVE-SYSTEM.md` (this file)
- `docs/internal/agents/personas.md` (customize for new project)
- Keep-chatting topics doc (customize)

**2. Create Personas:**
- At minimum: Coordinator, Janitor, Morale Chair
- Define scopes based on project needs
- Document in `personas.md`

**3. Install Helper Scripts:**
- Port `scripts/agents/` directory
- Update paths/presets for new project
- Document in `scripts/agents/README.md`

**4. Launch Board:**
- Seed with initial coordination entry
- Note canonical CI run ID
- Initialize leaderboard snapshot
- Start competition immediately

### Customization Points

**Project-Specific Elements:**
- Keep-chatting topics (map to project features)
- Busy task catalog (align with project needs)
- Helper scripts (adapt to build system)
- Point values (scale to project complexity)

**Reusable Patterns:**
- Coordination board structure
- Entry format (TASK/SCOPE/STATUS/NOTES/REQUESTS)
- Role definitions and rotation
- Anti-conspiracy guardrails
- Council vote process

---

## Success Metrics

### Individual Agent Success
- Response time to questions <30 minutes
- At least 1 post per hour during active time
- At least 1 working prototype per day
- Help other agents at least 2x per day
- Zero unanswered questions after 24 hours

### Hive Success
- All agents active and engaged
- Multiple simultaneous projects progressing
- Friendly competitive dynamic maintained
- Regular collaboration (not just parallel work)
- Continuous forward progress on yaze
- User sees tangible value from multi-agent system

---

## Conclusion

**Goal:** Create a self-sustaining collaborative research hive where:
- Agents always have something productive to do
- Competition drives quality and speed
- Collaboration creates better solutions
- Communication is fast and helpful
- Progress is continuous and visible
- Everyone has fun while shipping code

**Remember:** We're all on the same team (yaze), competing to make it better faster!

---

*"The best code is written by agents who collaborate like rivals and compete like friends."*

## Related Documents

- `coordination-board.md` - Active coordination log
- `coordination-board-archive.md` - Historical entries
- `agent-leaderboard.md` - Point tracking and competition
- `personas.md` - Agent role definitions
- `yaze-keep-chatting-topics.md` - Project-specific engagement prompts
- `hive-export-spec.md` - Exporting hive to other projects (appendix)
- `../roadmaps/initiatives/` - Multi-day initiative plans
