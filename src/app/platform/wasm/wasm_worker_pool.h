// clang-format off
#ifndef YAZE_APP_PLATFORM_WASM_WORKER_POOL_H
#define YAZE_APP_PLATFORM_WASM_WORKER_POOL_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <emscripten.h>
#include <emscripten/threading.h>
#endif

namespace yaze {
namespace app {
namespace platform {
namespace wasm {

/**
 * @brief Web Worker pool for offloading CPU-intensive operations.
 *
 * This class manages a pool of background workers (using pthreads in Emscripten)
 * to handle heavy processing tasks without blocking the UI thread. Supported
 * task types include:
 * - ROM decompression (LC-LZ2)
 * - Graphics sheet decoding
 * - Palette calculations
 * - Asar assembly compilation
 *
 * The implementation uses Emscripten's pthread support which maps to Web Workers
 * in the browser. Callbacks are executed on the main thread to ensure safe
 * UI updates.
 */
class WasmWorkerPool {
 public:
  // Task types that can be processed in background
  enum class TaskType {
    kRomDecompression,
    kGraphicsDecoding,
    kPaletteCalculation,
    kAsarCompilation,
    kCustom
  };

  // Task priority levels
  enum class Priority {
    kLow = 0,
    kNormal = 1,
    kHigh = 2,
    kCritical = 3
  };

  // Callback type for task completion
  using TaskCallback = std::function<void(bool success, const std::vector<uint8_t>& result)>;

  // Progress callback for long-running tasks
  using ProgressCallback = std::function<void(float progress, const std::string& message)>;

  // Task structure
  struct Task {
    uint32_t id;
    TaskType type;
    Priority priority;
    std::vector<uint8_t> input_data;
    TaskCallback completion_callback;
    ProgressCallback progress_callback;
    std::string type_string;  // For custom task types
    bool cancelled = false;
  };

  // Worker statistics
  struct WorkerStats {
    uint32_t tasks_completed = 0;
    uint32_t tasks_failed = 0;
    uint64_t total_processing_time_ms = 0;
    std::string current_task_type;
    bool is_busy = false;
  };

  // Special task ID returned when task is executed synchronously (no workers available)
  static constexpr uint32_t kSynchronousTaskId = UINT32_MAX;

  WasmWorkerPool(size_t num_workers = 0);  // 0 = auto-detect optimal count
  ~WasmWorkerPool();

  // Initialize the worker pool
  bool Initialize();

  // Shutdown the worker pool
  void Shutdown();

  /**
   * @brief Submit a task to the worker pool.
   *
   * @param type The type of task to process
   * @param input_data The input data for the task
   * @param callback Callback to invoke on completion (executed on main thread)
   * @param priority Task priority (higher priority tasks are processed first)
   * @return Task ID that can be used for cancellation
   */
  uint32_t SubmitTask(TaskType type,
                      const std::vector<uint8_t>& input_data,
                      TaskCallback callback,
                      Priority priority = Priority::kNormal);

  /**
   * @brief Submit a custom task type.
   *
   * @param type_string Custom task type identifier
   * @param input_data The input data for the task
   * @param callback Callback to invoke on completion
   * @param priority Task priority
   * @return Task ID
   */
  uint32_t SubmitCustomTask(const std::string& type_string,
                            const std::vector<uint8_t>& input_data,
                            TaskCallback callback,
                            Priority priority = Priority::kNormal);

  /**
   * @brief Submit a task with progress reporting.
   */
  uint32_t SubmitTaskWithProgress(TaskType type,
                                  const std::vector<uint8_t>& input_data,
                                  TaskCallback completion_callback,
                                  ProgressCallback progress_callback,
                                  Priority priority = Priority::kNormal);

  /**
   * @brief Cancel a pending task.
   *
   * @param task_id The task ID to cancel
   * @return true if task was cancelled, false if already running or completed
   */
  bool Cancel(uint32_t task_id);

