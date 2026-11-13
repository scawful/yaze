#include "util/ui_thread_queue.h"

#include <string>
#include <utility>

namespace yaze {
namespace util {

UiThreadQueue::~UiThreadQueue() {
  // Wake any caller still waiting so it returns Cancelled instead of
  // waiting out its timeout. Waiters hold their Call by shared_ptr, so they
  // do not touch this object after waking.
  Shutdown();
}

UiThreadQueue& UiThreadQueue::Get() {
  static UiThreadQueue* instance = new UiThreadQueue();
  // Leaked on purpose: gRPC and HTTP threads can still call into the queue
  // while static destructors run at exit.
  return *instance;
}

void UiThreadQueue::BindToCurrentThread() {
  std::lock_guard<std::mutex> lock(mu_);
  ui_thread_id_ = std::this_thread::get_id();
  bound_ = true;
  shutdown_ = false;
}

bool UiThreadQueue::IsUiThread() const {
  std::lock_guard<std::mutex> lock(mu_);
  return bound_ && ui_thread_id_ == std::this_thread::get_id();
}

bool UiThreadQueue::RunsInline() const {
  std::lock_guard<std::mutex> lock(mu_);
  if (shutdown_) {
    return false;
  }
  if (bound_) {
    return ui_thread_id_ == std::this_thread::get_id();
  }
  return true;
}

void UiThreadQueue::SetWaker(std::function<void()> waker) {
  // Taking waker_mu_ waits for any Wake() in progress, so the old waker's
  // captures (the window backend) are no longer in use when this returns.
  std::lock_guard<std::mutex> lock(waker_mu_);
  waker_ = std::move(waker);
}

bool UiThreadQueue::Post(std::function<void()> task) {
  if (!task) {
    return false;
  }
  return Enqueue(Entry{std::move(task), nullptr});
}

bool UiThreadQueue::Enqueue(Entry entry) {
  bool was_empty = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (shutdown_) {
      return false;
    }
    was_empty = queue_.empty();
    queue_.push_back(std::move(entry));
  }
  // Only the first task after a Drain() needs a wake: a non-empty queue
  // already has a wake in flight, and the next Drain() takes every task.
  // This keeps a burst of agent calls from flooding the SDL event queue.
  // Wake outside mu_ so the waker cannot deadlock with Drain().
  if (was_empty) {
    Wake();
  }
  return true;
}

void UiThreadQueue::Wake() {
  std::lock_guard<std::mutex> lock(waker_mu_);
  if (waker_) {
    waker_();
  }
}

size_t UiThreadQueue::Drain() {
  std::deque<Entry> batch;
  {
    std::lock_guard<std::mutex> lock(mu_);
    batch.swap(queue_);
  }
  // Tasks run without mu_ held, so they may Post() follow-up work; that
  // work lands in queue_ and waits for the next Drain().
  for (Entry& entry : batch) {
    entry.task();
  }
  return batch.size();
}

bool UiThreadQueue::HasPendingTasks() const {
  std::lock_guard<std::mutex> lock(mu_);
  return !queue_.empty();
}

void UiThreadQueue::Shutdown() {
  std::deque<Entry> dropped;
  {
    std::lock_guard<std::mutex> lock(mu_);
    shutdown_ = true;
    dropped.swap(queue_);
  }
  for (Entry& entry : dropped) {
    if (!entry.call) {
      continue;
    }
    {
      std::lock_guard<std::mutex> lock(entry.call->mu);
      if (entry.call->state == Call::State::kPending) {
        entry.call->state = Call::State::kCancelled;
      }
    }
    entry.call->cv.notify_all();
  }
}

bool UiThreadQueue::IsShutdown() const {
  std::lock_guard<std::mutex> lock(mu_);
  return shutdown_;
}

absl::Status UiThreadQueue::RunAndWait(std::function<void()> body,
                                       std::chrono::milliseconds timeout) {
  bool run_inline = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (shutdown_) {
      return absl::CancelledError("UI thread queue is shut down");
    } else if (bound_ && ui_thread_id_ == std::this_thread::get_id()) {
      // Posting from the UI thread and waiting would deadlock.
      run_inline = true;
    } else if (!bound_) {
      // No frame loop exists to drain the queue (CLI, unit tests).
      run_inline = true;
    }
  }
  if (run_inline) {
    body();
    return absl::OkStatus();
  }

  auto call = std::make_shared<Call>();
  Entry entry;
  entry.call = call;
  entry.task = [call, body = std::move(body)]() {
    {
      std::lock_guard<std::mutex> lock(call->mu);
      // An abandoned call's waiter has returned; body may reference its
      // stack, so it must not run.
      if (call->state != Call::State::kPending) {
        return;
      }
      call->state = Call::State::kRunning;
    }
    body();
    {
      std::lock_guard<std::mutex> lock(call->mu);
      call->state = Call::State::kDone;
    }
    call->cv.notify_all();
  };
  if (!Enqueue(std::move(entry))) {
    return absl::CancelledError("UI thread queue is shut down");
  }

  std::unique_lock<std::mutex> lock(call->mu);
  const bool finished = call->cv.wait_for(lock, timeout, [&call]() {
    return call->state == Call::State::kDone ||
           call->state == Call::State::kCancelled;
  });
  if (!finished) {
    if (call->state == Call::State::kPending) {
      call->state = Call::State::kAbandoned;
      return absl::DeadlineExceededError(
          "UI thread did not run the task within " +
          std::to_string(timeout.count()) + " ms");
    }
    // The task already started; its captures point at this stack, so
    // returning now would let it write to freed memory.
    call->cv.wait(lock,
                  [&call]() { return call->state == Call::State::kDone; });
  }
  if (call->state == Call::State::kCancelled) {
    return absl::CancelledError(
        "UI thread queue shut down before the task ran");
  }
  return absl::OkStatus();
}

int MainLoopEventWaitMs(const MainLoopIdleState& state) {
  if (state.busy || state.ui_tasks_pending) {
    return 0;
  }
  if (state.window_hidden) {
    return kMainLoopHiddenWaitMs;
  }
  if (state.quiet_for >= kMainLoopIdleAfter) {
    return kMainLoopIdleWaitMs;
  }
  return 0;
}

}  // namespace util
}  // namespace yaze
