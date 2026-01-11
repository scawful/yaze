# Web Terminal Integration for Z3ed

This document describes how to integrate the z3ed terminal functionality into the WASM web build.

## Overview

The `wasm_terminal_bridge.cc` file provides C++ functions that can be called from JavaScript to enable z3ed command processing in the browser. This allows users to interact with ROM data and use AI-powered features directly in the web interface.

## Exported Functions

### Core Functions

```javascript
// Process a z3ed command
const char* Z3edProcessCommand(const char* command);

// Get command completions for autocomplete
const char* Z3edGetCompletions(const char* partial);

// Set API key for AI services (Gemini-only in browser)
void Z3edSetApiKey(const char* api_key);

// Check if terminal bridge is ready
int Z3edIsReady();

// Load ROM data from ArrayBuffer
int Z3edLoadRomData(const uint8_t* data, size_t size);

// Get current ROM information as JSON
const char* Z3edGetRomInfo();

// Execute resource queries
const char* Z3edQueryResource(const char* query);
```

## JavaScript Integration Example

```javascript
// Initialize the terminal when module is ready
Module.onRuntimeInitialized = function() {
  // Check if terminal is ready
  if (Module.ccall('Z3edIsReady', 'number', [], [])) {
    console.log('Z3ed terminal ready');
  }

  // Set API key for AI features
  const apiKey = localStorage.getItem('gemini_api_key');
  if (apiKey) {
    Module.ccall('Z3edSetApiKey', null, ['string'], [apiKey]);
  }

  // Create terminal interface
  const terminal = new Terminal({
    prompt: 'z3ed> ',
    onCommand: (cmd) => {
      const result = Module.ccall('Z3edProcessCommand', 'string', ['string'], [cmd]);
      terminal.print(result);
    },
    onTab: (partial) => {
      const completions = Module.ccall('Z3edGetCompletions', 'string', ['string'], [partial]);
      return JSON.parse(completions);
    }
  });

  // Expose terminal globally
  window.z3edTerminal = terminal;
};

// Load ROM file
async function loadRomFile(file) {
  const arrayBuffer = await file.arrayBuffer();
  const data = new Uint8Array(arrayBuffer);

  // Allocate memory in WASM heap
  const ptr = Module._malloc(data.length);
  Module.HEAPU8.set(data, ptr);

  // Load ROM
  const success = Module.ccall('Z3edLoadRomData', 'number',
    ['number', 'number'], [ptr, data.length]);

  // Free memory
  Module._free(ptr);

  if (success) {
    // Get ROM info
    const info = Module.ccall('Z3edGetRomInfo', 'string', [], []);
    console.log('ROM loaded:', JSON.parse(info));
  }
}
```

**Note:** The WASM terminal bridge currently wires API keys to Gemini in
`BrowserAIService`. For OpenAI/Anthropic, use a collaboration server with
`AI_AGENT_ENDPOINT` or extend the bridge to pass provider selection.

## Terminal UI Component

```javascript
class Z3edTerminal {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.history = [];
    this.historyIndex = 0;
    this.setupUI();
  }

  setupUI() {
    this.container.innerHTML = `
      <div class="terminal-output"></div>
      <div class="terminal-input">
        <span class="prompt">z3ed> </span>
        <input type="text" class="command-input" />
      </div>
    `;

    this.output = this.container.querySelector('.terminal-output');
    this.input = this.container.querySelector('.command-input');

    this.input.addEventListener('keydown', (e) => this.handleKeydown(e));
  }

  handleKeydown(e) {
    if (e.key === 'Enter') {
      this.executeCommand(this.input.value);
      this.history.push(this.input.value);
      this.historyIndex = this.history.length;
      this.input.value = '';
    } else if (e.key === 'Tab') {
      e.preventDefault();
      this.handleAutocomplete();
    } else if (e.key === 'ArrowUp') {
      this.navigateHistory(-1);
    } else if (e.key === 'ArrowDown') {
      this.navigateHistory(1);
    }
  }

  executeCommand(cmd) {
    this.print(`z3ed> ${cmd}`, 'command');

    if (!Module.ccall) {
      this.printError('WASM module not loaded');
      return;
    }

    try {
      const result = Module.ccall('Z3edProcessCommand', 'string', ['string'], [cmd]);
      this.print(result);
    } catch (error) {
      this.printError(`Error: ${error.message}`);
    }
  }

  handleAutocomplete() {
    const partial = this.input.value;
    const completions = Module.ccall('Z3edGetCompletions', 'string', ['string'], [partial]);
    const options = JSON.parse(completions);

    if (options.length === 1) {
      this.input.value = options[0];
    } else if (options.length > 1) {
      this.print(`Available commands: ${options.join(', ')}`);
    }
  }

  navigateHistory(direction) {
    this.historyIndex = Math.max(0, Math.min(this.history.length, this.historyIndex + direction));
    this.input.value = this.history[this.historyIndex] || '';
  }

  print(text, className = 'output') {
    const line = document.createElement('div');
    line.className = className;
    line.textContent = text;
    this.output.appendChild(line);
    this.output.scrollTop = this.output.scrollHeight;
  }

  printError(text) {
    this.print(text, 'error');
  }

  clear() {
    this.output.innerHTML = '';
  }
}
```

## Available Commands

The WASM build includes a subset of z3ed commands that don't require native dependencies:

### Basic Commands
- `help` - Show available commands
- `help <category>` - Show commands in a category
- `clear` - Clear terminal output
- `version` - Show version information

### ROM Commands
- `rom-info` - Display ROM information
- `rom-validate` - Validate ROM structure
- `rom-diff --rom_a <file> --rom_b <file>` - Compare two ROM files

### Resource Commands
- `resource-list --type <type>` - List resource labels
- `resource-search --query <query>` - Search resource labels

### Agent Commands (requires API key)
- `agent simple-chat` - Interactive AI chat
- `agent plan <task>` - Generate an execution plan
- `agent diff` - Review pending proposal diff
- `agent list` / `agent describe <resource>` - List or describe resources

### Graphics & Hex Commands
- `hex-read --address <hex> --length <bytes>` - Read ROM bytes
- `hex-write --address <hex> --data <hex>` - Write ROM bytes
- `hex-search --pattern <hex>` - Search for a byte pattern
- `palette-get-colors --palette <id>` - Read palette colors
- `palette-set-color --palette <id> --index <idx> --color <hex>` - Update a palette entry

## Build Configuration

The WASM terminal bridge is automatically included when building with Emscripten:

```bash
# Configure for WASM with AI support
cmake --preset wasm-ai

# Build
cmake --build build --target yaze

# The resulting files will be:
# - yaze.js (JavaScript loader)
# - yaze.wasm (WebAssembly module)
# - yaze.html (Example HTML page)
```

## Security Considerations

1. **API Keys**: Store API keys in sessionStorage or localStorage, never hardcode them
2. **ROM Data**: ROM data stays in browser memory, never sent to servers
3. **CORS**: AI API requests go through browser fetch, respecting CORS policies
4. **Sandboxing**: WASM runs in browser sandbox with limited filesystem access

## Troubleshooting

### Module not loading
- Ensure WASM files are served with correct MIME type: `application/wasm`
- Check browser console for CORS errors
- Verify SharedArrayBuffer support if using threads

### Commands not working
- Check if ROM is loaded: `Z3edGetRomInfo()`
- Verify terminal is ready: `Z3edIsReady()`
- Check browser console for error messages

### AI features not working
- Ensure API key is set: `Z3edSetApiKey()`
- Check network tab for API request failures
- Verify Gemini API quota and limits
