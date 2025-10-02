# z3ed Quick Reference Card

**Last Updated**: October 2, 2025  
**For**: z3ed v0.1.0-alpha (macOS production-ready)

---

## Build & Setup

### Build with gRPC Support
```bash
# Initial build (15-20 min)
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target yaze -j$(sysctl -n hw.ncpu)
cmake --build build-grpc-test --target z3ed -j$(sysctl -n hw.ncpu)

# Incremental rebuild (5-10 sec)
cmake --build build-grpc-test --target z3ed
```

### Start Test Harness
```bash
# Start YAZE with test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# Verify server running
lsof -i :50052

# Kill existing instance
killall yaze 2>/dev/null
```

---

## CLI Commands

### Agent Workflow

#### Create Proposal
```bash
# Run agent with sandbox (recommended)
z3ed agent run --prompt "Make soldiers red" --rom=zelda3.sfc --sandbox

# Run without sandbox (modifies ROM directly)
z3ed agent run --prompt "..." --rom=zelda3.sfc
```

#### List Proposals
```bash
# List all proposals
z3ed agent list

# Output shows:
# - ID
# - Status (Pending/Accepted/Rejected)
# - Created timestamp
# - Prompt
# - Commands executed
# - Bytes changed
```

#### View Diff
```bash
# View latest pending proposal
z3ed agent diff

# View specific proposal
z3ed agent diff --proposal-id proposal-20251002T100000-1
```

#### Review in GUI
```bash
# Open YAZE
./build/bin/yaze.app/Contents/MacOS/yaze

# Navigate: Debug → Agent Proposals
# Select proposal → Review → Accept/Reject/Delete
```

#### Export API Schema
```bash
# Export all commands as YAML (for AI consumption)
z3ed agent describe --output docs/api/z3ed-resources.yaml

# Export as JSON
z3ed agent describe --format json --output api.json

# Export specific resource
z3ed agent describe --resource rom --format json
```

### Agent Testing (IT-02)

#### Run Natural Language Test
```bash
# Open editor and wait for window
z3ed agent test --prompt "Open Overworld editor"

# Complex workflow
z3ed agent test --prompt "Open Dungeon editor and verify it loads"

# Click specific button
z3ed agent test --prompt "Click Save button"
```

#### Test Introspection (IT-05) 🔜 PLANNED
```bash
# Get test status
z3ed agent test status --test-id grpc_click_12345678

# Poll until completion
z3ed agent test status --test-id grpc_click_12345678 --follow

# Get detailed results
z3ed agent test results --test-id grpc_click_12345678 --include-logs

# List all tests
z3ed agent test list --category grpc
```

#### Widget Discovery (IT-06) 🔜 PLANNED
```bash
# Discover all widgets
z3ed agent gui discover

# Filter by window
z3ed agent gui discover --window "Overworld"

# Get only buttons
z3ed agent gui discover --type button --format json
```

#### Test Recording (IT-07) 🔜 PLANNED
```bash
# Start recording
z3ed agent test record start --output tests/my_test.json

# Perform actions...

# Stop recording
z3ed agent test record stop --validate

# Replay test
z3ed agent test replay tests/my_test.json

# Run test suite
z3ed agent test suite run tests/smoke.yaml --ci-mode
```

### ROM Commands

```bash
# Display ROM metadata
z3ed rom info --rom=zelda3.sfc

# Validate ROM integrity
z3ed rom validate --rom=zelda3.sfc

# Compare two ROMs
z3ed rom diff --rom1=original.sfc --rom2=modified.sfc

# Generate golden checksums
z3ed rom generate-golden --rom=zelda3.sfc --output=golden.json
```

### Palette Commands

```bash
# Export palette
z3ed palette export sprites_aux1 4 soldier.col

# Import palette
z3ed palette import sprites_aux1 4 soldier_red.col

# List palettes
z3ed palette list --group sprites_aux1
```

### Overworld Commands

```bash
# Get tile at coordinates
z3ed overworld get-tile --map=0 --x=100 --y=50

# Set tile at coordinates
z3ed overworld set-tile --map=0 --x=100 --y=50 --tile-id=0x1234
```

### Dungeon Commands

```bash
# List dungeon rooms
z3ed dungeon list-rooms --dungeon=0

# Add object to room
z3ed dungeon add-object --dungeon=0 --room=5 --object=chest
```

---

## gRPC Testing with grpcurl

### Setup
```bash
# Install grpcurl
brew install grpcurl

# Set proto path
export PROTO_PATH="src/app/core/proto"
export PROTO_FILE="imgui_test_harness.proto"
```

### Core RPCs

#### Ping (Health Check)
```bash
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"message":"test"}' 127.0.0.1:50052 yaze.test.ImGuiTestHarness/Ping

# Response:
# {
#   "message": "Pong: test",
#   "timestampMs": "1696271234567",
#   "yazeVersion": "0.3.2"
# }
```

#### Click
```bash
# Click button
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"target":"button:Save","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Click menu item
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"target":"menuitem: Overworld Editor","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```

#### Type
```bash
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"target":"input:Search","text":"tile16","clear_first":true}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Type
```

#### Wait
```bash
# Wait for window to be visible
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"condition":"window_visible:Overworld","timeout_ms":5000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait

# Wait for element to be enabled
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"condition":"element_enabled:button:Save","timeout_ms":3000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait
```

#### Assert
```bash
# Assert window visible
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"condition":"visible:Overworld"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Assert

# Assert element enabled
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"condition":"enabled:button:Save"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Assert
```

