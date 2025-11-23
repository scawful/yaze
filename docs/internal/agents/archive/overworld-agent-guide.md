# Overworld Agent Guide - AI-Powered Overworld Editing

**Version**: 1.0  
**Last Updated**: October 6, 2025  
**Audience**: AI Agents, z3ed users, automation developers

---

## Overview

This guide explains how AI agents can interact with YAZE's overworld editor through the `z3ed` CLI and automation APIs. It covers:
- Available tools and commands
- Multimodal vision workflows
- Proposal-based editing
- Best practices for AI-generated edits

---

## Quick Start

### Prerequisites
```bash
# Build YAZE with AI and gRPC support
cmake -B build -DZ3ED_AI=ON -DYAZE_WITH_GRPC=ON
cmake --build build --target z3ed

# Set up AI provider (Gemini recommended for vision)
export GEMINI_API_KEY="your-key-here"
```

### First Agent Interaction
```bash
# Ask AI about a map
z3ed agent simple-chat "What tiles are at position 10,10 on map 0?" --rom zelda3.sfc

# AI agent generates edits
z3ed agent run --prompt "Place trees in a 3x3 grid at position 10,10 on map 0" \
  --rom zelda3.sfc --sandbox

# Review and accept
z3ed agent diff --proposal-id <id>
z3ed agent accept --proposal-id <id>
```

---

## Available Tools

### Read-Only Tools (Safe for AI)

#### overworld-get-tile
Query tile ID at coordinates.

**Purpose**: Analyze existing tile placement  
**Safety**: Read-only, no ROM modification  
**Rate Limit**: None

```json
{
  "tool": "overworld-get-tile",
  "parameters": {
    "map": 0,
    "x": 10,
    "y": 10
  }
}
```

**Response**:
```json
{
  "tile_id": 66,
  "tile_id_hex": "0x0042",
  "position": {"x": 10, "y": 10}
}
```

**Use Cases**:
- Check what tile currently exists before painting
- Analyze patterns in tile placement
- Verify expected tiles after edits

---

#### overworld-get-visible-region
Get tiles currently visible on canvas.

**Purpose**: Understand what the user is looking at  
**Safety**: Read-only  
**Rate Limit**: None

```json
{
  "tool": "overworld-get-visible-region",
  "parameters": {
    "map": 0
  }
}
```

**Response**:
```json
{
  "region": {
    "x_start": 0,
    "y_start": 0,
    "x_end": 31,
    "y_end": 31
  },
  "tiles": [
    {"x": 0, "y": 0, "tile_id": 40},
    {"x": 1, "y": 0, "tile_id": 40},
    ...
  ]
}
```

**Use Cases**:
- Analyze visible area before suggesting edits
- Generate context-aware suggestions
- Understand user's current focus

---

#### overworld-analyze-region
Get tile composition and patterns in a region.

**Purpose**: Deep analysis of tile distribution  
**Safety**: Read-only  
**Rate Limit**: Large regions (>1000 tiles) may be slow

```json
{
  "tool": "overworld-analyze-region",
  "parameters": {
    "map": 0,
    "x1": 0,
    "y1": 0,
    "x2": 31,
    "y2": 31
  }
}
```

**Response**:
```json
{
  "tile_counts": {
    "40": 512,   // Grass
    "66": 128,   // Tree
    "80": 64     // Water
  },
  "patterns": [
    {
      "type": "forest",
      "center": {"x": 15, "y": 15},
      "size": {"width": 10, "height": 10}
    }
  ],
  "statistics": {
    "total_tiles": 1024,
    "unique_tiles": 15,
    "most_common_tile": 40
  }
}
```

**Use Cases**:
- Understand map composition before edits
- Detect patterns (forests, water bodies, paths)
- Generate statistics for reports

---

### Write Tools (Sandboxed - Creates Proposals)

#### overworld-set-tile
Paint a single tile (creates proposal).

