#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_TAG_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_TAG_H

#include <string>

namespace yaze {
namespace zelda3 {

enum CollisionKey {
  One_Collision,
  Both,
  Both_With_Scroll,
  Moving_Floor_Collision,
  Moving_Water_Collision,
};

enum EffectKey {
  Effect_Nothing,
  One,
  Moving_Floor,
  Moving_Water,
  Four,
  Red_Flashes,
  Torch_Show_Floor,
  Ganon_Room,
};

static const std::string RoomEffect[] = {"Nothing",
                                         "Nothing",
                                         "Moving Floor",
                                         "Moving Water",
                                         "Trinexx Shell",
                                         "Red Flashes",
                                         "Light Torch to See Floor",
                                         "Ganon's Darkness"};
enum TagKey {
  Nothing,
  NW_Kill_Enemy_to_Open,
  NE_Kill_Enemy_to_Open,
  SW_Kill_Enemy_to_Open,
  SE_Kill_Enemy_to_Open,
  W_Kill_Enemy_to_Open,
  E_Kill_Enemy_to_Open,
  N_Kill_Enemy_to_Open,
  S_Kill_Enemy_to_Open,
  Clear_Quadrant_to_Open,
  Clear_Room_to_Open,
  NW_Push_Block_to_Open,
  NE_Push_Block_to_Open,
  SW_Push_Block_to_Open,
  SE_Push_Block_to_Open,
  W_Push_Block_to_Open,
  E_Push_Block_to_Open,
  N_Push_Block_to_Open,
  S_Push_Block_to_Open,
  Push_Block_to_Open,
  Pull_Lever_to_Open,
  Clear_Level_to_Open,
  Switch_Open_Door_Hold,
  Switch_Open_Door_Toggle,
  Turn_off_Water,
  Turn_on_Water,
  Water_Gate,
  Water_Twin,
  Secret_Wall_Right,
  Secret_Wall_Left,
  Crash1,
  Crash2,
  Pull_Switch_to_bomb_Wall,
  Holes_0,
  Open_Chest_Activate_Holes_0,
  Holes_1,
  Holes_2,
  Kill_Enemy_to_clear_level,
  SE_Kill_Enemy_to_Move_Block,
  Trigger_activated_Chest,
  Pull_lever_to_Bomb_Wall,
  NW_Kill_Enemy_for_Chest,
  NE_Kill_Enemy_for_Chest,
  SW_Kill_Enemy_for_Chest,
  SE_Kill_Enemy_for_Chest,
  W_Kill_Enemy_for_Chest,
  E_Kill_Enemy_for_Chest,
  N_Kill_Enemy_for_Chest,
  S_Kill_Enemy_for_Chest,
  Clear_Quadrant_for_Chest,
  Clear_Room_for_Chest,
  Light_Torches_to_Open,
  Holes_3,
  Holes_4,
  Holes_5,
  Holes_6,
  Agahnim_Room,
  Holes_7,
  Holes_8,
  Open_Chest_for_Holes_8,
  Push_Block_for_Chest,
  Kill_to_open_Ganon_Door,
  Light_Torches_to_get_Chest,
  Kill_boss_Again
};

static const std::string RoomTag[] = {"Nothing",
                                      "NW Kill Enemy to Open",
                                      "NE Kill Enemy to Open",
                                      "SW Kill Enemy to Open",
                                      "SE Kill Enemy to Open",
                                      "W Kill Enemy to Open",
                                      "E Kill Enemy to Open",
                                      "N Kill Enemy to Open",
                                      "S Kill Enemy to Open",
                                      "Clear Quadrant to Open",
                                      "Clear Full Tile to Open",

                                      "NW Push Block to Open",
                                      "NE Push Block to Open",
                                      "SW Push Block to Open",
                                      "SE Push Block to Open",
                                      "W Push Block to Open",
                                      "E Push Block to Open",
                                      "N Push Block to Open",
                                      "S Push Block to Open",
                                      "Push Block to Open",
                                      "Pull Lever to Open",
                                      "Collect Prize to Open",

                                      "Hold Switch Open Door",
                                      "Toggle Switch to Open Door",
                                      "Turn off Water",
                                      "Turn on Water",
                                      "Water Gate",
                                      "Water Twin",
                                      "Moving Wall Right",
                                      "Moving Wall Left",
                                      "Crash",
                                      "Crash",
                                      "Push Switch Exploding Wall",
                                      "Holes 0",
                                      "Open Chest (Holes 0)",
                                      "Holes 1",
                                      "Holes 2",
                                      "Defeat Boss for Dungeon Prize",

                                      "SE Kill Enemy to Push Block",
                                      "Trigger Switch Chest",
                                      "Pull Lever Exploding Wall",
                                      "NW Kill Enemy for Chest",
                                      "NE Kill Enemy for Chest",
                                      "SW Kill Enemy for Chest",
                                      "SE Kill Enemy for Chest",
                                      "W Kill Enemy for Chest",
                                      "E Kill Enemy for Chest",
                                      "N Kill Enemy for Chest",
                                      "S Kill Enemy for Chest",
                                      "Clear Quadrant for Chest",
                                      "Clear Full Tile for Chest",

                                      "Light Torches to Open",
                                      "Holes 3",
                                      "Holes 4",
                                      "Holes 5",
                                      "Holes 6",
                                      "Agahnim Room",
                                      "Holes 7",
                                      "Holes 8",
                                      "Open Chest for Holes 8",
                                      "Push Block for Chest",
                                      "Clear Room for Triforce Door",
                                      "Light Torches for Chest",
                                      "Kill Boss Again"};

} // namespace zelda3
} // namespace yaze

#endif // YAZE_APP_ZELDA3_DUNGEON_ROOM_TAG_H
