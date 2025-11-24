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

      // DOM elements - More flexible selector to work with different structures
      this.outputElement = this.container.querySelector('.terminal-output') ||
                           document.getElementById('terminal-output');
      this.inputElement = this.container.querySelector('.terminal-input') ||
                          this.container.querySelector('input[type="text"]') ||
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
      this.completionCache = {};  // Cache for autocomplete
      this.currentCompletions = [];  // Current completion suggestions
      this.completionIndex = -1;  // Index in completion list

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

      // Focus the input field to ensure it's ready for typing
      setTimeout(() => {
        this.focusInput();
      }, 100);

      console.log('Z3edTerminal initialized');
    }

    /**
     * Bind event listeners
     */
    bindEvents() {
      if (this.inputElement) {
        this.inputElement.addEventListener('keydown', this.handleKeyDown.bind(this));
        this.inputElement.addEventListener('keyup', this.handleKeyUp.bind(this));
        // Ensure emulator-level key listeners don't steal terminal input
        document.addEventListener('keydown', (event) => {
          if (event.target === this.inputElement) {
            event.stopPropagation();
          }
        }, true);
        // Make any click within the terminal focus the input
        if (this.container) {
          this.container.addEventListener('click', () => this.focusInput());
        }
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
     * Focus the terminal input safely
     */
    focusInput() {
      if (this.inputElement) {
        this.inputElement.focus();
        // Place cursor at end without selecting text
        const len = this.inputElement.value.length;
        this.inputElement.setSelectionRange(len, len);
      }
    }

    /**
     * Handle keydown events
     * @param {KeyboardEvent} event
     */
    handleKeyDown(event) {
      // Always keep terminal input events local
      event.stopPropagation();
      event.stopImmediatePropagation();

      // Only stop propagation for special keys, allow normal typing to work
      const specialKeys = ['Enter', 'ArrowUp', 'ArrowDown', 'Tab', 'Escape'];

      switch (event.key) {
        case 'Enter':
          event.preventDefault();
          // Clear any autocomplete suggestions before submitting
          this.clearAutocompleteSuggestions();
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
          event.preventDefault();
          // Clear input and any autocomplete suggestions
          this.inputElement.value = '';
          this.historyIndex = -1;
          this.clearAutocompleteSuggestions();
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
     * Handle keyup events (for saving current input state and live autocomplete)
     * @param {KeyboardEvent} event
     */
    handleKeyUp(event) {
      // Save current input when not navigating history
      if (this.historyIndex === -1) {
        this.currentInput = this.inputElement.value;
      }

      // Trigger autocomplete suggestions on typing (but not for special keys)
      const specialKeys = ['Enter', 'ArrowUp', 'ArrowDown', 'Tab', 'Escape', 'Control', 'Alt', 'Meta', 'Shift'];
      if (!specialKeys.includes(event.key) && this.inputElement.value.length > 0) {
        // Debounce autocomplete to avoid too many calls
        if (this.autocompleteTimeout) {
          clearTimeout(this.autocompleteTimeout);
        }
        this.autocompleteTimeout = setTimeout(() => {
          const completions = this.getCompletions(this.inputElement.value);
          if (completions.length > 0 && completions.length <= 10) {
            this.currentCompletions = completions;
            this.completionIndex = -1;
            this.showAutocompleteSuggestions(completions);
          } else {
            this.clearAutocompleteSuggestions();
          }
        }, 200);  // 200ms delay for autocomplete
      } else if (this.inputElement.value.length === 0) {
        this.clearAutocompleteSuggestions();
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

      // Get all available completions
      const completions = this.getCompletions(input);

      if (completions.length === 0) {
        // No completions available
        return;
      } else if (completions.length === 1) {
        // Single completion - apply it
        this.inputElement.value = completions[0].command + ' ';
        this.clearAutocompleteSuggestions();
      } else {
        // Multiple completions - cycle through them or show all
        if (this.currentCompletions.length > 0 &&
            this.currentCompletions[0].command.startsWith(input.split(' ')[0])) {
          // Already showing completions, cycle through them
          this.completionIndex = (this.completionIndex + 1) % this.currentCompletions.length;
          const selected = this.currentCompletions[this.completionIndex];
          this.inputElement.value = selected.command + ' ';
          this.showAutocompleteSuggestions(this.currentCompletions, this.completionIndex);
        } else {
          // Show new completions
          this.currentCompletions = completions;
          this.completionIndex = -1;
          this.showAutocompleteSuggestions(completions);
        }
      }
    }

    /**
     * Get command completions
     * @param {string} input - The partial command
     * @returns {Array} Array of completion objects with command and description
     */
    getCompletions(input) {
      if (!input) {
        // Show all top-level commands
        return this.getAllCommands();
      }

      const parts = input.trim().split(' ');
      const firstWord = parts[0].toLowerCase();

      // Try to get completions from WASM if available
      if (this.isModuleReady && typeof Module !== 'undefined') {
        try {
          const wasmCompletions = Module.ccall(
            'Z3edGetCompletions',
            'string',
            ['string'],
            [input]
          );

          if (wasmCompletions) {
            // Parse JSON response
            try {
              const parsed = JSON.parse(wasmCompletions);
              if (Array.isArray(parsed)) {
                return parsed.map(cmd => ({
                  command: cmd,
                  description: this.getCommandDescription(cmd)
                }));
              }
            } catch (e) {
              // Fallback to string parsing
              const options = wasmCompletions.split('\n').filter(s => s.length > 0);
              return options.map(cmd => ({
                command: cmd,
                description: this.getCommandDescription(cmd)
              }));
            }
          }
        } catch (error) {
          console.warn('WASM completion error:', error);
        }
      }

      // Fallback to local completions
      return this.getLocalCompletions(input);
    }

    /**
     * Get local command completions
     * @param {string} input
     * @returns {Array} Array of completion objects
     */
    getLocalCompletions(input) {
      const allCommands = this.getAllCommands();
      const matches = allCommands.filter(cmd =>
        cmd.command.toLowerCase().startsWith(input.toLowerCase())
      );
      return matches;
    }

    /**
     * Get all available commands with descriptions
     * @returns {Array} Array of command objects
     */
    getAllCommands() {
      // Define all z3ed commands with descriptions
      const commands = [
        // Built-in commands
        { command: '/help', description: 'Show help message' },
        { command: '/clear', description: 'Clear terminal output' },
        { command: '/apikey', description: 'Manage Gemini API key' },
        { command: '/history', description: 'Show command history' },
        { command: '/version', description: 'Show version information' },

        // z3ed commands
        { command: 'help', description: 'Show z3ed command help' },
        { command: 'rom', description: 'ROM management commands' },
        { command: 'hex', description: 'Hexadecimal operations' },
        { command: 'palette', description: 'Palette manipulation' },
        { command: 'sprite', description: 'Sprite operations' },
        { command: 'music', description: 'Music/sound commands' },
        { command: 'dialogue', description: 'Dialogue text commands' },
        { command: 'message', description: 'Message text commands' },
        { command: 'resource', description: 'Resource catalog commands' },
        { command: 'dungeon', description: 'Dungeon editor commands' },
        { command: 'overworld', description: 'Overworld editor commands' },
        { command: 'gui', description: 'GUI automation commands' },
        { command: 'emulator', description: 'Emulator control' },
        { command: 'ai', description: 'AI assistant commands' },
        { command: 'query', description: 'Query ROM data with AI' },
        { command: 'analyze', description: 'Analyze loaded ROM' },
        { command: 'catalog', description: 'Browse ROM resource catalog' }
      ];

      return commands;
    }

    /**
     * Get command description
     * @param {string} command
     * @returns {string} Description of the command
     */
    getCommandDescription(command) {
      const allCommands = this.getAllCommands();
      const found = allCommands.find(cmd => cmd.command === command);
      return found ? found.description : '';
    }

    /**
     * Show autocomplete suggestions
     * @param {Array} suggestions - Array of suggestion objects
     * @param {number} selectedIndex - Currently selected index
     */
    showAutocompleteSuggestions(suggestions, selectedIndex = -1) {
      // Remove existing suggestions
      this.clearAutocompleteSuggestions();

      if (suggestions.length === 0) return;

      // Create suggestions container
      const suggestionsDiv = document.createElement('div');
      suggestionsDiv.className = 'terminal-autocomplete';
      suggestionsDiv.style.cssText = `
        position: absolute;
        bottom: 100%;
        left: 0;
        right: 0;
        background: rgba(0, 0, 0, 0.95);
        border: 1px solid #00ff00;
        border-bottom: none;
        max-height: 150px;
        overflow-y: auto;
        z-index: 1000;
      `;

      // Add each suggestion
      suggestions.forEach((suggestion, index) => {
        const item = document.createElement('div');
        item.className = 'autocomplete-item';
        item.style.cssText = `
          padding: 4px 8px;
          cursor: pointer;
          display: flex;
          justify-content: space-between;
          ${index === selectedIndex ? 'background: rgba(0, 255, 0, 0.1);' : ''}
        `;

        const command = document.createElement('span');
        command.textContent = suggestion.command;
        command.style.color = '#00ff00';

        const description = document.createElement('span');
        description.textContent = suggestion.description;
        description.style.cssText = 'color: #888; font-size: 0.9em; margin-left: 10px;';

        item.appendChild(command);
        if (suggestion.description) {
          item.appendChild(description);
        }

        // Click to select
        item.addEventListener('click', () => {
          this.inputElement.value = suggestion.command + ' ';
          this.inputElement.focus();
          this.clearAutocompleteSuggestions();
        });

        // Hover effect
        item.addEventListener('mouseenter', () => {
          item.style.background = 'rgba(0, 255, 0, 0.1)';
        });
        item.addEventListener('mouseleave', () => {
          if (index !== selectedIndex) {
            item.style.background = 'transparent';
          }
        });

        suggestionsDiv.appendChild(item);
      });

      // Add to input container
      if (this.inputElement && this.inputElement.parentElement) {
        this.inputElement.parentElement.style.position = 'relative';
        this.inputElement.parentElement.appendChild(suggestionsDiv);
      }
    }

    /**
     * Clear autocomplete suggestions
     */
    clearAutocompleteSuggestions() {
      const existing = document.querySelector('.terminal-autocomplete');
      if (existing) {
        existing.remove();
      }
      this.currentCompletions = [];
      this.completionIndex = -1;
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

  // ============================================================
  // Auto-Initialize Terminal
  // ============================================================

  /**
   * Initialize terminal when DOM is ready
   */
  function initializeTerminal() {
    // Find the terminal container
    const terminalContainer = document.getElementById('panel-terminal') ||
                              document.getElementById('terminal-container') ||
                              document.querySelector('.terminal-container');

    if (terminalContainer) {
      console.log('[Terminal] Initializing Z3edTerminal...');
      window.z3edTerminal = new Z3edTerminal(terminalContainer.id);
      console.log('[Terminal] Z3edTerminal initialized successfully');
    } else {
      console.warn('[Terminal] Terminal container not found, retrying in 500ms...');
      setTimeout(initializeTerminal, 500);
    }
  }

  // Initialize after DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initializeTerminal);
  } else {
    initializeTerminal();
  }

})();
