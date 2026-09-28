# Sprite catalog rules and Oracle migration plan

Status: M1 implemented and locally verified on 2026-09-23. Runtime migration not started.
Date: 2026-09-23.
Primary surface: `ui_ux_editor`; owner: `imgui-frontend-engineer`.

Agent entry point: [Sprite catalog and custom sprite standards](../architecture/sprite-catalog.md).
Use that document for the current implementation contract; this plan describes
migration order, evidence, and future scope.

## Decision

Make existing custom sprites visible and understandable in Yaze before changing
their runtime implementation. Adopt one family at a time. Keep current IDs,
placement bytes, registration, Prep/Main routines, source layout, and save data
unchanged during catalog adoption.

The first deliverable is the `$F0` Mermaid/Maple/Librarian family in a project-scoped
catalog and SpriteEditor browser. Initially expose source navigation, variant names,
and supported previews. Placement writes, draw-data writes, and runtime generation
are separate capabilities with separate acceptance gates.

The user approved M1 implementation. This plan authorizes no automatic conversion
of Oracle ASM or ROMs. M2 and later remain separate scopes.

## 1. Current evidence and limits

Reviewed working trees, not pristine release snapshots:

| Repository | HEAD at review | Relevant state |
|---|---|---|
| Yaze | `06343577b21a0a0fb46d4e5189bedbbd3c1b67ce` | Pre-existing deletions: `OWNERS`, `dungeon_object_validation_report.csv`, `dungeon_object_validation_report.json` |
| Oracle | `adb9b69a70507c0bba9e2ca9ccfd5e911619b9c8` | Substantial existing changes, including `Sprites/Objects/minecart.asm` and its track data; preserve them |

The companion `sprite-catalog-source-snapshot-2026-09-23.json` records selected
source hashes and the mechanical registry/include inventory. It is review evidence,
not a complete dependency manifest, placement census, or migration input contract.

Verified source facts:

- `Sprites/registry.csv` has 43 rows covering 39 distinct IDs. Duplicate-ID groups
  are `$07`, `$0A`, and `$F0`. This does not enumerate every runtime variant.
- A lexical scan of direct `Sprites/` includes in `Sprites/all_sprites.asm` finds
  67 include directives, including the generated IDs file. Of these, 23 other
  included paths are absent from registry path fields. These are source files,
  not 23 proven missing sprite registrations. Conditional assembly and transitive
  dependencies require the assembler's resolved build evidence.
- `Core/sprite_macros.asm` registers Prep/Main pointers and property bytes by main
  ID. The macro and expanded dispatcher enforce IDs no greater than `$F2`.
- `Core/sprite_new_table.asm` uses 24-bit pointers, enters targets with 8-bit A/X/Y,
  and guards null/out-of-range dispatch. Preserve this behavior until a separately
  reviewed runtime change is needed.
- `Room::EncodeSprites()` packs five subtype bits into dungeon placement bytes.
  `Sprite` also recognizes overlords for IDs `$01–$1A` when low subtype bits are 7.
- Yaze's overworld serialization writes Y, X, and ID; the inspected
  `Util/ZScreamNew/spritesmove.asm` loader consumes those three bytes without a
  separate subtype field. This is evidence for withholding an arbitrary subtype
  control, not proof that every active Oracle spawn path has been audited.
- `SpriteEditor::Save()` saves the selected ZSM asset. Static `SpriteOamRegistry`
  layouts are preview data, not descriptors for writing vanilla ROM draw tables.
- `SpriteBuilder` has stub creation/property/action methods. Its interface is not
  evidence of a usable compiler.

No assembler build, ROM comparison, emulator run, or placement census was performed
for this plan. Source-based variant classifications below are not runtime proof.

## 2. Catalog identity and selection rules

### R1 — Distinguish family, variant, asset, and placement

A family owns a main ID within a game profile. A variant has a stable textual key
such as `oracle.maple`, a family reference, and selection semantics. An asset owns
frames and animations. A placement stores the game's encoded data and resolves to
a variant when enough context is available.

Numeric IDs are addresses in the game's schema, not stable document identifiers.
Do not change existing IDs or reinterpret placements as part of adoption. Keep
catalogs per project/session; never install Oracle names into a process-global
vanilla table. Missing profiles fall back to raw IDs and existing vanilla behavior.

### R2 — Separate authored selector from runtime state

Record which stage consumes or changes a selector: placement load, Prep, Main,
spawn initialization, or transition restore. Do not read mutable `SprSubtype` as a
permanent identity unless the family's contract proves it is stable.

Each family declares one or more of these selection kinds:

