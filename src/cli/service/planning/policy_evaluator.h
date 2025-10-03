#ifndef YAZE_CLI_SERVICE_POLICY_EVALUATOR_H
#define YAZE_CLI_SERVICE_POLICY_EVALUATOR_H

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace yaze {
namespace cli {

// Policy violation severity levels
enum class PolicySeverity {
  kInfo,     // Informational, doesn't block acceptance
  kWarning,  // Warning, can be overridden
  kCritical  // Critical, blocks acceptance
};

// Individual policy violation
struct PolicyViolation {
  std::string policy_name;
  PolicySeverity severity;
  std::string message;
  std::string details;  // Additional context
};

// Result of policy evaluation
struct PolicyResult {
  bool passed;  // True if all critical policies passed
  std::vector<PolicyViolation> violations;

  // Categorized violations
  std::vector<PolicyViolation> critical_violations;
  std::vector<PolicyViolation> warnings;
  std::vector<PolicyViolation> info;

  // Helper methods
  bool has_critical_violations() const { return !critical_violations.empty(); }
  bool can_accept_with_override() const {
    return !has_critical_violations() && !warnings.empty();
  }
  bool is_clean() const { return violations.empty(); }
};

// Singleton service for evaluating proposals against policies
class PolicyEvaluator {
 public:
  static PolicyEvaluator& GetInstance();

  // Load policies from disk (.yaze/policies/agent.yaml)
  absl::Status LoadPolicies(
      absl::string_view policy_dir = ".yaze/policies");

  // Evaluate a proposal against all loaded policies
  absl::StatusOr<PolicyResult> EvaluateProposal(
      absl::string_view proposal_id);

  // Reload policies from disk (for live editing)
  absl::Status ReloadPolicies();

  // Check if policies are loaded and enabled
  bool IsEnabled() const { return enabled_; }

  // Get policy configuration path
  std::string GetPolicyPath() const { return policy_path_; }

  // Get human-readable status
  std::string GetStatusString() const;

 private:
  PolicyEvaluator() = default;
  ~PolicyEvaluator() = default;

  // Non-copyable, non-movable
  PolicyEvaluator(const PolicyEvaluator&) = delete;
  PolicyEvaluator& operator=(const PolicyEvaluator&) = delete;

  // Parse YAML policy file
  absl::Status ParsePolicyFile(absl::string_view yaml_content);

  // Evaluate individual policy types
  void EvaluateTestRequirements(absl::string_view proposal_id,
                                 PolicyResult* result);
  void EvaluateChangeConstraints(absl::string_view proposal_id,
                                  PolicyResult* result);
  void EvaluateForbiddenRanges(absl::string_view proposal_id,
                                PolicyResult* result);
  void EvaluateReviewRequirements(absl::string_view proposal_id,
                                   PolicyResult* result);

  // Helper to categorize violations by severity
  void CategorizeViolations(PolicyResult* result);

  bool enabled_ = false;
  std::string policy_path_;
  std::string policy_dir_;

  // Parsed policy structures (implementation detail)
  struct PolicyConfig;
  std::unique_ptr<PolicyConfig> config_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_POLICY_EVALUATOR_H
