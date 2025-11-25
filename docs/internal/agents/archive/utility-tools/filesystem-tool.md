# FileSystemTool Documentation

## Overview

The FileSystemTool provides read-only filesystem operations for AI agents to explore the yaze codebase safely. It includes security features to prevent path traversal attacks and restricts access to the project directory.

## Available Tools

### 1. filesystem-list

List files and directories in a given path.

**Usage:**
```
filesystem-list --path <directory> [--recursive] [--format <json|text>]
```

**Parameters:**
- `--path`: Directory to list (required)
- `--recursive`: Include subdirectories (optional, default: false)
- `--format`: Output format (optional, default: json)

**Example:**
```json
{
  "tool_name": "filesystem-list",
  "args": {
    "path": "src/cli/service/agent",
    "recursive": "true",
    "format": "json"
  }
}
```

### 2. filesystem-read

Read the contents of a text file.

**Usage:**
```
filesystem-read --path <file> [--lines <count>] [--offset <start>] [--format <json|text>]
```

**Parameters:**
- `--path`: File to read (required)
- `--lines`: Maximum number of lines to read (optional, default: all)
- `--offset`: Starting line number (optional, default: 0)
- `--format`: Output format (optional, default: json)

**Example:**
```json
{
  "tool_name": "filesystem-read",
  "args": {
    "path": "src/cli/service/agent/tool_dispatcher.h",
    "lines": "50",
    "offset": "0",
    "format": "json"
  }
}
```

### 3. filesystem-exists

Check if a file or directory exists.

**Usage:**
```
filesystem-exists --path <file|directory> [--format <json|text>]
```

**Parameters:**
- `--path`: Path to check (required)
- `--format`: Output format (optional, default: json)

**Example:**
```json
{
  "tool_name": "filesystem-exists",
  "args": {
    "path": "docs/internal/agents",
    "format": "json"
  }
}
```

### 4. filesystem-info

Get detailed information about a file or directory.

**Usage:**
```
filesystem-info --path <file|directory> [--format <json|text>]
```

**Parameters:**
- `--path`: Path to get info for (required)
- `--format`: Output format (optional, default: json)

**Returns:**
- File/directory name
- Type (file, directory, symlink)
- Size (for files)
- Modification time
- Permissions
- Absolute path

**Example:**
```json
{
  "tool_name": "filesystem-info",
  "args": {
    "path": "CMakeLists.txt",
    "format": "json"
  }
}
```

## Security Features

### Path Traversal Protection

The FileSystemTool prevents path traversal attacks by:
1. Rejecting paths containing ".." sequences
2. Normalizing all paths to absolute paths
3. Verifying paths are within the project directory

### Project Directory Restriction

All filesystem operations are restricted to the yaze project directory. The tool automatically detects the project root by looking for:
- CMakeLists.txt and src/yaze.cc (primary markers)
- .git directory with src/cli and src/app subdirectories (fallback)

### Binary File Protection

The `filesystem-read` tool only reads text files. It determines if a file is text by:
1. Checking file extension against a whitelist of known text formats
2. Scanning the first 512 bytes for null bytes or non-printable characters

## Integration with ToolDispatcher

The FileSystemTool is integrated with the agent's ToolDispatcher system:

```cpp
// In tool_dispatcher.h
enum class ToolCallType {
  // ... other tools ...
  kFilesystemList,
  kFilesystemRead,
  kFilesystemExists,
  kFilesystemInfo,
};

// Tool preference settings
struct ToolPreferences {
  // ... other preferences ...
  bool filesystem = true;  // Enable/disable filesystem tools
};
```

## Implementation Details

### Base Class: FileSystemToolBase

Provides common functionality for all filesystem tools:
- `ValidatePath()`: Validates and normalizes paths with security checks
- `GetProjectRoot()`: Detects the yaze project root directory
- `IsPathInProject()`: Verifies a path is within project bounds
- `FormatFileSize()`: Human-readable file size formatting
- `FormatTimestamp()`: Human-readable timestamp formatting

### Tool Classes

Each tool inherits from FileSystemToolBase and implements:
- `GetName()`: Returns the tool name
- `GetDescription()`: Returns a brief description
- `GetUsage()`: Returns usage syntax
- `ValidateArgs()`: Validates required arguments
- `Execute()`: Performs the filesystem operation
- `RequiresLabels()`: Returns false (no ROM labels needed)

## Usage in AI Agents

AI agents can use these tools to:
1. **Explore project structure**: List directories to understand codebase organization
2. **Read source files**: Examine implementation details and patterns
3. **Check file existence**: Verify paths before operations
4. **Get file metadata**: Understand file sizes, types, and timestamps

Example workflow:
```python
# Check if a directory exists
response = tool_dispatcher.dispatch({
  "tool_name": "filesystem-exists",
  "args": {"path": "src/cli/service/agent/tools"}
})

# List contents if it exists
if response["exists"] == "true":
  response = tool_dispatcher.dispatch({
    "tool_name": "filesystem-list",
    "args": {"path": "src/cli/service/agent/tools"}
  })

  # Read each source file
  for entry in response["entries"]:
    if entry["type"] == "file" and entry["name"].endswith(".cc"):
      content = tool_dispatcher.dispatch({
        "tool_name": "filesystem-read",
        "args": {"path": f"src/cli/service/agent/tools/{entry['name']}"}
      })
```

## Testing

Unit tests are provided in `test/unit/filesystem_tool_test.cc`:
- Directory listing (normal and recursive)
- File reading (with and without line limits)
- File existence checks
- File/directory info retrieval
- Security validation (path traversal, binary files)

Run tests with:
```bash
./build/bin/yaze_test "*FileSystemTool*"
```

## Future Enhancements

Potential improvements for future versions:
1. **Pattern matching**: Support glob patterns in list operations
2. **File search**: Find files by name or content patterns
3. **Directory statistics**: Count files, calculate total size
4. **Change monitoring**: Track file modifications since last check
5. **Write operations**: Controlled write access for specific directories (with strict validation)