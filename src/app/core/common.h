#ifndef YAZE_CORE_COMMON_H
#define YAZE_CORE_COMMON_H

#include <imgui/imgui.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stack>
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

    // Use the new platform specific file dialog wrappers.
    bool kNewFileDialogWrapper = true;

    // Platform specific loading of fonts from the system. Currently
    // only supports macOS.
    bool kLoadSystemFonts = true;
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

// NotifyValue is a special type class which stores two copies of a type
// and uses that to check if the value was updated last or not
// It should have an accessor which says if it was modified or not
// and when that is read it should reset the value and state
template <typename T>
class NotifyValue {
 public:
  NotifyValue() : value_(), modified_(false) {}
  NotifyValue(const T &value) : value_(value), modified_(false) {}

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

struct TaskCheckpoint {
  int task_index = 0;
  bool complete = false;
  // You can add more internal data or state-related variables here as needed
};

class TaskTimer {
 public:
  // Starts the timer
  void StartTimer() { start_time_ = std::chrono::steady_clock::now(); }

  // Checks if the task should finish based on the given timeout in seconds
  bool ShouldFinishTask(int timeout_seconds) {
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(
        current_time - start_time_);
    return elapsed_time.count() >= timeout_seconds;
  }

 private:
  std::chrono::steady_clock::time_point start_time_;
};

class TaskManager {
 public:
  using TaskFunction = std::function<void(int)>;

  TaskManager(int totalTasks, int timeoutSeconds, const TaskFunction &taskFunc)
      : total_tasks_(totalTasks),
        timeout_seconds_(timeoutSeconds),
        task_function_(taskFunc),
        task_index_(0),
        task_complete_(false) {}

  void ExecuteTasks() {
    if (task_complete_) {
      return;
    }

    StartTimer();

    for (; task_index_ < total_tasks_; ++task_index_) {
      task_function_(task_index_);

      if (ShouldFinishTask()) {
        break;
      }
    }

    if (task_index_ == total_tasks_) {
      task_complete_ = true;
    }
  }

  bool IsTaskComplete() const { return task_complete_; }

 private:
  int total_tasks_;
  int timeout_seconds_;
  TaskFunction task_function_;
  int task_index_;
  bool task_complete_;
  std::chrono::steady_clock::time_point start_time_;

  void StartTimer() { start_time_ = std::chrono::steady_clock::now(); }

  bool ShouldFinishTask() {
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(
        current_time - start_time_);
    return elapsed_time.count() >= timeout_seconds_;
  }
};

class ImGuiIdIssuer {
 private:
  static std::stack<ImGuiID> idStack;

 public:
  // Generate and push a new ID onto the stack
  static ImGuiID GetNewID() {
    static int counter = 1;  // Start from 1 to ensure uniqueness
    ImGuiID child_id = ImGui::GetID((void *)(intptr_t)counter++);
    idStack.push(child_id);
    return child_id;
  }

  // Pop all IDs from the stack (can be called explicitly or upon program exit)
  static void Cleanup() {
    while (!idStack.empty()) {
      idStack.pop();
    }
  }
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