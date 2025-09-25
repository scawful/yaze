# Canvas Interface Migration Strategy

## Overview

This document outlines a strategy for migrating from the current complex Canvas class to a simplified version that uses pure functions while maintaining backward compatibility.

## Current Issues

1. **Duplicate Declarations**: Many methods are declared in both public and private sections
2. **Mixed Access Patterns**: Some private methods are accessed publicly through accessors
3. **Complex State Management**: Canvas state is scattered across many member variables
4. **Tight Coupling**: Drawing operations are tightly coupled to the class implementation

## Migration Strategy

### Phase 1: Create Pure Function Interface (COMPLETED)

- ✅ Created `canvas_interface.h` with pure function definitions
- ✅ Created `CanvasSimplified` class that uses pure functions
- ✅ Maintained identical public API for backward compatibility

### Phase 2: Gradual Migration

#### Step 1: Add Compatibility Layer
```cpp
// In existing Canvas class, add delegation methods
class Canvas {
public:
  // Existing methods delegate to pure functions
  void DrawBitmap(gfx::Bitmap& bitmap, int border_offset, float scale) override {
    canvas_ops::drawing::DrawBitmap(state_, bitmap, border_offset, scale);
  }
  
private:
  canvas_ops::CanvasState state_;  // Replace individual members
};
```

#### Step 2: Update Existing Code
```cpp
// Before
canvas.DrawBitmap(bitmap, 2, 1.0f);

// After (same API, different implementation)
canvas.DrawBitmap(bitmap, 2, 1.0f);  // No change needed!
```

#### Step 3: Gradual Replacement
```cpp
// Option 1: Replace Canvas with CanvasSimplified
using Canvas = CanvasSimplified;

// Option 2: Use typedef for gradual migration
typedef CanvasSimplified NewCanvas;
```

### Phase 3: Benefits of New Interface

#### 1. Pure Functions
```cpp
// Testable without Canvas instance
canvas_ops::CanvasState test_state;
canvas_ops::drawing::DrawBitmap(test_state, bitmap, 2, 1.0f);
```

#### 2. Better Separation of Concerns
```cpp
// Drawing operations
canvas_ops::drawing::DrawGrid(state, 64.0f);

// Interaction handling
canvas_ops::interaction::HandleMouseInput(state);

// State management
canvas_ops::canvas_management::SetCanvasSize(state, ImVec2(512, 512));
```

#### 3. Easier Testing
```cpp
TEST(CanvasDrawing, DrawBitmap) {
  canvas_ops::CanvasState state;
  gfx::Bitmap bitmap;
  
  canvas_ops::drawing::DrawBitmap(state, bitmap, 2, 1.0f);
  
  // Assert on state changes
  EXPECT_TRUE(state.refresh_graphics_);
}
```

## Implementation Plan

### Week 1: Core Infrastructure
- [ ] Implement pure function bodies in `canvas_interface.cc`
- [ ] Create `CanvasSimplified` implementation
- [ ] Add unit tests for pure functions

### Week 2: Compatibility Layer
- [ ] Add delegation methods to existing Canvas class
- [ ] Replace member variables with `CanvasState`
- [ ] Test backward compatibility

### Week 3: Gradual Migration
- [ ] Update one editor at a time (overworld, dungeon, etc.)
- [ ] Use `CanvasSimplified` in new code
- [ ] Monitor for any breaking changes

### Week 4: Cleanup
- [ ] Remove old Canvas implementation
- [ ] Rename `CanvasSimplified` to `Canvas`
- [ ] Update documentation

## Breaking Changes

### None Expected
The new interface maintains the exact same public API, so existing code should continue to work without changes.

### Optional Improvements
```cpp
// Old way (still works)
canvas.DrawBitmap(bitmap, 2, 1.0f);

// New way (optional, more explicit)
canvas_ops::drawing::DrawBitmap(canvas.GetState(), bitmap, 2, 1.0f);
```

## Testing Strategy

### Unit Tests
```cpp
TEST(CanvasOps, DrawBitmap) {
  canvas_ops::CanvasState state;
  gfx::Bitmap bitmap;
  
  canvas_ops::drawing::DrawBitmap(state, bitmap, 2, 1.0f);
  
  // Verify state changes
  EXPECT_TRUE(state.refresh_graphics_);
}
```

### Integration Tests
```cpp
TEST(CanvasIntegration, OverworldEditor) {
  CanvasSimplified canvas;
  OverworldEditor editor(&canvas);
  
  // Test that editor still works with new canvas
  editor.Draw();
}
```

## Performance Considerations

### Minimal Overhead
- Pure functions have minimal overhead compared to method calls
- `CanvasState` is passed by reference, no copying
- Same memory layout as original class

### Potential Optimizations
```cpp
// Batch operations
canvas_ops::CanvasState& state = canvas.GetState();
canvas_ops::drawing::DrawGrid(state, 64.0f);
canvas_ops::drawing::DrawOverlay(state);
// Single state update at end
```

## Rollback Plan

If issues arise:
1. Keep both implementations available
2. Use compile-time flag to switch between them
3. Revert to original Canvas if needed
4. Gradual rollback of individual editors

## Conclusion

This migration strategy provides:
- ✅ Backward compatibility
- ✅ Better testability
- ✅ Cleaner separation of concerns
- ✅ Easier maintenance
- ✅ No breaking changes

The pure function approach makes the Canvas system more modular and maintainable while preserving all existing functionality.
