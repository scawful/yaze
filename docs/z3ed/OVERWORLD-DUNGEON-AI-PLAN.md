# Overworld & Dungeon AI Integration Plan

**Date**: October 3, 2025  
**Status**: 🎯 Design Phase  
**Focus**: Practical tile16 editing and ResourceLabels awareness

## Executive Summary

This document outlines the strategic shift from general-purpose ROM editing to **specialized overworld and dungeon AI workflows**. The focus is on practical, visual editing with accept/reject flows that leverage the existing tile16 editor and ResourceLabels system.

## Vision: AI-Driven Visual Editing

### Why Overworld/Dungeon Focus?

**Overworld Canvas Editing** is ideal for AI because:
1. **Simple Data Model**: Just tile16 IDs on a 512x512 grid
2. **Visual Feedback**: Immediate preview of changes
3. **Reversible**: Easy accept/reject workflow
4. **Common Use Case**: Most ROM hacks modify overworld layout
5. **Safe Sandbox**: Changes don't affect game logic

**Dungeon Editing** is next logical step:
1. **Structured Data**: Rooms, objects, sprites, entrances
2. **ResourceLabels**: User-defined names make AI navigation intuitive
3. **No Preview Yet**: AI can still generate valid data
4. **Complex Workflows**: Requires AI to understand relationships

## Architecture: Tile16 Accept/Reject Workflow

### Current State
- ✅ Tile16Editor fully implemented (`src/app/editor/overworld/tile16_editor.{h,cc}`)
- ✅ Overworld canvas displays tile16 grid (32x32 tile16s per screen)
- ✅ Tile16 IDs are 16-bit values (0x000 to 0xFFF)
- ✅ Changes update blockset bitmap in real-time
- ⚠️ **Missing**: Proposal-based workflow for AI edits

### Proposed Workflow

```
┌─────────────────────────────────────────────────────────────┐
│  User: "Add a river flowing from north to south on map 0"  │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
      ┌──────────────────────────────┐
      │  AI Service (Gemini/Ollama)  │
      │  - Understands "river"       │
      │  - Knows water tile16 IDs    │
      │  - Plans tile placement      │
      └──────────────┬───────────────┘
                     │
                     ▼
      ┌──────────────────────────────────────────┐
      │  Generate Tile16 Proposal (JSON)         │
      │  {                                        │
      │    "map": 0,                              │
      │    "changes": [                           │
      │      {"x": 10, "y": 0, "tile": 0x14C},   │ ← Water top
      │      {"x": 10, "y": 1, "tile": 0x14D},   │ ← Water middle
      │      {"x": 10, "y": 2, "tile": 0x14D},   │
      │      {"x": 10, "y": 30, "tile": 0x14E}   │ ← Water bottom
      │    ]                                      │
      │  }                                        │
      └──────────────┬───────────────────────────┘
                     │
                     ▼
      ┌──────────────────────────────────────────┐
      │  Apply to Sandbox ROM (Preview)          │
      │  - Load map 0 from sandbox ROM           │
      │  - Apply tile16 changes                  │
      │  - Render preview bitmap                 │
      │  - Generate diff image (before/after)    │
      └──────────────┬───────────────────────────┘
                     │
                     ▼
      ┌──────────────────────────────────────────┐
      │  Display to User                         │
      │  ┌─────────┬──────────┬─────────┐       │
      │  │ Before  │ Changes  │ After   │       │
      │  │ [Image] │ +47      │ [Image] │       │
      │  │         │ tiles    │         │       │
      │  └─────────┴──────────┴─────────┘       │
      │                                          │
      │  [Accept] [Reject] [Modify]              │
      └──────────────┬───────────────────────────┘
                     │
                     ▼
      ┌──────────────────────────────────────────┐
      │  User Decision                           │
      │  ✓ Accept  → Write to main ROM           │
      │  ✗ Reject  → Discard sandbox changes     │
      │  ✎ Modify  → Adjust proposal parameters  │
      └──────────────────────────────────────────┘
```

### Implementation Components

#### 1. Tile16ProposalGenerator
**File**: `src/cli/service/tile16_proposal_generator.{h,cc}`

