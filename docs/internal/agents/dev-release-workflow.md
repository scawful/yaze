# Dev + Release Workflow Protocol

Status: Active
Owner: backend-infra-engineer
Created: 2026-02-14
Last Reviewed: 2026-02-14
Next Review: 2026-02-28
Validation Criteria: `scripts/dev/local-workflow.sh all` succeeds on a configured local machine.
Exit Criteria: Replaced by a newer protocol doc linked from `AGENTS.md` and archived.

## 1. Standard Local Workflow
Use one entrypoint for local build/test/sync:

```bash
scripts/dev/local-workflow.sh all
```

Subcommands:

```bash
scripts/dev/local-workflow.sh build
scripts/dev/local-workflow.sh test
scripts/dev/local-workflow.sh sync
scripts/dev/local-workflow.sh status
scripts/dev/local-workflow.sh hooks
scripts/dev/local-workflow.sh release-check
```

Key defaults:
- Build preset: `dev`
- CTest preset: auto by platform (`mac-ai-quick-editor` on macOS)
- Sync target app: `/Applications/yaze.app`
- PATH link for CLI: `/usr/local/bin/z3ed`

### Incremental agent builds

Use the build-only entrypoint for local agent iteration:

```bash
scripts/agent_build.sh yaze
scripts/agent_build.sh yaze z3ed --jobs 4
scripts/agent_build.sh yaze_test_unit --dry-run
scripts/agent_build.sh --status
```

It defaults to two workers and accepts one through four. `--jobs` overrides
the task-specific `YAZE_BUILD_JOBS`. It intentionally ignores an inherited
`CMAKE_BUILD_PARALLEL_LEVEL` and sets the selected count for its children.
It resolves the preset's
build directory, checks an existing cache's source/configuration, and configures
only when the cache is absent. `YAZE_BUILD_DIR` overrides both configure and build
paths. `--fresh` explicitly requests a fresh configure after the same identity
checks. No test execution, installation or runtime sync is included.

On macOS it runs configure/build with `taskpolicy -b nice -n 10` for background
priority. It waits up to 600 seconds for `/tmp/oos-heavy.lock`, polling every two
seconds and reporting owner changes or 30-second wait updates. Use
`--wait-seconds 0` to refuse immediately, or choose a limit up to 3600 seconds.
It never steals a stale, missing-owner or foreign lock. SIGINT/SIGTERM cancel only
its child process group; it waits for that group to exit before releasing its
matching lock. `--status` and `--dry-run` do not acquire the lock or configure.

The helper keeps the existing compiler-cache configuration and infers
`CCACHE_BASEDIR` from the Git common repository/worktree parent unless explicitly
set. Existing `local-workflow.sh`, preset worker counts and other build wrappers
retain their own behavior; this change does not make every build entrypoint use
the quiet workflow.

## 2. Runtime Sync Rules
Who may sync, and the pre-sync checks: [operating-rules.md](operating-rules.md) section 4.

`sync` must guarantee:
- App bundle in `/Applications` is byte-identical to the selected build output.
- PATH-visible `z3ed` points to the selected binary (symlink + hash verification).
- `scripts/z3ed --which` and `scripts/yaze --which` are the source of truth for selected binaries.

Developer diagnostics:

```bash
scripts/z3ed --doctor
scripts/yaze --doctor
scripts/dev/local-workflow.sh status
```

## 3. Versioning Protocol
Version source of truth:
- `VERSION`

Required updates when version changes:
- `docs/CHANGELOG.md` must include a section for the new version.
- `docs/public/reference/changelog.md` should be updated in the same PR/commit.

Validation command:

```bash
scripts/dev/release-version-check.sh --staged
```

## 4. Hook Policy
Install both hooks with one command:

```bash
scripts/install-git-hooks.sh install
```

Hook responsibilities:
- `pre-commit` runs staged-file checks (`scripts/pre-commit.sh`):
  - C/C++ formatting (`clang-format` dry-run)
  - shell syntax (`bash -n`)
  - python syntax (`python3 -m py_compile`)
  - version/changelog protocol (`release-version-check --staged`)
- `pre-push` runs fast project validation (`scripts/pre-push.sh`):
  - Builds `yaze_test_unit`
  - Runs unit smoke tests
  - Runs change-aware UI regression filters for panel/workflow paths

Emergency bypass is allowed but discouraged:
- `git commit --no-verify`
- `git push --no-verify`

## 5. Multi-Agent Release Protocol
For release-related work (version bump, release notes, packaging, sync scripts):
1. Create/claim a universe coordination task before edits (`scripts/agents/coord task-add ...`, `task-claim ...`).
2. Assign one release owner persona (`backend-infra-engineer`) and supporting roles.
3. Require explicit ownership split in task notes/history:
   - Version/changelog owner
   - Build/package owner
   - Validation owner
   - Deployment/sync owner
4. Post validation evidence (commands + pass/fail summary) before marking COMPLETE.
5. Do not mark release-ready until both hooks and CI checks are green.

## 6. Experimental release candidates

Keep `VERSION` numeric, for example `0.8.0`. Run the Release workflow on the
candidate branch with version `v0.8.0-experimental.1` and `publish=false` first.
Add an exact `## v0.8.0-experimental.1` section to `docs/public/release-notes.md`
listing the included features, supported projects, remaining gaps, and test steps.
Stable release notes are not used as a fallback for a preview.

The workflow carries `-experimental.1` through `YAZE_VERSION_SUFFIX` to the app,
archive name, and manifest. Debian control metadata uses `0.8.0~experimental.1`
so that the final `0.8.0` package is an upgrade. All suffixed tags are prereleases.
An existing tag must resolve to the tested commit; a new tag targets that commit.

Before publication, qualify the packaged candidate's named editing workflow:
create/open, edit, undo/redo, save, and fresh reopen. For Oracle, use disposable
ROM/project copies and qualify the resulting game behavior separately. Lyra
project drafts do not establish runtime transitions or save support.

Run the build-free release contract checks with:

```bash
python3 scripts/release/test-release-contracts.py
python3 scripts/dev/test_oracle_smoke_workflow_contract.py
```
