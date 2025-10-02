# Widget ID Refactoring - Next Actions

**Date**: October 2, 2025  
**Status**: Phase 1 Complete - Testing & Integration Phase  
**Previous Session**: [SESSION_SUMMARY_OCT2_NIGHT.md](SESSION_SUMMARY_OCT2_NIGHT.md)

## Quick Start - Next Session

### Option 1: Manual Testing (15 minutes) 🎯 RECOMMENDED FIRST

**Goal**: Verify widgets register correctly in running GUI

```bash
# 1. Launch YAZE
./build/bin/yaze.app/Contents/MacOS/yaze

# 2. Open a ROM
# File → Open ROM → assets/zelda3.sfc

# 3. Open Overworld Editor
# Click "Overworld" button in main window

# 4. Test toolset buttons
# Click through: Pan, DrawTile, Entrances, etc.
# Expected: All work normally, no crashes

# 5. Check console output
# Look for any errors or warnings
# Widget registrations happen silently
```

**Success Criteria**:
- ✅ GUI launches without crashes
- ✅ Overworld editor opens normally
- ✅ All toolset buttons clickable
- ✅ No error messages in console

---

### Option 2: Add Widget Discovery Command (30 minutes)

**Goal**: Create CLI command to list registered widgets

**File to Edit**: `src/cli/handlers/agent.cc`

**Add New Command**: `z3ed agent discover`

```cpp
// Add to agent.cc:
absl::Status HandleDiscoverCommand(const std::vector<std::string>& args) {
  // Parse --pattern flag (default "*")
  std::string pattern = "*";
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--pattern" && i + 1 < args.size()) {
      pattern = args[++i];
    }
  }
  
  // Get widget registry
  auto& registry = gui::WidgetIdRegistry::Instance();
  auto matches = registry.FindWidgets(pattern);
  
  if (matches.empty()) {
    std::cout << "No widgets found matching pattern: " << pattern << "\n";
    return absl::NotFoundError("No widgets found");
  }
  
  std::cout << "=== Registered Widgets ===\n\n";
  std::cout << "Pattern: " << pattern << "\n";
  std::cout << "Count: " << matches.size() << "\n\n";
  
  for (const auto& path : matches) {
    const auto* info = registry.GetWidgetInfo(path);
    if (info) {
      std::cout << path << "\n";
      std::cout << "  Type: " << info->type << "\n";
      std::cout << "  ImGui ID: " << info->imgui_id << "\n";
      if (!info->description.empty()) {
        std::cout << "  Description: " << info->description << "\n";
      }
      std::cout << "\n";
    }
  }
  
  return absl::OkStatus();
}

// Add routing in HandleAgentCommand:
if (subcommand == "discover") {
  return HandleDiscoverCommand(args);
}
```

**Test**:
```bash
# Rebuild
cmake --build build --target z3ed -j8

# Test discovery (will fail - widgets registered at runtime)
./build/bin/z3ed agent discover
# Note: This requires YAZE to be running with widgets registered
# We'll need a different approach - see Option 3
```

---

### Option 3: Widget Export at Shutdown (30 minutes) 🎯 BETTER APPROACH

**Goal**: Export widget catalog when YAZE exits

**File to Edit**: `src/app/editor/editor_manager.cc`

**Add Destructor or Shutdown Method**:

```cpp
// In editor_manager.cc destructor or Shutdown():
void EditorManager::Shutdown() {
  // Export widget catalog for z3ed agent
  auto& registry = gui::WidgetIdRegistry::Instance();
  std::string catalog_path = "/tmp/yaze_widgets.yaml";
  
  try {
    registry.ExportCatalogToFile(catalog_path, "yaml");
    std::cout << "Widget catalog exported to: " << catalog_path << "\n";
  } catch (const std::exception& e) {
    std::cerr << "Failed to export widget catalog: " << e.what() << "\n";
  }
}
```

**Test**:
```bash
# 1. Rebuild
cmake --build build --target yaze -j8

# 2. Launch YAZE
./build/bin/yaze.app/Contents/MacOS/yaze

# 3. Open Overworld editor
# (registers widgets)

# 4. Quit YAZE
# File → Quit or Cmd+Q

# 5. Check exported catalog
cat /tmp/yaze_widgets.yaml

# Expected output:
# widgets:
#   - path: "Overworld/Toolset/button:Pan"
#     type: button
#     imgui_id: 12345
#     context:
#       editor: Overworld
#       tab: Toolset
#     ...
```

---

### Option 4: Test Harness Integration (1-2 hours)

**Goal**: Enable test harness to click widgets by hierarchical ID

**Files to Edit**:
1. `src/app/core/imgui_test_harness_service.cc`
2. `src/app/core/proto/imgui_test_harness.proto` (optional - add DiscoverWidgets RPC)

**Implementation**:

