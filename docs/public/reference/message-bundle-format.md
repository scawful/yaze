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
z3ed message-import-bundle --file messages.json --apply
```