#### Screenshot (Stub)
```bash
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"window_title":"Overworld","output_path":"/tmp/test.png","format":"PNG"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Screenshot

# Response: {"success":false,"message":"Screenshot not yet implemented"}
```

---

## E2E Testing

### Run Full Test Suite
```bash
# Run all E2E tests
./scripts/test_harness_e2e.sh

# Expected output:
# ✓ Ping (Health Check)
# ✓ Click (Open Overworld Editor)
# ✓ Wait (Overworld Editor Window)
# ✓ Assert (Overworld Editor Visible)
# ✓ Click (Open Dungeon Editor)
# Tests Passed: 5
```

### Manual Workflow Test
```bash
# 1. Start YAZE
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# 2. Create proposal
./build/bin/z3ed agent run --prompt "Test proposal" --rom=zelda3.sfc --sandbox

# 3. List proposals
./build/bin/z3ed agent list

# 4. View diff
./build/bin/z3ed agent diff

# 5. Review in GUI
./build/bin/yaze.app/Contents/MacOS/yaze
# Debug → Agent Proposals → Select → Accept

# 6. Cleanup
killall yaze
```

---

## Troubleshooting

### Port Already in Use
```bash
# Find process
lsof -i :50052

# Kill process
kill <PID>

# Or use different port
./yaze --enable_test_harness --test_harness_port=50053
```

### Connection Refused
```bash
# Check if server is running
lsof -i :50052

# Check firewall (macOS)
# System Preferences → Security & Privacy → Firewall

# Check logs
./yaze --enable_test_harness --log_level=debug
```

### Widget Not Found
```bash
# Problem: "Button 'XYZ' not found"

# Solutions:
# 1. Verify exact label (case-sensitive)
# 2. Wait for window to be visible first
grpcurl ... Wait '{"condition":"window_visible:WindowName"}'

# 3. Assert widget exists
grpcurl ... Assert '{"condition":"exists:button:XYZ"}'

# 4. Use widget discovery (IT-06, planned)
z3ed agent gui discover --window "WindowName"
```

### Build Errors
```bash
# Clean build
rm -rf build-grpc-test
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target yaze -j$(sysctl -n hw.ncpu)

# Check gRPC installation
cmake --build build-grpc-test --target help | grep -i grpc

# Verify proto generation
ls build-grpc-test/_deps/grpc-src/
```

---

## File Locations

### Core Files
```
src/app/core/
  ├── proto/imgui_test_harness.proto         # gRPC service definition
  ├── core/service/imgui_test_harness_service.{h,cc}      # RPC implementation
  └── test_manager.{h,cc}                    # Test execution management

src/cli/
  ├── handlers/agent.cc                      # CLI agent commands
  └── service/
      ├── proposal_registry.{h,cc}           # Proposal tracking
      ├── rom_sandbox_manager.{h,cc}         # ROM isolation
      └── resource_catalog.{h,cc}            # API schema

src/app/editor/system/
  └── proposal_drawer.{h,cc}                 # GUI review panel

docs/z3ed/
  ├── README.md                              # Overview & links
  ├── E6-z3ed-cli-design.md                  # Architecture
  ├── E6-z3ed-implementation-plan.md         # Roadmap
  ├── E6-z3ed-reference.md                   # Technical reference
  ├── IMPLEMENTATION_CONTINUATION.md         # Next steps
  └── IT-05-IMPLEMENTATION-GUIDE.md          # Introspection API guide
```

### Build Artifacts
```
build-grpc-test/
  ├── bin/
  │   ├── yaze.app/Contents/MacOS/yaze       # YAZE with test harness
  │   └── z3ed                               # CLI tool
  └── _deps/
      └── grpc-src/                          # gRPC source (auto-fetched)

/tmp/yaze/
  ├── proposals/                             # Proposal metadata
  │   └── proposal-<timestamp>-<seq>/
  │       ├── execution.log
  │       ├── diff.txt
  │       └── screenshots/
  └── sandboxes/                             # Isolated ROM copies
      └── <timestamp>-<seq>/
          └── zelda3.sfc
```

---

## Environment Variables

```bash
# Optional: Set default ROM path
export YAZE_ROM_PATH=~/roms/zelda3.sfc

# Optional: Set default test harness port
export YAZE_TEST_HARNESS_PORT=50052

# Optional: Enable verbose logging
export YAZE_LOG_LEVEL=debug

# Optional: Set proposal directory
export YAZE_PROPOSAL_DIR=/custom/path/proposals
```

---

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| macOS ARM64 | ✅ Production Ready | Fully tested |
| macOS Intel | ⚠️ Should Work | Not explicitly tested |
| Linux | ⚠️ Should Work | gRPC has excellent support |
| Windows | 🔬 Experimental | Build system ready, needs validation |

---

## Next Steps

**Current Phase**: Test Harness Enhancements (IT-05 to IT-09)

**Immediate Priority**: IT-05 (Test Introspection API)

**See**:
- [IMPLEMENTATION_CONTINUATION.md](IMPLEMENTATION_CONTINUATION.md) - Detailed roadmap
- [IT-05-IMPLEMENTATION-GUIDE.md](IT-05-IMPLEMENTATION-GUIDE.md) - Step-by-step guide

---

## Resources

- **GitHub**: https://github.com/scawful/yaze
- **Documentation**: `docs/z3ed/`
- **Slack/Discord**: [TBD]
- **Contributors**: @scawful, GitHub Copilot

---

**Last Updated**: October 2, 2025  
**Version**: z3ed v0.1.0-alpha  
**License**: Same as YAZE (see LICENSE)