| Kind | Required meaning |
|---|---|
| Exact value | An explicit encoded value chooses a variant |
| Parameter | A value is a track number or other argument, not a new character |
| Bit fields | Named non-overlapping fields with masks, ranges, and reserved combinations |
| Contextual | Room/map/world/progression conditions affect selection |
| Runtime assigned | Prep/Main/randomness chooses appearance or behavior |
| Child role | Parent code creates a dependent projectile, hitbox, or boss component |

Selection returns `resolved`, `context_required`, `unknown`, or `conflict` with an
explanation. Never silently choose the first catalog entry. Unknown raw values
remain intact through loading, editing unrelated fields, and saving.

Legacy fallback branches must be modeled explicitly. For `$F0`, subtype 1 selects
Maple, 2 selects Librarian, and other byte values follow Mermaid initialization.
Only 0/1/2 are initially offered as canonical authored choices. Existing subtype 3
must remain 3; display its observed Mermaid fallback rather than normalizing it.

For `$07`, zero selects Bean Vendor behavior and nonzero selects Village Elder
behavior. Do not misrepresent the implementation as an exact match on 1 alone.
Context-dependent flower behavior remains a contextual state of that family.

For declarative selectors, reject overlapping matches. For legacy ordered branches,
encode the source order explicitly and validate the fallback. Missing context must
not become an assumed zero flag. Do not execute arbitrary ASM to evaluate metadata.

### R3 — Capabilities are independent and evidence-backed

Track `browse`, `preview`, `place_dungeon`, `place_overworld`, `spawn_runtime`,
`edit_draw`, `edit_properties`, and `generate_runtime` separately. Each capability
has a reason and supporting evidence, not just a Boolean inherited from its family.

Distinguish frame preview, context-aware preview, and emulator-verified appearance.
Manual sheet/palette choices must remain visible as preview assumptions. Unsupported
assets get a labeled placeholder, not a plausible but wrong vanilla thumbnail.

A browsable runtime child is not automatically placeable. A previewable contextual
variant is not automatically a selectable placement. A registered ID is not proof
that a new sprite can use it safely.

### R4 — Encoding is owned by the placement backend

Dungeon encoding validates subtype `$00–$1F`, layer, coordinates, overlord rules,
hidden key markers, and all family reservations before any write. Do not rely on
bit masking to truncate invalid values. Preserve existing unknown records.

Overworld support initially recognizes existing contextual behavior. Arbitrary
overworld variant selection stays unavailable until the active loader and spawn
initialization path are proven. Any future side table needs its own versioned
placement identity, relocation rules, loader patch, and round-trip tests. Do not
borrow coordinate bits or allocate ROM space implicitly.

Runtime spawn contracts state when the subtype is assigned relative to property
initialization and Prep. Existing callers often assign `SprSubtype,Y` after
`Sprite_SpawnDynamically`; a generated Prep dispatcher cannot assume that value was
already available. Audit the actual spawn helper before introducing a wrapper.

### R5 — Allocation is conservative

During adoption, offer no automatic main-ID allocation or reassignment. Later,
allocate only from profile-declared, audited availability. Check vanilla ownership,
hooks, active registration sites, raw numeric references, spawners, placement data,
and reserved combinations. A missing CSV entry means unknown, not free.

Subtype expansion increases available definitions, not simultaneous sprite slots,
OAM capacity, DMA capacity, or graphics residency. Report those resources separately.

## 3. Source ownership and write rules

### R6 — One registration owner per family

Bind the existing `%Set_Sprite_Properties` invocation once. Other variants reference
it without emitting another registration. Aliases in CSV are not additional owners.
Use the active assembled configuration to detect competing registrations.

Defaults written into main-ID property tables are family-wide. A variant override
is editable only when its supported runtime initialization is defined. Show inherited
values and affected variants before a family-wide edit. Do not add a generic property
override path to existing ASM during catalog adoption.

### R7 — Keep one authority for each kind of data

| Data | Initial authority | Later ownership transfer |
|---|---|---|
| Existing IDs/ASM defines | Registry CSV and its existing generator | Separate explicit migration; no parallel writers |
| Existing behavior and dispatch | Hand-authored ASM | Remains hand-authored unless an individual family is deliberately converted |
| Catalog identity and bindings | Proposed project-linked versioned manifest | Manifest remains authoritative for metadata |
| Existing draw tables | Hand-authored ASM | Individual reviewed blocks may become generated assets |
| New managed frames | Versioned asset document | Generated ASM is an output, never a second editable master |
| ZSM interchange | Imported/exported file | Import is explicit; do not silently synchronize two editable masters |
| Existing placements | ROM placement records | Existing transaction-backed editor path |

