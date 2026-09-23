#include <cstdint>
#include <cstring>
#include <new>

#include "gtest/gtest.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace zelda3 {
namespace {

// The dungeon constructor initializes id_, nx_, ny_, subtype_ and layer_ only.
// Before default member initializers were added, map_id_ and game_state_ kept
// whatever was in the storage, so a dungeon sprite could report a nonzero
// overworld map id.
//
// The sprite is constructed into storage pre-filled with 0xFF so that a
// missing initializer reads back as 0xFF rather than an incidental zero: this
// test fails against the unfixed header instead of passing by luck.
TEST(SpriteValueInitTest, DungeonConstructorZeroInitializesUnsetMembers) {
  alignas(Sprite) unsigned char storage[sizeof(Sprite)];
  std::memset(storage, 0xFF, sizeof(storage));

  auto* sprite = new (storage) Sprite(/*id=*/0x1A, /*x=*/4, /*y=*/5,
                                      /*subtype=*/0x07, /*layer=*/1);

  EXPECT_EQ(sprite->map_id(), 0);
  EXPECT_EQ(sprite->game_state(), 0);

  // The arguments the constructor does set are unaffected.
  EXPECT_EQ(sprite->id(), 0x1A);
  EXPECT_EQ(sprite->nx(), 4);
  EXPECT_EQ(sprite->ny(), 5);

  sprite->~Sprite();
}

// `Sprite() = default` is not user-provided, so `Sprite()` value-initializes
// and zero-fills even without the member initializers. This case therefore
// passes both before and after the change: it guards the guarantee against a
// future user-provided default constructor, which would silently reintroduce
// indeterminate members.
TEST(SpriteValueInitTest, DefaultConstructorZeroInitializesMembers) {
  alignas(Sprite) unsigned char storage[sizeof(Sprite)];
  std::memset(storage, 0xFF, sizeof(storage));

  auto* sprite = new (storage) Sprite();

  EXPECT_EQ(sprite->id(), 0);
  EXPECT_EQ(sprite->map_id(), 0);
  EXPECT_EQ(sprite->game_state(), 0);
  EXPECT_EQ(sprite->nx(), 0);
  EXPECT_EQ(sprite->ny(), 0);

  sprite->~Sprite();
}

}  // namespace
}  // namespace zelda3
}  // namespace yaze
