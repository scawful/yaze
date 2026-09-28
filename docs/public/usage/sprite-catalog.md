# Browse Oracle sprite families

The Sprite Catalog is a read-only browser for named variants that share a sprite
ID. The initial Oracle catalog covers Mermaid, Maple, and Librarian at `$F0`.
It does not change sprite placements, draw tables, properties, ASM, or ROM bytes.
The existing custom ZSM editor remains a separate workflow.

## Open the catalog

1. Open a Yaze project and select the Sprite editor.
2. Open **Sprite Catalog** from the Sprite windows/sidebar if it is not visible.
3. Select **Choose catalog...** and choose `assets/sprite_catalogs/oracle_f0.json`
   from the Yaze source/assets directory. You may copy this file into your project.
4. Select **Choose source root...** and choose your Oracle source directory: the
   directory containing `Sprites/` and `Core/`.
5. Save the project to retain these two paths.

You can also configure a native `.yaze` descriptor directly:

```ini
[files]
sprite_catalog_file=assets/oracle_f0.json
sprite_source_root=oracle-source
```

Both paths resolve relative to the project descriptor. These example paths assume
you placed the catalog and source at those locations; Yaze does not create them or
guess an Oracle checkout. A project without a catalog retains its normal behavior.

## Inspect a variant

Choose Mermaid, Maple, or Librarian. **Inspect raw subtype (decimal)** evaluates a
value without editing a placement. Subtypes 0, 1, and 2 select the canonical
variants. Other byte values show the existing Mermaid fallback while preserving
the inspected raw value. Values outside a byte are reported without truncation.

**View source** opens a read-only view at the bound label. Maple's draw routine is
in `Sprites/NPCs/mermaid.asm`; its behavior is in `Sprites/NPCs/maple.asm`.
The source view is a snapshot from when the button was pressed. Press it again to
read current source. **Reload catalog** rereads metadata and clears old source text.

Missing files or labels appear as local errors. Browsing the catalog still works
when the Oracle source directory is unavailable. Unsupported profiles, versions,
or selection rules are rejected; Yaze does not rewrite their files or show stale
catalog data after a failed reload.

The catalog itself shows **Preview unavailable** because it does not bind graphics
or palettes. **Import draw copy** creates an editable ZSM document from supported
literal tables. See [live sprite authoring](sprite-authoring.md) for editing,
animation playback, vanilla preview copies, and draw-table export limits.

## Version 1 format

See the shipped `oracle_f0.json` for a complete example. The root has integer
`schema_version: 1`, `profile: "oracle"`, and a nonempty `families` array.
Each family has a unique `key`, `name`, integer `main_id` in `$00–$F2`, `sources`,
`variants`, and a selector with `kind: "exact_with_fallback"` and a
`fallback_variant` key belonging to that family.

Each variant has a globally unique `key`, `name`, integer `authored_subtype` in
`0–31`, and source bindings. Canonical subtypes must be unique within a family.
Each source binding has a unique `role` within its binding list, a relative `path`,
and a `label`. A draw binding may also set `adapter` to `literal_v1` (default)
or `oracle_maple_v1` (reviewed Maple driver). Unknown adapters are rejected.
Paths use `/` separators and must stay inside the configured source
root, including after resolving symlinks. Source files and catalogs are limited
to 1 MiB each.

The loader rejects unknown fields and duplicate JSON fields instead of silently
discarding unsupported semantics. All catalog entries are browse-only in this
version. The format cannot yet describe contextual selectors, unallocated actors,
bit fields, or generated runtime code. Keep those in design documents until the
appropriate schema and backend capabilities exist.

Implementation and migration boundaries are recorded in
[the agent-facing standards](../../internal/architecture/sprite-catalog.md) and
[the migration plan](../../internal/plans/sprite-catalog-migration-2026-09-23.md).
