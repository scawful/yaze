# Pixel-art references and drawing profiles

Use [the exact pixel recipe tool](../../scripts/pixel_art/README.md) to assemble
reviewed pixels, crops and explicit color mappings. The
[catalog template](../../scripts/pixel_art/references.catalog.template.json) and
[Oracle Sky profile](../../scripts/pixel_art/profiles/oracle-sky.example.json)
describe reference roles. They do not load artwork or train a model.

## Keep the reference roles distinct

| Reference | Role in an Oracle/ALTTP target | Keep distinct |
|---|---|---|
| ALTTP | Primary perspective, wall faces, occlusion and floor boundaries | A collage is not an addressed graphics sheet. |
| Oracle of Ages / Seasons | Primary compact contours and deliberate pixel clusters | Pick the game, palette variant and cluster treatment per asset family. |
| Minish Cap | Secondary architecture: masonry, caps, blue geometric borders and wind/cloud details | Re-author its shapes using the selected primary pixel treatment; do not blur or rescale GBA art into a target style. |
| Allhallows Eve / Parallel Worlds | Compare a specific custom scene's material separation and silhouettes | Identify and bind the exact game and image before making visual claims. |
| A Link To The Proto / development material | Prototype vocabulary and alternative forms | A reconstruction, archive interpretation and authentic development revision are different sources. |

The Oracle Sky profile is advisory. Its 64-tile canvas and 32-unique-tile limit
are example drawing budgets. They reserve no ROM sheet, VRAM, palette or map
space. The profile proposes eight slots for a 3bpp family. A separate 4bpp
profile can use 16 slots. Generic planar bytes do not prove that an Oracle
graphics slot or a complete room can consume the result.

## Bind a local image

1. Select a local image and record its source, creator/ripper credit, SHA-256,
   dimensions and palette mode in a private manifest. Keep absolute paths and
   file hashes out of the portable catalog.
2. Inspect the image at its native pixel scale. Select exact integer crop
   coordinates; a crop is an image region, not a verified tile or ROM address.
3. Declare the target RGBA palette and explicitly map every source RGBA color,
   or every used source palette index for an indexed image. Preserve duplicate
   palette slots and assign transparency deliberately.
4. Run the recipe validator and render an integer nearest-neighbor preview.
   Review the complete floor/edge/wall/corner/doorway family before expanding it.

`LOCAL_BINDING_REQUIRED` means the catalog ships metadata only.
`MISSING_REFERENCE` means a reviewed local image is still needed. A patch,
readme, release page or video link does not count as an available sampled image.
Do not check copyrighted reference sheets or sampled crops into this repository.
Keep distributable examples synthetic or use original pixels with explicit
provenance.

## Verified title provenance

- **The Legend of Zelda: Allhallows Eve** is the ALTTP hack identified in
  [Letterbomb's release thread](https://www.zeldix.net/t2725-the-legend-of-zelda-allhallows-eve-v1-10).
  It is not another game with the same Halloween title.
- **Parallel Worlds** is identified in
  [SePH's development retrospective](https://www.zeldix.net/t2295-parallel-worlds-15th-anniversary-celebration)
  and the [maintained release thread](https://www.zeldix.net/t1373-the-legend-of-zelda-parallel-worlds).
  Parallel Remodel and Parallel Universes are separate references.
- **A Link To The Proto** is SePH's reconstruction, identified in
  [the creator's release thread](https://www.zeldix.net/t2180-the-legend-of-zelda-a-link-to-the-proto).
  Its readme describes an interpretation of multiple development phases, not
  an authentic prototype from one specific date.

These links identify projects. They do not bind local image files or grant an
asset license. For the known Minish Cap Wind Tribe rip, retain Moon's visible
credit with the private source record. For development material, record the
individual file and interpretation rather than assigning one style to an
entire archive.
