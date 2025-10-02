# Policy Evaluation Framework (AW-04)

**Status**: Implementation In Progress  
**Priority**: High (Next Phase)  
**Time Estimate**: 6-8 hours  
**Last Updated**: October 2, 2025

## Overview

The Policy Evaluation Framework provides a YAML-based constraint system for gating proposal acceptance in the z3ed agent workflow. It ensures that AI-generated ROM modifications meet quality, safety, and testing requirements before being merged into the main ROM.

## Goals

1. **Quality Gates**: Enforce minimum test pass rates and code quality standards
2. **Safety Constraints**: Prevent modifications to critical ROM regions (headers, checksums)
3. **Scope Limits**: Restrict changes to reasonable byte counts and specific banks
4. **Human Review**: Require manual review for large or complex changes
5. **Flexibility**: Allow policy overrides with confirmation and logging

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│ ProposalDrawer (GUI)                                    │
│  └─ Accept button gated by PolicyEvaluator             │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│ PolicyEvaluator (Singleton Service)                     │
│  ├─ LoadPolicies() from .yaze/policies/                │
│  ├─ EvaluateProposal(proposal_id) → PolicyResult       │
│  └─ Cache of parsed YAML policies                      │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│ .yaze/policies/agent.yaml (YAML Configuration)          │
│  ├─ test_requirements (min pass rates)                 │
│  ├─ change_constraints (byte limits, allowed banks)    │
│  ├─ review_requirements (human review triggers)        │
│  └─ forbidden_ranges (protected ROM regions)           │
└─────────────────────────────────────────────────────────┘
```

## YAML Policy Schema

### Example Policy File

```yaml
# .yaze/policies/agent.yaml
version: 1.0
enabled: true

policies:
  # Policy 1: Test Requirements
  - name: require_tests
    type: test_requirement
    enabled: true
    severity: critical  # critical | warning | info
    rules:
      - test_suite: "overworld_rendering"
        min_pass_rate: 0.95
      - test_suite: "palette_integrity"
        min_pass_rate: 1.0
      - test_suite: "dungeon_logic"
        min_pass_rate: 0.90
    message: "All required test suites must pass before accepting proposal"

  # Policy 2: Change Scope Limits
  - name: limit_change_scope
    type: change_constraint
    enabled: true
    severity: critical
    rules:
      - max_bytes_changed: 10240  # 10KB limit
      - allowed_banks: [0x00, 0x01, 0x0E, 0x0F]  # Graphics banks only
      - max_commands_executed: 20
    message: "Proposal exceeds allowed change scope"

  # Policy 3: Protected ROM Regions
  - name: protect_critical_regions
    type: forbidden_range
    enabled: true
    severity: critical
    ranges:
      - start: 0xFFB0  # ROM header
        end: 0xFFFF
        reason: "ROM header is protected"
      - start: 0x00FFC0  # Internal header
        end: 0x00FFDF
        reason: "Internal ROM header"
    message: "Proposal modifies protected ROM region"

  # Policy 4: Human Review Requirements
  - name: human_review_required
    type: review_requirement
    enabled: true
    severity: warning
    conditions:
      - if: bytes_changed > 1024
        then: require_diff_review
        message: "Large change requires diff review"
      - if: commands_executed > 10
        then: require_log_review
        message: "Complex operation requires log review"
      - if: test_failures > 0
        then: require_explanation
        message: "Test failures require explanation"

  # Policy 5: Palette Modifications
  - name: palette_safety
    type: change_constraint
    enabled: true
    severity: warning
    rules:
      - max_palettes_changed: 5
      - preserve_transparency: true  # Don't modify color index 0
    message: "Palette changes exceed safety threshold"
```

### Schema Definition

```yaml
# Policy file structure
version: string  # Semantic version (e.g., "1.0")
enabled: boolean  # Master enable/disable

