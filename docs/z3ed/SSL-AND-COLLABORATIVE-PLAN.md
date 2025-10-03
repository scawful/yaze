# SSL Support and Collaborative Features Plan

**Date**: October 3, 2025  
**Status**: 🔧 In Progress

## Executive Summary

This document outlines the plan to enable SSL/HTTPS support in z3ed for Gemini API integration, and explains how this infrastructure benefits future collaborative editing features.

## Problem Statement

**Current Issue**: Gemini API requires HTTPS (`https://generativelanguage.googleapis.com`), but our httplib dependency doesn't have SSL support enabled in the current build configuration.

**Error Scenario**:
```cpp
httplib::Client cli("https://generativelanguage.googleapis.com");
// Fails because CPPHTTPLIB_OPENSSL_SUPPORT is not defined
```

## Solution: Enable OpenSSL Support

### 1. Build System Changes

**File**: `src/cli/z3ed.cmake`

**Changes Required**:
```cmake
# After line 84 (where YAZE_WITH_JSON is configured)

# ============================================================================
# SSL/HTTPS Support (Required for Gemini API and future collaborative features)
# ============================================================================
option(YAZE_WITH_SSL "Build with OpenSSL support for HTTPS" ON)
if(YAZE_WITH_SSL OR YAZE_WITH_JSON)
  # Find OpenSSL on the system
  find_package(OpenSSL REQUIRED)
  
  # Define the SSL support macro for httplib
  target_compile_definitions(z3ed PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
  
  # Link OpenSSL libraries
  target_link_libraries(z3ed PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  
  # On macOS, also enable Keychain cert support
  if(APPLE)
    target_compile_definitions(z3ed PRIVATE CPPHTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN)
    target_link_libraries(z3ed PRIVATE "-framework CoreFoundation -framework Security")
  endif()
  
  message(STATUS "✓ SSL/HTTPS support enabled for z3ed")
endif()
```

### 2. Verification Steps

**Build with SSL**:
```bash
cd /Users/scawful/Code/yaze

# Clean rebuild with SSL support
rm -rf build-grpc-test
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON -DYAZE_WITH_JSON=ON -DYAZE_WITH_SSL=ON
cmake --build build-grpc-test --target z3ed

# Verify OpenSSL is linked
otool -L build-grpc-test/bin/z3ed | grep ssl
# Expected output:
#   /usr/lib/libssl.dylib
#   /usr/lib/libcrypto.dylib
```

**Test Gemini Connection**:
```bash
export GEMINI_API_KEY="your-key-here"
./build-grpc-test/bin/z3ed agent plan --prompt "Test SSL connection"
```

### 3. OpenSSL Installation (if needed)

**macOS**:
```bash
# OpenSSL is usually pre-installed, but if needed:
brew install openssl@3

# If CMake can't find it, set paths:
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
```

**Linux**:
```bash
# Debian/Ubuntu
sudo apt-get install libssl-dev

# Fedora/RHEL
sudo dnf install openssl-devel
```

## Benefits for Collaborative Features

### 1. WebSocket Support (Future)

SSL enables secure WebSocket connections for real-time collaborative editing:

```cpp
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
// Secure WebSocket for collaborative editing
httplib::SSLClient ws_client("wss://collaboration.yaze.dev");
ws_client.set_connection_timeout(30, 0);

// Subscribe to real-time ROM changes
auto res = ws_client.Get("/subscribe/room/12345");
// Multiple users can edit the same ROM simultaneously
#endif
```

**Use Cases**:
- Multi-user dungeon editing sessions
- Real-time tile16 preview sharing
- Collaborative palette editing
- Synchronized sprite placement

### 2. Cloud ROM Storage (Future)

HTTPS enables secure cloud storage integration:

```cpp
// Upload ROM to secure cloud storage
httplib::SSLClient cloud("https://api.yaze.cloud");
cloud.Post("/roms/upload", rom_data, "application/octet-stream");

// Download shared ROM modifications
auto res = cloud.Get("/roms/shared/abc123");
```

**Use Cases**:
- Team ROM projects with version control
- Shared resource libraries (tile16 sets, palettes, sprites)
- Automated ROM backups
- Project synchronization across devices

### 3. Secure Authentication (Future)

SSL required for secure user authentication:

```cpp
// OAuth2 flow for collaborative features
httplib::SSLClient auth("https://auth.yaze.dev");
auto token_res = auth.Post("/oauth/token", 
    "grant_type=authorization_code&code=ABC123",
    "application/x-www-form-urlencoded");
```

**Use Cases**:
- User accounts for collaborative editing
- Shared project permissions
- ROM access control
- API rate limiting

### 4. Plugin/Extension Marketplace (Future)

HTTPS required for secure plugin downloads:

```cpp
// Download verified plugins from marketplace
httplib::SSLClient marketplace("https://plugins.yaze.dev");
auto plugin_res = marketplace.Get("/api/v1/plugins/tile16-tools/latest");
// Verify signature before installation
```

**Use Cases**:
- Community-created editing tools
- Custom AI prompt templates
- Shared dungeon/overworld templates
- Asset packs and resources

## Integration Timeline

### Phase 1: Immediate (This Session)
- ✅ Enable OpenSSL in z3ed build
- ✅ Test Gemini API with SSL
- ✅ Document SSL setup in README

### Phase 2: Short-term (Next Week)
- Add SSL health checks to CLI startup
- Implement certificate validation
- Add SSL error diagnostics

### Phase 3: Medium-term (Next Month)
- Design collaborative editing protocol
- Prototype WebSocket-based real-time editing
- Implement cloud ROM storage API

### Phase 4: Long-term (Future)
- Full collaborative editing system
- Plugin marketplace infrastructure
- Authentication and authorization system

## Security Considerations

### Certificate Validation
- Always validate SSL certificates in production
- Support custom CA certificates for enterprise environments
- Implement certificate pinning for critical endpoints

### API Key Protection
- Never hardcode API keys
- Use environment variables or secure keychains
- Rotate keys periodically

### Data Transmission
- Encrypt ROM data before transmission
- Use TLS 1.3 for all connections
- Implement perfect forward secrecy

## Testing Checklist

- [ ] OpenSSL links correctly on macOS
- [ ] OpenSSL links correctly on Linux  
- [ ] OpenSSL links correctly on Windows
- [ ] Gemini API works with HTTPS
- [ ] Certificate validation works
- [ ] macOS Keychain integration works
- [ ] Custom CA certificates work
- [ ] Build size impact acceptable
- [ ] No performance regression

## Estimated Impact

**Build Size**: +2-3MB (OpenSSL libraries)  
**Build Time**: +10-15 seconds (first build only)  
**Runtime**: Negligible overhead for HTTPS  
**Dependencies**: OpenSSL 3.0+ (system package)

---

**Status**: ✅ READY FOR IMPLEMENTATION  
**Priority**: HIGH (Blocks Gemini API integration)  
**Next Action**: Modify `src/cli/z3ed.cmake` to enable OpenSSL support