Initially add metadata alongside the registry. Do not replace CSV or regenerate
`all_sprites.asm`. Separate registration, Prep, Main/Long, draw, behavior, and helper
bindings; a single `paths` field is insufficient.

Project configuration supplies the Oracle source root and profile. Store portable
relative paths under an explicitly configured root, resolve symlinks, and reject
output traversal outside approved roots. Do not assume neighboring checkouts or a
particular user's home directory. Catalog load must not run build commands.

### R8 — Preserve hand-authored code byte-for-byte

The first importer is read-only. It recognizes known table syntax and reports what
it cannot understand. It does not rewrite routines, macros, comments, line endings,
or unrecognized expressions. A no-edit session writes nothing.

Draw adoption requires a bounded ownership transfer: import a recognized table
block, emit equivalent tables to a separate candidate, inspect the diff, and prove
assembled equivalence before replacing the original block with an include. Keep
labels, scoping, bank locality, frame order, tile order, and widths compatible.

Record source digests on import. Recheck them before applying edits. If another
editor changes a source, stop that write and offer reload/diff; never overwrite it.
Detect external edits to generated outputs instead of silently discarding them.

### R9 — Draw formats retain their real constraints

Use a normalized signed-coordinate frame model, with loss-checked adapters for ZSM,
Oracle/ZSpriteMaker tables, and supported vanilla tables. Preserve OAM priority,
palette, flips, tile/name-table identity, size, order, and graphics requirements.

For the inherited draw template, `.nbr_of_tiles` is count minus one and table/frame
index arithmetic has byte-width constraints. Validate the selected backend's exact
limits, empty-frame behavior, offsets, and aggregate indices. Do not assume every
format supports a 256-tile frame just because a field is a byte.

Vanilla ROM edits require explicit revision/table descriptors and expected-byte
checks. Static preview layouts never authorize ROM writes. Unsupported procedural
drawing stays source-linked/read-only. Copying a supported visual into a new custom
asset is separate from rewriting the original sprite.

### R10 — Save, export, build, and apply are separate operations

Saving catalog metadata does not patch a ROM. Exporting ASM does not assemble it.
Building a candidate does not install it or change an emulator's loaded ROM.
Multi-file output uses a staging directory, validates all outputs first, and records
exact before/after hashes and recoverable backups before committing a batch.

Oracle placement edits target an explicitly selected base `oos<VERSION>.sfc`.
Patched `oos<VERSION>x.sfc` files are build/test artifacts, not placement-edit inputs.
Build comparisons use isolated copies with exact source/base identities. Do not run
a production-output build during a read-only migration review.

Rollback restores only the exact files/bytes owned by the failed migration and only
if their expected after-hashes still match. Never reset a checkout or restore an old
ROM over unrelated newer edits. No migration writes SRAM or savestates.

## 4. Initial family migration matrix

These are representative source-reviewed families, not a complete catalog. Wave A
means suitable for early metadata work, not already validated for editable placement.

| Family | Observed selection/ownership | Adoption rule | Wave |
|---|---|---|---|
| `$F0` Mermaid/Maple/Librarian | Registration, Prep, Long, and three draw routines in `NPCs/mermaid.asm`; `MapleHandler` in `NPCs/maple.asm`; Prep copies selection to `SprMiscE` | Exact canonical 0/1/2 choices plus explicit legacy fallback; preserve dispatch and separate source bindings | A: first vertical slice |
| `$07` Bean Vendor/Village Elder | `NPCs/bean_vendor.asm` owns registration; subtype zero vs nonzero selects behavior; elder code in `NPCs/village_elder.asm`; map/progression affects flower state | Shared drawing and contextual state; preserve nonzero aliases | B |
| `$11` Keese/Vampire Bat | `Enemies/keese.asm` owns registration and Prep; subtype 2 calls `Bosses/vampire_bat.asm`; Vampire Bat spawns subtype-1 Fire Keese | Capture spawn dependency and variant-specific HP initialization; prove spawn order before generated dispatch | B |
| `$A0` Deku NPCs | Subtypes 1/2 choose Butler/Princess, but areas `$2E/$2F` take precedence; progression changes state/despawn | Context-aware resolution with explicit precedence, not a subtype-only picker | C |
| `$0A` Eon Owl/Kaepora Gaebora | `NPCs/eon_owl.asm` checks map `$0E` and progression; Main assigns subtype 1 | Contextual appearance; do not claim subtype 1 alone places Kaepora | C |
| `$B8` Zora family | `NPCs/zora_princess.asm` owns registration; `NPCs/zora.asm` prioritizes room `$0105`, world flag, then zero/nonzero subtype | Bind actual registered Prep separately from similarly named helpers; missing context remains unresolved | C |
| `$F1` Korok | Prep assigns `GetRandomInt & 3`; draw selects Makar/Hollo/Rown, with 3 falling back to Makar | Browse all appearances, mark selection runtime-assigned; fixed appearance needs a separate gameplay change | C |
| `$A3/$AF/$B0` Minecart/Mineswitch/SwitchTrack | Subtype indexes track/switch state and related data | Parameter controls and cross-reference validation; no character-style subtype allocation; preserve dirty minecart work | C |
| `$C1` Dark Link | Subtype 1 used by spawned sword-damage child; subtype 5 enters Ganon-related path | Runtime-child/context bindings; retain source ownership and hide unproven direct placement | D |
| `$14/$2C/$B1/$CF` and related spawners | Business Scrub, Goriya, Puffstool, and Kydreeok components reuse subtype for spawned roles | Audit parent/child initialization, lifetime, draw dependencies, and helper calls before write support | D |
| `$EF` Poltergeist | Prep and spawn code inspect subtype bits and special values `$10/$11` | Preserve the existing field interpretation; requires dedicated adapter and fixtures | D |

