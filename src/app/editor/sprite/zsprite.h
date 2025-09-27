#ifndef YAZE_APP_EDITOR_SPRITE_ZSPRITE_H
#define YAZE_APP_EDITOR_SPRITE_ZSPRITE_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "util/macro.h"

namespace yaze {
namespace editor {
/**
 * @brief Namespace for the ZSprite format from Zarby's ZSpriteMaker.
 */
namespace zsprite {

struct OamTile {
  OamTile(uint8_t x, uint8_t y, bool mx, bool my, uint16_t id, uint8_t pal,
          bool s, uint8_t p)
      : x(x),
        y(y),
        mirror_x(mx),
        mirror_y(my),
        id(id),
        palette(pal),
        size(s),
        priority(p) {}

  uint8_t x;
  uint8_t y;
  bool mirror_x;
  bool mirror_y;
  uint16_t id;
  uint8_t palette;
  bool size;
  uint8_t priority;
  uint8_t z;
};

struct AnimationGroup {
  AnimationGroup() = default;
  AnimationGroup(uint8_t fs, uint8_t fe, uint8_t fsp, std::string fn)
      : frame_name(fn), frame_start(fs), frame_end(fe), frame_speed(fsp) {}

  std::string frame_name;
  uint8_t frame_start;
  uint8_t frame_end;
  uint8_t frame_speed;
  std::vector<OamTile> Tiles;
};

struct UserRoutine {
  UserRoutine(std::string n, std::string c) : name(n), code(c) {}

  std::string name;
  std::string code;
  int Count;
};

struct SubEditor {
  std::vector<AnimationGroup> Frames;
  std::vector<UserRoutine> user_routines;
};

struct SpriteProperty {
  bool IsChecked;
  std::string Text;
};

struct ZSprite {
 public:
  absl::Status Load(const std::string& filename) {
    std::ifstream fs(filename, std::ios::binary);
    if (!fs.is_open()) {
      return absl::NotFoundError("File not found");
    }

    std::vector<char> buffer(std::istreambuf_iterator<char>(fs), {});

    int animation_count = *reinterpret_cast<int32_t*>(&buffer[0]);
    int offset = sizeof(int);

    for (int i = 0; i < animation_count; i++) {
      std::string aname = std::string(&buffer[offset]);
      offset += aname.size() + 1;
      uint8_t afs = *reinterpret_cast<uint8_t*>(&buffer[offset]);
      offset += sizeof(uint8_t);
      uint8_t afe = *reinterpret_cast<uint8_t*>(&buffer[offset]);
      offset += sizeof(uint8_t);
      uint8_t afspeed = *reinterpret_cast<uint8_t*>(&buffer[offset]);
      offset += sizeof(uint8_t);

      animations.push_back(AnimationGroup(afs, afe, afspeed, aname));
    }
    // RefreshAnimations();

    int frame_count = *reinterpret_cast<int32_t*>(&buffer[offset]);
    offset += sizeof(int);
    for (int i = 0; i < frame_count; i++) {
      // editor.Frames[i] = new Frame();
      editor.Frames.emplace_back();
      // editor.AddUndo(i);
      int tCount = *reinterpret_cast<int*>(&buffer[offset]);
      offset += sizeof(int);

      for (int j = 0; j < tCount; j++) {
        uint16_t tid = *reinterpret_cast<uint16_t*>(&buffer[offset]);
        offset += sizeof(uint16_t);
        uint8_t tpal = *reinterpret_cast<uint8_t*>(&buffer[offset]);
        offset += sizeof(uint8_t);
        bool tmx = *reinterpret_cast<bool*>(&buffer[offset]);
        offset += sizeof(bool);
        bool tmy = *reinterpret_cast<bool*>(&buffer[offset]);
        offset += sizeof(bool);
        uint8_t tprior = *reinterpret_cast<uint8_t*>(&buffer[offset]);
        offset += sizeof(uint8_t);
        bool tsize = *reinterpret_cast<bool*>(&buffer[offset]);
        offset += sizeof(bool);
        uint8_t tx = *reinterpret_cast<uint8_t*>(&buffer[offset]);
        offset += sizeof(uint8_t);
        uint8_t ty = *reinterpret_cast<uint8_t*>(&buffer[offset]);
        offset += sizeof(uint8_t);
        uint8_t tz = *reinterpret_cast<uint8_t*>(&buffer[offset]);
        offset += sizeof(uint8_t);
        OamTile to(tx, ty, tmx, tmy, tid, tpal, tsize, tprior);
        to.z = tz;
        editor.Frames[i].Tiles.push_back(to);
      }
    }

    // all sprites properties
    property_blockable.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_canfall.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_collisionlayer.IsChecked =
        *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_customdeath.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_damagesound.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_deflectarrows.IsChecked =
        *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_deflectprojectiles.IsChecked =
        *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_fast.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_harmless.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_impervious.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_imperviousarrow.IsChecked =
        *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_imperviousmelee.IsChecked =
        *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_interaction.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_isboss.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_persist.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_shadow.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_smallshadow.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_statis.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_statue.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);
    property_watersprite.IsChecked = *reinterpret_cast<bool*>(&buffer[offset]);
    offset += sizeof(bool);

