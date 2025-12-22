// clang-format off
#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_worker_pool.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "absl/strings/str_format.h"

namespace yaze {
namespace app {
namespace platform {
namespace wasm {

namespace {

// Get optimal worker count based on hardware
size_t GetOptimalWorkerCount() {
#ifdef __EMSCRIPTEN__
  // In Emscripten, check navigator.hardwareConcurrency
  EM_ASM({
    if (typeof navigator !== 'undefined' && navigator.hardwareConcurrency) {
      Module['_yaze_hardware_concurrency'] = navigator.hardwareConcurrency;
    } else {
      Module['_yaze_hardware_concurrency'] = 4;  // Default fallback
    }
  });

  // Read the value set by JavaScript
  int concurrency = EM_ASM_INT({
    return Module['_yaze_hardware_concurrency'] || 4;
  });

  // Use half the available cores for workers, minimum 2, maximum 8
  return std::max(2, std::min(8, concurrency / 2));
#else
  // Native platform
  unsigned int hw_threads = std::thread::hardware_concurrency();
  if (hw_threads == 0) hw_threads = 4;  // Fallback
  return std::max(2u, std::min(8u, hw_threads / 2));
#endif
}

}  // namespace

WasmWorkerPool::WasmWorkerPool(size_t num_workers)
    : num_workers_(num_workers == 0 ? GetOptimalWorkerCount() : num_workers) {
  worker_stats_.resize(num_workers_);
}

WasmWorkerPool::~WasmWorkerPool() {
  if (initialized_) {
    Shutdown();
  }
}

bool WasmWorkerPool::Initialize() {
  if (initialized_) {
    return true;
  }

#ifdef __EMSCRIPTEN__
  // Check if SharedArrayBuffer is available (required for pthreads)
  bool has_shared_array_buffer = EM_ASM_INT({
    return typeof SharedArrayBuffer !== 'undefined';
  });

  if (!has_shared_array_buffer) {
    std::cerr << "WasmWorkerPool: SharedArrayBuffer not available. "
              << "Workers will run in degraded mode.\n";
    // Could fall back to single-threaded mode or use postMessage-based workers
    // For now, we'll proceed but with reduced functionality
    num_workers_ = 0;
    initialized_ = true;
    return true;
  }

  // Log initialization
  EM_ASM({
    console.log('WasmWorkerPool: Initializing with', $0, 'workers');
  }, num_workers_);
#endif

  // Create worker threads
  workers_.reserve(num_workers_);
  for (size_t i = 0; i < num_workers_; ++i) {
    workers_.emplace_back(&WasmWorkerPool::WorkerThread, this, i);
  }

  initialized_ = true;
  return true;
}

void WasmWorkerPool::Shutdown() {
  if (!initialized_) {
    return;
  }

  // Signal shutdown
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    shutting_down_ = true;
  }
  queue_cv_.notify_all();

  // Wait for all workers to finish
  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }

  workers_.clear();

  // Clear any remaining tasks
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!task_queue_.empty()) {
      task_queue_.pop();
    }
    active_tasks_.clear();
  }

  initialized_ = false;
}

uint32_t WasmWorkerPool::SubmitTask(TaskType type,
                                    const std::vector<uint8_t>& input_data,
                                    TaskCallback callback,
                                    Priority priority) {
  return SubmitTaskWithProgress(type, input_data, callback, nullptr, priority);
}

uint32_t WasmWorkerPool::SubmitCustomTask(const std::string& type_string,
                                          const std::vector<uint8_t>& input_data,
                                          TaskCallback callback,
                                          Priority priority) {
  auto task = std::make_shared<Task>();
  task->id = next_task_id_++;
  task->type = TaskType::kCustom;
  task->type_string = type_string;
  task->priority = priority;
  task->input_data = input_data;
  task->completion_callback = callback;

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    active_tasks_[task->id] = task;
    task_queue_.push(task);
    total_tasks_submitted_++;
  }

  queue_cv_.notify_one();
  return task->id;
}

uint32_t WasmWorkerPool::SubmitTaskWithProgress(TaskType type,
                                                const std::vector<uint8_t>& input_data,
                                                TaskCallback completion_callback,
                                                ProgressCallback progress_callback,
                                                Priority priority) {
  // If no workers available, execute synchronously
  if (num_workers_ == 0 || !initialized_) {
    if (completion_callback) {
      try {
        auto task = std::make_shared<Task>();
        task->type = type;
        task->input_data = input_data;
        auto result = ExecuteTask(*task);
        completion_callback(true, result);
      } catch (const std::exception& e) {
        completion_callback(false, std::vector<uint8_t>());
      }
    }
    // Return special ID to indicate synchronous execution (task already completed)
    return kSynchronousTaskId;
  }

  auto task = std::make_shared<Task>();
  task->id = next_task_id_++;
  task->type = type;
  task->priority = priority;
  task->input_data = input_data;
  task->completion_callback = completion_callback;
  task->progress_callback = progress_callback;

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    active_tasks_[task->id] = task;
    task_queue_.push(task);
    total_tasks_submitted_++;
  }

  queue_cv_.notify_one();
  return task->id;
}

bool WasmWorkerPool::Cancel(uint32_t task_id) {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  auto it = active_tasks_.find(task_id);
  if (it != active_tasks_.end()) {
    it->second->cancelled = true;
    return true;
  }

  return false;
}

void WasmWorkerPool::CancelAllOfType(TaskType type) {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  for (auto& [id, task] : active_tasks_) {
    if (task->type == type) {
      task->cancelled = true;
    }
  }
}

bool WasmWorkerPool::WaitAll(uint32_t timeout_ms) {
  auto start = std::chrono::steady_clock::now();

  while (true) {
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      if (task_queue_.empty() && active_workers_ == 0) {
        return true;
      }
    }

    if (timeout_ms > 0) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start).count();
      if (elapsed >= timeout_ms) {
        return false;
      }
    }

    std::unique_lock<std::mutex> lock(queue_mutex_);
    completion_cv_.wait_for(lock, std::chrono::milliseconds(100));
  }
}

size_t WasmWorkerPool::GetPendingCount() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return task_queue_.size();
}

size_t WasmWorkerPool::GetActiveWorkerCount() const {
  return active_workers_.load();
}

std::vector<WasmWorkerPool::WorkerStats> WasmWorkerPool::GetWorkerStats() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return worker_stats_;
}

void WasmWorkerPool::SetMaxWorkers(size_t count) {
  // This would require stopping and restarting workers
  // For simplicity, we'll just store the value for next initialization
  if (!initialized_) {
    num_workers_ = count;
  }
}

void WasmWorkerPool::ProcessCallbacks() {
  std::queue<std::function<void()>> callbacks_to_process;

  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callbacks_to_process.swap(callback_queue_);
  }

  while (!callbacks_to_process.empty()) {
    callbacks_to_process.front()();
    callbacks_to_process.pop();
  }
}

void WasmWorkerPool::WorkerThread(size_t worker_id) {
#ifdef __EMSCRIPTEN__
  // Set thread name for debugging
  emscripten_set_thread_name(pthread_self(),
      absl::StrFormat("YazeWorker%zu", worker_id).c_str());
#endif

  while (true) {
    std::shared_ptr<Task> task;

    // Get next task from queue
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait(lock, [this] {
        return shutting_down_ || !task_queue_.empty();
      });

      if (shutting_down_ && task_queue_.empty()) {
        break;
      }

      if (!task_queue_.empty()) {
        task = task_queue_.top();
        task_queue_.pop();
        active_workers_++;
        worker_stats_[worker_id].is_busy = true;
        worker_stats_[worker_id].current_task_type =
            task->type == TaskType::kCustom ? task->type_string :
            absl::StrFormat("Type%d", static_cast<int>(task->type));
      }
    }

    if (task && !task->cancelled) {
      ProcessTask(*task, worker_id);
    }

    // Clean up
    if (task) {
      {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        active_tasks_.erase(task->id);
        active_workers_--;
        worker_stats_[worker_id].is_busy = false;
        worker_stats_[worker_id].current_task_type.clear();
      }
      completion_cv_.notify_all();
    }
  }
}

