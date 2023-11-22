#ifndef YAZE_CORE_COMMON_H
#define YAZE_CORE_COMMON_H

#include <cstdint>
#include <memory>
#include <string>

namespace yaze {
namespace app {
namespace core {

class ExperimentFlags {
 public:
  struct Flags {
    // Load and render overworld sprites to the screen. Unstable.
    bool kDrawOverworldSprites = false;

    // Bitmap manager abstraction to manage graphics bin of ROM.
    bool kUseBitmapManager = true;

    // Log instructions to the GUI debugger.
    bool kLogInstructions = false;

    // Flag to enable ImGui input config flags. Currently is
    // handled manually by controller class but should be
    // ported away from that eventually.
    bool kUseNewImGuiInput = false;

    // Flag to enable the saving of all palettes to the ROM.
    bool kSaveAllPalettes = false;

    // Flag to enable the change queue, which could have any anonymous
    // save routine for the ROM. In practice, just the overworld tilemap
    // and tile32 save.
    bool kSaveWithChangeQueue = false;

    // Attempt to run the dungeon room draw routine when opening a room.
    bool kDrawDungeonRoomGraphics = true;
  };

  ExperimentFlags() = default;
  virtual ~ExperimentFlags() = default;
  auto flags() const {
    if (!flags_) {
      flags_ = std::make_shared<Flags>();
    }
    Flags *flags = flags_.get();
    return flags;
  }
  Flags *mutable_flags() {
    if (!flags_) {
      flags_ = std::make_shared<Flags>();
    }
    return flags_.get();
  }

 private:
  static std::shared_ptr<Flags> flags_;
};

// NotifyFlag is a special type class which stores two copies of a type
// and uses that to check if the value was updated last or not
// It should have an accessor which says if it was modified or not
// and when that is read it should reset the value and state
template <typename T>
class NotifyFlag {
 public:
  NotifyFlag() : value_(), modified_(false) {}

  void set(const T &value) {
    value_ = value;
    modified_ = true;
  }

  const T &get() {
    modified_ = false;
    return value_;
  }

  void operator=(const T &value) { set(value); }
  operator T() const { return get(); }

  bool isModified() const { return modified_; }

 private:
  T value_;
  bool modified_;
};

uint32_t SnesToPc(uint32_t addr);
uint32_t PcToSnes(uint32_t addr);

uint32_t MapBankToWordAddress(uint8_t bank, uint16_t addr);

int AddressFromBytes(uint8_t addr1, uint8_t addr2, uint8_t addr3);
int HexToDec(char *input, int length);

bool StringReplace(std::string &str, const std::string &from,
                   const std::string &to);

void stle16b_i(uint8_t *const p_arr, size_t const p_index,
               uint16_t const p_val);
uint16_t ldle16b_i(uint8_t const *const p_arr, size_t const p_index);

void stle16b(uint8_t *const p_arr, uint16_t const p_val);
void stle32b(uint8_t *const p_arr, uint32_t const p_val);

void stle32b_i(uint8_t *const p_arr, size_t const p_index,
               uint32_t const p_val);

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif