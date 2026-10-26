#include "app/controller.h"

#include <mutex>

namespace yaze {

void Controller::RequestScreenshot(const ScreenshotRequest& request) {
  std::lock_guard<std::mutex> lock(screenshot_mutex_);
  screenshot_requests_.push(request);
}

}  // namespace yaze
