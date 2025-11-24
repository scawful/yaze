# Public Docs Guide (Doxygen-Friendly)

Purpose: keep `docs/public` readable, accurate, and exportable via Doxygen.

## Authoring checklist
- One H1 per page; keep openings short (2–3 lines) and outcome-focused.
- Lead with “who/what/why” and a minimal quick-start or TL;DR when relevant.
- Prefer short sections and bullet lists over walls of text; keep code blocks minimal and copy-pastable.
- Use relative links within `docs/public`; point to `docs/internal` only when the public doc would otherwise be incomplete.
- Label platform-specific steps explicitly (macOS/Linux/Windows) and validate against current `CMakePresets.json`.
- Avoid agent-only workflow details; those belong in `docs/internal`.

## Doxygen structure
- `docs/public/index.md` serves as the `@mainpage` with concise navigation.
- Keep headings shallow (H1–H3) so Doxygen generates clean TOCs.
- Include a short “New here?” or quick-start block on entry pages to aid scanning.

## Accuracy cadence
- Re-verify build commands and presets after CMake or CI changes.
- Review public docs at least once per release cycle; archive or rewrite stale guidance instead of adding new pages.

## Naming & formatting
- Use kebab-case filenames; reserve ALL-CAPS for anchors like README/CONTRIBUTING/AGENTS/GEMINI/CLAUDE.
- Prefer present tense, active voice, and consistent terminology (e.g., “yaze”, “z3ed”, “preset”).

## When to add vs. link
- Add a new public page only if it benefits external readers; otherwise, link to the relevant internal doc.
- For experimental or rapidly changing features, keep the source in `docs/internal` and expose a short, stable summary here.
