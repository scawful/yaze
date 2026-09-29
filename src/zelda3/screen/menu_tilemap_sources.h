#ifndef YAZE_APP_ZELDA3_SCREEN_MENU_TILEMAP_SOURCES_H
#define YAZE_APP_ZELDA3_SCREEN_MENU_TILEMAP_SOURCES_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "app/gfx/types/snes_color.h"

namespace yaze {
class Rom;
namespace emu::debug {
class SymbolProvider;
}
}  // namespace yaze

namespace yaze::zelda3 {

struct GameData;

// --- Palette sources (menu_tilemap.h's ComposeMenuTilemapRgba() wants a
// 32-entry std::array<gfx::SnesColor, 32> indexed by [subpalette*4+color])
// ---------------------------------------------------------------------

// Source (a): 32 SNES colors read from a ROM symbol's address, applying
// the menu upload's CGRAM +1 offset. Oracle of Secrets' Menu_UploadLeft
// copies its 32-word Menu_Palette table to CGRAM starting at color 1 (not
// color 0), so table[i] lands on CGRAM color i+1: sub-palette p, color c
// (c=1..3) = table[p*4 + c - 1]; color 0 of every sub-palette is always
// transparent and is intentionally never read from the table (see
// docs/public/usage/screen-editor.md for the derivation).
//
// IMPORTANT (Oracle of Secrets specifically): labels like
// "Oracle_Menu_Palette" are produced by asar assembling the ASM sources
// (Menu/menu_palette.asm et al.) and only exist in the *patched* ROM
// (oos168x.sfc in the two-ROM build pipeline), not in the "clean" base
// ROM that a yaze project normally points at (oos168.sfc, where
// ZScream/yaze-authored room/overworld/graphics data lives). If `rom` is
// the base ROM, the resolved address typically reads back all zero bytes;
// this function detects that case and returns a descriptive error instead
// of silently returning a black palette, naming the label and suggesting
// the patched ROM or an explicit CGRAM dump / .pal file source instead.
absl::StatusOr<std::array<gfx::SnesColor, 32>> ResolveMenuPaletteFromSymbol(
    const Rom& rom, const emu::debug::SymbolProvider& symbols,
    const std::string& label);

// Source (b): palette_groups.hud[0], the 32-color HUD palette group
// GameData already loads from the base ROM. This is vanilla ALTTP's HUD
// palette (or whatever static replacement a project has written into
// that ROM location via yaze/ZScream); Oracle's *runtime* HUD palette
// patches at $1BD662 etc. are ASM-only and are not reflected here.
absl::StatusOr<std::array<gfx::SnesColor, 32>> ResolveMenuPaletteFromHud(
    const GameData& game_data);

// Source (c): a raw CGRAM dump (512 bytes = 256 SNES colors) or a plain
// .pal file (concatenated little-endian 16-bit SNES color words). Reads
// 32 colors starting at `byte_offset` (default 2, i.e. CGRAM color 1 --
// the first color menu uploads actually land on, matching the +1 offset
// used by ResolveMenuPaletteFromSymbol so swapping sources doesn't shift
// the palette).
absl::StatusOr<std::array<gfx::SnesColor, 32>> ResolveMenuPaletteFromFile(
    const std::string& path, size_t byte_offset = 2);

// Source (d): Oracle of Secrets' Menu/menu_palette.asm parsed directly, so
// the palette is available even when the project ROM is the *base* ROM
// (oos168.sfc), where the assembled Menu_Palette table reads all zero
// (see the note on source (a)). Same +1 CGRAM mapping as source (a).
//
// Accepted grammar after the `Menu_Palette:` label (CRLF or LF, `;`
// comments, blank lines, one or several comma-separated operands per
// `dw`):
//   dw hexto555($RRGGBB)   -> Hexto555() (menu_palette.asm's macro)
//   dw $XXXX / 0xXXXX / N  -> that raw 16-bit BGR555 word
// The table ends at the first line that is not a `dw`, blank or comment,
// or after 32 entries. At least 31 are required (8 sub-palettes x 3
// colors + the unused final slot; the real file defines exactly 31).
// Errors carry the 1-based line number ("line N: ...").

// $RRGGBB -> SNES BGR555, exactly menu_palette.asm's
//   function hexto555(h) = (((h&$FF)/8)<<10)|(((h>>8&$FF)/8)<<5)|
//                          (((h>>16&$FF)/8)<<0)
// i.e. each 8-bit channel is divided by 8 (floor) and packed B<<10 | G<<5
// | R. Example: $814f16 -> 0x0930, $f9f9f9 -> 0x7FFF.
uint16_t Hexto555(uint32_t rgb);

// Pure text parser (no I/O) for the table under `label` (default
// "Menu_Palette"). Returns the raw BGR555 words (31 or 32 of them).
absl::StatusOr<std::vector<uint16_t>> ParseMenuPaletteAsmText(
    std::string_view text, std::string_view label = "Menu_Palette");

// Reads `path` and applies ParseMenuPaletteAsmText(). If `label` starts
// with "Oracle_" (the symbol-file spelling, e.g. "Oracle_Menu_Palette")
// and is not found, retries with that prefix stripped, so one label
// field can drive both this source and the symbol source. Parse errors
// are prefixed with the file path ("<path>: line N: ...").
absl::StatusOr<std::array<gfx::SnesColor, 32>> ResolveMenuPaletteFromAsm(
    const std::string& path, const std::string& label = "Menu_Palette");

// Looks for `Menu/menu_palette.asm` under a project: first
// <project_dir>/Menu/, then <project_dir>/<code_folder>/Menu/ (if
// code_folder is set and relative; an absolute code_folder is tried as
// is). Returns the first existing regular file.
std::optional<std::string> FindMenuPaletteAsm(
    const std::string& project_dir, const std::string& code_folder = "");

// --- CHR (tile graphics) sources ---------------------------------------

// Source (a), default: the ROM's 2bpp sheets the game puts in BG3 while
// the item/menu screen is open (zelda3::Load2BppGraphics: sheets 0x71,
// 0x72, 0xDA-0xDE, concatenated into 128-tile chunks). These are
// ZScream/yaze-managed compressed graphics sheets, not ASM-injected, so
// (unlike the palette symbol above) they are present in Oracle of
// Secrets' base ROM (oos168.sfc) as well as the patched one.
absl::StatusOr<std::vector<uint8_t>> ResolveMenuChrFromRom(const Rom& rom);

// Source (b): a raw 2bpp .bin chr file or VRAM dump, decoded into the
// same 128-tile-chunk layout as ResolveMenuChrFromRom() so both can be
// used interchangeably with zelda3::MakeChrPixelFn().
absl::StatusOr<std::vector<uint8_t>> ResolveMenuChrFromFile(
    const std::string& path);

}  // namespace yaze::zelda3

#endif  // YAZE_APP_ZELDA3_SCREEN_MENU_TILEMAP_SOURCES_H
