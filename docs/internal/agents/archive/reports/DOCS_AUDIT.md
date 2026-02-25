# Yaze Documentation Audit Report

*Generated: 2026-01-07*

## Executive Summary

Deep audit of Yaze documentation against actual codebase implementation.
This audit found several inaccuracies in CLI command formats and identified
gaps in Zelda3 core and ImGui abstraction documentation.

---

## 1. CLI Documentation Errors

### Command Format Issues

**Location:** `docs/public/usage/z3ed-cli.md`, `docs/public/overview/getting-started.md`

| Documented Command | Actual Command | Issue |
|-------------------|----------------|-------|
| `z3ed dungeon list-sprites --dungeon 2` | `z3ed dungeon-list-sprites --room <id>` | Wrong format: uses `--dungeon` instead of `--room`, space vs dash |
| `z3ed agent chat` | `z3ed agent simple-chat` | Command name is `simple-chat`, not `chat` |
| `z3ed agent plan --prompt "..."` | `z3ed agent plan` | Syntax may differ |
| `z3ed overworld describe-map --map 80` | `z3ed overworld-describe-map --map 80` | Dash format |
| `z3ed palette export` | `z3ed palette-export` | Dash format |

### Actual z3ed Categories (68 commands)

```
game:      10 commands (dialogue-*, message-*, music-*)
dungeon:    7 commands (dungeon-list-sprites, dungeon-describe-room, etc.)
emulator:  12 commands (emulator-*)
misc:       6 commands
gui:        4 commands
graphics:  13 commands (graphics-*)
overworld:  8 commands (overworld-*)
resource:   2 commands
agent:      1 command (via agent.cc subcommands)
tools:      5 commands
```

### Agent Subcommands (Actual)

```
simple-chat           Interactive AI chat
test-conversation     Automated test conversation
plan                  Generate execution plan
run                   Execute plan in sandbox
diff                  Review proposal diff
accept                Apply proposal changes
commit                Save ROM changes
revert                Reload ROM from disk
learn                 Manage learned knowledge
todo                  Task management
list/describe         List/describe proposals
```

---

## 2. Corporate/Marketing Language to Remove

**README.md:**
- "All-in-one editing" - generic marketing speak
- "Automation & AI" - vague bullet point
- "Modular AI stack" - sounds like enterprise software
- "Bundles its toolchain" - product marketing
- "Cross-platform toolchains" - overly formal

**Suggested Tone:** Technical, honest, hobby-project style. Focus on what it does, not selling it.

---

## 3. Documentation Gaps

### Zelda3 Core (src/zelda3/)

Missing comprehensive documentation for:

| Module | Files | Coverage |
|--------|-------|----------|
| `dungeon/` | 45+ files | Only dungeon-spec.md |
| `overworld/` | 48+ files | Only overworld-tail-expansion.md |
| `music/` | 23+ files | None |
| `sprite/` | 6 files | None |
| `screen/` | 5 files | None |
| `game_data.h/cc` | Core data structures | None |

**Key constants undocumented:**
- 223 graphics sheets (`kNumGfxSheets`)
- 296 dungeon rooms
- 160 overworld maps
- 144 spritesets (`kNumSpritesets`)
- 72 palettesets (`kNumPalettesets`)

### ImGui Abstractions (src/app/gui/)

Missing documentation for custom UI framework:

| Directory | Files | Purpose | Docs |
|-----------|-------|---------|------|
| `canvas/` | 37 files | Pannable/zoomable canvas system | Only canvas-system.md |
| `widgets/` | 12 files | Themed widgets | None |
| `style/` | ? files | Theme system | Mentioned in architecture.md |
| `core/` | ? files | Core GUI utilities | None |
| `automation/` | ? files | GUI automation | None |

**Key undocumented abstractions:**
- `Canvas` class with pan/zoom
- `canvas_interaction_handler` for mouse events
- `canvas_context_menu` system
- `themed_widgets` for consistent styling
- `asset_browser` for resource viewing

---

## 4. Accuracy Verification

### Claims That Are TRUE

| Claim | Evidence |
|-------|----------|
| "160 overworld maps" | `kOverworldMapCount = 160` in code |
| "296 dungeon rooms" | `kNumRooms = 296` in dungeon constants |
| "Asar integration" | Full asar assembler embedded in ext/ |
| "WASM web port" | Working at scawful.github.io/yaze/ |
| "Multi-session support" | SessionCoordinator in src/app/session/ |
| "gRPC automation" | Full proto definitions and servers |

### Claims Needing Clarification

| Claim | Reality |
|-------|---------|
| "AI-assisted editing flows" | Requires Ollama or Gemini API key; doesn't work standalone |
| "Live previews" | Some editors have live preview, others don't |
| "Extensions menu" | Plugin system is "under development" per docs |

---

## 5. Recommended Fixes

### Immediate (CLI docs)

1. Fix command format: `noun-verb` not `noun verb`
2. Fix flag names: `--room` not `--dungeon`
3. Fix agent command: `simple-chat` not `chat`
4. Update z3ed-cli.md with actual --help output

### Short-term (Tone)

1. Remove marketing superlatives from README
2. Rewrite highlights as technical facts
3. Add honest status for experimental features

### Medium-term (Coverage)

1. Create zelda3/README.md documenting core data structures
2. Create gui/README.md documenting canvas and widget system
3. Add API reference for public classes

---

## 6. Veran Model Evaluation

**Test Query:** "What does register $2100 do on the SNES?"

**Veran Response:** (INCORRECT)
> Register $2100 is the "DMA Control" register...

**Actual Answer:**
$2100 is INIDISP (Screen Display Register), controls brightness and forced blank.
DMA Control is $420B.

**Assessment:** Veran v1 demonstrates 30% accuracy on SNES hardware as documented
in model-training-status.md. The model hallucinated a plausible-sounding but
completely wrong answer. Not currently reliable for technical SNES documentation.
