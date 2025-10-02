# z3ed Agent Test Command - Quick Reference

**Last Updated**: October 2, 2025  
**Feature**: IT-02 CLI Agent Test Command

---

## Command Syntax

```bash
z3ed agent test --prompt "<natural_language_prompt>" \
  [--host <hostname>] \
  [--port <port>] \
  [--timeout <seconds>]
```

---

## Supported Prompts

### 1. Open Editor
**Pattern**: "Open <Editor> editor"  
**Example**: `"Open Overworld editor"`  
**Actions**:
- Click button → Wait for window

```bash
z3ed agent test --prompt "Open Overworld editor"
z3ed agent test --prompt "Open Dungeon editor"
z3ed agent test --prompt "Open Sprite editor"
```

### 2. Open and Verify
**Pattern**: "Open <Editor> and verify it loads"  
**Example**: `"Open Dungeon editor and verify it loads"`  
**Actions**:
- Click button → Wait for window → Assert visible

```bash
z3ed agent test --prompt "Open Overworld editor and verify it loads"
z3ed agent test --prompt "Open Dungeon editor and verify it loads"
```

### 3. Click Button
**Pattern**: "Click <Button>"  
**Example**: `"Click Open ROM button"`  
**Actions**:
- Single click action

```bash
z3ed agent test --prompt "Click Open ROM button"
z3ed agent test --prompt "Click Save button"
z3ed agent test --prompt "Click Overworld"
```

### 4. Type Input
**Pattern**: "Type '<text>' in <input>"  
**Example**: `"Type 'zelda3.sfc' in filename input"`  
**Actions**:
- Click input → Type text (with clear_first)

```bash
z3ed agent test --prompt "Type 'zelda3.sfc' in filename input"
z3ed agent test --prompt "Type 'test' in search"
```

---

## Prerequisites

### 1. Build with gRPC
```bash
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target yaze -j$(sysctl -n hw.ncpu)
cmake --build build-grpc-test --target z3ed -j$(sysctl -n hw.ncpu)
```

### 2. Start YAZE Test Harness
```bash
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &
```

### 3. Verify Connection
```bash
# Check if server is running
lsof -i :50052

# Quick health check
grpcurl -plaintext -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"message":"test"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Ping
```

---

## Example Workflows

### Full Overworld Editor Test
```bash
# 1. Start test harness (if not running)
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# 2. Wait for startup
sleep 3

# 3. Run test
./build-grpc-test/bin/z3ed agent test \
  --prompt "Open Overworld editor and verify it loads"

# Expected output:
# === GUI Automation Test ===
# Prompt: Open Overworld editor and verify it loads
# Server: localhost:50052
#
# Generated workflow:
# Workflow: Open and verify Overworld Editor
#   1. Click(button:Overworld)
#   2. Wait(window_visible:Overworld Editor, 5000ms)
#   3. Assert(visible:Overworld Editor)
#
# ✓ Connected to test harness
#
# [1/3] Click(button:Overworld) ... ✓ (125ms)
# [2/3] Wait(window_visible:Overworld Editor, 5000ms) ... ✓ (1250ms)
# [3/3] Assert(visible:Overworld Editor) ... ✓ (50ms)
#
# ✅ Test passed in 1425ms
```

### Custom Server Configuration
```bash
# Connect to remote test harness
./build-grpc-test/bin/z3ed agent test \
  --prompt "Open Dungeon editor" \
  --host 192.168.1.100 \
  --port 50053 \
  --timeout 60
```

---

## Error Messages

### Connection Error
```
Failed to connect to test harness at localhost:50052
Make sure YAZE is running with:
  ./yaze --enable_test_harness --test_harness_port=50052 --rom_file=<rom>

Error: Connection refused
```

**Solution**: Start YAZE with test harness enabled

### Unsupported Prompt
```
Unable to parse prompt: "Do something complex"

Supported patterns:
  - Open <Editor> editor
  - Open <Editor> and verify it loads
  - Type '<text>' in <input>
  - Click <button>

Examples:
  - Open Overworld editor
  - Open Dungeon editor and verify it loads
  - Type 'zelda3.sfc' in filename input
  - Click Open ROM button
```

