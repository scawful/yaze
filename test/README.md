# yaze Test Suite

This directory contains the comprehensive test suite for YAZE, organized for optimal AI agent testing and development workflow.

## Directory Structure

```
test/
├── unit/                    # Unit tests for individual components
│   ├── core/               # Core functionality tests
│   ├── rom/                # ROM handling tests
│   ├── gfx/                # Graphics system tests
│   └── zelda3/             # Zelda 3 specific tests
├── integration/            # Integration tests
│   ├── editor/             # Editor integration tests
│   ├── asar_integration_test.cc
│   ├── asar_rom_test.cc
│   └── dungeon_editor_test.cc
├── e2e/                    # End-to-end tests
│   ├── rom_dependent/      # ROM-dependent E2E tests
│   └── zscustomoverworld/  # ZSCustomOverworld upgrade tests
├── deprecated/             # Outdated tests (for cleanup)
│   └── emu/                # Deprecated emulator tests
├── mocks/                  # Mock objects for testing
├── assets/                 # Test assets and patches
└── yaze_test.cc           # Enhanced test runner
```

## Test Categories

### Unit Tests (`unit/`)
- **Core**: ASAR wrapper, hex utilities, core functionality
- **ROM**: ROM loading, saving, validation
- **Graphics**: SNES tiles, palettes, compression
- **Zelda3**: Message system, overworld, objects, sprites

### Integration Tests (`integration/`)
- **Editor**: Tile editor, dungeon editor integration
- **ASAR**: ASAR integration and ROM patching
- **Dungeon**: Dungeon editor system integration

### End-to-End Tests (`e2e/`)
- **ROM Dependent**: Complete ROM editing workflow validation
- **ZSCustomOverworld**: Version upgrade testing (vanilla → v2 → v3)

## Enhanced Test Runner

The `yaze_test` executable now supports comprehensive argument handling for AI agents:

### Usage Examples

```bash
# Run all tests
./yaze_test

# Run specific test categories
./yaze_test --unit --verbose
./yaze_test --integration
./yaze_test --e2e --rom-path my_rom.sfc
./yaze_test --zscustomoverworld --verbose

# Run specific test patterns
./yaze_test RomTest.*
./yaze_test *ZSCustomOverworld*

# Skip ROM-dependent tests
./yaze_test --skip-rom-tests

# Enable UI tests
./yaze_test --enable-ui-tests
```

### Test Modes

- `--unit`: Unit tests only
- `--integration`: Integration tests only
- `--e2e`: End-to-end tests only
- `--rom-dependent`: ROM-dependent tests only
- `--zscustomoverworld`: ZSCustomOverworld tests only
- `--core`: Core functionality tests
- `--graphics`: Graphics tests
- `--editor`: Editor tests
- `--deprecated`: Deprecated tests (for cleanup)

### Options

- `--rom-path PATH`: Specify ROM path for testing
- `--skip-rom-tests`: Skip tests requiring ROM files
- `--enable-ui-tests`: Enable UI tests (requires display)
- `--verbose`: Enable verbose output
- `--help`: Show help message

## E2E ROM Testing

The E2E ROM test suite (`e2e/rom_dependent/e2e_rom_test.cc`) provides comprehensive validation of the complete ROM editing workflow:

1. **Load vanilla ROM**
2. **Apply various edits** (overworld, dungeon, graphics, etc.)
3. **Save changes**
4. **Reload ROM and verify edits persist**
5. **Verify no data corruption occurred**

### Test Cases

- `BasicROMLoadSave`: Basic ROM loading and saving
- `OverworldEditWorkflow`: Complete overworld editing workflow
- `DungeonEditWorkflow`: Complete dungeon editing workflow
- `TransactionSystem`: Multi-edit transaction validation
- `CorruptionDetection`: ROM corruption detection
- `LargeScaleEditing`: Large-scale editing without corruption

## ZSCustomOverworld Upgrade Testing

The ZSCustomOverworld test suite (`e2e/zscustomoverworld/zscustomoverworld_upgrade_test.cc`) validates version upgrades:

### Supported Upgrades

- **Vanilla → v2**: Basic upgrade with main palettes
- **v2 → v3**: Advanced upgrade with expanded features
- **Vanilla → v3**: Direct upgrade to latest version

### Test Cases

- `VanillaBaseline`: Validate vanilla ROM baseline
- `VanillaToV2Upgrade`: Test vanilla to v2 upgrade
- `V2ToV3Upgrade`: Test v2 to v3 upgrade
- `VanillaToV3Upgrade`: Test direct vanilla to v3 upgrade
- `AddressValidation`: Validate version-specific addresses
- `SaveCompatibility`: Test save compatibility between versions
- `FeatureToggle`: Test feature enablement/disablement
- `DataIntegrity`: Test data integrity during upgrades

### Version-Specific Features

#### Vanilla
- Basic overworld functionality
- Standard message IDs, area graphics, palettes

#### v2
- Main palettes support
- Expanded message ID table

#### v3
- Area-specific background colors
- Subscreen overlays
- Animated GFX
- Custom tile GFX groups
- Mosaic effects

## Environment Variables

- `YAZE_TEST_ROM_PATH`: Path to test ROM file
- `YAZE_SKIP_ROM_TESTS`: Skip ROM-dependent tests
- `YAZE_ENABLE_UI_TESTS`: Enable UI tests
- `YAZE_VERBOSE_TESTS`: Enable verbose test output

## CI/CD Integration

Tests are automatically labeled for CI/CD:

- `unit`: Fast unit tests
- `integration`: Medium-speed integration tests
- `e2e`: Slow end-to-end tests
- `rom`: ROM-dependent tests
- `zscustomoverworld`: ZSCustomOverworld specific tests
- `core`: Core functionality tests
- `graphics`: Graphics tests
- `editor`: Editor tests
- `deprecated`: Deprecated tests

## Deprecated Tests

The `deprecated/` directory contains outdated tests that no longer pass after the large refactor:

- **EMU tests**: CPU, PPU, SPC700, APU tests that are no longer compatible
- These tests are kept for reference but should not be run in CI/CD

## Best Practices

1. **Use appropriate test categories** for new tests
2. **Add comprehensive E2E tests** for new features
3. **Test upgrade paths** for ZSCustomOverworld features
4. **Validate data integrity** in all ROM operations
5. **Use descriptive test names** for AI agent clarity
6. **Include verbose output** for debugging

## AI Agent Testing

The enhanced test runner is specifically designed for AI agent testing:

- **Clear argument structure** for easy automation
- **Comprehensive help system** for understanding capabilities
- **Verbose output** for debugging and validation
- **Flexible test filtering** for targeted testing
- **Environment variable support** for configuration
