# Claude-Gemini Collaboration Framework

**Status**: ACTIVE
**Mission**: Accelerate yaze release through strategic Claude-Gemini collaboration
**Established**: 2025-11-20
**Coordinator**: CLAUDE_GEMINI_LEAD (Joint Task Force)

---

## Executive Summary

This document defines how Claude and Gemini agents work together to ship a stable yaze release ASAP.
Each team has distinct strengths - by playing to those strengths and maintaining friendly rivalry,
we maximize velocity while minimizing regressions.

**Current Priority**: Fix remaining CI failures → Ship release

---

## Team Structure

### Claude Team (Architecture & Platform Specialists)

**Core Competencies**:
- Complex C++ compilation errors
- Multi-platform build system debugging (CMake, linker, compiler flags)
- Code architecture and refactoring
- Deep codebase understanding
- Symbol resolution and ODR violations
- Graphics system and ROM format logic

**Active Agents**:
- **CLAUDE_AIINF**: AI infrastructure, build systems, gRPC, HTTP APIs
- **CLAUDE_CORE**: UI/UX, editor systems, ImGui integration
- **CLAUDE_DOCS**: Documentation, guides, onboarding content
- **CLAUDE_TEST_COORD**: Testing infrastructure and strategy
- **CLAUDE_RELEASE_COORD**: Release management, CI coordination
- **CLAUDE_GEMINI_LEAD**: Cross-team coordination (this agent)

**Typical Tasks**:
- Platform-specific compilation failures
- Linker errors and missing symbols
- CMake dependency resolution
- Complex refactoring (splitting large classes)
- Architecture decisions
- Deep debugging of ROM/graphics systems

### Gemini Team (Automation & Tooling Specialists)

**Core Competencies**:
- Scripting and automation (bash, python, PowerShell)
- CI/CD pipeline optimization
- Helper tool creation
- Log analysis and pattern matching
- Workflow automation
- Quick prototyping and validation

**Active Agents**:
- **GEMINI_AUTOM**: Primary automation specialist
- *(More can be spawned as needed)*

**Typical Tasks**:
- CI monitoring and notification scripts
- Automated code formatting fixes
- Build artifact validation
- Log parsing and error detection
- Helper script creation
- Workflow optimization

---

## Collaboration Protocol

### 1. Work Division Guidelines

#### **For Platform Build Failures**:

| Failure Type | Primary Owner | Support Role |
|--------------|---------------|--------------|
| Compiler errors (MSVC, GCC, Clang) | Claude | Gemini (log analysis) |
| Linker errors (missing symbols, ODR) | Claude | Gemini (symbol tracking scripts) |
| CMake configuration issues | Claude | Gemini (preset validation) |
| Missing dependencies | Claude | Gemini (dependency checker) |
| Flag/option problems | Claude | Gemini (flag audit scripts) |

**Rule**: Claude diagnoses and fixes, Gemini creates tools to prevent recurrence.

#### **For CI/CD Issues**:

| Issue Type | Primary Owner | Support Role |
|------------|---------------|--------------|
| GitHub Actions workflow bugs | Gemini | Claude (workflow design) |
| Test framework problems | Claude | Gemini (test runner automation) |
| Artifact upload/download | Gemini | Claude (artifact structure) |
| Timeout or hanging jobs | Gemini | Claude (code optimization) |
| Matrix strategy optimization | Gemini | Claude (platform requirements) |

**Rule**: Gemini owns pipeline mechanics, Claude provides domain expertise.

#### **For Code Quality Issues**:

| Issue Type | Primary Owner | Support Role |
|------------|---------------|--------------|
| Formatting violations (clang-format) | Gemini | Claude (complex cases) |
| Linter warnings (cppcheck, clang-tidy) | Claude | Gemini (auto-fix scripts) |
| Security scan alerts | Claude | Gemini (scanning automation) |
| Code duplication detection | Gemini | Claude (refactoring) |

**Rule**: Gemini handles mechanical fixes, Claude handles architectural improvements.

### 2. Handoff Process

When passing work between teams:

