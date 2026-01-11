/**
 * @fileoverview Web terminal component for z3ed WASM integration
 * Provides an interactive terminal interface for the z3ed CLI tool
 * running in the browser via WebAssembly.
 */

(function() {
  'use strict';

  // Configuration constants - use namespace if available, with fallbacks
  const yazeConstants = (window.yaze && window.yaze.constants) || {};
  const terminalConstants = yazeConstants.terminal || {};
  const storageKeys = yazeConstants.storageKeys || {};

  const CONFIG = {
    maxHistorySize: terminalConstants.MAX_HISTORY || 50,
    defaultPrompt: terminalConstants.DEFAULT_PROMPT || 'z3ed> ',
    apiKeyStorageKey: storageKeys.GEMINI_API_KEY || 'z3ed_gemini_api_key',
    maxOutputLines: terminalConstants.MAX_OUTPUT_LINES || 300
  };

  // Built-in commands that are handled client-side
  const BUILTIN_COMMANDS = {
    '/help': showHelp,
    '/clear': clearTerminal,
    '/apikey': handleApiKey,
    '/history': showHistory,
    '/version': showVersion,
    '/ai': handleAITools,
    'ai': handleAITools, // Alias without slash
    '/login': handleLogin,
    '/ask': handleAsk,
    'ask': handleAsk
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

      // Timers (for cleanup)
      this.moduleCheckInterval = null;
      this.autocompleteTimeout = null;
      this._focusTimeout = null;

      // Bound event handlers (for cleanup)
      this._boundHandlers = {
        keydown: this.handleKeyDown.bind(this),
        keyup: this.handleKeyUp.bind(this),
        docKeydown: null,
        docKeyup: null,
        docKeypress: null,
        containerClick: () => this.focusInput(),
        headerClick: this.toggle.bind(this)
      };

      // Initialize
      this.init();
    }

    /**
     * Initialize the terminal
     */
    init() {
      // Add ARIA attributes for accessibility
      this.setupAccessibility();

      this.bindEvents();
      this.loadHistory();
      this.checkModuleReady();
      this.showApiKeyStatus();

      // Set global reference for C++ callbacks
      window.z3edTerminal = this;

      // Focus the input field to ensure it's ready for typing
      this._focusTimeout = setTimeout(() => {
        this.focusInput();
      }, 100);

      console.log('Z3edTerminal initialized');
    }

    /**
     * Setup ARIA accessibility attributes
     */
    setupAccessibility() {
      // Terminal container
      if (this.container) {
        this.container.setAttribute('role', 'region');
        this.container.setAttribute('aria-label', 'Command terminal');
      }

      // Output area - log role for live output
      if (this.outputElement) {
        this.outputElement.setAttribute('role', 'log');
        this.outputElement.setAttribute('aria-live', 'polite');
        this.outputElement.setAttribute('aria-label', 'Terminal output');
        this.outputElement.setAttribute('aria-relevant', 'additions');
      }

      // Input field
      if (this.inputElement) {
        this.inputElement.setAttribute('aria-label', 'Terminal command input');
        this.inputElement.setAttribute('aria-autocomplete', 'list');
        this.inputElement.setAttribute('aria-haspopup', 'listbox');
        this.inputElement.setAttribute('aria-expanded', 'false');
      }

      // Header toggle
      if (this.headerElement) {
        this.headerElement.setAttribute('role', 'button');
        this.headerElement.setAttribute('aria-label', 'Toggle terminal visibility');
        this.headerElement.setAttribute('tabindex', '0');
        this.headerElement.setAttribute('aria-expanded', !this.isCollapsed ? 'true' : 'false');
      }
    }

    /**
     * Bind event listeners
     */
    bindEvents() {
      const self = this;

      if (this.inputElement) {
        this.inputElement.addEventListener('keydown', this._boundHandlers.keydown);
        this.inputElement.addEventListener('keyup', this._boundHandlers.keyup);

        // React to WASM ready event so queued commands flush immediately
        if (window.yaze && window.yaze.events && window.yaze.events.on) {
          window.yaze.events.on(window.yaze.events.WASM_READY, (module) => {
            // Prefer provided module instance
            if (module && typeof module === 'object') {
              window.Module = module;
            }
            this.onModuleReady();
          });
        }

        // Ensure emulator-level key listeners (SDL/Emscripten) don't steal terminal input
        // Use capture phase with stopImmediatePropagation for maximum isolation
        // Store handlers for cleanup
        this._boundHandlers.docKeydown = (event) => {
          if (event.target === self.inputElement) {
            self.handleKeyDown(event);
            event.stopImmediatePropagation();
          }
        };
        this._boundHandlers.docKeyup = (event) => {
          if (event.target === self.inputElement) {
            self.handleKeyUp(event);
            event.stopImmediatePropagation();
          }
        };
        this._boundHandlers.docKeypress = (event) => {
          if (event.target === self.inputElement) {
            event.stopPropagation();
            event.stopImmediatePropagation();
          }
        };

        document.addEventListener('keydown', this._boundHandlers.docKeydown, true);
        document.addEventListener('keyup', this._boundHandlers.docKeyup, true);
        document.addEventListener('keypress', this._boundHandlers.docKeypress, true);

        // Make any click within the terminal focus the input
        if (this.container) {
          this.container.addEventListener('click', this._boundHandlers.containerClick);
        }
      }

      if (this.headerElement) {
        this.headerElement.addEventListener('click', this._boundHandlers.headerClick);
      }

      // Use boot promise if available (preferred - no polling)
      if (window.yaze && window.yaze.core && window.yaze.core.ready) {
        const self = this;
        window.yaze.core.ready().then(function() {
          self.onModuleReady();
        });
      } else {
        // Fallback: Listen for Module ready state (legacy)
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

        // Fallback: Check periodically if Module becomes ready (legacy polling)
        this.moduleCheckInterval = setInterval(() => {
          if (this.checkModuleReady()) {
            clearInterval(this.moduleCheckInterval);
            this.moduleCheckInterval = null;
          }
        }, 500);
      }
    }

    /**
     * Check if WASM Module is ready
     * Uses multiple indicators for better detection with modularized builds
     * @returns {boolean}
     */
    checkModuleReady() {
      // Prefer explicit ready flag from app bootstrap
      if (window.yaze && window.yaze.core && window.yaze.core.state &&
          window.yaze.core.state.wasmReady && typeof Module !== 'undefined') {
        if (!this.isModuleReady) {
          this.onModuleReady();
        }
        return true;
      }

      if (typeof Module !== 'undefined') {
        // Check multiple indicators - modularized builds may not set calledRun
        const hasCalledRun = Module.calledRun === true;
        const hasCcall = typeof Module.ccall === 'function';
        const hasProcessCommand = typeof Module._Z3edProcessCommand === 'function';

        if (hasCalledRun || hasCcall || hasProcessCommand) {
          if (!this.isModuleReady) {
            this.onModuleReady();
          }
          return true;
        }
      }
      return false;
    }

    /**
     * Called when WASM Module is ready
     */
    onModuleReady() {
      this.isModuleReady = true;
      console.log('[Terminal] Module ready, processing queued commands');

      // Process any queued commands (handles both old string format and new object format)
      const now = Date.now();
      while (this.commandQueue.length > 0) {
        const item = this.commandQueue.shift();
        // Support both old format (string) and new format ({command, timestamp})
        const command = typeof item === 'string' ? item : item.command;
        const timestamp = typeof item === 'object' ? item.timestamp : now;

        // Skip commands older than 30 seconds
        if ((now - timestamp) > 30000) {
          console.log('[Terminal] Skipping expired command:', command);
          continue;
        }

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
      if (event._z3edHandled) return;
      event._z3edHandled = true;

      // Keep terminal input events local but allow default text entry
      event.stopPropagation();

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
      if (event._z3edHandled) return;
      event._z3edHandled = true;

      event.stopPropagation();

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
        // If the global ready flag is set, fast-path to ready to avoid stale queues
        if (window.yaze && window.yaze.core && window.yaze.core.state &&
            window.yaze.core.state.wasmReady && typeof Module !== 'undefined') {
          this.onModuleReady();
        } else {
          // Limit queue size to prevent memory growth if init fails
          if (this.commandQueue.length >= 50) {
            this.printError('Command queue full. WASM initialization may have failed.');
            this.printInfo('Try refreshing the page or check console for errors.');
            return;
          }

          // Add command with timestamp for timeout tracking
          this.commandQueue.push({ command: command, timestamp: Date.now() });

          // Clean old commands (older than 30 seconds)
          const now = Date.now();
          this.commandQueue = this.commandQueue.filter(function(item) {
            return (now - item.timestamp) < 30000;
          });

          this.printInfo('WASM module loading... Command queued (' + this.commandQueue.length + ' pending).');
          return;
        }
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

      // Update ARIA state for accessibility
      if (this.headerElement) {
        this.headerElement.setAttribute('aria-expanded', this.isCollapsed ? 'false' : 'true');
      }

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

      // SECURITY: First escape HTML to prevent XSS, then add safe color spans
      const escaped = this.escapeHtml(text);
      const colored = this.processColorCodes(escaped);
      line.innerHTML = colored;

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
     * Destroy the terminal and clean up all event listeners
     * Call this when removing the terminal from the DOM to prevent memory leaks
     */
    destroy() {
      // Clear all timers
      if (this.moduleCheckInterval) {
        clearInterval(this.moduleCheckInterval);
        this.moduleCheckInterval = null;
      }
      if (this.autocompleteTimeout) {
        clearTimeout(this.autocompleteTimeout);
        this.autocompleteTimeout = null;
      }
      if (this._focusTimeout) {
        clearTimeout(this._focusTimeout);
        this._focusTimeout = null;
      }

      // Remove input element listeners
      if (this.inputElement) {
        this.inputElement.removeEventListener('keydown', this._boundHandlers.keydown);
        this.inputElement.removeEventListener('keyup', this._boundHandlers.keyup);
      }

      // Remove document-level listeners
      if (this._boundHandlers.docKeydown) {
        document.removeEventListener('keydown', this._boundHandlers.docKeydown, true);
      }
      if (this._boundHandlers.docKeyup) {
        document.removeEventListener('keyup', this._boundHandlers.docKeyup, true);
      }
      if (this._boundHandlers.docKeypress) {
        document.removeEventListener('keypress', this._boundHandlers.docKeypress, true);
      }

      // Remove container click listener
      if (this.container && this._boundHandlers.containerClick) {
        this.container.removeEventListener('click', this._boundHandlers.containerClick);
      }

      // Remove header click listener
      if (this.headerElement && this._boundHandlers.headerClick) {
        this.headerElement.removeEventListener('click', this._boundHandlers.headerClick);
      }

      // Clear global reference
      if (window.z3edTerminal === this) {
        window.z3edTerminal = null;
      }

      // Clear namespace reference
      if (window.yaze && window.yaze.ui && window.yaze.ui.terminal === this) {
        window.yaze.ui.terminal = null;
      }

      console.log('Z3edTerminal destroyed');
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
      // Enhanced ANSI code replacement
      return text
        // Styles
        .replace(/\x1b\[1m/g, '<span class="terminal-bold">')
        .replace(/\x1b\[2m/g, '<span class="terminal-dim">')
        // Standard Colors
        .replace(/\x1b\[30m/g, '<span class="terminal-dim">') // Black as dim
        .replace(/\x1b\[31m/g, '<span class="terminal-red">')
        .replace(/\x1b\[32m/g, '<span class="terminal-green">')
        .replace(/\x1b\[33m/g, '<span class="terminal-yellow">')
        .replace(/\x1b\[34m/g, '<span class="terminal-blue">')
        .replace(/\x1b\[35m/g, '<span class="terminal-magenta">')
        .replace(/\x1b\[36m/g, '<span class="terminal-cyan">')
        .replace(/\x1b\[37m/g, '<span>') // White (default)
        // Bright Colors
        .replace(/\x1b\[90m/g, '<span class="terminal-dim">')
        .replace(/\x1b\[91m/g, '<span class="terminal-red terminal-bold">')
        .replace(/\x1b\[92m/g, '<span class="terminal-green terminal-bold">')
        .replace(/\x1b\[93m/g, '<span class="terminal-yellow terminal-bold">')
        .replace(/\x1b\[94m/g, '<span class="terminal-blue terminal-bold">')
        .replace(/\x1b\[95m/g, '<span class="terminal-magenta terminal-bold">')
        .replace(/\x1b\[96m/g, '<span class="terminal-cyan terminal-bold">')
        .replace(/\x1b\[97m/g, '<span class="terminal-bold">')
        // Reset
        .replace(/\x1b\[0m/g, '</span>')
        // Cleanup unsupported
        .replace(/\x1b\[\d+(;\d+)*m/g, '');
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
      '  Note: Browser AI uses Gemini; external providers require AI_AGENT_ENDPOINT',
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
          // Function may not exist in this WASM build - log for debugging
          console.debug('[Z3edTerminal] Z3edSetApiKey not available:', error.message);
        }
      }
    }
  }

  /**
   * Handle AI Tools commands
   * @param {string[]} args
   */
  function handleAITools(args) {
    if (!window.aiTools) {
      this.printError('AI Tools not initialized (window.aiTools missing).');
      return;
    }

    if (args.length === 0) {
      this.printInfo('AI Tools - Client Side Debugging');
      this.print('  /ai state       - Dump full app state');
      this.print('  /ai editor      - Dump editor state');
      this.print('  /ai cards       - List visible cards');
      this.print('  /ai nav <tgt>   - Navigate (e.g., room:5)');
      this.print('  /ai data <type> - Get data (room, map)');
      return;
    }

    const sub = args[0].toLowerCase();
    
    try {
      switch (sub) {
        case 'state':
          const appState = window.aiTools.getAppState();
          this.printSuccess('App State dumped to console');
          this.print(JSON.stringify(appState, null, 2));
          break;
        case 'editor':
          const edState = window.aiTools.getEditorState();
          this.printSuccess('Editor State dumped to console');
          this.print(JSON.stringify(edState, null, 2));
          break;
        case 'cards':
          const cards = window.aiTools.getVisiblePanels();
          this.print('Visible Panels:');
          if (Array.isArray(cards)) {
            cards.forEach(c => this.print(' - ' + (c.id || c)));
          } else {
            this.print(JSON.stringify(cards));
          }
          break;
        case 'nav':
          if (args[1]) {
            // Parse target from remaining args
            const target = args[1]; 
            // aiTools.navigateTo usually prompts, but we can try to simulate or parse
            if (target.includes(':')) {
               const parts = target.split(':');
               if (parts[0] === 'room') window.aiTools.jumpToRoom(parseInt(parts[1]));
               else if (parts[0] === 'map') window.aiTools.jumpToMap(parseInt(parts[1]));
               else if (parts[0] === 'editor') window.switchToEditor(parts[1]); // Global helper
               this.printSuccess('Navigated to ' + target);
            } else {
               this.printWarning('Invalid format. Use type:id (e.g., room:5)');
            }
          } else {
            window.aiTools.navigateTo(); // Opens prompt
          }
          break;
        case 'data':
           if (args[1] === 'room') {
             this.print(JSON.stringify(window.aiTools.getRoomData(), null, 2));
           } else if (args[1] === 'map') {
             this.print(JSON.stringify(window.aiTools.getMapData(), null, 2));
           } else {
             this.print('Usage: /ai data [room|map]');
           }
           break;
        default:
          this.printError('Unknown AI subcommand: ' + sub);
      }
    } catch (e) {
      this.printError('AI Tool Error: ' + e.message);
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
        // Version function may not exist in this WASM build
        console.debug('[Z3edTerminal] Z3edGetVersion not available:', error.message);
      }
    } else {
      this.printWarning('WASM module not yet loaded.');
    }
    this.print('');
  }

  /**
   * Handle Gemini Login
   */
  async function handleLogin(args) {
    if (!window.yaze.ai) {
      this.printError('AI Manager not initialized.');
      return;
    }

    // If args[0] is 'clear' or 'logout', clear tokens
    if (args[0] === 'clear' || args[0] === 'logout') {
      window.yaze.ai.logout();
      this.printSuccess('Logged out from AI service.');
      return;
    }

    try {
      this.printInfo('Initiating Device Auth...');
      const data = await window.yaze.ai.startDeviceAuth();
      
      this.print('--------------------------------------------------');
      this.print('Please authenticate with Google to use Gemini API:');
      this.print(`1. Go to: ${data.verification_url}`);
      this.print(`2. Enter code: ${data.user_code}`);
      this.print('--------------------------------------------------');
      
      this.printInfo('Waiting for authentication...');
      const tokens = await window.yaze.ai.pollForToken();
      
      this.printSuccess('Authentication successful!');
      this.printInfo(`Access Token: ${tokens.access_token.substring(0, 10)}...`);
      
    } catch (e) {
      this.printError('Login failed: ' + e.message);
    }
  }

  /**
   * Handle Ask Gemini
   */
  async function handleAsk(args) {
    if (args.length === 0) {
      this.print('Usage: /ask <your question>');
      return;
    }

    const prompt = args.join(' ');

    // If in a collaboration session, route through collab console to support host-based AI
    // We need to check if we are connected to a session but NOT the host
    // Accessing collab state is tricky from here without a clean API.
    // Let's check if the collab console is available and connected
    if (window.yazeCollab && window.yazeCollab.state && window.yazeCollab.state.isConnected) {
        // We are in a session. Delegate to collab console logic which handles
        // Host vs Client routing.
        // We simulate typing /ask in the chat input
        const collabInput = document.getElementById('yaze-chat-input');
        const collabSend = document.getElementById('yaze-chat-send');
        if (collabInput && collabSend) {
            this.printInfo('Routing request through collaboration session...');
            collabInput.value = `/ask ${prompt}`;
            collabSend.click();
            return;
        }
    }

    // Fallback to local execution (standard behavior)
    if (!window.yaze.ai) {
      this.printError('AI Manager not initialized.');
      return;
    }

    this.printInfo('Asking Gemini (Local)...');
    
    try {
      const response = await window.yaze.ai.generateContent(prompt);
      this.print('Gemini: ' + response);
    } catch (e) {
      this.printError('Error: ' + e.message);
      if (e.message.includes('No credentials')) {
        this.printInfo('Try running /login first.');
      }
    }
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

  // Integrate with yaze namespace
  if (window.yaze) {
    // Ensure ui namespace exists
    if (!window.yaze.ui) {
      window.yaze.ui = {};
    }

    // Register terminal class for external access
    window.yaze.ui.Terminal = Z3edTerminal;

    // Terminal instance will be set by initializeTerminal
    Object.defineProperty(window.yaze.ui, 'terminal', {
      get: function() { return window.z3edTerminal; },
      configurable: true
    });

    console.log('[Terminal] Integrated with yaze namespace');
  }

})();