```cpp
struct Tile16Change {
  int map_id;
  int x;  // Tile16 X coordinate (0-63 typically)
  int y;  // Tile16 Y coordinate (0-63 typically)
  uint16_t old_tile;  // Original tile16 ID
  uint16_t new_tile;  // New tile16 ID
};

struct Tile16Proposal {
  std::string id;  // Unique proposal ID
  std::string prompt;  // Original user prompt
  int map_id;
  std::vector<Tile16Change> changes;
  std::string reasoning;  // AI explanation
  
  // Metadata
  std::chrono::system_clock::time_point created_at;
  std::string ai_service;  // "gemini", "ollama", etc.
};

class Tile16ProposalGenerator {
 public:
  // Generate proposal from AI service
  absl::StatusOr<Tile16Proposal> GenerateFromPrompt(
      const std::string& prompt,
      const RomContext& context);
  
  // Apply proposal to sandbox ROM
  absl::Status ApplyProposal(
      const Tile16Proposal& proposal,
      Rom* sandbox_rom);
  
  // Generate visual diff
  absl::StatusOr<gfx::Bitmap> GenerateDiff(
      const Tile16Proposal& proposal,
      Rom* before_rom,
      Rom* after_rom);
  
  // Save proposal for later review
  absl::Status SaveProposal(
      const Tile16Proposal& proposal,
      const std::string& path);
};
```

#### 2. Enhanced Prompt Examples

**Current Examples** (Too Generic):
```cpp
examples_.push_back({
    "Place a tree at coordinates (10, 20) on map 0",
    {"overworld set-tile --map 0 --x 10 --y 20 --tile 0x02E"},
    "Tree tile ID is 0x02E in ALTTP"
});
```

**New Examples** (Practical & Visual):
```cpp
examples_.push_back({
    "Add a horizontal row of trees across the top of Light World",
    {
        "overworld batch-edit --map 0 --pattern horizontal_trees.json"
    },
    "Use batch patterns for repetitive tile placement",
    "overworld"  // Category
});

examples_.push_back({
    "Create a 3x3 water pond at position 10, 15",
    {
        "overworld set-area --map 0 --x 10 --y 15 --width 3 --height 3 --tile 0x14D --edges true"
    },
    "Area commands handle edge tiles automatically (corners, sides)",
    "overworld"
});

examples_.push_back({
    "Replace all grass tiles with dirt in the Lost Woods area",
    {
        "overworld replace-tile --map 0 --region lost_woods --from 0x020 --to 0x022"
    },
    "Region-based replacement uses predefined area boundaries",
    "overworld"
});

examples_.push_back({
    "Make the desert more sandy by adding sand dunes",
    {
        "overworld blend-tiles --map 3 --region desert --pattern sand_dunes --density 40"
    },
    "Blend patterns add visual variety while respecting terrain type",
    "overworld"
});
```

#### 3. ResourceLabels Context Injection

**Current Problem**: AI doesn't know user's custom names for dungeons, maps, etc.

**Solution**: Extract ResourceLabels and inject into prompt context.

**File**: `src/cli/service/resource_context_builder.{h,cc}`

```cpp
class ResourceContextBuilder {
 public:
  explicit ResourceContextBuilder(Rom* rom) : rom_(rom) {}
  
  // Extract all resource labels from current project
  absl::StatusOr<std::string> BuildResourceContext();
  
  // Get specific category of labels
  absl::StatusOr<std::map<std::string, std::string>> GetLabels(
      const std::string& category);
  
 private:
  Rom* rom_;
  
  // Extract from ROM's ResourceLabelManager
  std::string ExtractOverworldLabels();   // "light_world", "dark_world", etc.
  std::string ExtractDungeonLabels();     // "eastern_palace", "swamp_palace", etc.
  std::string ExtractEntranceLabels();    // "links_house", "sanctuary", etc.
  std::string ExtractRoomLabels();        // "boss_room", "treasure_room", etc.
  std::string ExtractSpriteLabels();      // "soldier", "octorok", etc.
};
```

