# Owned Project Save As replay

Source candidate only. Native/GUI and C++ execution remain UNRUN until root
reviews the combined producer. Compose this cut atop `172c6907a` with corrected
Lyra replay `81da86c00` and the current-bundle runner. Keep existing fixtures.

## Explicit destination fixture

`shortcut:Save As` now drives the real Cmd/Ctrl+Shift+S chord. During that
single owned harness action, `YAZE_TEST_PROJECT_SAVE_AS` supplies the destination
picker's result: an absolute new `.yaze` path selects it; an explicitly empty
string selects Cancel. An unset variable or relative/non-.yaze value fails
before the chord. The harness requires an open project and fails if the normal
shortcut never reaches the picker. The application still executes
`SaveCurrentSessionAs()` → `SaveProjectAs()` → `SaveProjectAs(filepath)`.

This is a disclosed non-native destination-selection fixture. It is not proof
of interaction with NSSavePanel. Native picker behavior stays unchanged outside
that one-shot scope, including ordinary keyboard/menu use with the variable set.
The scope is process-wide and mutex-protected because the test coroutine and
application UI can run on separate threads. Its destructor clears unused
selection state; nested scopes refuse rather than replace another selection.

## Bounded acceptance integration

1. Root prepares its complete copied current bundle and exact producer receipt.
   Capture full before hashes, including the typed `lyra_build_request_file`
   and every referenced BIN/resource. Normal Lyra edit/Undo/Redo/Save settles
   first. Immediately before Save As, run the read-only receipt generator:

   ```sh
   python3 scripts/agents/project_saveas_replay.py prepare \
     --bundle /absolute/copied/bundle \
     --destination /absolute/fresh/folder/moved.yaze \
     --output /absolute/new/evidence/save-as
   ```

2. In that owned hidden app, set `YAZE_TEST_PROJECT_SAVE_AS` to the exact bound
   destination at launch and run the generated `save-as.json` with the runner's
   existing `z3ed agent test replay` endpoint options. This invokes Save As;
   the helper never rewrites project data or substitutes SaveRomAs.

3. While quiescent, run `project_saveas_replay.py verify --output ...`.
   It requires the original full inventory/BIN hashes unchanged, the new
   descriptor present, a distinct identical populated sidecar, and canonical
   identity of every original resource reference, including typed request.
   Missing keys, retargeted requests, changed BINs or absent saves fail.

4. Root uses its normal Quit replay, proves owned process/listener teardown,
   launches a fresh process on **the destination descriptor**, navigates to
   Overworld A0 and runs the corrected Lyra `reopen.json`. Run Save As `verify`
   again and retain rendering screenshots and fresh hashes. This distinguishes
   descriptor readback from actual editor reopen/rendering evidence.

Cancel is a separate root-selected case on a fresh copy: launch with
`YAZE_TEST_PROJECT_SAVE_AS=''`, run the same normal Save As shortcut and
`verify-cancel --output ...`. It requires no destination descriptor/sidecar
and an unchanged complete source inventory. Preserve the first terminal failure.
Do not switch the configured destination or retry within an admitted episode.

## Source checks

```sh
PYTHONDONTWRITEBYTECODE=1 python3 scripts/agents/tests/test_project_saveas_replay.py
```

Eight host tests use explicit synthetic outputs to check the verifier only.
Three `ProjectSaveDialogSeamTest.*` C++ cases cover cross-thread one-shot
consumption, an engaged empty Cancel result, and scope cleanup/nested refusal;
they are registered in the existing integration source and remain UNRUN.
