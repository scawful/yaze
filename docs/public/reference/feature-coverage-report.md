# Editor readiness and feature coverage

Last reviewed: 2026-09-14 for the v0.8.0 development line.

This is the canonical readiness matrix for Yaze's user-facing editors. A status
describes the complete workflow, not the size of the implementation:

- **Tester ready**: a bounded load, edit, **File > Save ROM**, and reopen test is
  useful now.
- **Conditional**: only the named subset or save procedure is supported.
- **View only**: inspection is useful, but ROM persistence is not tester-ready.
- **Experimental**: availability or behavior depends on the build, provider, or
  unfinished subsystem.

No editor yet has an automated test for the entire GUI path from a user edit,
through **File > Save ROM**, to closing and reopening the disk file in the same
editor. Existing editor, serializer, and ROM readback tests provide good partial
coverage, but they do not replace that acceptance path.

## Desktop editor matrix

| Editor | Readiness | Durable workflow today | Main limitation before promotion |
| --- | --- | --- | --- |
| Dungeon | **Tester ready** | Edit a room, use **File > Save ROM**, close, and reopen a copied ROM. | Known object-rendering and layering exceptions remain; add full GUI-to-disk readback. |
| Overworld | **Tester ready** | Edit a map or entity, use **File > Save ROM**, close, and reopen a copied ROM. | Add full GUI-to-disk readback and a dedicated user guide. |
| Message | **Tester ready** | Edit valid message text, use **File > Save ROM**, close, and reopen a copied ROM. | Add full GUI-to-disk readback and a dedicated user guide. |
| Palette | **Conditional** | First use the Palette panel's **Save to ROM**, then use **File > Save ROM**, close, and reopen. | Integrate Palette with coordinated save or prove and clearly retain the two-step contract. |
| Assembly | **Conditional** | **Save File** writes the active ASM source. Applying an Asar patch is a separate, explicit ROM operation. | Add dirty-close, source-reopen, and separately fenced patch-application tests. |
| Sprite | **Conditional** | Custom `.zsm` files can be edited and saved. Vanilla sprites are for viewing here; room sprite placement belongs in Dungeon. | Remove or disable incomplete vanilla edit controls and prove `.zsm` roundtrip. |
| Settings | **Conditional** | Settings and layouts use application configuration files, not ROM save. Restart and verify each changed setting. | Add Settings-panel-to-disk-to-restart coverage. |
| Graphics | **View only** | Viewing and preview are useful. Do not make a persistence test edit. | A pending sheet edit deliberately blocks **Save ROM** until the serializer is safe. |
| Screen | **View only** | Inspect dungeon maps, inventory, title, and world-map screens. | Any pending Screen edit deliberately blocks coordinated save; writers need domain-by-domain readback proof. |
| Music | **View only** | Browse and play loaded music. Do not rely on ROM persistence. | Music is not in coordinated save; instrument and sample writers are unimplemented. |
| Hex / Memory | **View only** | Use only for expert inspection in tester builds. | Raw-buffer editing lacks an editor dirty state, undo, and a tested save contract. |
| Emulator | **Experimental** | Runtime inspection and play testing only. | Save-state UI and conditional breakpoint behavior are incomplete. |
| Agent | **Experimental** | Build- and provider-specific chat/tool exploration. | Availability depends on build flags and provider configuration; it is not a ROM-save participant. |

## What coordinated Save ROM currently covers

`EditorManager::SaveRom()` is the application-level persistence boundary.
For loaded editors it coordinates:

1. Screen serialization only when its state is valid and has no pending edit.
2. Dungeon serialization.
3. Overworld serialization.
4. Message serialization when message saving is enabled.
5. ROM safety checks, conflict handling, backup policy, and the final disk write.

Important exceptions:

- Palette changes must first be committed to the shared ROM buffer with the
  Palette panel's **Save to ROM** action.
- Graphics and dirty Screen edits fail closed before the disk write.
- Music, Assembly, Sprite, Settings, Emulator, and Agent use separate or partial
  persistence models and are not invoked by coordinated ROM save.
- Hex / Memory currently exposes raw ROM data without a complete editor
  transaction contract and is outside the supported tester lane.

## Evidence levels

Use these terms when updating this report or a pull request:

| Evidence | What it proves |
| --- | --- |
| Component test | A model, parser, widget policy, or serializer behaves under a focused test. |
| Direct ROM readback | A writer's bytes can be reopened and decoded, often without the application editor. |
| App-path test | An editor object or `EditorManager` participates in the tested save path. |
| GUI smoke | A panel opens or expected text appears; it does not prove editing or persistence. |
| Manual acceptance | A packaged app completes a named workflow on a named platform. |

Tests named `*_save_test.cc` or “E2E” are not automatically application-path
tests. Several construct a ROM/data writer directly and then write bytes to a
temporary file. Keep that evidence, but do not use it to claim GUI save parity.

## Current strengths

- ROM/project lifecycle, backup policy, conflict detection, and fail-closed
  save behavior have substantial focused coverage.
- Dungeon has the broadest editor-specific coverage: room objects, doors,
  sprites, headers, collision, chests, pot items, palette interaction, and
  workbench navigation.
- Overworld has editor-method coverage for save/reload, undo, clipboard, maps,
  and entities. Its persistent `ScratchPad.dat` workflow is implemented.
- Message saving is transactional and rejects invalid parsed text.
- Palette JSON import/export is implemented when JSON support is enabled and
  has focused validation tests.

## Highest-priority coverage gaps

1. Add one reusable application-path harness for:
   **GUI action -> File > Save ROM -> close -> reopen disk file -> verify**.
2. Use that harness first for Dungeon, Overworld, Message, and Palette.
3. Make ROM-backed GUI tests fail or visibly skip when their required ROM is
   unavailable; a window-only smoke must not stand in for persistence coverage.
4. Keep unsafe mutation surfaces disabled or clearly labeled until their writer
   and readback path exist.
5. Promote Graphics and Screen one independently verified data domain at a time.
6. Implement real CRC32 calculation in `AsarWrapper`. Its library and CLI
   patch paths currently return `0`, so that result field is not ROM-identity
   evidence yet.

## Other products

### z3ed CLI

`z3ed` provides scriptable ROM inspection, guarded writes, validation,
snapshots, doctor commands, and agent workflows. Its command-specific status is
documented in the [z3ed CLI guide](../usage/z3ed-cli.md). CLI serializer tests do
not promote the corresponding desktop editor automatically.

### Web / WASM preview

The web build is a preview with browser-storage and download workflows. It does
not include the emulator and is not a substitute for native editor acceptance.
See the [Web App guide](../usage/web-app.md).

## Updating this matrix

Promote an editor only when the durable workflow is explicit and the evidence
matches the claim. Record exact commands and the tested commit in the pull
request or release checklist; avoid embedding volatile test counts here.
