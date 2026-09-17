# Changelog

## 0.8.0 (in development)

Release date, final merge SHA, and packaged-candidate evidence are pending.

### Upgrading from 0.7.2
Every item below was checked against the `v0.7.2` tag and current `master`: the
behaviour is on master, was absent or different in 0.7.2, and a user, script,
or project will notice it.

#### Saves that used to succeed now stop with an error
- **Pending editor changes block saves.** A ROM save stops while Graphics
  Editor sheet edits or Screen Editor edits (dungeon maps, dungeon-map Tile16,
  title screen, and the **Overworld Map** pause-menu world map) are pending;
  0.7.2 reported success and left them out. Dungeon **Save** and **Apply Room**
  also stop while the Object Tile Editor has unapplied edits ("Apply or
  explicitly discard Object Tile Editor changes before using Save or Apply
  Room") or the Minecart Track Editor has unpublished drafts. That editor's
  **Save Tracks** button is now **Publish Tracks**.
- **Dungeon object saves reject encodings that would corrupt on write.** In the
  GUI and `z3ed dungeon-place-object --write`, a save fails when an object ID or
  position would become a different object on write (IDs outside
  `0x000-0x0F7`, `0x100-0x13F` and `0xF80-0xFFF`, `x=63`, or a reserved stream
  prefix), or when an object has a noncanonical size. 0.7.2 silently capped Type
  1 sizes above 15 at 15, so a script passing `--size 20 --write` now fails.
- **Special-table objects are validated before writing.** Torch saves fail when
  a torch has `x` or `y` of `63`. Torches and pushable blocks accept only draw
  layer selector `0` (upper/BG1) or `1` (lower/BG2); 0.7.2 let you pick a third
  layer and silently saved it as `0`. Pushable-block saves fail when the
  ROM-wide block table would be empty, when a room has unsaved block edits but
  its block table is not loaded (0.7.2 skipped that room and cleared its dirty
  flag), or when two blocks claim the same table slot.
- **Regular entrances outside the room range are rejected.** An edited regular
  entrance whose room ID is outside `0x000`-`0x127` fails with "Regular dungeon
  entrance 0x.. room ID .. is outside [0, 296)".
- **WaterFill zones reject room IDs `0x100`-`0x127`.** 0.7.2 accepted them and
  kept only the low byte.
- **More dungeon and palette writes are checked against a loaded Hack
  Manifest.** Dungeon Save and Apply Room now check pot-item and sprite
  streams, the door-pointer slot, changed chest-table bytes, entrance and
  spawn-point tables, the room's message-ID slot on header saves, WaterFill and
  custom collision data, and dungeon palette colors. Palette Editor saves are
  checked too, and refuse to save from an inactive ROM session. Under
  `write_policy` `block` a save that 0.7.2 allowed can stop with `Write
  conflict with Hack Manifest`; under the default `warn` it shows a warning
  toast and saves. 0.7.2 caught most of these bytes only when File > Save ROM
  compared the file on disk.
- **Project saves refuse a lone carriage return.** Saving a project whose
  values contain a CR not followed by LF fails with "Project contains a lone
  carriage return in a value; refusing to write a descriptor that could not be
  read back", and the existing file is kept.
- **Invalid minecart overlay drafts block project saves.** Minecart Tracks >
  Advanced overlay lists reject empty entries, values above `0xFFFF`, and junk
  instead of dropping them, and an uncommitted invalid draft makes File > Save
  Project and Save Project As fail until it is fixed.

#### z3ed commands and scripts
- **`message-write` and `message-import-bundle --apply` require `--project`.**
  The project's ROM must be the active, headerless ROM, its Hack Manifest must
  load, and writes go to the manifest's expanded region. `--apply` also needs a
  clean in-memory ROM that matches the file on disk, and WebAssembly builds
  reject it. `--apply` now exits non-zero when a precondition, write, save, or
  readback fails; 0.7.2 printed `"status": "error"` and exited `0`.
- **Vanilla message imports are checked against the manifest.** The vanilla
  message count parsed from the ROM must equal the manifest's positive
  `messages.vanilla_count` (default `397`, unchanged from 0.7.2), or apply fails
  with `Vanilla message count mismatch: expected N, got M`. The text must contain
  exactly one standalone `[BANK]`, and the ranges written must pass the project
  write policy. The Message editor runs the same count check on save when a
  manifest is loaded. `readback_verified=true` is reported only after the saved
  ROM is reopened independently and byte-compared.
- **Message text encodes differently.** Exported raw message strings write
  dictionary references as `[D:2C]` instead of `[D:$2C]`; import accepts both.
  `[BANK]` now encodes to byte `0x80` (0.7.2 wrote `0x00`), and vanilla saves no
  longer treat a command argument byte of `0x80` (for example `[W:80]`) as a bank
  switch, so written bytes can differ from 0.7.2.
- **The collision and WaterFill JSON importers now write the ROM file.**
  `dungeon-import-custom-collision-json` and `dungeon-import-water-fill-json`
  save to disk when run without `--dry-run`; 0.7.2 changed only the in-memory
  ROM unless `--sandbox` was used. The save needs a completed backup, and a
  failed write, backup, or save rolls back and exits with an error. They now
  reject `--report` without `--dry-run`, `--report` with `--sandbox`,
  `--mock-rom` with `--sandbox`, an empty `--report`, and a `--report` path that
  resolves to the ROM. The custom collision importer writes only rooms that
  change; with `--replace-all`, `cleared_rooms` and `replace_all_clears` count
  only unlisted rooms that had custom collision data. As agent tools, both
  commands are now registered as mutating and are refused.
- **The collision and WaterFill JSON exporters refuse dangerous paths.**
  `dungeon-export-custom-collision-json` and `dungeon-export-water-fill-json`
  refuse an empty `--out` or `--report`, a path that resolves to the active or
  sandbox source ROM, and `--out` and `--report` pointing at the same file.
  0.7.2 truncated whatever path it was given, including the loaded ROM.
- **Dungeon write commands require a backup.** `dungeon-place-sprite`,
  `dungeon-remove-sprite`, `dungeon-place-object`, `dungeon-set-collision-tile`,
  and `dungeon-generate-track-collision` with `--write`, and
  `dungeon-set-room-property`, abort without saving when the backup of the
  existing file cannot be made. 0.7.2 logged a warning and saved anyway.
  `dungeon-generate-track-collision --write` now exits with an error when a
  write or save fails; 0.7.2 printed `write_error`/`save_error` and exited `0`.
- **`dungeon-place-object` preflights every run.** A dry run exits non-zero
  when the save would fail, for example when the stream outgrows its space and
  no `--manifest` copy-on-write layout is given. Output adds
  `allocator_capability`, `preflight_status`, and `preflight_error`. Without
  `--size`, Type 2 and Type 3 objects get their ID-encoded size instead of `0`.
- **`dungeon-set-room-property` edits only the requested property.** 0.7.2
  started from an empty room, so every run also reset the room's other header
  bytes and its dungeon message ID to `0`. `layout`/`layout_id` (`0`-`7`) and
  `floor1`/`floor2` (`0`-`15`) are range-checked and now actually written;
  0.7.2 reported success without writing them. `--manifest` is now honoured.
- **`--spawn` output uses the spawn-point record.** `dungeon-get-entrance
  --spawn` and `entrance-info --spawn` accept only IDs `0x00`-`0x06`. Their JSON
  moves `dungeon_id` to the top level, makes `entrance_id` the linked entrance,
  and replaces `room_id_full`, `exit_id`, `blockset`, `door`, `ladder_bg`,
  `scrolling`, `scroll_quadrant`, `music`, and `camera.x`/`camera.y` with the
  spawn-point fields. `dungeon-group` reads only those 7 records, so its
  room-to-dungeon mapping can differ.
- **`palette-set-color --write` is disabled.** It fails argument validation and
  always reports `status: dry_run`; use `dungeon-set-palette-color` for dungeon
  palettes. Other palette groups have no CLI write replacement.
  `dungeon-room-header` reports `decoded.palette` as the full 8-bit value.
- **`dungeon-object-validate` output changed.** Invalid argument combinations
  are rejected, Type 2 and Type 3 objects are tested only at their encoded
  size, the `--report` CSV gains `state_profile` and `expected_has_tiles`
  columns, and JSON mismatches gain the same fields. It no longer writes
  `dungeon_object_validation_report.json` and `.csv` to the working directory
  unless `--report <path>` is given.
- **Other z3ed output changes.** `dungeon-doctor` `chest_count` now counts real
  stateful chests (0.7.2 always reported `0`) and no longer reports "Too many
  BG3 objects". `dungeon-render --scale` fails outside `0.25`-`8.0` instead of
  clamping, and `--output` refuses a path that resolves to the ROM.
  `dungeon-room-graph` no longer emits edges for layer-swap or dungeon-swap
  marker doors. `dungeon-generate-track-collision` stamps every `0x31` as 2x2
  and ignores subtypes 13 and 15, which `dungeon-minecart-audit` also no longer
  counts. `mesen-*` commands return the real connect status code instead of
  always `UNAVAILABLE`.
- **Oracle checks.** `oracle-smoke-check` fails D4 when rooms `0x25` or `0x27`
  are missing from the WaterFill table. `dungeon-oracle-preflight` failures now
  carry the first issue's status and message. `scripts/oracle_smoke.sh` needs
  `python3` and passes `--skip-collision-maps` to every preflight.
- **`project-bundle-verify --check-rom-hash` reads `romChecksum`.** A bundle
  whose manifest has only the iOS `romChecksum` field is now hash-checked and
  can fail. Disagreeing `romChecksum` and `rom_sha1`, or a non-string field,
  fail. Empty fields count as absent, digests are only trimmed and lowercased
  (a hash with internal whitespace now fails), and the warning and pass detail
  strings changed.

#### Hack manifests
- **`protected_regions` is validated strictly for every manifest version.** It
  must be an object with a `regions` array whose entries have
  hexadecimal-string `start` and `end`, with `end > start`. In 0.7.2 a missing
  endpoint defaulted to `0x000000` and a malformed section was skipped. Now the
  whole manifest fails to load: yaze logs `Failed to load hack manifest ...` and
  opens the project without it.
- **`messages` and `minecart_tracks` are validated.** A manifest fails to load
  when `messages` counts are negative, fractional, or not numbers, when
  `expanded_range.count` exceeds `65536`, or when `messages.source` is
  malformed. A manifest containing `minecart_tracks` fails unless it has exactly
  a `source` with `format: "yaze-minecart-track-table"`, `version: 1`, and a
  portable project-relative `path`. 0.7.2 ignored both.
- **Hook regions now take precedence over bank ownership.** Address
  classification checks protected and hook regions before `owned_banks`. An
  address inside a hook region is now `hook_patched` in every bank, so writes
  into hooks in `shared`, `vanilla_safe`, or `ram` banks may now warn or be
  blocked, depending on `write_policy`.
- Manifests at `manifest_version` 2 still load. Version 3 is only required to
  use the new `editor_managed_regions` block, and a v3 manifest must use mapped
  LoROM addresses for protected endpoints.

#### Projects, sessions, and backups
- **One ROM file backs only one loaded session, on every open path.** In 0.7.2
  only the File > Open ROM dialog blocked a duplicate, and only on an exact
  path-string match. Recent files, drag-and-drop, command-line and startup
  opens, and project files now fail with `ROM backing file '...' is already
  open in session N`. Paths are compared after resolution, and Save As onto a
  file another session has open is rejected too.
- **New Project uses the guided dialog and writes its file immediately.** File
  > New Project and the welcome screen open the ROM-plus-name dialog, and
  `<name>.yaze` is written next to the source ROM on create. Creation, and the
  Project File editor's Save As, fail with `Project file already exists:
  <path>`; File > Save Project As still replaces an existing file, via a
  temporary file and atomic rename. New Project is refused while any session has
  unsaved work.
- **Project File editor and ROM swaps protect unsaved work.** The Project File
  editor refuses New and Open while its draft is unsaved, and refuses to save
  over the open project when the draft would change the session's ROM or
  cannot be parsed. Project Management > Swap ROM now loads the new ROM
  immediately; Swap ROM and Reload ROM are refused while the session has
  unsaved ROM, graphics, screen, dungeon, or palette edits.
- **Restoring a backup stages it.** File > ROM Backups... > Restore loads the
  backup as unsaved changes; only Save ROM writes it, and the dialog can discard
  it. It is refused while edits are pending or for files that are not managed
  backups of the active ROM. 0.7.2 overwrote the live ROM in place.
- **Closing the window asks about unsaved work.** The title-bar close and
  system quit requests go through the same prompt as File > Quit and can be
  cancelled; 0.7.2 exited immediately.
- **`project.yaze` line endings.** CRLF files now load their settings; in 0.7.2
  every setting in a CRLF file, including write-policy flags, was ignored, so a
  Windows-edited project may now enforce policies it silently skipped. A file
  with a lone carriage return fails to open with "Project file contains
  unsupported lone carriage returns".
- **Custom object paths are confined to the project.** Custom object filenames
  must be forward-slash relative `.bin` paths inside `custom_objects_folder`.
  Absolute paths, `..` components, backslash separators, and symlinked targets
  are rejected on load and on apply. In `[custom_objects]`, an empty entry now
  keeps its position (`a.bin,,c.bin` maps `c.bin` to subtype 2), and entries
  past the fixed slot counts for `0x31` (16), `0x32` (3), and `0x54` (2) are
  ignored.
- **Wall corners `0x100`-`0x103` always draw as wall corners.** 0.7.2 replaced
  them with minecart track corner assets when a project mapped `0x31` track
  corner files and the room contained a `0x31` object.
- **The minecart track editor requires a manifest source.** It no longer reads
  and writes the fixed `Sprites/Objects/data/minecart_tracks.asm`. It needs a
  loaded Hack Manifest defining `minecart_tracks.source`, and refuses a source
  that is a symlink or resolves outside the project root.
- **Water Fill saving follows the project.** The Workbench Apply Scope **Water
  Fill** checkbox is read-only and set from the new `.yaze` key
  `save_dungeon_water_fill_zones` (default `true`). When it is `false`, the
  Water Fill panel is read-only.
- **Best-effort ROM backups copy the file being overwritten.** With
  `Rom::SaveSettings::backup` (used by the `yaze_save_rom()` C API), 0.7.2
  copied the originally loaded ROM, so saving to a different existing file did
  not keep that file's previous contents. A destination that does not exist yet
  gets no backup file. The GUI Save As path already did this in 0.7.2.

#### Dungeon data read or drawn differently
- **Pushable blocks decode differently.** Word bit 13 is the draw layer and bit
  14 a separate behaviour bit preserved on save; 0.7.2 read bit 13 as part of
  the Y coordinate. The lower-layer blocks in vanilla rooms `0xA8`, `0x66`, and
  `0x2C` now load 64 tiles higher and render on BG2.
- **Several objects draw and select with their USDASM footprint**, so their
  renders, selection boxes, and dimensions differ from 0.7.2: Somaria paths
  (`0xF83`-`0xF8C`, `0xF8E`, `0xF8F`), moving walls `0xCD`/`0xCE`, hammer pegs `0xF96`, `0x8B`
  and `0x8C`, light beams `0xFF0`/`0xFF1`, floor light `0xFF4`, `0xB5`, and
  rupee floor `0xF92`. Full room renders no longer draw the big light beam
  `0xFF1` unless the room `0x065` bombed-floor state is set.
- **Custom collision is rewritten in place when it fits.** A room whose new
  collision encoding fits its existing, unshared span keeps its pointer, so the
  written ROM bytes differ from 0.7.2, which always appended and repointed.
- **The object browser offers only encodable IDs.** It no longer lists
  `0x0F8`-`0x0FF` or `0x140`-`0x141`, new Type 1 placements start at size `2`
  instead of `0x12`, and the category filters use the shared object category
  table. The 128-object limit on the third object stream, and its validation
  error that blocked room saves, are gone.

#### Editor layout, menus, and shortcuts
- **First launch rearranges the Dungeon editor.** Settings migrations make the
  Dungeon Workbench visible and close the standalone Room Selector, Room
  Matrix, Object Selector, Sprite Editor, Item Editor, Room Graphics, Door
  Editor, Palette Editor, Entrance List, and Entrance Properties windows. Pinned
  panels and named layouts are untouched.
- **Menu items moved.** ROM Information, Create Backup, ROM Backups..., Validate
  ROM, Export BPS Patch..., and Apply BPS Patch... moved from File to Tools > ROM
  Analysis. View > Layout is gone (use Windows > Layout). Agent drawers moved to
  View > Drawers. Search > Window Finder is now **Find Window…**, opens the
  Command Palette, and is bound to `Ctrl+P`.
- **Shortcuts.** A shortcut bound to a plain key no longer fires while Ctrl,
  Shift, Alt, or Cmd is held. `Ctrl+Shift+W` is only Close Session and no longer
  toggles the Workbench.
- **Dungeon layer controls are renamed.** "Layer 1/2/3" is now **Object
  Stream: Primary / BG2 Overlay / BG1 Overlay** for room objects and **Upper
  Layer (BG1)** / **Lower Layer (BG2)** for torches and pushable blocks. Stored
  value `2`, formerly labelled BG3, is the BG1 overlay stream.
- **Issue reports are saved only on request.** The dungeon canvas no longer
  appends issue reports to the local issue log on open, copy, screenshot, or
  close; only **Save to Issue Log** writes one. The context menu's **Report**
  is **Capture Issue**, and **Sample Object** is **Use Object as Brush**.
- **Removed UI.** The welcome screen's Release History panel and its theme
  switcher (use Settings > Appearance), the Object Selector's **New Custom
  Object** button, and the Minecart Tracks **Generate** buttons (replaced by
  **Preview** and **Apply Preview**) are gone. Settings > Project Configuration
  > Dungeon Overlay is read-only; edit those lists in Minecart Tracks >
  Advanced, which reads unprefixed values as decimal and does not accept ranges.
- **Spawn points are edited only in Entrance Properties.** The Entrance List
  property table is read-only for slots `0x00`-`0x06`.
- **`--startup_dashboard=hide` also suppresses the editor chooser on later
  automatic opens**, after a ROM or project finishes loading or ROM load options
  are applied. `show` and `auto` are unchanged and `Ctrl+E` still opens the
  chooser.

#### Themes
- **Hand-written `.theme` files are filled in and preserved differently.** Keys
  a file declares are no longer overwritten by smart defaults: in 0.7.2 a
  declared opaque black was replaced for semantic keys such as
  `text_highlight`, `active_selection`, `focus_border` and the `editor_*`
  colours, and a declared transparent black for keys such as `border`,
  `separator` and `text_link`. Keys a file omits are now derived from seeded
  `accent`, `error`, `warning`, `success`, and `info` rather than left black,
  including `plot_lines` and `plot_histogram`, which no shipped theme declares,
  so plot colours change in the built-in presets too. An omitted agent
  `panel_border` is derived from `border` with alpha capped at `0.45`.
- **Theme discovery paths.** yaze no longer searches
  `/usr/local/share/yaze/themes/`, `/usr/share/yaze/themes/`, or
  `<app bundle>/Contents/Resources/themes/`; move themes kept only there to the
  user themes folder. The macOS app bundle ships only themes listed in
  `assets/themes/distributable-themes.txt`.
- **The theme picker no longer previews on hover.** Settings > Appearance uses
  a dropdown and applies a theme only when you pick it; Display Density is a
  dropdown too.

#### Packages and source builds
- **Linux packages use a standard filesystem layout.** The `.deb` installs
  `/usr/bin/yaze`, `/usr/bin/z3ed`, and `/usr/share/yaze/assets/`; 0.7.2 put
  them directly in `/usr`. The `.tar.gz` has the same `usr/` tree, so scripts
  that ran `./yaze` from the extracted folder must use `usr/bin/yaze`.
- **Release asset names changed.** Windows downloads are
  `yaze-<version>-windows-x64.exe` and `.zip` instead of
  `yaze-<version>-win64.*`, and the `.deb` is `yaze_<version>_<arch>.deb`
  instead of `yaze-<version>-Linux.deb`. The Linux `.tar.gz` name is unchanged.
- **Source builds.** CMake build presets that used 8 jobs now use 4. The iOS
  XcodeGen project reads the preset build directories
  (`build/presets/ios-debug`, `ios-release`, `ios-sim-debug`) instead of
  `build-ios/` and `build-ios-sim/`.

### Dungeon object rendering
- Fixed thin strip routines whose name suffix is a minimum length, not a
  position offset: `_plus3` solid strips (carpet trim `0x34`/`0x71`) and the
  `_plus13`/`_plus12` rail-wall routines (`0x2F`/`0x30`, `0x6C`/`0x6D`) now draw
  from the object's own origin instead of 3, 13, or 12 tiles away, with the
  USDASM opening and closing caps.
- Matched USDASM geometry for wall corners, diagonal walls, and all four
  diagonal-ceiling orientations.
- Used Room Header `Floor1`/`Floor2` patterns for the `0xC4`/`0xDB` floor-copy
  objects instead of the subtype payload.
- Lower straight inter-room stairs (`0xFA6`-`0xFA9`) now promote the adjacent
  BG1 column to high priority, and spiral stairs promote their left and right
  flank tiles, matching USDASM. Both change priority only, without painting
  over tiles already in the layout or object buffers.
- Conditional edge and cap routines (rails, rail-wall corners) now check the
  tile that actually owns each position, layout or object, before skipping a
  corner. Layout objects now record their own BG2 reveal requests, separate
  from room objects.
- Kept explicit door bodies above room wall-guidance objects while preserving
  ROM object-stream order, layout ownership, reveal masks, and SNES priority
  bits.
- Deferred BG2 reveal masks to compositing instead of punching transparent
  pixels into raw BG1, and copied object-layer bitmaps to their SDL surfaces
  row by row using the surface pitch instead of one flat copy that could
  shear rows when pitch exceeded width.
- Matched individual object families against the disassembly: Somaria paths,
  pushable blocks, torch codecs, hammer pegs, vertical jump ledges, light beams
  (including attic-state gating for the big beam), unconditional floor lights,
  archery curtains, rupee and bombable floors, big key locks, prison cells,
  moving walls, the enabled star tile and lit torch drawn as 2x2, the Mario
  portrait drawn as 4x2, BothBG wall size semantics, Big Wall Decor,
  TableBowl, BigGrayRock, Smithy Furnace, Agahnim's altar, the Fortune Teller
  room, the full 24-tile payload for Turtle Rock pipe `0xFDC`, and exact
  4-word bar-corner payloads (`0xFD6`-`0xFD9`).
- Corrected canonical object payload counts and canonicalized room object
  sizes, disabling resize controls for fixed-size objects.
- Removed the hard-coded `IsAllBgsObjectId` list that set `all_bgs_` from the
  object ID. BothBG routing now comes from draw-routine registry metadata plus
  explicit object-specific routing for stairs and fixed facades. ObjectDrawer's
  duplicate dimension switch was replaced by `DimensionService`.
- The dungeon object selector now shows a draw-routine badge on each object
  card: a direction or category glyph, or `C`/`K`/`B`/`P` for chests, big key
  locks, bombable floors, and prison cells. BothBG routines get a `2` suffix
  and accent color. The tooltip lists the routine family with its base pattern
  size (for example `Corner 4x4`) and notes when a routine writes to both BG1
  and BG2.
- Dungeon sprite previews no longer clip large multi-part sprites (`0x7E`,
  `0x7F`, `0x80`, `0xC7`, `0x92`). The preview buffer is sized by measuring
  the sprite's own tile stream, and the canvas places it using those bounds.

### Dungeon palettes and previews
- Fixed dungeon graphics palette slot mapping and refreshed cached rooms after
  palette edits, on global palette apply, and for the active placement ghost.
- Dungeon sprite previews now take their palettes from the room's palette-set
  selectors (row 8 left and rows 13/14 left) instead of a fixed sprite palette
  table, with a fixed Light World environment half-palette in row 8 right and
  the underworld environment palette in row 14 right. Preview pixels using
  color index 0xFF are no longer treated as transparent.
- Fixed the sprite preview dispatch chain so the generic fallback no longer
  overdraws sprite-specific previews, and normalized palette widget color
  values.
- Dungeon room sprite previews are now cached (up to 128 decoded previews,
  keyed by room graphics revision, sprite ID, subtype and overlord flag)
  instead of being re-decoded on every frame; palette, position and zoom still
  apply live, and the cache clears on asset reload or project/asset-path/hack
  change.

### Dungeon editor workflow
- Kept the room canvas vertically stable by removing the selection action
  shelf and the recent-room tab strip from above it; recent rooms moved to a
  toolbar popup and the status-bar selection entry opens the Selection
  inspector.
- Replaced the Tools inspector's two-row icon strip with a single tool chooser
  grouped into Edit (Object Selector, Door, Sprite, Item Tools), Room (Room
  Graphics, Palette), and Review (Room Tags, Custom Collision, Water Fill,
  Minecart Tracks), plus a **Pop out** button.
- Added **Pop out** (or **Show window** when the tool's window is already
  open) to Workbench tools, and a tool is now drawn in one place at a time:
  while its standalone window is open, the inspector points to that window
  instead of drawing the tool a second time. Leaving the Workbench now reopens
  only the navigation windows it closed.
- Workbench toolbar actions that do not fit at narrow widths now move into an
  overflow menu (tooltip "More dungeon actions") instead of disappearing.
  NESW room navigation stays inline.
- When the toolbar is too narrow for the Compare button, Compare is still
  reachable from the overflow menu, with the searchable room picker, direct
  room ID entry, **Swap Rooms**, **Sync View**, and **End Compare**.
- Streamlined Object Selector controls into search, category, a new object
  stream filter, and a **More** menu (Clear filters, Show thumbnails, Card
  size Compact/Medium/Large). Previews now keep their aspect ratio above a
  reserved hex ID footer, and the dungeon palette grid adapts to 16/8/4/2/1
  columns.
- Stabilized the canvas right-click menu: selection actions moved into a
  **Selection** submenu that always starts with **Use Object as Brush**
  (disabled when no object is under the cursor; replaces the conditional
  Sample Object row), layer actions use stream-aware labels, and **Report**
  was renamed **Capture Issue**.
- Made issue capture local and opt-in: reports are no longer auto-saved on
  open, copy, screenshot, or close. Only **Save to Issue Log** writes the
  report. The object drawer trace now replays the room-stream objects before
  the selected one, with layout tilewords, layer, and floor graphics context.
- Kept the report body independently scrollable with stable footer actions,
  reserved capture button width, ellipsized long status with a full tooltip,
  and sized the report within the usable viewport including menu-bar insets.
- Split room composite ownership by presentation so the canvas, matrix, issue
  report, and map preview cannot overwrite one another's texture identity, and
  retired composite textures through the graphics arena and texture queue.
- Loading a dungeon into Dungeon Map (preset or project registry) now clears
  the previous dungeon's room type badges, stair and holewarp connections, and
  room positions.
- Added editable dedicated spawn points in the UI and safe spawn-point
  persistence.
- Warned on room event slot conflicts, simulated chest and lock room-event
  slots, and classified stateful chest usage.

### Oracle and custom dungeon assets
- Replaced the modal Custom Object Workshop with a non-modal **Custom Assets**
  mode in the object selector covering the 21 fixed Oracle runtime assets: 16
  slots for object `0x31`, three for `0x32`, and two new sprite-body slots for
  `0x54`, grouped into Tracks + Props, Ice Props, and Boss Bodies. The browser
  edits or places existing slots only; new subtypes need an ASM dispatch-table
  change.
- Added **Edit Tile Layout** and **Place in Room** actions to Custom Assets,
  plus a **Minecart Routes & Collision** shortcut on track slots, replacing the
  blocking workshop popup. Edit Tile Layout is desktop-only.
- Modeled mapping provenance explicitly so default filenames, configured
  filenames, and configured-but-unmapped slots are distinguishable, and
  preserved sparse positional subtype mappings through project save/load.
- Removed the track-corner alias that routed wall-corner objects
  `0x100`-`0x103` to object `0x31` corner assets whenever a project mapped
  `0x31`. Rendering, geometry, previews, diagnostics, and tile editing now keep
  those IDs as wall corners unless the project maps the same ID directly.
- Kept `0x54` sprite-body `.bin` files in their raw source form, applying the
  Oracle page-`0x300` tile transform to nonzero words only when rendering rooms
  and previews.
- Published desktop asset edits with path confinement, exact-source comparison,
  atomic replacement, rollback, and decoded readback, failing closed on WASM,
  and validated portable forward-slash asset filenames before host path
  parsing.
- **Custom Assets > Reload Assets** (formerly Reload Workshop) now also
  refreshes external Oracle sprite preview art in every dungeon room,
  Workbench, and comparison viewer, stays available when Custom Objects is
  disabled, and keeps unsaved room edits, tile drafts, and selections.
- Made **Minecart Tracks > Advanced** the only editor for minecart overlay
  IDs. The project settings **Dungeon Overlay** section is now a read-only
  summary with an **Open Minecart Tracks** button, which registers the panel on
  demand when Custom Dungeon Objects was enabled after the dungeon editor
  loaded.
- `z3ed oracle-smoke-check` now fails D4 when rooms `0x25` and `0x27` are
  missing from the runtime WaterFill table, and `dungeon-oracle-preflight`
  accepts `--required-water-fill-rooms`.
- Saving chests keeps the ROM chest table's physical record order instead of
  regrouping all records by room ID; untouched records keep their exact bytes.

### Save safety and persistence
- Checked dungeon palette saves (Save and Apply Room), Palette Editor saves, and
  Object Tile Editor ROM writes against the Hack Manifest when a project
  manifest is loaded. The z3ed `dungeon-set-room-property --manifest` check now
  also covers room-header properties and message-ID bytes. Object Tile Editor
  writes also gained explicit source provenance, cache invalidation after writes,
  and detection of tile sources shared by several objects.
- Added an `editor_managed_regions` Hack Manifest section (requires
  `manifest_version` 3 or newer). It marks exact address ranges inside ASM-owned
  banks as safe for editor writes. Hook-protected addresses still block writes.
- Made z3ed `dungeon-set-room-property`, `dungeon-generate-track-collision`,
  `dungeon-import-custom-collision-json`, and `dungeon-import-water-fill-json`
  all-or-nothing: a failed write or save rolls back the in-memory ROM changes
  (ScopedRomTransaction) and returns an error. Also made minecart collision
  generation in the Minecart Track Editor transactional and undoable.
- Included unapplied Object Tile Editor layouts in dungeon/session dirty
  detection and blocked Save or Apply Room until those edits are applied or
  discarded.
- Blocked ROM saves while Screen Editor edits (dungeon maps, dungeon-map Tile16,
  title screen, and the **Overworld Map** pause-menu world map) or Graphics Editor sheet edits are pending,
  instead of reporting a partial save as successful. Dungeon Save and Apply Room
  now refuse while Minecart Track Editor drafts are unpublished (use Publish
  Tracks or discard them). Minecart overlay settings edits now mark the project
  dirty and are written by project save.
- Rejected unsafe room object encodings and guarded canonical room-object sizes
  on save.
- Saved room layout ID and floor 1/floor 2 changes to the object-stream header
  from the dungeon editor and from z3ed `dungeon-set-room-property`; 0.7.2
  silently dropped them. Door moves, nudges, and type changes now mark the room
  for saving. Room header saves keep the pit target layer bits. Room message-ID
  bytes are now included in manifest conflict checks. Regular dungeon entrance
  saves are validated and checked against the Hack Manifest before writing.
- Counted dungeon entrance, spawn-point, and pit-damage edits as unsaved dungeon
  work (0.7.2 counted only pending rooms), so unsaved-work checks and
  restore/discard guards see them. Session tabs and the session manager now mark
  a session Modified when only project settings or project editor drafts are
  unsaved.
- Made ROM backup restore safer: it refuses while ROM edits are pending, accepts
  only managed backups of the active ROM, and stages the restored ROM as unsaved
  until Save ROM. A new Discard Restored Backup button abandons a staged restore.
  `Rom::SaveSettings::backup` (used by the `yaze_save_rom()` C API) now copies
  the existing destination file instead of the originally loaded ROM.
- Kept project settings, project-file drafts, and palette edits and undo history
  per ROM session, so switching between open ROMs no longer loses or mixes
  unsaved project and palette work. Palette saves reject writes from a different
  session. Every open path, not only File > Open ROM, now refuses a ROM file
  already open in another session, and Save As onto such a file is rejected.
- `project.yaze` files with CRLF line endings now load their settings, and a
  file or saved value containing a lone carriage return is refused instead of
  silently falling back to defaults.

### CLI and ROM safety
- Added `dungeon-get-palette` to resolve the full room palette-set mapping,
  raw colors, and every room sharing the selected global US/OOS palette.
- Added dry-run-first `dungeon-set-palette-color` with exact mapping/color CAS,
  Hack Manifest ownership checks, clean disk baseline, two-byte write fence,
  required backup, atomic save, whole-ROM diff, and external reopen/readback.
- Disabled legacy `palette-set-color --write` because it could report an
  in-memory mutation as persisted; its read-only preview remains available.
- Added `dungeon-set-door-type` and `dungeon-set-pot-item` for guarded edits to
  existing doors and pot items (dry-run by default, expected-value checks,
  required `--manifest`), plus `dungeon-list-pot-items` to list pot entries.
- Added `--include-objects` to `dungeon-describe-room` to list encoded room
  objects (ID, subtype, position, size, stream index), and made `--spawn` on
  `dungeon-get-entrance` and `entrance-info` report the dedicated spawn-point
  schema (IDs 0x00-0x06).
- Added `dungeon-remove-object`, which removes one ordinary room object by
  stream index only when `--expect-id`, `--expect-x`, `--expect-y`,
  `--expect-size`, and `--expect-layer` all match. It is a dry run unless
  `--write` is given, accepts `--manifest`, and refuses table-backed and chest
  objects.
- `project-bundle-verify --check-rom-hash` accepts the iOS `romChecksum`
  manifest field alongside `rom_sha1`, treats empty fields as absent, and fails
  when the two fields disagree. iOS project manifests now record the ROM
  checksum.
- Made z3ed dungeon write commands roll back in-memory ROM changes when the
  write or save fails, and require a backup when saving.
- Added `--manifest` to `dungeon-place-object`; shared or growing object
  streams fail closed unless the manifest allows copy-on-write, and dry-run and
  write run the same capacity preflight.
- `dungeon-set-room-property --manifest <path>` rejects writes into
  manifest-owned ranges.
- Reused uniquely owned custom-collision blobs for same-size or smaller
  rewrites, while keeping aliased, overlapping, and growing streams on the
  copy-on-write append path.
- Made custom collision imports idempotent, collision JSON imports
  transactional, and collision exports ROM-alias-safe.
- Added Hack Manifest `dungeon_stream_regions` so shared or growing dungeon
  object and sprite streams can be saved copy-on-write into manifest-owned
  space, and pot-item streams can be repacked deterministically
  (`repack_all`).
- `z3ed --self-test` now checks that two representative runtime assets
  (`agent/prompt_catalogue.yaml` and an Overworld patch) resolve from the
  installed package layout.

### Messages
- Message editor saves now write only the dirty domains (font widths, vanilla
  messages, expanded messages), build the whole write plan and check it against
  the project's hack manifest write policy before writing any bytes, and no
  longer treat a command argument byte as a text bank switch.
  `message-import-bundle --apply` now reopens the saved ROM and verifies the
  written bytes and vanilla messages by readback.
- Message dumps now write dictionary tokens as `[D:XX]` (0.7.2 wrote
  `[D:$XX]`, which could not be imported back); the importer accepts both
  forms.
- Added `z3ed message-source-sync --project <path> --file <bundle.json>`,
  which merges expanded messages into the project's source files without
  touching the ROM; it is a dry run unless `--write` is given with
  `--expected-source-sha256`.
- `z3ed message-write` and `message-import-bundle --apply` now require
  `--project`; the write is refused unless the project's ROM is the active
  ROM, the ROM file is headerless, and the project has a loaded hack manifest,
  and the project's write policy is applied to the planned message writes.

### Appearance and themes
- Added five editor themes: Blood Moon, Catppuccin Mocha, Dracula, Rosé Pine,
  and Temple of Time. The upstream MIT notices for Catppuccin, Dracula, and
  Rosé Pine are kept in `assets/themes/THIRD_PARTY_NOTICES.md`. A unit test
  loads and applies each of the five new themes through the theme manager.
  Release bundles now include only the themes listed in
  `assets/themes/distributable-themes.txt`.
- Theme files now get smart defaults for colors they leave out. In 0.7.2
  the unset check looked for transparent black, so omitted borders,
  scrollbars, table colors, links, and modal backgrounds stayed opaque black
  on file-loaded themes. Colors that a `.theme` file declares are never
  replaced, so a deliberate black border or highlight is kept.
- Themes that leave out `accent`, `error`, `warning`, `success`, or `info`
  now get defaults. `accent` falls back to `primary`, and the status colors
  use fixed values. Colors derived from them, such as link text, histogram
  plots, text highlight, and secondary selection, are no longer black when a
  file omits them.
- Switching from a preset theme back to Classic YAZE no longer leaves the
  preset's docking preview, text cursor, link, tree-line, tab overline, and
  rounding values behind. Display Density now also works while Classic YAZE
  is active. Saving a `.theme` file keeps `grab_rounding`,
  `window_border_size`, `frame_border_size`, and `animation_speed`, and a
  malformed `[style]` number no longer aborts startup.
- Retuned Forest and Forest Light so buttons, headers, tabs, and title bars
  use neutral surfaces, with green kept for hover, active, and selected
  states. Nested editor-panel borders are fainter on all file-backed themes:
  the derived panel border is capped at 45% of the theme border's opacity.
- Replaced the fixed-height hover-preview theme list with an explicit
  full-width picker and the cramped density radio buttons with a responsive
  picker.
- Fixed theme filename synthesis to pass an unsigned char to `isalnum`.

### Editor shell and navigation
- Reworked the welcome screen into a compact start card. The Start and Recent
  panes no longer scroll: the Recent list is cut to what fits, and optional
  Start rows are dropped on small windows. The What's New release-history
  card is replaced by a Release notes link in a one-row footer, and the
  resume button now names the last project (`Resume <name>`).
- Removed the unused second welcome-screen instance and its duplicate
  callback wiring from `EditorManager`. Removed the fixed-height quick-actions
  pane that clipped first-run controls. Recent-file cards can now be opened
  from the keyboard.
- `gui::OpenUrl` now opens links through ImGui's platform shell opener
  instead of running `open`/`xdg-open`/`start` through `system()` with the
  URL pasted into the command. Web (Emscripten) builds open links in a new
  browser tab with `window.opener` detached. If the browser cannot be opened,
  the welcome screen's Release notes link shows a message saying so.
- Repaired the editor chooser dashboard. `--startup_dashboard` now controls
  the chooser that opens after a ROM or project loads (Ctrl+E is not
  affected). The Performance Dashboard no longer renders twice per frame, and
  the dashboard's shortcut hint says Ctrl+E instead of F1. Cards are readable
  on light themes, and Display Density now changes the layout. Headings scale
  the current font instead of switching to a hard-coded font slot. Recent
  editors now include Settings, skip invalid entries, and ignore duplicates.
- Renamed the Search menu's Window Finder item to **Find Window…**. Help →
  **Keyboard Shortcuts** now opens the shortcuts browser instead of Settings
  and is bound to Ctrl+Shift+/. The Dungeon Workbench window no longer
  shows Ctrl+Shift+W as its shortcut, because that chord closes the current
  session.
- Added an icon strip below the right sidebar drawer header that always shows
  every drawer: clicking the active icon closes the drawer, tooltips show
  assigned shortcuts, and Notifications shows an unread dot. The header gained
  a context badge (agent ready for Agent Chat, unread count for Notifications,
  a lock icon when Properties selection is locked, the current editor for
  Help) and a **Switch Sidebar Drawer** button.
- Removed three unreferenced dashboards: the agent metrics dashboard panel and
  the z3ed TUI dashboard component and its layout ID.

### Emulator, iOS, and platform
- Added TCP endpoint support to the Mesen socket client alongside Unix domain
  sockets.
- Restored the iOS device build by aligning XcodeGen header/library paths with
  the CMake iOS presets, included the remote desktop and review Swift views in
  the iOS target, and replaced an iOS-unavailable directory-creation path.
- Routed the normal macOS Quit menu item through ordered application shutdown.
- Mesen2-OOS CPU registers are read from the server's lowercase JSON keys.
  0.7.2 looked for uppercase keys, so every register read as `0` in the Mesen
  Debug panel, ASM follow, and `z3ed mesen-*` commands.

### Release engineering and packaging
- Added a Release-config native test gate that fails when CTest discovers no
  real unit or integration suites, resolves wrong multi-config binaries, or
  produces empty JUnit results, plus a `release-test` preset that enables
  stable source-level suites without changing package builds.
- Scoped build caches by configuration and capped shared build actions at four
  jobs; bounded default local build parallelism to four workers.
- Release validation now checks that the packaged `manifest.json` version and
  Git SHA match the release being built. Runtime asset lookup (`AssetLoader`,
  theme search, agent prompts, and the `TailMapExpansion.asm` patch) now uses
  `PlatformPaths::FindAsset` instead of paths relative to the working
  directory, and adds the Linux FHS location `usr/share/yaze/assets` next to
  the executable.
- Moved Linux DEB/TGZ payloads to an FHS layout (`/usr/bin`,
  `/usr/share/yaze/assets`, `/usr/share/doc/yaze`). Release validation now
  checks that layout, resolves shared libraries with `ldd`, matches the DEB
  version against the expected version, checks that assets exist, and runs
  a clean DEB install and purge.
- Embedded the runtime asset tree in the macOS app bundle under
  `Contents/Resources/assets` (themes limited to the distributable allowlist).
  DMG validation now requires a valid signature, verifies it again after
  copying the app to another location, checks that the bundle's assets match
  the DMG asset tree and that the manifest version matches, and smoke-tests the
  DMG executables.
- Windows Release packaging now fails unless app-local `msvcp`/`vcruntime`
  DLLs are found, ZIP validation requires them, and release CI runs an
  isolated NSIS silent install that checks the registry entry and
  uninstall cleanup.
- Replaced the two duplicate pull-request WASM debug builds with one
  Release-derived `wasm-smoke` preset (Emscripten version still 3.1.51, now
  set in one `EMSDK_VERSION` variable). The job has a 45-minute timeout and 4
  build jobs, caches CPM at the path the build really uses
  (`~/.cpm-cache`) with rekeyed CPM/ccache caches, and requires packaged
  output plus serial (`--workers=1`) Chromium Playwright smoke tests.
- Fixed the gRPC/CPack install graph in CMake (bundled gRPC is now
  `EXCLUDE_FROM_ALL`) instead of patching generated `cmake_install.cmake`
  scripts in the release workflow.
- `scripts/install-nightly-local.sh` now validates a nightly before
  activating it. It stages each install in a unique release directory,
  requires both `yaze` and `z3ed` executables, signs and verifies the macOS
  bundle after resources are copied, runs `--version` on both executables, and
  replaces the `current` symlink atomically only after validation passes.
- `scripts/pre-push.sh` now fails when any component of the smoke or UI
  gtest filter selects zero tests, so a mistyped or stale filter can no
  longer look like a passing run. The default filters now target
  `WorkspaceWindowManagerPolicyTest.*`.

### Testing and validation
- Added independent Mesen ROI baselines and bombable-floor Mesen baselines as
  witnesses separate from the editor's own renderer.
- Pinned visual parity for rails, BigHole, TableRock, and flood-water overlays,
  and regenerated the dungeon room regression goldens after the Left/Right
  dungeon palette-slot mapping fix.
- Expanded dungeon object validator state coverage and included palette writes
  in the save preflight.
- Added `scripts/agents/audit-dungeon-visual-parity.sh`, a tiered dungeon visual
  parity audit that fails on missing test or `z3ed` binaries, empty, skipped,
  or failed test selections, invalid or empty `dungeon-object-validate` JSON,
  nonzero mismatches, or unexpected empty traces.
- Stopped the test runner from printing its option help unconditionally, which
  polluted GoogleTest discovery, and added a `TestRunnerDiscoveryContract`
  CTest that fails if help text reappears in the test inventory.
- Reformatted selected dungeon editor and Oracle validation sources to the
  project clang-format style.

### Documentation
- Replaced stale parity percentages and Stable/Beta/WIP labels with one
  canonical desktop editor readiness matrix (Tester ready, Conditional, View
  only, Experimental) and a tester guide, distinguishing component test, direct
  ROM readback, app-path test, GUI smoke, and manual acceptance evidence.
- Rewrote the cross-platform artifact and acceptance contract, and documented
  all 21 fixed Oracle custom-object slots including `0x54` raw/runtime tilemap
  semantics.
- Generated `docs/internal/zelda3/alttp-quick-reference.md` from pinned usdasm
  (labels, dispatch, vectors) and jpdasm (RAM symbols), and added
  `docs/internal/zelda3/snes-hardware-reference.md` with register addresses
  checked against usdasm. `scripts/agents/alttp_reference.py check` fails when
  a generated section is stale or a table address disagrees with usdasm.
- Added a cloud agent bootstrap script and setup documentation for ROM-less
  container work.
- Updated the dungeon usage guide for the issue-reporting layout and fixed the
  stale README contributor link.

## 0.7.2 (July 17, 2026)

### Dungeon Editor Follow-through
- Made room navigation collapse into a compact 2x2 grid in tighter layouts instead of squeezing into a thin inline strip.
- Stacked workbench and compare controls earlier, trimmed helper copy, and reduced toolbar chrome so more height stays with the room canvas.
- Updated workbench pane compaction so the left-side chrome gives way before the inspector or center canvas.
- Kept this as groundwork for a future optional connected-room overview mode rather than a replacement for focused single-room editing.
- Rendered object and sprite selector previews by default and kept workbench
  local tools available when standalone windows are open.
- Persisted entrance and special-object dungeon save domains through the main
  save path.
- Rejected door positions outside ALTTP's 12-entry USDASM door-position
  tables, with unit coverage for all directions.
- Treated `DrawNothing` wall-moved check objects as explicit zero-tile
  payloads instead of forcing an 8-tile fallback.
- Sourced room-object labels from the canonical dungeon object tables and
  exported type-specific and unified room-object resource labels.
- Added editable pit-damage room membership controls and kept pit-damage ROM
  tests in the integration suite where their ROM dependency is explicit.
- Rejected invalid pushable-block loader operands before pointer arithmetic and
  pinned both operand shape and table-capacity boundaries in tests.
- Added a hardcoded room `0x001` BG1/BG2 overlap pixel golden so object-stream
  compositing regressions fail with a focused diagnostic.

### Overworld Follow-through
- Added a canvas context-menu Tile16 sampling action for the ZScream-style
  right-click eyedropper workflow.

### Save Safety and Project Reliability
- Made coordinated editor saves whole-ROM transactional: a late serializer,
  validation, backup, or disk-write failure now restores the in-memory ROM and
  leaves dirty edits available for retry.
- Preserved Save As destinations through ROM-hash, pot-item, and ASM-conflict
  confirmations, bound pending saves to their originating ROM session, and
  consumed confirmation bypasses safely.
- Made dungeon object, sprite, pot-item, and chest saves fail closed on
  unsupported growth or ambiguous shared-stream ownership instead of guessing
  free space or leaving partial repacks behind.
- Hardened expanded-message, Map32/custom-overworld, palette, manifest,
  project-path, CRC32, and SHA-1 boundaries, including headered-ROM and
  vanilla/Oracle round-trip regression coverage.

### Localization and Platform Reliability
- Added the internationalization catalog pipeline and French localization.
- Initialized native file dialogs explicitly on Linux and Windows.
- Hardened session restore, SDL startup, asset lookup, configuration loading,
  ROM handling, and CLI failure paths.

### Build and Test Reliability
- Stabilized fork pull-request checks, WASM builds, and the memory-sanitizer
  gate across the release matrix.
- Added GUI automation for pit-damage inspector membership edits.

## 0.7.1 (April 19, 2026)

### Welcome Screen & Project Startup
- Added **Quick Actions** to open **Prototype Research** (Graphics, `graphics.prototype_viewer`) or the **Assembly editor** (`assembly.code_editor`) **without loading a ROM**, plus session-loop support so those editors initialize and tick on an empty active session.
- Added a guided New Project flow for startup/project creation.
- Added async ROM metadata scanning for recent projects with safer first-scan behavior.
- Added recent-project pin/rename/notes actions and a short undo window for removals.
- Surfaced welcome/startup actions through the command palette.

### Dungeon Editor Parity & Polish
- Restored BG1/BG2 layout routing parity while preserving pit/mask behavior.
- Fixed single-tile `0x034` payload handling for real-room object rendering.
- Added replay-geometry-driven selection bounds for more accurate editor hitboxes.
- Added ROM-backed dungeon render parity tests and room snapshot coverage.
- Simplified dungeon workbench inspector/navigation flows and kept hidden room state sparse.
- Added lazy room materialization and released unused room buffer textures.

### Editor Memory & Startup Footprint
- Added lazy session-editor construction to reduce initial startup cost.
- Deferred hidden full-mode asset loads.
- Trimmed eager overworld bitmap memory.
- Split render-target texture creation for cleaner renderer/backend handling.

### CLI / CI
- Fixed the WASM build path for dungeon tile rows.
- Added overworld map ID validation before ROM access.
- Hardened Linux GUI smoke-run path handling.

### Developer / codebase (no user-facing behavior change)
- Reorganized `src/app/editor/` for clarity: `registry/` (ContentRegistry, undo, events), `shell/` (global UI chrome), `system/{workspace,session,commands}/` (matches CMake split), Oracle workflow UI under `hack/oracle/ui/`, domain editors under `*/ui/` with legacy dungeon/music/agent windows co-located under `*/ui/window/`.
- Added `ContentRegistry::WindowContents` as an alias of `Panels` and `REGISTER_WINDOW_CONTENT` as a shim over `REGISTER_PANEL` for preferred naming.

### Deferred to 0.8.0
- z3dk integration planning covers embedded assembly/lint/LSP workflows, shared Mesen2 plumbing, and `.mlb` export.

## 0.7.0 (March 2026)

### iOS Remote Control & Review
- Added Bonjour discovery (`_yaze._tcp.`) for automatic LAN desktop detection from iPad.
- Added Remote Room Viewer: browse all 296 dungeon rooms with overlay toggles, scale control, and metadata inspection.
- Added Remote Command Runner: execute z3ed CLI commands from iPad with autocomplete catalog, command history, and `--write` safety confirmation.
- Added Annotation Review Mode: browse rooms with annotation overlays, create/edit/delete annotations with REST-based desktop sync.
- Added Desktop Connection view with discovered hosts, manual IP entry, and persistent connection status pill in overlay bar.

### Desktop HTTP API
- Added `POST /api/v1/command/execute` for remote command execution via CommandRegistry.
- Added `GET /api/v1/command/list` exposing the full z3ed command catalog with metadata.
- Added `GET/POST/PUT/DELETE /api/v1/annotations` for annotation CRUD against project `annotations.json`.
- Added `BonjourPublisher` for macOS `dns_sd.h` service advertisement with TXT metadata.

### Editor Completion & Workflow Polish
- Added sprite editor undo/redo via snapshot actions with unit coverage.
- Added screen editor undo/redo for dungeon-map edits and restored toolbar wiring.
- Added message editor replace + replace-all operations.
- Completed music tracker stubs: Space key rest/key-off insertion, anchored range delete, and song rename popup.

### Dungeon Editor Improvements
- Added `DungeonUsageTracker` visual usage grids for blockset/spriteset/palette analysis.
- Polished dungeon workbench/panel workflow (mode clarity, status badge, and return affordances).
- Extended undo/redo coverage into adjacent dungeon map edit flows.

### Custom ROM Hack Features
- Added desktop BPS patch export/import with deterministic CRC validation for patch-based distribution.
- Improved overworld hack editing reliability with hard-delete semantics, list/filter/sort + duplicate/nudge iteration flow, and undo-backed batch flows.

### CLI
- Added `palette-get-colors` for palette dump/report output.
- Added `palette-set-color` for direct palette writes (legacy write mode is
  disabled on the 0.8 development line; preview remains available).
- Added `palette-analyze` for palette usage analysis.

### Themed Widget System
- Added `BeginThemedTabBar`/`EndThemedTabBar` for consistent styled tab bars.
- Adopted themed widget APIs across dungeon workbench, status bar, pixel editor, and screen editor.

### Deferred to 0.8.0
- Persistent overworld scratch-pad storage in project bundles.
- Overworld eyedropper tooling.
- Graphics editor clipboard operations.
- Music editor SPC/MML import path.
- Final dungeon draw-routine registry dedupe + ownership documentation cleanup.

## 0.6.2 (February 2026)

### Release Consistency
- Updated current-release metadata and docs markers to align all surfaces on `0.6.2`.
- Kept build/version source-of-truth flow pinned to the `VERSION` file.

### Project Bundles (`.yazeproj`)
- Improved unpack failure cleanup behavior and dry-run structural validation checks.
- Expanded bundle verification paths and hash compatibility handling.

### Oracle Tooling & Validation
- Continued smoke/preflight flow hardening for Oracle development and CI workflows.

### Editor UX
- Continued polish on dungeon placement feedback and tile selector interaction UX.

## 0.6.1 (February 2026)

### Oracle Tooling & Validation
- Added Oracle workflow command surfaces for smoke/preflight-style validation in `z3ed`.
- Added D6 minecart structural regression gating via `--min-d6-track-rooms`.
- Improved Oracle validation reporting with clearer structural/readiness status fields.

### Project Bundles (`.yazeproj`)
- Added project bundle verify command with structured checks and report output.
- Added project bundle pack/unpack commands with zip archive support.
- Hardened unpack behavior:
  - path traversal protection,
  - default cleanup on invalid bundle extraction failure,
  - `--keep-partial-output` for debugging extraction failures,
  - `--dry-run` structural validation mode.
- Added bundle help/registry examples for faster CLI discovery.

### Hashing & Integrity
- Replaced platform-conditional SHA1 behavior with portable SHA1 generation.
- Added ROM-hash verification path (`--check-rom-hash`) for bundle validation.
- Normalized hash comparisons (whitespace/case tolerant) for manifest compatibility.

### Editor UX
- Added limit-aware placement feedback for dungeon object/sprite/door workflows.
- Added custom object overlay controls and D6 quick-navigation affordances in the dungeon workbench.
- Improved tile selector usability:
  - hover preview tooltip,
  - jump-to-tile/filter bar,
  - range validation feedback,
  - explicit decimal input mode (`d:<id>`) while preserving hex-default behavior.

## 0.6.0 (February 2026)

### Undo/Redo
- Unified `UndoManager` embedded in `Editor` base class.
- Per-editor migrations: overworld, dungeon, graphics, music, message.
- Custom collision and water fill undo actions in dungeon editor.

### Dungeon Editor
- SNES priority compositing with coverage masks for transparent overwrites.
- Entity drag-drop with rich selection inspector.
- Mutation tagging by domain (tile objects, custom collision, water fill).
- Custom collision editor with JSON import/export.
- Water fill zone authoring with brush radius support.
- **Object Drawing Parity**: 100% vanilla object routine coverage (448/448 objects across subtypes 1/2/3).
  - Filled mapping gaps for objects 0xF8, 0xFE, 0xFF.
  - Room effects expanded: `ApplyRoomEffect()` handles Moving_Water, Moving_Floor, Torch_Show_Floor, Red_Flashes, Ganon_Room with appropriate layer blend modes.
  - SNES color math translucent blending in `CompositeToOutput()`: palette lookup, `(bg1_rgb + bg2_rgb) / 2`, nearest-color-in-bank for palette-indexed output.
  - 19 parity validation tests covering: routine coverage, palette offsets, pit/mask object identification, BothBG flag correctness, water object layer semantics, room effect blend modes, layer merge types, and drawer fallback behavior.

### UI
- Semantic color system replacing hardcoded ImGui style pushes.
- EventBus migration replacing legacy callback-based navigation.
- Right panel manager and sidebar simplification.
- Viewport-relative sizing helpers (`DialogSize`, `ConstrainToViewport`, `ScaledSize`).
- Warning button colors and status color standardization.

### ROM Safety
- Write fence stack rejecting negative/out-of-bounds writes.
- Dirty custom collision save without full room reload.
- Expanded message and service writes are fence-aware.

### Cleanup
- Removed `SessionObserver` and observer pattern from `SessionCoordinator`.
- Removed deprecated `WorkspaceWindowManager` callback setters (~79 lines).
- Removed legacy editor navigation APIs (`JumpTo*`, `HideCurrentEditorPanels`).
- Removed deprecated `SetMutationHook` alias.
- Migrated sidebar/session actions to EventBus.

---

## 0.5.6 (February 2026)

### Dungeon & Minecart Tooling
- Add configurable minecart collision/track IDs via project `[dungeon_overlay]`.
- Add track collision overlay with per-tile direction arrows + legend.
- Add minecart sprite overlay to flag carts off stop tiles.
- Expand Minecart Track Editor audit (32 slots, filler warnings, room coverage reporting).
- Add camera quadrant overlay for dungeon layout planning.

### Object Editing UX
- Custom object previews keyed by subtype; selection bounds use custom extents.
- Hover/selection respects active layer filters.

### Testing & Stability
- Harden headless ImGui initialization for editor tests.

---

## 0.5.5 (January 2026)

### Editor & Architecture
- **EditorManager Refactor**: Modernized `EditorManager` for better testability and isolation; introduced `yaze_core_lib` to separate core logic from the app shell.
- **Improved Testing**: Added `AsarCompilerTest` and `EditorManagerTest` suites.
- **Robust Graphics Loading**: Added fallback to grayscale palette for graphics sheets missing a palette, preventing crashes.
- **Build System**: Cleaned up CMake entry points and presets; unified `main` entry point logic.

---

## 0.5.4 (January 2026)

### Debugging & Mesen2
- Add Mesen2 debug panel in the Agent editor (socket picker, overlay controls, save/load state, screenshot capture).
- Add Mesen2 ↔ Yaze bridge endpoints + updated Lua bridge (symbol sync, PC navigation, breakpoint notifications, state telemetry).
- Add z3ed `mesen-*` CLI tools for live Mesen2 inspection (state, sprites, CPU, memory, trace, breakpoints).
- Add Mesen2 debug shortcut (Ctrl+Shift+M) and auto-refresh socket list on panel open.
- Harden socket client response handling + JSON escaping.

### AI, HTTP API & CLI
- Add ModelRegistry caching + `refresh` query support for `/api/v1/models`.
- Normalize OpenAI base URLs and auto-detect local OpenAI-compatible endpoints.
- Add CORS/error handling for HTTP API endpoints and `/symbols` format validation.
- Add `rom` and `debug` CLI routing plus sandbox flag for safe ROM edits.

### Emulator & gRPC Stability
- Use SNES Read/Write for gRPC memory operations to avoid crashes.
- Auto-initialize emulator on SaveState/LoadState when a ROM is loaded.
- Fall back to Null audio backend for headless init failures.

### Desktop & UX
- Fix message editor preview and font atlas palette sync on load.
- Sync panel/editor context on category switches to avoid blank ImGui windows (e.g., Emulator ↔ Overworld).

### Nightly & Packaging
- Normalize macOS nightly bundle layout so launchers find `yaze.app`.
- Align version strings across builds and docs.
- Move public headers to `inc/` and update build/install include paths.

---

## 0.5.3 (January 2026)

### Build & Release
- Fix release validation scripts for DMG packaging.
- Create VERSION file as canonical source of truth.
- Improve DMG validation diagnostics.

### AI & CLI
- Add LMStudio support with configurable `--openai_base_url` flag.
- Allow empty API key for local OpenAI-compatible servers.

### WASM/Web
- Add persistent directories for agent, proposals, themes, logs, etc.
- Merge PWA caching with COI service worker for unified root-scope caching.
- Add WASM stubs for build tools (return clear unavailable status).

---

## 0.5.2 (January 2026)

### Build
- Fix build when `YAZE_AI_RUNTIME` is disabled.
- Add proper guards around AI runtime-dependent code paths.

---

## 0.5.1 (January 2026)

### UX & Guidance
- Help panel now reflects configured shortcuts and adds shortcut search.
- Supported Features popup lists platform status + persistence details.
- User-facing warnings added for unimplemented collaboration and E2E menus.

### Storage & Paths
- App data consolidated under `~/.yaze` across desktop/CLI with legacy migration.
- Web build storage consolidated under `/.yaze` for ROMs/saves/projects.
- Agent chat, profiles, and sessions now persist under `.yaze/agent`.

### Project Management
- Storage locations and version mismatch warnings surfaced in the project panel.

### Versioning
- Added `VERSION` file as the build source of truth.
- Web version badge syncs to runtime WASM version.

---

## 0.5.0 (January 2026)

### Graphics & Palette Reliability
- Fixed palette conversion and Tile16 tint regressions.
- Corrected palette slicing for graphics sheets and indexed → SNES planar conversion.
- Stabilized overworld palette/tilemap saves and render service GameData loading.

### Editor UX & Tools
- Refined dashboard/editor selection layouts and card text rendering.
- Moved layout designer into a lab target for safer experimentation.
- Hardened CLI/API room loading and Asar patch handling.
- Refreshed welcome screen and help text across desktop/CLI/web to highlight multi-provider AI workflows.

### Automation & AI
- Added agent control server support and stabilized gRPC automation hooks.
- Expanded z3ed CLI test commands (`test-list`, `test-run`, `test-status`) and tool metadata.
- Improved agent command routing and help/schema surfacing for AI clients.
- Added OpenAI/Anthropic provider support in z3ed and refreshed AI provider docs/help.
- Introduced vision refiner/verification hooks for AI-assisted validation.

### Web/WASM Preview
- Reduced filesystem initialization overhead and fixed `/projects` directory handling.
- Hardened browser terminal integration and storage error reporting.

### Platform, Build, and Tests
- Added iOS platform scaffolding (experimental) plus build helper scripts.
- Simplified nightly workflow, refreshed toolchain/dependency wiring, and standardized build dirs.
- Added role-based ROM selection/availability gating and stabilized rendering/benchmark tests.
- Hardened Windows gRPC builds by forcing Win32 macro-compat includes for gRPC targets.
- Added AgentChat history/telemetry and agent metrics unit coverage; expanded WASM debug API checks.
- Fixed Linux static link order for test suites and tightened Abseil linkage.
- Release artifacts now include a release README and omit internal test helper utilities.
- Windows ships as a portable zip (no installer) with trimmed runtime DLLs.

---

## 0.4.1 (December 2025)

### Overworld Editor Fixes

**Vanilla ROM Corruption Fix**:
- Fixed critical bug where save functions wrote to custom ASM address space (0x140000+) for ALL ROMs without version checking
- `SaveAreaSpecificBGColors()`, `SaveCustomOverworldASM()`, and `SaveDiggableTiles()` now properly check ROM version before writing
- Vanilla and v1 ROMs are no longer corrupted by writes to ZSCustomOverworld address space
- Added `OverworldVersionHelper` with `SupportsCustomBGColors()` and `SupportsAreaEnum()` methods

**Toolbar UI Improvements**:
- Increased button widths from 30px to 40px for comfortable touch targets
- Added version badge showing "Vanilla", "v2", or "v3" ROM version with color coding
- Added "Upgrade" button for applying ZSCustomOverworld ASM patch to vanilla ROMs
- Improved panel toggle button spacing and column layout

### Testing Infrastructure

**ROM Auto-Discovery**:
- Tests now automatically discover ROMs in common locations (roms/, ../roms/, etc.)
- Searches for common vanilla filenames: alttp_vanilla.sfc, Legend of Zelda, The - A Link to the Past (USA).sfc
- Legacy environment variable `YAZE_TEST_ROM_PATH` is still supported as a fallback

**Overworld Regression Tests**:
- Added 9 new regression tests for save function version checks
- Tests verify vanilla/v1/v2/v3 ROM handling for all version-gated save functions
- Version feature matrix validation tests added

### Logging & Diagnostics
- Added CLI controls for log level/categories and console mirroring (`--log_level`, `--log_categories`, `--log_to_console`); `--debug` now force-enables console logging at debug level.
- Startup logging now reports the resolved level, categories, and log file destination for easier reproducibility.

### Editor & Panel Launch Controls
- `--open_panels` matching is case-insensitive and accepts both display names and stable panel IDs (e.g., `dungeon.room_list`, `Room 105`, `welcome`, `dashboard`).
- New startup visibility overrides (`--startup_welcome`, `--startup_dashboard`, `--startup_sidebar`) let you force panels to show/hide on launch for automation or demos.
- Welcome and dashboard behavior is coordinated through the UI layer so CLI overrides and in-app toggles stay in sync.

### Documentation & Testing
- Debugging guides refreshed with the new logging filters and startup panel controls.
- Startup flag reference and dungeon editor guide now use panel terminology and up-to-date CLI examples for automation setups.

---

## 0.3.9 (November 2025)

### AI Agent Infrastructure

**Semantic Inspection API**:
- New `SemanticIntrospectionEngine` class providing structured game state access for AI agents
- JSON output format optimized for LLM consumption: player state, sprites, location, game mode
- Comprehensive name lookup tables: 243+ ALTTP sprite types, 128+ overworld areas, 27 game modes
- Methods: `GetSemanticState()`, `GetStateAsJson()`, `GetPlayerState()`, `GetSpriteStates()`
- Ready for multimodal AI integration with visual grounding support

### Emulator Accuracy

**PPU JIT Catch-up System**:
- Implemented mid-scanline raster effect support via progressive rendering
- `StartLine()` and `CatchUp()` methods enable cycle-accurate PPU emulation
- Integrated into `WriteBBus` for immediate register change rendering
- Enables proper display of H-IRQ effects (Tales of Phantasia, Star Ocean)
- 19 comprehensive unit tests covering all edge cases

**Dungeon Sprite Encoding**:
- Complete sprite save functionality for dungeon rooms
- Proper ROM format encoding with layer and subtype support
- Handles sprite table pointer lookups correctly

### Editor Fixes

**Tile16 Palette System**:
- Fixed Tile8 source canvas showing incorrect colors
- Fixed palette buttons 0-7 not switching palettes correctly
- Fixed color alignment inconsistency across canvases
- Added `GetPaletteBaseForSheet()` for correct palette region mapping
- Palettes now properly use `SetPaletteWithTransparent()` with sheet-based offsets

### Documentation

**SDL3 Migration Plan**:
- Comprehensive migration plan document (58-62 hour estimate)
- Complete audit of SDL2 usage across all subsystems
- Identified existing abstraction layers (IAudioBackend, IInputBackend, IRenderer)
- 5-phase migration strategy for v0.4.0

**v0.4.0 Initiative Documentation**:
- Created initiative tracking document for SDL3 modernization
- Defined milestones, agent assignments, and success criteria
- Parallel workstream coordination protocol

---

## 0.3.2 (October 2025)

### AI Agent Infrastructure
**z3ed CLI Agent System**:
- **Conversational Agent Service**: Full chat integration with learned knowledge, TODO management, and context injection
- **Emulator Debugging Service**: 20/24 gRPC debugging methods for AI-driven emulator debugging
  - Breakpoint management (execute, read, write, access)
  - Step execution (single-step, run to breakpoint)
  - Memory inspection (read/write WRAM and hardware registers)
  - CPU state capture (full 65816 registers + flags)
  - Performance metrics (FPS, cycles, audio queue)
- **Command Registry**: Unified command architecture eliminating duplication across CLI/agent systems
- **Learned Knowledge Service**: Persistent preferences, ROM patterns, project context, and conversation memory
- **TODO Manager**: Task tracking with dependencies, execution plan generation, and priority-based scheduling
- **Advanced Router**: Response synthesis and enhancement with data type inference
- **Agent Pretraining**: ROM structure knowledge injection and tool usage examples

**Impact Metrics**:
- Debugging Time: 3+ hours → 15 minutes (92% faster)
- Code Iterations: 15+ rebuilds → 1-2 tool calls (93% fewer)
- AI Autonomy: 30% → 85% (2.8x better)
- Session Continuity: None → Full memory (∞% better)

**Documentation**: 2,000+ lines of comprehensive guides and real-world examples

### CI/CD & Release Improvements

**Release Workflow Fixes**:
- Fixed build matrix artifact upload issues (platform-specific uploads for Windows/Linux/macOS)
- Corrected macOS universal binary merge process with proper artifact paths
- Enhanced release to only include final artifacts (no intermediate build slices)
- Improved build diagnostics and error reporting across all platforms

**CI/CD Pipeline Enhancements**:
- Added manual workflow trigger with configurable build types and options
- Implemented vcpkg caching for faster Windows builds
- Enhanced Windows diagnostics (vcpkg status, Visual Studio info, disk space monitoring)
- Added Windows-specific build failure analysis (linker errors, missing dependencies)
- Conditional artifact uploads for CI builds with configurable retention
- Comprehensive job summaries with platform-specific information

### Rendering Pipeline Fixes

**Graphics Editor White Sheets Fixed**:
- Graphics sheets now receive appropriate default palettes during ROM loading
- Sheets 0-112: Dungeon main palettes, Sheets 113-127: Sprite palettes, Sheets 128-222: HUD/menu palettes
- Eliminated white/blank graphics on initial load

**Message Editor Preview Updates**:
- Fixed static message preview issue where changes weren't visible
- Corrected `mutable_data()` usage to `set_data()` for proper SDL surface synchronization
- Message preview now updates in real-time when selecting or editing messages

**Cross-Editor Graphics Synchronization**:
- Added `Arena::NotifySheetModified()` for centralized texture management
- Graphics changes in one editor now propagate to all other editors
- Replaced raw `printf()` calls with structured `LOG_*` macros throughout graphics pipeline

### Card-Based UI System

**EditorCardManager**:
- Centralized card registration and visibility management
- Context-sensitive card controls in main menu bar
- Category-based keyboard shortcuts (Ctrl+Shift+D for Dungeon, Ctrl+Shift+B for browser)
- Card browser for visual card management

**Editor Integration**:
- All major editors (Dungeon, Graphics, Screen, Sprite, Overworld, Assembly, Message, Emulator) now use card system
- Cards can be closed with X button, proper docking behavior across all editors
- Cards hidden by default to prevent crashes on ROM load

### Tile16 Editor & Graphics System

**Palette System Enhancements**:
- Comprehensive palette coordination with overworld palette system
- Sheet-based palette mapping (Sheets 0,3-6: AUX; Sheets 1-2: MAIN; Sheet 7: ANIMATED)
- Enhanced scrollable UI layout with right-click tile picking
- Save/discard workflow preventing ROM changes until explicit user action

**Performance & Stability**:
- Fixed segmentation faults caused by tile cache `std::move()` operations invalidating Bitmap surface pointers
- Disabled problematic tile cache, implemented direct SDL texture updates
- Added comprehensive bounds checking to prevent palette crashes
- Implemented surface/texture pooling and performance profiling

### Windows Platform Stability

**Build System Fixes**:
- Increased Windows stack size from 1MB to 8MB (matches macOS/Linux defaults)
- Fixed linker errors in development utilities (`extract_vanilla_values`, `rom_patch_utility`)
- Implemented Windows COM-based file dialog fallback for minimal builds
- Consistent cross-platform behavior and stack resources

### Emulator: Audio System Infrastructure

**Audio Backend Abstraction:**
- **IAudioBackend Interface**: Clean abstraction layer for audio implementations, enabling easy migration between SDL2, SDL3, and custom backends
- **SDL2AudioBackend**: Complete implementation with volume control, status queries, and smart buffer management (2-6 frames)
- **AudioBackendFactory**: Factory pattern for creating backends with minimal coupling
- **Benefits**: Future-proof audio system, easy to add platform-native backends (CoreAudio, WASAPI, PulseAudio)

**APU Debugging System:**
- **ApuHandshakeTracker**: Monitors CPU-SPC700 communication in real-time
- **Phase Tracking**: Tracks handshake progression (RESET → IPL_BOOT → WAITING_BBAA → HANDSHAKE_CC → TRANSFER_ACTIVE → RUNNING)
- **Port Activity Monitor**: Records last 1000 port write events with PC addresses
- **Visual Debugger UI**: Real-time phase display, port activity log, transfer progress bars, force handshake testing
- **Integration**: Connected to both CPU (Snes::WriteBBus) and SPC700 (Apu::Write) port operations

**Music Editor Integration:**
- **Live Playback**: `PlaySong(int song_id)` triggers songs via $7E012C memory write
- **Volume Control**: `SetVolume(float)` controls backend volume at abstraction layer
- **Playback Controls**: Stop/pause/resume functionality ready for UI integration

**Documentation:**
- Created comprehensive audio system guides covering IPL ROM protocol, handshake debugging, and testing procedures

### Emulator: Critical Performance Fixes

**Console Logging Performance Killer Fixed:**
- **Issue**: Console logging code was executing on EVERY instruction even when disabled, causing severe performance degradation (< 1 FPS)
- **Impact**: ~1,791,000 console writes per second with mutex locks and buffer flushes
- **Fix**: Removed 73 lines of console output from CPU instruction execution hot path
- **Result**: Emulator now runs at full 60 FPS

**Instruction Logging Default Changed:**
- **Changed**: `kLogInstructions` flag default from `true` to `false`
- **Reason**: Even without console spam, logging every instruction to DisassemblyViewer caused significant slowdown
- **Impact**: No logging overhead unless explicitly enabled by user

**Instruction Log Unbounded Growth Fixed:**
- **Issue**: Legacy `instruction_log_` vector growing to 60+ million entries after 10 minutes, consuming 6GB+ RAM
- **Fix**: Added automatic trimming to 10,000 most recent instructions
- **Result**: Memory usage stays bounded at ~50MB

**Audio Buffer Allocation Bug Fixed:**
- **Issue**: Audio buffer allocated as single `int16_t` instead of array, causing immediate buffer overflow
- **Fix**: Properly allocate as array using `new int16_t[size]` with custom deleter
- **Result**: Audio system can now queue samples without corruption

### Emulator: UI Organization & Input System

**New UI Architecture:**
- **Created `src/app/emu/ui/` directory** for separation of concerns
- **EmulatorUI Layer**: Separated all ImGui rendering code from emulator logic
- **Input Abstraction**: `IInputBackend` interface with SDL2 implementation for future SDL3 migration
- **InputHandler**: Continuous polling system using `SDL_GetKeyboardState()` instead of event-based ImGui keys

**Keyboard Input Fixed:**
- **Issue**: Event-based `ImGui::IsKeyPressed()` only fires once per press, doesn't work for held buttons
- **Fix**: New `InputHandler` uses continuous SDL keyboard state polling every frame
- **Result**: Proper game controls with held button detection

**DisassemblyViewer Enhancement:**
- **Sparse Address Map**: Mesen-style storage of unique addresses only, not every execution
- **Execution Counter**: Increments on re-execution for hotspot analysis
- **Performance**: Tracks millions of instructions with ~5MB RAM vs 6GB+ with old system
- **Always Active**: No need for toggle flag, efficiently active by default

**Feature Flags Cleanup:**
- Removed deprecated `kLogInstructions` flag entirely
- DisassemblyViewer now always active with zero performance cost

### Debugger: Breakpoint & Watchpoint Systems

**BreakpointManager:**
- **CRUD Operations**: Add/Remove/Enable/Disable breakpoints with unique IDs
- **Breakpoint Types**: Execute, Read, Write, Access, and Conditional breakpoints
- **Dual CPU Support**: Separate tracking for 65816 CPU and SPC700
- **Hit Counting**: Tracks how many times each breakpoint is triggered
- **CPU Integration**: Connected to CPU execution via callback system

**WatchpointManager:**
- **Memory Access Tracking**: Monitor reads/writes to memory ranges
- **Range-Based**: Watch single addresses or memory regions ($7E0000-$7E00FF)
- **Access History**: Deque-based storage of last 1000 memory accesses
- **Break-on-Access**: Optional execution pause when watchpoint triggered
- **Export**: CSV export of access history for analysis

**CPU Debugger UI Enhancements:**
- **Integrated Controls**: Play/Pause/Step/Reset buttons directly in debugger window
- **Breakpoint UI**: Address input (hex), add/remove buttons, enable/disable checkboxes, hit count display
- **Live Disassembly**: DisassemblyViewer showing real-time execution
- **Register Display**: Real-time CPU state (A, X, Y, D, SP, PC, PB, DB, flags)

### Build System Simplifications

**Eliminated Conditional Compilation:**
- **Before**: Optional flags for JSON (`YAZE_WITH_JSON`), gRPC (`YAZE_WITH_GRPC`), AI (`Z3ED_AI`)
- **After**: All features always enabled, no configuration required
- **Benefits**: Simpler development, easier onboarding, fewer ifdef-related bugs, consistent builds across all platforms
- **Build Command**: Just `cmake -B build && cmake --build build` - no flags needed!

**DisassemblyViewer Performance Limits:**
- Max 10,000 instructions stored (prevents memory bloat)
- Auto-trim to 8,000 when limit reached (keeps hottest code paths)
- Toggle recording on/off for performance testing
- Clear button to free memory

### Build System: Windows Platform Improvements

**gRPC v1.67.1 Upgrade:**
- **Issue**: v1.62.0 had template instantiation errors on MSVC
- **Fix**: Upgraded to v1.67.1 with MSVC template fixes and better C++17/20 compatibility
- **Result**: Builds successfully on Visual Studio 2022

**MSVC-Specific Compiler Flags:**
- `/bigobj` - Allow large object files (gRPC generates many)
- `/permissive-` - Standards conformance mode
- `/wd4267 /wd4244` - Suppress harmless conversion warnings
- `/constexpr:depth2048` - Handle deep template instantiations

**Cross-Platform Validation:**
- All new audio and input code uses cross-platform SDL2 APIs
- No platform-specific code in audio backend or input abstraction
- Ready for SDL3 migration with minimal changes

### GUI & UX Modernization
- **Theme System**: Implemented comprehensive theme system (`AgentUITheme`) centralizing all UI colors
- **UI Helper Library**: Created 30+ reusable UI helper functions reducing boilerplate code by over 50%
- **Visual Polish**: Enhanced UI panels with theme-aware colors, status badges, connection indicators

### Overworld Editor Refactoring
- **Modular Architecture**: Refactored 3,400-line `OverworldEditor` into smaller focused modules
- **Progressive Loading**: Implemented priority-based progressive loading in `gfx::Arena` to prevent UI freezes
- **Critical Graphics Fixes**: Resolved bugs with graphics refresh, multi-quadrant map updates, and feature visibility
- **Multi-Area Map Configuration**: Robust `ConfigureMultiAreaMap()` handling all area sizes

### Build System & Stability
- **Build Fixes**: Resolved 7 critical build errors including linker issues and filesystem crashes
- **C API Separation**: Decoupled C API library from main application for improved modularity

### Future Optimizations (Planned)

**Graphics System:**
- Lazy loading of graphics sheets (load on-demand rather than all at once)
- Heap-based allocation for large data structures instead of stack
- Streaming/chunked loading for large ROM assets
- Consider if all 223 sheets need to be in memory simultaneously

**Build System:**
- Further reduce CI build times
- Enhanced dependency caching strategies
- Improved vcpkg integration reliability

### Technical Notes

**Breaking Changes:**
- None - this is a patch release focused on stability and fixes

**Deprecations:**
- None

**Migration Guide:**
- No migration required - this release is fully backward compatible with 0.3.1

## 0.3.1 (September 2025)

### Major Features
- **Complete Tile16 Editor Overhaul**: Professional-grade tile editing with modern UI and advanced capabilities
- **Advanced Palette Management**: Full access to all SNES palette groups with configurable normalization
- **Comprehensive Undo/Redo System**: 50-state history with intelligent time-based throttling
- **ZSCustomOverworld v3 Full Support**: Complete implementation of ZScream Save.cs functionality with complex transition calculations
- **ZEML System Removal**: Converted overworld editor from markup to pure ImGui for better performance and maintainability
- **OverworldEditorManager**: New management system to handle complex v3 overworld features

### Tile16 Editor Enhancements
- **Modern UI Layout**: Fully resizable 3-column interface (Tile8 Source, Editor, Preview & Controls)
- **Multi-Palette Group Support**: Access to Overworld Main/Aux1/Aux2, Dungeon Main, Global Sprites, Armors, and Swords palettes
- **Advanced Transform Operations**: Flip horizontal/vertical, rotate 90°, fill with tile8, clear operations
- **Professional Workflow**: Copy/paste, 4-slot scratch space, live preview with auto-commit
- **Pixel Normalization Settings**: Configurable pixel value masks (0x01-0xFF) for handling corrupted graphics sheets

### ZSCustomOverworld v3 Implementation
- **SaveLargeMapsExpanded()**: Complex neighbor-aware transition calculations for all area sizes (Small, Large, Wide, Tall)
- **Interactive Overlay System**: Full `SaveMapOverlays()` with ASM code generation for revealing holes and changing map elements
- **SaveCustomOverworldASM()**: Complete custom overworld ASM application with feature toggles and data tables
- **Expanded Memory Support**: Automatic detection and use of v3 expanded memory locations (0x140xxx)
- **Area-Specific Features**: Background colors, main palettes, mosaic transitions, GFX groups, subscreen overlays, animated tiles
- **Transition Logic**: Sophisticated camera transition calculations based on neighboring area types and quadrants
- **Version Compatibility**: Maintains vanilla/v2 compatibility while adding full v3+ feature support

### Technical Improvements
- **SNES Data Accuracy**: Proper 4-bit palette index handling with configurable normalization
- **Bitmap Pipeline Fixes**: Corrected tile16 extraction using `GetTilemapData()` with manual fallback
- **Real-time Updates**: Immediate visual feedback for all editing operations
- **Memory Safety**: Enhanced bounds checking and error handling throughout
- **ASM Version Detection**: Automatic detection of custom overworld ASM version for feature availability
- **Conditional Save Logic**: Different save paths for vanilla, v2, and v3+ ROMs

### User Interface
- **Keyboard Shortcuts**: Comprehensive shortcuts for all operations (H/V/R for transforms, Q/E for palette cycling, 1-8 for direct palette selection)
- **Visual Feedback**: Hover preview restoration, current palette highlighting, texture status indicators
- **Compact Controls**: Streamlined property panel with essential tools easily accessible
- **Settings Dialog**: Advanced palette normalization controls with real-time application
- **Pure ImGui Layout**: Removed ZEML markup system in favor of native ImGui tabs and tables for better performance
- **v3 Settings Panel**: Dedicated UI for ZSCustomOverworld v3 features with ASM version detection and feature toggles

### Bug Fixes
- **Tile16 Bitmap Display**: Fixed blank/white tile issue caused by unnormalized pixel values
- **Hover Preview**: Restored tile8 preview when hovering over tile16 canvas
- **Canvas Scaling**: Corrected coordinate scaling for 8x magnification factor
- **Palette Corruption**: Fixed high-bit contamination in graphics sheets
- **UI Layout**: Proper column sizing and resizing behavior
- **Linux CI/CD Build**: Fixed undefined reference errors for `ShowSaveFileDialog` method
- **ZSCustomOverworld v3**: Fixed complex area transition calculations and neighbor-aware tilemap adjustments
- **ZEML Performance**: Eliminated markup parsing overhead by converting to native ImGui components

### ZScream Compatibility Improvements
- **Complete Save.cs Implementation**: All major methods from ZScream's Save.cs now implemented in YAZE
- **Area Size Support**: Full support for Small, Large, Wide, and Tall area types with proper transitions
- **Interactive Overlays**: Complete overlay save system matching ZScream's functionality  
- **Custom ASM Integration**: Proper handling of ZSCustomOverworld ASM versions 1-3+
- **Memory Layout**: Correct usage of expanded vs vanilla memory locations based on ROM type

## 0.3.0 (September 2025)

### Major Features
- **Complete Theme Management System**: 5+ built-in themes with custom theme creation and editing
- **Multi-Session Workspace**: Work with multiple ROMs simultaneously in enhanced docked interface
- **Enhanced Welcome Screen**: Themed interface with quick access to all editors and features
- **Asar 65816 Assembler Integration**: Complete cross-platform ROM patching with assembly code
- **ZSCustomOverworld v3**: Full integration with enhanced overworld editing capabilities
- **Advanced Message Editing**: Enhanced text editing interface with improved parsing and real-time preview
- **GUI Docking System**: Improved docking and workspace management for better user workflow
- **Symbol Extraction**: Extract symbol names and opcodes from assembly files
- **Modernized Build System**: Upgraded to CMake 3.16+ with target-based configuration

### User Interface & Theming
- **Built-in Themes**: Classic YAZE, YAZE Tre, Cyberpunk, Sunset, Forest, and Midnight themes
- **Theme Editor**: Complete custom theme creation with save-to-file functionality
- **Animated Background Grid**: Optional moving grid with color breathing effects
- **Theme Import/Export**: Share custom themes with the community
- **Responsive UI**: All UI elements properly adapt to selected themes

### Enhancements
- **Enhanced CLI Tools**: Improved z3ed with modern command line interface and TUI
- **CMakePresets**: Added development workflow presets for better productivity
- **Cross-Platform CI/CD**: Multi-platform automated builds and testing with lenient code quality checks
- **Professional Packaging**: NSIS, DMG, and DEB/RPM installers
- **ROM-Dependent Testing**: Separated testing infrastructure for CI compatibility with 46+ core tests
- **Comprehensive Documentation**: Updated guides, help menus, and API documentation

### Technical Improvements
- **Modern C++23**: Latest language features for performance and safety
- **Memory Safety**: Enhanced memory management with RAII and smart pointers
- **Error Handling**: Improved error handling using absl::Status throughout
- **Cross-Platform**: Consistent experience across Windows, macOS, and Linux
- **Performance**: Optimized rendering and data processing

### Bug Fixes
- **Graphics Arena Crash**: Fixed double-free error during Arena singleton destruction
- **SNES Tile Format**: Corrected tile unpacking algorithm based on SnesLab documentation
- **Palette System**: Fixed color conversion functions (ImVec4 float to uint8_t conversion)
- **CI/CD**: Fixed missing cstring include for Ubuntu compilation
- **ROM Loading**: Fixed file path issues in tests

## 0.2.2 (December 2024)

### Core Features
- DungeonMap editing improvements
- ZSCustomOverworld support
- Cross platform file handling

## 0.2.1 (August 2024)
- Improved MessageEditor parsing
- Added integration test window
- Bitmap bug fixes

## 0.2.0 (July 2024)
- iOS app support
- Graphics Sheet Browser
- Project Files

## 0.1.0 (May 2024)
- Bitmap bug fixes
- Error handling improvements

## 0.0.9 (April 2024)
- Documentation updates
- Entrance tile types
- Emulator subsystem overhaul

## 0.0.8 (February 2024)
- Hyrule Magic Compression
- Dungeon Room Entrances
- PNG Export

## 0.0.7 (January 2024)
- OverworldEntities
  - Entrances
  - Exits
  - Items
  - Sprites

## 0.0.6 (November 2023)
- ScreenEditor DungeonMap
- Tile16 Editor
- Canvas updates

## 0.0.5 (November 2023)
- DungeonEditor
- DungeonObjectRenderer

## 0.0.4 (November 2023)
- Tile16Editor
- GfxGroupEditor
- Add GfxGroups functions to Rom
- Add Tile16Editor and GfxGroupEditor to OverworldEditor

## 0.0.3 (October 2023)
- Emulator subsystem
  - SNES PPU and PPURegisters
