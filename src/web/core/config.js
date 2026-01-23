/**
 * @file config.js
 * @brief Default configuration for yaze WASM application
 *
 * This file provides the default configuration values for the yaze web application.
 * You can customize these values by modifying this file or by setting
 * window.YAZE_CONFIG before this script loads.
 *
 * Usage:
 * 1. Include this script in your HTML before the WASM module loads
 * 2. Optionally override values by setting window.YAZE_CONFIG first
 *
 * Example customization:
 * ```html
 * <script>
 *   window.YAZE_CONFIG = {
 *     collaboration: {
 *       serverUrl: "wss://my-server.com/ws"
 *     }
 *   };
 * </script>
 * <script src="config.js"></script>
 * ```
 */

(function() {
  'use strict';

  // Default configuration
  const defaultConfig = {
    // Collaboration settings for multi-user editing
    // Uses yaze-server (https://github.com/scawful/yaze-server) for WebSocket backend
    collaboration: {
      // WebSocket server URL (empty = disabled)
      // For local development: 'ws://localhost:8765'
      // For production: 'wss://your-domain.com' (requires SSL proxy)
      // For halext deployment: 'wss://yaze.halext.org/ws'
      serverUrl: '',

      // Time before a user is considered inactive (seconds)
      userTimeoutSeconds: 30.0,

      // Minimum time between cursor position updates (milliseconds)
      cursorSendIntervalMs: 100,

      // Maximum size of a single ROM change (bytes)
      // Must match server's maxChangeSizeBytes setting
      maxChangeSizeBytes: 1024,

      // Enable automatic reconnection on disconnect
      autoReconnect: true,

      // Maximum reconnection attempts before giving up
      maxReconnectAttempts: 10,

      // Initial reconnection delay (doubles each attempt, max 30s)
      initialReconnectDelayMs: 1000
    },

    // Autosave and crash recovery settings
    autosave: {
      // Time between auto-saves (seconds)
      intervalSeconds: 60,

      // Number of recovery slots to keep
      maxRecoverySlots: 5
    },

    // Web terminal settings
    terminal: {
      // Maximum command history items
      maxHistoryItems: 50,

      // Maximum output lines to keep
      maxOutputLines: 1000
    },

    // UI settings
    ui: {
      // Minimum zoom level
      minZoom: 0.25,

      // Maximum zoom level
      maxZoom: 4.0,

      // Touch gesture threshold (pixels)
      touchGestureThreshold: 10
    },

    // Cache and storage settings
    cache: {
      // Cache version identifier (change to invalidate cache)
      version: 'v1',

      // Maximum ROM cache size (MB)
      maxRomCacheSizeMb: 100
    },

    // AI service settings (for terminal AI commands)
    // Connects to yaze-server's AI endpoint or direct Gemini API
    ai: {
      // Enable AI features in terminal
      enabled: true,

      // AI provider ("gemini" or "openai")
      provider: 'gemini',

      // AI model to use (gemini-2.5-flash, gemini-2.5-pro, etc.)
      model: 'gemini-2.5-flash',

      // OpenAI-compatible base URL (used when provider = openai)
      // Examples: https://api.openai.com/v1, http://localhost:1234/v1
      openaiBaseUrl: 'https://api.openai.com/v1',

      // Server endpoint for AI queries (empty = use collaboration server)
      // Set to your yaze-server URL if different from collaboration server
      endpoint: '',

      // Maximum response length (characters)
      maxResponseLength: 4096,

      // Authentication settings for client-side Gemini API usage
      auth: {
        // Google OAuth2 Client ID (for Device Flow or Implicit Flow)
        // Defaulting to a placeholder - user must configure this for BYOK
        clientId: '',
        
        // OAuth2 Endpoints
        deviceEndpoint: 'https://oauth2.googleapis.com/device/code',
        tokenEndpoint: 'https://oauth2.googleapis.com/token',
        
        // Scopes required for Gemini API
        scopes: 'https://www.googleapis.com/auth/generative-language.retriever'
      }
    },

    // Server deployment info (read-only, for diagnostics)
    deployment: {
      // yaze-server repository
      serverRepo: 'https://github.com/scawful/yaze-server',

      // Default server port
      defaultPort: 8765,

      // Protocol version supported
      protocolVersion: '2.0'
    }
  };

  // Deep merge function to combine user config with defaults
  function deepMerge(target, source) {
    const output = Object.assign({}, target);
    if (isObject(target) && isObject(source)) {
      Object.keys(source).forEach(key => {
        if (isObject(source[key])) {
          if (!(key in target)) {
            Object.assign(output, { [key]: source[key] });
          } else {
            output[key] = deepMerge(target[key], source[key]);
          }
        } else {
          Object.assign(output, { [key]: source[key] });
        }
      });
    }
    return output;
  }

  function isObject(item) {
    return (item && typeof item === 'object' && !Array.isArray(item));
  }

  // Merge user configuration with defaults
  const userConfig = window.YAZE_CONFIG || {};
  const finalConfig = deepMerge(defaultConfig, userConfig);

  // Assign to both legacy and new namespace locations
  window.YAZE_CONFIG = finalConfig;

  // Integrate with yaze namespace if available
  if (window.yaze) {
    window.yaze.config = finalConfig;
  }

  // Try to read collaboration server URL from <meta name="yaze-collab-server"> tag
  // This allows deployment-specific configuration without modifying config.js
  if (!finalConfig.collaboration.serverUrl) {
    const metaTag = document.querySelector('meta[name="yaze-collab-server"]');
    if (metaTag && metaTag.content) {
      finalConfig.collaboration.serverUrl = metaTag.content;
    }
  }

  // If still unset and running on a halext.org host, default to the same host's
  // WebSocket endpoint under /ws so GH Pages (via yaze.halext.org) can talk to
  // the collab server without hosting the WASM bundle locally.
  if (!finalConfig.collaboration.serverUrl && typeof window !== 'undefined' && window.location) {
    const host = window.location.host || '';
    if (host.endsWith('halext.org')) {
      const scheme = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      finalConfig.collaboration.serverUrl = `${scheme}//${host}/ws`;
    }
  }

  // Log configuration status
  if (finalConfig.collaboration.serverUrl) {
    console.log('[yaze] Collaboration server configured:',
                finalConfig.collaboration.serverUrl);
  } else {
    console.log('[yaze] Collaboration disabled (no server URL configured)');
  }

  console.log('[yaze] Configuration loaded:', finalConfig);
})();
