#include "cli/service/planning/policy_evaluator.h"

#include <fstream>
#include <sstream>

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cli/service/planning/proposal_registry.h"

namespace yaze {
namespace cli {

// Internal policy configuration structures
struct PolicyEvaluator::PolicyConfig {
  std::string version;
  bool enabled = true;

  struct TestRequirement {
    std::string name;
    bool enabled = true;
    PolicySeverity severity = PolicySeverity::kCritical;
    // suite name → min pass rate
    std::vector<std::pair<std::string, double>> test_suites;
    std::string message;
  };

  struct ChangeConstraint {
    std::string name;
    bool enabled = true;
    PolicySeverity severity = PolicySeverity::kWarning;
    int max_bytes_changed = -1;
    std::vector<int> allowed_banks;
    int max_commands_executed = -1;
    int max_palettes_changed = -1;
    bool preserve_transparency = false;
    std::string message;
  };

  struct ForbiddenRange {
    std::string name;
    bool enabled = true;
    PolicySeverity severity = PolicySeverity::kCritical;
    // start, end, reason
    std::vector<std::tuple<int, int, std::string>> ranges;
    std::string message;
  };

  struct ReviewRequirement {
    std::string name;
    bool enabled = true;
    PolicySeverity severity = PolicySeverity::kWarning;
    struct Condition {
      std::string if_clause;      // e.g., "bytes_changed > 1024"
      std::string then_clause;    // e.g., "require_diff_review"
      std::string message;
    };
    std::vector<Condition> conditions;
    std::string message;
  };

