#ifndef YAZE_APP_SERVICE_EDITOR_CONTROL_QUEUE_H_
#define YAZE_APP_SERVICE_EDITOR_CONTROL_QUEUE_H_

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::app {

// Worker requests are applied only by ProcessOne on the main frame thread.
// A command that times out before execution can never run on a later frame.
class EditorControlQueue {
 public:
  absl::StatusOr<std::string> Submit(
      std::string json,
      std::chrono::milliseconds queue_timeout = std::chrono::seconds(5)) {
    auto request = std::make_shared<Request>();
    request->json = std::move(json);
    auto result = request->result.get_future();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (requests_.size() >= 16) {
        return absl::ResourceExhaustedError("Editor command queue is full");
      }
      requests_.push(request);
    }
    if (result.wait_for(queue_timeout) != std::future_status::ready) {
      int queued = 0;
      if (request->state.compare_exchange_strong(queued, 2)) {
        return absl::DeadlineExceededError(
            "Editor did not process the command; cancelled without execution");
      }
      // Once execution starts, return its actual result, not a false timeout.
    }
    return result.get();
  }

  void ProcessOne(
      const std::function<std::string(const std::string&)>& action) {
    std::shared_ptr<Request> request;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (requests_.empty())
        return;
      request = requests_.front();
      requests_.pop();
    }
    int queued = 0;
    if (request->state.compare_exchange_strong(queued, 1)) {
      try {
        request->result.set_value(action(request->json));
      } catch (...) {
        // Release the waiting HTTP worker even if the callback throws.
        request->result.set_value(
            R"({"protocol":"yaze-editor-control/v1","status":"error","http_status":500,"error":"Editor command failed"})");
      }
    }
  }

  bool pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !requests_.empty();
  }

 private:
  struct Request {
    std::string json;
    std::promise<std::string> result;
    // 0 queued, 1 running, 2 cancelled before execution.
    std::atomic<int> state{0};
  };
  mutable std::mutex mutex_;
  std::queue<std::shared_ptr<Request>> requests_;
};

}  // namespace yaze::app
#endif
