# Testing z3ed Without ROM Files

**Last Updated:** October 10, 2025  
**Status:** Active

## Overview

The `z3ed` AI agent now supports **mock ROM mode** for testing without requiring actual ROM files. This is essential for:

- **CI/CD pipelines** - No ROM files can be committed to GitHub
- **Development testing** - Quick iterations without ROM dependencies
- **Contributors** - Test the agent without needing to provide ROMs
- **Automated testing** - Consistent, reproducible test environments

## How Mock ROM Mode Works

Mock ROM mode creates a minimal but valid ROM structure with:
- Proper SNES header (LoROM mapping, 1MB size)
- Zelda3 embedded labels (rooms, sprites, entrances, items, music, etc.)
- Resource label manager initialization
- No actual ROM data (tiles, graphics, maps remain empty)

This allows the AI agent to:
- Answer questions about room names, sprite IDs, entrance numbers
- Lookup labels and constants
- Test function calling and tool dispatch
- Validate agent logic without game data

## Usage

### Command Line Flag

Add `--mock-rom` to any `z3ed agent` command:

```bash
# Simple chat with mock ROM
z3ed agent simple-chat "What is room 5?" --mock-rom

# Test conversation with mock ROM
z3ed agent test-conversation --mock-rom

# AI provider testing
z3ed agent simple-chat "List all dungeons" --mock-rom --ai_provider=ollama
```

### Test Suite

The `agent_test_suite.sh` script now defaults to mock ROM mode:

```bash
# Run tests with mock ROM (default)
./scripts/agent_test_suite.sh ollama

# Or with Gemini
./scripts/agent_test_suite.sh gemini
```

To use a real ROM instead, edit the script:

```bash
USE_MOCK_ROM=false  # At the top of agent_test_suite.sh
```

## What Works with Mock ROM

### Fully Supported

**Label Queries:**
- "What is room 5?" → "Tower of Hera - Moldorm Boss"
- "What sprites are in the game?" → Lists all 256 sprite names
- "What is entrance 0?" → "Link's House Main"
- "List all items" → Bow, Boomerang, Hookshot, etc.

**Resource Lookups:**
- Room names (296 rooms)
- Entrance names (133 entrances)
- Sprite names (256 sprites)
- Overlord names (14 overlords)
- Overworld map names (160 maps)
- Item names
- Music track names
- Graphics sheet names

**AI Testing:**
- Function calling / tool dispatch
- Natural language understanding
- Error handling
- Tool output parsing
- Multi-turn conversations

### Limited Support

**Queries Requiring Data:**
- "What tiles are used in room 5?" → No tile data in mock ROM
- "Show me the palette for map 0" → No palette data
- "What's at coordinate X,Y?" → No map data
- "Export graphics from dungeon 1" → No graphics data

These queries will either return empty results or errors indicating no ROM data is available.

### Not Supported

**Operations That Modify ROM:**
- Editing tiles
- Changing palettes
- Modifying sprites
- Patching ROM data

## Testing Strategy

### For Agent Logic
Use **mock ROM** for testing:
- Function calling mechanisms
- Tool dispatch and routing
- Natural language understanding
- Error handling
- Label resolution
- Resource lookups

### For ROM Operations
Use **real ROM** for testing:
- Tile editing
- Graphics manipulation
- Palette modifications
- Data extraction
- ROM patching

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Test z3ed Agent

on: [push, pull_request]

jobs:
  test-agent:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install Dependencies
        run: |
          # Install ollama if testing local models
          curl -fsSL https://ollama.ai/install.sh | sh
          ollama pull qwen2.5-coder
      
      - name: Build z3ed
        run: |
          cmake -B build_test
          cmake --build build_test --parallel
      
      - name: Run Agent Tests (Mock ROM)
        run: |
          ./scripts/agent_test_suite.sh ollama
        env:
          # Or use Gemini with API key
          GEMINI_API_KEY: ${{ secrets.GEMINI_API_KEY }}
```

## Embedded Labels Reference

Mock ROM includes all these labels from `zelda3::Zelda3Labels`:

| Resource Type | Count | Example |
|--------------|-------|---------|
| Rooms | 296 | "Sewer - Throne Room" |
| Entrances | 133 | "Link's House Main" |
| Sprites | 256 | "Moldorm (Boss)" |
| Overlords | 14 | "Overlord - Agahnim's Barrier" |
| Overworld Maps | 160 | "Light World - Hyrule Castle" |
| Items | 64+ | "Bow", "Boomerang", "Hookshot" |
| Music Tracks | 64+ | "Title Theme", "Overworld", "Dark World" |
| Graphics Sheets | 128+ | "Link Sprites", "Enemy Pack 1" |

See `src/zelda3/zelda3_labels.h` for the complete list.

## Troubleshooting

### "No ROM loaded" error

Make sure you're using the `--mock-rom` flag:

```bash
# Wrong
z3ed agent simple-chat "test"

# Correct
z3ed agent simple-chat "test" --mock-rom
```

### Mock ROM fails to initialize

Check the error message. Common issues:
- Build system didn't include `mock_rom.cc`
- Missing `zelda3_labels.cc` in build
- Linker errors with resource labels

### Agent returns empty/wrong results

Remember: Mock ROM has **labels only**, no actual game data.

Queries like "What tiles are in room 5?" won't work because there's no tile data.

Use queries about labels and IDs instead: "What is the name of room 5?"

## Development

### Adding New Labels

To add new label types to mock ROM:

1. **Add to `zelda3_labels.h`:**
```cpp
static const std::vector<std::string>& GetNewResourceNames();
```

2. **Implement in `zelda3_labels.cc`:**
```cpp
const std::vector<std::string>& Zelda3Labels::GetNewResourceNames() {
  static std::vector<std::string> names = {"Item1", "Item2", ...};
  return names;
}
```

3. **Add to `ToResourceLabels()`:**
```cpp
const auto& new_resources = GetNewResourceNames();
for (size_t i = 0; i < new_resources.size(); ++i) {
  labels["new_resource"][std::to_string(i)] = new_resources[i];
}
```

4. **Rebuild:**
```bash
cmake --build build --parallel
```

### Testing Mock ROM Directly

```cpp
#include "cli/handlers/mock_rom.h"

Rom rom;
auto status = InitializeMockRom(rom);
if (status.ok()) {
  // ROM is ready with all labels
  auto* label_mgr = rom.resource_label();
  std::string room_name = label_mgr->GetLabel("room", "5");
  // room_name == "Tower of Hera - Moldorm Boss"
}
```

## Best Practices

### DO 
- Use mock ROM for CI/CD and automated tests
- Use mock ROM for agent logic development
- Use mock ROM when contributing (no ROM files needed)
- Test with real ROM before releasing features
- Document which features require real ROM data

### DON'T ❌
- Commit ROM files to Git (legal issues)
- Assume mock ROM has actual game data
- Use mock ROM for testing data extraction
- Skip real ROM testing entirely

## Related Documentation

- [C1: z3ed Agent Guide](C1-z3ed-agent-guide.md) - Main agent documentation
- [A1: Testing Guide](A1-testing-guide.md) - General testing strategy
- [E3: API Reference](E3-api-reference.md) - ROM API documentation

---

**Implementation Status:**  Complete  
**Since Version:** v0.3.3  
**Files:**
- `src/cli/handlers/mock_rom.h`
- `src/cli/handlers/mock_rom.cc`
- `src/cli/flags.cc` (--mock-rom flag)
- `scripts/agent_test_suite.sh` (updated)
