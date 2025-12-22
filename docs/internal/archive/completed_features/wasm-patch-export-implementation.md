# WASM Patch Export Documentation

## Overview

The WASM patch export functionality allows users to export their ROM modifications as BPS or IPS patch files directly from the browser. This enables sharing modifications without distributing copyrighted ROM data.

## Features

### Supported Formats

#### BPS (Beat Patch Format)
- Modern patch format with advanced features
- Variable-length encoding for efficient storage
- Delta encoding for changed regions
- CRC32 checksums for validation
- No size limitations
- Better compression than IPS

#### IPS (International Patching System)
- Classic patch format with wide compatibility
- Simple record-based structure
- RLE encoding for repeated bytes
- Maximum file size of 16MB (24-bit addressing)
- Widely supported by emulators and patching tools

### API

```cpp
#include "app/platform/wasm/wasm_patch_export.h"

// Export as BPS patch
absl::Status status = WasmPatchExport::ExportBPS(
    original_rom_data,  // std::vector<uint8_t>
    modified_rom_data,  // std::vector<uint8_t>
    "my_hack.bps"      // filename
);

// Export as IPS patch
absl::Status status = WasmPatchExport::ExportIPS(
    original_rom_data,
    modified_rom_data,
    "my_hack.ips"
);

// Get preview of changes
PatchInfo info = WasmPatchExport::GetPatchPreview(
    original_rom_data,
    modified_rom_data
);
// info.changed_bytes - total bytes changed
// info.num_regions - number of distinct regions
// info.changed_regions - vector of (offset, length) pairs
```

## Implementation Details

### BPS Format Structure
```
Header:
  - "BPS1" magic (4 bytes)
  - Source size (variable-length)
  - Target size (variable-length)
  - Metadata size (variable-length, 0 for no metadata)

Patch Data:
  - Actions encoded as variable-length integers
  - SourceRead: Copy from source (action = (length-1) << 2)
  - TargetRead: Copy from patch (action = ((length-1) << 2) | 1)

Footer:
  - Source CRC32 (4 bytes, little-endian)
  - Target CRC32 (4 bytes, little-endian)
  - Patch CRC32 (4 bytes, little-endian)
```

### IPS Format Structure
```
Header:
  - "PATCH" (5 bytes)

Records (repeating):
  Normal Record:
    - Offset (3 bytes, big-endian)
    - Size (2 bytes, big-endian, non-zero)
    - Data (size bytes)

  RLE Record:
    - Offset (3 bytes, big-endian)
    - Size (2 bytes, always 0x0000)
    - Run length (2 bytes, big-endian)
    - Value (1 byte)

Footer:
  - "EOF" (3 bytes)
```

### Browser Integration

The patch files are downloaded using the HTML5 Blob API:

1. Patch data is generated in C++
2. Data is passed to JavaScript via EM_JS
3. JavaScript creates a Blob with the binary data
4. Object URL is created from the Blob
5. Hidden anchor element triggers download
6. Cleanup occurs after download starts

```javascript
// Simplified download flow
var blob = new Blob([patchData], { type: 'application/octet-stream' });
var url = URL.createObjectURL(blob);
var a = document.createElement('a');
a.href = url;
a.download = filename;
a.click();
URL.revokeObjectURL(url);
```

## Usage in Yaze Editor

### Menu Integration

Add to `MenuOrchestrator` or `RomFileManager`:

```cpp
if (ImGui::BeginMenu("File")) {
    if (ImGui::BeginMenu("Export", rom_->is_loaded())) {
        if (ImGui::MenuItem("Export BPS Patch...")) {
            ShowPatchExportDialog(PatchFormat::BPS);
        }
        if (ImGui::MenuItem("Export IPS Patch...")) {
            ShowPatchExportDialog(PatchFormat::IPS);
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenu();
}
```

### Tracking Original ROM State

To generate patches, the ROM class needs to track both original and modified states:

```cpp
class Rom {
    std::vector<uint8_t> original_data_;  // Preserve original
    std::vector<uint8_t> data_;          // Working copy

public:
    void LoadFromFile(const std::string& filename) {
        // Load data...
        original_data_ = data_;  // Save original state
    }

    const std::vector<uint8_t>& original_data() const {
        return original_data_;
    }
};
```

## Testing

### Unit Tests
```bash
# Run patch export tests
./build/bin/yaze_test --gtest_filter="*WasmPatchExport*"
```

### Manual Testing in Browser
1. Build for WASM: `emcc ... -s ENVIRONMENT=web`
2. Load a ROM in the web app
3. Make modifications
4. Use File → Export → Export BPS/IPS Patch
5. Verify patch downloads correctly
6. Test patch with external patching tool

## Limitations

### IPS Format
- Maximum ROM size: 16MB (0xFFFFFF bytes)
- No checksum validation
- Less efficient compression than BPS
- No metadata support

### BPS Format
- Requires more complex implementation
- Less tool support than IPS
- Larger patch size for small changes

### Browser Constraints
- Download triggered via user action only
- No direct filesystem access
- Patch must fit in browser memory
- Download folder determined by browser

## Error Handling

Common errors and solutions:

| Error | Cause | Solution |
|-------|-------|----------|
| Empty ROM data | No ROM loaded | Check `rom->is_loaded()` first |
| IPS size limit | ROM > 16MB | Use BPS format instead |
| No changes | Original = Modified | Show warning to user |
| Download failed | Browser restrictions | Ensure user-triggered action |

## Future Enhancements

Potential improvements:
- UPS (Universal Patching Standard) support
- Patch compression (zip/gzip)
- Batch patch export
- Patch preview/validation
- Incremental patch generation
- Patch metadata (author, description)
- Direct patch sharing via URL