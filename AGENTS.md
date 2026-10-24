# Yaze agent router

Inherit `~/src/AGENTS.md`; load only task-relevant references.
Operating rules: `docs/internal/agents/operating-rules.md`.
If absent in an older checkout, read `git show origin/master:docs/internal/agents/operating-rules.md`.
They cover native windows, coordination, installs, issue replies and Windows verification.
AFS quality/tool delivery: `~/.context/projects/oracle-of-secrets/memory/agent_rules.md`.

## Implementation
Work in an owned worktree; preserve the shared checkout's dirty work.
Use Google C++ style, explicit ownership and existing error handling.
Persistent features join normal dirty/save/discard/close and Undo lifecycles.
Domain mutation/serialization belongs outside rendering/input UI.
Architecture boundaries: `docs/internal/architecture/refactor-quality-guardrails.md`.
Relevant code/tool references: `docs/internal/agents/routing-skills-tools.md`.
Use a persona reference only when it contributes task-specific knowledge.

## Build and proof
Reuse the worktree's selected preset cache and `scripts/agent_build.sh` for incremental builds.
If an older helper still takes the native lock for builds, use equivalent preset CMake commands
under `quiet-build`; builds do not own the native lock. Mac builds/tests use at most four workers.
Use focused tests while iterating;
run the selected candidate's required pre-push/CI checks before publishing.
A green formatter or startup check does not establish behavior/save acceptance.
Native GUI: read `docs/internal/testing/remote-native-testing.md`;
register the exact-app rule before launch on a dedicated Space, preserve user apps,
then quit owned test apps and verify process/window/rule/Space cleanup.
Follow `~/src/docs/guides/ai-coder/AGENT_STORAGE_HYGIENE.md` after builds/deployments;
protect active work, ROMs/saves and rollback copies. Its archive gate still applies.

## Coordination and delivery
Use `scripts/agents/coord` for yaze ownership; Oracle inbox/board for cross-project asks.
Derived coordination boards are snapshots, not another task queue.
Direction: `docs/internal/roadmap.md`; release facts: `docs/internal/status.md`
and `docs/CHANGELOG.md`. Multi-PR work uses one linked plan.
Tooling has an entry point, owner, verification and adoption target; announce ready output
through AFS and close delivery only after adoption or explicit deferral.
`oos168.sfc` is Oracle's edit target; `oos168x.sfc` is emulator-only.
