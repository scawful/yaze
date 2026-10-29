# Dear ImGui 1.93 integration handoff (2026-10-26)

Codex owns the released work in `claude/imgui-193-20261026`, worktree
`/Users/scawful/src/hobby/yaze-worktrees/imgui-193-20261026`.
Universe task: `task_20261026T191635Z_25425`.

The original 108-path uncommitted handoff, both submodule identities, and
tracked patch are preserved outside the checkout in Oracle AFS:
`scratchpad/handoffs/unified_authoring_2026-10-26/imgui-integration-20261026/`.
The detailed migration report is [imgui-1.93-upgrade-2026-10-26.md](imgui-1.93-upgrade-2026-10-26.md).

1. Claude's background build `bj9a6tbqe` completed with exit 0. After review
   corrections, Codex rebuilt `yaze` and `yaze_test_unit` successfully and
   ran 127 focused tests across 16 suites: all passed, none skipped. These
   are donor-branch results, not acceptance of the final current-master
   composition.
2. Review corrected popup-blocked extended hex wheel edits, released canvas
   selection bounds, fractional floating-paste pixel coverage, and the UI
   queue's post-shutdown inline path. New frame fixtures disable ini
   persistence. Older property/deferred-input fixtures rewrote the full
   focused run's disposable sentinel; suite-wide ini isolation is not proved.
   Review also tightened the queue timeout test's skip condition and replaced
   the zero-case headless command with two real controller-close cases.
3. Preserve current master's shared HTTP lifecycle, extracted screenshot
   implementation, editor-control queue, and CLI/editor library separation
   when integrating. Keep general UI work before `NewFrame` and maintained
   editor-control work after it.
4. Normal hooks and exact-head Windows/CI checks still apply. The producer's
   Windows snapshot `92b9d7825` predates the final sprite/queue repairs.
   Product commit `7f1ae855` passed 13 stable CTest entries with a shorter
   isolated job root and explicit `--target z3ed`; 5,798 cases passed and
   459 were skipped. The default long runner path's two unchanged fixture
   failures remain documented. Native
   renderer parity, iOS, and WASM have no new acceptance evidence.
5. The report's architecture and UX ideas are proposals. Ctrl+S error
   reporting, overworld exit serialization, and a combined save/build/run
   action remain separate work. Follow current operating rules for authority
   and coordination; do not treat this note as a new approval policy.

Use disposable test paths and `YAZE_APP_DATA_DIR`; preserve the user's
settings, shared checkout and running applications. Do not overwrite `HOME`.