void WasmWorkerPool::ProcessTask(const Task& task, size_t worker_id) {
  auto start_time = std::chrono::steady_clock::now();
  bool success = false;
  std::vector<uint8_t> result;

  try {
    // Report starting
    if (task.progress_callback) {
      ReportProgress(task.id, 0.0f, "Starting task...");
    }

    // Execute the task
    result = ExecuteTask(task);
    success = true;

    // Update stats
    worker_stats_[worker_id].tasks_completed++;

    // Report completion
    if (task.progress_callback) {
      ReportProgress(task.id, 1.0f, "Task completed");
    }
  } catch (const std::exception& e) {
    std::cerr << "Worker " << worker_id << " task failed: " << e.what() << std::endl;
    worker_stats_[worker_id].tasks_failed++;
    success = false;
  }

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_time).count();
  worker_stats_[worker_id].total_processing_time_ms += elapsed;

  // Queue callback for main thread execution
  if (task.completion_callback && !task.cancelled) {
    QueueCallback([callback = task.completion_callback, success, result]() {
      callback(success, result);
    });
  }

  total_tasks_completed_++;
}

std::vector<uint8_t> WasmWorkerPool::ExecuteTask(const Task& task) {
  switch (task.type) {
    case TaskType::kRomDecompression:
      return ProcessRomDecompression(task.input_data);

    case TaskType::kGraphicsDecoding:
      return ProcessGraphicsDecoding(task.input_data);

    case TaskType::kPaletteCalculation:
      return ProcessPaletteCalculation(task.input_data);

    case TaskType::kAsarCompilation:
      return ProcessAsarCompilation(task.input_data);

    case TaskType::kCustom:
      // For custom tasks, just return the input as we don't know how to process it
      // Real implementation would need a registry of custom processors
      return task.input_data;

    default:
      throw std::runtime_error("Unknown task type");
  }
}

std::vector<uint8_t> WasmWorkerPool::ProcessRomDecompression(const std::vector<uint8_t>& input) {
  // Placeholder for LC-LZ2 decompression
  // In real implementation, this would call the actual decompression routine
  // from src/app/gfx/compression.cc

  // For now, simulate some work
  std::vector<uint8_t> result;
  result.reserve(input.size() * 2);  // Assume 2x expansion

  // Simulate decompression (just duplicate data for testing)
  for (size_t i = 0; i < input.size(); ++i) {
    result.push_back(input[i]);
    result.push_back(input[i] ^ 0xFF);  // Inverted copy

    // Simulate progress reporting
    if (i % 1000 == 0) {
      float progress = static_cast<float>(i) / input.size();
      // Would call ReportProgress here if we had the task ID
    }
  }

  return result;
}

std::vector<uint8_t> WasmWorkerPool::ProcessGraphicsDecoding(const std::vector<uint8_t>& input) {
  // Placeholder for graphics sheet decoding
  // In real implementation, this would decode SNES tile formats

  std::vector<uint8_t> result;
  result.reserve(input.size());

  // Simulate processing
  for (uint8_t byte : input) {
    // Simple transformation to simulate work
    result.push_back((byte << 1) | (byte >> 7));
  }

  return result;
}

std::vector<uint8_t> WasmWorkerPool::ProcessPaletteCalculation(const std::vector<uint8_t>& input) {
  // Placeholder for palette calculations
  // In real implementation, this would process SNES color formats

  std::vector<uint8_t> result;

  // Process in groups of 2 bytes (SNES color format)
  for (size_t i = 0; i + 1 < input.size(); i += 2) {
    uint16_t snes_color = (input[i + 1] << 8) | input[i];

    // Extract RGB components (5 bits each)
    uint8_t r = (snes_color & 0x1F) << 3;
    uint8_t g = ((snes_color >> 5) & 0x1F) << 3;
    uint8_t b = ((snes_color >> 10) & 0x1F) << 3;

    // Store as RGB24
    result.push_back(r);
    result.push_back(g);
    result.push_back(b);
  }

  return result;
}

std::vector<uint8_t> WasmWorkerPool::ProcessAsarCompilation(const std::vector<uint8_t>& input) {
  // Placeholder for Asar assembly compilation
  // In real implementation, this would call the Asar wrapper

  // For now, return empty result (compilation succeeded with no output)
  return std::vector<uint8_t>();
}

void WasmWorkerPool::ReportProgress(uint32_t task_id, float progress, const std::string& message) {
  // Find the task
  std::shared_ptr<Task> task;
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    auto it = active_tasks_.find(task_id);
    if (it != active_tasks_.end()) {
      task = it->second;
    }
  }

  if (task && task->progress_callback && !task->cancelled) {
    QueueCallback([callback = task->progress_callback, progress, message]() {
      callback(progress, message);
    });
  }
}

