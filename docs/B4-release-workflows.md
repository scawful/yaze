# Release Workflows Documentation

YAZE uses three different GitHub Actions workflows for creating releases, each designed for specific use cases and reliability levels. This document explains the differences, use cases, and when to use each workflow.

## Overview

| Workflow | Complexity | Reliability | Use Case |
|----------|------------|-------------|----------|
| **release-simplified.yml** | Low | Basic | Quick releases, testing |
| **release.yml** | Medium | High | Standard releases |
| **release-complex.yml** | High | Maximum | Production releases, fallbacks |

---

## 1. Release-Simplified (`release-simplified.yml`)

### Purpose
A streamlined workflow for quick releases and testing scenarios.

### Key Features
- **Minimal Configuration**: Basic build setup with standard dependencies
- **No Fallback Mechanisms**: Direct dependency installation without error handling
- **Standard vcpkg**: Uses fixed vcpkg commit without fallback options
- **Basic Testing**: Simple executable verification

### Use Cases
- **Development Testing**: Testing release process during development
- **Beta Releases**: Quick beta or alpha releases
- **Hotfixes**: Emergency releases that need to be deployed quickly
- **CI/CD Validation**: Ensuring the basic release process works

### Configuration
```yaml
# Standard vcpkg setup
vcpkgGitCommitId: 'c8696863d371ab7f46e213d8f5ca923c4aef2a00'
# No fallback mechanisms
# Basic dependency installation
```

### Platforms Supported
- Windows (x64, x86, ARM64)
- macOS Universal
- Linux x64

---

## 2. Release (`release.yml`)

### Purpose
The standard production release workflow with enhanced reliability.

### Key Features
- **Enhanced vcpkg**: Updated baseline and improved dependency management
- **Better Error Handling**: More robust error reporting and debugging
- **Comprehensive Testing**: Extended executable validation and artifact verification
- **Production Ready**: Designed for stable releases

### Use Cases
- **Stable Releases**: Official stable version releases
- **Feature Releases**: Major feature releases with full testing
- **Release Candidates**: Pre-release candidates for testing

### Configuration
```yaml
# Updated vcpkg baseline
builtin-baseline: "2024.12.12"
# Enhanced error handling
# Comprehensive testing
```

### Advantages over Simplified
- More reliable dependency resolution
- Better error reporting
- Enhanced artifact validation
- Production-grade stability

---

## 3. Release-Complex (`release-complex.yml`)

### Purpose
Maximum reliability release workflow with comprehensive fallback mechanisms.

### Key Features
- **Advanced Fallback System**: Multiple dependency installation strategies
- **vcpkg Failure Handling**: Automatic fallback to manual dependency installation
- **Chocolatey Integration**: Windows package manager fallback
- **Comprehensive Debugging**: Extensive logging and error analysis
- **Multiple Build Strategies**: CMake configuration fallbacks
- **Enhanced Validation**: Multi-stage build verification

### Use Cases
- **Production Releases**: Critical production releases requiring maximum reliability
- **Enterprise Deployments**: Releases for enterprise customers
- **Major Version Releases**: Significant version releases (v1.0, v2.0, etc.)
- **Problem Resolution**: When other workflows fail due to dependency issues

### Fallback Mechanisms

#### vcpkg Fallback
```yaml
# Primary: vcpkg installation
- name: Set up vcpkg (Windows)
  continue-on-error: true

# Fallback: Manual dependency installation
- name: Install dependencies manually (Windows fallback)
  if: steps.vcpkg_setup.outcome == 'failure'
```

#### Chocolatey Integration
```yaml
# Install Chocolatey if not present
if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
  # Install Chocolatey
}

# Install dependencies via Chocolatey
choco install -y cmake ninja git python3
```

#### Build Configuration Fallback
```yaml
# Primary: Full build with vcpkg
cmake -DCMAKE_TOOLCHAIN_FILE="vcpkg.cmake" -DYAZE_MINIMAL_BUILD=OFF

# Fallback: Minimal build without vcpkg
cmake -DYAZE_MINIMAL_BUILD=ON
```

### Advanced Features
- **Multi-stage Validation**: Visual Studio project validation
- **Artifact Verification**: Comprehensive build artifact checking
- **Debug Information**: Extensive logging for troubleshooting
- **Environment Detection**: Automatic environment configuration

---

## Workflow Comparison Matrix

| Feature | Simplified | Release | Complex |
|---------|------------|---------|---------|
| **vcpkg Integration** | Basic | Enhanced | Advanced + Fallback |
| **Error Handling** | Minimal | Standard | Comprehensive |
| **Fallback Mechanisms** | None | Limited | Multiple |
| **Debugging** | Basic | Standard | Extensive |
| **Dependency Management** | Fixed | Updated | Adaptive |
| **Build Validation** | Simple | Enhanced | Multi-stage |
| **Failure Recovery** | None | Limited | Automatic |
| **Production Ready** | No | Yes | Yes |
| **Build Time** | Fast | Medium | Slow |
| **Reliability** | Low | High | Maximum |

