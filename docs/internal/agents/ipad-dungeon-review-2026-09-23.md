# Native iPad dungeon review and room-zero connections

Baseline: `e5f84f999`, branch `codex/combined-editor-candidate`.
The combined candidate preserves prior overworld, chest, reciprocal-door,
room clone/import and destination-header work. Concurrent sprite work in the
main checkout is separate. No push, installation or ROM-file write was performed.

## What changed

- `src/ios/iOS/YazeOverlayView.swift`: native dungeon room sidebar has 44-point
  row/close targets, selected-room accessibility state, two-line room names,
  empty-search guidance and a height constrained by the available window.
- `src/ios/iOS/RemoteRoomViewerView.swift`: loads on entry; captures room, overlays,
  scale and desktop identity for each request. Superseded tasks cannot publish
  results. New requests clear old image/details; errors expose Retry. Fit,
  render scale, Refresh, responsive controls, searchable room browser and Done
  buttons support review on narrower windows.
- `src/ios/iOS/DesktopAPIClient.swift` and `DesktopConnectionView.swift`: cancellable
  connection attempts, generation checks against late health responses, visible
  connecting/Cancel state and validated manual ports. URLSession injection enables
  isolated regression checks with no network access.
- `src/app/editor/dungeon/dungeon_canvas_connected_view.cc`: a consumed staircase
  slot targeting room 000 now produces a connection rather than a missing-target
  error. Unconsumed zero-filled slots still produce no inferred links. Removed
  the issue formatter's zero-as-unset wording and corrected its synthetic test.

The native room sidebar controls the embedded editor. Desktop Connection is a
separate optional workflow that renders rooms from a connected desktop. This
pass does not make remote room previews an editing surface.

## Dungeon evidence and boundary

USDASM revision `d53311a`, `bank_01.asm` `$01C3CA–$01C3D4`, masks `$0462` with 3,
loads `$7EC001,X` and directly stores it in room ID `$A0`. There is no zero
sentinel check. The connected graph now agrees with the destination inspector.

Placement-order-to-header-slot mapping remains an assumption in the existing
connected graph. The existing stale-slot auto-clear feature uses that assumption;
this pass does not qualify it. Pit-link detection also needs a separate audit:
changing zero to a link for every room would invent links without verifying
whether a room has an active pit/warp trigger.

## Verification

```bash
bash scripts/dev/check-ios-review.sh
cmake --build --preset mac-ai --target yaze yaze_test_unit --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='*DungeonCanvasViewerConnectedGraph*:*DungeonRoomMetadata*:*DungeonWorkbench*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='*DungeonCanvasViewerConnectedGraph*:*DungeonRoomMetadata*:*DungeonWorkbench*'
```

The desktop app and unit target built successfully. The selected dungeon packet
passed **138 tests across 18 suites**, with zero failures or skips, including
room-zero graph traversal, metadata edits and Workbench regressions. The linker
retains existing duplicate-library warnings.

The iOS helper runs seven connection scenarios through a mock URLProtocol, then
checks all native Swift source against the iOS simulator SDK with the ObjC bridge
header. It does not link the native C++ core or launch an iOS app. Existing
CloudKit ISO8601DateFormatter Sendable and deprecated query API warnings remain.
No simulator screenshots, VoiceOver, touch, or physical-device acceptance is
claimed. The manual queue remains open in the consolidated checklist.

## Next bounded dungeon task

Trace staircase object writers through runtime `$0462` selection before extending
connection editing or relying on stale-slot auto-clear. Add source-backed mapping
fixtures, then qualify on a disposable ROM and in-game traversal. Preserve native
Undo/Redo and the shared metadata mutation path; do not create a second editor
state model in Swift. Keep all deferred user checks in the existing checklist.
