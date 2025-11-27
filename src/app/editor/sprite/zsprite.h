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
 *
 * ZSM files use .NET BinaryWriter/BinaryReader conventions:
 * - Strings: 7-bit encoded length prefix + UTF-8 bytes
 * - Integers: Little-endian 32-bit
 * - Booleans: Single byte (0x00 = false, 0x01 = true)
 */
namespace zsprite {

/**
 * @brief Read a .NET BinaryReader format string (7-bit encoded length prefix).
 */
inline std::string ReadDotNetString(std::istream& is) {
  uint32_t length = 0;
  uint8_t byte;
  int shift = 0;
  do {
    is.read(reinterpret_cast<char*>(&byte), 1);
    if (!is.good()) return "";
    length |= (byte & 0x7F) << shift;
    shift += 7;
  } while (byte & 0x80);

  std::string result(length, '\0');
  if (length > 0) {
    is.read(&result[0], length);
  }
  return result;
}

/**
 * @brief Write a .NET BinaryWriter format string (7-bit encoded length prefix).
 */
inline void WriteDotNetString(std::ostream& os, const std::string& str) {
  uint32_t length = static_cast<uint32_t>(str.size());

  // Write 7-bit encoded length
  do {
    uint8_t byte = length & 0x7F;
    length >>= 7;
    if (length > 0) {
      byte |= 0x80;  // Set continuation bit
    }
    os.write(reinterpret_cast<const char*>(&byte), 1);
  } while (length > 0);

  // Write string content
  if (!str.empty()) {
    os.write(str.data(), str.size());
  }
}

struct OamTile {
  OamTile() = default;
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

  uint8_t x = 0;
  uint8_t y = 0;
  bool mirror_x = false;
  bool mirror_y = false;
  uint16_t id = 0;
  uint8_t palette = 0;
  bool size = false;  // false = 8x8, true = 16x16
  uint8_t priority = 3;
  uint8_t z = 0;
};

struct AnimationGroup {
  AnimationGroup() = default;
  AnimationGroup(uint8_t fs, uint8_t fe, uint8_t fsp, std::string fn)
      : frame_name(std::move(fn)),
        frame_start(fs),
        frame_end(fe),
        frame_speed(fsp) {}

  std::string frame_name;
  uint8_t frame_start = 0;
  uint8_t frame_end = 0;
  uint8_t frame_speed = 1;
  std::vector<OamTile> Tiles;
};

struct Frame {
  std::vector<OamTile> Tiles;
};

struct UserRoutine {
  UserRoutine() = default;
  UserRoutine(std::string n, std::string c)
      : name(std::move(n)), code(std::move(c)) {}

