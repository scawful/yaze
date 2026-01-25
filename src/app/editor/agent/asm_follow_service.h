#ifndef YAZE_APP_EDITOR_AGENT_ASM_FOLLOW_SERVICE_H_
#define YAZE_APP_EDITOR_AGENT_ASM_FOLLOW_SERVICE_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

#include "app/emu/debug/symbol_provider.h"

namespace yaze {
namespace editor {

/**
 * @brief Service that synchronizes Mesen's Program Counter with ASM source files
 */
class AsmFollowService {
 public:
  AsmFollowService(emu::debug::SymbolProvider* symbol_provider);
  ~AsmFollowService() = default;

  void Update();
  
  void SetEnabled(bool enabled) { enabled_ = enabled; }
  bool IsEnabled() const { return enabled_; }

  // Callback signature: void OnSourceLocationChanged(const std::string& location)
  // location format: "file:line"
  using LocationCallback = std::function<void(const std::string&)>;
  void SetLocationCallback(LocationCallback callback) { callback_ = std::move(callback); }

 private:
  emu::debug::SymbolProvider* symbol_provider_;
  std::atomic<bool> enabled_{false};
  uint32_t last_pc_ = 0xFFFFFF;
  LocationCallback callback_;
  double last_poll_time_ = 0.0;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_ASM_FOLLOW_SERVICE_H_
