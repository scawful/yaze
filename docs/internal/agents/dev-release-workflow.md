# Dev + Release Workflow Protocol

Status: Active
Owner: backend-infra-engineer
Created: 2026-02-14
Last Reviewed: 2026-02-14
Next Review: 2026-02-28
Validation Criteria: `scripts/dev/local-workflow.sh all` succeeds on a configured local machine.
Exit Criteria: Replaced by a newer protocol doc linked from `coordination-board.md` and archived.

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

## 2. Runtime Sync Rules
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
- `CHANGELOG.md` must include a section for the new version.
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
- `pre-push` runs project validation (`scripts/pre-push.sh`)

Emergency bypass is allowed but discouraged:
- `git commit --no-verify`
- `git push --no-verify`

## 5. Multi-Agent Release Protocol
For release-related work (version bump, release notes, packaging, sync scripts):
1. Create/update a coordination board entry before edits.
2. Assign one release owner persona (`backend-infra-engineer`) and supporting roles.
3. Require explicit ownership split in board notes:
   - Version/changelog owner
   - Build/package owner
   - Validation owner
   - Deployment/sync owner
4. Post validation evidence (commands + pass/fail summary) before marking COMPLETE.
5. Do not mark release-ready until both hooks and CI checks are green.