  std::string name;
  std::string code;
};

struct SubEditor {
  std::vector<Frame> Frames;
  std::vector<UserRoutine> user_routines;
};

struct SpriteProperty {
  bool IsChecked = false;
  std::string Text;
};

struct ZSprite {
 public:
  /**
   * @brief Load a ZSM file from disk.
   */
  absl::Status Load(const std::string& filename) {
    std::ifstream fs(filename, std::ios::binary);
    if (!fs.is_open()) {
      return absl::NotFoundError("File not found: " + filename);
    }

    // Clear existing data
    Reset();

    // Read animation count
    int32_t animation_count = 0;
    fs.read(reinterpret_cast<char*>(&animation_count), sizeof(int32_t));

    // Read animations
    for (int i = 0; i < animation_count; i++) {
      std::string aname = ReadDotNetString(fs);
      uint8_t afs, afe, afspeed;
      fs.read(reinterpret_cast<char*>(&afs), sizeof(uint8_t));
      fs.read(reinterpret_cast<char*>(&afe), sizeof(uint8_t));
      fs.read(reinterpret_cast<char*>(&afspeed), sizeof(uint8_t));
      animations.emplace_back(afs, afe, afspeed, aname);
    }

    // Read frame count
    int32_t frame_count = 0;
    fs.read(reinterpret_cast<char*>(&frame_count), sizeof(int32_t));

    // Read frames
    for (int i = 0; i < frame_count; i++) {
      editor.Frames.emplace_back();

      int32_t tile_count = 0;
      fs.read(reinterpret_cast<char*>(&tile_count), sizeof(int32_t));

      for (int j = 0; j < tile_count; j++) {
        uint16_t tid;
        uint8_t tpal, tprior, tx, ty, tz;
        bool tmx, tmy, tsize;

        fs.read(reinterpret_cast<char*>(&tid), sizeof(uint16_t));
        fs.read(reinterpret_cast<char*>(&tpal), sizeof(uint8_t));
        fs.read(reinterpret_cast<char*>(&tmx), sizeof(bool));
        fs.read(reinterpret_cast<char*>(&tmy), sizeof(bool));
        fs.read(reinterpret_cast<char*>(&tprior), sizeof(uint8_t));
        fs.read(reinterpret_cast<char*>(&tsize), sizeof(bool));
        fs.read(reinterpret_cast<char*>(&tx), sizeof(uint8_t));
        fs.read(reinterpret_cast<char*>(&ty), sizeof(uint8_t));
        fs.read(reinterpret_cast<char*>(&tz), sizeof(uint8_t));

        OamTile tile(tx, ty, tmx, tmy, tid, tpal, tsize, tprior);
        tile.z = tz;
        editor.Frames[i].Tiles.push_back(tile);
      }
    }

    // Read 20 sprite boolean properties
    fs.read(reinterpret_cast<char*>(&property_blockable.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_canfall.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_collisionlayer.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_customdeath.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_damagesound.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_deflectarrows.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_deflectprojectiles.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_fast.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_harmless.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_impervious.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_imperviousarrow.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_imperviousmelee.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_interaction.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_isboss.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_persist.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_shadow.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_smallshadow.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_statis.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_statue.IsChecked), 1);
    fs.read(reinterpret_cast<char*>(&property_watersprite.IsChecked), 1);

    // Read 6 sprite stat bytes
    uint8_t prize, palette, oamnbr, hitbox, health, damage;
    fs.read(reinterpret_cast<char*>(&prize), sizeof(uint8_t));
    fs.read(reinterpret_cast<char*>(&palette), sizeof(uint8_t));
    fs.read(reinterpret_cast<char*>(&oamnbr), sizeof(uint8_t));
    fs.read(reinterpret_cast<char*>(&hitbox), sizeof(uint8_t));
    fs.read(reinterpret_cast<char*>(&health), sizeof(uint8_t));
    fs.read(reinterpret_cast<char*>(&damage), sizeof(uint8_t));

    property_prize.Text = std::to_string(prize);
    property_palette.Text = std::to_string(palette);
    property_oamnbr.Text = std::to_string(oamnbr);
    property_hitbox.Text = std::to_string(hitbox);
    property_health.Text = std::to_string(health);
    property_damage.Text = std::to_string(damage);

    // Read optional sections (check if more data exists)
    if (fs.peek() != EOF) {
      property_sprname.Text = ReadDotNetString(fs);
      sprName = property_sprname.Text;

      int32_t routine_count = 0;
      fs.read(reinterpret_cast<char*>(&routine_count), sizeof(int32_t));

      for (int i = 0; i < routine_count; i++) {
        std::string rname = ReadDotNetString(fs);
        std::string rcode = ReadDotNetString(fs);
        userRoutines.emplace_back(rname, rcode);
      }
    }

    // Read optional sprite ID
    if (fs.peek() != EOF) {
      property_sprid.Text = ReadDotNetString(fs);
    }

    fs.close();
    return absl::OkStatus();
  }

  /**
   * @brief Save a ZSM file to disk.
   */
  absl::Status Save(const std::string& filename) {
    std::ofstream fs(filename, std::ios::binary);
    if (!fs.is_open()) {
      return absl::InternalError("Failed to open file for writing: " + filename);
    }

    // Write animation count
    int32_t anim_count = static_cast<int32_t>(animations.size());
    fs.write(reinterpret_cast<const char*>(&anim_count), sizeof(int32_t));

    // Write animations
    for (const auto& anim : animations) {
      WriteDotNetString(fs, anim.frame_name);
      fs.write(reinterpret_cast<const char*>(&anim.frame_start), sizeof(uint8_t));
      fs.write(reinterpret_cast<const char*>(&anim.frame_end), sizeof(uint8_t));
      fs.write(reinterpret_cast<const char*>(&anim.frame_speed), sizeof(uint8_t));
    }

    // Write frame count
    int32_t frame_count = static_cast<int32_t>(editor.Frames.size());
    fs.write(reinterpret_cast<const char*>(&frame_count), sizeof(int32_t));

    // Write frames
    for (const auto& frame : editor.Frames) {
      int32_t tile_count = static_cast<int32_t>(frame.Tiles.size());
      fs.write(reinterpret_cast<const char*>(&tile_count), sizeof(int32_t));

      for (const auto& tile : frame.Tiles) {
        fs.write(reinterpret_cast<const char*>(&tile.id), sizeof(uint16_t));
        fs.write(reinterpret_cast<const char*>(&tile.palette), sizeof(uint8_t));
        fs.write(reinterpret_cast<const char*>(&tile.mirror_x), sizeof(bool));
        fs.write(reinterpret_cast<const char*>(&tile.mirror_y), sizeof(bool));
        fs.write(reinterpret_cast<const char*>(&tile.priority), sizeof(uint8_t));
        fs.write(reinterpret_cast<const char*>(&tile.size), sizeof(bool));
        fs.write(reinterpret_cast<const char*>(&tile.x), sizeof(uint8_t));
        fs.write(reinterpret_cast<const char*>(&tile.y), sizeof(uint8_t));
        fs.write(reinterpret_cast<const char*>(&tile.z), sizeof(uint8_t));
      }
    }

    // Write 20 sprite boolean properties
    fs.write(reinterpret_cast<const char*>(&property_blockable.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_canfall.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_collisionlayer.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_customdeath.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_damagesound.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_deflectarrows.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_deflectprojectiles.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_fast.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_harmless.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_impervious.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_imperviousarrow.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_imperviousmelee.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_interaction.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_isboss.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_persist.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_shadow.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_smallshadow.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_statis.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_statue.IsChecked), 1);
    fs.write(reinterpret_cast<const char*>(&property_watersprite.IsChecked), 1);

    // Write 6 sprite stat bytes (parse from Text properties)
    uint8_t prize = static_cast<uint8_t>(std::stoi(property_prize.Text.empty() ? "0" : property_prize.Text));
    uint8_t palette = static_cast<uint8_t>(std::stoi(property_palette.Text.empty() ? "0" : property_palette.Text));
    uint8_t oamnbr = static_cast<uint8_t>(std::stoi(property_oamnbr.Text.empty() ? "0" : property_oamnbr.Text));
    uint8_t hitbox = static_cast<uint8_t>(std::stoi(property_hitbox.Text.empty() ? "0" : property_hitbox.Text));
    uint8_t health = static_cast<uint8_t>(std::stoi(property_health.Text.empty() ? "0" : property_health.Text));
    uint8_t damage = static_cast<uint8_t>(std::stoi(property_damage.Text.empty() ? "0" : property_damage.Text));

    fs.write(reinterpret_cast<const char*>(&prize), sizeof(uint8_t));
    fs.write(reinterpret_cast<const char*>(&palette), sizeof(uint8_t));
    fs.write(reinterpret_cast<const char*>(&oamnbr), sizeof(uint8_t));
    fs.write(reinterpret_cast<const char*>(&hitbox), sizeof(uint8_t));
    fs.write(reinterpret_cast<const char*>(&health), sizeof(uint8_t));
    fs.write(reinterpret_cast<const char*>(&damage), sizeof(uint8_t));

    // Write sprite name
    WriteDotNetString(fs, sprName);

    // Write user routines
    int32_t routine_count = static_cast<int32_t>(userRoutines.size());
    fs.write(reinterpret_cast<const char*>(&routine_count), sizeof(int32_t));
    for (const auto& routine : userRoutines) {
      WriteDotNetString(fs, routine.name);
      WriteDotNetString(fs, routine.code);
    }

    // Write sprite ID
    WriteDotNetString(fs, property_sprid.Text);

    fs.close();
    return absl::OkStatus();
  }

  /**
   * @brief Reset all sprite data to defaults.
   */
  void Reset() {
    sprName.clear();
    animations.clear();
    userRoutines.clear();
    editor.Frames.clear();

    // Reset boolean properties
    property_blockable.IsChecked = false;
    property_canfall.IsChecked = false;
    property_collisionlayer.IsChecked = false;
    property_customdeath.IsChecked = false;
    property_damagesound.IsChecked = false;
    property_deflectarrows.IsChecked = false;
    property_deflectprojectiles.IsChecked = false;
    property_fast.IsChecked = false;
    property_harmless.IsChecked = false;
    property_impervious.IsChecked = false;
    property_imperviousarrow.IsChecked = false;
    property_imperviousmelee.IsChecked = false;
    property_interaction.IsChecked = false;
    property_isboss.IsChecked = false;
    property_persist.IsChecked = false;
    property_shadow.IsChecked = false;
    property_smallshadow.IsChecked = false;
    property_statis.IsChecked = false;
    property_statue.IsChecked = false;
    property_watersprite.IsChecked = false;

    // Reset text properties
    property_sprname.Text.clear();
    property_prize.Text = "0";
    property_palette.Text = "0";
    property_oamnbr.Text = "0";
    property_hitbox.Text = "0";
    property_health.Text = "0";
    property_damage.Text = "0";
    property_sprid.Text.clear();
  }

  std::string sprName;
  std::vector<AnimationGroup> animations;
  std::vector<UserRoutine> userRoutines;
  SubEditor editor;

  // Boolean properties (20 total)
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

  // Stat properties (6 total, stored as text)
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
