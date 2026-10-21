# Project-specific overworld graphics previews

Use a graphics preview manifest when a ROM hack uploads raw SNES background graphics that are not represented by the normal overworld graphics groups. The manifest affects editor rendering only. It does not patch the ROM, modify graphics sheets, or change the hack's runtime DMA code.

## Enable a preview

Add the manifest path to a native `.yaze` project:

```ini
[files]
overworld_graphics_preview=overworld-preview.json
```

Open that project in Yaze. Opening the ROM alone does not enable the manifest. Paths inside the manifest resolve relative to the manifest's directory; the project stores its manifest path relative to the project file when possible.

The following example describes Oracle's Dragon Ship transfers. Put the manifest alongside an `Overworld/gfx` directory containing the two assets, or adjust the asset paths to your project layout:

```json
{
  "schema_version": 1,
  "areas": [
    {
      "parent_map": 48,
      "bg_character_base_word": 8192,
      "transfers": [
        {
          "path": "Overworld/gfx/boat.bin",
          "format": "snes4bpp",
          "byte_count": 8192,
          "vram_word_address": 11264
        },
        {
          "path": "Overworld/gfx/boat2.bin",
          "format": "snes4bpp",
          "byte_count": 8192,
          "vram_word_address": 20480
        }
      ]
    }
  ]
}
```

JSON numbers are decimal: `48` is map `0x30`, `8192` is word address `0x2000`, `11264` is `0x2C00`, and `20480` is `0x5000`. VRAM addresses are **word addresses**, not byte offsets or graphics-sheet IDs. Each raw 4bpp tile occupies 32 bytes. With character base `0x2000`, these transfers replace background tile IDs `0xC0–0x1BF` and `0x300–0x3FF`.

## Map and editing behavior

The configured `parent_map` and its child screens use the same preview graphics. This follows the ROM's parent relationships for Small, Large, Wide, and Tall areas. Changing `parent_map` changes the association; it does not move or resize a map.

The map canvas, Tile16 source graphics and blockset, and destination-aware brush previews use the map's private decoded graphics. Neighboring areas retain their own graphics. Preview data stays out of the shared ordinary-sheet cache and remains in place through map refreshes after edits.

Remove the project setting, or open an ordinary project, to restore the normal graphics. After modifying a manifest or asset in place, use **Reload Project Graphics** at the top of the Map Properties panel, on native and Lyra maps alike; this feature does not watch files for changes. The reload validates the complete manifest and every asset before changing any map. When an affected native map is selected, it also test-renders that map with the new graphics before applying them. Other affected maps redraw when they are next shown. If either step fails, or nothing changed, the current graphics, unsaved map edits, and undo history stay as they were. If the selected map's refresh fails after the new graphics are applied, yaze restores the previous catalog, rebuilds the affected maps from it, and reports the error. Map edits are kept in that case, but a paint stroke in progress has already been closed into undo history. A reload that would change the area Lyra terrain reads its graphics from (map `$3A`'s area) is refused while Lyra terrain is open. The setting is supported by native `.yaze` projects; it is not a ZScream project export or a bundled-asset import feature.

## Validation and limits

Assets must exist, match `byte_count` exactly, and contain a nonempty multiple of 32 bytes. Transfers must be aligned to tile boundaries and fit within both VRAM and the configured 1,024-tile background character window. Duplicate parent entries, overlapping transfers within an area, unsupported formats, and malformed manifests are rejected. An invalid explicit reload keeps the applied preview. Switching to another project or manifest path clears the previous preview if the new configuration is invalid.

This is a static background preview. It does not emulate DMA timing, change sprite-object rendering, replace palettes, or install assembly patches. Match the manifest to the runtime's actual character base and transfer addresses.