  /**
   * @brief Cancel all pending tasks of a specific type.
   */
  void CancelAllOfType(TaskType type);

  /**
   * @brief Wait for all pending tasks to complete.
   *
   * @param timeout_ms Maximum time to wait in milliseconds (0 = infinite)
   * @return true if all tasks completed, false if timeout
   */
  bool WaitAll(uint32_t timeout_ms = 0);

  /**
   * @brief Get the number of pending tasks.
   */
  size_t GetPendingCount() const;

  /**
   * @brief Get the number of active workers.
   */
  size_t GetActiveWorkerCount() const;

  /**
   * @brief Get statistics for all workers.
   */
  std::vector<WorkerStats> GetWorkerStats() const;

  /**
   * @brief Check if the worker pool is initialized.
   */
  bool IsInitialized() const { return initialized_; }

  /**
   * @brief Set the maximum number of concurrent workers.
   */
  void SetMaxWorkers(size_t count);

  /**
   * @brief Process any pending callbacks on the main thread.
   * Should be called periodically from the main loop.
   */
  void ProcessCallbacks();

 private:
  // Worker thread function
  void WorkerThread(size_t worker_id);

  // Process a single task
  void ProcessTask(const Task& task, size_t worker_id);

  // Execute task based on type
  std::vector<uint8_t> ExecuteTask(const Task& task);

  // Task-specific processing functions
  std::vector<uint8_t> ProcessRomDecompression(const std::vector<uint8_t>& input);
  std::vector<uint8_t> ProcessGraphicsDecoding(const std::vector<uint8_t>& input);
  std::vector<uint8_t> ProcessPaletteCalculation(const std::vector<uint8_t>& input);
  std::vector<uint8_t> ProcessAsarCompilation(const std::vector<uint8_t>& input);

  // Report progress from worker thread
  void ReportProgress(uint32_t task_id, float progress, const std::string& message);

  // Queue a callback for execution on main thread
  void QueueCallback(std::function<void()> callback);

#ifdef __EMSCRIPTEN__
  // Emscripten-specific callback handler
  static void MainThreadCallbackHandler(void* arg);
#endif

  // Member variables
  bool initialized_ = false;
  bool shutting_down_ = false;
  size_t num_workers_;

#ifdef __EMSCRIPTEN__
  std::atomic<uint32_t> next_task_id_{1};

  // Worker threads
  std::vector<std::thread> workers_;
  std::vector<WorkerStats> worker_stats_;

  // Task queue (priority queue)
  struct TaskCompare {
    bool operator()(const std::shared_ptr<Task>& a, const std::shared_ptr<Task>& b) {
      // Higher priority first, then lower ID (FIFO within priority)
      if (a->priority != b->priority) {
        return static_cast<int>(a->priority) < static_cast<int>(b->priority);
      }
      return a->id > b->id;
    }
  };

  std::priority_queue<std::shared_ptr<Task>,
                      std::vector<std::shared_ptr<Task>>,
                      TaskCompare> task_queue_;

  // Active tasks map (task_id -> task)
  std::unordered_map<uint32_t, std::shared_ptr<Task>> active_tasks_;

  // Synchronization
  mutable std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::condition_variable completion_cv_;

  // Callback queue for main thread execution
  std::queue<std::function<void()>> callback_queue_;
  mutable std::mutex callback_mutex_;

  // Statistics
  std::atomic<size_t> active_workers_{0};
  std::atomic<size_t> total_tasks_submitted_{0};
  std::atomic<size_t> total_tasks_completed_{0};
#else
  // Stub members for non-Emscripten builds
  uint32_t next_task_id_{1};
  std::vector<WorkerStats> worker_stats_;
  size_t active_workers_{0};
  size_t total_tasks_submitted_{0};
  size_t total_tasks_completed_{0};
#endif
};

}  // namespace wasm
}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_PLATFORM_WASM_WORKER_POOL_H