---

## When to Use Each Workflow

### Use Simplified When:
- ✅ Testing release process during development
- ✅ Creating beta or alpha releases
- ✅ Quick hotfix releases
- ✅ Validating basic CI/CD functionality
- ✅ Development team testing

### Use Release When:
- ✅ Creating stable production releases
- ✅ Feature releases with full testing
- ✅ Release candidates
- ✅ Standard version releases
- ✅ Most production scenarios

### Use Complex When:
- ✅ Critical production releases
- ✅ Major version releases (v1.0, v2.0)
- ✅ Enterprise customer releases
- ✅ When other workflows fail
- ✅ Maximum reliability requirements
- ✅ Complex dependency scenarios

---

## Workflow Selection Guide

### For Development Team
```
Development → Simplified
Testing → Release
Production → Complex
```

### For Release Manager
```
Hotfix → Simplified
Feature Release → Release
Major Release → Complex
```

### For CI/CD Pipeline
```
PR Validation → Simplified
Nightly Builds → Release
Release Pipeline → Complex
```

---

## Configuration Examples

### Triggering a Release

#### Manual Release (All Workflows)
```bash
# Using workflow_dispatch
gh workflow run release.yml -f tag=v0.3.0
gh workflow run release-simplified.yml -f tag=v0.3.0-beta
gh workflow run release-complex.yml -f tag=v1.0.0
```

#### Automatic Release (Tag Push)
```bash
# Creates release automatically
git tag v0.3.0
git push origin v0.3.0
```

### Customizing Release Notes
All workflows support automatic changelog extraction:
```bash
# Extract changelog for version
python3 scripts/extract_changelog.py "0.3.0" > release_notes.md
```

---

## Troubleshooting

### Common Issues

#### vcpkg Failures (Windows)
- **Simplified**: Fails completely
- **Release**: Basic error reporting
- **Complex**: Automatic fallback to manual installation

#### Dependency Conflicts
- **Simplified**: Manual intervention required
- **Release**: Enhanced error reporting
- **Complex**: Multiple resolution strategies

#### Build Failures
- **Simplified**: Basic error output
- **Release**: Enhanced debugging
- **Complex**: Comprehensive failure analysis

### Debug Information

#### Simplified Workflow
- Basic build output
- Simple error messages
- Minimal logging

#### Release Workflow
- Enhanced error reporting
- Artifact verification
- Build validation

#### Complex Workflow
- Extensive debug output
- Multi-stage validation
- Comprehensive error analysis
- Automatic fallback execution

---

## Best Practices

### Workflow Selection
1. **Start with Simplified** for development and testing
2. **Use Release** for standard production releases
3. **Use Complex** only when maximum reliability is required

### Release Process
1. **Test with Simplified** first
2. **Validate with Release** for production readiness
3. **Use Complex** for critical releases

### Maintenance
1. **Keep all workflows updated** with latest dependency versions
2. **Monitor workflow performance** and adjust as needed
3. **Document any custom modifications** for team knowledge

---

## Future Improvements

### Planned Enhancements
- **Automated Workflow Selection**: Based on release type and criticality
- **Enhanced Fallback Strategies**: Additional dependency resolution methods
- **Performance Optimization**: Reduced build times while maintaining reliability
- **Cross-Platform Consistency**: Unified behavior across all platforms

### Integration Opportunities
- **Release Automation**: Integration with semantic versioning
- **Quality Gates**: Automated quality checks before release
- **Distribution**: Integration with package managers and app stores

---

## CI/CD Reliability Improvements

### Retry Mechanisms

**vcpkg Setup (Windows)**:
- Two-stage vcpkg setup with automatic retry on failure
- Reuses existing clone on retry for faster recovery
- Handles network issues and GitHub API rate limits

**Dependency Installation**:
- Platform-specific retry logic with cleanup
- Ubuntu: `apt-get clean` and `--fix-missing` on retry  
- macOS: `brew update` before retry
- Typical success rate improvement: 60% → 95%

### Enhanced Error Reporting

**Build Failures**:
- Full build logs captured to `build.log`
- GitHub error annotations for quick identification
- Grouped verbose output for clean UI
- CMake error logs displayed on failure

**Artifacts on Failure**:
- Automatic upload of diagnostic files (cmake_config.log, build.log, CMakeError.log)
- 7-day retention for debugging
- Platform-specific artifacts

###Windows vcpkg Configuration

**Manifest Mode**:
- Dependencies declared in `vcpkg.json`
- Automatic installation during CMake configure
- Locked baseline for reproducible builds

**Static Triplet**:
- Uses `x64-windows-static` for CI consistency
- Eliminates DLL deployment issues
- Faster builds with binary caching

**Key Settings**:
```yaml
env:
  VCPKG_DEFAULT_TRIPLET: x64-windows-static
cmake:
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
  -DVCPKG_MANIFEST_MODE=ON
```

---

*This documentation is maintained alongside the yaze project. For updates or corrections, please refer to the project repository.*
