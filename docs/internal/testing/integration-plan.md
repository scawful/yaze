# Testing Infrastructure Integration Plan

**Owner**: CLAUDE_TEST_COORD
**Status**: Draft
**Created**: 2025-11-20
**Target Completion**: 2025-12-15

## Executive Summary

This document outlines the rollout plan for comprehensive testing infrastructure improvements across the yaze project. The goal is to reduce CI failures, catch issues earlier, and provide developers with fast, reliable testing tools.

## Current State Assessment

### What's Working Well

✅ **Test Organization**:
- Clear directory structure (unit/integration/e2e/benchmarks)
- Good test coverage for core systems
- ImGui Test Engine integration for GUI testing

✅ **CI/CD**:
- Multi-platform matrix (Linux, macOS, Windows)
- Automated test execution on every commit
- Test result artifacts on failure

✅ **Helper Scripts**:
- `run-tests.sh` for preset-based testing
- `smoke-build.sh` for quick build verification
- `run-gh-workflow.sh` for remote CI triggers

### Current Gaps

❌ **Developer Experience**:
- No pre-push validation hooks
- Long CI feedback loop (10-15 minutes)
- Unclear what tests to run locally
- Format checking often forgotten

❌ **Test Infrastructure**:
- No symbol conflict detection tools
- No CMake configuration validators
- Platform-specific test failures hard to reproduce locally
- Flaky test tracking is manual

❌ **Documentation**:
- Testing docs scattered across multiple files
- No clear "before you push" checklist
- Platform-specific troubleshooting incomplete
- Release testing process not documented

## Goals and Success Criteria

### Primary Goals

1. **Fast Local Feedback** (<5 minutes for pre-push checks)
2. **Early Issue Detection** (catch 90% of CI failures locally)
3. **Clear Documentation** (developers know exactly what to run)
4. **Automated Validation** (pre-push hooks, format checking)
5. **Platform Parity** (reproducible CI failures locally)

### Success Metrics

- **CI Failure Rate**: Reduce from ~20% to <5%
- **Time to Fix**: Average time from failure to fix <30 minutes
- **Developer Satisfaction**: Positive feedback on testing workflow
- **Test Runtime**: Unit tests complete in <10s, full suite in <5min
- **Coverage**: Maintain >80% test coverage for critical paths

## Rollout Phases

### Phase 1: Documentation and Tools (Week 1-2) ✅ COMPLETE

**Status**: COMPLETE
**Completion Date**: 2025-11-20

#### Deliverables

- ✅ Master testing documentation (`docs/internal/testing/README.md`)
- ✅ Developer quick-start guide (`docs/public/developer/testing-quick-start.md`)
- ✅ Integration plan (this document)
- ✅ Updated release checklist with testing requirements

#### Validation

- ✅ All documents reviewed and approved
- ✅ Links between documents verified
- ✅ Content accuracy checked against actual implementation

### Phase 2: Pre-Push Validation (Week 3)

**Status**: PLANNED
**Target Date**: 2025-11-27

#### Deliverables

1. **Pre-Push Script** (`scripts/pre-push.sh`)
   - Run unit tests automatically
   - Check code formatting
   - Verify build compiles
   - Exit with error if any check fails
   - Run in <2 minutes

2. **Git Hook Integration** (`.git/hooks/pre-push`)
   - Optional installation script
   - Easy enable/disable mechanism
   - Clear output showing progress
   - Skip with `--no-verify` flag

3. **Developer Documentation**
   - How to install pre-push hook
   - How to customize checks
   - How to skip when needed

#### Implementation Steps

```bash
# 1. Create pre-push script
scripts/pre-push.sh

# 2. Create hook installer
scripts/install-git-hooks.sh

# 3. Update documentation
docs/public/developer/git-workflow.md
docs/public/developer/testing-quick-start.md

# 4. Test on all platforms
- macOS: Verify script runs correctly
- Linux: Verify script runs correctly
- Windows: Create PowerShell equivalent
```

#### Validation

- [ ] Script runs in <2 minutes on all platforms
- [ ] All checks are meaningful (catch real issues)
- [ ] False positive rate <5%
- [ ] Developers report positive feedback

### Phase 3: Symbol Conflict Detection (Week 4)

**Status**: PLANNED
**Target Date**: 2025-12-04

#### Background

Recent Linux build failures were caused by symbol conflicts (FLAGS_rom, FLAGS_norom redefinition). We need automated detection to prevent this.

#### Deliverables

1. **Symbol Conflict Checker** (`scripts/check-symbols.sh`)
   - Parse CMake target link graphs
   - Detect duplicate symbol definitions
   - Report conflicts with file locations
   - Run in <30 seconds

