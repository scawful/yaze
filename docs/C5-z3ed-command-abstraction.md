# z3ed Command Abstraction Layer Guide

**Created**: October 11, 2025  
**Status**: Implementation Complete

## Overview

This guide documents the new command abstraction layer for z3ed CLI commands. The abstraction layer eliminates ~500+ lines of duplicated code across tool commands and provides a consistent, maintainable architecture for future command development.

## Problem Statement

### Before Abstraction

The original `tool_commands.cc` (1549 lines) had severe code duplication:

1. **ROM Loading**: Every command had 20-30 lines of identical ROM loading logic
2. **Argument Parsing**: Each command manually parsed `--format`, `--rom`, `--type`, etc.
3. **Output Formatting**: JSON vs text formatting was duplicated across every command
4. **Label Initialization**: Resource label loading was repeated in every handler
5. **Error Handling**: Inconsistent error messages and validation patterns

### Code Duplication Example

```cpp
// Repeated in EVERY command (30+ times):
Rom rom_storage;
Rom* rom = nullptr;
if (rom_context != nullptr && rom_context->is_loaded()) {
  rom = rom_context;
} else {
  auto rom_or = LoadRomFromFlag();
  if (!rom_or.ok()) {
    return rom_or.status();
  }
  rom_storage = std::move(rom_or.value());
  rom = &rom_storage;
}

// Initialize labels (repeated in every command that needs labels)
if (rom->resource_label()) {
  if (!rom->resource_label()->labels_loaded_) {
    core::YazeProject project;
    project.use_embedded_labels = true;
    auto labels_status = project.InitializeEmbeddedLabels();
    // ... more boilerplate ...
  }
}

// Manual argument parsing (repeated everywhere)
std::string format = "json";
for (size_t i = 0; i < arg_vec.size(); ++i) {
  const std::string& token = arg_vec[i];
  if (token == "--format") {
    if (i + 1 >= arg_vec.size()) {
      return absl::InvalidArgumentError("--format requires a value.");
    }
    format = arg_vec[++i];
  } else if (absl::StartsWith(token, "--format=")) {
    format = token.substr(9);
  }
}

// Manual output formatting (repeated everywhere)
if (format == "json") {
  std::cout << "{\n";
  std::cout << "  \"field\": \"value\",\n";
  std::cout << "}\n";
} else {
  std::cout << "Field: value\n";
}
```

## Solution Architecture

### Three-Layer Abstraction

1. **CommandContext** - ROM loading, context management
2. **ArgumentParser** - Unified argument parsing
3. **OutputFormatter** - Consistent output formatting
4. **CommandHandler** (Optional) - Base class for structured commands

### File Structure

```
src/cli/service/resources/
├── command_context.h          # Context management
├── command_context.cc
├── command_handler.h          # Base handler class
├── command_handler.cc
└── (existing files...)

src/cli/handlers/agent/
├── tool_commands.cc           # Original (to be refactored)
├── tool_commands_refactored.cc # Example refactored commands
└── (other handlers...)
```

## Core Components

### 1. CommandContext

Encapsulates ROM loading and common context:

```cpp
// Create context
CommandContext::Config config;
config.external_rom_context = rom_context;  // Optional: use existing ROM
config.rom_path = "/path/to/rom.sfc";       // Optional: override ROM path
config.use_mock_rom = false;                // Optional: use mock for testing
config.format = "json";

CommandContext context(config);

// Get ROM (auto-loads if needed)
ASSIGN_OR_RETURN(Rom* rom, context.GetRom());

// Ensure labels loaded
RETURN_IF_ERROR(context.EnsureLabelsLoaded(rom));
```

**Benefits**:
- Single location for ROM loading logic
- Automatic error handling
- Mock ROM support for testing
- Label management abstraction

### 2. ArgumentParser

Unified argument parsing with type safety:

```cpp
ArgumentParser parser(arg_vec);

// String arguments
auto type = parser.GetString("type");        // Returns std::optional<string>
auto format = parser.GetString("format").value_or("json");

// Integer arguments (supports hex with 0x prefix)
ASSIGN_OR_RETURN(int room_id, parser.GetInt("room"));

// Hex-only arguments
ASSIGN_OR_RETURN(int tile_id, parser.GetHex("tile"));

// Flags
if (parser.HasFlag("verbose")) {
  // ...
}

// Validation
RETURN_IF_ERROR(parser.RequireArgs({"type", "query"}));
```

**Benefits**:
- Consistent argument parsing across all commands
- Type-safe with proper error handling
- Supports both `--arg=value` and `--arg value` forms
- Built-in hex parsing for ROM addresses

### 3. OutputFormatter

Consistent JSON/text output:

```cpp
ASSIGN_OR_RETURN(auto formatter, OutputFormatter::FromString("json"));

formatter.BeginObject("Room Information");
formatter.AddField("room_id", "0x12");
formatter.AddHexField("address", 0x1234, 4);  // Formats as "0x1234"
formatter.AddField("sprite_count", 5);

formatter.BeginArray("sprites");
formatter.AddArrayItem("Sprite 1");
formatter.AddArrayItem("Sprite 2");
formatter.EndArray();

formatter.EndObject();
formatter.Print();
```

**Output (JSON)**:
```json
{
  "room_id": "0x12",
  "address": "0x1234",
  "sprite_count": 5,
  "sprites": [
    "Sprite 1",
    "Sprite 2"
  ]
}
```

**Output (Text)**:
```
=== Room Information ===
  room_id              : 0x12
  address              : 0x1234
  sprite_count         : 5
  sprites:
    - Sprite 1
    - Sprite 2
```

**Benefits**:
- No manual JSON escaping
- Consistent formatting rules
- Easy to switch between JSON and text
- Proper indentation handling

### 4. CommandHandler (Optional Base Class)

For more complex commands, use the base class pattern:

```cpp
class MyCommandHandler : public CommandHandler {
 protected:
  std::string GetUsage() const override {
    return "agent my-command --required <value> [--format <json|text>]";
  }
  
  absl::Status ValidateArgs(const ArgumentParser& parser) override {
    return parser.RequireArgs({"required"});
  }
  
  absl::Status Execute(Rom* rom, const ArgumentParser& parser,
                      OutputFormatter& formatter) override {
    auto value = parser.GetString("required").value();
    
    // Business logic here
    formatter.AddField("result", value);
    
    return absl::OkStatus();
  }
  
  bool RequiresLabels() const override { return true; }
};

// Usage:
absl::Status HandleMyCommand(const std::vector<std::string>& args, Rom* rom) {
  MyCommandHandler handler;
  return handler.Run(args, rom);
}
```

**Benefits**:
- Enforces consistent structure
- Automatic context setup and teardown
- Built-in error handling
- Easy to test individual components

## Migration Guide

### Step-by-Step Refactoring

#### Before (80 lines):

```cpp
absl::Status HandleResourceListCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
  std::string type;
  std::string format = "table";

  // Manual argument parsing (20 lines)
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--type") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--type requires a value.");
      }
      type = arg_vec[++i];
    } else if (absl::StartsWith(token, "--type=")) {
      type = token.substr(7);
    }
    // ... repeat for --format ...
  }

  if (type.empty()) {
    return absl::InvalidArgumentError("Usage: ...");
  }

  // ROM loading (30 lines)
  Rom rom_storage;
  Rom* rom = nullptr;
  if (rom_context != nullptr && rom_context->is_loaded()) {
    rom = rom_context;
  } else {
    auto rom_or = LoadRomFromFlag();
    if (!rom_or.ok()) {
      return rom_or.status();
    }
    rom_storage = std::move(rom_or.value());
    rom = &rom_storage;
  }

  // Label initialization (15 lines)
  if (rom->resource_label()) {
    if (!rom->resource_label()->labels_loaded_) {
      core::YazeProject project;
      project.use_embedded_labels = true;
      auto labels_status = project.InitializeEmbeddedLabels();
      if (labels_status.ok()) {
        rom->resource_label()->labels_ = project.resource_labels;
        rom->resource_label()->labels_loaded_ = true;
      }
    }
  }

  // Business logic
  ResourceContextBuilder context_builder(rom);
  auto labels_or = context_builder.GetLabels(type);
  if (!labels_or.ok()) {
    return labels_or.status();
  }
  auto labels = std::move(labels_or.value());

  // Manual output formatting (15 lines)
  if (format == "json") {
    std::cout << "{\n";
    for (const auto& [key, value] : labels) {
      std::cout << "  \"" << key << "\": \"" << value << "\",\n";
    }
    std::cout << "}\n";
  } else {
    for (const auto& [key, value] : labels) {
      std::cout << key << ": " << value << "\n";
    }
  }

  return absl::OkStatus();
}
```

#### After (30 lines):

```cpp
absl::Status HandleResourceListCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
  
  // Parse arguments
  ArgumentParser parser(arg_vec);
  auto type = parser.GetString("type");
  auto format_str = parser.GetString("format").value_or("table");
  
  if (!type.has_value()) {
    return absl::InvalidArgumentError(
        "Usage: agent resource-list --type <type> [--format <table|json>]");
  }
  
  // Create formatter
  ASSIGN_OR_RETURN(auto formatter, OutputFormatter::FromString(format_str));
  
  // Setup context
  CommandContext::Config config;
  config.external_rom_context = rom_context;
  CommandContext context(config);
  
  // Get ROM and labels
  ASSIGN_OR_RETURN(Rom* rom, context.GetRom());
  RETURN_IF_ERROR(context.EnsureLabelsLoaded(rom));
  
  // Execute business logic
  ResourceContextBuilder builder(rom);
  ASSIGN_OR_RETURN(auto labels, builder.GetLabels(*type));
  
  // Format output
  formatter.BeginObject("Labels");
  for (const auto& [key, value] : labels) {
    formatter.AddField(key, value);
  }
  formatter.EndObject();
  formatter.Print();
  
  return absl::OkStatus();
}
```

**Savings**: 50+ lines eliminated, clearer intent, easier to maintain

### Commands to Refactor

Priority order for refactoring (based on duplication level):

1. ✅ **High Priority** (Heavy duplication):
   - `HandleResourceListCommand` - Example provided ✓
   - `HandleResourceSearchCommand` - Example provided ✓
   - `HandleDungeonDescribeRoomCommand` - 80 lines → ~35 lines
   - `HandleOverworldDescribeMapCommand` - 100 lines → ~40 lines
   - `HandleOverworldListWarpsCommand` - 120 lines → ~45 lines

2. **Medium Priority** (Moderate duplication):
   - `HandleDungeonListSpritesCommand`
   - `HandleOverworldFindTileCommand`
   - `HandleOverworldListSpritesCommand`
   - `HandleOverworldGetEntranceCommand`
   - `HandleOverworldTileStatsCommand`

3. **Low Priority** (Simple commands, less duplication):
   - `HandleMessageListCommand` (delegates to message handler)
   - `HandleMessageReadCommand` (delegates to message handler)
   - `HandleMessageSearchCommand` (delegates to message handler)

### Estimated Impact

| Metric | Before | After | Savings |
|--------|--------|-------|---------|
| Lines of code (tool_commands.cc) | 1549 | ~800 | **48%** |
| Duplicated ROM loading | ~600 lines | 0 | **600 lines** |
| Duplicated arg parsing | ~400 lines | 0 | **400 lines** |
| Duplicated formatting | ~300 lines | 0 | **300 lines** |
| **Total Duplication Removed** | | | **~1300 lines** |

## Testing Strategy

### Unit Testing