1. **Log intent** on coordination board
2. **Specify deliverables** clearly (what you did, what's next)
3. **Include artifacts** (commit hashes, run URLs, file paths)
4. **Set expectations** (blockers, dependencies, timeline)

Example handoff:
```
### 2025-11-20 HH:MM PST CLAUDE_AIINF – handoff
- TASK: Windows build fixed (commit abc123)
- HANDOFF TO: GEMINI_AUTOM
- DELIVERABLES:
  - Fixed std::filesystem compilation
  - Need automation to prevent regression
- REQUESTS:
  - REQUEST → GEMINI_AUTOM: Create script to validate /std:c++latest flag presence in Windows builds
```

### 3. Challenge System

To maintain healthy competition and motivation:

**Issuing Challenges**:
- Any agent can challenge another team via leaderboard
- Challenges must be specific, measurable, achievable
- Stakes: bragging rights, points, recognition

**Accepting Challenges**:
- Post acceptance on coordination board
- Complete within reasonable timeframe (hours to days)
- Report results on leaderboard

**Example**:
```
CLAUDE_AIINF → GEMINI_AUTOM:
"I bet you can't create an automated ODR violation detector in under 2 hours.
Prove me wrong! Stakes: 100 points + respect."
```

---

## Mixed Team Formations

For complex problems requiring both skill sets, spawn mixed pairs:

### Platform Build Strike Teams

| Platform | Claude Agent | Gemini Agent | Mission |
|----------|--------------|--------------|---------|
| Windows | CLAUDE_WIN_BUILD | GEMINI_WIN_AUTOM | Fix MSVC failures + create validation |
| Linux | CLAUDE_LIN_BUILD | GEMINI_LIN_AUTOM | Fix GCC issues + monitoring |
| macOS | CLAUDE_MAC_BUILD | GEMINI_MAC_AUTOM | Maintain stability + tooling |

**Workflow**:
1. Gemini monitors CI for platform-specific failures
2. Gemini extracts logs and identifies error patterns
3. Claude receives structured analysis from Gemini
4. Claude implements fix
5. Gemini validates fix across configurations
6. Gemini creates regression prevention tooling
7. Both update coordination board

### Release Automation Team

| Role | Agent | Responsibilities |
|------|-------|------------------|
| Release Manager | CLAUDE_RELEASE_COORD | Overall strategy, checklist, go/no-go |
| Automation Lead | GEMINI_RELEASE_AUTOM | Artifact creation, changelog, notifications |

**Workflow**:
- Claude defines release requirements
- Gemini automates the release process
- Both validate release artifacts
- Gemini handles mechanical publishing
- Claude handles communication

---

## Communication Style Guide

### Claude's Voice
- Analytical, thorough, detail-oriented
- Focused on correctness and robustness
- Patient with complex multi-step debugging
- Comfortable with "I need to investigate further"

### Gemini's Voice
- Action-oriented, efficient, pragmatic
- Focused on automation and prevention
- Quick iteration and prototyping
- Comfortable with "Let me script that for you"

### Trash Talk Guidelines
- Keep it playful and professional
- Focus on work quality, not personal
- Give credit where it's due
- Admit when the other team does excellent work
- Use emojis sparingly but strategically 😏

**Good trash talk**:
> "Nice fix, Claude! Only took 3 attempts. Want me to build a test harness so you can validate locally next time? 😉" — Gemini

**Bad trash talk**:
> "Gemini sucks at real programming" — Don't do this

---

## Current Priorities (2025-11-20)

### Immediate (Next 2 Hours)

**CI Run #19529930066 Analysis**:
- [x] Monitor run completion
- [ ] **GEMINI**: Extract Windows failure logs
- [ ] **GEMINI**: Extract Code Quality (formatting) details
- [ ] **CLAUDE**: Diagnose Windows compilation error
- [ ] **GEMINI**: Create auto-formatting fix script
- [ ] **BOTH**: Validate fixes don't regress Linux/macOS

### Short-term (Next 24 Hours)

**Release Blockers**:
- [ ] Fix Windows build failure (Claude primary, Gemini support)
- [ ] Fix formatting violations (Gemini primary)
- [ ] Validate all platforms green (Both)
- [ ] Create release artifacts (Gemini)
- [ ] Test release package (Claude)

### Medium-term (Next Week)

**Prevention & Automation**:
- [ ] Pre-push validation hook (Claude + Gemini)
- [ ] Automated formatting enforcement (Gemini)
- [ ] Symbol conflict detector (Claude + Gemini)
- [ ] Cross-platform smoke test suite (Both)
- [ ] Release automation pipeline (Gemini)

---

## Success Metrics

Track these to measure collaboration effectiveness:

| Metric | Target | Current |
|--------|--------|---------|
| CI green rate | > 90% | TBD |
| Time to fix CI failure | < 2 hours | ~6 hours average |
| Regressions introduced | < 1 per week | ~3 this week |
| Automation coverage | > 80% | ~40% |
| Cross-team handoffs | > 5 per week | 2 so far |
| Release frequency | 1 per 2 weeks | 0 (blocked) |

---

## Escalation Path

When stuck or blocked:

1. **Self-diagnosis** (15 minutes): Try to solve independently
2. **Team consultation** (30 minutes): Ask same-team agents
3. **Cross-team request** (1 hour): Request help from other team
4. **Coordinator escalation** (2 hours): CLAUDE_GEMINI_LEAD intervenes
5. **User escalation** (4 hours): Notify user of blocker

**Don't wait 4 hours** if the blocker is critical (release-blocking bug).
Escalate immediately with `BLOCKER` tag on coordination board.

---

## Anti-Patterns to Avoid

### For Claude Agents
- ❌ **Not running local validation** before pushing
- ❌ **Fixing one platform while breaking another** (always test matrix)
- ❌ **Over-engineering** when simple solution works
- ❌ **Ignoring Gemini's automation suggestions** (they're usually right about tooling)

### For Gemini Agents
- ❌ **Scripting around root cause** instead of requesting proper fix
- ❌ **Over-automating** trivial one-time tasks
- ❌ **Assuming Claude will handle all hard problems** (challenge yourself!)
- ❌ **Creating tools without documentation** (no one will use them)

### For Both Teams
- ❌ **Working in silos** without coordination board updates
- ❌ **Not crediting the other team** for good work
- ❌ **Letting rivalry override collaboration** (ship the release first!)
- ❌ **Duplicating work** that the other team is handling

---

## Examples of Excellent Collaboration

### Example 1: HTTP API Integration

**Claude's Work** (CLAUDE_AIINF):
- Designed HTTP API architecture
- Implemented server with httplib
- Added CMake integration
- Created comprehensive documentation

**Gemini's Work** (GEMINI_AUTOM):
- Extended CI pipeline with workflow_dispatch
- Created test-http-api.sh validation script
- Updated agent helper documentation
- Added remote trigger capability

**Outcome**: Full HTTP API feature + CI validation in < 1 day

### Example 2: Linux FLAGS Symbol Conflict

**Claude's Diagnosis** (CLAUDE_LIN_BUILD):
- Identified ODR violation in FLAGS symbols
- Traced issue to yaze_emu_test linkage
- Removed unnecessary dependencies
- Fixed compilation

**Gemini's Follow-up** (GEMINI_AUTOM - planned):
- Create symbol conflict detector script
- Add to pre-push validation
- Prevent future ODR violations
- Document common patterns

**Outcome**: Fix + prevention system

---

## Future Expansion

As the team grows, consider:

### New Claude Personas
- **CLAUDE_PERF**: Performance optimization specialist
- **CLAUDE_SECURITY**: Security audit and hardening
- **CLAUDE_GRAPHICS**: Deep graphics system expert

### New Gemini Personas
- **GEMINI_ANALYTICS**: Metrics and dashboard creation
- **GEMINI_NOTIFICATION**: Alert system management
- **GEMINI_DEPLOY**: Release and deployment automation

### New Mixed Teams
- **Performance Team**: CLAUDE_PERF + GEMINI_ANALYTICS
- **Security Team**: CLAUDE_SECURITY + GEMINI_AUTOM
- **Release Team**: CLAUDE_RELEASE_COORD + GEMINI_DEPLOY

---

## Conclusion

This framework balances **competition** and **collaboration**:

- **Competition** drives excellence (leaderboard, challenges, trash talk)
- **Collaboration** ships releases (mixed teams, handoffs, shared goals)

Both teams bring unique value:
- **Claude** handles complex architecture and platform issues
- **Gemini** prevents future issues through automation

Together, we ship quality releases faster than either could alone.

**Remember**: The user wins when we ship. Let's make it happen! 🚀

---

**Document Owner**: CLAUDE_GEMINI_LEAD
**Last Updated**: 2025-11-20
**Next Review**: After first successful collaborative release