2. **CI Integration**
   - Add symbol check job to `.github/workflows/ci.yml`
   - Run on every PR
   - Fail build if conflicts detected

3. **Documentation**
   - Troubleshooting guide for symbol conflicts
   - Best practices for avoiding conflicts

#### Implementation Steps

```bash
# 1. Create symbol checker
scripts/check-symbols.sh
# - Use nm/objdump to list symbols
# - Compare across linked targets
# - Detect duplicates

# 2. Add to CI
.github/workflows/ci.yml
# - New job: symbol-check
# - Runs after build

# 3. Document usage
docs/internal/testing/symbol-conflict-detection.md
```

#### Validation

- [ ] Detects known symbol conflicts (FLAGS_rom case)
- [ ] Zero false positives on current codebase
- [ ] Runs in <30 seconds
- [ ] Clear, actionable error messages

### Phase 4: CMake Configuration Validation (Week 5)

**Status**: PLANNED
**Target Date**: 2025-12-11

#### Deliverables

1. **CMake Preset Validator** (`scripts/validate-cmake-presets.sh`)
   - Verify all presets configure successfully
   - Check for missing variables
   - Validate preset inheritance
   - Test preset combinations

2. **Build Matrix Tester** (`scripts/test-build-matrix.sh`)
   - Test common preset/platform combinations
   - Verify all targets build
   - Check for missing dependencies

3. **Documentation**
   - CMake troubleshooting guide
   - Preset creation guidelines

#### Implementation Steps

```bash
# 1. Create validators
scripts/validate-cmake-presets.sh
scripts/test-build-matrix.sh

# 2. Add to CI (optional job)
.github/workflows/cmake-validation.yml

# 3. Document
docs/internal/testing/cmake-validation.md
```

#### Validation

- [ ] All current presets validate successfully
- [ ] Catches common configuration errors
- [ ] Runs in <5 minutes for full matrix
- [ ] Provides clear error messages

### Phase 5: Platform Matrix Testing (Week 6)

**Status**: PLANNED
**Target Date**: 2025-12-18

#### Deliverables

1. **Local Platform Testing** (`scripts/test-all-platforms.sh`)
   - Run tests on all configured platforms
   - Parallel execution for speed
   - Aggregate results
   - Report differences across platforms

2. **CI Enhancement**
   - Add platform-specific test suites
   - Better artifact collection
   - Test result comparison across platforms

3. **Documentation**
   - Platform-specific testing guide
   - Troubleshooting platform differences

#### Implementation Steps

```bash
# 1. Create platform tester
scripts/test-all-platforms.sh

# 2. Enhance CI
.github/workflows/ci.yml
# - Better artifact collection
# - Result comparison

# 3. Document
docs/internal/testing/platform-testing.md
```

#### Validation

- [ ] Detects platform-specific failures
- [ ] Clear reporting of differences
- [ ] Runs in <10 minutes (parallel)
- [ ] Useful for debugging platform issues

## Training and Communication

### Developer Training

**Target Audience**: All contributors

**Format**: Written documentation + optional video walkthrough

**Topics**:
1. How to run tests locally (5 minutes)
2. Understanding test categories (5 minutes)
3. Using pre-push hooks (5 minutes)
4. Debugging test failures (10 minutes)
5. CI workflow overview (5 minutes)

**Materials**:
- ✅ Quick start guide (already created)
- ✅ Testing guide (already exists)
- [ ] Video walkthrough (optional, Phase 6)

### Communication Plan

**Announcements**:
1. **Phase 1 Complete**: Email/Slack announcement with links to new docs
2. **Phase 2 Ready**: Announce pre-push hooks, encourage adoption
3. **Phase 3-5**: Update as each phase completes
4. **Final Rollout**: Comprehensive announcement when all phases done

**Channels**:
- GitHub Discussions
- Project README updates
- CONTRIBUTING.md updates
- Coordination board updates

## Risk Mitigation

### Risk 1: Developer Resistance to Pre-Push Hooks

**Mitigation**:
- Make hooks optional (install script)
- Keep checks fast (<2 minutes)
- Allow easy skip with `--no-verify`
- Provide clear value proposition

### Risk 2: False Positives Causing Frustration

**Mitigation**:
- Test extensively before rollout
- Monitor false positive rate
- Provide clear bypass mechanisms
- Iterate based on feedback

### Risk 3: Tools Break on Platform Updates

**Mitigation**:
- Test on all platforms before rollout
- Document platform-specific requirements
- Version-pin critical dependencies
- Maintain fallback paths

### Risk 4: CI Becomes Too Slow

**Mitigation**:
- Use parallel execution
- Cache aggressively
- Make expensive checks optional
- Profile and optimize bottlenecks