**Solution**: Use one of the supported prompt patterns

### Widget Not Found
```
[1/2] Click(button:NonExistent) ... ✗ FAILED
  Error: Button 'NonExistent' not found

Step 1 failed: Button 'NonExistent' not found
```

**Solution**: 
- Verify widget exists in YAZE
- Check spelling (case-sensitive)
- Use exact label from GUI

### Timeout Error
```
[2/2] Wait(window_visible:Slow Editor, 5000ms) ... ✗ FAILED
  Error: Condition not met after 5000 ms

Step 2 failed: Condition not met after 5000 ms
```

**Solution**:
- Increase timeout: `--timeout 10`
- Verify window actually opens
- Check for errors in YAZE

---

## Exit Codes

- `0` - Success (all steps passed)
- `1` - Failure (connection, parsing, or execution error)

---

## Troubleshooting

### Port Already in Use
```bash
# Kill existing instances
killall yaze

# Wait for cleanup
sleep 2

# Use different port
./yaze --enable_test_harness --test_harness_port=50053 ...
./z3ed agent test --port 50053 ...
```

### gRPC Not Available
```
GUI automation requires YAZE_WITH_GRPC=ON at build time.
Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON
```

**Solution**: Rebuild with gRPC support enabled

### Widget Names Unknown
```bash
# Manual exploration with grpcurl
grpcurl -plaintext -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"condition":"visible:Main Window"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Assert

# Try different widget names until you find the right one
```

---

## Advanced Usage

### Shell Script Integration
```bash
#!/bin/bash
set -e

# Start YAZE
./yaze --enable_test_harness --rom_file=zelda3.sfc &
YAZE_PID=$!
sleep 3

# Run tests
./z3ed agent test --prompt "Open Overworld editor" || exit 1
./z3ed agent test --prompt "Open Dungeon editor" || exit 1

# Cleanup
kill $YAZE_PID
```

### CI/CD Pipeline
```yaml
# .github/workflows/gui-tests.yml
- name: Start YAZE Test Harness
  run: |
    ./yaze --enable_test_harness --rom_file=zelda3.sfc &
    sleep 5

- name: Run GUI Tests
  run: |
    ./z3ed agent test --prompt "Open Overworld editor"
    ./z3ed agent test --prompt "Open Dungeon editor"
```

---

## Performance Characteristics

### Typical Timings
- **Click**: 50-200ms
- **Type**: 100-300ms
- **Wait**: 100-5000ms (depends on condition)
- **Assert**: 10-100ms

### Total Test Duration
- Simple click: ~100ms
- Open editor: ~1-2s
- Open + verify: ~1.5-2.5s
- Complex workflow: ~3-5s

---

## Extending Functionality

### Add New Pattern Type

1. **Add pattern matcher** (`test_workflow_generator.h`):
```cpp
bool MatchesYourPattern(const std::string& prompt, ...);
```

2. **Add workflow builder** (`test_workflow_generator.cc`):
```cpp
TestWorkflow BuildYourPatternWorkflow(...);
```

3. **Add to GenerateWorkflow()** (`test_workflow_generator.cc`):
```cpp
if (MatchesYourPattern(prompt, &params)) {
  return BuildYourPatternWorkflow(params);
}
```

### Add New Widget Type

Currently supported: `button:`, `input:`, `window:`

To add more, extend the target format in RPC calls.

---

## See Also

- **Full Documentation**: [IT-01-QUICKSTART.md](IT-01-QUICKSTART.md)
- **E2E Validation**: [E2E_VALIDATION_GUIDE.md](E2E_VALIDATION_GUIDE.md)
- **Implementation Details**: [IMPLEMENTATION_PROGRESS_OCT2.md](IMPLEMENTATION_PROGRESS_OCT2.md)
- **Architecture Overview**: [E6-z3ed-implementation-plan.md](E6-z3ed-implementation-plan.md)

---

**Last Updated**: October 2, 2025  
**Version**: IT-02 Complete  
**Status**: Ready for validation
