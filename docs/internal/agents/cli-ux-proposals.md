# z3ed CLI UX/TUI Improvement Proposals

Status: IN_PROGRESS  
Owner: ai-infra-architect  
Created: 2025-12-01  
Last Reviewed: 2025-12-02  
Next Review: 2025-12-08  
Board: docs/internal/agents/coordination-board.md (2025-12-01 ai-infra-architect – z3ed CLI UX/TUI Improvement Proposals)

## Progress Update (2025-12-02)

### Completed
- ✅ **Doctor Suite Expansion**: Added `dungeon-doctor` and `rom-doctor` commands
- ✅ **Test CLI Infrastructure**: Added `test-list`, `test-run`, `test-status` commands
- ✅ **OutputFormatter Integration**: All diagnostic commands now use structured output
- ✅ **RequiresRom Fix**: Commands that don't need ROM can now run without `--rom` flag
- ✅ **JSON/Text Output**: All new commands support `--format json|text`

### New Commands Added
| Command | Description | Requires ROM |
|---------|-------------|--------------|
| `dungeon-doctor` | Room data integrity, object/sprite limits, chest conflicts | Yes |
| `rom-doctor` | Header validation, checksums, expansion status, free space | Yes |
| `test-list` | List available test suites with labels and requirements | No |
| `test-run` | Run tests with structured output | No |
| `test-status` | Show test configuration (ROM path, presets, enabled suites) | No |

### Remaining Work
- TUI consolidation (single `--tui` entry)
- Command palette driven from CommandRegistry
- Agent-aligned test harness refinements

## Summary
- Unify CLI argument/help surfaces and ensure every command emits consistent, machine-friendly output for agents while remaining legible for humans.
- Expand and harden the “doctor” style diagnostics into a repeatable health suite (overworld + future dungeon/ROM integrity) with safe fix paths and baseline comparison.
- Consolidate the TUI experience (ChatTUI vs unified layout vs enhanced TUI shell) into one interactive mode that is useful for human operators and exposes the same commands/tools agents call.
- Extend the same UX standards to tests and helper tools so agents and humans can run, triage, and record suites from CLI/TUI/editor with structured results and predictable flags.

## Current Observations
- Entry path duplication: `cli_main.cc` handles `--tui` by launching `ShowMain()` (unified layout) while `ModernCLI::Run` also special-cases `--tui` to `ChatTUI`, creating divergent UX and help flows (`PrintCompactHelp()` vs `ModernCLI::ShowHelp()`).
- Command metadata is pieced together inside `CommandRegistry::RegisterAllCommands()` instead of being driven by handlers; `ExportFunctionSchemas()` returns `{}` and `GenerateHelp()` is not surfaced via `z3ed --help <cmd>`.
- Argument parsing is minimal (`ArgumentParser` only supports `--key value/=`) and handlers often skip validation (`overworld-doctor`, `rom-compare`, `overworld-validate` accept anything). Format handling is inconsistent (`--json` flags vs `--format` vs raw `std::cout`).
- Doctor/compare tooling writes heavy ASCII art directly to `std::cout` and ignores `OutputFormatter`, so agents cannot consume structured output; no dry-run, no severity levels, and no notion of “fix plan vs applied fixes”.
- TUI pieces are fragmented: `tui/command_palette.cc` hardcodes demo commands, `UnifiedLayout` shows placeholder status/workflows, `ChatTUI` has its own shortcuts/history, and the ANSI `EnhancedTUI` shell is disconnected from ftxui flows. No TUI path renders real command metadata or schemas.

## Proposed Improvements
### 1) Argument/Help/Schema Consolidation
- Make `CommandRegistry` the single source for help and schemas: require handlers to supply description/examples/requirements, expose `z3ed help <command|category|all>` using `GenerateHelp/GenerateCategoryHelp`, and implement `ExportFunctionSchemas()` for AI tool discovery.
- Standardize global/common flags (`--rom`, `--mock-rom`, `--format {json,text,table}`, `--verbose`, `--grpc`) and teach `ArgumentParser` to parse booleans/ints/enum values with better errors and `--` passthrough for prompts.
- Add per-command validation hooks that surface actionable errors (missing required args, invalid ranges) and return status codes without dumping stack traces to stdout; ensure `ValidateArgs` is used in all handlers.

### 2) Doctor Suite (Diagnostics + Fixes)
- Convert `overworld-doctor`, `overworld-validate`, and `rom-compare` to use `OutputFormatter` with a compact JSON schema (summary, findings with severities, suggested actions, fix_applied flags) plus a readable text mode for humans.
- Split diagnose vs fix: `doctor overworld diagnose [--baseline … --include-tail --format json]` and `doctor overworld fix [--baseline … --output … --dry-run]`, with safety gates for pointer-table expansion and backup writing.
- Add baseline handling and snapshotting: auto-load vanilla baseline from configured path, emit diff stats, and allow `--save-report <path>` (JSON/markdown) for agents to ingest.
- Roadmap new scopes: `doctor dungeon`, `doctor rom-header/checksums`, and `doctor sprites/palettes` that reuse the same report schema so agents can stack health checks.

