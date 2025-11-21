# Z3ED Workflow Examples

This guide demonstrates practical workflows using z3ed CLI for common ROM hacking tasks, test automation, and AI-assisted development.

## Table of Contents

1. [Basic ROM Editing Workflow](#basic-rom-editing-workflow)
2. [Automated Testing Pipeline](#automated-testing-pipeline)
3. [AI-Assisted Development](#ai-assisted-development)
4. [Multi-Agent Collaboration](#multi-agent-collaboration)
5. [CI/CD Integration](#cicd-integration)
6. [Advanced Automation Scripts](#advanced-automation-scripts)

## Basic ROM Editing Workflow

### Scenario: Adding a New Dungeon Room

This workflow demonstrates how to create and populate a new dungeon room using z3ed commands.

```bash
#!/bin/bash
# new_dungeon_room.sh - Create and populate a new dungeon room

# Load ROM and create snapshot for safety
z3ed rom snapshot --name "before_new_room" --compress

# Open dungeon editor for room 50
z3ed editor dungeon set-property --room 50 --property "layout" --value "2x2"
z3ed editor dungeon set-property --room 50 --property "floor1_graphics" --value 0x0A

# Place entrance and exit doors
z3ed editor dungeon place-object --room 50 --type 0x00 --x 7 --y 0  # North door
z3ed editor dungeon place-object --room 50 --type 0x00 --x 7 --y 15 # South door

# Add enemies
z3ed editor dungeon place-object --room 50 --type 0x08 --x 4 --y 5  # Soldier
z3ed editor dungeon place-object --room 50 --type 0x08 --x 10 --y 5 # Soldier
z3ed editor dungeon place-object --room 50 --type 0x0C --x 7 --y 8  # Knight

# Add treasure chest with key
z3ed editor dungeon place-object --room 50 --type 0x22 --x 7 --y 12
z3ed editor dungeon set-property --room 50 --property "chest_contents" --value "small_key"

# Validate the room
z3ed editor dungeon validate-room --room 50 --fix-issues

# Test in emulator
z3ed emulator run --warp-to-room 50
```

### Scenario: Batch Tile Replacement

Replace all instances of a tile across multiple overworld maps.

```bash
#!/bin/bash
# batch_tile_replace.sh - Replace tiles across overworld maps

# Define old and new tile IDs
OLD_TILE=0x142  # Old grass tile
NEW_TILE=0x143  # New grass variant

# Create snapshot
z3ed rom snapshot --name "before_tile_replacement"

# Create batch operation file
cat > tile_replacement.json << EOF
{
  "operations": [
EOF

# Generate operations for all Light World maps (0x00-0x3F)
for map in {0..63}; do
  # Find all occurrences of the old tile
  positions=$(z3ed query find-tiles --map $map --tile $OLD_TILE --format json)

  # Parse positions and add replacement operations
  echo "$positions" | jq -r '.positions[] |
    "    {\"editor\": \"overworld\", \"action\": \"set-tile\", \"params\": {\"map\": '$map', \"x\": .x, \"y\": .y, \"tile\": '$NEW_TILE'}},"' >> tile_replacement.json
done

# Close JSON and execute
echo '  ]
}' >> tile_replacement.json

# Execute batch operation
z3ed editor batch --script tile_replacement.json --dry-run
read -p "Proceed with replacement? (y/n) " -n 1 -r
if [[ $REPLY =~ ^[Yy]$ ]]; then
  z3ed editor batch --script tile_replacement.json
fi
```

## Automated Testing Pipeline

### Scenario: Test-Driven Dungeon Development

Create tests before implementing dungeon features.

```bash
#!/bin/bash
# test_driven_dungeon.sh - TDD approach for dungeon creation

# Start test recording
z3ed test record --name "dungeon_puzzle_test" --start

# Define expected behavior
cat > expected_behavior.json << EOF
{
  "room": 75,
  "requirements": [
    "Player must push block to activate switch",
    "Door opens when switch is activated",
    "Chest appears after door opens",
    "Room must be completable in under 60 seconds"
  ]
}
EOF

# Record the intended solution path
z3ed editor dungeon place-object --room 75 --type "push_block" --x 5 --y 5
z3ed editor dungeon place-object --room 75 --type "floor_switch" --x 10 --y 10
z3ed editor dungeon place-object --room 75 --type "locked_door" --x 7 --y 0
z3ed editor dungeon set-property --room 75 --property "switch_target" --value "locked_door"

# Stop recording and generate test
z3ed test record --stop --save-as dungeon_puzzle_recording.json
z3ed test generate --from-recording dungeon_puzzle_recording.json \
                  --requirements expected_behavior.json \
                  --output test_dungeon_puzzle.cc

# Compile and run the test
z3ed test run --file test_dungeon_puzzle.cc

# Run continuously during development
watch -n 5 'z3ed test run --file test_dungeon_puzzle.cc --quiet'
```

### Scenario: Regression Test Suite

Automated regression testing for ROM modifications.

```bash
#!/bin/bash
# regression_test_suite.sh - Comprehensive regression testing

# Create baseline from stable version
z3ed test baseline --create --name "stable_v1.0"

# Define test suite
cat > regression_tests.yaml << EOF
tests:
  - name: "Overworld Collision"
    commands:
      - "editor overworld validate-collision --map ALL"
    expected: "no_errors"

  - name: "Dungeon Room Connectivity"
    commands:
      - "query dungeon-graph --check-connectivity"
    expected: "all_rooms_reachable"

  - name: "Sprite Limits"
    commands:
      - "query sprite-count --per-screen"
    expected: "max_sprites <= 16"

  - name: "Memory Usage"
    commands:
      - "query memory-usage --runtime"
    expected: "usage < 95%"

  - name: "Save/Load Integrity"
    commands:
      - "test save-load --iterations 100"
    expected: "no_corruption"
EOF

# Run regression suite
z3ed test run --suite regression_tests.yaml --parallel

# Compare against baseline
z3ed test baseline --compare --name "stable_v1.0" --threshold 98

# Generate report
z3ed test coverage --report html --output coverage_report.html
```

## AI-Assisted Development

### Scenario: AI-Powered Bug Fix

Use AI to identify and fix a bug in dungeon logic.

```bash
#!/bin/bash
# ai_bug_fix.sh - AI-assisted debugging

# Describe the bug to AI
BUG_DESCRIPTION="Player gets stuck when entering room 42 from the south"

# Ask AI to analyze
z3ed ai analyze --type bug \
                --context "room=42" \
                --description "$BUG_DESCRIPTION" \
                --output bug_analysis.json

# Get AI suggestions for fix
z3ed ai suggest --task "fix dungeon room entry bug" \
               --context bug_analysis.json \
               --output suggested_fix.json

# Review suggestions
cat suggested_fix.json | jq '.suggestions[]'

# Apply AI-suggested fix (after review)
z3ed ai apply --fix suggested_fix.json --dry-run

read -p "Apply AI-suggested fix? (y/n) " -n 1 -r
if [[ $REPLY =~ ^[Yy]$ ]]; then
  z3ed ai apply --fix suggested_fix.json

  # Validate the fix
  z3ed editor dungeon validate-room --room 42
  z3ed test run --filter "*Room42*"
fi

# Generate regression test for the bug
z3ed test generate --type regression \
                  --bug "$BUG_DESCRIPTION" \
                  --fix suggested_fix.json \
                  --output test_room42_entry.cc
```

### Scenario: AI Test Generation

Generate comprehensive tests using AI.

```bash
#!/bin/bash
# ai_test_generation.sh - AI-powered test creation

# Select component to test
COMPONENT="OverworldEditor"

# Analyze code and generate test specification
z3ed ai analyze --code src/app/editor/overworld_editor.cc \
               --task "identify test cases" \
               --output test_spec.json

# Generate comprehensive test suite
z3ed test generate --target $COMPONENT \
                  --spec test_spec.json \
                  --include-edge-cases \
                  --include-mocks \
                  --framework gtest \
                  --output ${COMPONENT}_test.cc

# AI review of generated tests
z3ed ai review --file ${COMPONENT}_test.cc \
              --criteria "coverage,correctness,performance" \
              --output test_review.json

# Apply AI improvements
z3ed ai improve --file ${COMPONENT}_test.cc \
               --feedback test_review.json \
               --output ${COMPONENT}_test_improved.cc

# Run and validate tests
z3ed test run --file ${COMPONENT}_test_improved.cc --coverage
```

## Multi-Agent Collaboration

### Scenario: Parallel ROM Development

Multiple AI agents working on different aspects simultaneously.

```python
#!/usr/bin/env python3
# multi_agent_development.py - Coordinate multiple AI agents

import asyncio
import json
from z3ed_client import Agent, Coordinator

async def main():
    # Initialize coordinator
    coordinator = Coordinator("localhost:8080")

    # Define agents with specializations
    agents = [
        Agent("overworld_specialist", capabilities=["overworld", "sprites"]),
        Agent("dungeon_specialist", capabilities=["dungeon", "objects"]),
        Agent("graphics_specialist", capabilities=["graphics", "palettes"]),
        Agent("testing_specialist", capabilities=["testing", "validation"])
    ]

    # Connect all agents
    for agent in agents:
        await agent.connect()

    # Define parallel tasks
    tasks = [
        {
            "id": "task_1",
            "type": "overworld",
            "description": "Optimize Light World map connections",
            "assigned_to": "overworld_specialist"
        },
        {
            "id": "task_2",
            "type": "dungeon",
            "description": "Balance enemy placement in dungeons 1-3",
            "assigned_to": "dungeon_specialist"
        },
        {
            "id": "task_3",
            "type": "graphics",
            "description": "Create new palette variations for seasons",
            "assigned_to": "graphics_specialist"
        },
        {
            "id": "task_4",
            "type": "testing",
            "description": "Generate tests for all recent changes",
            "assigned_to": "testing_specialist"
        }
    ]

    # Queue tasks
    for task in tasks:
        coordinator.queue_task(task)

    # Monitor progress
    while not coordinator.all_tasks_complete():
        status = coordinator.get_status()
        print(f"Progress: {status['completed']}/{status['total']} tasks")

        # Handle conflicts if they arise
        if status['conflicts']:
            for conflict in status['conflicts']:
                resolution = coordinator.resolve_conflict(
                    conflict,
                    strategy="merge"  # or "last_write_wins", "manual"
                )
                print(f"Resolved conflict: {resolution}")

        await asyncio.sleep(5)

    # Collect results
    results = coordinator.get_all_results()

    # Generate combined report
    report = {
        "timestamp": datetime.now().isoformat(),
        "agents": [agent.name for agent in agents],
        "tasks_completed": len(results),
        "results": results
    }

    with open("multi_agent_report.json", "w") as f:
        json.dump(report, f, indent=2)

    print("Multi-agent development complete!")

if __name__ == "__main__":
    asyncio.run(main())
```

### Scenario: Agent Coordination Script

Bash script for coordinating multiple z3ed instances.

```bash
#!/bin/bash
# agent_coordination.sh - Coordinate multiple z3ed agents

# Start coordination server
z3ed server start --port 8080 --config server.yaml &
SERVER_PID=$!

# Function to run agent task
run_agent() {
    local agent_name=$1
    local task=$2
    local log_file="${agent_name}.log"

    echo "Starting $agent_name for task: $task"
    z3ed agent run --name "$agent_name" \
                  --task "$task" \
                  --server localhost:8080 \
                  --log "$log_file" &
}

# Start multiple agents
run_agent "agent_overworld" "optimize overworld maps 0x00-0x3F"
run_agent "agent_dungeon" "validate and fix all dungeon rooms"
run_agent "agent_graphics" "compress unused graphics data"
run_agent "agent_testing" "generate missing unit tests"

# Monitor agent progress
while true; do
    clear
    echo "=== Agent Status ==="
    z3ed agent status --server localhost:8080 --format table

    # Check for completion
    if z3ed agent status --server localhost:8080 --check-complete; then
        echo "All agents completed!"
        break
    fi

    sleep 10
done

# Collect and merge results
z3ed agent collect-results --server localhost:8080 --output results/

# Stop server
kill $SERVER_PID
```

## CI/CD Integration

### Scenario: GitHub Actions Integration

`.github/workflows/z3ed-testing.yml`:

```yaml
name: Z3ED Automated Testing

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  rom-validation:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Setup z3ed
        run: |
          ./scripts/install-z3ed.sh
          z3ed --version

      - name: Load ROM
        run: |
          z3ed rom load --file ${{ secrets.ROM_PATH }}
          z3ed rom validate

      - name: Run validation suite
        run: |
          z3ed test run --suite validation.yaml
          z3ed query stats --type all --output stats.json

      - name: Check for regressions
        run: |
          z3ed test baseline --compare --name stable --threshold 95

      - name: Generate report
        run: |
          z3ed test coverage --report markdown --output REPORT.md

      - name: Comment on PR
        if: github.event_name == 'pull_request'
        uses: actions/github-script@v6
        with:
          script: |
            const fs = require('fs');
            const report = fs.readFileSync('REPORT.md', 'utf8');
            github.rest.issues.createComment({
              issue_number: context.issue.number,
              owner: context.repo.owner,
              repo: context.repo.repo,
              body: report
            });

  ai-review:
    runs-on: ubuntu-latest
    needs: rom-validation
    steps:
      - name: AI Code Review
        run: |
          z3ed ai review --changes ${{ github.sha }} \
                        --model gemini-pro \
                        --output review.json

      - name: Apply AI Suggestions
        run: |
          z3ed ai apply --suggestions review.json --auto-approve safe
```

### Scenario: Local CI Pipeline

```bash
#!/bin/bash
# local_ci.sh - Local CI/CD pipeline

# Configuration
ROM_FILE="zelda3.sfc"
BUILD_DIR="build"
TEST_RESULTS="test_results"

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Function to print colored output
print_status() {
    local status=$1
    local message=$2
    case $status in
        "SUCCESS") echo -e "${GREEN}✓${NC} $message" ;;
        "FAILURE") echo -e "${RED}✗${NC} $message" ;;
        "WARNING") echo -e "${YELLOW}⚠${NC} $message" ;;
        *) echo "$message" ;;
    esac
}

# Step 1: Clean and build
print_status "INFO" "Starting CI pipeline..."
z3ed build clean --all
z3ed build --preset lin-dbg --parallel 8

if [ $? -eq 0 ]; then
    print_status "SUCCESS" "Build completed"
else
    print_status "FAILURE" "Build failed"
    exit 1
fi

# Step 2: Run tests
mkdir -p $TEST_RESULTS
z3ed test run --category unit --output $TEST_RESULTS/unit.xml
UNIT_RESULT=$?

z3ed test run --category integration --rom $ROM_FILE --output $TEST_RESULTS/integration.xml
INTEGRATION_RESULT=$?

if [ $UNIT_RESULT -eq 0 ] && [ $INTEGRATION_RESULT -eq 0 ]; then
    print_status "SUCCESS" "All tests passed"
else
    print_status "FAILURE" "Some tests failed"
    z3ed test report --dir $TEST_RESULTS --format console
fi

# Step 3: ROM validation
z3ed rom load --file $ROM_FILE
z3ed rom validate --checksums --headers --regions

if [ $? -eq 0 ]; then
    print_status "SUCCESS" "ROM validation passed"
else
    print_status "WARNING" "ROM validation warnings"
fi

# Step 4: Performance benchmarks
z3ed test benchmark --suite performance.yaml --output benchmarks.json
z3ed test benchmark --compare-baseline --threshold 110

# Step 5: Generate reports
z3ed test coverage --report html --output coverage/
z3ed test report --comprehensive --output CI_REPORT.md

print_status "SUCCESS" "CI pipeline completed!"
```

## Advanced Automation Scripts

### Scenario: Intelligent Room Generator

Generate dungeon rooms using AI and templates.

```python
#!/usr/bin/env python3
# intelligent_room_generator.py - AI-powered room generation

import json
import random
from z3ed_client import Z3edClient

class IntelligentRoomGenerator:
    def __init__(self, client):
        self.client = client
        self.templates = self.load_templates()

    def load_templates(self):
        """Load room templates from file"""
        with open("room_templates.json", "r") as f:
            return json.load(f)

    def generate_room(self, room_id, difficulty="medium", theme="castle"):
        """Generate a room based on parameters"""

        # Select appropriate template
        template = self.select_template(difficulty, theme)

        # Get AI suggestions for room layout
        suggestions = self.client.ai_suggest(
            task="generate dungeon room layout",
            constraints={
                "room_id": room_id,
                "difficulty": difficulty,
                "theme": theme,
                "template": template["name"]
            }
        )

        # Apply base template
        self.apply_template(room_id, template)

        # Apply AI suggestions
        for suggestion in suggestions["modifications"]:
            self.apply_modification(room_id, suggestion)

        # Validate room
        validation = self.client.validate_room(room_id)

        if not validation["valid"]:
            # Ask AI to fix issues
            fixes = self.client.ai_suggest(
                task="fix dungeon room issues",
                context={
                    "room_id": room_id,
                    "issues": validation["issues"]
                }
            )

            for fix in fixes["fixes"]:
                self.apply_modification(room_id, fix)

        return self.get_room_data(room_id)

    def select_template(self, difficulty, theme):
        """Select best matching template"""
        matching = [
            t for t in self.templates
            if t["difficulty"] == difficulty and t["theme"] == theme
        ]
        return random.choice(matching) if matching else self.templates[0]

    def apply_template(self, room_id, template):
        """Apply template to room"""
        # Set room properties
        for prop, value in template["properties"].items():
            self.client.set_room_property(room_id, prop, value)

        # Place template objects
        for obj in template["objects"]:
            self.client.place_object(
                room_id,
                obj["type"],
                obj["x"],
                obj["y"]
            )

    def apply_modification(self, room_id, modification):
        """Apply a single modification to room"""
        action = modification["action"]
        params = modification["params"]

        if action == "place_object":
            self.client.place_object(room_id, **params)
        elif action == "set_property":
            self.client.set_room_property(room_id, **params)
        elif action == "remove_object":
            self.client.remove_object(room_id, **params)

    def generate_dungeon(self, start_room, num_rooms, difficulty_curve):
        """Generate entire dungeon"""
        rooms = []

        for i in range(num_rooms):
            room_id = start_room + i

            # Adjust difficulty based on curve
            difficulty = self.calculate_difficulty(i, num_rooms, difficulty_curve)

            # Generate room
            room_data = self.generate_room(room_id, difficulty)
            rooms.append(room_data)

            # Connect to previous room
            if i > 0:
                self.connect_rooms(room_id - 1, room_id)

        # Final validation
        self.validate_dungeon(start_room, num_rooms)

        return rooms

    def calculate_difficulty(self, index, total, curve):
        """Calculate difficulty based on position and curve"""
        if curve == "linear":
            return ["easy", "medium", "hard"][min(index // (total // 3), 2)]
        elif curve == "exponential":
            return ["easy", "medium", "hard"][min(int((index / total) ** 2 * 3), 2)]
        else:
            return "medium"

# Main execution
if __name__ == "__main__":
    client = Z3edClient("localhost:8080")
    generator = IntelligentRoomGenerator(client)

    # Generate a 10-room dungeon
    dungeon = generator.generate_dungeon(
        start_room=100,
        num_rooms=10,
        difficulty_curve="exponential"
    )

    # Save dungeon data
    with open("generated_dungeon.json", "w") as f:
        json.dump(dungeon, f, indent=2)

    print(f"Generated {len(dungeon)} rooms successfully!")
```

### Scenario: Automated ROM Optimizer

Optimize ROM for size and performance.

```bash
#!/bin/bash
# rom_optimizer.sh - Automated ROM optimization

# Create backup
z3ed rom snapshot --name "pre-optimization" --compress

# Step 1: Identify unused space
echo "Analyzing ROM for optimization opportunities..."
z3ed query find-unused-space --min-size 256 --output unused_space.json

# Step 2: Compress graphics
echo "Compressing graphics data..."
for sheet in {0..223}; do
    z3ed editor graphics compress-sheet --sheet $sheet --algorithm lz77
done

# Step 3: Optimize sprite data
echo "Optimizing sprite data..."
z3ed ai analyze --type optimization \
               --target sprites \
               --output sprite_optimization.json

z3ed ai apply --optimizations sprite_optimization.json

# Step 4: Remove duplicate data
echo "Removing duplicate data..."
z3ed query find-duplicates --min-size 16 --output duplicates.json
z3ed optimize remove-duplicates --input duplicates.json --safe-mode

# Step 5: Optimize room data
echo "Optimizing dungeon room data..."
for room in {0..295}; do
    z3ed editor dungeon optimize-room --room $room \
                                     --remove-unreachable \
                                     --compress-objects
done

# Step 6: Pack data efficiently
echo "Repacking ROM data..."
z3ed optimize repack --strategy best-fit

# Step 7: Validate optimization
echo "Validating optimized ROM..."
z3ed rom validate --comprehensive
z3ed test run --suite optimization_validation.yaml

# Step 8: Generate report
original_size=$(z3ed rom info --snapshot "pre-optimization" | jq '.size')
optimized_size=$(z3ed rom info | jq '.size')
saved=$((original_size - optimized_size))
percent=$((saved * 100 / original_size))

cat > optimization_report.md << EOF
# ROM Optimization Report

## Summary
- Original Size: $original_size bytes
- Optimized Size: $optimized_size bytes
- Space Saved: $saved bytes ($percent%)

## Optimizations Applied
$(z3ed optimize list-applied --format markdown)

## Validation Results
$(z3ed test results --suite optimization_validation.yaml --format markdown)
EOF

echo "Optimization complete! Report saved to optimization_report.md"
```

## Best Practices

### 1. Always Create Snapshots
Before any major operation:
```bash
z3ed rom snapshot --name "descriptive_name" --compress
```

### 2. Use Dry-Run for Dangerous Operations
```bash
z3ed editor batch --script changes.json --dry-run
```

### 3. Validate After Changes
```bash
z3ed rom validate
z3ed test run --quick
```

### 4. Document Your Workflows
```bash
# Generate documentation from your scripts
z3ed docs generate --from-script my_workflow.sh --output workflow_docs.md
```

### 5. Use AI for Review
```bash
z3ed ai review --changes . --criteria "correctness,performance,style"
```

## Troubleshooting Common Issues

### Issue: Command Not Found
```bash
# Verify z3ed is in PATH
which z3ed

# Or use full path
/usr/local/bin/z3ed --version
```

### Issue: ROM Won't Load
```bash
# Check ROM validity
z3ed rom validate --file suspicious.sfc --verbose

# Try with different region
z3ed rom load --file rom.sfc --region USA
```

### Issue: Test Failures
```bash
# Run with verbose output
z3ed test run --verbose --filter failing_test

# Generate detailed report
z3ed test debug --test failing_test --output debug_report.json
```

### Issue: Network Connection Failed
```bash
# Test connection
z3ed network ping --host localhost --port 8080

# Use fallback mode
z3ed --offline --cache-only
```

## Next Steps

- Explore the [Z3ED Command Reference](z3ed-command-reference.md)
- Read the [API Documentation](../reference/api/)
- Join the [YAZE Discord](https://discord.gg/yaze) for support
- Contribute your workflows to the [Examples Repository](https://github.com/yaze/z3ed-examples)