  std::vector<TestRequirement> test_requirements;
  std::vector<ChangeConstraint> change_constraints;
  std::vector<ForbiddenRange> forbidden_ranges;
  std::vector<ReviewRequirement> review_requirements;
};

// Singleton instance
PolicyEvaluator& PolicyEvaluator::GetInstance() {
  static PolicyEvaluator instance;
  return instance;
}

absl::Status PolicyEvaluator::LoadPolicies(absl::string_view policy_dir) {
  policy_dir_ = std::string(policy_dir);
  policy_path_ = absl::StrFormat("%s/agent.yaml", policy_dir);

  // Check if file exists
  std::ifstream file(policy_path_);
  if (!file.good()) {
    // No policy file - policies disabled
    enabled_ = false;
    return absl::OkStatus();
  }

  // Read file content
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string yaml_content = buffer.str();

  return ParsePolicyFile(yaml_content);
}

absl::Status PolicyEvaluator::ReloadPolicies() {
  if (policy_dir_.empty()) {
    return absl::FailedPreconditionError(
        "No policy directory set. Call LoadPolicies first.");
  }
  return LoadPolicies(policy_dir_);
}

std::string PolicyEvaluator::GetStatusString() const {
  if (!enabled_) {
    return "Policies disabled (no configuration file)";
  }
  if (!config_) {
    return "Policies enabled but not loaded";
  }

  int total_policies = config_->test_requirements.size() +
                       config_->change_constraints.size() +
                       config_->forbidden_ranges.size() +
                       config_->review_requirements.size();

  return absl::StrFormat("Policies enabled (%d policies loaded from %s)",
                         total_policies, policy_path_);
}

absl::Status PolicyEvaluator::ParsePolicyFile(absl::string_view yaml_content) {
  // For now, implement a simple key-value parser
  // In production, we'd use yaml-cpp or similar library
  // This stub implementation allows the system to work without YAML dependency

  config_ = std::make_unique<PolicyConfig>();
  config_->version = "1.0";
  config_->enabled = true;

  // Parse simple YAML-like format
  std::vector<std::string> lines = absl::StrSplit(yaml_content, '\n');
  bool in_policies = false;
  std::string current_policy_type;
  std::string current_policy_name;

  for (const auto& line : lines) {
    std::string trimmed = std::string(absl::StripAsciiWhitespace(line));

    // Skip comments and empty lines
    if (trimmed.empty() || trimmed[0] == '#') continue;

    // Check for main keys
    if (absl::StartsWith(trimmed, "version:")) {
      std::vector<std::string> parts = absl::StrSplit(trimmed, ':');
      if (parts.size() >= 2) {
        config_->version = std::string(absl::StripAsciiWhitespace(parts[1]));
      }
    } else if (absl::StartsWith(trimmed, "enabled:")) {
      std::vector<std::string> parts = absl::StrSplit(trimmed, ':');
      if (parts.size() >= 2) {
        std::string value = std::string(absl::StripAsciiWhitespace(parts[1]));
        config_->enabled = (value == "true");
      }
    } else if (trimmed == "policies:") {
      in_policies = true;
    } else if (in_policies && absl::StartsWith(trimmed, "- name:")) {
      // Start of new policy
      std::vector<std::string> parts = absl::StrSplit(trimmed, ':');
      if (parts.size() >= 2) {
        current_policy_name = std::string(absl::StripAsciiWhitespace(parts[1]));
      }
    } else if (in_policies && absl::StartsWith(trimmed, "type:")) {
      std::vector<std::string> parts = absl::StrSplit(trimmed, ':');
      if (parts.size() >= 2) {
        current_policy_type = std::string(absl::StripAsciiWhitespace(parts[1]));

        // Create appropriate policy structure
        if (current_policy_type == "change_constraint") {
          PolicyConfig::ChangeConstraint constraint;
          constraint.name = current_policy_name;
          constraint.max_bytes_changed = 5120;  // Default 5KB
          constraint.max_commands_executed = 15;
          constraint.message = "Change scope exceeded";
          config_->change_constraints.push_back(constraint);
        } else if (current_policy_type == "forbidden_range") {
          PolicyConfig::ForbiddenRange range;
          range.name = current_policy_name;
          range.ranges.push_back(
              std::make_tuple(0xFFB0, 0xFFFF, "ROM header"));
          range.message = "Cannot modify protected region";
          config_->forbidden_ranges.push_back(range);
        } else if (current_policy_type == "test_requirement") {
          PolicyConfig::TestRequirement test;
          test.name = current_policy_name;
          test.test_suites.push_back(std::make_pair("smoke_test", 1.0));
          test.message = "Required tests must pass";
          config_->test_requirements.push_back(test);
        } else if (current_policy_type == "review_requirement") {
          PolicyConfig::ReviewRequirement review;
          review.name = current_policy_name;
          review.message = "Manual review required";
          config_->review_requirements.push_back(review);
        }
      }
    }
  }

  if (!config_->enabled) {
    enabled_ = false;
    return absl::OkStatus();
  }

  enabled_ = true;
  return absl::OkStatus();
}

absl::StatusOr<PolicyResult> PolicyEvaluator::EvaluateProposal(
    absl::string_view proposal_id) {
  PolicyResult result;
  result.passed = true;

  if (!enabled_ || !config_) {
    // No policies - everything passes
    return result;
  }

  // Evaluate each policy type
  EvaluateTestRequirements(std::string(proposal_id), &result);
  EvaluateChangeConstraints(std::string(proposal_id), &result);
  EvaluateForbiddenRanges(std::string(proposal_id), &result);
  EvaluateReviewRequirements(std::string(proposal_id), &result);

  // Categorize violations by severity
  CategorizeViolations(&result);

  // Determine overall pass/fail
  result.passed = !result.has_critical_violations();

  return result;
}

void PolicyEvaluator::EvaluateTestRequirements(absl::string_view proposal_id,
                                                PolicyResult* result) {
  // TODO: Implement test requirement evaluation
  // For now, all test requirements pass (no test framework yet)
  std::string proposal_id_str(proposal_id);
  for (const auto& policy : config_->test_requirements) {
    if (!policy.enabled) continue;

    // Placeholder: would check actual test results here
    // For now, we skip test validation
  }
}

void PolicyEvaluator::EvaluateChangeConstraints(absl::string_view proposal_id,
                                                 PolicyResult* result) {
  auto& registry = ProposalRegistry::Instance();
  auto proposal_result = registry.GetProposal(std::string(proposal_id));

  if (!proposal_result.ok()) {
    return;  // Can't evaluate non-existent proposal
  }
  
  const auto& proposal = proposal_result.value();

  for (const auto& policy : config_->change_constraints) {
    if (!policy.enabled) continue;

    // Check max bytes changed
    if (policy.max_bytes_changed > 0 &&
        proposal.bytes_changed > policy.max_bytes_changed) {
      PolicyViolation violation;
      violation.policy_name = policy.name;
      violation.severity = policy.severity;
      violation.message = absl::StrFormat(
          "%s: %d bytes changed (limit: %d)", policy.message,
          proposal.bytes_changed, policy.max_bytes_changed);
      violation.details = absl::StrFormat("Proposal changed %d bytes",
                                          proposal.bytes_changed);
      result->violations.push_back(violation);
    }

    // Check max commands executed
    if (policy.max_commands_executed > 0 &&
        proposal.commands_executed > policy.max_commands_executed) {
      PolicyViolation violation;
      violation.policy_name = policy.name;
      violation.severity = policy.severity;
      violation.message = absl::StrFormat(
          "%s: %d commands executed (limit: %d)", policy.message,
          proposal.commands_executed, policy.max_commands_executed);
      violation.details = absl::StrFormat("Proposal executed %d commands",
                                          proposal.commands_executed);
      result->violations.push_back(violation);
    }
  }
}

void PolicyEvaluator::EvaluateForbiddenRanges(absl::string_view proposal_id,
                                               PolicyResult* result) {
  // TODO: Implement forbidden range checking
  // Would need to parse diff or track ROM modifications
  // For now, we assume no forbidden range violations
  for (const auto& policy : config_->forbidden_ranges) {
    if (!policy.enabled) continue;

    // Placeholder: would check ROM modification ranges here
  }
}

void PolicyEvaluator::EvaluateReviewRequirements(absl::string_view proposal_id,
                                                  PolicyResult* result) {
  auto& registry = ProposalRegistry::Instance();
  auto proposal_result = registry.GetProposal(std::string(proposal_id));

  if (!proposal_result.ok()) {
    return;
  }
  
  const auto& proposal = proposal_result.value();

  for (const auto& policy : config_->review_requirements) {
    if (!policy.enabled) continue;

    // Evaluate conditions
    for (const auto& condition : policy.conditions) {
      bool condition_met = false;

      // Simple condition evaluation
      if (absl::StrContains(condition.if_clause, "bytes_changed")) {
        // Extract threshold from condition like "bytes_changed > 1024"
        if (absl::StrContains(condition.if_clause, ">")) {
          std::vector<std::string> parts =
              absl::StrSplit(condition.if_clause, '>');
          if (parts.size() == 2) {
            int threshold;
            if (absl::SimpleAtoi(absl::StripAsciiWhitespace(parts[1]),
                                 &threshold)) {
              condition_met = (proposal.bytes_changed > threshold);
            }
          }
        }
      } else if (absl::StrContains(condition.if_clause, "commands_executed")) {
        if (absl::StrContains(condition.if_clause, ">")) {
          std::vector<std::string> parts =
              absl::StrSplit(condition.if_clause, '>');
          if (parts.size() == 2) {
            int threshold;
            if (absl::SimpleAtoi(absl::StripAsciiWhitespace(parts[1]),
                                 &threshold)) {
              condition_met = (proposal.commands_executed > threshold);
            }
          }
        }
      }

      if (condition_met) {
        PolicyViolation violation;
        violation.policy_name = policy.name;
        violation.severity = policy.severity;
        violation.message =
            condition.message.empty() ? policy.message : condition.message;
        violation.details = absl::StrFormat(
            "Condition met: %s → %s", condition.if_clause, condition.then_clause);
        result->violations.push_back(violation);
      }
    }
  }
}

void PolicyEvaluator::CategorizeViolations(PolicyResult* result) {
  for (const auto& violation : result->violations) {
    switch (violation.severity) {
      case PolicySeverity::kCritical:
        result->critical_violations.push_back(violation);
        break;
      case PolicySeverity::kWarning:
        result->warnings.push_back(violation);
        break;
      case PolicySeverity::kInfo:
        result->info.push_back(violation);
        break;
    }
  }
}

}  // namespace cli
}  // namespace yaze
