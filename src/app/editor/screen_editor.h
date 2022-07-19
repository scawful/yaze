#ifndef YAZE_APP_EDITOR_SCREEN_EDITOR_H
#define YAZE_APP_EDITOR_SCREEN_EDITOR_H

#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {
namespace editor {

class ScreenEditor {
 public:
  void Update();

 private:
  int sword_x_ = 0;
  int mx_click_ = 0;
  int my_click_ = 0;
  int mx_dist_ = 0;
  int my_dist_ = 0;
  int last_x_ = 0;
  int last_y_ = 0;
  int x_in_ = 0;
  int y_in_ = 0;
  int dungmap_selected_tile_ = 0;
  int dungmap_selected_ = 0;
  int selected_palette_ = 0;
  int total_floors_ = 0;
  int current_floor_ = 0;
  int num_basement_ = 0;
  int num_floor_ = 0;
  int selected_map_tile = 0;
  int current_floor_rooms;  // [1][];
  int current_floor_gfx;    // [1][];
  int copied_data_rooms;    // 25
  int copied_data_gfx;      // 25
  int addresses[] = {0x53de4, 0x53e2c, 0x53e08, 0x53e50,
                     0x53e74, 0x53e98, 0x53ebc};
  int addressesgfx[] = {0x53ee0, 0x53f04, 0x53ef2, 0x53f16,
                        0x53f28, 0x53f3a, 0x53f4c};

  ushort bossRoom = 0x000F;
  ushort selected_tile = 0;
  ushort tilesBG1Buffer = new ushort[0x1000];
  ushort tilesBG2Buffer = new ushort[0x1000];
  uchar mapdata = new uchar[64 * 64];
  uchar dwmapdata = new uchar[64 * 64];

  bool mDown = false;
  bool swordSelected = false;
  bool darkWorld = false;
  bool currentDungeonChanged = false;
  bool editedFromEditor = false;
  bool mouseDown = false;
  bool mdown = false;

  std::vector<MapIcon> all_map_icons_;

  OAMTile oam_data[10];
  OAMTile selected_oam_tile = nullptr;
  OAMTile last_selected_oam_tile = nullptr;

  gfx::Bitmap tilesBG1Bitmap;  // 0x80000
  gfx::Bitmap tilesBG2Bitmap;  // 0x80000
  gfx::Bitmap oamBGBitmap;     // 0x80000

  gfx::Bitmap dungeon_map_tiles8_bmp;  // 0x8000
  gfx::Bitmap dungmaptiles16Bitmap;    // 0x20000
  gfx::Bitmap tiles8Bitmap;            // 0x20000
  gfx::Bitmap floor_selector;

  //   DungeonMap dungeon_maps_[14];
  //   MapIcon selectedMapIcon;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif