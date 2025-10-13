# YAZE Modern Command Handler Architecture

This directory contains the modern command handler system that provides a consistent interface for both CLI and AI agent interactions with ROM data.

## Architecture Overview

The command handler system follows a clean, layered architecture:

```
┌─────────────────────────────────────────┐
│         CLI / Agent Interface           │
│  (cli.cc, agent.cc, simple-chat, etc)  │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│      Command Handler Base Class         │
│    (resources/command_handler.h)        │
│  - Argument parsing                     │
│  - ROM context management               │
│  - Output formatting                    │
│  - Error handling                       │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│     Concrete Command Handlers           │
│  (handlers/*/*)                         │
│  - SpriteListCommandHandler             │
│  - DungeonDescribeRoomCommandHandler    │
│  - OverworldFindTileCommandHandler      │
│  - PaletteGetColorsCommandHandler       │
│  - etc...                               │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│       Core YAZE Libraries               │
│  - zelda3/ (overworld, dungeon, sprite) │
│  - gfx/ (graphics, palette)             │
│  - app/editor/ (ROM operations)         │
└─────────────────────────────────────────┘
```

## Namespace Structure

All command handlers are organized under a simplified namespace:

```cpp
namespace yaze {
namespace cli {
namespace handlers {
  // All command handler classes live here
  class SpriteListCommandHandler : public resources::CommandHandler { ... };
  class DungeonDescribeRoomCommandHandler : public resources::CommandHandler { ... };
  // etc.
}
}
}
```

## Directory Organization

```
handlers/
├── README.md (this file)
├── commands.h                    // Legacy command function declarations
├── command_wrappers.cc           // Wrapper functions for backward compatibility
├── command_handlers.h            // Forward declarations and factory functions
├── command_handlers.cc           // Factory implementations
│
├── graphics/                     // Graphics-related commands
│   ├── sprite_commands.h/.cc    // Sprite listing and properties
│   ├── palette_commands.h/.cc   // Palette manipulation
│   ├── hex_commands.h/.cc       // Raw hex data access
│   └── gfx.cc                   // Legacy graphics commands
│
├── game/                         // Game data inspection
│   ├── dungeon_commands.h/.cc   // Dungeon room inspection
│   ├── overworld_commands.h/.cc // Overworld map inspection
│   ├── music_commands.h/.cc     // Music track information
│   ├── dialogue_commands.h/.cc  // Dialogue/message search
│   └── message_commands.h/.cc   // Message data access
│
├── tools/                        // Development tools
│   ├── resource_commands.h/.cc  // Resource label inspection
│   ├── gui_commands.h/.cc       // GUI automation
│   └── emulator_commands.h/.cc  // Emulator/debugger control
│
└── agent/                        // AI agent specific
    ├── general_commands.cc      // Agent command routing
    ├── test_commands.cc         // Test harness
    └── todo_commands.h/.cc      // Task management
```

## Creating a New Command Handler

### 1. Define the Handler Class

Create a header file (e.g., `new_feature_commands.h`):

```cpp
#ifndef YAZE_SRC_CLI_HANDLERS_NEW_FEATURE_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_NEW_FEATURE_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

class NewFeatureCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "new-feature"; }
  
  std::string GetDescription() const {
    return "Brief description of what this command does";
  }
  
  std::string GetUsage() const {
    return "new-feature --arg1 <value> [--optional-arg2 <value>]";
  }
  
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    // Validate required arguments
    return parser.RequireArgs({"arg1"});
  }
  
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_NEW_FEATURE_COMMANDS_H_
```

### 2. Implement the Handler

Create the implementation file (e.g., `new_feature_commands.cc`):

```cpp
#include "cli/handlers/new_feature_commands.h"

#include "absl/strings/str_format.h"

namespace yaze {
namespace cli {
namespace handlers {

absl::Status NewFeatureCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  
  // Parse arguments
  auto arg1 = parser.GetString("arg1").value();
  auto arg2 = parser.GetString("optional-arg2").value_or("default");
  
  // Begin output
  formatter.BeginObject("New Feature Result");
  formatter.AddField("input_arg", arg1);
  
  // Do work with ROM
  // ... use rom->read(), zelda3 classes, etc.
  
  // Add results to formatter
  formatter.AddField("result", "success");
  formatter.BeginArray("items");
  // ... add items
  formatter.EndArray();
  formatter.EndObject();
  
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
```

### 3. Register in Factory

Add to `command_handlers.cc`:

```cpp
#include "cli/handlers/new_feature_commands.h"

std::vector<std::unique_ptr<resources::CommandHandler>> CreateCliCommandHandlers() {
  // ... existing handlers ...
  handlers.push_back(std::make_unique<NewFeatureCommandHandler>());
  return handlers;
}
```

### 4. Add Forward Declaration

Add to `command_handlers.h`:

```cpp
// Forward declarations for command handler classes
class NewFeatureCommandHandler;
```

## Command Handler Base Class

The `resources::CommandHandler` base class provides:

### Lifecycle Methods

- `Run(args, rom_context)` - Main entry point that orchestrates the full command execution
- `ValidateArgs(parser)` - Override to validate command arguments
- `Execute(rom, parser, formatter)` - Override to implement command logic

### Helper Methods

- `GetUsage()` - Return usage string for help
- `GetName()` - Return command name
- `GetDescription()` - Return brief description
- `RequiresLabels()` - Return true if command needs ROM labels loaded
- `GetDefaultFormat()` - Return "json" or "text" for default output
- `GetOutputTitle()` - Return title for output object

## Argument Parsing

The `ArgumentParser` class handles common CLI patterns:

```cpp
// Get string argument
auto value = parser.GetString("arg_name").value_or("default");

// Get integer (supports hex with 0x prefix)
auto count = parser.GetInt("count").value_or(10);

// Get hex value
auto address = parser.GetHex("address").value();

// Check for flag
if (parser.HasFlag("verbose")) { ... }

// Get positional arguments
auto positional = parser.GetPositional();

// Require specific arguments
RETURN_IF_ERROR(parser.RequireArgs({"required1", "required2"}));
```

## Output Formatting

The `OutputFormatter` class provides consistent JSON/text output:

```cpp
// Begin object
formatter.BeginObject("Title");

// Add fields
formatter.AddField("string_field", "value");
formatter.AddField("int_field", 42);
formatter.AddField("bool_field", true);
formatter.AddHexField("address", 0x1234, 4);  // Width in digits

// Arrays
formatter.BeginArray("items");
for (const auto& item : items) {
  formatter.AddArrayItem(absl::StrFormat("Item %d", i));
}
formatter.EndArray();

// Nested objects
formatter.BeginObject("nested");
formatter.AddField("nested_field", "value");
formatter.EndObject();

// End object
formatter.EndObject();
```

## Integration with Public API

Command handlers are designed to work alongside the public C API defined in `incl/yaze.h` and `incl/zelda.h`. 

- Handlers use internal C++ classes from `zelda3/`
- Output structures align with C API data types where possible
- Future: C API bridge will expose commands to external applications

## Best Practices

1. **Keep handlers focused** - One command per handler class
2. **Use existing zelda3 classes** - Don't duplicate ROM parsing logic
3. **Validate inputs early** - Use `ValidateArgs()` to catch errors
4. **Provide good error messages** - Return descriptive `absl::Status` errors
5. **Support both JSON and text** - Format output using `OutputFormatter`
6. **Document parameters** - Include full usage string in `GetUsage()`
7. **Test with agents** - Commands should be AI-friendly
8. **Mark unused rom parameter** - Use `Rom* /*rom*/` if not needed

## Testing

Test commands using the CLI:

```bash
# Direct command execution
./build/bin/z3ed agent sprite-list --limit 10 --format json --rom zelda3.sfc

# Via simple-chat interface
./build/bin/z3ed agent simple-chat --rom zelda3.sfc
> sprite-list --limit 5

# In agent test suite
./build/bin/z3ed agent test-conversation --rom zelda3.sfc
```

## Future Enhancements

- [ ] C API bridge for external language bindings
- [ ] Command auto-discovery and registration
- [ ] Per-command help system
- [ ] Command aliases and shortcuts
- [ ] Batch command execution
- [ ] Command pipelines (output of one → input of another)
- [ ] Interactive command REPL improvements

