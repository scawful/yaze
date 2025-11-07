# Git Workflow and Branching Strategy

**Last Updated:** October 10, 2025  
**Status:** Active  
**Current Phase:** Pre-1.0 (Relaxed Rules)

## Warning: Pre-1.0 Workflow (Current)

**TLDR for now:** Since yaze is pre-1.0 and actively evolving, we use a **simplified workflow**:

-  **Documentation changes**: Commit directly to `master` or `develop`
-  **Small bug fixes**: Can go direct to `develop`, no PR required
-  **Solo work**: Push directly when you're the only one working
- Warning: **Breaking changes**: Use feature branches and document in changelog
- Warning: **Major refactors**: Use feature branches for safety (can always revert)

**Why relaxed?**
- Small team / solo development
- Pre-1.0 means breaking changes are expected
- Documentation needs to be public quickly
- Overhead of PRs/reviews slows down experimentation

**When to transition to strict workflow:**
- Multiple active contributors
- Stable API (post-1.0)
- Large user base depending on stability
- Critical bugs need rapid hotfixes

---

## Pre-1.0 Release Strategy: Best Effort Releases

For all versions prior to 1.0.0, yaze follows a **"best effort"** release strategy. This prioritizes getting working builds to users quickly, even if not all platforms build successfully on the first try.

### Core Principles
1.  **Release Can Proceed with Failed Platforms**: The `release` CI/CD workflow will create a GitHub Release even if one or more platform-specific build jobs (e.g., Windows, Linux, macOS) fail.
2.  **Missing Platforms Can Be Added Later**: A failed job for a specific platform can be re-run from the GitHub Actions UI. If it succeeds, the binary artifact will be **automatically added to the existing GitHub Release**.
3.  **Transparency is Key**: The release notes will automatically generate a "Platform Availability" report, clearly indicating which platforms succeeded () and which failed (❌), so users know the current status.

### How It Works in Practice
- The `build-and-package` jobs in the `release.yml` workflow have `continue-on-error: true`.
- The final `create-github-release` job has `if: always()` and uses `softprops/action-gh-release@v2`, which intelligently updates an existing release if the tag already exists.
- If a platform build fails, a developer can investigate the issue and simply re-run the failed job. Upon success, the new binary is uploaded and attached to the release that was already created.

This strategy provides flexibility and avoids blocking a release for all users due to a transient issue on a single platform. Once the project reaches v1.0.0, this policy will be retired in favor of a stricter approach where all platforms must pass for a release to proceed.

---

## 📚 Full Workflow Reference (Future/Formal)

The sections below document the **formal Git Flow model** that yaze will adopt post-1.0 or when the team grows. For now, treat this as aspirational best practices.

## Branch Structure

### Main Branches

#### `master`
- **Purpose**: Production-ready release branch
- **Protection**: Protected, requires PR approval
- **Versioning**: Tagged with semantic versions (e.g., `v0.3.2`, `v0.4.0`)
- **Updates**: Only via approved PRs from `develop` or hotfix branches

#### `develop`
- **Purpose**: Main development branch, integration point for all features
- **Protection**: Protected, requires PR approval
- **State**: Should always build and pass tests
- **Updates**: Merges from feature branches, releases merge back after tagging

### Supporting Branches

#### Feature Branches
**Naming Convention:** `feature/<short-description>`

**Examples:**
- `feature/overworld-editor-improvements`
- `feature/dungeon-room-painter`
- `feature/add-sprite-animations`

**Rules:**
- Branch from: `develop`
- Merge back to: `develop`
- Lifetime: Delete after merge
- Naming: Use kebab-case, be descriptive but concise

**Workflow:**
```bash
# Create feature branch
git checkout develop
git pull origin develop
git checkout -b feature/my-feature

# Work on feature
git add .
git commit -m "feat: add new feature"

# Keep up to date with develop
git fetch origin
git rebase origin/develop

# Push and create PR
git push -u origin feature/my-feature
```

#### Bugfix Branches
**Naming Convention:** `bugfix/<issue-number>-<short-description>`

**Examples:**
- `bugfix/234-canvas-scroll-regression`
- `bugfix/fix-dungeon-crash`

**Rules:**
- Branch from: `develop`
- Merge back to: `develop`
- Lifetime: Delete after merge
- Reference issue number when applicable

#### Hotfix Branches
**Naming Convention:** `hotfix/<version>-<description>`

**Examples:**
- `hotfix/v0.3.3-memory-leak`
- `hotfix/v0.3.2-crash-on-startup`

**Rules:**
- Branch from: `master`
- Merge to: BOTH `master` AND `develop`
- Creates new patch version
- Used for critical production bugs only

**Workflow:**
```bash
# Create hotfix from master
git checkout master
git pull origin master
git checkout -b hotfix/v0.3.3-critical-fix

# Fix the issue
git add .
git commit -m "fix: critical production bug"

# Merge to master
git checkout master
git merge --no-ff hotfix/v0.3.3-critical-fix
git tag -a v0.3.3 -m "Hotfix: critical bug"
git push origin master --tags

# Merge to develop
git checkout develop
git merge --no-ff hotfix/v0.3.3-critical-fix
git push origin develop

# Delete hotfix branch
git branch -d hotfix/v0.3.3-critical-fix
```

#### Release Branches
**Naming Convention:** `release/<version>`

**Examples:**
- `release/v0.4.0`
- `release/v0.3.2`

**Rules:**
- Branch from: `develop`
- Merge to: `master` AND `develop`
- Used for release preparation (docs, version bumps, final testing)
- Only bugfixes allowed, no new features

**Workflow:**
```bash
# Create release branch
git checkout develop
git pull origin develop
git checkout -b release/v0.4.0

# Prepare release (update version, docs, changelog)
# ... make changes ...
git commit -m "chore: prepare v0.4.0 release"

# Merge to master and tag
git checkout master
git merge --no-ff release/v0.4.0
git tag -a v0.4.0 -m "Release v0.4.0"
git push origin master --tags

# Merge back to develop
git checkout develop
git merge --no-ff release/v0.4.0
git push origin develop

# Delete release branch
git branch -d release/v0.4.0
```

#### Experimental Branches
**Naming Convention:** `experiment/<description>`

**Examples:**
- `experiment/vulkan-renderer`
- `experiment/wasm-build`

**Rules:**
- Branch from: `develop` or `master`
- May never merge (prototypes, research)
- Document findings in docs/experiments/
- Delete when concluded or merge insights into features

## Commit Message Conventions

Follow **Conventional Commits** specification:

### Format
```
<type>(<scope>): <subject>

<body>

<footer>
```

### Types
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `style`: Code style (formatting, semicolons, etc.)
- `refactor`: Code refactoring
- `perf`: Performance improvements
- `test`: Adding or updating tests
- `build`: Build system changes (CMake, dependencies)
- `ci`: CI/CD configuration changes
- `chore`: Maintenance tasks

### Scopes (optional)
- `overworld`: Overworld editor
- `dungeon`: Dungeon editor
- `graphics`: Graphics editor
- `emulator`: Emulator core
- `canvas`: Canvas system
- `gui`: GUI/ImGui components

### Examples
```bash
# Good commit messages
feat(overworld): add tile16 quick-select palette
fix(canvas): resolve scroll regression after refactoring
docs: update build instructions for SDL3
refactor(emulator): extract APU timing to cycle-accurate model
perf(dungeon): optimize room rendering with batched draw calls

# With body and footer
feat(overworld): add multi-tile selection tool

Allows users to select and copy/paste rectangular regions
of tiles in the overworld editor. Supports undo/redo.

Closes #123
```

## Pull Request Guidelines

### PR Title
Follow commit message convention:
```
feat(overworld): add new feature
fix(dungeon): resolve crash
```

### PR Description Template
```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix (non-breaking change)
- [ ] New feature (non-breaking change)
- [ ] Breaking change (fix or feature that breaks existing functionality)
- [ ] Documentation update

## Testing
- [ ] All tests pass
- [ ] Added new tests for new features
- [ ] Manual testing completed

## Checklist
- [ ] Code follows style guidelines
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] No new warnings
- [ ] Dependent changes merged
```

### PR Review Process
1. **Author**: Create PR, fill out template, request reviewers
2. **CI**: Automatic build and test on all platforms
3. **Reviewers**: Code review, suggest changes
4. **Author**: Address feedback, push updates
5. **Approval**: At least 1 approval required for merge
6. **Merge**: Squash or merge commit (case-by-case)

## Version Numbering

Follow **Semantic Versioning (SemVer)**: `MAJOR.MINOR.PATCH`

### MAJOR (e.g., 1.0.0)
- Breaking API changes
- Major architectural changes
- First stable release

### MINOR (e.g., 0.4.0)
- New features (backward compatible)
- Significant improvements
- Major dependency updates (SDL3)

### PATCH (e.g., 0.3.2)
- Bug fixes
- Minor improvements
- Documentation updates

### Pre-release Tags
- `v0.4.0-alpha.1` - Early testing
- `v0.4.0-beta.1` - Feature complete
- `v0.4.0-rc.1` - Release candidate

## Release Process

### For Minor/Major Releases (0.x.0, x.0.0)

1. **Create release branch**
   ```bash
   git checkout -b release/v0.4.0
   ```

2. **Update version numbers**
   - `CMakeLists.txt`
   - `../reference/changelog.md`
   - `README.md`

3. **Update documentation**
   - Review all docs for accuracy
   - Update migration guides if breaking changes
   - Finalize changelog

4. **Create release commit**
   ```bash
   git commit -m "chore: prepare v0.4.0 release"
   ```

5. **Merge and tag**
   ```bash
   git checkout master
   git merge --no-ff release/v0.4.0
   git tag -a v0.4.0 -m "Release v0.4.0"
   git push origin master --tags
   ```

6. **Merge back to develop**
   ```bash
   git checkout develop
   git merge --no-ff release/v0.4.0
   git push origin develop
   ```

7. **Create GitHub Release**
   - Draft release notes
   - Attach build artifacts (CI generates these)
   - Publish release

### For Patch Releases (0.3.x)

1. **Collect fixes on develop**
   - Merge all bugfix PRs to develop
   - Ensure tests pass

2. **Create release branch**
   ```bash
   git checkout -b release/v0.3.2
   ```

3. **Follow steps 2-7 above**

## Long-Running Feature Branches

For large features (e.g., v0.4.0 modernization), use a **feature branch with sub-branches**:

```
develop
  └── feature/v0.4.0-modernization (long-running)
       ├── feature/v0.4.0-sdl3-core
       ├── feature/v0.4.0-sdl3-graphics
       └── feature/v0.4.0-sdl3-audio
```

**Rules:**
- Long-running branch stays alive during development
- Sub-branches merge to long-running branch
- Long-running branch periodically rebases on `develop`
- Final merge to `develop` when complete

## Tagging Strategy

### Release Tags
- Format: `v<MAJOR>.<MINOR>.<PATCH>[-prerelease]`
- Examples: `v0.3.2`, `v0.4.0-rc.1`, `v1.0.0`
- Annotated tags with release notes

### Internal Milestones
- Format: `milestone/<name>`
- Examples: `milestone/canvas-refactor-complete`
- Used for tracking major internal achievements

## Best Practices

### DO 
- Keep commits atomic and focused
- Write descriptive commit messages
- Rebase feature branches on develop regularly
- Run tests before pushing
- Update documentation with code changes
- Delete branches after merging

### DON'T ❌
- Commit directly to master or develop
- Force push to shared branches
- Mix unrelated changes in one commit
- Merge without PR review
- Leave stale branches

## Quick Reference

```bash
# Start new feature
git checkout develop
git pull
git checkout -b feature/my-feature

# Update feature branch with latest develop
git fetch origin
git rebase origin/develop

# Finish feature
git push -u origin feature/my-feature
# Create PR on GitHub → Merge → Delete branch

# Start hotfix
git checkout master
git pull
git checkout -b hotfix/v0.3.3-fix
# ... fix, commit, merge to master and develop ...

# Create release
git checkout develop
git pull
git checkout -b release/v0.4.0
# ... prepare, merge to master, tag, merge back to develop ...
```

## Emergency Procedures

### If master is broken
1. Create hotfix branch immediately
2. Fix critical issue
3. Fast-track PR review
4. Hotfix deploy ASAP

### If develop is broken
1. Identify breaking commit
2. Revert if needed
3. Fix in new branch
4. Merge fix with priority

### If release needs to be rolled back
1. Tag current state as `v0.x.y-broken`
2. Revert master to previous tag
3. Create hotfix branch
4. Fix and release as patch version

---

##  Current Simplified Workflow (Pre-1.0)

### Daily Development Pattern

```bash
# For documentation or small changes
git checkout master  # or develop, your choice
git pull
# ... make changes ...
git add docs/
git commit -m "docs: update workflow guide"
git push origin master

# For experimental features
git checkout -b feature/my-experiment
# ... experiment ...
git push -u origin feature/my-experiment
# If it works: merge to develop
# If it doesn't: delete branch, no harm done
```

### When to Use Branches (Pre-1.0)

**Use a branch for:**
- Large refactors that might break things
- Experimenting with new ideas
- Features that take multiple days
- SDL3 migration or other big changes

**Don't bother with branches for:**
- Documentation updates
- Small bug fixes
- Typo corrections
- README updates
- Adding comments or tests

### Current Branch Usage

For now, treat `master` and `develop` interchangeably:
- `master`: Latest stable-ish code + docs
- `develop`: Optional staging area for integration

When you want docs public, just push to `master`. The GitHub Pages / docs site will update automatically.

### Commit Message (Simplified)

Still try to follow the convention, but don't stress:

```bash
# Good enough
git commit -m "docs: reorganize documentation structure"
git commit -m "fix: dungeon editor crash on load"
git commit -m "feat: add overworld sprite editor"

# Also fine for now
git commit -m "update docs"
git commit -m "fix crash"
```

### Releases (Pre-1.0)

Just tag and push:

```bash
# When you're ready for v0.3.2
git tag -a v0.3.2 -m "Release v0.3.2"
git push origin v0.3.2
# GitHub Actions builds it automatically
```

No need for release branches or complex merging until you have multiple contributors.

---

**References:**
- [Git Flow](https://nvie.com/posts/a-successful-git-branching-model/)
- [Conventional Commits](https://www.conventionalcommits.org/)
- [Semantic Versioning](https://semver.org/)
