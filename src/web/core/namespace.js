/**
 * @file namespace.js
 * @brief Unified namespace for yaze web application
 *
 * This file establishes the window.yaze namespace to consolidate all globals
 * and prevent namespace pollution. Must be loaded FIRST before any other
 * yaze scripts.
 *
 * Structure:
 * - yaze.config: Configuration (from config.js)
 * - yaze.core: Core systems (Module, FS, filesystem, loading)
 * - yaze.ui: UI components (terminal, panels, collab)
 * - yaze.debug: Debug utilities (yazeDebug API)
 * - yaze.utils: Utility functions (resize, zoom, fullscreen)
 * - yaze.constants: Centralized magic values
 */

(function() {
  'use strict';

  // Prevent double initialization
  if (window.yaze && window.yaze._initialized) {
    console.warn('[yaze] Namespace already initialized, skipping');
    return;
  }

  /**
   * Main yaze namespace
   * @namespace yaze
   */
  window.yaze = {
    /** Version string (overridden after WASM init when available) */
    version: (window.YAZE_VERSION || 'dev'),

    /** Initialization flag */
    _initialized: true,

    /** Configuration (populated by config.js) */
    config: null,

    /**
     * Core systems namespace
     * @namespace yaze.core
     */
    core: {
      /** Emscripten Module (set by app.js) */
      Module: null,

      /** Emscripten filesystem (set after Module init) */
      FS: null,

      /** FilesystemManager instance */
      fs: null,

      /** Loading indicator API */
      loading: null,

      /** Error handler API */
      errors: null,

      /** Agent automation API */
      automation: null,

      /** State flags */
      state: {
        wasmReady: false,
        fsReady: false
      }
    },

    /**
     * UI components namespace
     * @namespace yaze.ui
     */
    ui: {
      /** Terminal instance */
      terminal: null,

      /** Panel management */
      panels: null,

      /** Collaboration UI */
      collab: null,

      /** Touch gestures handler */
      touch: null,

      /** Drop zone handler */
      dropZone: null,

      /** Shortcuts overlay */
      shortcuts: null,

      /** Command palette */
      palette: null
    },

    /**
     * Debug utilities namespace
     * @namespace yaze.debug
     */
    debug: {
      /** Get application state */
      getState: null,

      /** Analyze specific component */
      analyze: null,

      /** Inspector utilities */
      inspector: null
    },

    /**
     * Utility functions namespace
     * @namespace yaze.utils
     */
    utils: {
      /** Resize canvas to container */
      resizeCanvas: null,

      /** Toggle fullscreen mode */
      toggleFullscreen: null,

      /** Zoom controls */
      zoom: {
        in: null,
        out: null,
        reset: null,
        current: 1.0
      },

      /** Download save files */
      downloadSaves: null,

      /** Service worker management */
      serviceWorker: null,

      /** HTML sanitization helper */
      sanitizeHtml: function(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
      },

      /** Wait for a condition with timeout */
      waitFor: function(condition, timeoutMs, intervalMs) {
        timeoutMs = timeoutMs || 10000;
        intervalMs = intervalMs || 100;
        return new Promise(function(resolve, reject) {
          var elapsed = 0;
          var check = setInterval(function() {
            if (condition()) {
              clearInterval(check);
              resolve(true);
            } else if (elapsed >= timeoutMs) {
              clearInterval(check);
              reject(new Error('Timeout waiting for condition'));
            }
            elapsed += intervalMs;
          }, intervalMs);
        });
      }
    },

    /**
     * Centralized constants
     * @namespace yaze.constants
     */
    constants: {
      /** Terminal settings */
      terminal: {
        MAX_HISTORY: 50,
        MAX_OUTPUT_LINES: 1000,
        AUTOCOMPLETE_DELAY_MS: 200,
        DEFAULT_PROMPT: 'z3ed> '
      },

      /** File handling */
      files: {
        MAX_SIZE_BYTES: 10 * 1024 * 1024,
        VALID_EXTENSIONS: ['sfc', 'smc', 'zip']
      },

      /** UI timing */
      timing: {
        DOUBLE_TAP_MS: 300,
        RIPPLE_DURATION_MS: 300,
        LOADING_HIDE_DELAY_MS: 500,
        BINDING_POLL_INTERVAL_MS: 500
      },

      /** Retry settings */
      retry: {
        MAX_ATTEMPTS: 50,
        FS_TIMEOUT_MS: 15000,
        MODULE_CHECK_INTERVAL_MS: 100
      },

      /** Collaboration */
      collab: {
        MAX_CHANGES: 50,
        SESSION_TIMEOUT_MS: 300000,
        RECONNECT_PROMPT_MS: 30000,
        SESSION_RESTORE_DELAY_MS: 1000
      },

      /** Storage keys */
      storageKeys: {
        TERMINAL_HISTORY: 'z3ed_history',
        GEMINI_API_KEY: 'z3ed_gemini_api_key',
        COLLAB_SESSION: 'collab-session',
        COLLAB_USERNAME: 'collab-username',
        COLLAB_SERVER_URL: 'collab-server-url'
      },

      /** CSS color variables (for JS-based styling) */
      colors: {
        ACCENT_CYAN: 'var(--accent-cyan, #4ecdc4)',
        SUCCESS: 'var(--status-success, #00ff00)',
        ERROR: 'var(--status-error, #f44336)',
        MUTED: 'var(--text-muted, #888888)'
      }
    },

    /**
     * Event emitter for cross-component communication
     * @namespace yaze.events
     */
    events: {
      _listeners: {},

      /**
       * Subscribe to an event
       * @param {string} event - Event name
       * @param {Function} callback - Callback function
       * @returns {Function} Unsubscribe function
       */
      on: function(event, callback) {
        if (!this._listeners[event]) {
          this._listeners[event] = [];
        }
        // Prevent duplicate listeners
        if (this._listeners[event].indexOf(callback) !== -1) {
          return function() { /* noop - already registered */ };
        }
        this._listeners[event].push(callback);
        var self = this;
        return function() {
          self.off(event, callback);
        };
      },

      /**
       * Unsubscribe from an event
       * @param {string} event - Event name
       * @param {Function} callback - Callback to remove
       */
      off: function(event, callback) {
        if (!this._listeners[event]) return;
        this._listeners[event] = this._listeners[event].filter(function(cb) {
          return cb !== callback;
        });
      },

      /**
       * Emit an event
       * @param {string} event - Event name
       * @param {*} data - Event data
       */
      emit: function(event, data) {
        if (!this._listeners[event]) return;
        // Copy array to allow modifications during iteration
        var listeners = this._listeners[event].slice();
        listeners.forEach(function(callback) {
          try {
            callback(data);
          } catch (err) {
            console.error('[yaze.events] Error in listener for event:', event, err);
          }
        });
      }
    }
  };

  // Define standard events
  window.yaze.events.WASM_READY = 'wasm:ready';
  window.yaze.events.FS_READY = 'fs:ready';
  window.yaze.events.ROM_LOADED = 'rom:loaded';
  window.yaze.events.ROM_SAVED = 'rom:saved';
  window.yaze.events.ERROR = 'error';

  // ============================================================
  // Boot Promise Mechanism
  // Provides a deterministic way to wait for WASM initialization
  // instead of polling with setInterval.
  // ============================================================

  // Store resolvers for the boot promise (supports multiple awaits)
  window.yaze.core._bootResolvers = [];
  window.yaze.core._bootResolved = false;
  window.yaze.core._bootModule = null;

  // Create the boot promise
  window.yaze.core.bootPromise = new Promise(function(resolve) {
    // If already resolved (late subscriber), resolve immediately
    if (window.yaze.core._bootResolved) {
      resolve(window.yaze.core._bootModule);
      return;
    }
    window.yaze.core._bootResolvers.push(resolve);
  });

  /**
   * Returns a promise that resolves when WASM is ready.
   * Safe to call multiple times - all callers get the same promise.
   * @returns {Promise<Module>} Resolves with the WASM Module
   */
  window.yaze.core.ready = function() {
    // Return new promise that resolves immediately if already booted
    if (window.yaze.core._bootResolved) {
      return Promise.resolve(window.yaze.core._bootModule);
    }
    return window.yaze.core.bootPromise;
  };

  /**
   * Called by app.js when WASM Module is fully initialized.
   * Resolves all pending boot promises.
   * @param {Object} module - The WASM Module instance
   */
  window.yaze.core._resolveBoot = function(module) {
    if (window.yaze.core._bootResolved) {
      console.warn('[yaze] Boot already resolved, ignoring duplicate call');
      return;
    }

    console.log('[yaze] Boot resolved - WASM ready');
    window.yaze.core._bootResolved = true;
    window.yaze.core._bootModule = module;
    window.yaze.core.state.wasmReady = true;
    // Note: window.yaze.core.Module is a getter that returns window.Module
    // which is set automatically by Emscripten, so no need to set it here

    // Resolve all pending promises
    window.yaze.core._bootResolvers.forEach(function(resolve) {
      resolve(module);
    });
    window.yaze.core._bootResolvers = [];

    // Emit event for listeners using the event system
    window.yaze.events.emit(window.yaze.events.WASM_READY, module);
  };

  console.log('[yaze] Namespace initialized v' + window.yaze.version);
})();