void WasmWorkerPool::QueueCallback(std::function<void()> callback) {
#ifdef __EMSCRIPTEN__
  // In Emscripten, we need to execute callbacks on the main thread
  // Use emscripten_async_run_in_main_runtime_thread for thread safety

  auto* callback_ptr = new std::function<void()>(std::move(callback));

  emscripten_async_run_in_main_runtime_thread(
      EM_FUNC_SIG_VI,
      &WasmWorkerPool::MainThreadCallbackHandler,
      callback_ptr);
#else
  // For non-Emscripten builds, just queue for later processing
  std::lock_guard<std::mutex> lock(callback_mutex_);
  callback_queue_.push(callback);
#endif
}

#ifdef __EMSCRIPTEN__
void WasmWorkerPool::MainThreadCallbackHandler(void* arg) {
  auto* callback_ptr = static_cast<std::function<void()>*>(arg);
  if (callback_ptr) {
    (*callback_ptr)();
    delete callback_ptr;
  }
}
#endif

}  // namespace wasm
}  // namespace platform
}  // namespace app
}  // namespace yaze

#else  // !__EMSCRIPTEN__

// Stub implementation for non-Emscripten builds
#include "app/platform/wasm/wasm_worker_pool.h"

namespace yaze {
namespace app {
namespace platform {
namespace wasm {

WasmWorkerPool::WasmWorkerPool(size_t num_workers) : num_workers_(0) {}
WasmWorkerPool::~WasmWorkerPool() {}

bool WasmWorkerPool::Initialize() { return false; }
void WasmWorkerPool::Shutdown() {}

uint32_t WasmWorkerPool::SubmitTask(TaskType type,
                                    const std::vector<uint8_t>& input_data,
                                    TaskCallback callback,
                                    Priority priority) {
  // No-op in non-WASM builds
  if (callback) {
    callback(false, std::vector<uint8_t>());
  }
  return 0;
}

uint32_t WasmWorkerPool::SubmitCustomTask(const std::string& type_string,
                                          const std::vector<uint8_t>& input_data,
                                          TaskCallback callback,
                                          Priority priority) {
  if (callback) {
    callback(false, std::vector<uint8_t>());
  }
  return 0;
}

uint32_t WasmWorkerPool::SubmitTaskWithProgress(TaskType type,
                                                const std::vector<uint8_t>& input_data,
                                                TaskCallback completion_callback,
                                                ProgressCallback progress_callback,
                                                Priority priority) {
  if (completion_callback) {
    completion_callback(false, std::vector<uint8_t>());
  }
  return 0;
}

bool WasmWorkerPool::Cancel(uint32_t task_id) { return false; }
void WasmWorkerPool::CancelAllOfType(TaskType type) {}
bool WasmWorkerPool::WaitAll(uint32_t timeout_ms) { return true; }
size_t WasmWorkerPool::GetPendingCount() const { return 0; }
size_t WasmWorkerPool::GetActiveWorkerCount() const { return 0; }
std::vector<WasmWorkerPool::WorkerStats> WasmWorkerPool::GetWorkerStats() const { return {}; }
void WasmWorkerPool::SetMaxWorkers(size_t count) {}
void WasmWorkerPool::ProcessCallbacks() {}

void WasmWorkerPool::WorkerThread(size_t worker_id) {}
void WasmWorkerPool::ProcessTask(const Task& task, size_t worker_id) {}
std::vector<uint8_t> WasmWorkerPool::ExecuteTask(const Task& task) { return {}; }
std::vector<uint8_t> WasmWorkerPool::ProcessRomDecompression(const std::vector<uint8_t>& input) { return {}; }
std::vector<uint8_t> WasmWorkerPool::ProcessGraphicsDecoding(const std::vector<uint8_t>& input) { return {}; }
std::vector<uint8_t> WasmWorkerPool::ProcessPaletteCalculation(const std::vector<uint8_t>& input) { return {}; }
std::vector<uint8_t> WasmWorkerPool::ProcessAsarCompilation(const std::vector<uint8_t>& input) { return {}; }
void WasmWorkerPool::ReportProgress(uint32_t task_id, float progress, const std::string& message) {}
void WasmWorkerPool::QueueCallback(std::function<void()> callback) {
  if (callback) callback();
}

}  // namespace wasm
}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__