# YAZE Testing Quick Start Guide

This guide helps you get started with writing and running tests for YAZE immediately.

## Running Tests

### Build Tests

```bash
# From project root
cmake --build build --target yaze_test_stable
```

### Run All Tests

```bash
./build/bin/yaze_test_stable
```

### Run Specific Categories

```bash
# Unit tests only (fast, no ROM required)
./build/bin/yaze_test_stable --unit

# Integration tests (may require ROM)
./build/bin/yaze_test_stable --integration

# E2E GUI tests (requires display)
./build/bin/yaze_test_gui --e2e --show-gui
```

### Run Specific Tests

```bash
# By name pattern
./build/bin/yaze_test_stable "*Rom*"
./build/bin/yaze_test_stable "DungeonEditorTest.*"

# With verbose output
./build/bin/yaze_test_stable --verbose "*Asar*"
```

### Run with ROM

```bash
./build/bin/yaze_test_stable --rom-dependent --rom-path=/path/to/zelda3.sfc
```

## Writing Your First Test

### 1. Unit Test Example

**When:** Testing pure logic without external dependencies

**File:** `test/unit/my_component_test.cc`

```cpp
#include <gtest/gtest.h>
#include "app/my_component.h"

// Test naming: TEST(ComponentName, Method_Condition_ExpectedOutcome)
TEST(MyComponentTest, ProcessData_ValidInput_ReturnsSuccess) {
  // GIVEN - Setup test state
  MyComponent component;
  std::vector<uint8_t> input_data = {0x01, 0x02, 0x03};

  // WHEN - Execute the behavior
  auto result = component.ProcessData(input_data);

  // THEN - Verify expectations
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().size(), 3);
}

TEST(MyComponentTest, ProcessData_EmptyInput_ReturnsError) {
  MyComponent component;
  std::vector<uint8_t> empty_input;

  auto result = component.ProcessData(empty_input);

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}
```

### 2. Integration Test Example

**When:** Testing components that interact with ROM or other subsystems

**File:** `test/integration/my_editor_test.cc`

```cpp
#include <gtest/gtest.h>
#include "app/editor/my_editor.h"
#include "app/rom.h"
#include "test_utils.h"

class MyEditorIntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    auto status = rom_->LoadFromFile("zelda3.sfc");
    ASSERT_TRUE(status.ok()) << "Failed to load ROM: " << status.message();

    editor_ = std::make_unique<MyEditor>(rom_.get());
  }

  void TearDown() override {
    editor_.reset();
    rom_.reset();
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<MyEditor> editor_;
};

TEST_F(MyEditorIntegrationTest, LoadEditor_ValidRom_Succeeds) {
  editor_->Initialize();
  auto status = editor_->Load();

  ASSERT_TRUE(status.ok()) << "Load failed: " << status.message();
}

TEST_F(MyEditorIntegrationTest, EditData_ModifiesRom) {
  editor_->Initialize();
  editor_->Load();

  // Get original value
  uint8_t original = rom_->ReadByte(0x1234);

  // Make edit
  editor_->SetValue(0x1234, 0x42);
  editor_->Save();

  // Verify ROM was modified
  uint8_t modified = rom_->ReadByte(0x1234);
  EXPECT_EQ(modified, 0x42);
  EXPECT_NE(modified, original);
}
```

### 3. E2E Test Example

**When:** Testing full UI workflows with ImGui Test Engine

**File:** `test/e2e/my_editor_workflow_test.cc`

```cpp
#include "e2e/my_editor_workflow_test.h"
#include "app/controller.h"
#include "e2e/test_helpers.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "test_utils.h"

void E2ETest_MyEditorWorkflow(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Starting My Editor Workflow Test ===");

  // Load ROM
  yaze::test::e2e::LoadRomInTest(ctx, "zelda3.sfc");

  // Open editor
  yaze::test::e2e::OpenEditor(ctx, yaze::editor::EditorType::kMyEditor);
  ctx->Yield(2);  // Wait for editor to initialize

  // Perform UI actions
  ctx->SetRef("My Editor");
  ctx->ItemClick("Edit Button");
  ctx->Yield();

  // Verify outcome
  IM_CHECK(ctx->WindowInfo("Edit Dialog").Window != nullptr);

  ctx->LogInfo("=== Test Completed Successfully ===");
}
```

## Test File Organization

```
test/
├── unit/               # Fast, isolated tests
│   ├── core/
│   ├── gfx/
│   ├── zelda3/
│   └── ...
├── integration/        # Multi-component tests
│   ├── editor/
│   ├── zelda3/
│   └── ...
├── e2e/               # Full UI workflow tests
│   └── ...
├── benchmarks/        # Performance tests
│   └── ...
└── test_utils/        # Shared test utilities
    ├── test_utils.h
    └── ...
```

