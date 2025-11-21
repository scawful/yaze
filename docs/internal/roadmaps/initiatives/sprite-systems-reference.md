# Sprite Systems Reference – Planning Notes

Topic: “Sprite Editor + ASM Knowledge” (from the keep-chatting prompts)

## Goal
Create a reusable reference that maps sprite IDs to:
- ASM routines / callable labels (`assets/asm/usdasm/`, `docs/internal/agents/ai-agent-debugging-guide.md`)
- Data tables (`assets/asm/sprites/`, `docs/internal/agents/z3ed-refactoring.md`)
- Graphics sheets + palettes (`src/app/gfx/`, `assets/sheets/`)
- Tests/docs that exercise sprite behavior (`test/unit/`, `docs/internal/roadmaps/future-improvements.md`)

## Immediate TODOs
1. **Source Inventory**
   - ✅ `assets/asm/usdasm/` (disassembly)
   - ⬜ `docs/internal/agents/ai-agent-debugging-guide.md` §Oracle of Secrets – cross-check sprite notes
   - ⬜ `docs/internal/agents/z3ed-hive-plan.md` – identify AI touchpoints for sprite metadata
   - ⬜ `docs/internal/agents/REALTIME_EMULATOR_RESEARCH.md` – see if SSP plans need sprite hooks

2. **Data Extraction Tasks**
   - Script to parse `usdasm` symbol files → YAML/JSON table (SpriteID, routine, addr, behavior)
   - Map sprites to graphics sheets: tie `Arena::Get().gfx_sheet(index)` usage to sprite metadata
   - Document palette dependencies (light/dark world, dungeon vs overworld sprites)

3. **Documentation Structure**
   - Proposed sections:
     1. Overview + goals
     2. ASM References (tables + callouts)
     3. Graphics/Palette crosswalk
     4. Testing hooks (unit/integration)
     5. AI/Hive usage hints (z3ed prompts, SSP overlays)
   - Output format: Markdown doc under `docs/internal/agents/` with quick links + TODOs

4. **Testing Hooks to Add**
   - Identify candidate files under `test/unit/` for new sprite behavior tests
   - Outline integration tests once sprite reference is in place

## Next Steps
- Need volunteers to:
  - Parse ASM tables (Gemini Autom?)
  - Draft doc skeleton (Codex / Claude Docs)
  - Tie into sprite editor UI plan (Claude Core)

Log progress on the coordination board under the Sprite Editor topic when you pick up a subtask.
