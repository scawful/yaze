# Examples & Recipes

Short, task-focused snippets for everyday YAZE workflows. These examples supplement the primary
guides (Getting Started, z3ed CLI, Dungeon/Overworld editors) and should remain concise. When in
doubt, link back to the relevant guide instead of duplicating long explanations.

## 1. Launching Common Editors
```bash
# Open YAZE directly in the Dungeon editor with room cards preset
./build/bin/yaze --rom_file=zelda3.sfc \
  --editor=Dungeon \
  --cards="Rooms List,Room Graphics,Object Editor"

# Jump to an Overworld map from the CLI/TUI companion
./build/bin/z3ed overworld describe-map --map 0x80 --rom zelda3.sfc
```

## 2. AI/Automation Recipes
```bash
# Generate an AI plan to reposition an entrance, but do not apply yet
./build/bin/z3ed agent plan \
  --rom zelda3.sfc \
  --prompt "Move the desert palace entrance 2 tiles north" \
  --sandbox

# Resume the plan and apply it once reviewed
./build/bin/z3ed agent accept --proposal-id <ID> --rom zelda3.sfc --sandbox
```

## 3. Building & Testing Snippets
```bash
# Debug build with tests
cmake --preset mac-dbg
cmake --build --preset mac-dbg --target yaze yaze_test
./build/bin/yaze_test --unit

# AI-focused build in a dedicated directory (recommended for assistants)
cmake --preset mac-ai -B build_ai
cmake --build build_ai --target yaze z3ed
```

## 4. Quick Verification
- Run `./scripts/verify-build-environment.sh --fix` (or the PowerShell variant on Windows) whenever
  pulling major build changes.
- See the [Build & Test Quick Reference](../build/quick-reference.md) for the canonical list of
  commands and testing recipes.

Want to contribute another recipe? Add it here with a short description and reference the relevant
guide so the examples stay focused.
