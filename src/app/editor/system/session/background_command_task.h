#ifndef YAZE_APP_EDITOR_SYSTEM_BACKGROUND_COMMAND_TASK_H_
#define YAZE_APP_EDITOR_SYSTEM_BACKGROUND_COMMAND_TASK_H_

#include <mutex>
#include <string>
#include <thread>

#include "absl/status/status.h"

namespace yaze::editor {

class BackgroundCommandTask {
 public:
  struct Snapshot {
    bool started = false;
    bool running = false;
    bool finished = false;
    bool cancel_requested = false;
    bool can_cancel = false;
    int exit_code = -1;
    std::string command;
    std::string directory;
    std::string output;
    std::string output_tail;
    absl::Status status = absl::UnknownError("Task not started");
  };

  BackgroundCommandTask() = default;
  ~BackgroundCommandTask();

  BackgroundCommandTask(const BackgroundCommandTask&) = delete;
  BackgroundCommandTask& operator=(const BackgroundCommandTask&) = delete;

  absl::Status Start(const std::string& command, const std::string& directory);
  void Cancel();
  Snapshot GetSnapshot() const;
  absl::Status Wait();

 private:
  void WorkerMain(std::string command, std::string directory);
  void AppendOutput(const char* data, size_t size);
  void Finalize(absl::Status status, int exit_code);
  void JoinIfNeeded();

  static std::string ComputeOutputTail(const std::string& output);

  mutable std::mutex mutex_;
  std::thread worker_;
  bool started_ = false;
  bool running_ = false;
  bool finished_ = false;
  bool cancel_requested_ = false;
  int exit_code_ = -1;
  int child_pid_ = -1;
  std::string command_;
  std::string directory_;
  std::string output_;
  std::string output_tail_;
  absl::Status status_ = absl::UnknownError("Task not started");
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_BACKGROUND_COMMAND_TASK_H_