### 3) Interactive TUI for Humans + Agents
- Collapse the two TUI modes into one `--tui` entry: single ftxui layout that hosts chat, command palette, status, and tool output panes; retire the duplicate ChatTUI path in `ModernCLI::Run` or make it a sub-mode inside the unified layout.
- Drive the TUI command palette from `CommandRegistry` (real command list, usage, examples, requirements), with fuzzy search, previews, and a “run with args” form that populates flags for common tasks (rom load, format).
- Pipe tool/doctor output into a scrollback pane with toggles for text vs JSON, and surface quick actions for common diagnostics (overworld diagnose, rom compare, palette inspect) plus agent handoff buttons (run in simple-chat with the same args).
- Share history/autocomplete between TUI and `simple-chat` so agents and humans see the same recent commands/prompts; add inline help overlay (hotkey) that renders registry metadata instead of static placeholder text.

### 4) Agent & Automation Alignment
- Enforce that all agent-callable commands emit JSON by default; mark human-only commands as `available_to_agent=false` in metadata and warn when agents attempt them.
- Add `--capture <file>` / `--emit-schema` options so agents can snapshot outputs without scraping stdout, and wire doctor results into the agent TODO manager for follow-up actions.
- Provide a thin `z3ed doctor --profile minimal|full` wrapper that batches key diagnostics for CI/agents and returns a single aggregated status code plus JSON report.

## Test & Tools UX Proposals
### Current Observations
- Tests are well-documented for humans (`test/README.md`), but there is no machine-readable manifest of suites/labels or CLI entry to run/parse results; agents must shell out to `ctest` and scrape text.
- Agent-side test commands (`agent test run/list/status/...`) print ad-hoc logs and lack `OutputFormatter`/metadata, making automation fragile; no JSON status, exit codes, or artifacts paths surfaced.
- Test helper tools (`tools-*/` commands, `tools/test_helpers/*`) mix stdout banners with file emission and manual path requirements; they are not discoverable via TUI or CommandRegistry-driven palettes and do not expose dry-run/plan outputs.
- TUI/editor have no test surface: no panel to run `stable/gui/rom_dependent/experimental` suites, inspect failing cases, or attach ROM paths/presets; quick actions and history are missing.
- Build/preset coupling is implicit—no guided flow to pick `mac-test/mac-ai/mac-dev`, enable ROM/AI flags, or attach `YAZE_TEST_ROM_PATH`; agents/humans can misconfigure and get empty test sets.

### Proposed Improvements
- **Unified test CLI/TUI API**
  - Add `z3ed test list --format json` (labels, targets, requirements, presets) and `z3ed test run --label stable|gui|rom_dependent --preset <preset> [--rom …] [--artifact <path>]` backed by `ctest` with structured OutputFormatter.
  - Emit JSON summaries (pass/fail, duration, failing tests, log paths) with clear exit codes; support `--capture` to write reports for agents and CI.
  - Map labels to presets and requirements automatically (ROM path, AI runtime) and surface actionable errors instead of silent skips.
- **TUI/editor integration**
  - Add a Tests panel in the unified TUI: quick buttons for `stable`, `stable+gui`, `rom`, `experimental`; show live progress, failures, and links to logs/artifacts; allow rerun of last failure set.
  - Mirror the panel in ImGui editor (if available) with a lightweight runner that shells through the same CLI API to keep behavior identical.
- **Agent-aligned test harness**
  - Refactor `agent test *` commands to use CommandRegistry metadata and OutputFormatter (JSON default, text fallback), including workflow generation/replay, recording state, and results paths.
  - Provide a `test manifest` JSON file (generated from CMake/ctest) listing suites, labels, and prerequisites; expose via `z3ed --export-test-manifest`.
- **Tools/test-helpers cleanup**
  - Convert `tools-harness-state`, `tools-extract-values`, `tools-extract-golden`, and `tools-patch-v3` to strict arg validation, `--format {json,text}`, and `--dry-run`/`--output` defaults; summarize emitted artifacts in JSON.
  - Register these tools in the TUI command palette with real metadata/examples; add quick actions (“Generate harness state from ROM”, “Extract vanilla values as JSON”).
- **Build/preset ergonomics**
  - Add `z3ed test configure --profile {fast,ai,rom,full}` to set the right CMake preset and flags, prompt for ROM path when needed, and persist the choice for the session.
  - Surface preset/flag status in the TUI status bar and in `z3ed test status` so agents/humans know why suites are skipped.

## Deliverables / Exit Criteria
- Implemented help/schema surface (`z3ed help`, `z3ed --export-schemas`) backed by handler-supplied metadata; `ExportFunctionSchemas()` returns real data.
- All doctor/validate/compare commands emit structured output via `OutputFormatter` with diagnose/fix separation, dry-run, and baseline inputs; text mode remains readable.
- Single `--tui` experience that pulls commands from `CommandRegistry`, executes them, and displays outputs/history consistently for humans and agents.
- Updated documentation and examples reflecting the consolidated flag/command layout, plus quick-start snippets for agents (JSON) and humans (text).
