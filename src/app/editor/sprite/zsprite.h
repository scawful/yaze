#ifndef YAZE_APP_EDITOR_SPRITE_ZSPRITE_H
#define YAZE_APP_EDITOR_SPRITE_ZSPRITE_H

#include <imgui/imgui.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace yaze {
namespace editor {
namespace zsprite {

struct Tile {
  uint16_t id;
  uint8_t palette;
  bool mirrorX;
  bool mirrorY;
  uint8_t priority;
  bool size;
  uint8_t x;
  uint8_t y;
  uint8_t z;
};

struct AnimationGroup {
  std::string FrameName;
  uint8_t FrameStart;
  uint8_t FrameEnd;
  uint8_t FrameSpeed;
  std::vector<Tile> Tiles;
};

struct UserRoutine {
  std::string name;
  std::string code;
  int Count;
};

struct SubEditor {
  std::vector<AnimationGroup> Frames;
  std::vector<UserRoutine> UserRoutines;
};

struct SpriteProperty {
  bool IsChecked;
  std::string Text;
};

class ZSprite {
 public:
  void SaveZSpriteFormat() {
    std::ofstream fs("filename.bin", std::ios::binary);
    if (fs.is_open()) {
      // Write data to the file
      fs.write(reinterpret_cast<const char*>(animations.size()), sizeof(int));
      for (const AnimationGroup& anim : animations) {
        fs.write(anim.FrameName.c_str(), anim.FrameName.size() + 1);
        fs.write(reinterpret_cast<const char*>(&anim.FrameStart),
                 sizeof(uint8_t));
        fs.write(reinterpret_cast<const char*>(&anim.FrameEnd),
                 sizeof(uint8_t));
        fs.write(reinterpret_cast<const char*>(&anim.FrameSpeed),
                 sizeof(uint8_t));
      }

      fs.write(reinterpret_cast<const char*>(editor.Frames.size()),
               sizeof(int));
      for (int i = 0; i < editor.Frames.size(); i++) {
        fs.write(reinterpret_cast<const char*>(editor.Frames[i].Tiles.size()),
                 sizeof(int));

        for (int j = 0; j < editor.Frames[i].Tiles.size(); j++) {
          fs.write(reinterpret_cast<const char*>(&editor.Frames[i].Tiles[j].id),
                   sizeof(ushort));
          fs.write(
              reinterpret_cast<const char*>(&editor.Frames[i].Tiles[j].palette),
              sizeof(uint8_t));
          fs.write(
              reinterpret_cast<const char*>(&editor.Frames[i].Tiles[j].mirrorX),
              sizeof(bool));
          fs.write(
              reinterpret_cast<const char*>(&editor.Frames[i].Tiles[j].mirrorY),
              sizeof(bool));
          fs.write(reinterpret_cast<const char*>(
                       &editor.Frames[i].Tiles[j].priority),
                   sizeof(uint8_t));
          fs.write(
              reinterpret_cast<const char*>(&editor.Frames[i].Tiles[j].size),
              sizeof(bool));
          fs.write(reinterpret_cast<const char*>(&editor.Frames[i].Tiles[j].x),
                   sizeof(uint8_t));
          fs.write(reinterpret_cast<const char*>(&editor.Frames[i].Tiles[j].y),
                   sizeof(uint8_t));
          fs.write(reinterpret_cast<const char*>(&editor.Frames[i].Tiles[j].z),
                   sizeof(uint8_t));
        }
      }

      // Write other properties
      fs.write(reinterpret_cast<const char*>(&property_blockable.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_canfall.IsChecked),
               sizeof(bool));
      fs.write(
          reinterpret_cast<const char*>(&property_collisionlayer.IsChecked),
          sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_customdeath.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_damagesound.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_deflectarrows.IsChecked),
               sizeof(bool));
      fs.write(
          reinterpret_cast<const char*>(&property_deflectprojectiles.IsChecked),
          sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_fast.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_harmless.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_impervious.IsChecked),
               sizeof(bool));
      fs.write(
          reinterpret_cast<const char*>(&property_imperviousarrow.IsChecked),
          sizeof(bool));
      fs.write(
          reinterpret_cast<const char*>(&property_imperviousmelee.IsChecked),
          sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_interaction.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_isboss.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_persist.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_shadow.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_smallshadow.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_statis.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_statue.IsChecked),
               sizeof(bool));
      fs.write(reinterpret_cast<const char*>(&property_watersprite.IsChecked),
               sizeof(bool));

      fs.write(reinterpret_cast<const char*>(&property_prize.Text),
               sizeof(uint8_t));
      fs.write(reinterpret_cast<const char*>(&property_palette.Text),
               sizeof(uint8_t));
      fs.write(reinterpret_cast<const char*>(&property_oamnbr.Text),
               sizeof(uint8_t));
      fs.write(reinterpret_cast<const char*>(&property_hitbox.Text),
               sizeof(uint8_t));
      fs.write(reinterpret_cast<const char*>(&property_health.Text),
               sizeof(uint8_t));
      fs.write(reinterpret_cast<const char*>(&property_damage.Text),
               sizeof(uint8_t));

      fs.write(sprName.c_str(), sprName.size() + 1);

      fs.write(reinterpret_cast<const char*>(userRoutines.size()), sizeof(int));
      for (const UserRoutine& userR : userRoutines) {
        fs.write(userR.name.c_str(), userR.name.size() + 1);
        fs.write(userR.code.c_str(), userR.code.size() + 1);
      }

      fs.write(reinterpret_cast<const char*>(&property_sprid.Text),
               sizeof(property_sprid.Text));

      fs.close();
    }
  }

 private:
  std::string sprName;
  std::vector<AnimationGroup> animations;
  std::vector<UserRoutine> userRoutines;
  SubEditor editor;

  SpriteProperty property_blockable;
  SpriteProperty property_canfall;
  SpriteProperty property_collisionlayer;
  SpriteProperty property_customdeath;
  SpriteProperty property_damagesound;
  SpriteProperty property_deflectarrows;
  SpriteProperty property_deflectprojectiles;
  SpriteProperty property_fast;
  SpriteProperty property_harmless;
  SpriteProperty property_impervious;
  SpriteProperty property_imperviousarrow;
  SpriteProperty property_imperviousmelee;
  SpriteProperty property_interaction;
  SpriteProperty property_isboss;
  SpriteProperty property_persist;
  SpriteProperty property_shadow;
  SpriteProperty property_smallshadow;
  SpriteProperty property_statis;
  SpriteProperty property_statue;
  SpriteProperty property_watersprite;

  SpriteProperty property_prize;
  SpriteProperty property_palette;
  SpriteProperty property_oamnbr;
  SpriteProperty property_hitbox;
  SpriteProperty property_health;
  SpriteProperty property_damage;

  SpriteProperty property_sprid;
};

}  // namespace zsprite
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_ZSPRITE_H