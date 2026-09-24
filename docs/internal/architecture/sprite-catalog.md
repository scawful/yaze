# Sprite catalog and custom sprite standards

Status: M1 catalog plus live visual-authoring implementation contract. Applies to SpriteEditor, project catalogs,
source bindings, and future sprite migration work.

Read this document before changing sprite catalog code, adding a custom sprite
definition, migrating Oracle sprite assets, or extending subtype placement support.
Catalog metadata remains read-only. The visual-authoring extension below adds detached
draw-data copies and ZSM editing. Future runtime features in the migration plan
are not implemented capabilities.

## 1. Authority and ownership

| Concern | Authority |
|---|---|
| Current catalog/editor contract | This document and the implementation listed below |
| Migration sequence and evidence gates | [Sprite migration plan](../plans/sprite-catalog-migration-2026-09-23.md) |
| Initial source-review evidence | [Source snapshot](../plans/sprite-catalog-source-snapshot-2026-09-23.json); hashes describe the review date, not guaranteed current state |
| Existing Oracle IDs | Oracle `Sprites/registry.csv` and its generated `Sprites/sprite_registry_ids.asm` |
| Existing Oracle registration, properties, behavior, and draw data | The active hand-authored ASM and resolved build configuration |
| Catalog metadata | Explicitly selected project catalog JSON |
| Placement bytes | Existing dungeon/overworld ROM data and their serializers |

Resolve Oracle paths under a configured source root. Do not assume a neighboring
checkout, a home directory, a ROM version, or a fixed host. Verify live source when
a document disagrees with code. Do not expand this standard into an unrelated ASM
cleanup or fix a game's behavior while importing metadata.

The initial catalog does not supersede Oracle's registry or register any sprite.
One family owns each main-ID registration. CSV aliases and catalog variants do not
grant permission to emit extra `%Set_Sprite_Properties` calls.

## 2. Terms and identity

Use these terms consistently:

| Term | Meaning |
|---|---|
| Family | Owns one main sprite ID, registration, and shared property defaults |
| Variant | Named identity within a family, selected by declared rules |
| Asset | Frames, OAM tiles, animations, and graphics dependencies; editable copies use ZSM |
| Placement | Encoded instance in a dungeon or overworld; M1 does not edit placements |
| Runtime state | Mutable behavior/action/timer data; not necessarily variant identity |
| Encounter parameter | Route, track, profile, or other instance configuration |

Stable keys such as `oracle.maple` identify catalog entries. Numeric IDs and subtype
values identify game encodings. Do not rename a stable key merely to move an ID,
and do not move an existing ID during metadata adoption.

Never assume every subtype identifies a character. Minecart track indices, spawned
hitboxes, random Korok appearances, and context-selected NPCs need different rules.
Missing context must remain unresolved, not be treated as a zero flag.

## 3. Implemented M1 boundary

M1 provides a project-scoped catalog, exact-value/fallback resolution, and read-only
source navigation. It has no catalog serializer, ROM dependency, assembler call,
generated runtime, or placement writer. The separate visual-authoring adapter can
import supported literal draw tables into detached ZSM documents.

The catalog belongs to each SpriteEditor instance. Do not put it in a singleton or
feed its names into the global vanilla name provider. Switching/rebinding a project
must clear stale catalog and source content. Loading a project without settings
must not retain settings from the prior descriptor.

Catalog errors are local to the catalog panel. Missing source roots/files must not
prevent catalog browsing, ROM loading, ZSM editing, or unrelated editors from
working. Unsupported or malformed catalogs are rejected without changing their
files. A failed reload must not leave the previous catalog displayed as current.

Current capability statements:

| Capability | M1 |
|---|---|
| Browse family/variant metadata | Supported |
| Inspect a raw subtype without modifying it | Supported |
| Navigate to a bound source label | Supported, read-only, when source exists |
| Custom visual preview | Unavailable; show a labeled placeholder |
| Dungeon/overworld placement editing through the catalog | Unavailable |
| In-place draw/property editing through the catalog | Unavailable; explicit literal draw-copy import opens ZSM authoring |
| Runtime installation or new-ID allocation | Unavailable; reviewed behavior candidates can be generated separately |

Browsing does not mutate ZSM documents. The explicit **Import draw copy** action
creates a new ZSM document and does not confer runtime support.

### Live visual-authoring extension

Read [the authoring guide](../../public/usage/sprite-authoring.md) for supported UI
operations and limits. Implementation lives in
`src/app/editor/sprite/sprite_authoring.h`, `sprite_draw_import.h`,
`sprite_editor.cc`, and `sprite_drawer.cc`. Focused coverage is in
`test/unit/editor/sprite_authoring_test.cc`.

- Preserve ZSpriteMaker coordinate encoding: stored bytes are canvas coordinates;
  draw offsets subtract X=128 and Y=112. Do not reinterpret those bytes as int8.
  Upstream evidence: `MainWindow.xaml.cs`, `GetASM()`, computes these differences.
- All frame consumers use the same conversion, including vanilla preview copies.
  The static vanilla registry remains a preview approximation, not a write map.
- Frame deletion remaps animation endpoints; frame creation is bounded to 256.
  Playback sanitizes malformed ranges locally without rewriting imported assets.
  Frame edit transactions must not span switching documents. Clipboard data belongs
  to an editor instance; Paste appends an independent frame copy.
- Literal import is scoped to one resolved source label using the catalog's safe
  source reader. Require all seven tables and valid dimensions and encodings.
  Never infer missing tables, evaluate ASM expressions, or silently use another
  global label's data. Mermaid/Librarian use `literal_v1`. Maple uses an explicitly
  selected `oracle_maple_v1` adapter (see below).
- Imported animation timing and action boundaries are unknown. The default sequence
  is explicitly a preview. Graphics/palette selection remains author-supplied and
  is not persisted by ZSM; project asset bindings persist those choices separately.
- Draw export emits data only, never `%Set_Sprite_Properties`, hooks, or a runnable
  replacement. Nonempty frames and at most 128 flattened tiles are required by
  the inherited driver's byte `ASL` indexing of word offsets. Reject nonzero Z
  and out-of-range attributes. Bank placement and OAM capacity still need review.
- Preview is not emulation. BG priority is stored but not composited against game
  backgrounds. Behavior candidates have a separate reviewed profile; source
  write-back, new sprite registration, placement, and emulator hot editing remain
  separate capabilities requiring verification.

### Persistent asset bindings and Maple adapter

`core/sprite_asset.h` defines project-owned asset metadata. Native projects store
version-1 (visual) or version-2 (with behavior) JSON records under `[sprite_assets]` (`asset_0`, `asset_1`, ...); JSON
projects store a `sprite_assets` array. A version-1 record contains exactly:
`version`, `zsm_path`, `catalog_key`, `draw_adapter`, `source_path`, `source_label`,
`source_sha256`, `sheets`, and `palette_rows`. Each of the eight palette rows has
`group` and `index`. Supported groups: `auto`, `global_sprites`, `sprites_aux1`,
`sprites_aux2`, `sprites_aux3`. `auto` is a heuristic, never proof of game palette
selection. Explicit missing rows must produce an error, not fall back.

The project-relative ZSM path identifies a saved asset; several independent assets
may refer to one catalog variant. Neither a catalog key nor a binding registers a
sprite. Saving ZSM updates the binding in project memory; normal project Save
persists it. Do not claim an atomic transaction across the two files. Loading an
older project clears the optional asset list. A missing/malformed asset is local to
the sprite panel and must not stop other assets or ROM loading.

Hash exact source bytes from the same bounded read used for import.
`SpriteSourceDocument::content` supplies those bytes and
`core::ComputeSourceArtifactSha256` supplies SHA-256. Do not hash normalized text
for provenance. Opening an asset or an explicit check can report drift, but only a
fresh check immediately before export permits the bound export candidate. Failed
checks preserve edits and the original hash. Saving is not source acknowledgement.
No in-place overwrite or automatic reimport is implied.

Catalog draw bindings optionally specify `adapter`; absence means `literal_v1`.
The only additional supported value is `oracle_maple_v1`, valid on draw bindings.
Unknown adapters are rejected. Maple's driver assigns X directly from `$00` and
writes size `$02`; it has no `.x_offsets` or `.sizes` table. The adapter verifies the
complete instruction sequence before supplying X=0 and size=16×16 per tile.
The reviewed driver SHA-256 is
`45fc82df667e131b3979473c4c6e9d12c890a7fb1e7589b024405a98b3987e6f`, computed by
removing comments and whitespace from the body before `.start_index`, excluding
its opening brace. Instruction changes require review, not an automatic fingerprint
update. Literal table changes still pass the generic range/shape validator.
Maple-bound export rejects edits incompatible with the fixed X/size contract.