policies:
  - name: string  # Unique policy identifier
    type: enum  # test_requirement | change_constraint | forbidden_range | review_requirement
    enabled: boolean  # Policy-specific enable/disable
    severity: enum  # critical | warning | info
    
    # Type-specific fields:
    rules: array  # For test_requirement, change_constraint
    ranges: array  # For forbidden_range
    conditions: array  # For review_requirement
    
    message: string  # User-facing error message
```

## Implementation Plan

### Phase 1: Core Infrastructure (2 hours)

#### 1.1 Create PolicyEvaluator Service

**File**: `src/cli/service/policy_evaluator.h`

```cpp
#ifndef YAZE_CLI_SERVICE_POLICY_EVALUATOR_H
#define YAZE_CLI_SERVICE_POLICY_EVALUATOR_H

#include <string>
#include <vector>
#include <memory>
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace yaze {
namespace cli {

// Policy violation severity levels
enum class PolicySeverity {
  kInfo,      // Informational, doesn't block acceptance
  kWarning,   // Warning, can be overridden
  kCritical   // Critical, blocks acceptance
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
};

// Singleton service for evaluating proposals against policies
class PolicyEvaluator {
 public:
  static PolicyEvaluator& GetInstance();
  
  // Load policies from disk (.yaze/policies/agent.yaml)
  absl::Status LoadPolicies(absl::string_view policy_dir = ".yaze/policies");
  
  // Evaluate a proposal against all loaded policies
  absl::StatusOr<PolicyResult> EvaluateProposal(
      absl::string_view proposal_id);
  
  // Reload policies from disk (for live editing)
  absl::Status ReloadPolicies();
  
  // Check if policies are loaded and enabled
  bool IsEnabled() const { return enabled_; }
  
  // Get policy configuration path
  std::string GetPolicyPath() const { return policy_path_; }

 private:
  PolicyEvaluator() = default;
  ~PolicyEvaluator() = default;
  
  // Non-copyable, non-movable
  PolicyEvaluator(const PolicyEvaluator&) = delete;
  PolicyEvaluator& operator=(const PolicyEvaluator&) = delete;
  
  // Parse YAML policy file
  absl::Status ParsePolicyFile(absl::string_view yaml_content);
  
  // Evaluate individual policy types
  void EvaluateTestRequirements(
      absl::string_view proposal_id, PolicyResult* result);
  void EvaluateChangeConstraints(
      absl::string_view proposal_id, PolicyResult* result);
  void EvaluateForbiddenRanges(
      absl::string_view proposal_id, PolicyResult* result);
  void EvaluateReviewRequirements(
      absl::string_view proposal_id, PolicyResult* result);
  
  bool enabled_ = false;
  std::string policy_path_;
  
  // Parsed policy structures (implementation detail)
  struct PolicyConfig;
  std::unique_ptr<PolicyConfig> config_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_POLICY_EVALUATOR_H
```

#### 1.2 Create Policy Configuration Structures

**File**: `src/cli/service/policy_evaluator.cc` (partial)

```cpp
#include "src/cli/service/policy_evaluator.h"

#include <fstream>
#include <sstream>
#include "absl/strings/str_format.h"
#include "src/cli/service/proposal_registry.h"

// If YAML parsing is available
#ifdef YAZE_WITH_YAML
#include <yaml-cpp/yaml.h>
#endif

namespace yaze {
namespace cli {

// Internal policy configuration structures
struct PolicyEvaluator::PolicyConfig {
  std::string version;
  bool enabled;
  
  struct TestRequirement {
    std::string name;
    bool enabled;
    PolicySeverity severity;
    std::vector<std::pair<std::string, double>> test_suites;  // suite name → min pass rate
    std::string message;
  };
  
  struct ChangeConstraint {
    std::string name;
    bool enabled;
    PolicySeverity severity;
    int max_bytes_changed = -1;
    std::vector<int> allowed_banks;
    int max_commands_executed = -1;
    int max_palettes_changed = -1;
    bool preserve_transparency = false;
    std::string message;
  };
  
  struct ForbiddenRange {
    std::string name;
    bool enabled;
    PolicySeverity severity;
    std::vector<std::tuple<int, int, std::string>> ranges;  // start, end, reason
    std::string message;
  };
  
  struct ReviewRequirement {
    std::string name;
    bool enabled;
    PolicySeverity severity;
    std::vector<std::string> conditions;
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

absl::Status PolicyEvaluator::ParsePolicyFile(absl::string_view yaml_content) {
#ifndef YAZE_WITH_YAML
  return absl::UnimplementedError(
      "YAML support not compiled. Build with YAZE_WITH_YAML=ON");
#else
  try {
    YAML::Node root = YAML::Load(std::string(yaml_content));
    
    config_ = std::make_unique<PolicyConfig>();
    config_->version = root["version"].as<std::string>("1.0");
    config_->enabled = root["enabled"].as<bool>(true);
    
    if (!config_->enabled) {
      enabled_ = false;
      return absl::OkStatus();
    }
    
    // Parse policies array
    if (root["policies"]) {
      for (const auto& policy_node : root["policies"]) {
        std::string type = policy_node["type"].as<std::string>();
        
        if (type == "test_requirement") {
          // Parse test requirement policy
          // ... (implementation continues)
        } else if (type == "change_constraint") {
          // Parse change constraint policy
          // ... (implementation continues)
        } else if (type == "forbidden_range") {
          // Parse forbidden range policy
          // ... (implementation continues)
        } else if (type == "review_requirement") {
          // Parse review requirement policy
          // ... (implementation continues)
        }
      }
    }
    
    enabled_ = true;
    return absl::OkStatus();
    
  } catch (const YAML::Exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse policy YAML: %s", e.what()));
  }
#endif
}

// ... (implementation continues with evaluation methods)

}  // namespace cli
}  // namespace yaze
```

### Phase 2: Policy Evaluation Logic (2-3 hours)

Implement the core evaluation methods that check proposals against each policy type.

### Phase 3: GUI Integration (2 hours)

#### 3.1 Update ProposalDrawer

**File**: `src/app/editor/system/proposal_drawer.cc`

Add policy status display and gating logic:

```cpp
#include "src/cli/service/policy_evaluator.h"

void ProposalDrawer::DrawProposalDetail(const std::string& proposal_id) {
  // ... existing detail view code ...
  
  // === Policy Status Section ===
  ImGui::Separator();
  ImGui::TextUnformatted("Policy Status:");
  
  auto& policy_eval = cli::PolicyEvaluator::GetInstance();
  if (policy_eval.IsEnabled()) {
    auto policy_result = policy_eval.EvaluateProposal(proposal_id);
    
    if (policy_result.ok()) {
      const auto& result = policy_result.value();
      
      if (result.passed) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ All policies passed");
      } else {
        // Show violations
        if (result.has_critical_violations()) {
          ImGui::TextColored(ImVec4(1, 0, 0, 1), "⛔ Critical violations:");
          for (const auto& violation : result.critical_violations) {
            ImGui::BulletText("%s: %s", 
                violation.policy_name.c_str(), 
                violation.message.c_str());
          }
        }
        
        if (!result.warnings.empty()) {
          ImGui::TextColored(ImVec4(1, 1, 0, 1), "⚠️ Warnings:");
          for (const auto& violation : result.warnings) {
            ImGui::BulletText("%s: %s", 
                violation.policy_name.c_str(), 
                violation.message.c_str());
          }
        }
      }
      
      // Gate Accept button
      ImGui::Separator();
      bool can_accept = !result.has_critical_violations();
      
      if (!can_accept) {
        ImGui::BeginDisabled();
      }
      
      if (ImGui::Button("Accept Proposal")) {
        if (result.can_accept_with_override() && !override_confirmed_) {
          // Show override confirmation dialog
          ImGui::OpenPopup("Override Policy");
        } else {
          AcceptProposal(proposal_id);
        }
      }
      
      if (!can_accept) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0, 0, 1), 
            "(Accept blocked by policy violations)");
      }
      
      // Override confirmation dialog
      if (ImGui::BeginPopupModal("Override Policy", nullptr, 
          ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("This proposal has policy warnings.");
        ImGui::Text("Do you want to override and accept anyway?");
        ImGui::Text("This action will be logged.");
        ImGui::Separator();
        
        if (ImGui::Button("Override and Accept")) {
          override_confirmed_ = true;
          AcceptProposal(proposal_id);
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    } else {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), 
          "Policy evaluation failed: %s", 
          policy_result.status().message().data());
    }
  } else {
    ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), 
        "No policies configured");
  }
}
```

### Phase 4: Testing & Documentation (1-2 hours)

#### 4.1 Example Policy File

Create `.yaze/policies/agent.yaml.example`:

```yaml
# Example agent policy configuration
# Copy to .yaze/policies/agent.yaml and customize