**Purpose**: Modify single tile  
**Safety**: Sandboxed, creates proposal  
**Rate Limit**: Reasonable (don't spam)

```json
{
  "tool": "overworld-set-tile",
  "parameters": {
    "map": 0,
    "x": 10,
    "y": 10,
    "tile_id": 66
  }
}
```

**Response**:
```json
{
  "proposal_id": "abc123",
  "success": true,
  "message": "Proposal created: Set tile at (10,10) to 0x0042"
}
```

**Use Cases**:
- Fix individual tiles
- Place objects at specific coordinates
- Correct tile placement errors

---

#### overworld-set-tiles-batch
Paint multiple tiles in one operation (creates proposal).

**Purpose**: Efficient multi-tile editing  
**Safety**: Sandboxed, creates proposal  
**Rate Limit**: Max 1000 tiles per batch

```json
{
  "tool": "overworld-set-tiles-batch",
  "parameters": {
    "map": 0,
    "tiles": [
      {"x": 10, "y": 10, "tile_id": 66},
      {"x": 11, "y": 10, "tile_id": 66},
      {"x": 12, "y": 10, "tile_id": 66}
    ]
  }
}
```

**Response**:
```json
{
  "proposal_id": "abc123",
  "tiles_painted": 3,
  "success": true
}
```

**Use Cases**:
- Create patterns (forests, paths, water bodies)
- Fill regions with specific tiles
- Generate complex map structures

---

## Multimodal Vision Workflow

### Step 1: Capture Canvas Screenshot
```bash
# From CLI
z3ed agent vision --capture-canvas "Overworld Canvas" \
  --prompt "Analyze this overworld map" \
  --rom zelda3.sfc

# From agent workflow
z3ed agent run --prompt "Analyze map 0 and suggest improvements" \
  --rom zelda3.sfc --sandbox
```

### Step 2: AI Analyzes Screenshot
Gemini Vision API receives:
- Screenshot of canvas (PNG/JPEG)
- User prompt
- Context (map index, visible region)

AI returns:
```json
{
  "analysis": {
    "observations": [
      "Grass tiles dominate the visible area",
      "Tree tiles are sparse and unnatural",
      "Water tiles at (15,15) have incorrect palette colors",
      "Path from (5,5) to (25,5) is broken"
    ],
    "composition_score": 6.5,
    "issues": [
      {
        "type": "sparse_trees",
        "severity": "medium",
        "location": {"x": 10, "y": 10},
        "suggestion": "Add more tree tiles for forest theme"
      }
    ]
  }
}
```

### Step 3: Generate Edit Plan
AI creates actionable plan:
```json
{
  "plan": [
    {
      "tool": "overworld-set-tiles-batch",
      "parameters": {
        "map": 0,
        "tiles": [
          {"x": 10, "y": 10, "tile_id": 66},
          {"x": 11, "y": 10, "tile_id": 66},
          {"x": 12, "y": 10, "tile_id": 66}
        ]
      },
      "reason": "Create denser forest area"
    }
  ]
}
```

### Step 4: Execute Plan (Sandbox)
```bash
# z3ed executes plan in sandbox
z3ed agent run --plan plan.json --rom zelda3.sfc --sandbox
```

### Step 5: Human Review
```bash
# View proposed changes
z3ed agent diff --proposal-id abc123

# Accept or reject
z3ed agent accept --proposal-id abc123
# or
z3ed agent reject --proposal-id abc123
```

---

## Example Workflows

### Workflow 1: Create Forest Area

**User Prompt**: "Create a forest clearing at position 15,15 with grass in the center"

**AI Plan**:
```json
{
  "steps": [
    {
      "step": 1,
      "description": "Check current tiles in region",
      "tool": "overworld-analyze-region",
      "parameters": {
        "map": 0,
        "x1": 10,
        "y1": 10,
        "x2": 20,
        "y2": 20
      }
    },
    {
      "step": 2,
      "description": "Paint grass clearing (center 5x5)",
      "tool": "overworld-set-tiles-batch",
      "parameters": {
        "map": 0,
        "tiles": [
          {"x": 13, "y": 13, "tile_id": 40},
          {"x": 14, "y": 13, "tile_id": 40},
          ...
        ]
      }
    },
    {
      "step": 3,
      "description": "Plant trees around edges",
      "tool": "overworld-set-tiles-batch",
      "parameters": {
        "map": 0,
        "tiles": [
          {"x": 10, "y": 10, "tile_id": 66},
          {"x": 11, "y": 10, "tile_id": 66},
          ...
        ]
      }
    }
  ]
}
```

**CLI Execution**:
```bash
z3ed agent run --prompt "Create a forest clearing at position 15,15 with grass in the center" \
  --rom zelda3.sfc --sandbox
```

**Result**: Proposal created with 50+ tile changes

---

### Workflow 2: Fix Tile Placement Errors

**User Prompt**: "Fix any water tiles that should be grass in the visible region"

**AI Plan**:
```json
{
  "steps": [
    {
      "step": 1,
      "tool": "overworld-get-visible-region",
      "parameters": {"map": 0}
    },
    {
      "step": 2,
      "tool": "overworld-analyze-region",
      "parameters": {
        "map": 0,
        "x1": 0,
        "y1": 0,
        "x2": 31,
        "y2": 31
      }
    },
    {
      "step": 3,
      "description": "Identify misplaced water tiles",
      "logic": "Find water tiles (80) surrounded by grass (40)"
    },
    {
      "step": 4,
      "tool": "overworld-set-tiles-batch",
      "parameters": {
        "map": 0,
        "tiles": [
          {"x": 5, "y": 5, "tile_id": 40},
          {"x": 12, "y": 8, "tile_id": 40}
        ]
      }
    }
  ]
}
```

---

### Workflow 3: Generate Path

**User Prompt**: "Create a path from (5,5) to (25,25) using path tiles"

**AI Plan**:
```json
{
  "steps": [
    {
      "step": 1,
      "description": "Calculate path coordinates",
      "logic": "Line from (5,5) to (25,25)"
    },
    {
      "step": 2,
      "tool": "overworld-set-tiles-batch",
      "parameters": {
        "map": 0,
        "tiles": [
          {"x": 5, "y": 5, "tile_id": 50},
          {"x": 6, "y": 6, "tile_id": 50},
          {"x": 7, "y": 7, "tile_id": 50},
          ...
        ]
      }
    }
  ]
}
```

---

## Common Tile IDs Reference

### Grass & Ground
- `0x0028` (40) - Grass
- `0x0029` (41) - Dark grass
- `0x002A` (42) - Dirt
- `0x002B` (43) - Sand

### Trees & Plants
- `0x0042` (66) - Tree
- `0x0043` (67) - Bush
- `0x0044` (68) - Flower

### Water
- `0x0050` (80) - Water
- `0x0051` (81) - Deep water
- `0x0052` (82) - Shore

### Paths & Roads
- `0x0032` (50) - Path
- `0x0033` (51) - Road
- `0x0034` (52) - Bridge

### Structures
- `0x0060` (96) - Wall
- `0x0061` (97) - Door
- `0x0062` (98) - Window

---

## Best Practices for AI Agents

### 1. Always Analyze Before Editing
```bash
# GOOD: Check current state first
z3ed agent run --prompt "Analyze map 0 then suggest improvements" --rom zelda3.sfc --sandbox

# BAD: Blindly paint without context
z3ed agent run --prompt "Paint trees everywhere" --rom zelda3.sfc --sandbox
```

### 2. Use Batch Operations
```bash
# GOOD: Single batch operation
overworld-set-tiles-batch (50 tiles)

# BAD: 50 individual operations
overworld-set-tile (×50)
```

### 3. Provide Clear Reasoning
```json
{
  "tool": "overworld-set-tile",
  "parameters": {"x": 10, "y": 10, "tile_id": 66},
  "reason": "Creating forest theme - tree tile at center"
}
```

### 4. Respect Tile Boundaries
Large maps (0x00-0x09, 0x80-0x89) are 512×512 pixels = 32×32 tiles.  
Don't paint beyond `(31, 31)` for these maps.

### 5. Check Visibility
```json
{
  "step": 1,
  "tool": "overworld-get-visible-region",
  "reason": "Ensure tiles are visible before analysis"
}
```

### 6. Create Reversible Edits
Always generate proposals that can be rejected:
```bash
z3ed agent run --prompt "..." --rom zelda3.sfc --sandbox  # Creates proposal
z3ed agent reject --proposal-id abc123                     # Can undo
```

---

## Error Handling

### "Tile ID out of range"
- **Cause**: Invalid tile ID (>4095 for Tile16)
- **Fix**: Validate tile IDs before `set-tile`

### "Coordinates out of bounds"
- **Cause**: Painting beyond map boundaries
- **Fix**: Check map dimensions (typically 32×32 tiles)

### "Proposal rejected"
- **Cause**: Human reviewer rejected changes
- **Fix**: Analyze feedback, adjust plan, try again

### "ROM file locked"
- **Cause**: ROM file open in another process
- **Fix**: Close other instances of YAZE

---

## Testing AI-Generated Edits

### Manual Testing
```bash
# Generate proposal
z3ed agent run --prompt "..." --rom zelda3.sfc --sandbox

# Review in YAZE GUI
yaze zelda3.sfc
# Open Debug → Agent Chat → Proposals
# Review proposal, accept/reject
```

### Automated Testing
```bash
# GUI automation test
z3ed agent test replay overworld_ai_edit.jsonl --rom zelda3.sfc --grpc localhost:50051

# Validate tile placement
z3ed agent test assert --tile-at 10,10 --expected-tile 66 --rom zelda3.sfc
```

---

## Advanced Techniques

### Technique 1: Pattern Recognition
Use multimodal vision to detect patterns:
```bash
z3ed agent vision --capture-canvas "Overworld Canvas" \
  --prompt "Identify repeated tile patterns in this map" \
  --rom zelda3.sfc
```

AI detects:
- Forest clusters
- Water bodies
- Paths and roads
- Building layouts

### Technique 2: Style Transfer
```bash
z3ed agent run --prompt "Make this map look like Kakariko Village from the dark world" \
  --rom zelda3.sfc --sandbox
```

AI:
1. Analyzes Kakariko Village (map 0x18)
2. Extracts tile palette and patterns
3. Applies similar patterns to target map

### Technique 3: Procedural Generation
```bash
z3ed agent run --prompt "Generate a random forest area at 10,10 with natural-looking tree placement" \
  --rom zelda3.sfc --sandbox
```

AI uses procedural algorithms:
- Perlin noise for natural randomness
- Clustering for realistic tree placement
- Edge smoothing for organic boundaries

---

## Integration with GUI Automation

### Record Human Edits
```bash
# Record editing session
z3ed agent test record --suite overworld_forest.jsonl --rom zelda3.sfc
```

### Replay for AI Training
```bash
# Replay recorded session
z3ed agent test replay overworld_forest.jsonl --rom zelda3.sfc

# AI learns from human edits
z3ed agent learn --from-recording overworld_forest.jsonl
```

### Validate AI Edits
```bash
# AI generates edits
z3ed agent run --prompt "..." --rom zelda3.sfc --sandbox

# GUI automation validates
z3ed agent test verify --proposal-id abc123 --suite validation_tests.jsonl
```

---

## Collaboration Features

### Network Collaboration
```bash
# Connect to yaze-server
z3ed net connect ws://localhost:8765

# Join session
z3ed net join ABC123 --username "ai-agent"

# AI agent edits, humans review in real-time
z3ed agent run --prompt "..." --rom zelda3.sfc --sandbox

# Proposal synced to all participants
```

### Proposal Voting
```bash
# Submit proposal to session
z3ed proposal submit --proposal-id abc123 --session ABC123

# Wait for votes
z3ed proposal wait --proposal-id abc123

# Check result
z3ed proposal status --proposal-id abc123
# Output: approved (3/3 votes)
```

---

## Troubleshooting

### Agent Not Responding
```bash
# Check AI provider
z3ed agent ping

# Test simple query
z3ed agent simple-chat "Hello" --rom zelda3.sfc
```

### Tools Not Available
```bash
# Verify z3ed build
z3ed agent describe --resource overworld

# Should show:
# - overworld-get-tile
# - overworld-set-tile
# - overworld-analyze-region
```

### gRPC Connection Failed
```bash
# Check YAZE is running with gRPC
z3ed agent test ping --grpc localhost:50051

# Start YAZE with gRPC enabled
yaze --enable-grpc zelda3.sfc
```

---

## See Also

- [Canvas Automation API](../canvas_automation_api.md) - C++ API reference
- [GUI Automation Scenarios](gui_automation_scenarios.md) - Test examples
- [z3ed README](README.md) - CLI documentation
- [Multimodal Vision](README.md#multimodal-vision-gemini) - Screenshot analysis


