#include "util/ui_thread_queue.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"

namespace yaze {
namespace util {
namespace {

using std::chrono::milliseconds;

// Polls pred until it is true or the limit passes. Used instead of fixed
// sleeps so the tests stay fast.
bool WaitUntil(const std::function<bool()>& pred,
               milliseconds limit = milliseconds(5000)) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (!pred()) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::sleep_for(milliseconds(1));
  }
  return true;
}

TEST(UiThreadQueueTest, PostFromWorkerRunsOnBoundThreadDuringDrain) {
  UiThreadQueue queue;
  queue.BindToCurrentThread();
  const auto ui_id = std::this_thread::get_id();

  std::thread::id ran_on;
  bool posted = false;
  std::thread worker([&]() {
    posted = queue.Post([&]() { ran_on = std::this_thread::get_id(); });
  });
  worker.join();

  ASSERT_TRUE(posted);
  EXPECT_TRUE(queue.HasPendingTasks());
  EXPECT_EQ(queue.Drain(), 1u);
  EXPECT_EQ(ran_on, ui_id);
  EXPECT_FALSE(queue.HasPendingTasks());
}

TEST(UiThreadQueueTest, WakerCalledWhenPostMakesQueueNonEmpty) {
  UiThreadQueue queue;
  queue.BindToCurrentThread();
  std::atomic<int> wakes{0};
  queue.SetWaker([&wakes]() { ++wakes; });

  ASSERT_TRUE(queue.Post([]() {}));
  EXPECT_EQ(wakes.load(), 1);
  // The queue already has a wake in flight; the next Drain() takes both.
  ASSERT_TRUE(queue.Post([]() {}));
  EXPECT_EQ(wakes.load(), 1);

  EXPECT_EQ(queue.Drain(), 2u);
  ASSERT_TRUE(queue.Post([]() {}));
  EXPECT_EQ(wakes.load(), 2);

  // Clearing the waker stops further calls.
  queue.Drain();
  queue.SetWaker({});
  ASSERT_TRUE(queue.Post([]() {}));
  EXPECT_EQ(wakes.load(), 2);
}

TEST(UiThreadQueueTest, RunOnUiThreadRunsInlineOnBoundThread) {
  UiThreadQueue queue;
  queue.BindToCurrentThread();
  EXPECT_TRUE(queue.IsUiThread());
  EXPECT_TRUE(queue.RunsInline());

  bool ran = false;
  absl::Status status = queue.RunOnUiThread([&ran]() {
    ran = true;
    return absl::OkStatus();
  });
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(ran);
  EXPECT_FALSE(queue.HasPendingTasks());

  absl::StatusOr<int> value =
      queue.RunOnUiThread([]() -> absl::StatusOr<int> { return 42; });
  ASSERT_TRUE(value.ok());
  EXPECT_EQ(*value, 42);

  // The task's own error is returned unchanged.
  absl::Status error =
      queue.RunOnUiThread([]() { return absl::NotFoundError("no canvas"); });
  EXPECT_EQ(error.code(), absl::StatusCode::kNotFound);
}

TEST(UiThreadQueueTest, RunOnUiThreadRunsInlineWhenUnbound) {
  UiThreadQueue queue;
  EXPECT_FALSE(queue.IsUiThread());
  EXPECT_TRUE(queue.RunsInline());

  std::thread::id caller_id;
  std::thread::id ran_on;
  absl::Status status;
  std::thread worker([&]() {
    caller_id = std::this_thread::get_id();
    status = queue.RunOnUiThread([&]() {
      ran_on = std::this_thread::get_id();
      return absl::OkStatus();
    });
  });
  worker.join();

  EXPECT_TRUE(status.ok());
  EXPECT_EQ(ran_on, caller_id);
  EXPECT_FALSE(queue.HasPendingTasks());
}

TEST(UiThreadQueueTest, RunOnUiThreadFromWorkerRunsOnDrain) {
  UiThreadQueue queue;
  queue.BindToCurrentThread();
  const auto ui_id = std::this_thread::get_id();

  std::thread::id ran_on;
  absl::StatusOr<int> result = absl::UnknownError("not set");
  std::atomic<bool> done{false};
  std::thread worker([&]() {
    EXPECT_FALSE(queue.RunsInline());
    result = queue.RunOnUiThread([&]() -> absl::StatusOr<int> {
      ran_on = std::this_thread::get_id();
      return 7;
    });
    done = true;
  });

  // Act as the frame loop: drain until the worker's call completes.
  const bool finished = WaitUntil([&]() {
    queue.Drain();
    return done.load();
  });
  worker.join();

  ASSERT_TRUE(finished);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(*result, 7);
  EXPECT_EQ(ran_on, ui_id);
}

TEST(UiThreadQueueTest, MoveOnlyCallableIsAccepted) {
  UiThreadQueue queue;
  auto payload = std::make_unique<int>(5);
  absl::StatusOr<int> value = queue.RunOnUiThread(
      [p = std::move(payload)]() -> absl::StatusOr<int> { return *p; });
  ASSERT_TRUE(value.ok());
  EXPECT_EQ(*value, 5);
}

TEST(UiThreadQueueTest, TimeoutReturnsDeadlineExceededAndLateTaskIsSkipped) {
  UiThreadQueue queue;
  queue.BindToCurrentThread();

  // Shared flag outlives the worker, so the check below never reads the
  // worker's stack.
  auto ran = std::make_shared<std::atomic<bool>>(false);
  absl::Status status;
  std::thread worker([&queue, &status, ran]() {
    int caller_local = 0;
    status = queue.RunOnUiThread(
        [&caller_local, ran]() {
          // Writing caller_local after the worker returned would be a
          // use-after-return; the queue must never get here.
          caller_local = 1;
          *ran = true;
          return absl::OkStatus();
        },
        milliseconds(20));
  });
  worker.join();

  EXPECT_EQ(status.code(), absl::StatusCode::kDeadlineExceeded);
  // The abandoned entry is still queued; draining it must skip the body.
  EXPECT_TRUE(queue.HasPendingTasks());
  queue.Drain();
  EXPECT_FALSE(ran->load());
  EXPECT_FALSE(queue.HasPendingTasks());
}

// The caller's timeout and a task runtime past it. Namespace scope so the
// task lambda can use them without a capture.
constexpr milliseconds kAwaitTimeout(500);
constexpr milliseconds kAwaitTaskRuntime(700);

TEST(UiThreadQueueTest, TaskStartedBeforeTimeoutIsAwaited) {
  UiThreadQueue queue;
  queue.BindToCurrentThread();

  absl::StatusOr<int> result = absl::UnknownError("not set");
  std::atomic<bool> task_started{false};
  std::chrono::steady_clock::time_point call_start;
  std::thread worker([&]() {
    call_start = std::chrono::steady_clock::now();
    result = queue.RunOnUiThread(
        [&task_started]() -> absl::StatusOr<int> {
          task_started.store(true);
          // Runs past the caller's timeout. The caller must wait instead of
          // returning while this task can still write its result.
          std::this_thread::sleep_for(kAwaitTaskRuntime);
          return 9;
        },
        kAwaitTimeout);
  });

  ASSERT_TRUE(WaitUntil([&]() { return queue.HasPendingTasks(); }));
  const auto drain_start = std::chrono::steady_clock::now();
  queue.Drain();
  worker.join();

  // Skip only if the caller abandoned an unstarted task before Drain began.
  // Once the task starts, any DeadlineExceeded is a real failure.
  if (result.status().code() == absl::StatusCode::kDeadlineExceeded &&
      !task_started.load() && drain_start - call_start >= kAwaitTimeout) {
    GTEST_SKIP() << "Drain() ran after the caller's timeout; scheduler stall";
  }
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(*result, 9);
}

TEST(UiThreadQueueTest, ShutdownCancelsWaitingCallerAndLaterCallsFailFast) {
  UiThreadQueue queue;
  queue.BindToCurrentThread();

  auto ran = std::make_shared<std::atomic<bool>>(false);
  absl::Status status;
  std::thread worker([&queue, &status, ran]() {
    status = queue.RunOnUiThread(
        [ran]() {
          *ran = true;
          return absl::OkStatus();
        },
        milliseconds(10000));
  });

  ASSERT_TRUE(WaitUntil([&]() { return queue.HasPendingTasks(); }));
  queue.Shutdown();
  worker.join();

  EXPECT_EQ(status.code(), absl::StatusCode::kCancelled);
  EXPECT_FALSE(ran->load());
  EXPECT_TRUE(queue.IsShutdown());
  EXPECT_FALSE(queue.HasPendingTasks());

  // The bound owner also fails instead of taking the inline path.
  EXPECT_FALSE(queue.RunsInline());
  const absl::Status owner_status = queue.RunOnUiThread([ran]() {
    *ran = true;
    return absl::OkStatus();
  });
  EXPECT_EQ(owner_status.code(), absl::StatusCode::kCancelled);
  EXPECT_FALSE(ran->load());

  // Later calls from other threads fail without waiting for a timeout.
  absl::Status late;
  auto elapsed = std::chrono::steady_clock::duration::zero();
  std::thread late_worker([&]() {
    EXPECT_FALSE(queue.RunsInline());
    const auto start = std::chrono::steady_clock::now();
    late = queue.RunOnUiThread([]() { return absl::OkStatus(); },
                               milliseconds(10000));
    elapsed = std::chrono::steady_clock::now() - start;
  });
  late_worker.join();
  EXPECT_EQ(late.code(), absl::StatusCode::kCancelled);
  EXPECT_LT(elapsed, milliseconds(1000));
  EXPECT_FALSE(queue.Post([]() {}));
  EXPECT_FALSE(queue.HasPendingTasks());

  // A new frame loop re-arms the queue.
  queue.BindToCurrentThread();
  EXPECT_FALSE(queue.IsShutdown());
  EXPECT_TRUE(queue.RunsInline());
  EXPECT_TRUE(queue.RunOnUiThread([]() { return absl::OkStatus(); }).ok());
  EXPECT_TRUE(queue.Post([]() {}));
  EXPECT_EQ(queue.Drain(), 1u);
}

TEST(UiThreadQueueTest, ShutdownRejectsUnboundInlineCalls) {
  UiThreadQueue queue;
  queue.Shutdown();

  bool ran = false;
  EXPECT_FALSE(queue.RunsInline());
  const absl::Status status = queue.RunOnUiThread([&ran]() {
    ran = true;
    return absl::OkStatus();
  });
  EXPECT_EQ(status.code(), absl::StatusCode::kCancelled);
  EXPECT_FALSE(ran);
}

TEST(UiThreadQueueTest, TaskPostedDuringDrainRunsOnNextDrain) {
  UiThreadQueue queue;
  queue.BindToCurrentThread();

  bool second_ran = false;
  ASSERT_TRUE(queue.Post(
      [&]() { EXPECT_TRUE(queue.Post([&]() { second_ran = true; })); }));

  EXPECT_EQ(queue.Drain(), 1u);
  EXPECT_FALSE(second_ran);
  EXPECT_TRUE(queue.HasPendingTasks());

  EXPECT_EQ(queue.Drain(), 1u);
  EXPECT_TRUE(second_ran);
  EXPECT_FALSE(queue.HasPendingTasks());
}

TEST(MainLoopEventWaitMsTest, BusyOrPendingTasksNeverBlock) {
  MainLoopIdleState state;
  state.quiet_for = std::chrono::seconds(10);

  state.busy = true;
  EXPECT_EQ(MainLoopEventWaitMs(state), 0);

  state.busy = false;
  state.ui_tasks_pending = true;
  state.window_hidden = true;
  EXPECT_EQ(MainLoopEventWaitMs(state), 0);
}

TEST(MainLoopEventWaitMsTest, HiddenWindowWaitsLongest) {
  MainLoopIdleState state;
  state.window_hidden = true;
  EXPECT_EQ(MainLoopEventWaitMs(state), kMainLoopHiddenWaitMs);
}

TEST(MainLoopEventWaitMsTest, VisibleWindowIdlesOnlyAfterQuietPeriod) {
  MainLoopIdleState state;
  state.quiet_for = kMainLoopIdleAfter - milliseconds(1);
  EXPECT_EQ(MainLoopEventWaitMs(state), 0);

  state.quiet_for = kMainLoopIdleAfter;
  EXPECT_EQ(MainLoopEventWaitMs(state), kMainLoopIdleWaitMs);
}

}  // namespace
}  // namespace util
}  // namespace yaze
