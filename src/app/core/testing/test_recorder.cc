#include "app/core/testing/test_recorder.h"

#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/core/testing/test_script_parser.h"
#include "app/test/test_manager.h"

namespace yaze {
namespace test {
namespace {

constexpr absl::Duration kTestCompletionTimeout = absl::Seconds(10);
constexpr absl::Duration kPollInterval = absl::Milliseconds(50);

const char* HarnessStatusToString(HarnessTestStatus status) {
  switch (status) {
    case HarnessTestStatus::kQueued:
      return "queued";
    case HarnessTestStatus::kRunning:
      return "running";
    case HarnessTestStatus::kPassed:
      return "passed";
    case HarnessTestStatus::kFailed:
      return "failed";
    case HarnessTestStatus::kTimeout:
      return "timeout";
    case HarnessTestStatus::kUnspecified:
    default:
      return "unknown";
  }
}

}  // namespace

TestRecorder::ScopedSuspension::ScopedSuspension(TestRecorder* recorder,
                         bool active)
  : recorder_(recorder), active_(active) {}

TestRecorder::ScopedSuspension::~ScopedSuspension() {
  if (!recorder_ || !active_) {
    return;
  }
  absl::MutexLock lock(&recorder_->mu_);
  recorder_->suspended_ = false;
}

TestRecorder::TestRecorder(TestManager* test_manager)
    : test_manager_(test_manager) {}

absl::StatusOr<std::string> TestRecorder::Start(
    const RecordingOptions& options) {
  absl::MutexLock lock(&mu_);
  return StartLocked(options);
}

absl::StatusOr<TestRecorder::StopRecordingSummary> TestRecorder::Stop(
    const std::string& recording_id, bool discard) {
  absl::MutexLock lock(&mu_);
  return StopLocked(recording_id, discard);
}

void TestRecorder::RecordStep(const RecordedStep& step) {
  absl::MutexLock lock(&mu_);
  RecordStepLocked(step);
}

bool TestRecorder::IsRecording() const {
  absl::MutexLock lock(&mu_);
  return recording_ && !suspended_;
}

std::string TestRecorder::CurrentRecordingId() const {
  absl::MutexLock lock(&mu_);
  return recording_id_;
}

TestRecorder::ScopedSuspension TestRecorder::Suspend() {
  absl::MutexLock lock(&mu_);
  bool activate = false;
  if (!suspended_) {
    suspended_ = true;
    activate = true;
  }
  return ScopedSuspension(this, activate);
}

absl::StatusOr<std::string> TestRecorder::StartLocked(
    const RecordingOptions& options) {
  if (recording_) {
    return absl::FailedPreconditionError(
        "A recording session is already active");
  }
  if (!test_manager_) {
    return absl::FailedPreconditionError("TestManager unavailable");
  }
  if (options.output_path.empty()) {
    return absl::InvalidArgumentError(
        "Recording requires a non-empty output path");
  }

  recording_ = true;
  suspended_ = false;
  options_ = options;
  if (options_.session_name.empty()) {
    options_.session_name = "Untitled Recording";
  }
  started_at_ = absl::Now();
  steps_.clear();
  recording_id_ = GenerateRecordingId();
  return recording_id_;
}

absl::StatusOr<TestRecorder::StopRecordingSummary> TestRecorder::StopLocked(
    const std::string& recording_id, bool discard) {
  if (!recording_) {
    return absl::FailedPreconditionError("No active recording session");
  }
  if (!recording_id.empty() && recording_id != recording_id_) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Recording ID mismatch (expected %s)", recording_id_));
  }

  StopRecordingSummary summary;
  summary.step_count = static_cast<int>(steps_.size());
  summary.duration = absl::Now() - started_at_;
  summary.output_path = options_.output_path;
  summary.saved = !discard;

  if (!discard) {
    RETURN_IF_ERROR(PopulateFinalStatusLocked());
    TestScript script;
    script.recording_id = recording_id_;
    script.name = options_.session_name;
    script.description = options_.description;
    script.created_at = started_at_;
    script.duration = summary.duration;

    for (const auto& step : steps_) {
      TestScriptStep script_step;
      script_step.action = ActionTypeToString(step.type);
      script_step.target = step.target;
      script_step.click_type = absl::AsciiStrToLower(step.click_type);
      script_step.text = step.text;
      script_step.clear_first = step.clear_first;
      script_step.condition = step.condition;
      script_step.timeout_ms = step.timeout_ms;
      script_step.region = step.region;
      script_step.format = step.format;
      script_step.expect_success = step.success;
      script_step.expect_status = HarnessStatusToString(step.final_status);
      if (!step.final_error_message.empty()) {
        script_step.expect_message = step.final_error_message;
      } else {
        script_step.expect_message = step.message;
      }
      script_step.expect_assertion_failures = step.assertion_failures;
      script_step.expect_metrics = step.metrics;
      script.steps.push_back(std::move(script_step));
    }

    RETURN_IF_ERROR(
        TestScriptParser::WriteToFile(script, options_.output_path));
  }

  // Reset state
  recording_ = false;
  suspended_ = false;
  recording_id_.clear();
  options_ = RecordingOptions{};
  started_at_ = absl::InfinitePast();
  steps_.clear();

  return summary;
}

void TestRecorder::RecordStepLocked(const RecordedStep& step) {
  if (!recording_ || suspended_) {
    return;
  }
  RecordedStep copy = step;
  if (copy.captured_at == absl::InfinitePast()) {
    copy.captured_at = absl::Now();
  }
  steps_.push_back(std::move(copy));
}

absl::Status TestRecorder::PopulateFinalStatusLocked() {
  if (!test_manager_) {
    return absl::FailedPreconditionError("TestManager unavailable");
  }

  for (auto& step : steps_) {
    if (step.test_id.empty()) {
      continue;
    }

    const absl::Time deadline = absl::Now() + kTestCompletionTimeout;
    while (absl::Now() < deadline) {
      absl::StatusOr<HarnessTestExecution> execution =
          test_manager_->GetHarnessTestExecution(step.test_id);
      if (!execution.ok()) {
        absl::SleepFor(kPollInterval);
        continue;
      }

      step.final_status = execution->status;
      step.final_error_message = execution->error_message;
      step.assertion_failures = execution->assertion_failures;
      step.metrics = execution->metrics;

      if (execution->status == HarnessTestStatus::kQueued ||
          execution->status == HarnessTestStatus::kRunning) {
        absl::SleepFor(kPollInterval);
        continue;
      }
      break;
    }
  }

  return absl::OkStatus();
}

std::string TestRecorder::GenerateRecordingId() {
  return absl::StrFormat(
      "rec_%s", absl::FormatTime("%Y%m%dT%H%M%S", absl::Now(),
                                     absl::UTCTimeZone()));
}

const char* TestRecorder::ActionTypeToString(ActionType type) {
  switch (type) {
    case ActionType::kClick:
      return "click";
    case ActionType::kType:
      return "type";
    case ActionType::kWait:
      return "wait";
    case ActionType::kAssert:
      return "assert";
    case ActionType::kScreenshot:
      return "screenshot";
    case ActionType::kUnknown:
    default:
      return "unknown";
  }
}

}  // namespace test
}  // namespace yaze
