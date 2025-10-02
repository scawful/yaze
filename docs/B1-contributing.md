# Contributing

Guidelines for contributing to the YAZE project.

## Development Setup

### Prerequisites
- **CMake 3.16+**: Modern build system
- **C++23 Compiler**: GCC 13+, Clang 16+, MSVC 2022 17.8+
- **Git**: Version control with submodules

### Quick Start
```bash
# Clone with submodules
git clone --recursive https://github.com/scawful/yaze.git
cd yaze

# Build with presets
cmake --preset dev
cmake --build --preset dev

# Run tests
ctest --preset stable
```

## Code Style

### C++ Standards
- **C++23**: Use modern language features
- **Google C++ Style**: Follow Google C++ style guide
- **Naming**: Use descriptive names, avoid abbreviations

### File Organization
```cpp
// Header guards
#pragma once

// Includes (system, third-party, local)
#include <vector>
#include "absl/status/status.h"
#include "app/core/asar_wrapper.h"

// Namespace usage
namespace yaze::editor {

class ExampleClass {
public:
    // Public interface
    absl::Status Initialize();
    
private:
    // Private implementation
    std::vector<uint8_t> data_;
};

}
```

### Error Handling
```cpp
// Use absl::Status for error handling
absl::Status LoadRom(const std::string& filename) {
    if (filename.empty()) {
        return absl::InvalidArgumentError("Filename cannot be empty");
    }
    
    // ... implementation
    
    return absl::OkStatus();
}

// Use absl::StatusOr for operations that return values
absl::StatusOr<std::vector<uint8_t>> ReadFile(const std::string& filename);
```

## Testing Requirements

### Test Categories
- **Stable Tests**: Fast, reliable, no external dependencies
- **ROM-Dependent Tests**: Require ROM files, skip in CI
- **Experimental Tests**: Complex, may be unstable

### Writing Tests
```cpp
// Stable test example
TEST(SnesTileTest, UnpackBppTile) {
    std::vector<uint8_t> tile_data = {0xAA, 0x55};
    auto result = UnpackBppTile(tile_data, 2);
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result->size(), 64);
}

// ROM-dependent test example
YAZE_ROM_TEST(AsarIntegration, RealRomPatching) {
    auto rom_data = TestRomManager::LoadTestRom();
    if (!rom_data.has_value()) {
        GTEST_SKIP() << "ROM file not available";
    }
    // ... test implementation
}
```

### Test Execution
```bash
# Run stable tests (required)
ctest --label-regex "STABLE"

# Run experimental tests (optional)
ctest --label-regex "EXPERIMENTAL"

# Run specific test
ctest -R "AsarWrapperTest"
```

## Pull Request Process

### Before Submitting
1. **Run tests**: Ensure all stable tests pass
2. **Check formatting**: Use clang-format
3. **Update documentation**: Update relevant docs if needed
4. **Test on multiple platforms**: Verify cross-platform compatibility

### Pull Request Template
```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
- [ ] Stable tests pass
- [ ] Manual testing completed
- [ ] Cross-platform testing (if applicable)

## Checklist
- [ ] Code follows project style guidelines
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] Tests added/updated
```

## Development Workflow

### Branch Strategy
- **main**: Stable, release-ready code
- **feature/**: New features and enhancements
- **fix/**: Bug fixes
- **docs/**: Documentation updates

### Commit Messages
```
type(scope): brief description

Detailed explanation of changes, including:
- What was changed
- Why it was changed
- Any breaking changes

Fixes #issue_number
```

### Types
- **feat**: New features
- **fix**: Bug fixes
- **docs**: Documentation changes
- **style**: Code style changes
- **refactor**: Code refactoring
- **test**: Test additions/changes
- **chore**: Build/tooling changes

## Architecture Guidelines

### Component Design
- **Single Responsibility**: Each class should have one clear purpose
- **Dependency Injection**: Use dependency injection for testability
- **Interface Segregation**: Keep interfaces focused and minimal

### Memory Management
- **RAII**: Use RAII for resource management
- **Smart Pointers**: Prefer unique_ptr and shared_ptr
- **Avoid Raw Pointers**: Use smart pointers or references

### Performance
- **Profile Before Optimizing**: Measure before optimizing
- **Use Modern C++**: Leverage C++23 features for performance
- **Avoid Premature Optimization**: Focus on correctness first

## Documentation

### Code Documentation
- **Doxygen Comments**: Use Doxygen format for public APIs
- **Inline Comments**: Explain complex logic
- **README Updates**: Update relevant README files

### API Documentation
```cpp
/**
 * @brief Applies an assembly patch to ROM data
 * @param patch_path Path to the assembly patch file
 * @param rom_data ROM data to patch (modified in place)
 * @param include_paths Optional include paths for assembly
 * @return Result containing success status and extracted symbols
 * @throws std::invalid_argument if patch_path is empty
 */
absl::StatusOr<AsarPatchResult> ApplyPatch(
    const std::string& patch_path,
    std::vector<uint8_t>& rom_data,
    const std::vector<std::string>& include_paths = {});
```

## Community Guidelines

### Communication
- **Be Respectful**: Treat all contributors with respect
- **Be Constructive**: Provide helpful feedback
- **Be Patient**: Remember that everyone is learning

### Getting Help
- **GitHub Issues**: Report bugs and request features
- **Discussions**: Ask questions and discuss ideas
- **Discord**: [Oracle of Secrets Discord](https://discord.gg/MBFkMTPEmk)

## Release Process

### Version Numbering
- **Semantic Versioning**: MAJOR.MINOR.PATCH
- **v0.3.1**: Current stable release
- **Pre-release**: v0.4.0-alpha, v0.4.0-beta

### Release Checklist
- [ ] All stable tests pass
- [ ] Documentation updated
- [ ] Changelog updated
- [ ] Cross-platform builds verified
- [ ] Release notes prepared
