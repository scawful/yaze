# YAZE Testing Documentation Index

**Quick Navigation Guide for All Testing Resources**

This index helps you find the right testing documentation for your needs.

---

## For Developers: Getting Started

**I want to run tests right now:**
→ **[Quick Start Guide](QUICK_START_GUIDE.md)**
- How to run tests (all categories)
- Command examples
- Debugging tips

**I need build and test commands:**
→ **[Build Quick Reference](../../public/build/quick-reference.md)**
- CMake presets
- Build commands
- Platform-specific instructions

**I'm writing my first test:**
→ **[Quick Start Guide - Writing Tests](QUICK_START_GUIDE.md#writing-your-first-test)**
- Unit test example
- Integration test example
- E2E test example

---

## For Project Stakeholders

**I need a high-level overview:**
→ **[Executive Summary](EXECUTIVE_SUMMARY.md)**
- Current state and gaps
- Proposed improvements
- Timeline and ROI
- Resource requirements

**I want to understand the strategic plan:**
→ **[Test Infrastructure Improvement Plan](TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md)**
- Comprehensive technical plan
- 12-week implementation roadmap
- Detailed deliverables with code examples

---

## For Test Engineers

**I'm implementing the improvement plan:**
→ **[Test Infrastructure Improvement Plan](TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md)**
- ImGui Test Engine expansion
- Feature validation framework
- CI/CD enhancements
- Complete code examples

**I need testing infrastructure details:**
→ **[Testing Master Documentation](README.md)**
- Test organization matrix
- CI/CD testing strategy
- Platform-specific considerations
- Troubleshooting guide

**I'm working on test helpers:**
→ Implementation headers:
- `test/e2e/test_helpers.h` - E2E utilities
- `test/test_utils/mock_rom_generator.h` - Mock ROM creation
- `test/test_utils/rom_integrity_validator.h` - ROM validation

---

## By Topic

### Running Tests
1. **[Quick Start Guide](QUICK_START_GUIDE.md)** - Basic commands
2. **[Testing Master Docs](README.md)** - Advanced workflows
3. **[Build Quick Reference](../../public/build/quick-reference.md)** - Preset usage

### Writing Tests
1. **[Quick Start Guide - Writing Tests](QUICK_START_GUIDE.md#writing-your-first-test)**
2. **[Testing Master Docs - Guidelines](README.md#test-writing-guidelines)**
3. **[Improvement Plan - Appendix](TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md#7-appendix-code-examples)**

### Test Organization
1. **[Testing Master Docs - Organization Matrix](README.md#test-organization-matrix)**
2. **[Improvement Plan - Test Organization](TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md#3-test-organization-improvements)**
3. CMakeLists.txt - `test/CMakeLists.txt`

### CI/CD Integration
1. **[Testing Master Docs - CI/CD Strategy](README.md#cicd-testing-strategy)**
2. **[Improvement Plan - CI/CD Enhancements](TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md#4-cicd-integration-enhancements)**
3. **[CI Improvements Proposal](ci-improvements-proposal.md)**

### Platform-Specific Testing
1. **[Testing Master Docs - Platform Considerations](README.md#platform-specific-test-considerations)**
2. **[Matrix Testing Strategy](matrix-testing-strategy.md)**
3. **[Improvement Plan - Cross-Platform Matrix](TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md#25-cross-platform-test-matrix-p1)**

### Troubleshooting
1. **[Quick Start Guide - Common Issues](QUICK_START_GUIDE.md#common-issues)**
2. **[Testing Master Docs - Troubleshooting](README.md#troubleshooting-test-failures)**
3. **[Improvement Plan - Flaky Test Detection](TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md#35-flaky-test-detection-system-p2)**

---

## By User Role

### New Contributor
**Path:** QUICK_START_GUIDE.md → Testing Master Docs → Build Quick Reference

**Essential reading:**
1. Quick Start Guide for basic test execution
2. Testing Master Docs for understanding categories
3. Write first test using examples

### Experienced Developer
**Path:** Testing Master Docs → Improvement Plan (for advanced features)

**Essential reading:**
1. Testing Master Docs for comprehensive workflows
2. CMakeLists.txt for test suite organization
3. Improvement Plan for upcoming features

### Test Infrastructure Engineer
**Path:** Executive Summary → Full Improvement Plan → Implementation Headers

**Essential reading:**
1. Executive Summary for context
2. Complete Improvement Plan for specifications
3. Implementation headers for reference code
4. Testing Master Docs for current infrastructure

### Project Manager / Lead
**Path:** Executive Summary → Testing Master Docs → Release Checklist

**Essential reading:**
1. Executive Summary for strategic overview
2. Testing Master Docs for current capabilities
3. Release Checklist for validation requirements

### AI Agent
**Path:** Testing Master Docs → Quick Start Guide → Improvement Plan Examples

**Essential reading:**
1. Testing Master Docs for structure and protocols
2. Quick Start Guide for test patterns
3. Improvement Plan Appendix for code examples
4. Coordination Board for ongoing work

---

## Document Status

| Document | Status | Last Updated | Audience |
|----------|--------|--------------|----------|
| QUICK_START_GUIDE.md | ✅ Complete | 2025-11-21 | All developers |
| EXECUTIVE_SUMMARY.md | ✅ Complete | 2025-11-21 | Stakeholders |
| TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md | ✅ Complete | 2025-11-21 | Test engineers |
| README.md (Master Docs) | ✅ Current | 2025-11-20 | All |
| test/e2e/test_helpers.h | 📝 Stub | 2025-11-21 | Developers |
| test/test_utils/mock_rom_generator.h | 📝 Stub | 2025-11-21 | Developers |
| test/test_utils/rom_integrity_validator.h | 📝 Stub | 2025-11-21 | Developers |

Legend:
- ✅ Complete and current
- 📝 Stub/header only (needs implementation)
- 🔄 In progress
- ⏳ Planned

---

## Related Documentation

### Internal Docs
- **[Architecture Handoff](ARCHITECTURE_HANDOFF.md)** - Testing architecture decisions
- **[Integration Plan](integration-plan.md)** - Rolling out improvements
- **[Pre-Push Checklist](pre-push-checklist.md)** - Developer validation workflow
- **[Symbol Conflict Detection](symbol-conflict-detection.md)** - Avoiding build conflicts

### Public Docs
- **[Testing Guide](../../public/developer/testing-guide.md)** - User-facing guide
- **[Testing Quick Start](../../public/developer/testing-quick-start.md)** - Developer quick ref
- **[Build Quick Reference](../../public/build/quick-reference.md)** - Build commands

### Project Docs
- **[CLAUDE.md](../../../CLAUDE.md)** - Project guidelines
- **[Release Checklist](../release-checklist.md)** - Pre-release validation
- **[Coordination Board](../agents/coordination-board.md)** - Agent coordination

---

## File Locations

### Documentation
```
docs/internal/testing/
├── TESTING_INDEX.md                    # This file
├── QUICK_START_GUIDE.md                # NEW: Developer quick start
├── EXECUTIVE_SUMMARY.md                # NEW: Strategic overview
├── TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md  # NEW: Complete plan
├── README.md                           # Master documentation
├── testing-strategy.md                 # Overall strategy
├── integration-plan.md                 # Implementation roadmap
├── matrix-testing-strategy.md          # Platform matrix
└── ci-improvements-proposal.md         # CI enhancements
```

### Test Code
```
test/
├── e2e/
│   ├── test_helpers.h                  # NEW: E2E utilities (stub)
│   └── ...
├── test_utils/
│   ├── test_utils.h                    # Existing utilities
│   ├── mock_rom_generator.h            # NEW: Mock ROM (stub)
│   └── rom_integrity_validator.h       # NEW: Validation (stub)
├── unit/                               # Unit tests
├── integration/                        # Integration tests
└── benchmarks/                         # Performance tests
```

---

## Quick Decision Tree

**What do you need?**

```
┌─ Run tests now?
│  └─→ QUICK_START_GUIDE.md
│
┌─ Understand current testing?
│  └─→ README.md (Master Docs)
│
┌─ Write a new test?
│  └─→ QUICK_START_GUIDE.md → Examples section
│
┌─ Understand strategic plan?
│  └─→ EXECUTIVE_SUMMARY.md
│
┌─ Implement improvements?
│  └─→ TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md
│
┌─ Fix failing CI test?
│  └─→ README.md → Troubleshooting
│
└─ Something else?
   └─→ Use topic index above
```

---

## Improvement Plan Implementation Phases

**From:** [TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md](TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md#6-implementation-roadmap)

### Phase 1: Foundation (Weeks 1-3)
- E2E test helpers
- Mock ROM generator
- Pre-commit hooks
- Parallel test execution

### Phase 2: Core Coverage (Weeks 4-7)
- Dungeon editor e2e tests
- Overworld editor e2e tests

### Phase 3: Validation (Weeks 8-10)
- Performance regression tests
- Graphics editor tests
- Documentation completion

### Phase 4: Advanced (Weeks 11-12)
- Flaky test detection
- Visual regression
- Test dashboard

**Status:** Plan approved, awaiting implementation start

---

## Getting Help

**For test execution issues:**
1. Check QUICK_START_GUIDE.md Common Issues
2. Review README.md Troubleshooting
3. Search existing GitHub issues

**For test writing questions:**
1. Review code examples in QUICK_START_GUIDE.md
2. Check existing tests in `test/` for patterns
3. Consult TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md Appendix

**For strategic/planning questions:**
1. Review EXECUTIVE_SUMMARY.md
2. Check Coordination Board for active work
3. Tag appropriate agent (see README.md Contact section)

---

**Maintained by:** Test Infrastructure Team
**Last Updated:** 2025-11-21
**Questions?** See [Coordination Board](../agents/coordination-board.md)
