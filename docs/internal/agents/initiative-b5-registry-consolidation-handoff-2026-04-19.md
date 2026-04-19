# Initiative B.5 — CommandRegistry / ToolRegistry Consolidation Handoff

Status: PENDING (post-0.7.1 work)
Owner: Codex (ai-infra-architect)
Created: 2026-04-19
Blocker for: Initiative B close-out

## Summary

Two registries currently track the same universe of z3ed commands:

| | `cli::CommandRegistry` | `cli::agent::ToolRegistry` |
|---|---|---|
| File | `src/cli/service/command_registry.{h,cc}` | `src/cli/service/agent/tool_registry.{h,cc}` |
| Namespace | `yaze::cli` | `yaze::cli::agent` |
| Storage | `map<string, unique_ptr<CommandHandler>>` + `map<string, CommandMetadata>` | `map<string, ToolEntry{ToolDefinition, HandlerFactory}>` |
| Registration | Eager (CliCommands auto-registered in `Instance()`) | Lazy factory via `RegisterBuiltinAgentTools` in `tool_registration.cc` |
| Metadata fields | name, category, description, usage, available_to_agent, requires_rom, requires_grpc, aliases, examples, todo_reference | name, category, description, usage, examples, requires_rom, requires_project, access (Read/Mutate), required_args, flag_args |
| Callers | `cli_main`, `cli.cc`, `api_handlers`, `handlers/agent*`, `wasm_terminal_bridge` | `tool_dispatcher.cc` |

Both surfaces need: per-command metadata, handler lookup, and category grouping.
Today registration happens twice — once in each registry. B.4 added argument
validation (`required_args`, `flag_args`, `access`) only to `ToolRegistry`; if a
CLI user hits a handler through `CommandRegistry::Execute`, those validators
never run.

## Goal

One registry as source of truth. Pick the shape that keeps B.4's arg validation
and the JSON schema export from CommandRegistry, then delete the other.

## Recommended Direction

**Keep `CommandRegistry` as the canonical store; retire `ToolRegistry`.**
Rationale:

1. `CommandRegistry` has more callers (7 files vs. 1) and the broader
   `CommandMetadata` envelope.
2. It already exposes `ExportFunctionSchemas()` and `GenerateHelp()`, the two
   consumer surfaces that make a registry worth having.
3. `ToolRegistry`'s factory pattern is a nice-to-have but can be a second-order
   cleanup — `CommandRegistry::Register()` already takes `unique_ptr<Handler>`
   which is eager. A follow-up can swap to a factory if lazy registration
   matters.

Steps:

1. Extend `CommandMetadata` with fields from `ToolDefinition` that aren't already
   there: `access` (ToolAccess enum), `required_args`, `flag_args`,
   `requires_project`. Keep `requires_grpc` (CommandRegistry-only).
2. Move `ToolAccess` enum from `agent/tool_registry.h` up to
   `command_registry.h` (or a shared `cli/service/command_types.h`).
3. Update `tool_dispatcher.cc` — four callsites
   (`tool_dispatcher.cc:217,239,305,320`) all read from
   `ToolRegistry::Get()`. Change to `CommandRegistry::Instance()` and use
   `GetMetadata` / execute via `CommandRegistry::Execute`.
4. Fold `RegisterBuiltinAgentTools` (`tool_registration.cc`) into the
   CommandRegistry registration path. The B.4 `REGISTER_BUILTIN_AGENT_TOOL`
   macro can become a thin wrapper that populates `CommandMetadata` and calls
   `CommandRegistry::Register`.
5. Delete `src/cli/service/agent/tool_registry.{h,cc}` and
   `src/cli/service/agent/tool_registration.{h,cc}` (or keep the filename
   and just have it forward to the consolidated registration, if less churn
   for existing callsites).
6. Update `src/cli/service/agent/tool_dispatcher.cc` arg-validation path
   (B.4 code) to read `required_args`/`flag_args` from `CommandMetadata`.

## Alternate Direction (if you disagree)

Keep `ToolRegistry` because factory-lazy construction matters. Then:

- Expand `ToolDefinition` with CommandRegistry's extra fields (aliases,
  todo_reference, requires_grpc, available_to_agent).
- Convert `CommandRegistry::Register` to a factory shape.
- Update the 7 CommandRegistry callsites.

This is more churn (more callsites to update) and loses the existing JSON
schema export, so it's a harder sell.

## Scope Boundaries

- **In scope:** consolidate metadata, single registration path, single dispatch
  lookup, B.4 arg validation available to CLI callers.
- **Out of scope:** renaming commands, changing the `CommandHandler` base class
  contract, touching `handlers/command_handlers.h` catalog helpers beyond what
  the registration merge requires.

## Validation

- `ctest --preset mac-ai-unit` — full green (baseline 1622/1622 as of v0.7.0).
  Particularly `tool_registry_test.cc` and any CommandRegistry tests.
- `ctest --preset mac-ai-integration` — 237/237.
- Sanity-check a CLI invocation (`z3ed dungeon-describe-room --room=0`) and an
  agent tool call (via `tool_dispatcher_test.cc`) both hit the same handler
  instance.
- Help output and JSON schema export unchanged.

## Risk Notes

- `tool_dispatcher.cc` arg validation (B.4, landed as `b16d34c0 + f8d871e7`)
  currently only runs when the agent invokes tools. Moving registration to
  CommandRegistry means CLI invocations also get validated — this is *good*,
  but some existing CLI tests may pass args in forms the validator rejects.
  Audit failing tests carefully.
- Meta-tools (`tools-list`, `tools-describe`, `tools-search`) are currently
  handled specially in ToolDispatcher. When the registration merges, keep
  the "dispatcher intercepts these by name" behavior; don't delegate them
  to the normal handler path.
- `ToolAccess::kMutating` vs. `requires_rom` semantics: not identical. A
  mutating tool always requires a ROM, but read-only tools may still require
  a ROM. Keep both fields; don't collapse.

## References

- Canonical plan: nothing in `docs/internal/plans/` yet — write one or treat
  this handoff as the plan.
- Memory context: MEMORY.md "Initiative B progress" section tracks B.1–B.6
  state; B.5 is the last pending sub-phase.
- Related past work:
  - B.4 (arg validation): `b16d34c0` + fix `f8d871e7`
  - B.3a/b (shared tool-schema builder): `1cde1b7a` + `aa3798d1`
  - B.2 (agent_editor decomposition): `3525783d → 8f629165 → 83137d33 → 548af7b7`
