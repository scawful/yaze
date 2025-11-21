## Inter-Agent Collaboration Protocol

Multiple assistants may work in this repository at the same time. To avoid conflicts, every agent
must follow the shared protocol defined in
[`docs/internal/agents/coordination-board.md`](docs/internal/agents/coordination-board.md).

### Required Steps
1. **Read the board** before starting a task to understand active work, blockers, or pending
   requests.
2. **Append a new entry** (format described in the coordination board) outlining your intent,
   affected files, and any dependencies.
3. **Respond to requests** addressed to your agent ID before taking on new work whenever possible.
4. **Record completion or handoffs** so the next agent has a clear state snapshot.
5. For multi-day initiatives, fill out the template in
   [`docs/internal/agents/initiative-template.md`](docs/internal/agents/initiative-template.md) and
   link it from your board entry instead of duplicating long notes.

### Agent IDs
Use the following canonical identifiers in board entries and handoffs (see
[`docs/internal/agents/personas.md`](docs/internal/agents/personas.md) for details):

| Agent ID        | Description                                      |
|-----------------|--------------------------------------------------|
| `CLAUDE_CORE`   | Claude agent handling general editor/engine work |
| `CLAUDE_AIINF`  | Claude agent focused on AI/agent infrastructure   |
| `CLAUDE_DOCS`   | Claude agent dedicated to docs/product guidance   |
| `GEMINI_FLASH_AUTOM` | Gemini agent focused on automation/CLI/test work |
| `CODEX`         | This Codex CLI assistant                          |
| `OTHER`         | Any future agent (define in entry)                |

If you introduce a new agent persona, add it to the table along with a short description.

### Helper Scripts
Common automation helpers live under [`scripts/agents/`](scripts/agents). Use them whenever possible:
- `run-gh-workflow.sh` – trigger GitHub workflows (`ci.yml`, etc.) with parameters such as `enable_http_api_tests`.
- `smoke-build.sh` – configure/build a preset in place and report how long it took.
- `run-tests.sh` – configure/build a preset and run `ctest` (`scripts/agents/run-tests.sh mac-dbg --output-on-failure`).
- `test-http-api.sh` – poll the `/api/v1/health` endpoint once the HTTP server is running.

Log command results and workflow URLs on the coordination board so other agents know what ran and where to find artifacts.

### Escalation
If two agents need the same subsystem concurrently, negotiate via the board using the
`REQUEST`/`BLOCKER` keywords. When in doubt, prefer smaller, well-defined handoffs instead of broad
claims over directories.
