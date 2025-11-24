/**
 * @fileoverview Web terminal component for z3ed WASM integration
 * Provides an interactive terminal interface for the z3ed CLI tool
 * running in the browser via WebAssembly.
 */

(function() {
  'use strict';

  // Configuration constants
  const CONFIG = {
    maxHistorySize: 50,
    defaultPrompt: 'z3ed> ',
    apiKeyStorageKey: 'z3ed_gemini_api_key',
    maxOutputLines: 1000
  };

  // Built-in commands that are handled client-side
  const BUILTIN_COMMANDS = {
    '/help': showHelp,
    '/clear': clearTerminal,
    '/apikey': handleApiKey,
    '/history': showHistory,
    '/version': showVersion
  };

  /**
   * Z3edTerminal - Web terminal component for z3ed WASM
   * @class
   */
  class Z3edTerminal {
    /**
     * Create a new terminal instance
     * @param {string} containerId - ID of the container element
     */
    constructor(containerId) {
      this.container = document.getElementById(containerId);
      if (!this.container) {
        console.error('Z3edTerminal: Container element not found:', containerId);
        return;
      }

      // DOM elements
      this.outputElement = this.container.querySelector('.terminal-output') ||
                           document.getElementById('terminal-output');
      this.inputElement = this.container.querySelector('input[type="text"]') ||
                          document.getElementById('terminal-input');
      this.promptElement = this.container.querySelector('.terminal-prompt');
      this.headerElement = this.container.querySelector('.terminal-header');
      this.toggleElement = this.container.querySelector('.terminal-toggle');

      // State
      this.history = [];
      this.historyIndex = -1;
      this.currentInput = '';
      this.prompt = CONFIG.defaultPrompt;
      this.isCollapsed = true;
      this.isModuleReady = false;
      this.commandQueue = [];

      // Initialize
      this.init();
    }

    /**
     * Initialize the terminal
     */
    init() {
      this.bindEvents();
      this.loadHistory();
      this.checkModuleReady();
      this.showApiKeyStatus();

      // Set global reference for C++ callbacks
      window.z3edTerminal = this;

      console.log('Z3edTerminal initialized');
    }

    /**
     * Bind event listeners
     */
    bindEvents() {
      if (this.inputElement) {
        this.inputElement.addEventListener('keydown', this.handleKeyDown.bind(this));
        this.inputElement.addEventListener('keyup', this.handleKeyUp.bind(this));
      }

      if (this.headerElement) {
        this.headerElement.addEventListener('click', this.toggle.bind(this));
      }

      // Listen for Module ready state
      if (typeof Module !== 'undefined' && Module.onRuntimeInitialized) {
        const originalCallback = Module.onRuntimeInitialized;
        const self = this;
        Module.onRuntimeInitialized = function() {
          if (typeof originalCallback === 'function') {
            originalCallback();
          }
          self.onModuleReady();
        };
      }

      // Check periodically if Module becomes ready
      this.moduleCheckInterval = setInterval(() => {
        if (this.checkModuleReady()) {
          clearInterval(this.moduleCheckInterval);
        }
      }, 500);
    }

    /**
     * Check if WASM Module is ready
     * @returns {boolean}
     */
    checkModuleReady() {
      if (typeof Module !== 'undefined' && Module.calledRun) {
        if (!this.isModuleReady) {
          this.onModuleReady();
        }
        return true;
      }
      return false;
    }

    /**
     * Called when WASM Module is ready
     */
    onModuleReady() {
      this.isModuleReady = true;

      // Process any queued commands
      while (this.commandQueue.length > 0) {
        const command = this.commandQueue.shift();
        this.executeCommand(command);
      }
    }

    /**
     * Handle keydown events
     * @param {KeyboardEvent} event
     */
    handleKeyDown(event) {
      // Stop event from bubbling to Emscripten/Global handlers to ensure typing works
      event.stopPropagation();

      switch (event.key) {
        case 'Enter':
          event.preventDefault();
          this.submitCommand();
          break;

        case 'ArrowUp':
          event.preventDefault();
          this.navigateHistory(-1);
          break;

        case 'ArrowDown':
          event.preventDefault();
          this.navigateHistory(1);
          break;

        case 'Tab':
          event.preventDefault();
          this.handleTabCompletion();
          break;

        case 'Escape':
          // Clear input on Escape
          this.inputElement.value = '';
          this.historyIndex = -1;
          break;

        case 'c':
          // Ctrl+C to cancel/clear
          if (event.ctrlKey) {
            event.preventDefault();
            this.print('^C', 'terminal-error');
            this.inputElement.value = '';
            this.historyIndex = -1;
          }
          break;

        case 'l':
          // Ctrl+L to clear screen
          if (event.ctrlKey) {
            event.preventDefault();
            this.clear();
          }
          break;
      }
    }

    /**
     * Handle keyup events (for saving current input state)
     * @param {KeyboardEvent} event
     */
    handleKeyUp(event) {
      // Save current input when not navigating history
      if (this.historyIndex === -1) {
        this.currentInput = this.inputElement.value;
      }
    }

    /**
     * Submit the current command
     */
    submitCommand() {
      const command = this.inputElement.value.trim();
      if (!command) return;

      // Echo the command
      this.print(this.prompt + command, 'terminal-command');

      // Add to history
      this.addToHistory(command);

      // Clear input
      this.inputElement.value = '';
      this.historyIndex = -1;
      this.currentInput = '';

      // Execute the command
      this.executeCommand(command);
    }

    /**
     * Execute a command
     * @param {string} command
     */
    executeCommand(command) {
      // Check for built-in commands first
      const parts = command.split(/\s+/);
      const cmd = parts[0].toLowerCase();
      const args = parts.slice(1);

      if (BUILTIN_COMMANDS.hasOwnProperty(cmd)) {
        BUILTIN_COMMANDS[cmd].call(this, args);
        return;
      }

      // Check if Module is ready
      if (!this.isModuleReady) {
        this.printInfo('WASM module loading... Command queued.');
        this.commandQueue.push(command);
        return;
      }

      // Call WASM function
      try {
        if (typeof Module !== 'undefined' && typeof Module.ccall === 'function') {
          const result = Module.ccall(
            'Z3edProcessCommand',
            'string',
            ['string'],
            [command]
          );

          if (result) {
            this.print(result);
          }
        } else {
          this.printError('WASM module not available. Please wait for initialization.');
        }
      } catch (error) {
        this.printError('Error executing command: ' + error.message);
        console.error('Z3edTerminal command error:', error);
      }
    }

    /**
     * Handle tab completion
     */
    handleTabCompletion() {
      const input = this.inputElement.value;
      if (!input) return;

      // Try to get completions from WASM
      if (this.isModuleReady && typeof Module !== 'undefined') {
        try {
          // Check if completion function exists
          if (typeof Module._Z3edGetCompletions === 'function') {
            const completions = Module.ccall(
              'Z3edGetCompletions',
              'string',
              ['string'],
              [input]
            );

            if (completions) {
              const options = completions.split('\n').filter(s => s.length > 0);
              if (options.length === 1) {
                // Single completion - apply it
                this.inputElement.value = options[0];
              } else if (options.length > 1) {
                // Multiple completions - show them
                this.print('');
                this.print(options.join('  '), 'terminal-info');
                this.print(this.prompt + input, 'terminal-command');
              }
            }
          } else {
            // Fallback: basic command completion
            this.fallbackCompletion(input);
          }
        } catch (error) {
          console.warn('Tab completion error:', error);
          this.fallbackCompletion(input);
        }
      } else {
        this.fallbackCompletion(input);
      }
    }

    /**
     * Fallback tab completion for built-in commands
     * @param {string} input
     */
    fallbackCompletion(input) {
      const builtinCmds = Object.keys(BUILTIN_COMMANDS);
      const matches = builtinCmds.filter(cmd => cmd.startsWith(input));

      if (matches.length === 1) {
        this.inputElement.value = matches[0] + ' ';
      } else if (matches.length > 1) {
        this.print('');
        this.print(matches.join('  '), 'terminal-info');
      }
    }

    /**
     * Navigate command history
     * @param {number} direction - -1 for up, 1 for down
     */
    navigateHistory(direction) {
      if (this.history.length === 0) return;

      if (direction === -1) {
        // Going up (older)
        if (this.historyIndex < this.history.length - 1) {
          this.historyIndex++;
          this.inputElement.value = this.history[this.history.length - 1 - this.historyIndex];
        }
      } else {
        // Going down (newer)
        if (this.historyIndex > 0) {
          this.historyIndex--;
          this.inputElement.value = this.history[this.history.length - 1 - this.historyIndex];
        } else if (this.historyIndex === 0) {
          this.historyIndex = -1;
          this.inputElement.value = this.currentInput;
        }
      }

      // Move cursor to end
      setTimeout(() => {
        this.inputElement.selectionStart = this.inputElement.value.length;
        this.inputElement.selectionEnd = this.inputElement.value.length;
      }, 0);
    }

    /**
     * Add command to history
     * @param {string} command
     */
    addToHistory(command) {
      // Don't add duplicates of the last command
      if (this.history.length > 0 && this.history[this.history.length - 1] === command) {
        return;
      }

      this.history.push(command);

      // Limit history size
      if (this.history.length > CONFIG.maxHistorySize) {
        this.history.shift();
      }

      // Save to localStorage
      this.saveHistory();
    }

    /**
     * Save history to localStorage
     */
    saveHistory() {
      try {
        localStorage.setItem('z3ed_history', JSON.stringify(this.history));
      } catch (error) {
        console.warn('Failed to save history:', error);
      }
    }

    /**
     * Load history from localStorage
     */
    loadHistory() {
      try {
        const saved = localStorage.getItem('z3ed_history');
        if (saved) {
          this.history = JSON.parse(saved);
        }
      } catch (error) {
        console.warn('Failed to load history:', error);
        this.history = [];
      }
    }

    /**
     * Toggle terminal visibility
     */
    toggle() {
      this.isCollapsed = !this.isCollapsed;

      if (this.isCollapsed) {
        this.container.classList.add('terminal-collapsed');
        if (this.toggleElement) {
          this.toggleElement.textContent = '\u25B2'; // Up arrow
        }
      } else {
        this.container.classList.remove('terminal-collapsed');
        if (this.toggleElement) {
          this.toggleElement.textContent = '\u25BC'; // Down arrow
        }
        // Focus input when expanding
        this.focus();
      }
    }

    /**
     * Print text to the terminal output
     * @param {string} text - Text to print
     * @param {string} className - Optional CSS class for styling
     */
    print(text, className) {
      if (!this.outputElement) return;

      const line = document.createElement('div');
      line.className = 'terminal-line' + (className ? ' ' + className : '');

      // Handle ANSI-style color codes (basic support)
      text = this.processColorCodes(text);
      line.innerHTML = this.escapeHtml(text);

      this.outputElement.appendChild(line);

      // Limit output lines
      while (this.outputElement.children.length > CONFIG.maxOutputLines) {
        this.outputElement.removeChild(this.outputElement.firstChild);
      }

      // Scroll to bottom
      this.outputElement.scrollTop = this.outputElement.scrollHeight;
    }

    /**
     * Print error text (red)
     * @param {string} text
     */
    printError(text) {
      this.print(text, 'terminal-error');
    }

    /**
     * Print info text (blue/cyan)
     * @param {string} text
     */
    printInfo(text) {
      this.print(text, 'terminal-info');
    }

    /**
     * Print success text (green)
     * @param {string} text
     */
    printSuccess(text) {
      this.print(text, 'terminal-success');
    }

    /**
     * Print warning text (yellow)
     * @param {string} text
     */
    printWarning(text) {
      this.print(text, 'terminal-warning');
    }

    /**
     * Clear the terminal output
     */
    clear() {
      if (this.outputElement) {
        this.outputElement.innerHTML = '';
      }
    }

    /**
     * Focus the input field
     */
    focus() {
      if (this.inputElement) {
        this.inputElement.focus();
      }
    }

    /**
     * Set the prompt text
     * @param {string} prompt
     */
    setPrompt(prompt) {
      this.prompt = prompt;
      if (this.promptElement) {
        this.promptElement.textContent = prompt;
      }
    }

    /**
     * Escape HTML special characters
     * @param {string} text
     * @returns {string}
     */
    escapeHtml(text) {
      const div = document.createElement('div');
      div.textContent = text;
      return div.innerHTML;
    }

    /**
     * Process basic ANSI color codes
     * @param {string} text
     * @returns {string}
     */
    processColorCodes(text) {
      // Basic ANSI code replacement (simplified)
      // This is a simple implementation - could be expanded for full ANSI support
      return text
        .replace(/\x1b\[31m/g, '<span class="terminal-red">')
        .replace(/\x1b\[32m/g, '<span class="terminal-green">')
        .replace(/\x1b\[33m/g, '<span class="terminal-yellow">')
        .replace(/\x1b\[34m/g, '<span class="terminal-blue">')
        .replace(/\x1b\[35m/g, '<span class="terminal-magenta">')
        .replace(/\x1b\[36m/g, '<span class="terminal-cyan">')
        .replace(/\x1b\[0m/g, '</span>')
        .replace(/\x1b\[\d+m/g, ''); // Remove other codes
    }

    /**
     * Show API key status on startup
     */
    showApiKeyStatus() {
      const apiKey = sessionStorage.getItem(CONFIG.apiKeyStorageKey);
      if (apiKey) {
        const masked = apiKey.substring(0, 4) + '...' + apiKey.substring(apiKey.length - 4);
        this.printInfo('Gemini API key configured: ' + masked);
      }
    }

    /**
     * Get the stored API key
     * @returns {string|null}
     */
    getApiKey() {
      return sessionStorage.getItem(CONFIG.apiKeyStorageKey);
    }

    /**
     * Set the API key
     * @param {string} key
     */
    setApiKey(key) {
      sessionStorage.setItem(CONFIG.apiKeyStorageKey, key);
    }

    /**
     * Clear the API key
     */
    clearApiKey() {
      sessionStorage.removeItem(CONFIG.apiKeyStorageKey);
    }
  }

  // ============================================================
  // Built-in Command Implementations
  // ============================================================

  /**
   * Show help message
   */
  function showHelp() {
    const helpText = [
      '',
      '=== z3ed Web Terminal Help ===',
      '',
      'Built-in Commands:',
      '  /help              Show this help message',
      '  /clear             Clear the terminal output',
      '  /apikey <key>      Set Gemini API key (stored in sessionStorage)',
      '  /apikey clear      Clear the stored API key',
      '  /apikey            Show current API key status',
      '  /history           Show command history',
      '  /version           Show version information',
      '',
      'z3ed Commands (when WASM is loaded):',
      '  help               Show z3ed command help',
      '  catalog            Browse ROM resource catalog',
      '  query <question>   Ask AI about ROM data',
      '  analyze            Analyze loaded ROM',
      '',
      'Keyboard Shortcuts:',
      '  Up/Down            Navigate command history',
      '  Tab                Auto-complete commands',
      '  Ctrl+C             Cancel/clear current input',
      '  Ctrl+L             Clear terminal screen',
      '  Escape             Clear input',
      ''
    ];

    helpText.forEach(line => this.print(line));
  }

  /**
   * Clear terminal output
   */
  function clearTerminal() {
    this.clear();
  }

  /**
   * Handle API key command
   * @param {string[]} args
   */
  function handleApiKey(args) {
    if (args.length === 0) {
      // Show status
      const apiKey = this.getApiKey();
      if (apiKey) {
        const masked = apiKey.substring(0, 4) + '...' + apiKey.substring(apiKey.length - 4);
        this.printInfo('Gemini API key: ' + masked);
      } else {
        this.printWarning('No Gemini API key configured.');
        this.print('Use: /apikey <your-api-key>');
      }
    } else if (args[0].toLowerCase() === 'clear') {
      this.clearApiKey();
      this.printSuccess('API key cleared.');
    } else {
      const key = args[0];
      if (key.length < 10) {
        this.printError('Invalid API key format.');
        return;
      }
      this.setApiKey(key);
      const masked = key.substring(0, 4) + '...' + key.substring(key.length - 4);
      this.printSuccess('API key set: ' + masked);

      // Notify WASM if available
      if (this.isModuleReady && typeof Module !== 'undefined' && typeof Module.ccall === 'function') {
        try {
          Module.ccall('Z3edSetApiKey', null, ['string'], [key]);
        } catch (error) {
          // Function may not exist
        }
      }
    }
  }

  /**
   * Show command history
   */
  function showHistory() {
    if (this.history.length === 0) {
      this.printInfo('No command history.');
      return;
    }

    this.print('');
    this.print('=== Command History ===');
    this.history.forEach((cmd, index) => {
      this.print('  ' + (index + 1) + ': ' + cmd);
    });
    this.print('');
  }

  /**
   * Show version information
   */
  function showVersion() {
    this.print('');
    this.print('z3ed Web Terminal v1.0.0');
    this.print('YAZE - Yet Another Zelda3 Editor');

    if (this.isModuleReady && typeof Module !== 'undefined') {
      try {
        const version = Module.ccall('Z3edGetVersion', 'string', [], []);
        if (version) {
          this.print('z3ed WASM: ' + version);
        }
      } catch (error) {
        // Version function may not exist
      }
    } else {
      this.printWarning('WASM module not yet loaded.');
    }
    this.print('');
  }

  // ============================================================
  // Global Exports for C++ Integration
  // ============================================================

  /**
   * Print text to the terminal (called from C++ via EM_JS)
   * @param {string} text
   */
  window.z3edPrint = function(text) {
    if (window.z3edTerminal) {
      window.z3edTerminal.print(text);
    } else {
      console.log('[z3ed]', text);
    }
  };

  /**
   * Print error to the terminal (called from C++ via EM_JS)
   * @param {string} text
   */
  window.z3edPrintError = function(text) {
    if (window.z3edTerminal) {
      window.z3edTerminal.printError(text);
    } else {
      console.error('[z3ed]', text);
    }
  };

  /**
   * Print info to the terminal (called from C++ via EM_JS)
   * @param {string} text
   */
  window.z3edPrintInfo = function(text) {
    if (window.z3edTerminal) {
      window.z3edTerminal.printInfo(text);
    } else {
      console.info('[z3ed]', text);
    }
  };

  /**
   * Print success to the terminal (called from C++ via EM_JS)
   * @param {string} text
   */
  window.z3edPrintSuccess = function(text) {
    if (window.z3edTerminal) {
      window.z3edTerminal.printSuccess(text);
    } else {
      console.log('[z3ed:success]', text);
    }
  };

  /**
   * Print warning to the terminal (called from C++ via EM_JS)
   * @param {string} text
   */
  window.z3edPrintWarning = function(text) {
    if (window.z3edTerminal) {
      window.z3edTerminal.printWarning(text);
    } else {
      console.warn('[z3ed]', text);
    }
  };

  /**
   * Clear the terminal (called from C++ via EM_JS)
   */
  window.z3edClear = function() {
    if (window.z3edTerminal) {
      window.z3edTerminal.clear();
    }
  };

  /**
   * Get the stored API key (called from C++ via EM_JS)
   * @returns {string}
   */
  window.z3edGetApiKey = function() {
    if (window.z3edTerminal) {
      return window.z3edTerminal.getApiKey() || '';
    }
    return sessionStorage.getItem(CONFIG.apiKeyStorageKey) || '';
  };

  // ============================================================
  // Global Toggle Function (referenced by shell.html)
  // ============================================================

  /**
   * Toggle terminal visibility (called from shell.html button)
   */
  window.toggleTerminal = function() {
    if (window.z3edTerminal) {
      window.z3edTerminal.toggle();
    } else {
      // Fallback if terminal not yet initialized
      const container = document.getElementById('terminal-container');
      if (container) {
        container.classList.toggle('terminal-collapsed');
      }
    }
  };

  // ============================================================
  // Export Class
  // ============================================================

  window.Z3edTerminal = Z3edTerminal;

})();
