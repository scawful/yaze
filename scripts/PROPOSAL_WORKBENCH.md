# Editable proposals in Yaze

`proposal_workbench.py` prepares an isolated, ordinary Yaze project. The native
**Proposal** window works beside the Overworld or Dungeon editor. It does not
paint over screenshots or introduce a second ROM writer.

## Prepare a draft

Use a self-contained `.yazeproj` bundle whose ROM already contains the proposed
native map or room data. The helper copies that bundle, keeps the ROM baseline
and editable source/graphics bytes plus file hashes, relocates bundle-local paths, and preserves copied save receipts
and stale build output outside the draft. Only Yaze's real Save creates a new
save receipt. The original bundle is never edited.

```sh
python3 scripts/proposal_workbench.py prepare \
  --project /private/CoastCandidate.yazeproj --out /private/coast-review \
  --domain overworld --target 0x90 --proposal-id coast-04

python3 scripts/proposal_workbench.py prepare \
  --project /private/WaterCandidate.yazeproj --out /private/water-review \
  --domain dungeon --target 0x25 --graphics-entrance 0x31 \
  --proposal-id water-control-03
```

`--graphics-entrance` is the **unified editor entrance slot**, including the seven
spawn slots, not a regular entrance ID. The proposal owner must choose the slot
matching the intended room graphics context. The editor uses its existing
entrance-aware renderer and retains all room headers, layers, objects, doors,
actors and tags because the whole native project is copied.

`--reference /path/concept.png` keeps unbound artwork outside the editable project.
It is clearly counted as reference-only. It does not allocate CHR, palette slots,
Tile16 definitions or collision. Allocate through the existing native graphics
and page Tile16 systems before representing that artwork as editable game data.

## Open and edit

The new native entry point is `yaze --proposal=/private/coast-review`.
Generate its exact argument/environment record for a dock/menu or owned window
host with:

```sh
python3 scripts/proposal_workbench.py launch-plan /private/coast-review \
  --yaze /path/to/yaze --out /private/coast-launch.json
```

The host must apply the recorded `YAZE_APP_DATA_DIR` as well as `argv`. Each draft
uses a separate app-data directory and ROM. The helper does not launch, focus,
move, or stop applications. On macOS, use the existing dock/menu launcher and
background launch/Space ownership rules. Do not replace a user's open editor.

The native **Proposal** window provides:

1. **Open proposal…** selects `proposal.json`, refuses pending unsaved work, and
   opens its draft using normal project loading.
2. **Edit target** returns to the selected map or room with its graphics context.
3. **Save draft** uses the ordinary editor Save path.
4. **Save and export feedback** first saves; only on success does it write a new
   feedback JSON beside the proposal, containing the note, original/edited ROM
   hashes and changed-file hashes, including source-backed graphics files.

Use normal Build and Run after Save. Their existing receipt and source checks
remain in force; preparing a draft does not count as passing those checks.

For command-line feedback with ROM byte ranges:

```sh
python3 scripts/proposal_workbench.py feedback /private/coast-review \
  --out /private/coast-feedback.json
```

These packets contain private ROM/source material. They are local review packets,
not public distribution archives. No operation adopts edits into Main or another
project. Agent continuation compares the revised draft with its preserved
baseline and separately reviews changed definitions, graphics and collision.

## Current boundaries

- Native windows use Yaze's existing `WindowContent` docking. Multiple focused
  app instances are possible; detached OS viewports are not implemented by this
  change. SDL2/SDL3 renderer backends currently disable ImGui multi-viewports.
- Source graphics specifications and feature profiles are copied verbatim.
  Unsupported external paths, symlinks and multiple-ROM descriptors are refused.
- Feedback identifies files/bytes, not yet semantic object/Tile16 changes.
  Do not infer target-only changes from the selected map/room. The existing
  `dungeon-describe-room --include-objects` and
  `overworld-export-native-source --project-context`
  contracts are the next semantic comparison integration.
- The packet's native baseline is preserved, but side-by-side baseline rendering
  and graphic allocation assistance remain future passes.
- Native Save/reopen, focused startup rendering and Build/Run need an exact app
  package rehearsal before delivery. Unit tests alone do not prove that workflow.