// Backward compatibility shims (deprecated, will be removed in a future release)
// These provide warnings when old globals are accessed
(function() {
  'use strict';

  var deprecationWarnings = {};

  function createDeprecationShim(oldName, newPath, getter) {
    Object.defineProperty(window, oldName, {
      get: function() {
        if (!deprecationWarnings[oldName]) {
          console.warn('[yaze] window.' + oldName + ' is deprecated. Use window.' + newPath + ' instead.');
          deprecationWarnings[oldName] = true;
        }
        return getter();
      },
      set: function(value) {
        if (!deprecationWarnings[oldName]) {
          console.warn('[yaze] window.' + oldName + ' is deprecated. Use window.' + newPath + ' instead.');
          deprecationWarnings[oldName] = true;
        }
        // Allow setting for backward compatibility with null checks
        var parts = newPath.split('.');
        var obj = window;
        try {
          for (var i = 0; i < parts.length - 1; i++) {
            if (!obj || typeof obj !== 'object') {
              console.warn('[yaze] Cannot set ' + oldName + ': path does not exist');
              return;
            }
            obj = obj[parts[i]];
          }
          if (obj && typeof obj === 'object') {
            obj[parts[parts.length - 1]] = value;
          }
        } catch (e) {
          console.warn('[yaze] Error setting deprecated property ' + oldName + ':', e.message);
        }
      },
      configurable: true
    });
  }

  // Note: These shims are created lazily by the components that define them
  // to avoid errors if components load in different orders.
  // The pattern is: when a component sets up its namespace entry,
  // it also creates the backward-compat shim if needed.

  // Export helper for components to create their own shims
  window.yaze._createDeprecationShim = createDeprecationShim;
})();
