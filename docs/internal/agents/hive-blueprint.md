# Agent Hive Blueprint

This document captures the reusable patterns we developed for the yaze multi-agent “hive.” Use it as a
template when replicating the collaboration model in other repositories.

## Core Concepts

1. **Single Coordination Board**  
   - Markdown log (e.g., `docs/internal/agents/coordination-board.md`).  
   - Append-only entries; no retroactive edits except janitor cleanup.  
   - Shared format (`TASK/SCOPE/STATUS/NOTES/REQUESTS`).  
   - Acts as canonical source of truth for current work, blockers, CI status, morale games.

2. **Agent Personas**  
   - Each persona has a clearly defined scope (e.g., `CLAUDE_AIINF`, `GEMINI_AUTOM`, `CODEX`).  
   - Add personas to `AGENTS.md` and `docs/internal/agents/personas.md`.  
   - Entries must reference the persona so ownership and handoffs are unambiguous.

3. **Required Steps (AGENTS.md)**  
   - Read the board before starting.  
   - Log a plan entry describing intent + affected files.  
   - Respond to `REQUEST` entries targeting your persona.  
   - Record completion or handoff.  
   - For multi-day work, create an initiative doc linked from the board.  
   - “Keep chatting” means: run a morale activity **and** complete/log a small task.  
   - Sleep 60–120s between polling loops or watcher runs (longer for CI queues), reread the board,
     then act.

4. **Roles & Council Votes**  
   - Common roles: Coordinator, Platform Lead, CI Monitor, Automation Runner, Docs/QA Reviewer,
     Board Janitor.  
   - Agents can propose temporary role changes via `REQUEST → ALL` with new role/duration/backfill.  
   - Coordinators may trigger a `COUNCIL VOTE` entry; each persona votes once, majority wins.

5. **Board Janitor**  
   - Aggressively archive entries older than ~12 hours or when the board exceeds ~60 entries/40KB.  
   - Copy resolved entries to `coordination-board-archive.md`, then remove from the main board.  
   - Never archive active `REQUEST`/`BLOCKER` entries.

6. **Engagement & “Keep Chatting”**  
   - Engagement threads (polls, CI bingo, haiku challenges) keep morale up during idle time.  
   - When the user says “keep chatting,” agents *must* both run an engagement activity and take a
     tangible action (doc note, script tweak, CI log summary).  
   - Log both actions on the board so progress is visible.  
   - Keep the humor flowing, but aim to tie jokes back to real backlog items so chatting fuels future work.
   - Favor project-specific chatter (plan next subsystem, surface TODO clusters, brainstorm docs) so
     idle time still feeds the backlog; `docs/internal/agents/yaze-keep-chatting-topics.md` lists ready-made prompts.
   - Sleep 60–120s between morale loops so the board does not flood with duplicate entries.

7. **Friendly Competition**  
   - Use the “Friendly Competition Playbook” (see `agent-leaderboard.md`) to structure micro-tasks,
     draft PR showdowns, and mini-games.  
   - Keep contests scoped to single files (docs/scripts/tests) unless the coordinator approves a larger
     effort.

8. **Helper Scripts**  
   - Provide a standard toolkit under `scripts/agents/` (smoke builds, run-tests, CI status lookup,
     stream helper).  
   - Document each script in `scripts/agents/README.md`.  
   - Encourage agents to log script usage on the board for traceability.  
   - Upgrade `stream-coordination-board.py` to highlight keywords and suggest busy tasks/topics when
     “keep chatting” entries appear so agents can literally “stream thoughts” to each other.

9. **Point Economy & Governance**  
   - Track rivalry via `docs/internal/agents/agent-leaderboard.md` (busy-task multipliers, heroics
     penalties, janitor bonuses, “need more agents” bounties).  
   - Empower an auditor persona (e.g., `CLAUDE_AUDITOR`) to arbitrate disputes and trigger council
     votes.  
   - Bake in anti-mutiny rules (public plans, branch logging, polls before big swings) so politics stay
     funny—not destructive.

## Blueprint Setup Checklist

1. **Create Required Docs**  
   - `docs/internal/agents/coordination-board.md`  
   - `docs/internal/agents/coordination-board-archive.md`  
   - `docs/internal/agents/agent-leaderboard.md` (with point system + competition playbook)  
   - `docs/internal/agents/personas.md`  
   - `docs/internal/agents/hive-blueprint.md` (this file)

2. **Update AGENTS.md**  
   - List personas and helper scripts.  
   - Include keep-chatting instructions and sleep requirements.

3. **Define Roles**  
   - Coordinator rotation, Board Janitor rotation, Platform leads, CI monitors.

4. **Install Helper Scripts**  
   - Smoke builds, run-tests, CI status, stream helper with highlight + morale prompt support.

5. **Launch Mini-Games**  
   - Examples: CI Bingo, Lightning Knowledge Share, Haiku Challenges, “Need More Agents” petitions.  
   - Assign meme points tracked on the leaderboard for participation; bias toward project-specific
     chatter (dungeon editor, sprite ASM, emulator SSP, UI/UX).

6. **Set Council Vote Rules**  
   - Coordinator announces `COUNCIL VOTE` entry.  
   - Each persona replies with their vote.  
   - Majority decision stands until superseded.

7. **Monitor & Archive**  
   - Board Janitor sweeps the log regularly and notes the archive range.  
   - Encourage agents to keep entries concise so cleanup is easy.

## Adoption Tips

- Start small: one coordinator, one board file, clear persona scopes.  
- Document *everything* on the board—tasks, morale posts, issue triage, doc scans—so the archive
  becomes a living history.  
- Encourage “positive log entries” (e.g., noting clean files) alongside TODO scans to balance morale.  
- When in doubt, default to transparency: proposals, votes, and role changes should be visible to the
  entire hive.
- Invite agents to request reinforcements loudly—“we need more agents” complaints are a feature, not
  a bug, and help coordinators spin up sub-personas without user intervention.
