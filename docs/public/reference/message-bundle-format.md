# Message Bundle JSON Format

The message bundle format is a round-trip JSON format for exporting and
importing message text (vanilla + expanded) with validation.

## Top-Level

```
{
  "format": "yaze-message-bundle",
  "version": 1,
  "counts": { "vanilla": 396, "expanded": 0 },
  "messages": [ ... ]
}
```

- `format` (string): Identifies the bundle format.
- `version` (int): Format version. Current: `1`.
- `counts` (object): Optional counts for each bank.
- `messages` (array): Message entries.

## Message Entry

```
{
  "id": 12,
  "bank": "vanilla",
  "address": 917504,
  "raw": "Hello [L][1]Welcome to Hyrule.",
  "parsed": "Hello [L]\nWelcome to Hyrule.",
  "length": 28,
  "line_width_warnings": [
    "Line 2: 35 visible characters (max 32)"
  ]
}
```

Required fields:
- `id` (int): Message index within the bank.
- `bank` (string): `vanilla` or `expanded`.
- One of `raw`, `text`, or `parsed` must be present when importing.

Optional fields:
- `address` (int): ROM address (for reference only).
- `raw` (string): Tokenized message text (recommended for import).
- `parsed` (string): Expanded text with dictionary tokens replaced.
- `text` (string): Alternative alias for `raw`.
- `length` (int): Encoded byte length.
- `line_width_warnings` (array): Export-time diagnostics.

## Import Behavior

- `raw` is preferred; if missing, `text` or `parsed` will be used.
- Unknown tokens or unsupported characters produce parse errors.
- Literal newlines are ignored; use `[1]`, `[2]`, `[3]`, `[V]`, or `[K]`.
- Line width warnings are computed at import time and reported.

## CLI

Export:
```
z3ed message-export-bundle --output messages.json --range all
```

Import (validate only):
```
z3ed message-import-bundle --file messages.json
```

Import with strict validation (non-zero exit on parse errors):
```
z3ed message-import-bundle --file messages.json --strict
```

Import and apply to ROM:
```
z3ed message-import-bundle --file messages.json --apply --range vanilla
```

Applying expanded entries also requires the matching Yaze project so `z3ed`
can validate the active ROM, manifest ownership, and project write policy:
```
z3ed message-import-bundle --file messages.json --apply \
  --rom path/to/rom.sfc --project path/to/project.yaze
```

## ASM-Owned Expanded Message Source

Projects that rebuild expanded messages from ASM can opt into a durable source
handoff by adding this metadata beneath the Hack Manifest's existing
`messages` object:

```json
"source": {
  "format": "yaze-message-bundle",
  "version": 1,
  "canonical_bundle_path": "Data/Messages/expanded.json",
  "generated_asm_include_path": "Core/generated/messages.asm"
}
```

Both paths are project-relative and must remain inside the project root after
symlink resolution. The sibling `messages.expanded_range`, `data_start`, and
`data_end` fields remain authoritative for IDs and capacity.

Preview a validated subset merge:

```bash
z3ed message-source-sync \
  --project Oracle-of-Secrets.yaze \
  --file /tmp/message-edits.json \
  --format json
```

Publication is explicit and compare-and-swap protected:

```bash
z3ed message-source-sync \
  --project Oracle-of-Secrets.yaze \
  --file /tmp/message-edits.json \
  --expected-source-sha256 <sha256-from-preview> \
  --write --format json
```

The canonical source is always rewritten deterministically as one complete,
expanded-only bank with contiguous bank-local IDs. Source sync treats an
explicit `text` field as authoritative over export-time `raw`/`parsed`
metadata, normalizes dictionary tokens to uppercase `[D:XX]`, and emits only
`bank`, `id`, and `text` per entry. The generated Asar include has absolute
`Message_XXX` labels, no `org`, appends one `$7F` terminator per message, and
has one final `$FF`. Its header binds both the exact canonical bundle bytes and
the exact generated ASM body bytes with SHA-256.

Argument command tokens are case-sensitive and accept one or two uppercase
hex digits without a `$` prefix (for example, `[W:7]`, `[W:7F]`, and
`[W:FF]`). Source sync preserves those spellings rather than zero-padding them;
missing or lowercase command arguments are rejected.

Write mode rejects stale source hashes, a drifted generated include, `[BANK]`,
incomplete or duplicate IDs, capacity overflow, path escapes, and symlink
targets. The bundle and include publish as one rollback-backed artifact set
using same-directory temporary files and exact reopen/readback. Writers are
serialized in-process and across processes with a persistent
`.yaze-message-source-sync.lock` in each distinct publication-target directory.
Projects should ignore that basename wherever source artifacts live rather
than deleting lock files between runs. This command does not open or mutate a
ROM. Browser builds reject `--write` because they cannot guarantee durable
atomic filesystem publication.
