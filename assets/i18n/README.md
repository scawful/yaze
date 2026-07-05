# yaze translation catalogs (i18n)

Each `<locale>.json` is a **flat JSON object** mapping an English source string
(the msgid used in the code as `tr("...")`) to its translation:

```json
{ "File": "Fichier", "Open ROM": "Ouvrir la ROM", "Lines: %d": "Lignes : %d" }
```

- **`en.json`** is the identity locale and is normally empty (`{}`): untranslated
  lookups fall back to the English source string, so the app is always correct
  even with zero coverage.
- **`fr.json`** is the French catalog.

## Rules for translators

1. **Keys are the visible text only.** Never include ImGui id suffixes: the key
   is `"Mode"`, not `"Mode##selector"`. The code strips `##...`/`###...` before
   lookup and re-attaches it, so the widget id stays stable.
2. **Preserve format specifiers exactly.** A value must contain the same
   `printf`-style specifiers (`%d`, `%s`, `%.2f`, ...) in the **same order** as
   its key. `"Lines: %d"` → `"Lignes : %d"` (OK); dropping or reordering `%`
   tokens is rejected by `scripts/i18n/extract.py`'s linter.
3. **UTF-8.** Write accents directly (`é è à ç œ …`). The app bakes the needed
   glyph ranges at startup.
4. Missing/empty values fall back to English — safe to ship partial catalogs.

Catalogs are generated/updated by `scripts/i18n/extract.py` and loaded at runtime
via `util::PlatformPaths::FindAsset("i18n/<locale>.json")`.