## Common Patterns

### Testing with Mock Data

```cpp
#include "test_utils/mock_rom_generator.h"

TEST(MyTest, WorksWithMockRom) {
  auto mock_rom_data = yaze::test::CreateDefaultMockRom();

  Rom rom;
  ASSERT_TRUE(rom.LoadFromBytes(mock_rom_data).ok());

  // Test with mock ROM...
}
```

### Testing Error Conditions

```cpp
TEST(MyTest, HandlesError_ReturnsStatus) {
  MyComponent component;

  auto status = component.MethodThatCanFail();

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), testing::HasSubstr("expected error text"));
}
```

### Parameterized Tests

```cpp
class TileConversionTest : public ::testing::TestWithParam<std::pair<int, int>> {
};

TEST_P(TileConversionTest, Converts) {
  auto [input, expected] = GetParam();
  EXPECT_EQ(ConvertTile(input), expected);
}

INSTANTIATE_TEST_SUITE_P(
    TileConversions, TileConversionTest,
    ::testing::Values(
        std::make_pair(0, 0),
        std::make_pair(1, 1),
        std::make_pair(255, 255)
    )
);
```

### Using Fixtures for Reusability

```cpp
// Define fixture once
class RomTestFixture : public ::testing::Test {
protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    rom_->LoadFromFile("zelda3.sfc");
  }

  Rom* rom() { return rom_.get(); }

private:
  std::unique_ptr<Rom> rom_;
};

// Reuse in multiple tests
TEST_F(RomTestFixture, Test1) {
  EXPECT_TRUE(rom()->is_loaded());
}

TEST_F(RomTestFixture, Test2) {
  uint8_t value = rom()->ReadByte(0x1234);
  EXPECT_NE(value, 0);
}
```

## CI Integration

### Add Test to CMakeLists.txt

```cmake
# test/CMakeLists.txt
set(STABLE_TEST_SOURCES
  # ... existing sources ...
  unit/my_component_test.cc        # Add your test here
  integration/my_editor_test.cc
)
```

### Tests Run Automatically On

- Every push to master/develop
- Every pull request
- Manual workflow dispatch

### View Test Results

- Check GitHub Actions "Test" workflow
- Download test result artifacts on failure
- Review test output in workflow logs

## Debugging Tests

### Run Single Test in Debugger

```bash
# Build with debug symbols
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DYAZE_BUILD_TESTS=ON
cmake --build build --target yaze_test_stable

# Run with debugger
lldb ./build/bin/yaze_test_stable -- --gtest_filter="MyTest.MyCase"
```

### Enable Verbose Logging

```cpp
TEST(MyTest, DebugIssue) {
  std::cout << "Debug: Value is " << value << std::endl;
  // ... test code ...
}
```

### E2E Test Visual Debugging

```bash
# Run in slow-motion with GUI visible
./build/bin/yaze_test_gui --e2e --show-gui --cinematic
```

## Best Practices Checklist

- [ ] Test name describes what is being tested and expected outcome
- [ ] Test is independent and doesn't depend on other tests
- [ ] Unit tests run in < 100ms
- [ ] Error cases are tested, not just happy path
- [ ] ROM state is verified after edits (not just UI)
- [ ] Test uses appropriate category (unit/integration/e2e)
- [ ] Test cleans up resources in TearDown()
- [ ] Assertions have descriptive failure messages

## Getting Help

- See full documentation in `docs/testing/`
- Review existing tests in `test/` for examples
- Ask in project discussions for testing questions
- Reference Google Test documentation: https://google.github.io/googletest/

## Common Issues

**"ROM file not found"**
```bash
# Ensure ROM is in correct location or specify path
./build/bin/yaze_test_stable --rom-path=/full/path/to/zelda3.sfc
```

**"Test timeout"**
```bash
# Increase timeout for slow tests
ctest --timeout 300  # 5 minutes
```

**"ImGui Test Engine not found"**
```bash
# Rebuild with test engine enabled
cmake -B build -DYAZE_BUILD_TESTS=ON
```

**"Flaky test failures"**
- Add more `ctx->Yield()` calls in e2e tests
- Avoid hardcoded timing assumptions
- Verify state before making assertions

## Next Steps

1. Write your first test using examples above
2. Run test locally to verify it works
3. Commit and push - CI will run automatically
4. Review test coverage in CI results
5. Iterate and add more tests

Happy testing!
