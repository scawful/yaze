## Inter-Agent Protocol (Lean)

**Quick tasks (<30 min):** Skip the board. Just do the work and commit with a clear message.

**Substantial work (>30 min or multi-file):**
1) **Check for blockers** - Scan `docs/internal/agents/coordination-board.md` for `REQUEST`/`BLOCKER` tags
2) **Claim if overlapping** - Only post if your work touches files another agent is actively editing
3) **Record milestones** - Post completion notes for significant features (not routine fixes)

**Multi-day initiatives:** Use `docs/internal/agents/initiative-template.md` to track progress separately from the board.

**Board hygiene:** Keep entries concise (≤5 lines). Archive completed work weekly. Target ≤40 active entries.

## Agent IDs (shared with Oracle-of-Secrets/.claude/agents)
Use these canonical IDs (scopes in `docs/internal/agents/personas.md` and `.claude/agents/*`):

| Agent ID                   | Focus                                                  |
|----------------------------|--------------------------------------------------------|
| `ai-infra-architect`       | AI/agent infra, z3ed CLI/TUI, gRPC/network             |
| `backend-infra-engineer`   | Build/packaging, CMake/toolchains, CI reliability      |
| `docs-janitor`             | Docs, onboarding, release notes, process hygiene       |
| `imgui-frontend-engineer`  | ImGui/renderer/UI systems                              |
| `snes-emulator-expert`     | Emulator core (CPU/APU/PPU), perf/debugging            |
| `test-infrastructure-expert` | Test harness, CTest/gMock, flake triage              |
| `zelda3-hacking-expert`    | Gameplay/ROM logic, Zelda3 data model                  |
| `GEMINI_FLASH_AUTOM`       | Gemini automation/CLI/tests                            |
| `CODEX`                    | Codex CLI assistant                                    |
| `OTHER`                    | Define in entry                                        |

Legacy aliases (`CLAUDE_CORE`, `CLAUDE_AIINF`, `CLAUDE_DOCS`) → use `imgui-frontend-engineer`/`snes-emulator-expert`/`zelda3-hacking-expert`, `ai-infra-architect`, and `docs-janitor`.

## Helper Scripts (keep it short)
Located in `scripts/agents/`:
- `run-gh-workflow.sh`, `smoke-build.sh`, `run-tests.sh`, `test-http-api.sh`
Log command results + workflow URLs on the board for traceability.

## Escalation
If overlapping on a subsystem, post `REQUEST`/`BLOCKER` on the board and coordinate; prefer small, well-defined handoffs.
