# Drag & Drop ROM Loading for WASM

This document describes the drag & drop ROM loading feature for the WASM/web build of yaze.

## Overview

The drag & drop system allows users to drag ROM files (`.sfc`, `.smc`, or `.zip`) directly onto the web page to load them into the editor. This provides a seamless and intuitive way to open ROMs without using file dialogs.

## Features

- **Visual Feedback**: Full-screen overlay with animations when dragging files
- **File Validation**: Only accepts valid ROM file types (`.sfc`, `.smc`, `.zip`)
- **Progress Indication**: Shows loading progress for large files
- **Error Handling**: Clear error messages for invalid files
- **Responsive Design**: Works on desktop and tablet devices
- **Accessibility**: Supports keyboard navigation and screen readers

## Architecture

### Components

1. **C++ Backend** (`wasm_drop_handler.h/cc`)
   - Singleton pattern for global drop zone management
   - Callback system for ROM data handling
   - JavaScript interop via Emscripten's EM_JS
   - Integration with Rom::LoadFromData()

2. **JavaScript Handler** (`drop_zone.js`)
   - DOM event handling (dragenter, dragover, dragleave, drop)
   - File validation and reading
   - Progress tracking
   - Module integration

3. **CSS Styling** (`drop_zone.css`)
   - Full-screen overlay with glassmorphism effect
   - Smooth animations and transitions
   - Dark mode support
   - High contrast mode support

## Implementation

### C++ Integration

```cpp
#ifdef __EMSCRIPTEN__
#include "app/platform/wasm/wasm_drop_handler.h"

// In your initialization code:
auto& drop_handler = yaze::platform::WasmDropHandler::GetInstance();

drop_handler.Initialize(
  "",  // Use document body as drop zone
  [this](const std::string& filename, const std::vector<uint8_t>& data) {
    // Handle dropped ROM
    auto rom = std::make_unique<Rom>();
    auto status = rom->LoadFromData(data);
    if (status.ok()) {
      // Load into editor
      LoadRomIntoEditor(std::move(rom), filename);
    }
  },
  [](const std::string& error) {
    // Handle errors
    ShowErrorMessage(error);
  }
);
#endif
```

### HTML Integration

```html
<!DOCTYPE html>
<html>
<head>
  <!-- Include the drop zone styles -->
  <link rel="stylesheet" href="drop_zone.css">
</head>
<body>
  <!-- Your application canvas -->
  <canvas id="canvas"></canvas>

  <!-- Include the drop zone script -->
  <script src="drop_zone.js"></script>

  <!-- Your Emscripten module -->
  <script src="yaze.js"></script>
</body>
</html>
```

### JavaScript Customization

```javascript
// Optional: Customize the drop zone after Module is ready
Module.onRuntimeInitialized = function() {
  YazeDropZone.init({
    config: {
      validExtensions: ['sfc', 'smc', 'zip', 'sfc.gz'],
      maxFileSize: 8 * 1024 * 1024,  // 8MB
      messages: {
        dropHere: 'Drop A Link to the Past ROM',
        loading: 'Loading ROM...',
        supported: 'Supported: .sfc, .smc, .zip'
      }
    },
    callbacks: {
      onDrop: function(filename, data) {
        console.log('ROM dropped:', filename, data.length + ' bytes');
      },
      onError: function(error) {
        console.error('Drop error:', error);
      }
    }
  });
};
```

## User Experience

### Workflow

1. User opens yaze in a web browser
2. User drags a ROM file from their file manager
3. When the file enters the browser window:
   - Full-screen overlay appears with drop zone
   - Visual feedback indicates valid drop target
4. User drops the file:
   - Loading animation shows progress
   - File is validated and loaded
   - ROM opens in the editor
5. If there's an error:
   - Clear error message is displayed
   - User can try again

### Visual States

- **Idle**: No overlay visible
- **Drag Enter**: Semi-transparent overlay with dashed border
- **Drag Over**: Green glow effect, scaled animation
- **Loading**: Blue progress bar with file info
- **Error**: Red border with error message

## Build Configuration

The drag & drop feature is automatically included when building for WASM:

```bash
# Install Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Build yaze with WASM preset
cd /path/to/yaze
cmake --preset wasm-release
cmake --build build --target yaze

# Serve the files
python3 -m http.server 8000 -d build
# Open http://localhost:8000/yaze.html
```

## Browser Compatibility

| Browser | Version | Support |
|---------|---------|---------|
| Chrome | 90+ | ✅ Full |
| Firefox | 88+ | ✅ Full |
| Safari | 14+ | ✅ Full |
| Edge | 90+ | ✅ Full |
| Mobile Chrome | Latest | ⚠️ Limited (no drag & drop on mobile) |
| Mobile Safari | Latest | ⚠️ Limited (no drag & drop on mobile) |

## Performance Considerations

- Files are read into memory completely before processing
- Large files (>4MB) may take a few seconds to load
- Progress indication helps with user feedback
- Consider implementing streaming for very large files

## Security

- Files are processed entirely in the browser
- No data is sent to any server
- File validation prevents loading non-ROM files
- Cross-origin restrictions apply to drag & drop

## Testing

### Manual Testing

1. Test with valid ROM files (.sfc, .smc)
2. Test with invalid files (should show error)
3. Test with large files (>4MB)
4. Test drag enter/leave behavior
5. Test multiple file drops (should handle first only)
6. Test with compressed files (.zip)

### Automated Testing

```javascript
// Example test using Playwright or Puppeteer
test('drag and drop ROM loading', async ({ page }) => {
  await page.goto('http://localhost:8000/yaze.html');

  // Create a DataTransfer object with a file
  await page.evaluateHandle(async () => {
    const dt = new DataTransfer();
    const file = new File(['rom data'], 'zelda3.sfc', {
      type: 'application/octet-stream'
    });
    dt.items.add(file);

    // Dispatch drag events
    const dropEvent = new DragEvent('drop', {
      dataTransfer: dt,
      bubbles: true,
      cancelable: true
    });
    document.body.dispatchEvent(dropEvent);
  });

  // Verify ROM loaded
  await expect(page).toHaveText('ROM loaded successfully');
});
```

## Troubleshooting

### Common Issues

1. **Overlay doesn't appear**
   - Check browser console for JavaScript errors
   - Verify drop_zone.js is loaded
   - Ensure Module is initialized

2. **ROM doesn't load after drop**
   - Check if file is a valid ROM format
   - Verify file size is within limits
   - Check console for error messages

3. **Styles are missing**
   - Ensure drop_zone.css is included
   - Check for CSS conflicts with other stylesheets

4. **Performance issues**
   - Consider reducing file size limit
   - Implement chunked reading for large files
   - Use Web Workers for processing

## Future Enhancements

- [ ] Support for IPS/BPS patches via drag & drop
- [ ] Multiple file selection for batch operations
- [ ] Drag & drop for graphics/palette files
- [ ] Preview ROM information before loading
- [ ] Integration with cloud storage providers
- [ ] Touch device support via file input fallback

## References

- [MDN Drag and Drop API](https://developer.mozilla.org/en-US/docs/Web/API/HTML_Drag_and_Drop_API)
- [Emscripten EM_JS Documentation](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html#interacting-with-code-ccall-cwrap)
- [File API Specification](https://www.w3.org/TR/FileAPI/)