**Enhanced Prompt with ResourceLabels**:
```
=== AVAILABLE RESOURCES ===

Overworld Maps:
  - 0: "Light World" (user label: "hyrule_overworld")
  - 1: "Dark World" (user label: "dark_world")
  - 3: "Desert" (user label: "lanmola_desert")

Dungeons:
  - 0x00: "Hyrule Castle" (user label: "castle")
  - 0x02: "Eastern Palace" (user label: "east_palace")
  - 0x04: "Desert Palace" (user label: "desert_dungeon")

Entrances:
  - 0x00: "Link's House" (user label: "starting_house")
  - 0x01: "Sanctuary" (user label: "church")

Common Tile16s:
  - 0x020: Grass
  - 0x022: Dirt
  - 0x14C: Water (top edge)
  - 0x14D: Water (middle)
  - 0x14E: Water (bottom edge)
  - 0x02E: Tree

=== USER PROMPT ===
{user_prompt}

=== INSTRUCTIONS ===
1. Use the user's custom labels when referencing resources
2. If user says "eastern palace", use dungeon ID 0x02
3. If user says "my custom dungeon", check for matching label
4. Provide tile16 IDs as hex values (0x###)
5. Explain which labels you're using in your reasoning
```

#### 4. CLI Command Structure

**New Commands**:
```bash
# Tile16 editing commands (AI-friendly)
z3ed overworld set-tile --map <id> --x <x> --y <y> --tile <tile16_id>
z3ed overworld set-area --map <id> --x <x> --y <y> --width <w> --height <h> --tile <tile16_id>
z3ed overworld replace-tile --map <id> --from <old_tile> --to <new_tile> [--region <name>]
z3ed overworld batch-edit --map <id> --pattern <json_file>
z3ed overworld blend-tiles --map <id> --pattern <name> --density <percent>

# Dungeon editing commands (label-aware)
z3ed dungeon get-room --dungeon <label_or_id> --room <label_or_id>
z3ed dungeon set-object --dungeon <id> --room <id> --object <type> --x <x> --y <y>
z3ed dungeon list-entrances --dungeon <label_or_id>
z3ed dungeon add-sprite --dungeon <id> --room <id> --sprite <type> --x <x> --y <y>

# ResourceLabel commands (for AI context)
z3ed labels list [--category <type>]
z3ed labels export --to <json_file>
z3ed labels get --type <type> --key <key>
```

## ResourceLabels Deep Integration

### Current System
**Location**: `src/app/core/project.{h,cc}`

```cpp
struct ResourceLabelManager {
  // Format: labels_["dungeon"]["0x02"] = "eastern_palace"
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> labels_;
  
  std::string GetLabel(const std::string& type, const std::string& key);
  void EditLabel(const std::string& type, const std::string& key, const std::string& newValue);
};
```

**File Format** (`labels.txt`):
```ini
[overworld]
0=Light World
1=Dark World
3=Desert Region

[dungeon]
0x00=Hyrule Castle
0x02=Eastern Palace
0x04=Desert Palace

[entrance]
0x00=Links House
0x01=Sanctuary

[room]
0x00_0x10=Eastern Palace Boss Room
0x04_0x05=Desert Palace Treasure Room
```

### Proposed Enhancement

**1. Export ResourceLabels to JSON for AI**

```json
{
  "overworld": {
    "maps": [
      {"id": 0, "label": "Light World", "user_label": "hyrule_overworld"},
      {"id": 1, "label": "Dark World", "user_label": "dark_world"},
      {"id": 3, "label": "Desert", "user_label": "lanmola_desert"}
    ]
  },
  "dungeons": {
    "list": [
      {"id": "0x00", "label": "Hyrule Castle", "user_label": "castle", "rooms": 67},
      {"id": "0x02", "label": "Eastern Palace", "user_label": "east_palace", "rooms": 20}
    ]
  },
  "entrances": {
    "list": [
      {"id": "0x00", "label": "Link's House", "user_label": "starting_house", "map": 0},
      {"id": "0x01", "label": "Sanctuary", "user_label": "church", "map": 0}
    ]
  }
}
```

**2. Enhanced PromptBuilder Integration**

```cpp
// In BuildContextualPrompt()
std::string PromptBuilder::BuildContextualPrompt(
    const std::string& user_prompt,
    const RomContext& context) {
  
  std::string prompt = BuildSystemInstruction();
  
  // NEW: Add resource labels context
  if (context.rom_loaded && !context.resource_labels.empty()) {
    prompt += "\n\n=== AVAILABLE RESOURCES ===\n";
    
    for (const auto& [category, labels] : context.resource_labels) {
      prompt += absl::StrFormat("\n%s:\n", absl::AsciiStrToTitle(category));
      
      for (const auto& [key, label] : labels) {
        prompt += absl::StrFormat("  - %s: \"%s\"\n", key, label);
      }
    }
  }
  
  prompt += absl::StrFormat("\n\n=== USER PROMPT ===\n%s\n", user_prompt);
  
  return prompt;
}
```

