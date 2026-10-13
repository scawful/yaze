#ifndef YAZE_UTIL_STOP_TOKEN_H_
#define YAZE_UTIL_STOP_TOKEN_H_

#include <atomic>
#include <memory>
#include <utility>

namespace yaze::util {

struct NoStopStateTag {};
inline constexpr NoStopStateTag kNoStopState{};

// Poll-only cancellation for background scans. Some supported libc++ versions
// lack std::stop_token; the scans do not need stop callbacks or std::jthread.
class StopToken {
 public:
  StopToken() = default;

  bool has_stop_state() const noexcept { return state_ != nullptr; }

  bool stop_requested() const noexcept {
    return state_ && state_->load(std::memory_order_acquire);
  }

 private:
  friend class StopSource;
  explicit StopToken(std::shared_ptr<std::atomic<bool>> state)
      : state_(std::move(state)) {}

  std::shared_ptr<std::atomic<bool>> state_;
};

class StopSource {
 public:
  StopSource() : state_(std::make_shared<std::atomic<bool>>(false)) {}
  explicit StopSource(NoStopStateTag) noexcept {}

  StopToken get_token() const noexcept { return StopToken(state_); }

  bool request_stop() noexcept {
    return state_ && !state_->exchange(true, std::memory_order_acq_rel);
  }

 private:
  std::shared_ptr<std::atomic<bool>> state_;
};

}  // namespace yaze::util

#endif  // YAZE_UTIL_STOP_TOKEN_H_
