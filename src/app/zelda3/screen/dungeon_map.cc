#include "dungeon_map.h"

#include <fstream>
#include <vector>

#include "app/core/platform/file_dialog.h"
#include "app/core/platform/renderer.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace zelda3 {

absl::Status LoadDungeonMapGfxFromBinary(Rom &rom,
                                         std::array<gfx::Bitmap, 4> &sheets,
                                         gfx::Tilesheet &tile16_sheet,
                                         std::vector<uint8_t> &gfx_bin_data) {
  std::string bin_file = core::FileDialogWrapper::ShowOpenFileDialog();
  if (bin_file.empty()) {
    return absl::InternalError("No file selected");
  }

  std::ifstream file(bin_file, std::ios::binary);
  if (file.is_open()) {
    // Read the gfx data into a buffer
    std::vector<uint8_t> bin_data((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
    auto converted_bin = gfx::SnesTo8bppSheet(bin_data, 4, 4);
    gfx_bin_data = converted_bin;
    tile16_sheet.clear();
    if (LoadDungeonMapTile16(converted_bin, true).ok()) {
      std::vector<std::vector<uint8_t>> gfx_sheets;
      for (int i = 0; i < 4; i++) {
        gfx_sheets.emplace_back(converted_bin.begin() + (i * 0x1000),
                                converted_bin.begin() + ((i + 1) * 0x1000));
        sheets[i] = gfx::Bitmap(128, 32, 8, gfx_sheets[i]);
        sheets[i].SetPalette(*rom.mutable_dungeon_palette(3));
        core::Renderer::Get().RenderBitmap(&sheets[i]);
      }
    } else {
      return absl::InternalError("Failed to load dungeon map tile16");
    }
    file.close();
  }

  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
