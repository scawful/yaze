#include "app/controller.h"

#include <mutex>

#include "util/ui_thread_queue.h"

namespace yaze {

void Controller::RequestScreenshot(const ScreenshotRequest& request) {
  {
    std::lock_guard<std::mutex> lock(screenshot_mutex_);
    screenshot_requests_.push(request);
  }
  // Reuse the queue's guarded waker so an idle loop captures promptly without
  // calling a window backend that another thread could be shutting down.
  util::UiThreadQueue::Get().Post([]() {});
}

}  // namespace yaze