## Rollback Plan

If any phase causes significant issues:

1. **Immediate**: Disable problematic feature (remove hook, comment out CI job)
2. **Investigate**: Gather feedback and logs
3. **Fix**: Address root cause
4. **Re-enable**: Gradual rollout with fixes
5. **Document**: Update docs with lessons learned

## Success Indicators

### Week-by-Week Targets

- **Week 2**: Documentation complete and published ✅
- **Week 3**: Pre-push hooks adopted by 50% of active developers
- **Week 4**: Symbol conflicts detected before reaching CI
- **Week 5**: CMake preset validation catches configuration errors
- **Week 6**: Platform-specific failures reproducible locally

### Final Success Criteria (End of Phase 5)

- ✅ All documentation complete and reviewed
- [ ] CI failure rate <5% (down from ~20%)
- [ ] Average time to fix CI failure <30 minutes
- [ ] 80%+ developers using pre-push hooks
- [ ] Zero symbol conflict issues reaching production
- [ ] Platform parity: local tests match CI results

## Maintenance and Long-Term Support

### Ongoing Responsibilities

**Testing Infrastructure Lead** (CLAUDE_TEST_COORD):
- Monitor CI failure rates
- Respond to testing infrastructure issues
- Update documentation as needed
- Coordinate with platform specialists

**Platform Specialists**:
- Maintain platform-specific test helpers
- Troubleshoot platform-specific failures
- Keep documentation current

**All Developers**:
- Report testing infrastructure issues
- Suggest improvements
- Keep tests passing locally before pushing

### Quarterly Reviews

**Schedule**: Every 3 months

**Review**:
1. CI failure rate trends
2. Test runtime trends
3. Developer feedback
4. New platform/tool needs
5. Documentation updates

**Adjustments**:
- Update scripts for new platforms
- Optimize slow tests
- Add new helpers as needed
- Archive obsolete tools/docs

## Budget and Resources

### Time Investment

**Initial Rollout** (Phases 1-5): ~6 weeks
- Documentation: 1 week ✅
- Pre-push validation: 1 week
- Symbol detection: 1 week
- CMake validation: 1 week
- Platform testing: 1 week
- Buffer/testing: 1 week

**Ongoing Maintenance**: ~4 hours/month
- Monitoring CI
- Updating docs
- Fixing issues
- Quarterly reviews

### Infrastructure Costs

**Current**: $0 (using GitHub Actions free tier)

**Projected**: $0 (within free tier limits)

**Potential Future Costs**:
- GitHub Actions minutes (if exceed free tier)
- External CI service (if needed)
- Test infrastructure hosting (if needed)

## Appendix: Related Work

### Completed by Other Agents

**GEMINI_AUTOM**:
- ✅ Remote workflow trigger support
- ✅ HTTP API testing infrastructure
- ✅ Helper scripts for agents

**CLAUDE_AIINF**:
- ✅ Platform-specific build fixes
- ✅ CMake preset expansion
- ✅ gRPC integration improvements

**CODEX**:
- ✅ Documentation audit and consolidation
- ✅ Build verification scripts
- ✅ Coordination board setup

### Planned by Other Agents

**CLAUDE_TEST_ARCH**:
- Pre-push testing automation
- Gap analysis of test coverage

**CLAUDE_CMAKE_VALIDATOR**:
- CMake configuration validation tools
- Preset verification

**CLAUDE_SYMBOL_CHECK**:
- Symbol conflict detection
- Link graph analysis

**CLAUDE_MATRIX_TEST**:
- Platform matrix testing
- Cross-platform validation

## Questions and Clarifications

**Q: Are pre-push hooks mandatory?**
A: No, they're optional but strongly recommended. Developers can install with `scripts/install-git-hooks.sh` and remove anytime.

**Q: How long will pre-push checks take?**
A: Target is <2 minutes. Unit tests (<10s) + format check (<5s) + build verification (~1min).

**Q: What if I need to push despite failing checks?**
A: Use `git push --no-verify` to bypass hooks. This should be rare and only for emergencies.

**Q: Will this slow down CI?**
A: No. Most tools run locally to catch issues before CI. Some new CI jobs are optional/parallel.

**Q: What if tools break on my platform?**
A: Report in GitHub issues with platform details. We'll fix or provide platform-specific workaround.

## References

- [Testing Documentation](README.md)
- [Quick Start Guide](../../public/developer/testing-quick-start.md)
- [Coordination Board](../agents/coordination-board.md)
- [Release Checklist](../release-checklist.md)
- [CI Workflow](../../../.github/workflows/ci.yml)

---

**Next Actions**: Proceed to Phase 2 (Pre-Push Validation) once Phase 1 is approved and published.