Per-document binding edits participate in sprite undo/redo. Document selection must
restore its own graphics context. ZSM bytes stay compatible with ZSpriteMaker;
project metadata must never be appended to the upstream binary format. Automated
reopen uses a bounded, validated byte snapshot before decoding a ZSM asset.

### Shared behavior/action model

The **Behavior** tab and `oracle_actions_v1` candidate backend are implemented.
Read [the behavior contract](sprite-behavior.md) before touching actions, helper
adapters, generation, or profile source checks. Asset record version 2 adds a
`behavior` object; version-1 records remain supported. Actions are runtime state,
not subtypes. The first profile covers animation, solicited dialogue, player
blocking, fixed XY movement/tile bounce, and timer transitions. It does not import
existing scripts or install code into any family. The editor preview runs only the
selected animation, not a game simulation.

Do not add generic behavior presets based only on similar function names. An
adapter must document inputs, clobbers, RAM ownership, bank/call convention,
initialization order, timer semantics, and its evidence for each supported profile.
For example, Oracle `%PlayAnimation` mutates `SprFrame` and `SprTimerB`; a UI
sequence alone does not prove equivalent game timing or state transitions.

## 4. Version 1 executable schema

The complete fixture is [oracle_f0.json](../../../assets/sprite_catalogs/oracle_f0.json).
It is an executable example for the current loader, not an exported Oracle registry.

```json
{
  "schema_version": 1,
  "profile": "oracle",
  "families": [
    {
      "key": "oracle.example_family",
      "name": "Example family",
      "main_id": 240,
      "selector": {
        "kind": "exact_with_fallback",
        "fallback_variant": "oracle.example"
      },
      "sources": [
        {"role": "prep", "path": "Sprites/example.asm", "label": "Example_Prep"}
      ],
      "variants": [
        {
          "key": "oracle.example",
          "name": "Example",
          "authored_subtype": 0,
          "sources": [
            {"role": "draw", "path": "Sprites/example.asm", "label": "Example_Draw"}
          ]
        }
      ]
    }
  ]
}
```

The example demonstrates structure only; it does not allocate `$F0` to an example
sprite. Use the actual fixture for Mermaid/Maple/Librarian.

Rules enforced by the loader:

1. `schema_version` is integer 1; `profile` is `oracle`. Unknown versions/profiles
   are unsupported. Unknown fields, duplicate JSON fields, and malformed types are
   rejected rather than silently discarded.
2. Families and variants are nonempty arrays. Family/variant keys are globally
   unique; main IDs are unique and in `0–242` (`$00–$F2`). This range check does not
   establish slot availability.
3. `exact_with_fallback` is the only implemented selector. Canonical
   `authored_subtype` values are integers `0–31`, unique within the family.
   `fallback_variant` must reference a variant in that family.
4. Source binding arrays are nonempty and roles are unique within each array.
   Each binding has a `role`, source-root-relative `path`, and `label`. Family
   bindings and variant bindings have separate role namespaces.
5. Documents are limited to 1 MiB and JSON nesting is bounded. There is no support
   for executable hooks, arbitrary expressions, or commands in catalog metadata.

The resolver accepts an inspected raw byte `0–255`. Exact canonical values win;
other bytes resolve through the declared fallback. It returns the original raw
value and whether fallback occurred. Out-of-range integers and unknown families
do not resolve; they are never truncated. Browsing a byte above 31 does not make it
encodable as a dungeon placement.

Unknown/contextual/bit-field selectors require a future schema change with tests.
Do not force them into `exact_with_fallback` or invent a numeric ID for an unbound
design. The parser cannot establish that metadata matches ASM; source review and
runtime evidence remain separate duties.

## 5. Source navigation and project configuration

Native project descriptors store these optional `[files]` settings:

```ini
sprite_catalog_file=assets/oracle_f0.json
sprite_source_root=oracle-source
```

Resolve both against the project descriptor. Preserve portable relative paths when
saving; support optional JSON project serialization as well. Choosing paths changes
project settings in memory; normal project saving persists them. Opening a catalog
does not save it, run a build, or scan arbitrary source directories.

Source bindings use `/` separators. Reject absolute paths, `..` traversal, and
symlinks outside the canonical configured root. Read only bounded regular files.
Locate the exact label, excluding comment-only occurrences; missing or ambiguous
labels are errors. Do not guess another routine with a similar name.