The 23 direct source includes absent from CSV path fields include Maple and Keese.
This confirms that CSV-only import misses relevant dependencies. The inventory step
must reconcile registry rows, actual registration sites, included files, hooks,
spawners, and placement data. Do not generate registry entries for every include.

## 5. Proposed manifest shape

This YAML is a design sketch, not an implemented schema or valid import file.
Serialization should follow existing project conventions at implementation time.

```yaml
schema_version: 1
profile: oracle
families:
  - key: oracle.mermaid_family
    main_id: 0xF0
    ownership: legacy_asm
    registration:
      path: Sprites/NPCs/mermaid.asm
      prep: Sprite_Mermaid_Prep
      main: Sprite_Mermaid_Long
    selector:
      authored_field: dungeon.subtype
      consumed_at: prep
      runtime_cache: SprMiscE
      canonical_values: [0, 1, 2]
      fallback: oracle.mermaid
    variants:
      - key: oracle.mermaid
        authored_value: 0
        draw: {path: Sprites/NPCs/mermaid.asm, label: Sprite_Mermaid_Draw}
      - key: oracle.maple
        authored_value: 1
        draw: {path: Sprites/NPCs/mermaid.asm, label: Sprite_Maple_Draw}
        behavior: {path: Sprites/NPCs/maple.asm, label: MapleHandler}
      - key: oracle.librarian
        authored_value: 2
        draw: {path: Sprites/NPCs/mermaid.asm, label: Sprite_Librarian_Draw}
```

The final schema must also carry source evidence, graphics/palette dependencies,
capability reasons, reserved values, preview context, and migration ownership.
Forward-version handling must preserve unknown data or make the document read-only;
an older editor must not silently discard fields.

## 6. Delivery sequence and acceptance gates

### M1 — Catalog support; no runtime migration

Implement a pure catalog/resolver under `src/zelda3/sprite/`, project configuration
under `src/core/project.*`, and browser integration under `src/app/editor/sprite/`.
Keep ImGui out of the catalog and ASM imports out of the per-frame render loop.

Use only `$F0` in the first executable fixture. Resolve canonical variants and the
legacy fallback, navigate each source binding, and expose capability limitations.
If frame import is not yet reliable, use an honest placeholder. Add per-document
identity, dirty state, and undo boundaries before editable asset documents.

Acceptance: catalog open/reload changes no source or ROM bytes; unknown subtypes
survive; unsupported profile/version stays read-only; two ROM sessions do not leak
names or assets; missing source files do not block unrelated editor functions.

### M2 — Placement integration and read-only draw import

Use the catalog in the dungeon picker and inspector through existing placement
serialization. Add usage discovery that distinguishes decoded placement evidence
from literal source references. Report incomplete coverage rather than claiming
there are no callers or placements.

Import recognized `$F0` draw tables and reuse the existing rendering path. Verify
each variant's required sheets/palettes and frame mappings. Expand to `$07/$11`
only after their contextual/spawn rules are represented.

Acceptance: changing a supported dungeon variant produces only the intended
placement change; all coordinates, layer, hidden markers, unrelated subtype bits,
and other room records survive. Test `$1F/$20` boundary rejection and overlord
reservations. Explicitly test fallback values, zero/nonzero aliases, and missing
context. No overworld subtype encoding is added in this milestone.

### M3 — Adopt draw assets one family at a time

