# Yaze capability assessment

Last reviewed: 2026-09-14 for the v0.8.0 development line.

Yaze is a cross-platform ALttP editor with modern safety checks, undo-oriented
workflows, a built-in emulator, and the `z3ed` automation CLI. ZScream and
Hyrule Magic remain important workflow references because many ROM hackers know
their object placement and project conventions.

This assessment is job-based. It deliberately avoids percentage-parity claims:
a feature can look complete while still lacking a safe save/reopen path.
Current persistence status lives in the
[editor readiness matrix](feature-coverage-report.md).

## Practical comparison

| Job | Yaze today | Established-editor reference | Main Yaze follow-up |
| --- | --- | --- | --- |
| Edit dungeon rooms | Broad objects, doors, sprites, headers, collision, items, undo, responsive workbench, and fail-closed ROM checks. Bounded beta. | ZScream and Hyrule Magic provide mature single-room placement conventions. | Finish rare object/layer parity, custom/gameplay object authoring, and full application save/reopen proof. |
| Edit the playable overworld | Maps, Tile16 painting, entities, properties, clipboard, undo, and version-aware save. Bounded beta. | ZScream provides a mature ALttP-focused overworld workflow. | Close remaining sprite/paste gaps and add full application save/reopen proof. |
| Edit messages | Parsing, preview, search, bundle/source workflows, and transactional save. Bounded beta. | Older editors provide familiar message-table editing. | Add a focused user guide and complete GUI-to-disk readback. |
| Edit palettes | Broad palette groups, live preview, undo, JSON exchange, and ROM-buffer commit. Conditional beta. | Palette editing is an established workflow in both older editors. | Replace or fully prove the current two-step save procedure. |
| Edit graphics and screens | Strong inspection and partial editing UI; persistence is fail-closed when unsafe. | Mature tools may be more appropriate for production graphics/screen edits today. | Prove one serializer domain at a time before enabling general persistence. |
| Author custom dungeon systems | Project-mapped custom objects, previews, a tile workshop, Oracle water/collision tools, and minecart source/collision utilities exist. | Custom workflows are usually patch- or project-specific. | Consolidate mappings, visuals, collision semantics, source publishing, and validation into one authoring workflow. |
| Apply ASM patches | Integrated Asar support and project source editing. | External assembler workflows remain common and transparent. | Separate source-file save proof from fenced ROM patch-application proof. |
| Inspect/test runtime | Built-in emulator and debug panels plus Mesen-oriented validation workflows. | External emulators remain the independent runtime truth. | Complete save-state and conditional-breakpoint workflows; retain independent Mesen checks. |
| Automate edits | `z3ed` provides structured CLI inspection, validation, guarded edits, and agent workflows. | Older GUI editors generally have less scriptable coverage. | Keep CLI evidence separate from desktop-editor readiness and expand readback checks. |
| Work across operating systems | Native build/package pipelines for macOS, Windows, and Linux; browser preview through WASM. | Hyrule Magic is Windows-centric; ZScream availability depends on its current distribution. | Finish exact-artifact and hands-on acceptance before each tester release. |

## Where Yaze is strongest

- Fail-closed ROM writes, project manifests, write-range conflict checks, and
  backup/restore policy.
- Dungeon inspection and diagnostics that expose room IDs, object IDs, streams,
  layers, geometry, and validation evidence.
- Cross-platform source and packaging infrastructure.
- Undo-aware editor architecture and scriptable `z3ed` workflows.
- A validation ladder that distinguishes synthetic replay, ROM parsing, stored
  fingerprints, independent Mesen pixels, and structural CLI validation.

## Where another editor may still be safer

- Production Graphics and Screen persistence.
- Familiar mature workflows not yet covered by Yaze's complete save/reopen
  tests.
- ROM hacks whose custom layout conventions are understood only by an existing
  project-specific tool.

Using another editor for one surface is not a failure. Keep a clean base ROM,
patch sources, and small reproducible changes so output can be compared.

## Dungeon visual parity rule

Yaze does not claim full 1:1 dungeon output from synthetic tests alone. Use the
smallest applicable proof tier:

1. Synthetic draw-registry replay.
2. Real-ROM parser/drawer comparison.
3. Room fingerprint regression.
4. Independent Mesen RGBA region.
5. Structural `z3ed dungeon-object-validate` output.

Runtime-only effects such as HDMA water control and moving BG layers may require
structural or emulator-state proof rather than a static pixel crop.

## Decision guide

- Use **Dungeon, Overworld, or Message** for a small Yaze beta edit on a copied
  ROM.
- Use **Palette** only after reading its two-step save procedure.
- Use **Graphics, Screen, Music, Hex / Memory, and vanilla Sprite** for the
  supported inspection subset described in the readiness matrix.
- Use ZScream, Hyrule Magic, source patches, and Mesen as comparison tools when
  they provide independent evidence or a currently safer production workflow.

See the [Beta Testing guide](../usage/beta-testing.md) for a bounded first pass.