Read sources only on explicit navigation. The displayed source is a read-only
snapshot; selecting the source again refreshes it. Catalog reload clears that
snapshot. Do not run filesystem scans or ASM imports every frame.

Source roles must reflect separate responsibilities. For `$F0`:

| Role | Source |
|---|---|
| Registration owner | Existing macro invocation in `Sprites/NPCs/mermaid.asm`; not generated by the catalog |
| Prep / Long | `Sprite_Mermaid_Prep` / `Sprite_Mermaid_Long` in `mermaid.asm` |
| Mermaid draw | `Sprite_Mermaid_Draw` in `mermaid.asm` |
| Maple draw | `Sprite_Maple_Draw` in `mermaid.asm` |
| Maple behavior | `MapleHandler` in `Sprites/NPCs/maple.asm` |
| Librarian draw | `Sprite_Librarian_Draw` in `mermaid.asm` |

The initial fixture is intentionally partial metadata. It does not claim a complete
behavior/dependency graph merely because its six source labels resolve.

## 6. Rules for later migrations

These are requirements for future work, not implemented features:

1. Import existing data read-only first. Do not renumber IDs, canonicalize unknown
   placement values, alter registration, or rewrite behavior as part of adoption.
2. Transfer draw ownership one bounded block at a time. Preserve labels, scoping,
   widths, table order, and bank locality. Check source hashes before apply; prove
   no-edit assembled equivalence before replacing hand-authored tables.
3. Keep one authority for every editable field. Generated ASM is an output, not a
   competing editable master. Distinguish family defaults from supported runtime
   variant overrides. A property UI must not silently alter every shared-ID variant.
4. Prove selector transport and initialization relative to Prep, dynamic spawn,
   transitions, and respawn. Do not assume dungeon encoding on the overworld or
   borrow coordinate bits. Audit the active build and loader, not merely a file's
   presence. Existing callers may assign subtype after a spawn helper.
5. Keep save/export/build/apply separate. Use isolated candidate builds and bounded
   ROM diffs. Rollback only owned outputs whose expected hashes still match. Never
   restore over unrelated work, modify SRAM/savestates, or treat previews as proof
   of runtime behavior.

For inherited ZSpriteMaker draw data, `.nbr_of_tiles` is count minus one. Validate
the real table/index/coordinate limits before export. Static vanilla OAM preview
layouts are not write descriptors for vanilla ROM data.

For `oracle.stalfos_patrol`, retain an unallocated logical identity in the Oracle
design scaffold. Its patrol states and route/profile parameters do not become
subtypes. Preserve existing `$3F` blockers. The presentation-only arrival sequence
requires no gameplay sprite ID. See migration plan section 9 for the joint decisions.

## 7. Implementation map and verification

| Surface | Files |
|---|---|
| Pure model, validation, resolver, bounded source reader | `src/zelda3/sprite/sprite_catalog.{h,cc}` |
| Project path serialization | `src/core/project.{h,cc}` |
| Session lifecycle and read-only browser | `src/app/editor/sprite/sprite_catalog_panel.cc`, `sprite_editor.{h,cc}` |
| Window registration and layout | `src/app/editor/sprite/ui/sprite_editor_views.h`, `src/app/editor/layout/layout_presets.{h,cc}` |
| Catalog/source tests | `test/unit/zelda3/sprite_catalog_test.cc` |
| Project/session/UI tests | `test/unit/editor/sprite_catalog_session_test.cc` |
| User instructions | [Sprite Catalog usage](../../public/usage/sprite-catalog.md) |

Build the affected targets with the repository's supported preset. Discover the
test names before running a filter so an empty selection cannot count as success:

```sh
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze_test_unit yaze --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='SpriteCatalog*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='SpriteCatalog*'
```

Use the appropriate supported preset and binary path on other platforms. If cache
directories are sandbox-blocked, use task-local compiler caches; do not weaken
filesystem permissions or alter system-wide configuration.

Before reporting M1 complete, verify canonical/fallback resolution, invalid schema
handling, project path round-trip, session isolation, stale-content clearing,
missing-source behavior, root containment, and absence of source/ROM mutation.
Headless panel rendering is UI regression evidence, not a physical desktop or
emulator playtest. Record exact commands, counts, and any skipped checks.