Add a managed frame document and loss-checked ZSM/ASM adapters. First produce a
candidate for `$F0` without modifying source. Transfer ownership only after no-edit
assembled equivalence and bounded source-diff review. Then make one small visual
edit and verify its intended effect. Keep behavior ASM and registration intact.

Acceptance: ZSM semantic round-trip; supported ASM table equivalence; unsupported
syntax fails without writes; stale hashes and shared-asset edits are surfaced;
atomic save failure preserves originals; exact candidate ROM diff stays in declared
ranges. Test actual source ownership across both `mermaid.asm` and `maple.asm`.

### M4 — New managed variants and optional generated families

Finish or replace the stub builder only behind a documented backend contract.
Prefer new families over rewriting established dispatch. Define A/X/Y widths, DB,
stack balance, X slot preservation, scratch RAM, near/long calls, and who draws and
checks active state. Never wrap a complete Long routine as though it were a Main
helper. Keep source imports and generated runtime as distinct capabilities.

Acceptance: compile fixture code with the real assembler; reject duplicate
registrations, illegal IDs, unresolved symbols, invalid branches, bank overflow,
and incompatible overrides. Verify caller/Prep ordering with real spawn paths.
Undefined subtype behavior is explicit and bounded. Do not invent unallocated WRAM
for cached identity. Existing families remain on their legacy implementation.

### M5 — Contextual families, vanilla editing, and runtime proof

Adopt Wave C/D by dedicated adapters and context fixtures. Add a small separately
verified set of vanilla draw-table descriptors. Treat optional expanded overworld
placement as a distinct design requiring loader and serialization approval.

Runtime evidence for a promoted family covers real load/Prep/Main, visual frames,
graphics/palette residency, interaction, pause/inactive behavior, despawn/respawn,
room transitions, and parent/child behavior where applicable. Compare a fixed
baseline and candidate with exact hashes and reproducible fixture context.

Source checks, unit tests, assembly checks, ROM diffs, emulator observations, and
manual gameplay acceptance are separate results. A passing preview is not runtime
proof, and injected subtype state is not proof that normal placement initializes it.

## 7. Review checks performed

Executed from the Oracle repository:

```sh
python3 Scripts/Validate/validate_sprite_registry.py --strict --no-ids
```

Result: `sprite registry OK`. This checks the current validator's ID/duplicate
rules only. It does not prove complete registration coverage or runtime safety.

The validator's default generated-ID check refers to
`Scripts/generate_sprite_registry.py`, while the current generator is under
`Scripts/Generate/`. Do not count `--no-ids` as an ID-file freshness check. A separate
temporary-output generation/comparison is recorded in the companion evidence file.
That generation exited 0 and matched `Sprites/sprite_registry_ids.asm` byte-for-byte.
No generated file was written back into Oracle.

Coordination attempt:

```text
touch: /Users/scawful/.context/agent-universe/events.jsonl: Operation not permitted
```

No coordination record was created. This session can write the Yaze planning
artifacts but cannot update that external log. Preserve that limitation in handoff.

## 8. Next implementation scope

Approved M1: a project-scoped catalog/resolver and a `$F0` fixture, with browser/source
navigation, explicit support states, and no ASM/ROM writer. M2 and later write
capabilities require their stated acceptance evidence; they are not implied by M1
completion.

## 9. Future use case: intro patrol and arrival presentation

Reviewed the Oracle-owned `Docs/Planning/Plans/intro_stealth_and_arrival_scaffold.md`
and `Data/planning/intro_stealth_contract.json` on 2026-09-23. Resolve these paths
under the configured Oracle source root. They are design scaffolds, not executable
catalog schemas or runtime inputs. Yaze owns this cross-reference only.

The future logical actor is `oracle.stalfos_patrol`. Its family, main ID, subtype,
registration owner, and RAM remain unallocated. Existing `$3F` blockers remain
unchanged. Patrol, notice, chase, search, and return are transient behavior states;
route and encounter profile values are not variant identities.

M1's executable v1 catalog requires a concrete family/main ID. Keep this unresolved
actor in the design scaffold; never use a dummy ID to make it parse. A future schema
must represent an unbound actor, shared draw references, source-owned behavior,
context requirements, encounter parameters, and unavailable placement capabilities
explicitly. This requirement does not expand M1 into runtime generation.

Joint Oracle/Yaze decisions before any allocation or placement export:

1. Prove the active overworld loader, conditional build configuration, and normal
   spawn path. The Oracle review found no include of `spritesmove.asm`; its presence
   alone does not identify the active loader.
2. Select transport without borrowing coordinate bits or assuming dungeon encoding.
   If a side table is chosen, define stable placement identity, phase, duplicate
   coordinates, reorder/relocation/deletion, versioning, and ROM-range ownership.