```cpp
// In imgui_test_harness_service.cc, update Click RPC:
absl::Status ImGuiTestHarnessServiceImpl::Click(
    const ClickRequest* request, ClickResponse* response) {
  
  const std::string& target = request->target();
  
  // Try hierarchical widget ID first
  auto& registry = gui::WidgetIdRegistry::Instance();
  ImGuiID widget_id = registry.GetWidgetId(target);
  
  if (widget_id != 0) {
    // Found in registry - use ImGui ID directly
    std::string test_name = absl::StrFormat("DynamicClick_%s", target);
    
    auto* dynamic_test = ImGuiTest_CreateDynamicTest(
        test_manager_->GetEngine(), test_category_.c_str(), test_name.c_str());
    
    dynamic_test->GuiFunc = [widget_id](ImGuiTestContext* ctx) {
      ctx->ItemClick(widget_id);
    };
    
    ImGuiTest_RunTest(test_manager_->GetEngine(), dynamic_test);
    
    response->set_success(true);
    response->set_message(absl::StrFormat("Clicked widget: %s", target));
    return absl::OkStatus();
  }
  
  // Fallback to legacy string-based lookup
  // ... existing code ...
  
  // If not found, suggest alternatives
  auto matches = registry.FindWidgets("*" + target + "*");
  if (!matches.empty()) {
    std::string suggestions = absl::StrJoin(matches, ", ");
    return absl::NotFoundError(
        absl::StrFormat("Widget not found: %s. Did you mean: %s?",
                        target, suggestions));
  }
  
  return absl::NotFoundError(
      absl::StrFormat("Widget not found: %s", target));
}
```

**Test**:
```bash
# 1. Rebuild with gRPC
cmake --build build-grpc-test --target yaze -j8

# 2. Start test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# 3. Open Overworld editor in GUI
# (registers widgets)

# 4. Test hierarchical click
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"Overworld/Toolset/button:DrawTile","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Expected: Click succeeds, DrawTile mode activated
```

---

## Recommended Sequence

### Tonight (30 minutes)
1. ✅ **Option 1**: Manual testing - verify no crashes
2. 📋 **Option 3**: Add widget export at shutdown
3. 📋 Inspect exported YAML, verify 13 toolset widgets

### Tomorrow Morning (1-2 hours)
1. 📋 **Option 4**: Test harness integration
2. 📋 Test clicking widgets via hierarchical IDs
3. 📋 Update E2E test script with new IDs

### Tomorrow Afternoon (2-3 hours)
1. 📋 Complete Overworld editor (canvas, properties)
2. 📋 Add DiscoverWidgets RPC to proto
3. 📋 Document patterns and best practices

---

## Files to Modify Next

### High Priority
1. `src/app/editor/editor_manager.cc` - Add widget export at shutdown
2. `src/app/core/imgui_test_harness_service.cc` - Registry lookup in Click RPC

### Medium Priority
3. `src/app/core/proto/imgui_test_harness.proto` - Add DiscoverWidgets RPC
4. `src/app/editor/overworld/overworld_editor.cc` - Add canvas/properties widgets

### Low Priority
5. `scripts/test_harness_e2e.sh` - Update with hierarchical IDs
6. `docs/z3ed/IT-01-QUICKSTART.md` - Add widget ID examples

---

## Success Criteria

### Phase 1 (Complete) ✅
- [x] Widget registry in build
- [x] 13 toolset widgets registered
- [x] Clean build
- [x] Documentation updated

### Phase 2 (Current) 🔄
- [ ] Manual testing passes
- [ ] Widget export works
- [ ] Test harness can click by hierarchical ID
- [ ] At least 1 E2E test updated

### Phase 3 (Next) 📋
- [ ] Complete Overworld editor (30+ widgets)
- [ ] DiscoverWidgets RPC working
- [ ] All E2E tests use hierarchical IDs
- [ ] Performance validated (< 1ms overhead)

---

## Quick Commands

### Build
```bash
# Regular build
cmake --build build --target yaze -j8

# Test harness build
cmake --build build-grpc-test --target yaze -j8

# CLI build
cmake --build build --target z3ed -j8
```

### Test
```bash
# Manual test
./build/bin/yaze.app/Contents/MacOS/yaze

# Test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc
```

### Cleanup
```bash
# Kill running YAZE instances
killall yaze

# Clean build
rm -rf build/CMakeFiles build/bin
cmake --build build -j8
```

---

## References

**Progress Docs**:
- [WIDGET_ID_REFACTORING_PROGRESS.md](WIDGET_ID_REFACTORING_PROGRESS.md) - Detailed tracker
- [SESSION_SUMMARY_OCT2_NIGHT.md](SESSION_SUMMARY_OCT2_NIGHT.md) - Tonight's work

**Design Docs**:
- [IMGUI_ID_MANAGEMENT_REFACTORING.md](IMGUI_ID_MANAGEMENT_REFACTORING.md) - Complete plan
- [IT-01-QUICKSTART.md](IT-01-QUICKSTART.md) - Test harness guide

**Code References**:
- `src/app/gui/widget_id_registry.{h,cc}` - Registry implementation
- `src/app/editor/overworld/overworld_editor.cc` - Usage example
- `src/app/core/imgui_test_harness_service.cc` - Test harness

---

**Last Updated**: October 2, 2025, 11:30 PM  
**Next Action**: Option 1 (Manual Testing) or Option 3 (Widget Export)  
**Time Estimate**: 15-30 minutes
