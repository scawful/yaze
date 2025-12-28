#pragma once

#include <string>

#include "absl/status/status.h"
#include "app/application.h"

namespace yaze {
namespace ios {

struct IOSHostConfig {
  AppConfig app_config;
  bool auto_start = true;
};

class IOSHost {
 public:
  IOSHost() = default;
  ~IOSHost();

  absl::Status Initialize(const IOSHostConfig& config);
  void Tick();
  void Shutdown();

  void SetMetalView(void* view);
  void* GetMetalView() const;

 private:
  IOSHostConfig config_{};
  void* metal_view_ = nullptr;
  bool initialized_ = false;
};

}  // namespace ios
}  // namespace yaze
