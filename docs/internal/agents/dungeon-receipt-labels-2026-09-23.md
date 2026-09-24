# Shared chest receipt labels and CLI readback

Baseline: `5c5bf26b2`, `codex/combined-editor-candidate`.

## Changes

The receipt-ID name table now belongs to `Zelda3Labels::GetItemReceiptNames`.
`GetItemReceiptLabel` applies explicit project item overrides, then the vanilla
receipt reference, then an unknown label. The generic inventory-oriented table
is unchanged. The shared chest inspector, older standalone chest panel, room
CLI description and chest-list/duplicate summary all use the receipt API.

CLI chest records retain raw `item_id` and now include `item_name_source`:
`project`, `vanilla_receipt`, or `unknown`. This field identifies label provenance;
it does not claim that a patched ROM handler implements the vanilla behavior.
Receipt 00 is Fighter Sword and is included in unique/duplicate reward summaries.

## Source verification

USDASM `d53311a`, `bank_09.asm`, `AncillaAdd_ItemReceipt.offset_y` starts at
`$09836C`. Entries `$098387`, `$098390`, `$098391`, `$09839E`, `$0983A6` establish
receipt IDs 1B=Power Glove, 24=Small Key, 25=Compass, 32=Big Key, 3A=Tossed Bow.
These differ from the generic table's shifted names.

Oracle can override 3A. An explicit project label such as Wolf Mask is honored;
without an override, the output says `vanilla_receipt`, not that the hack reward
was independently verified. No Oracle labels, handlers or ROMs were changed.

## Validation commands

```bash
cmake --build --preset mac-ai --target yaze z3ed yaze_test_unit --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='DungeonEditCommandsTest.*:DungeonChestEditorTest.*:ResourceLabelsTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='DungeonEditCommandsTest.*:DungeonChestEditorTest.*:ResourceLabelsTest.*'
```

The app, z3ed and unit target built successfully. **114 tests across three suites
passed**, with zero failures or skips. Existing duplicate-library linker warnings
remain. These are source/build and synthetic fixture checks, not installed-nightly
or real Oracle reward-handler qualification.

Regression cases exercise CLI output names/provenance, explicit project override,
unknown IDs, receipt-zero duplicate summaries and unchanged synthetic ROM bytes.
The audit's pot word 04CE is checked through the current CLI as tile (39,9), with
raw position retained. That tests the existing corrected codec; this pass does
not modify pot serialization.

## Remaining qualification and ownership

The reported sprite 80 name remains a separate sprite-label issue. Preserve the
sprite agent's ownership; do not overwrite its working tree. Source changes here
are isolated to the combined candidate. The installed September 14 nightly is
not replaced, so its output remains stale until an explicitly chosen installation.

Continue read-only real-room qualification for Oracle 73/74/75/83/84/85/86 with
an exact candidate binary and ROM hashes. Registry adjacency is not door proof.
The consolidated manual checklist remains the single user testing queue.