```cpp
TEST(CommandContextTest, LoadsRomFromConfig) {
  CommandContext::Config config;
  config.rom_path = "test.sfc";
  CommandContext context(config);
  
  auto rom_or = context.GetRom();
  ASSERT_OK(rom_or);
  EXPECT_TRUE(rom_or.value()->is_loaded());
}

TEST(ArgumentParserTest, ParsesStringArguments) {
  std::vector<std::string> args = {"--type=dungeon", "--format", "json"};
  ArgumentParser parser(args);
  
  EXPECT_EQ(parser.GetString("type").value(), "dungeon");
  EXPECT_EQ(parser.GetString("format").value(), "json");
}

TEST(OutputFormatterTest, GeneratesValidJson) {
  auto formatter = OutputFormatter::FromString("json").value();
  formatter.BeginObject("Test");
  formatter.AddField("key", "value");
  formatter.EndObject();
  
  std::string output = formatter.GetOutput();
  EXPECT_THAT(output, HasSubstr("\"key\": \"value\""));
}
```

### Integration Testing

```cpp
TEST(ResourceListCommandTest, ListsDungeons) {
  std::vector<std::string> args = {"--type=dungeon", "--format=json"};
  Rom rom;
  rom.LoadFromFile("test.sfc");
  
  auto status = HandleResourceListCommand(args, &rom);
  EXPECT_OK(status);
}
```

## Benefits Summary

### For Developers

1. **Less Code to Write**: New commands take 30-40 lines instead of 80-120
2. **Consistent Patterns**: All commands follow the same structure
3. **Better Error Handling**: Standardized error messages and validation
4. **Easier Testing**: Each component can be tested independently
5. **Self-Documenting**: Clear separation of concerns

### For Maintainability

1. **Single Source of Truth**: ROM loading logic in one place
2. **Easy to Update**: Change all commands by updating one class
3. **Consistent Behavior**: All commands handle errors the same way
4. **Reduced Bugs**: Less duplication = fewer places for bugs

### For AI Integration

1. **Predictable Structure**: AI can generate commands using templates
2. **Type Safety**: ArgumentParser prevents common errors
3. **Consistent Output**: AI can reliably parse JSON responses
4. **Easy to Extend**: New tool types follow existing patterns

## Next Steps

### Immediate (Current PR)

1. ✅ Create abstraction layer (CommandContext, ArgumentParser, OutputFormatter)
2. ✅ Add CommandHandler base class
3. ✅ Provide refactored examples
4. ✅ Update build system
5. ✅ Document architecture

### Phase 2 (Next PR)

1. Refactor high-priority commands (5 commands)
2. Add comprehensive unit tests
3. Update AI tool dispatcher to use new patterns
4. Create command generator templates for AI

### Phase 3 (Future)

1. Refactor remaining commands
2. Remove old helper functions
3. Add performance benchmarks
4. Create VS Code snippets for command development

## Migration Checklist

For each command being refactored:

- [ ] Replace manual argument parsing with ArgumentParser
- [ ] Replace ROM loading with CommandContext
- [ ] Replace label initialization with context.EnsureLabelsLoaded()
- [ ] Replace manual formatting with OutputFormatter
- [ ] Update error messages to use GetUsage()
- [ ] Add unit tests for the command
- [ ] Update documentation
- [ ] Test with both JSON and text output
- [ ] Test with missing/invalid arguments
- [ ] Test with mock ROM

## References

- Implementation: `src/cli/service/resources/command_context.{h,cc}`
- Examples: `src/cli/handlers/agent/tool_commands_refactored.cc`
- Base class: `src/cli/service/resources/command_handler.{h,cc}`
- Build config: `src/cli/agent.cmake`

## Questions & Answers

**Q: Should I refactor all commands at once?**  
A: No. Refactor in phases to minimize risk. Start with 2-3 commands as proof of concept.

**Q: What if my command needs custom argument handling?**  
A: ArgumentParser is flexible. You can still access raw args or add custom parsing logic.

**Q: Can I use both old and new patterns temporarily?**  
A: Yes. The new abstraction layer works alongside existing code. Migrate gradually.

**Q: Will this affect AI tool calling?**  
A: No breaking changes. The command interfaces remain the same. Internal implementation improves.

**Q: How do I test commands with the new abstractions?**  
A: Use CommandContext with mock ROM, or pass external rom_context in tests.

---

**Last Updated**: October 11, 2025  
**Author**: AI Assistant  
**Review Status**: Ready for Implementation