    property_prize.Text =
        std::to_string(*reinterpret_cast<uint8_t*>(&buffer[offset]));
    offset += sizeof(uint8_t);
    property_palette.Text =
        std::to_string(*reinterpret_cast<uint8_t*>(&buffer[offset]));
    offset += sizeof(uint8_t);
    property_oamnbr.Text =
        std::to_string(*reinterpret_cast<uint8_t*>(&buffer[offset]));
    offset += sizeof(uint8_t);
    property_hitbox.Text =
        std::to_string(*reinterpret_cast<uint8_t*>(&buffer[offset]));
    offset += sizeof(uint8_t);
    property_health.Text =
        std::to_string(*reinterpret_cast<uint8_t*>(&buffer[offset]));
    offset += sizeof(uint8_t);
    property_damage.Text =
        std::to_string(*reinterpret_cast<uint8_t*>(&buffer[offset]));
    offset += sizeof(uint8_t);

    if (offset != buffer.size()) {
      property_sprname.Text = std::string(&buffer[offset]);
      offset += property_sprname.Text.size() + 1;

      int actionL = buffer[offset];
      offset += sizeof(int);
      for (int i = 0; i < actionL; i++) {
        std::string a = std::string(&buffer[offset]);
        offset += a.size() + 1;
        std::string b = std::string(&buffer[offset]);
        offset += b.size() + 1;
        userRoutines.push_back(UserRoutine(a, b));
      }
    }

    if (offset != buffer.size()) {
      property_sprid.Text = std::string(&buffer[offset]);
      fs.close();
    }

    // UpdateUserRoutines();
    // userroutinesListbox.SelectedIndex = 0;
    // RefreshScreen();

    return absl::OkStatus();
  }

  absl::Status Save(const std::string& filename) {
    std::ofstream fs(filename, std::ios::binary);
    if (fs.is_open()) {
      // Write data to the file
      fs.write(reinterpret_cast<const char*>(animations.size()), sizeof(int));
      for (const AnimationGroup& anim : animations) {
        fs.write(anim.frame_name.c_str(), anim.frame_name.size() + 1);
        fs.write(reinterpret_cast<const char*>(&anim.frame_start),
                 sizeof(uint8_t));
        fs.write(reinterpret_cast<const char*>(&anim.frame_end),
                 sizeof(uint8_t));
        fs.write(reinterpret_cast<const char*>(&anim.frame_speed),
                 sizeof(uint8_t));
      }

      fs.write(reinterpret_cast<const char*>(editor.Frames.size()),
               sizeof(int));
      for (int i = 0; i < editor.Frames.size(); i++) {
        fs.write(reinterpret_cast<const char*>(editor.Frames[i].Tiles.size()),
                 sizeof(int));

        for (int j = 0; j < editor.Frames[i].Tiles.size(); j++) {
          fs.write(reinterpret_cast<const char*>(&editor.Frames[i].Tiles[j].id),
                   sizeof(uint16_t));
          fs.write(
              reinterpret_cast<const char*>(&editor.Frames[i].Tiles[j].palette),
              sizeof(uint8_t));
          fs.write(reinterpret_cast<const char*>(
                       &editor.Frames[i].Tiles[j].mirror_x),
                   sizeof(bool));
          fs.write(reinterpret_cast<const char*>(
                       &editor.Frames[i].Tiles[j].mirror_y),
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

    return absl::OkStatus();
  }

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
  SpriteProperty property_sprname;

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
