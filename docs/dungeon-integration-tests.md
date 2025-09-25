# Dungeon Integration Tests

This document describes the comprehensive integration test suite for the Zelda 3 dungeon object rendering system.

## Overview

The integration tests provide comprehensive validation of the dungeon object rendering system using both real ROM data and mock implementations. The test suite ensures that all components work together correctly and can handle real-world scenarios.

## Test Files

### 1. `dungeon_object_renderer_integration_test.cc`

**Purpose**: Integration tests using the real Zelda 3 ROM (`build/bin/zelda3.sfc`)

**Key Features**:
- Tests with actual ROM data from Link to the Past
- Validates against disassembly information from `assets/asm/usdasm/`
- Tests specific rooms mentioned in disassembly (Ganon's room, sewer rooms, Agahnim's tower)
- Comprehensive object type validation
- Performance benchmarking
- Cache effectiveness testing
- Memory usage validation
- ROM integrity validation

**Test Categories**:
- Basic object rendering functionality
- Multi-palette rendering
- Real room object rendering with specific disassembly rooms
- Performance testing with various object counts
- Cache effectiveness validation
- Object type testing based on disassembly data
- ROM integrity and validation
- Palette validation against vanilla values
- Comprehensive room loading and validation

### 2. `dungeon_object_renderer_mock_test.cc`

**Purpose**: Integration tests using mock ROM data for testing without real ROMs

**Key Features**:
- Mock ROM implementation with realistic data structure
- Mock room generation with different room types
- Mock object creation and rendering
- Performance testing with mock data
- Error handling validation
- Cache functionality testing

**Mock Components**:
- `MockRom`: Complete mock ROM implementation
- `MockRoomGenerator`: Generates realistic test rooms
- Mock palette data
- Mock object data

### 3. `dungeon_editor_system_integration_test.cc`

**Purpose**: Integration tests for the complete dungeon editor system

**Key Features**:
- Room loading and management
- Object editor integration
- Sprite management (add, update, remove, move)
- Item management (keys, items, hidden items)
- Entrance/exit management and room connections
- Door management with key requirements
- Chest management with item storage
- Room properties and metadata
- Dungeon-wide settings
- Undo/redo functionality
- Validation and error handling
- Performance testing

## Test Data Sources

### Real ROM Data
- **ROM File**: `build/bin/zelda3.sfc` (Link to the Past ROM)
- **Disassembly**: `assets/asm/usdasm/` directory
- **Room Data**: Based on room pointers at `0x1F8000`
- **Object Types**: Validated against disassembly object definitions

### Disassembly Integration
The tests use information from the US disassembly to validate:
- Room IDs and their purposes (Ganon's room: 0x0000, Sewer rooms: 0x0002/0x0012, etc.)
- Object type IDs (chests: 0xF9/0xFA, walls: 0x10, floors: 0x20, etc.)
- Room data structure and pointers
- Palette and graphics data locations

## Running the Tests

### Prerequisites
1. Real ROM file at `build/bin/zelda3.sfc`
2. Compiled test suite
3. All dependencies available

### Command Line
```bash
# Run all dungeon integration tests
./build/bin/yaze_test --gtest_filter="DungeonObjectRendererIntegrationTest.*"

# Run mock tests only (no ROM required)
./build/bin/yaze_test --gtest_filter="DungeonObjectRendererMockTest.*"

# Run dungeon editor system tests
./build/bin/yaze_test --gtest_filter="DungeonEditorSystemIntegrationTest.*"

# Run specific test
./build/bin/yaze_test --gtest_filter="DungeonObjectRendererIntegrationTest.RealRoomObjectRendering"
```

### CI/CD Considerations
- Tests are skipped on Linux for automated builds (requires ROM file)
- Mock tests can run in any environment
- Performance tests have reasonable timeouts

## Test Coverage

### Object Rendering
- ✅ Basic object rendering
- ✅ Multi-palette rendering
- ✅ Different object types (walls, floors, chests, stairs, doors)
- ✅ Different object sizes and layers
- ✅ Real ROM room data rendering
- ✅ Performance benchmarking
- ✅ Memory usage validation
- ✅ Cache effectiveness

### Dungeon Editor System
- ✅ Room management (load, save, create, delete)
- ✅ Object editing (insert, delete, move, resize)
- ✅ Sprite management (CRUD operations)
- ✅ Item management (CRUD operations)
- ✅ Entrance/exit management
- ✅ Door management with key requirements
- ✅ Chest management with item storage
- ✅ Room properties and metadata
- ✅ Dungeon-wide settings
- ✅ Undo/redo functionality
- ✅ Validation and error handling

### Integration Features
- ✅ Real ROM data validation
- ✅ Disassembly data correlation
- ✅ Mock system for testing without ROMs
- ✅ Performance benchmarking
- ✅ Error handling and edge cases
- ✅ Memory management
- ✅ Cache optimization

## Performance Benchmarks

The tests include performance benchmarks with expected thresholds:

- **Object Rendering**: < 100ms for 50 objects
- **Large Object Sets**: < 500ms for 100 objects
- **System Operations**: < 5000ms for 200 operations
- **Cache Hit Rate**: > 50% for repeated operations

## Validation Points

### ROM Integrity
- ROM header validation
- Room data pointer validation
- Palette data validation
- Object data structure validation

### Object Data
- Object type validation against disassembly
- Object size and layer validation
- Object position validation
- Object collision detection

### System Integration
- Component interaction validation
- Data flow validation
- State management validation
- Error propagation validation

## Debugging and Troubleshooting

### Common Issues
1. **ROM Not Found**: Ensure `zelda3.sfc` is in `build/bin/`
2. **Private Member Access**: Some tests skip private member validation
3. **Performance Failures**: Check system resources and adjust thresholds
4. **Memory Issues**: Monitor memory usage during large object tests

### Debug Output
Tests include extensive debug output:
- Room loading statistics
- Object type discovery
- Performance metrics
- Cache statistics
- Memory usage reports

## Extending the Tests

### Adding New Tests
1. Follow existing test patterns
2. Use appropriate test data (real ROM or mock)
3. Include performance benchmarks where relevant
4. Add debug output for troubleshooting

### Adding New Mock Data
1. Extend `MockRom` class for new data types
2. Update `MockRoomGenerator` for new room types
3. Add validation in corresponding tests

### Adding New Validation
1. Use disassembly data for validation
2. Include both positive and negative test cases
3. Add performance benchmarks
4. Document expected behavior

## Maintenance

### Regular Updates
- Update test data when ROM structure changes
- Validate against new disassembly information
- Adjust performance thresholds based on system changes
- Update mock data to match real ROM structure

### Monitoring
- Track test execution time
- Monitor memory usage trends
- Watch for performance regressions
- Validate against new ROM versions

This integration test suite provides comprehensive validation of the dungeon object rendering system, ensuring reliability and performance across all components and use cases.
