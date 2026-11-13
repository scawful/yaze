#ifndef YAZE_UTIL_UI_THREAD_QUEUE_H_
#define YAZE_UTIL_UI_THREAD_QUEUE_H_

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace util {

/**
 * @brief Moves work from service threads (gRPC, HTTP) onto the UI thread.
 *
 * Editor, canvas and ROM state is owned by the thread that runs the frame
 * loop. Socket threads must not touch it directly, so they hand a task to
 * this queue and the Controller runs it at the start of the next frame,
 * before ImGui::NewFrame().
 *
 * Lifecycle:
 * 1. The Controller calls BindToCurrentThread() and SetWaker() once the
 *    window backend exists.
 * 2. Every frame, the Controller calls Drain() before NewFrame.
 * 3. On quit, the Controller calls Shutdown() and SetWaker({}). Shutdown
 *    must happen before services are torn down, so a gRPC handler blocked
 *    in RunOnUiThread() returns Cancelled instead of holding up
 *    grpc::Server::Shutdown().
 *
 * Get() returns the process-wide instance. A singleton is used because the
 * gRPC and HTTP services are built by Application, apart from the
 * Controller, and need to reach the same queue without new constructor
 * plumbing. Tests construct their own UiThreadQueue instances.
 */
class UiThreadQueue {
 public:
  // Long enough for a slow frame (ROM load, large redraw), short enough
  // that a stuck UI thread does not hang an agent call forever.
  static constexpr std::chrono::milliseconds kDefaultTimeout{5000};

  UiThreadQueue() = default;
  ~UiThreadQueue();

  UiThreadQueue(const UiThreadQueue&) = delete;
  UiThreadQueue& operator=(const UiThreadQueue&) = delete;

  // Process-wide instance used by the Controller and the services.
  static UiThreadQueue& Get();

  // Marks the calling thread as the UI thread and re-arms the queue after a
  // previous Shutdown(), so a second Controller in the same process works.
  void BindToCurrentThread();

  // True when the calling thread is the bound UI thread.
  bool IsUiThread() const;

  // True when RunOnUiThread() would run the task on the calling thread:
  // either the caller is the UI thread, or no UI thread is bound (CLI and
  // unit-test paths that never start a frame loop) and the queue is not
  // shut down.
  bool RunsInline() const;

  // Called when a Post() makes the queue non-empty, to interrupt a blocking
  // event wait. Later posts before the next Drain() do not call it again,
  // because that Drain() takes them too. It runs on the posting thread, so
  // it must be thread-safe and must not call back into this queue. Passing
  // an empty function blocks until any wake in progress has returned, so
  // the Controller can destroy the window backend safely right after
  // SetWaker({}).
  void SetWaker(std::function<void()> waker);

  // Queues a task for the next Drain(). Returns false, and drops the task,
  // after Shutdown().
  bool Post(std::function<void()> task);

  // Runs the tasks that were queued when Drain() began. Tasks posted while
  // draining wait for the next Drain(), so a task that re-posts itself
  // cannot starve the frame. Returns the number of tasks that ran.
  size_t Drain();

  bool HasPendingTasks() const;

  // Drops queued tasks, fails waiting RunOnUiThread() callers with
  // Cancelled, and makes later Post() and RunOnUiThread() calls fail fast
  // until BindToCurrentThread() is called again.
  void Shutdown();

  bool IsShutdown() const;

  /**
   * @brief Runs fn on the UI thread and returns its result.
   *
   * fn must return absl::Status or absl::StatusOr<T>.
   * - On the UI thread, or with no UI thread bound: runs inline.
   * - Otherwise: posts fn and waits up to timeout.
   * - Returns DeadlineExceeded if fn did not start before timeout. fn is
   *   then never run, so it may safely capture the caller's stack by
   *   reference. If fn already started, the call waits for it to finish.
   * - Returns Cancelled after Shutdown().
   */
  template <typename Fn, typename R = std::invoke_result_t<Fn&>>
  R RunOnUiThread(Fn fn, std::chrono::milliseconds timeout = kDefaultTimeout) {
    static_assert(std::is_same_v<R, absl::Status> || IsStatusOr<R>::value,
                  "RunOnUiThread requires a callable returning absl::Status "
                  "or absl::StatusOr<T>");
    // Shared ownership keeps the result slot alive if the waiter gives up
    // while the queue still holds the task.
    auto result = std::make_shared<std::optional<R>>();
    auto shared_fn = std::make_shared<Fn>(std::move(fn));
    absl::Status wait_status = RunAndWait(
        [result, shared_fn]() { result->emplace((*shared_fn)()); }, timeout);
    if (!wait_status.ok()) {
      return R(wait_status);
    }
    if (!result->has_value()) {
      return R(absl::InternalError("UI thread task produced no result"));
    }
    return std::move(**result);
  }

 private:
  template <typename T>
  struct IsStatusOr : std::false_type {};
  template <typename T>
  struct IsStatusOr<absl::StatusOr<T>> : std::true_type {};

  // State of one RunOnUiThread() call, shared by the waiter and the task.
  struct Call {
    enum class State { kPending, kRunning, kDone, kAbandoned, kCancelled };
    std::mutex mu;
    std::condition_variable cv;
    State state = State::kPending;
  };

  struct Entry {
    std::function<void()> task;
    // Set only for RunOnUiThread() entries, so Shutdown() can wake waiters.
    std::shared_ptr<Call> call;
  };

  absl::Status RunAndWait(std::function<void()> body,
                          std::chrono::milliseconds timeout);
  bool Enqueue(Entry entry);
  void Wake();

  mutable std::mutex mu_;
  std::deque<Entry> queue_;
  std::thread::id ui_thread_id_;
  bool bound_ = false;
  bool shutdown_ = false;

  // Separate from mu_ so a slow waker never blocks Drain() or Post().
  std::mutex waker_mu_;
  std::function<void()> waker_;
};

/**
 * @brief Inputs for the frame loop's idle decision.
 */
struct MainLoopIdleState {
  // Something needs continuous frames (animation, emulator, screenshot,
  // UI test).
  bool busy = false;
  // UiThreadQueue has tasks waiting for Drain().
  bool ui_tasks_pending = false;
  bool window_hidden = false;
  // Time since the last real window event.
  std::chrono::steady_clock::duration quiet_for{};
};

// Quiet time before a visible window starts to idle.
inline constexpr std::chrono::milliseconds kMainLoopIdleAfter{1500};
// Event wait used while a visible window idles.
inline constexpr int kMainLoopIdleWaitMs = 100;
// Event wait used while the window is hidden (service mode).
inline constexpr int kMainLoopHiddenWaitMs = 250;

/**
 * @brief Returns how long the frame loop may block waiting for an event.
 *
 * 0 means do not block. Queued UI tasks always force 0, so a task posted
 * just before the wait starts is still drained on the next frame even if
 * the wake event was consumed earlier.
 */
int MainLoopEventWaitMs(const MainLoopIdleState& state);

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_UI_THREAD_QUEUE_H_
