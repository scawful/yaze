# Remote native Yaze testing on macOS

Status: active. Applies to agent-driven visible Yaze tests, including screenshot,
manual interaction, hidden-window GUI, and remote computer-use sessions.
User policy, 2026-09-29: isolate testing in another Space and always shut down
owned test applications when finished. Leaving a test app open is not a handoff.

## 1. Identify and isolate the session

- Before launch, record the candidate commit, executable path/hash, bundle ID,
  app display name, disposable project/ROM paths, fixture hashes, and test owner.
- Use a task-specific verification bundle and `YAZE_APP_DATA_DIR`. Never reuse
  the user's running Yaze instance or shared project settings for automation.
- Prefer existing headless/focused tests when they answer the question. Native
  acceptance still requires the actual interaction and renderer being claimed.
- Inventory windows and Spaces with `yabai -m query --windows` and
  `yabai -m query --spaces`. Record the user's current Space UUID and focused
  window before moving or focusing anything.
- Reuse only a dedicated test Space whose ownership is known and whose windows
  belong to the same test owner. Otherwise choose an empty, non-visible,
  non-fullscreen Space, or create a new empty Space. Never commandeer an
  occupied user Space. Do not run parallel GUI input workers in the same Space.

## 2. Prepare the Space before opening Yaze

Use a unique label such as `yaze-test-<task-id>`. Space indices can change;
retain the UUID/ID and label, and resolve the live index before each operation.

```sh
yabai -m space --create
# Re-query Spaces and identify the single new UUID/ID; do not assume 'last'.
yabai -m space <verified-new-index> --label <unique-test-space-label>
yabai -m rule --add label=<unique-rule-label> \
  app='^Exact Verification App Name$' manage=off space=<unique-test-space-label>
```

Escape regular-expression characters in the exact app name. Rules must match
all windows of that verification app, including detached ImGui viewports and
transient windows. Do not limit the rule to the main window title.
The ordinary `app=^Yaze$` rule does **not** match renamed verification bundles.
Register the rule before launch so the app does not briefly tile on the user's
Space. When recovering an already running owned app, apply only its rule:

```sh
yabai -m rule --apply <unique-rule-label>
```

Query the launched PID's windows and verify both the Space and `is-floating`.
Focus the test Space only when interactive inspection requires it. Establish a
repeatable window size and wait for the canvas viewport to settle before input
or screenshots. Record the actual window/viewport size and UI scale.

If Space creation/routing is unavailable, use Mission Control through the
supported computer-use tool or an existing dedicated empty Space. Do not disable
SIP, change accessibility permissions, or fall back to the user's occupied Space.
Report the environment blocker if no isolated session can be established.

## 3. Operate and collect evidence

- Use the supported computer-use tool for native UI actions, or the existing
  application/test API for supported operations. Use yabai for Space/window
  management. Do not fabricate mouse events with unrelated automation APIs.
- Re-query UI state after window movement, resize, docking, Space changes,
  reopen, or a stale-window error. Never replay old screenshot coordinates.
- Check yabai state before diagnosing a suddenly resized, clipped, or misplaced
  viewport as an editor defect. Stop input if focus goes to another app.
- Keep original ROM/project files unchanged. Save As/reopen tests write only
  explicit disposable destinations. Preserve evidence before shutting down.
- Report source tests, hidden GUI tests, native renderer evidence, save/reopen,
  and runtime qualification separately. A launched window is not acceptance.

## 4. Mandatory teardown, including failed or interrupted tests

Testing is not finished until cleanup is verified. Do this before the final
response, a normal task handoff, or changing to a different workstream.

1. Save required test evidence and any explicitly requested disposable output.
   Quit each owned test app through its normal Quit/Close flow. Resolve unsaved
   prompts explicitly: discard only known disposable test edits. Never save
   them into the user's source ROM/project.
2. Confirm every owned PID exited and its native windows disappeared. Closing
   one detached viewport, issuing Quit, or minimizing the app is insufficient.
   Verify with read-only process/window queries. Do not call an app lookup that
   launches missing apps (such as macOS `cua.getApp`) after requesting quit; it
   can reopen the app and defeat cleanup.
   Never use `killall yaze` or broad name matching. For a hung process, reverify
   its executable/PID and task ownership; attempt targeted graceful shutdown.
   Forced termination is a cleanup failure to record, not a successful quit test.
   Do not terminate user-owned or other-agent processes.
3. Remove only this session's temporary yabai rules. Restore the original Space
   and focused window if they still exist and the user has not moved elsewhere
   since the session began. Do not force focus back over intervening user work.
4. Destroy a Space only if this session created it, its UUID/ID still matches,
   and a fresh query confirms it contains no windows. Preserve reused Spaces and
   restore their original label if temporarily changed. If the user or another
   agent placed a window there, leave it and report why cleanup was deferred.
5. Re-query processes, windows, rules and Spaces. Report cleanup status and any
   remaining resources. On a hard interruption, inspect the recorded session
   identities at the next turn and complete cleanup before launching again.

```sh
yabai -m rule --remove <unique-rule-label>
# Only after verifying session ownership, UUID/ID and emptiness:
yabai -m space --destroy <verified-live-index>
```

Do not leave the app running merely so the user can inspect it later. Preserve
screenshots/evidence and close it. An explicit user request to leave a specific
app open overrides this default; record that exception in the handoff.

## 5. Evidence record

Record the task owner, commit and executable hash, app/bundle identity, launch
PID, original Space UUID, test Space UUID/label and whether created or reused,
exact temporary rule labels, fixture hashes, test actions/results, and teardown
results. The minimum final cleanup statement is: owned apps closed; temporary
rules removed; created empty Space removed (or the exact remaining blocker).

For installed-app quit regression testing, see
`scripts/agents/test-installed-macos-quit.sh`. Follow this Space isolation policy
as well; the script's process/exit checks do not by themselves prove isolation.
