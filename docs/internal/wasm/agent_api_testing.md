# WASM Agent API Testing Guide

**Status:** Active
**Last Updated:** 2025-11-27
**Purpose:** Guide for browser-capable AI agents to test the new `window.yaze.agent` API
**Audience:** AI agents (Gemini Antigravity, Claude, etc.) testing the WASM web port

## Overview

The yaze WASM build now includes a dedicated `window.yaze.agent` API namespace for AI/LLM agent integration. This API enables browser-based agents to:

- Send messages to the built-in AI chat system
- Access and manage chat history
- Configure AI providers (Ollama, Gemini, Mock)
- Review, accept, and reject code proposals
- Control the agent sidebar UI

This guide provides step-by-step instructions for testing all agent API features.

## Prerequisites

### 1. Serve the WASM Build

```bash
# Build (if needed)
./scripts/build-wasm.sh

# Serve locally
./scripts/serve-wasm.sh --force 8080
```

### 2. Open in Browser

Navigate to `http://127.0.0.1:8080` in a browser with DevTools access.

### 3. Load a ROM

Drop a Zelda 3 ROM file onto the application or use the File menu to load one. Many agent APIs require a loaded ROM.

### 4. Verify Module Ready

Open browser DevTools console and verify:

```javascript
// Check if module is ready
window.Module?.calledRun  // Should be true

// Check if control API is ready
window.yaze.control.isReady()  // Should be true

// Check if agent API is ready
window.yaze.agent.isReady()  // Should be true (after ROM loaded)
```

## API Reference

### Agent Namespace: `window.yaze.agent`

All agent APIs return JSON objects with either success data or `{error: "message"}`.

---

## Testing Workflow

### Phase 1: Verify API Availability

Run these commands in the browser console to verify the agent API is available:

```javascript
// 1. Check API readiness
console.log("Agent ready:", window.yaze.agent.isReady());

// 2. List all available methods
console.log("Agent API methods:", Object.keys(window.yaze.agent));
// Expected: sendMessage, getChatHistory, getConfig, setConfig,
//           getProviders, getProposals, acceptProposal, rejectProposal,
//           getProposalDetails, openSidebar, closeSidebar, isReady
```

### Phase 2: Test Configuration APIs

```javascript
// 1. Get available AI providers
const providers = window.yaze.agent.getProviders();
console.log("Available providers:", providers);
// Expected: [{id: "mock", name: "Mock Provider", ...},
//           {id: "ollama", name: "Ollama", ...},
//           {id: "gemini", name: "Google Gemini", ...}]

// 2. Get current configuration
const config = window.yaze.agent.getConfig();
console.log("Current config:", config);
// Expected: {provider: "mock", model: "", ollama_host: "http://localhost:11434", ...}

// 3. Set new configuration
const result = window.yaze.agent.setConfig({
  provider: "ollama",
  model: "llama3",
  ollama_host: "http://localhost:11434",
  verbose: true
});
console.log("Config update result:", result);
// Expected: {success: true}

// 4. Verify configuration was applied
const newConfig = window.yaze.agent.getConfig();
console.log("Updated config:", newConfig);
```

### Phase 3: Test Sidebar Control

```javascript
// 1. Open the agent sidebar
const openResult = window.yaze.agent.openSidebar();
console.log("Open sidebar:", openResult);
// Expected: {success: true, sidebar_open: true}

// 2. Verify sidebar state via yazeDebug
const panelState = window.yazeDebug?.rightPanel?.getState?.();
console.log("Panel state:", panelState);

// 3. Close the agent sidebar
const closeResult = window.yaze.agent.closeSidebar();
console.log("Close sidebar:", closeResult);
// Expected: {success: true, sidebar_open: false}
```

### Phase 4: Test Chat APIs

```javascript
// 1. Send a test message
const msgResult = window.yaze.agent.sendMessage("Hello, agent! What can you help me with?");
console.log("Send message result:", msgResult);
// Expected: {success: true, status: "queued", message: "Hello, agent!..."}

// 2. Get chat history
const history = window.yaze.agent.getChatHistory();
console.log("Chat history:", history);
// Note: May be empty array initially - full implementation
// requires AgentChatWidget to expose history

// 3. Send a task-oriented message
const taskResult = window.yaze.agent.sendMessage("Analyze dungeon room 0");
console.log("Task message result:", taskResult);
```

### Phase 5: Test Proposal APIs

```javascript
// 1. Get current proposals
const proposals = window.yaze.agent.getProposals();
console.log("Proposals:", proposals);
// Note: Returns empty array until proposal system is integrated

// 2. Test accept proposal (with mock ID)
const acceptResult = window.yaze.agent.acceptProposal("proposal-123");
console.log("Accept result:", acceptResult);
// Expected: {success: false, error: "Proposal system not yet integrated", ...}

// 3. Test reject proposal (with mock ID)
const rejectResult = window.yaze.agent.rejectProposal("proposal-456");
console.log("Reject result:", rejectResult);

// 4. Test get proposal details
const details = window.yaze.agent.getProposalDetails("proposal-123");
console.log("Proposal details:", details);
```

## Complete Test Script

Copy and paste this complete test script into the browser console:

```javascript
// ============================================================================
// YAZE Agent API Test Suite
// ============================================================================

async function runAgentAPITests() {
  const results = [];

  function test(name, fn) {
    try {
      const result = fn();
      results.push({name, status: 'PASS', result});
      console.log(`✅ ${name}:`, result);
    } catch (e) {
      results.push({name, status: 'FAIL', error: e.message});
      console.error(`❌ ${name}:`, e.message);
    }
  }

  console.log("=== YAZE Agent API Test Suite ===\n");

  // Phase 1: Availability
  console.log("--- Phase 1: API Availability ---");
  test("Agent API exists", () => typeof window.yaze.agent === 'object');
  test("isReady() returns boolean", () => typeof window.yaze.agent.isReady() === 'boolean');
  test("All expected methods exist", () => {
    const expected = ['sendMessage', 'getChatHistory', 'getConfig', 'setConfig',
                      'getProviders', 'getProposals', 'acceptProposal', 'rejectProposal',
                      'getProposalDetails', 'openSidebar', 'closeSidebar', 'isReady'];
    const missing = expected.filter(m => typeof window.yaze.agent[m] !== 'function');
    if (missing.length > 0) throw new Error(`Missing: ${missing.join(', ')}`);
    return true;
  });

  // Phase 2: Configuration
  console.log("\n--- Phase 2: Configuration APIs ---");
  test("getProviders() returns array", () => {
    const providers = window.yaze.agent.getProviders();
    if (!Array.isArray(providers)) throw new Error("Not an array");
    if (providers.length < 3) throw new Error("Expected at least 3 providers");
    return providers;
  });
  test("getConfig() returns object", () => {
    const config = window.yaze.agent.getConfig();
    if (typeof config !== 'object') throw new Error("Not an object");
    return config;
  });
  test("setConfig() returns result", () => {
    const result = window.yaze.agent.setConfig({provider: "mock"});
    return result;
  });

  // Phase 3: Sidebar Control
  console.log("\n--- Phase 3: Sidebar Control ---");
  test("openSidebar() returns result", () => window.yaze.agent.openSidebar());
  test("closeSidebar() returns result", () => window.yaze.agent.closeSidebar());

  // Phase 4: Chat APIs
  console.log("\n--- Phase 4: Chat APIs ---");
  test("sendMessage() returns result", () => {
    return window.yaze.agent.sendMessage("Test message from API suite");
  });
  test("getChatHistory() returns array", () => {
    const history = window.yaze.agent.getChatHistory();
    if (!Array.isArray(history)) throw new Error("Not an array");
    return history;
  });

  // Phase 5: Proposal APIs
  console.log("\n--- Phase 5: Proposal APIs ---");
  test("getProposals() returns array", () => {
    const proposals = window.yaze.agent.getProposals();
    if (!Array.isArray(proposals)) throw new Error("Not an array");
    return proposals;
  });
  test("acceptProposal() returns result", () => {
    return window.yaze.agent.acceptProposal("test-proposal-id");
  });
  test("rejectProposal() returns result", () => {
    return window.yaze.agent.rejectProposal("test-proposal-id");
  });
  test("getProposalDetails() returns result", () => {
    return window.yaze.agent.getProposalDetails("test-proposal-id");
  });

  // Summary
  console.log("\n=== Test Summary ===");
  const passed = results.filter(r => r.status === 'PASS').length;
  const failed = results.filter(r => r.status === 'FAIL').length;
  console.log(`Passed: ${passed}/${results.length}`);
  console.log(`Failed: ${failed}/${results.length}`);

  if (failed > 0) {
    console.log("\nFailed tests:");
    results.filter(r => r.status === 'FAIL').forEach(r => {
      console.log(`  - ${r.name}: ${r.error}`);
    });
  }

  return results;
}

// Run the tests
runAgentAPITests();
```

## Integration with Existing APIs

The Agent API works alongside existing WASM APIs:

### Combined Usage Example

```javascript
// 1. Use control API to switch to Agent editor
window.yaze.control.switchEditor('Agent');

// 2. Use agent API to configure
window.yaze.agent.setConfig({
  provider: "ollama",
  model: "codellama"
});

// 3. Open the sidebar
window.yaze.agent.openSidebar();

// 4. Send a message
window.yaze.agent.sendMessage("Help me analyze this ROM");

// 5. Use yazeDebug for additional diagnostics
console.log(window.yazeDebug.formatForAI());
```

### Using with aiTools

```javascript
// Get full app state including agent status
const state = window.aiTools.getAppState();
console.log("App state:", state);

// Navigate to agent editor
window.aiTools.navigateTo('Agent');

// Then use agent API
window.yaze.agent.openSidebar();
```

## Error Handling

All API calls return objects. Check for errors before processing:

```javascript
const result = window.yaze.agent.sendMessage("Hello");
if (result.error) {
  console.error("API error:", result.error);
} else {
  console.log("Success:", result);
}
```

Common errors:
- `"API not ready"` - Module not initialized or ROM not loaded
- `"Agent editor not available"` - Agent UI not built (`YAZE_BUILD_AGENT_UI=OFF`)
- `"Chat widget not available"` - AgentChatWidget not initialized
- `"Proposal system not yet integrated"` - Proposal APIs pending full integration

## Current Limitations

1. **Chat History**: `getChatHistory()` returns empty array until AgentChatWidget exposes history
2. **Proposals**: Proposal APIs return stub responses until proposal system integration
3. **Message Processing**: `sendMessage()` queues messages but actual processing is async
4. **ROM Required**: Most APIs require a ROM to be loaded first

## Related Documentation

- [WASM API Reference](../wasm-yazeDebug-api-reference.md) - Full JavaScript API documentation
- [WASM Development Guide](./wasm-development-guide.md) - Building and debugging WASM
- [WASM Antigravity Playbook](./wasm-antigravity-playbook.md) - AI agent workflows

## Version History

**1.0.0** (2025-11-27)
- Initial agent API documentation
- 12 API methods: isReady, sendMessage, getChatHistory, getConfig, setConfig,
  getProviders, getProposals, acceptProposal, rejectProposal, getProposalDetails,
  openSidebar, closeSidebar
- Test suite script for automated validation