3. Establish selector availability relative to Prep, property initialization,
   dynamic spawn, transition restore, and respawn. Specify defaults and fallbacks.
4. Audit a compatible family, shared defaults, graphics residency, scratch RAM, and
   parent/child responsibilities before enabling generation or placement.
5. Validate a single actor with normal load/spawn, interaction, cleanup, and an
   isolated candidate ROM before adding encounters.

The arrival sequence is presentation-only and requests no gameplay sprite ID. Its
OAM/assets and runtime controller belong to a separate presentation contract, not a
placeholder gameplay family in the sprite catalog.

An acknowledgment to Oracle thread `01a0d06d-ee1d-7893-be24-c52b3a73c09c` was attempted
but the messaging tool required approval while this session's approval policy is
`never`. Delivery was not confirmed; this section records the review and decisions.

## 10. M1 delivery and verification

Implemented a project/session-local JSON catalog and resolver, the executable
`assets/sprite_catalogs/oracle_f0.json` fixture, optional **Sprite Catalog** window,
explicit project path selection, and bounded read-only source navigation. The UI
reports unavailable previews and placement/edit/generation capabilities. Source
inspection stays inside the catalog panel; it does not invoke an ASM editor or
write to source. Existing vanilla and ZSM editing paths remain separate.

Canonical subtype resolution, legacy fallback preservation for all 256 byte
values, malformed/unsupported metadata, traversal/symlink rejection, missing source,
failed reload, project settings round-trip, independent editor sessions, and actual
headless panel drawing have focused coverage. The six fixture bindings were also
checked against the current Oracle source. The 24 Oracle source hashes in the
original review snapshot still matched at verification time; that snapshot's Yaze
hashes intentionally describe the earlier pre-implementation state.

Successful build commands:

```sh
cmake --preset mac-ai
CCACHE_DIR=/tmp/yaze-sprite-catalog-ccache CLANG_MODULE_CACHE_PATH=/tmp/yaze-sprite-catalog-clang cmake --build --preset mac-ai --target yaze_test_unit yaze --parallel 4
```

The task-local cache paths were needed because the default ccache directory was
outside the sandbox. The application output is
`build/presets/mac-ai/bin/Debug/yaze.app/Contents/MacOS/yaze`.

Tests were listed before execution to verify selection:

```sh
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='SpriteCatalog*:ProjectPathsTest.*:SpriteEditorPreviewTest.*:SpriteEditActionTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='SpriteCatalog*:ProjectPathsTest.*:SpriteEditorPreviewTest.*:SpriteEditActionTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='*LayoutPresets*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='LayoutPresetsTest.*'
```

Results: 36 focused tests and 6 layout tests passed, with no skips. This includes
15 new catalog/source/session tests. Agent `protocol-audit.sh` and
`test-universe-coord.sh` passed. Documentation links, fenced JSON examples,
new-code formatting, and `git diff --check` passed.

No Oracle ASM/ROM/save writes, game-runtime build, emulator test, installed-app
replacement, or physical desktop visual acceptance was performed. Headless panel
drawing proves session-specific content and error rendering, not visual layout
quality on a desktop. M2 remains the next separate implementation scope.


## 11. Live visual-authoring extension (2026-09-23)