## Dungeon Editor Considerations

### Current State
- ✅ DungeonEditor exists (`src/app/editor/dungeon/dungeon_editor.h`)
- ✅ DungeonEditorSystem provides object/sprite/entrance editing
- ✅ ObjectRenderer handles room rendering
- ⚠️ **No visual preview available yet** (mentioned by user)
- ⚠️ Room data structure is complex

### AI-Friendly Dungeon Operations

**Focus on Data Generation** (not visual editing):
```cpp
// AI can generate valid dungeon data without preview
struct DungeonProposal {
  std::string dungeon_label;  // "eastern_palace" or "0x02"
  std::string room_label;     // "boss_room" or "0x10"
  
  std::vector<ObjectPlacement> objects;  // Walls, floors, decorations
  std::vector<SpritePlacement> sprites;  // Enemies, NPCs
  std::vector<Entrance> entrances;       // Room connections
  std::vector<Chest> chests;             // Treasure
};

// Example AI generation
AI Prompt: "Add 3 soldiers to the entrance of Eastern Palace"
AI Response:
{
  "commands": [
    "dungeon add-sprite --dungeon east_palace --room entrance_room --sprite soldier --x 5 --y 3",
    "dungeon add-sprite --dungeon east_palace --room entrance_room --sprite soldier --x 10 --y 3",
    "dungeon add-sprite --dungeon east_palace --room entrance_room --sprite soldier --x 7 --y 8"
  ],
  "reasoning": "Using user label 'east_palace' for dungeon 0x02, placing soldiers in entrance room formation"
}
```

## Implementation Phases

### Phase 1: SSL + Overworld Tile16 Basics (This Week)
- [x] Enable SSL support (see SSL-AND-COLLABORATIVE-PLAN.md)
- [ ] Implement Tile16ProposalGenerator basic structure
- [ ] Add overworld tile16 commands to CLI
- [ ] Update PromptBuilder with overworld-focused examples
- [ ] Test basic "place a tree" workflow

### Phase 2: ResourceLabels Integration (Next Week)
- [ ] Implement ResourceContextBuilder
- [ ] Extract labels from ROM project
- [ ] Inject labels into AI prompts
- [ ] Test label-aware prompts ("add trees to my custom forest")
- [ ] Document label file format for users

### Phase 3: Visual Diff & Accept/Reject (Week 3)
- [ ] Implement visual diff generation
- [ ] Add before/after screenshot comparison
- [ ] Create accept/reject CLI workflow
- [ ] Add proposal history tracking
- [ ] Test multi-step proposals

### Phase 4: Dungeon Editing (Month 2)
- [ ] Implement DungeonProposalGenerator
- [ ] Add dungeon CLI commands
- [ ] Test sprite/object placement
- [ ] Validate entrance connections
- [ ] Document dungeon editing workflow

## Success Metrics

### Overworld Editing
- [ ] AI can place individual tiles correctly
- [ ] AI can create tile patterns (rivers, paths, forests)
- [ ] AI understands user's custom map labels
- [ ] Visual diff shows changes clearly
- [ ] Accept/reject workflow is intuitive

### Dungeon Editing
- [ ] AI can find rooms by user labels
- [ ] AI can place sprites in valid positions
- [ ] AI can configure entrances correctly
- [ ] Proposals don't break room data
- [ ] Generated data passes validation

### ResourceLabels
- [ ] AI uses user's custom labels correctly
- [ ] AI falls back to IDs when no label exists
- [ ] AI explains which resources it's using
- [ ] Label extraction works for all resource types
- [ ] JSON export is complete and accurate

---

**Status**: 📋 DESIGN COMPLETE - Ready for Phase 1 Implementation  
**Next Action**: Enable SSL support, then implement Tile16ProposalGenerator  
**Timeline**: 3-4 weeks for full overworld/dungeon AI integration

