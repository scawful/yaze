# i18n tooling

Scripts for the translation module. Full architecture + "how to add a language"
guide: [`docs/public/developer/i18n.md`](../../docs/public/developer/i18n.md).

Runtime: `tr("English")` (`src/util/i18n/`) resolves against
`assets/i18n/<locale>.json`; English is identity. Language is chosen in-app via
**Help → Language** (global, persisted). New locales are auto-discovered — just
add `assets/i18n/<locale>.json`.

## Scripts

| Script | Purpose |
|--------|---------|
| `wrap.py [paths]` | Codemod: wrap eligible ImGui text literals in `tr()` (`.cc/.mm/.cpp/.h`). |
| `extract.py` | Harvest source msgids from `tr("...")` + menu labels. |
| `check_catalog.py [catalog...]` | CI gate: valid JSON, no empty value, `%`-specifier parity key↔value. |
| `check_wrapped.py [--diff BASE HEAD] FILE...` | CI gate: no *new* unwrapped user-facing text (`--diff` = added lines only). |
| `check_msgids.py` | Coverage report (missing / orphan msgids); non-failing. |
| `i18n_common.py` | Shared helpers (`format_specifiers`, `visible_key`). |
| `merge_parts.py` | Merge partial catalog fragments. |
| `glossary_fr.md` | French terminology reference. |

## Common tasks

```bash
# Validate all catalogs (what CI runs)
python3 scripts/i18n/check_catalog.py assets/i18n/*.json

# Coverage report
python3 scripts/i18n/check_msgids.py

# Add a language (e.g. Spanish): create the catalog, translate values, validate
cp assets/i18n/fr.json assets/i18n/es.json
python3 scripts/i18n/check_catalog.py assets/i18n/es.json

# Bulk-wrap literals in a file, then translate the new msgids
python3 scripts/i18n/wrap.py src/app/editor/foo.cc
```
