#include "zelda3/atlas/dungeon_door_edges.h"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <utility>

namespace yaze::zelda3::atlas {

namespace {

constexpr int kRooms = 0x128;

char WallLetter(int direction) {
  switch (direction) {
    case 0:
      return 'N';
    case 1:
      return 'S';
    case 2:
      return 'W';
    case 3:
      return 'E';
  }
  return '?';
}

char Opposite(char wall) {
  switch (wall) {
    case 'N':
      return 'S';
    case 'S':
      return 'N';
    case 'E':
      return 'W';
    case 'W':
      return 'E';
  }
  return '?';
}

// room_data.py CAT_RANK; categories it does not list rank as "open".
int Rank(DoorCategory category) {
  switch (category) {
    case DoorCategory::kBigKey:
      return 5;
    case DoorCategory::kKey:
      return 4;
    case DoorCategory::kShutter:
      return 3;
    case DoorCategory::kBomb:
      return 2;
    case DoorCategory::kOneSided:
      return 0;
    default:
      return 1;
  }
}

bool Pairable(DoorCategory category) {
  return category != DoorCategory::kExit &&
         category != DoorCategory::kKeyStairs;
}

}  // namespace

const char* DoorCategoryName(DoorCategory category) {
  switch (category) {
    case DoorCategory::kOpen:
      return "open";
    case DoorCategory::kOneSided:
      return "onesided";
    case DoorCategory::kShutter:
      return "shutter";
    case DoorCategory::kKey:
      return "key";
    case DoorCategory::kBigKey:
      return "bigkey";
    case DoorCategory::kBomb:
      return "bomb";
    case DoorCategory::kExit:
      return "exit";
    case DoorCategory::kKeyStairs:
      return "keystairs";
    case DoorCategory::kOther:
      return "other";
    case DoorCategory::kIgnore:
      return "ignore";
  }
  return "other";
}

DoorCategory DoorCategoryForType(int door_type) {
  switch (door_type) {
    case 0x00:  // Normal door
    case 0x02:  // Normal door (lower layer)
    case 0x32:  // Curtain door
    case 0x46:  // Explicit room door
      return DoorCategory::kOpen;
    case 0x40:
      return DoorCategory::kOneSided;
    case 0x18:  // Double-sided shutter
    case 0x36:  // Bottom-sided shutter
    case 0x38:  // Top-sided shutter
    case 0x44:  // Double-sided shutter (lower layer)
    case 0x48:  // Bottom-sided shutter (lower layer)
      return DoorCategory::kShutter;
    case 0x1C:
      return DoorCategory::kKey;
    case 0x1E:
      return DoorCategory::kBigKey;
    case 0x28:  // Dash wall
    case 0x2E:  // Bombable door
    case 0x30:  // Exploding wall
      return DoorCategory::kBomb;
    case 0x04:  // Exit (lower layer)
    case 0x0A:  // Fancy dungeon exit
    case 0x0E:  // Cave exit
    case 0x10:  // Lit cave exit (lower layer)
    case 0x12:  // Exit marker
      return DoorCategory::kExit;
    case 0x20:  // Small key stairs (up)
    case 0x22:  // Small key stairs (down)
      return DoorCategory::kKeyStairs;
    case 0x16:  // Layer swap marker
      return DoorCategory::kIgnore;
  }
  return DoorCategory::kOther;
}

std::vector<AtlasDoor> OuterWallDoors(const std::vector<AtlasDoorFact>& doors) {
  std::vector<AtlasDoor> out;
  for (const auto& fact : doors) {
    const DoorCategory category = DoorCategoryForType(fact.type);
    const char wall = WallLetter(fact.direction);
    const bool outer = (wall == 'N' && fact.tile_y <= 8) ||
                       (wall == 'S' && fact.tile_y >= 50) ||
                       (wall == 'W' && fact.tile_x <= 8) ||
                       (wall == 'E' && fact.tile_x >= 50);
    if (category == DoorCategory::kIgnore || !outer) {
      continue;
    }
    AtlasDoor door;
    door.wall = wall;
    door.pos = (wall == 'N' || wall == 'S') ? fact.tile_x + 2 : fact.tile_y + 2;
    door.type = fact.type;
    door.type_name = fact.type_name;
    door.category = category;
    out.push_back(std::move(door));
  }
  std::vector<std::pair<char, int>> exits;
  for (const auto& door : out) {
    if (door.category == DoorCategory::kExit) {
      exits.emplace_back(door.wall, door.pos);
    }
  }
  for (auto& door : out) {
    if (door.category == DoorCategory::kExit) {
      continue;
    }
    for (const auto& [wall, pos] : exits) {
      if (wall == door.wall && std::abs(pos - door.pos) <= 2) {
        door.category = DoorCategory::kExit;
        break;
      }
    }
  }
  return out;
}

std::vector<DoorEdge> PairDoorEdges(
    const std::vector<std::vector<AtlasDoor>>& doors_by_room) {
  std::vector<DoorEdge> out;
  std::set<std::pair<int, size_t>> used;
  const int rooms = std::min<int>(kRooms, doors_by_room.size());
  for (int a = 0; a < rooms; ++a) {
    // Same order as room_data.py: E, S, W, N. Rows are 16 rooms wide across
    // the whole $000-$127 range (0xF5 south is 0x105).
    const std::pair<char, int> neighbours[] = {
        {'E', (a & 0xF) != 0xF ? a + 1 : -1},
        {'S', a + 0x10 < kRooms ? a + 0x10 : -1},
        {'W', (a & 0xF) != 0 ? a - 1 : -1},
        {'N', a >= 0x10 ? a - 0x10 : -1}};
    for (const auto& [dir, b] : neighbours) {
      if (b < 0 || b >= rooms) {
        continue;
      }
      const auto& doors_a = doors_by_room[a];
      const auto& doors_b = doors_by_room[b];
      for (size_t i = 0; i < doors_a.size(); ++i) {
        const AtlasDoor& da = doors_a[i];
        if (da.wall != dir || !Pairable(da.category) || used.contains({a, i})) {
          continue;
        }
        int match = -1;
        for (size_t j = 0; j < doors_b.size(); ++j) {
          const AtlasDoor& db = doors_b[j];
          if (db.wall == Opposite(dir) && Pairable(db.category) &&
              std::abs(db.pos - da.pos) <= 3 && !used.contains({b, j})) {
            match = static_cast<int>(j);
            break;
          }
        }
        used.insert({a, i});
        DoorEdge edge;
        edge.a = a;
        edge.b = b;
        edge.dir = dir;
        edge.pos = da.pos;
        if (match >= 0) {
          const AtlasDoor& db = doors_b[match];
          used.insert({b, static_cast<size_t>(match)});
          // Python max(): the first of equal ranks wins.
          DoorCategory category =
              Rank(db.category) > Rank(da.category) ? db.category : da.category;
          if (category == DoorCategory::kOneSided) {
            category = DoorCategory::kOpen;
          }
          edge.category = category;
          edge.both_sides = true;
          edge.one_sided_type = da.category == DoorCategory::kOneSided ||
                                db.category == DoorCategory::kOneSided;
          edge.types = {da.type_name, db.type_name};
        } else {
          edge.category = da.category == DoorCategory::kOneSided
                              ? DoorCategory::kOpen
                              : da.category;
          edge.both_sides = false;
          edge.one_sided_type = da.category == DoorCategory::kOneSided;
          edge.types = {da.type_name};
        }
        out.push_back(std::move(edge));
      }
    }
  }
  return out;
}

}  // namespace yaze::zelda3::atlas