User continuation authorized live frame editing, previews, animation authoring,
and shared abstractions for custom and vanilla sprites. Implemented a shared ZSM
visual-authoring layer; see [usage](../../public/usage/sprite-authoring.md) and
[the updated contract](../architecture/sprite-catalog.md#live-visual-authoring-extension).

Delivered: source-scoped literal draw-copy import, vanilla static-layout copies,
ZSpriteMaker coordinate conversion, signed offset controls, priority storage,
frame/tile duplication, frame clipboard, endpoint repair on deletion, frame-count
bounds, playback catch-up, and validated data-only ASM clipboard export. Removed
inactive vanilla frame/OAM controls in favor of the explicit copy workflow.

Validation commands (repository root):

```sh
CCACHE_DIR=/tmp/yaze-sprite-catalog-ccache CLANG_MODULE_CACHE_PATH=/tmp/yaze-sprite-catalog-clang cmake --build --preset mac-ai --target yaze_test_unit yaze --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='SpriteAuthoring*:SpriteCatalog*:ProjectPathsTest.*:SpriteEditorPreviewTest.*:SpriteEditActionTest.*:LayoutPresetsTest.*'
YAZE_ORACLE_SOURCE_ROOT=/path/to/oracle-of-secrets build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='SpriteAuthoring*:SpriteCatalog*:ProjectPathsTest.*:SpriteEditorPreviewTest.*:SpriteEditActionTest.*:LayoutPresetsTest.*'
git diff --check
```

Local validation used the current Oracle checkout via `YAZE_ORACLE_SOURCE_ROOT`:
54 tests passed, no skips. The optional source test skips when that variable is
absent. Mermaid imported four frames, Librarian two; Maple's specialized draw
routine was rejected. ZSM coordinate rendering has a pixel assertion, table fields
have a round-trip check, and public editor import/clipboard/undo operations have
source-preservation coverage. App and unit targets built; linker duplicate-library
warnings remain. These are source/unit results, not manual GUI or ROM/emulator proof.

Next: persist asset identity and graphics/palette bindings with source hashes,
implement a reviewed Maple adapter, then model common behavior actions against
verified Oracle/vanilla calling and RAM contracts. There is still no ASM write-back,
new-ID allocator, complete vanilla draw-data decoder, generated behavior runtime,
or emulator hot-edit backend. Existing source and ROM authority is unchanged.

## 12. Persistent assets and Maple adapter (2026-09-23)

The approved continuation adds project-owned asset bindings and Maple's explicit
draw adapter. `core/sprite_asset.h` and `sprite_asset_json.h` define and validate
version-1 records. `sprite_editor_assets.cc` handles opening saved assets, binding
controls, source checks, and guarded data export. ZSM bytes remain compatible with
ZSpriteMaker; metadata belongs to the project descriptor.

Delivered behavior:

- Saved ZSM paths, catalog keys, adapter selection, eight graphics sheets, eight
  palette rows, and draw-source SHA-256 survive native project Save/Open.
- Document selection and undo/redo restore the asset's graphics bindings.
- Source drift or missing source preserves edited assets, allows local ZSM saving,
  and blocks the bound ASM export candidate until reviewed through a fresh import.
- Maple's reviewed constant-X/16x16 driver imports two frames. The adapter checks
  its complete normalized instruction fingerprint; changed instructions fail.
  Export rejects tile edits that the specialized driver cannot represent.
- Automatic reopening validates a bounded ZSM byte snapshot. Missing/corrupt files
  do not stop other assets. Invalid saves are rejected before opening the target.

Verification:

```sh
CCACHE_DIR=/tmp/yaze-sprite-catalog-ccache CLANG_MODULE_CACHE_PATH=/tmp/yaze-sprite-catalog-clang cmake --build --preset mac-ai --target yaze_test_unit yaze --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='SpriteAuthoring*:SpriteCatalog*:ProjectPathsTest.*:SpriteEditorPreviewTest.*:SpriteEditActionTest.*:LayoutPresetsTest.*:*ZSprite*:*Zsm*'
YAZE_ORACLE_SOURCE_ROOT=/path/to/oracle-of-secrets build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='SpriteAuthoring*:SpriteCatalog*:ProjectPathsTest.*:SpriteEditorPreviewTest.*:SpriteEditActionTest.*:LayoutPresetsTest.*:*ZSprite*:*Zsm*'
scripts/agents/protocol-audit.sh
git diff --check
```

Result: both build targets passed; 62 selected tests passed with no skips against
current Oracle draw source. The optional source test requires the explicit source
root environment variable. Protocol audit and relative documentation links passed.
Existing duplicate-library linker warnings remain. Coverage includes native project
reopen, source drift/missing source, palette selection, document switching, metadata
undo/redo, corruption isolation, rejected-save preservation, Maple's expected tiles,
changed-driver rejection, and export-time source checking.

Limits: manual GUI and emulator acceptance remain unverified. Bindings do not hash
ROM graphics or all behavior/build dependencies. Saving ZSM and saving the project
are separate operations, not one atomic transaction. No ASM write-back, runtime
behavior generation, placement writer, or ROM patching was added. Next milestone:
reviewed behavior/action adapters with explicit caller, RAM, timer, and spawn-order
contracts.


## 13. Shared action authoring and candidate generation (2026-09-23)

The Behavior tab now authors `oracle_actions_v1` models: up to 16 actions,
references to ZSM animations, player blocking, solicited dialogue, XY velocity,
tile bounce, and Timer A transitions. Action animation preview uses the existing
canvas; collision, movement, and dialogue are not simulated there. Entries write
explicit state and timer values. Transitions do not run two updates in one call.

Models persist in version-2 project asset records and participate in editor
undo/redo. Version-1 visual-only records remain compatible. Referenced animations
cannot be deleted, and incoming action transitions must be redirected before
removing their target. No subtype selector is encoded in an action index.

Export produces a reviewable ASM candidate using three reviewed Oracle source
contracts, with normalized-code fingerprints and exact bound-source hashes.
Copying a candidate repeats validation and source checks. Neither generation nor
copying installs routines, modifies ASM, allocates IDs, or patches ROMs. Existing
sprite behavior is not automatically imported. Vanilla visual copies may target
this Oracle backend; a native vanilla behavior backend is still pending.

Implementation: `src/core/sprite_behavior{,_json}.h`,
`src/app/editor/sprite/sprite_behavior{.h,_panel.cc}`, asset serialization,
SpriteEditor animation/reference management, and the safe whole-source reader.
See [the agent contract](../architecture/sprite-behavior.md) and
[the user guide](../../public/usage/sprite-behavior.md).

Verification commands:

```sh
CCACHE_DIR=/tmp/yaze-sprite-catalog-ccache CLANG_MODULE_CACHE_PATH=/tmp/yaze-sprite-catalog-clang cmake --build --preset mac-ai --target yaze_test_unit yaze --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='SpriteBehavior*:SpriteAuthoring*:SpriteCatalog*:ProjectPathsTest.*:SpriteEditorPreviewTest.*:SpriteEditActionTest.*:LayoutPresetsTest.*:*ZSprite*:*Zsm*'
YAZE_ORACLE_SOURCE_ROOT=/Users/scawful/src/hobby/oracle-of-secrets build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='SpriteBehavior*:SpriteAuthoring*:SpriteCatalog*:ProjectPathsTest.*:SpriteEditorPreviewTest.*:SpriteEditActionTest.*:LayoutPresetsTest.*:*ZSprite*:*Zsm*'
scripts/agents/protocol-audit.sh
git diff --check
```

Result: both targets built; **69 tests across 11 suites passed with no skips**.
The seven new tests cover model validation, guarded deletion, v1/v2 compatibility,
reviewed-source drift, persistence with undo/redo and fresh export checks, and
candidate assembly against reviewed macro bodies in an in-memory synthetic ROM.
External helper addresses in that assembly test are explicit link symbols; the
helper code is not executed. Existing duplicate-library linker warnings remain.
Protocol audit and relative documentation links passed.

Logs for this local run: `/tmp/yaze-sprite-behavior-build-final.log`,
`/tmp/yaze-sprite-behavior-test-list.log`, `/tmp/yaze-sprite-behavior-tests.log`.
These temporary logs are evidence pointers, not portable dependencies.
Manual GUI acceptance, a complete Oracle build, emulator execution, and gameplay
acceptance remain unverified. Source fingerprints do not attest loaded ROM bytes.

## 14. Patrol integration readiness boundary (2026-09-23)

Reviewed the incoming Oracle owner packet:
`Docs/Planning/Reviews/stalfos_patrol_packet_2026-09-23.md` in the Oracle checkout.
This is a local response record; no cross-thread delivery is claimed.

`oracle.stalfos_patrol` remains unbound. No existing family/sole registration owner,
main ID, subtype value/default, or selector transport has been jointly selected.
The new action authoring backend does not supply those decisions. Do not add a
placeholder registration/catalog variant, reuse overworld coordinate bits, or
copy dungeon placement encoding into overworld records.

The precise integration choice is an approved family with verified RAM, property,
and draw compatibility, plus a selector carrier and initialization phase that
survive loader/property resets before the selected Prep dispatch. Current source
inspection confirms the normal loader clears `$0E30,X` at `$09C804`, and the Prep
path calls `SpritePrep_LoadProperties` at `$06864D`. Dynamic spawn/respawn ordering
needs its own evidence; a normal placement result cannot establish it.

Yaze's existing overworld I/O reads three-byte Y/X/ID records. No patrol selector
round-trip through export, game load, dynamic spawn, and respawn is established.
Retain the packet's `$3F` blockers and leave selector allocation unresolved until
that end-to-end contract is chosen and tested.

The opening owner's reported contact policy (half-heart damage, normal
knockback/invulnerability, brief guard recovery pause) belongs to the patrol
behavior proposal; it is not part of `oracle_actions_v1`. Full search behavior,
probe ownership, and shared runtime hooks also remain outside this action backend.
No Oracle ASM, ROM, save, or placement data was changed for this milestone.


## 15. Bounded patrol binding response (2026-09-23)

See [the binding handoff](stalfos-patrol-binding-handoff-2026-09-23.md) for the
accepted moving-guard audit target, exact normal/dynamic initialization distinction,
conditional contextual-selector recommendation, smallest integration packet, and
sprite-name audit disposition. No family member, selector, cache RAM or hook was
allocated. Cross-thread delivery was blocked by tool approval policy; the handoff
is recorded locally for the opening and patrol owners.