version: 1.0
enabled: true

policies:
  # Require test suites to pass
  - name: require_tests
    type: test_requirement
    enabled: false  # Disabled by default (no tests yet)
    severity: critical
    rules:
      - test_suite: "smoke_test"
        min_pass_rate: 1.0
    message: "All smoke tests must pass"

  # Limit change scope
  - name: limit_changes
    type: change_constraint
    enabled: true
    severity: warning
    rules:
      - max_bytes_changed: 5120  # 5KB
      - max_commands_executed: 15
    message: "Keep changes small and focused"

  # Protect ROM header
  - name: protect_header
    type: forbidden_range
    enabled: true
    severity: critical
    ranges:
      - start: 0xFFB0
        end: 0xFFFF
        reason: "ROM header"
    message: "Cannot modify ROM header"
```

#### 4.2 Unit Tests

Create `test/cli/policy_evaluator_test.cc`:

```cpp
#include "src/cli/service/policy_evaluator.h"
#include "gtest/gtest.h"

namespace yaze {
namespace cli {
namespace {

TEST(PolicyEvaluatorTest, LoadPoliciesSuccess) {
  auto& eval = PolicyEvaluator::GetInstance();
  auto status = eval.LoadPolicies("test/fixtures/policies");
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(eval.IsEnabled());
}

TEST(PolicyEvaluatorTest, EvaluateProposal_NoViolations) {
  // ... test implementation
}

TEST(PolicyEvaluatorTest, EvaluateProposal_CriticalViolation) {
  // ... test implementation
}

}  // namespace
}  // namespace cli
}  // namespace yaze
```

## Deliverables

- [x] Policy evaluator service interface
- [ ] YAML policy parser implementation
- [ ] Policy evaluation logic for all 4 types
- [ ] ProposalDrawer GUI integration
- [ ] Policy override workflow
- [ ] Example policy configurations
- [ ] Unit tests
- [ ] Documentation and usage guide

## Success Criteria

1. **Functional**:
   - Policies load from YAML files
   - Proposals evaluated against all enabled policies
   - Accept button gated by critical violations
   - Override workflow for warnings

2. **User Experience**:
   - Clear policy status display in ProposalDrawer
   - Helpful violation messages
   - Override confirmation dialog
   - Policy evaluation fast (< 100ms)

3. **Quality**:
   - Unit test coverage > 80%
   - No crashes or memory leaks
   - Graceful handling of malformed YAML
   - Works with policies disabled

## Future Enhancements

- Policy templates for common scenarios
- Policy violation history/analytics
- Auto-fix suggestions for violations
- Integration with CI/CD for automated policy checks
- Policy versioning and migration

---

**Status**: Ready for implementation  
**Next Step**: Create PolicyEvaluator skeleton and wire into build system  
**Estimated Completion**: October 3-4, 2025
