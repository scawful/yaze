#include "cli/service/agent/agent_pretraining.h"

#include "absl/strings/str_format.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"

namespace yaze {
namespace cli {
namespace agent {

std::vector<AgentPretraining::KnowledgeModule> AgentPretraining::GetModules() {
  return {
    {"rom_structure", GetRomStructureKnowledge(nullptr), true},
    {"hex_analysis", GetHexAnalysisKnowledge(), true},
    {"map_editing", GetMapEditingKnowledge(), true},
    {"tool_usage", GetToolUsageExamples(), true},
  };
}

std::string AgentPretraining::GetRomStructureKnowledge(Rom* rom) {
  return R"(
# ALTTP ROM Structure Deep Dive

## Memory Map
- 0x00000-0x07FFF: Header + Low ROM
- 0x08000-0x0FFFF: Character data
- 0x10000-0x1FFFF: Overworld maps
- 0x1C800-0x1D7FF: Overworld tile16 data
- 0x20000-0x2FFFF: Dungeon rooms (296 rooms)
- 0xDE6C8-0xDEDC7: Overworld palettes
- 0xDD308-0xDD3C7: Dungeon palettes

## Overworld Structure
- 64 maps (0x00-0x3F): Light world + Dark world
- Each map: 32x32 tiles (1024 tile16s)
- Tile16 address = 0x1C800 + (tile_id * 8 bytes)
- Map data stored compressed

## Dungeon Structure
- 296 rooms total (0x00-0x127)
- Room header: 14 bytes
- Sprite data: 3 bytes per sprite, up to 16 sprites
- Layer 1/2 separate data
- Object encoding: Layer/Size/X/Y format

## Palette System
- 8 groups, 8 palettes per group
- 16 colors per palette (SNES 555 format)
- $0000-$7FFF range (5 bits per R/G/B channel)
- Conversion: RGB8 = (SNES_val & 0x1F) << 3

## Critical Addresses
- Sprite sheets: 0x80000+
- Entrance table: 0x02C000
- Item table: 0x0DC800
- Text data: 0x0E0000+
)";
}

std::string AgentPretraining::GetHexAnalysisKnowledge() {
  return R"(
# Hex Data Analysis Patterns

## Pattern Recognition

### Sprite Data (3-byte pattern)
- Byte 0: Sprite ID (0x00-0xFF)
- Byte 1: X position in room
- Byte 2: Y position in room
- Example: "09 48 56" = Sprite 0x09 at (72, 86)

### Tile16 Structure (8 bytes)
- Bytes 0-1: Top-left tile8
- Bytes 2-3: Top-right tile8
- Bytes 4-5: Bottom-left tile8
- Bytes 6-7: Bottom-right tile8
- Format: tile8_id (lower byte) + properties (upper byte)

### Pointers (Little Endian)
- 2-byte pointer: Low byte first, high byte second
- Example: "00 1C" = 0x1C00 (address 0x001C00)
- SNES addressing: Add bank byte for 24-bit

## Search Strategies

### Finding Unused Space
- Pattern: "FF FF FF FF" (often unused)
- Pattern: "00 00 00 00" (zeroed regions)

### Finding Sprite Definitions
- Search for known sprite IDs
- Look for X/Y coordinate patterns (0x00-0x1F typical range)

### Finding Compressed Data
- Look for compression headers (0xE0-0xFF often signify compression)
- Data density changes (sparse vs dense byte values)
)";
}

std::string AgentPretraining::GetMapEditingKnowledge() {
  return R"(
# Map Editing Workflow with Test Harness

## Tile Placement Flow
1. Parse natural language: "Place water tile at (5, 7)\"
2. Tool chain:
   a. overworld-find-tile to get water tile ID
   b. Calculate screen coordinates from game coords
   c. Generate GUI action: Click(overworld_canvas, x, y)
   d. Generate GUI action: SelectTile(tile_id)
   e. Generate GUI action: Click(target_x, target_y)

## Coordinate Systems
- Game coords: Tile-based (0-31 for 32x32 map)
- Screen coords: Pixel-based, depends on zoom/scroll
- Conversion: screen_x = game_x * tile_size * zoom + canvas_offset_x

## GUI Automation Best Practices
- Always wait for UI updates (Wait(100ms) between actions)
- Assert widget states before clicking
- Screenshot before/after for verification
- Use widget discovery to find exact IDs

## Proposal System Integration
- Read-only tools: Execute directly
- Write operations: Submit as proposal first
- Wait for approval before proceeding
- Show user what will change (diff view)
)";
}

std::string AgentPretraining::GetToolUsageExamples() {
  return R"(
# Tool Usage Examples & Chaining

## Example 1: Find and Analyze Sprites
User: "What enemies are in Hyrule Castle?"
Tool chain:
1. resource-search --type=dungeon --query=hyrule
2. dungeon-list-sprites --dungeon=hyrule_castle
3. resource-list --type=sprite (to get sprite names)
Result: "Hyrule Castle (dungeon 0) has 3 guards (sprite 0x41), 2 knights..."

## Example 2: Palette Investigation
User: "What colors are used for grass?"
Tool chain:
1. overworld-find-tile --tile_id=0x20 (grass tile)
2. palette-get-colors --group=0 --palette=0
3. palette-analyze --type=palette --id=0/0
Result: "Grass uses palette 0/0 with colors: light green #98FB98..."

## Example 3: Hex Pattern Search
User: "Find all chests in the ROM"
Tool chain:
1. hex-search --pattern="20 ?? ??" (chest object code 0x20)
2. For each match, hex-read context bytes
3. Parse room numbers from addresses
Result: "Found 50 chests across 30 rooms..."

## Example 4: Complex Edit Planning
User: "Add a new enemy to room 5"
Steps:
1. dungeon-describe-room --room_id=5 (check current sprites)
2. resource-list --type=sprite (available enemies)
3. todo-create --title="Add sprite to room 5" --steps="find_sprite,get_coords,write_data"
4. hex-read --address=<room_5_sprite_addr> (check free slots)
5. Submit proposal with hex-write
)";
}

std::string AgentPretraining::GeneratePretrainingPrompt(Rom* rom) {
  std::string prompt = "# Agent Pre-Training Session\n\n";
  prompt += "You are being initialized with deep knowledge about this ROM.\n\n";
  
  if (rom && rom->is_loaded()) {
    prompt += absl::StrFormat("## Current ROM: %s\n", rom->title());
    prompt += absl::StrFormat("Size: %zu bytes\n", rom->size());
    // prompt += absl::StrFormat("Type: %s\n\n", rom->is_expanded() ? "Expanded" : "Vanilla");
  }
  
  for (const auto& module : GetModules()) {
    prompt += absl::StrFormat("## Module: %s\n", module.name);
    prompt += module.content;
    prompt += "\n---\n\n";
  }
  
  prompt += R"(
## Your Capabilities After Training

You now understand:
✓ ROM memory layout and addressing
✓ How to chain tools for complex analysis
✓ Hex pattern recognition and data structures
✓ GUI automation for map editing
✓ Proposal system for safe ROM modification

**Test your knowledge**: When I ask about sprites, dungeons, or tiles, 
use multiple tools in one response to give comprehensive answers.
)";
  
  return prompt;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
