#ifndef YAZE_APP_CORE_TESTING_TEST_RECORDER_H_
#define YAZE_APP_CORE_TESTING_TEST_RECORDER_H_

#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "app/test/test_manager.h"

namespace yaze {
namespace test {

class TestManager;

// Recorder responsible for capturing GUI automation RPCs and exporting them as
// replayable JSON test scripts.
class TestRecorder {
 public:
  enum class ActionType {
    kUnknown,
    kClick,
    kType,
    kWait,
    kAssert,
    kScreenshot,
  };

  struct RecordingOptions {
    std::string output_path;
    std::string session_name;
    std::string description;
  };

  struct RecordedStep {
    ActionType type = ActionType::kUnknown;
    std::string target;
    std::string text;
    std::string condition;
    std::string click_type;
    std::string region;
    std::string format;
    int timeout_ms = 0;
    bool clear_first = false;
    bool success = false;
    int execution_time_ms = 0;
    std::string message;
    std::string test_id;
    std::vector<std::string> assertion_failures;
    std::string expected_value;
    std::string actual_value;
#if defined(YAZE_WITH_GRPC)
    HarnessTestStatus final_status = HarnessTestStatus::kUnspecified;
#endif
    std::string final_error_message;
    std::map<std::string, int32_t> metrics;
    absl::Time captured_at = absl::InfinitePast();
  };

  struct StopRecordingSummary {
    bool saved = false;
    std::string output_path;
    int step_count = 0;
    absl::Duration duration = absl::ZeroDuration();
  };

  class ScopedSuspension {
   public:
    ScopedSuspension(TestRecorder* recorder, bool active);
    ScopedSuspension(const ScopedSuspension&) = delete;
    ScopedSuspension& operator=(const ScopedSuspension&) = delete;
    ~ScopedSuspension();

   private:
    TestRecorder* recorder_;
    bool active_ = false;
  };

  explicit TestRecorder(TestManager* test_manager);

  absl::StatusOr<std::string> Start(const RecordingOptions& options);
  absl::StatusOr<StopRecordingSummary> Stop(const std::string& recording_id,
                                            bool discard);

  void RecordStep(const RecordedStep& step);

  bool IsRecording() const;
  std::string CurrentRecordingId() const;

  ScopedSuspension Suspend();

 private:
  absl::StatusOr<std::string> StartLocked(const RecordingOptions& options)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  absl::StatusOr<StopRecordingSummary> StopLocked(
      const std::string& recording_id, bool discard)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  void RecordStepLocked(const RecordedStep& step)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  absl::Status PopulateFinalStatusLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  static std::string GenerateRecordingId();
  static const char* ActionTypeToString(ActionType type);

  mutable absl::Mutex mu_;
  TestManager* const test_manager_;  // Not owned
  bool recording_ ABSL_GUARDED_BY(mu_) = false;
  bool suspended_ ABSL_GUARDED_BY(mu_) = false;
  std::string recording_id_ ABSL_GUARDED_BY(mu_);
  RecordingOptions options_ ABSL_GUARDED_BY(mu_);
  absl::Time started_at_ ABSL_GUARDED_BY(mu_) = absl::InfinitePast();
  std::vector<RecordedStep> steps_ ABSL_GUARDED_BY(mu_);
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_CORE_TESTING_TEST_RECORDER_H_
