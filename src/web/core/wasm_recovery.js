/**
 * WASM Module Recovery System
 *
 * Automatically detects WASM crashes and offers to reload the module
 * without requiring a full page refresh. Preserves ROM data in IndexedDB.
 */

(function() {
  'use strict';

  let moduleHealthy = true;
  let recoveryAttempts = 0;
  const MAX_RECOVERY_ATTEMPTS = 3;
  let lastRomPath = null;

  /**
   * Check if the WASM module is responsive
   */
  function isModuleHealthy() {
    if (typeof Module === 'undefined') return false;
    if (!Module.calledRun) return false;

    // Try a simple operation to verify module is responsive
    try {
      if (Module._malloc && Module._free) {
        const ptr = Module._malloc(4);
        if (ptr) {
          Module._free(ptr);
          return true;
        }
      }
      return false;
    } catch (e) {
      return false;
    }
  }

  /**
   * Save recovery state before crash
   */
  function saveRecoveryState() {
    const state = {
      timestamp: Date.now(),
      editor: null,
      romPath: lastRomPath,
      visiblePanels: []
    };

    try {
      if (window.yaze && window.yaze.control) {
        const editor = window.yaze.control.getCurrentEditor();
        if (editor && !editor.error) state.editor = editor.name;

        const cards = window.yaze.control.getVisiblePanels();
        if (Array.isArray(cards)) state.visiblePanels = cards;
      }
    } catch (e) {
      // Module might be dead, ignore
    }

    try {
      localStorage.setItem('yaze_recovery_state', JSON.stringify(state));
    } catch (e) {
      console.warn('[Recovery] Could not save state:', e);
    }

    return state;
  }

  /**
   * Load recovery state after reload
   */
  function loadRecoveryState() {
    try {
      const json = localStorage.getItem('yaze_recovery_state');
      if (json) {
        const state = JSON.parse(json);
        // Only use if recent (within 5 minutes)
        if (Date.now() - state.timestamp < 5 * 60 * 1000) {
          return state;
        }
      }
    } catch (e) {
      console.warn('[Recovery] Could not load state:', e);
    }
    return null;
  }

  /**
   * Clear recovery state
   */
  function clearRecoveryState() {
    try {
      localStorage.removeItem('yaze_recovery_state');
    } catch (e) {}
  }

  /**
   * Create the recovery overlay UI
   */
  function createRecoveryOverlay(errorMessage) {
    // Remove existing overlay
    const existing = document.getElementById('yaze-recovery-overlay');
    if (existing) existing.remove();

    const overlay = document.createElement('div');
    overlay.id = 'yaze-recovery-overlay';
    overlay.innerHTML = `
      <div class="yaze-recovery-content">
        <div class="yaze-recovery-icon">⚠️</div>
        <h2>Editor Crashed</h2>
        <p class="yaze-recovery-error">${escapeHtml(errorMessage || 'The WASM module encountered an error')}</p>
        <p>Your ROM data is safely stored. You can try to recover or reload the page.</p>
        <div class="yaze-recovery-buttons">
          <button class="yaze-recovery-btn primary" id="yaze-recovery-retry">
            🔄 Attempt Recovery
          </button>
          <button class="yaze-recovery-btn" id="yaze-recovery-reload">
            🔃 Reload Page
          </button>
          <button class="yaze-recovery-btn secondary" id="yaze-recovery-dismiss">
            Continue Anyway
          </button>
        </div>
        <p class="yaze-recovery-attempts">Recovery attempts: ${recoveryAttempts}/${MAX_RECOVERY_ATTEMPTS}</p>
      </div>
    `;

    // Add styles
    if (!document.getElementById('yaze-recovery-styles')) {
      const style = document.createElement('style');
      style.id = 'yaze-recovery-styles';
      style.textContent = `
        #yaze-recovery-overlay {
          position: fixed;
          top: 0;
          left: 0;
          right: 0;
          bottom: 0;
          background: rgba(0, 0, 0, 0.85);
          display: flex;
          align-items: center;
          justify-content: center;
          z-index: 100000;
          font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        }
        .yaze-recovery-content {
          background: #1e1e1e;
          border: 1px solid #ff6b6b;
          border-radius: 8px;
          padding: 32px;
          max-width: 500px;
          text-align: center;
          color: #fff;
        }
        .yaze-recovery-icon {
          font-size: 48px;
          margin-bottom: 16px;
        }
        .yaze-recovery-content h2 {
          margin: 0 0 16px 0;
          color: #ff6b6b;
        }
        .yaze-recovery-error {
          background: #2d2d2d;
          padding: 12px;
          border-radius: 4px;
          font-family: monospace;
          font-size: 12px;
          color: #ff9999;
          word-break: break-all;
          margin-bottom: 16px;
        }
        .yaze-recovery-buttons {
          display: flex;
          gap: 12px;
          justify-content: center;
          margin-top: 24px;
          flex-wrap: wrap;
        }
        .yaze-recovery-btn {
          padding: 12px 24px;
          border: none;
          border-radius: 4px;
          cursor: pointer;
          font-size: 14px;
          font-weight: 500;
          transition: background 0.2s;
        }
        .yaze-recovery-btn.primary {
          background: #4caf50;
          color: white;
        }
        .yaze-recovery-btn.primary:hover {
          background: #45a049;
        }
        .yaze-recovery-btn:not(.primary):not(.secondary) {
          background: #2196f3;
          color: white;
        }
        .yaze-recovery-btn:not(.primary):not(.secondary):hover {
          background: #1976d2;
        }
        .yaze-recovery-btn.secondary {
          background: #444;
          color: #ccc;
        }
        .yaze-recovery-btn.secondary:hover {
          background: #555;
        }
        .yaze-recovery-attempts {
          margin-top: 16px;
          font-size: 12px;
          color: #888;
        }
        .yaze-recovery-progress {
          margin-top: 16px;
          color: #4caf50;
        }
      `;
      document.head.appendChild(style);
    }

    document.body.appendChild(overlay);

    // Event handlers
    document.getElementById('yaze-recovery-retry').onclick = attemptRecovery;
    document.getElementById('yaze-recovery-reload').onclick = () => window.location.reload();
    document.getElementById('yaze-recovery-dismiss').onclick = dismissRecovery;

    return overlay;
  }

  function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }

  /**
   * Attempt to recover the WASM module
   */
  async function attemptRecovery() {
    if (recoveryAttempts >= MAX_RECOVERY_ATTEMPTS) {
      alert('Maximum recovery attempts reached. Please reload the page.');
      return;
    }

    recoveryAttempts++;
    const overlay = document.getElementById('yaze-recovery-overlay');

    if (overlay) {
      const content = overlay.querySelector('.yaze-recovery-content');
      content.innerHTML = `
        <div class="yaze-recovery-icon">🔄</div>
        <h2>Attempting Recovery...</h2>
        <p class="yaze-recovery-progress">Reinitializing WASM module...</p>
      `;
    }

    try {
      // Save current state
      const savedState = loadRecoveryState();

      // Try to reinitialize the module
      console.log('[Recovery] Attempting module reinitialization...');

      // Check if we have createYazeModule function
      if (typeof createYazeModule === 'function') {
        // Re-create the module
        const newModule = await createYazeModule({
          canvas: document.getElementById('canvas'),
          print: console.log,
          printErr: console.error
        });

        // Replace global Module
        window.Module = newModule;
        moduleHealthy = true;

        console.log('[Recovery] Module reinitialized successfully');

        // Try to restore state
        if (savedState && savedState.romPath) {
          updateRecoveryStatus('Restoring ROM data...');

          // The ROM should still be in IndexedDB/IDBFS
          // Try to reload it
          if (window.FilesystemManager && FilesystemManager.ready) {
            // ROM data should persist in IDBFS
            console.log('[Recovery] ROM should be available at:', savedState.romPath);
          }
        }

        // Close overlay
        if (overlay) overlay.remove();
        clearRecoveryState();

        // Show success toast
        if (window.showYazeToast) {
          window.showYazeToast('Editor recovered successfully!', 'success', 3000);
        }

        return true;
      } else {
        throw new Error('createYazeModule not available - page reload required');
      }
    } catch (e) {
      console.error('[Recovery] Recovery failed:', e);

      if (overlay) {
        const content = overlay.querySelector('.yaze-recovery-content');
        content.innerHTML = `
          <div class="yaze-recovery-icon">❌</div>
          <h2>Recovery Failed</h2>
          <p class="yaze-recovery-error">${escapeHtml(e.message)}</p>
          <p>Please reload the page to continue.</p>
          <div class="yaze-recovery-buttons">
            <button class="yaze-recovery-btn primary" onclick="window.location.reload()">
              🔃 Reload Page
            </button>
          </div>
        `;
      }

      return false;
    }
  }

  function updateRecoveryStatus(message) {
    const progress = document.querySelector('.yaze-recovery-progress');
    if (progress) progress.textContent = message;
  }

  /**
   * Dismiss the recovery overlay and continue (may not work)
   */
  function dismissRecovery() {
    const overlay = document.getElementById('yaze-recovery-overlay');
    if (overlay) overlay.remove();

    if (window.showYazeToast) {
      window.showYazeToast('Editor may be unstable. Save your work frequently.', 'warning', 5000);
    }
  }

  /**
   * Handle WASM crash
   */
  function handleCrash(errorMessage) {
    moduleHealthy = false;
    saveRecoveryState();
    createRecoveryOverlay(errorMessage);
  }

  /**
   * Track ROM loads for recovery
   */
  function trackRomLoad(path) {
    lastRomPath = path;
  }

  // Hook into existing error handlers
  const originalOnError = window.onerror;
  window.onerror = function(message, source, lineno, colno, error) {
    // Check if this is a fatal WASM error
    const isFatalWasm = (source && source.includes('.wasm')) ||
                        (message && (message.includes('RuntimeError') ||
                                    message.includes('unreachable') ||
                                    message.includes('memory access')));

    if (isFatalWasm && moduleHealthy) {
      handleCrash(message);
    }

    // Call original handler
    if (originalOnError) {
      return originalOnError.call(window, message, source, lineno, colno, error);
    }
    return false;
  };

  // Also hook unhandled rejections
  const originalOnUnhandled = window.onunhandledrejection;
  window.onunhandledrejection = function(event) {
    const error = event.reason;
    if (error && moduleHealthy) {
      const message = error.message || String(error);
      if (message.includes('wasm') || message.includes('RuntimeError')) {
        handleCrash(message);
      }
    }

    if (originalOnUnhandled) {
      return originalOnUnhandled.call(window, event);
    }
  };

  // Periodic health check (optional, disabled by default)
  let healthCheckInterval = null;

  function startHealthCheck(intervalMs) {
    intervalMs = intervalMs || 5000;
    if (healthCheckInterval) clearInterval(healthCheckInterval);

    healthCheckInterval = setInterval(() => {
      if (!isModuleHealthy() && moduleHealthy) {
        moduleHealthy = false;
        handleCrash('WASM module became unresponsive');
      }
    }, intervalMs);
  }

  function stopHealthCheck() {
    if (healthCheckInterval) {
      clearInterval(healthCheckInterval);
      healthCheckInterval = null;
    }
  }

  // Check for pending recovery on page load
  window.addEventListener('load', () => {
    const savedState = loadRecoveryState();
    if (savedState) {
      console.log('[Recovery] Found recovery state from previous session:', savedState);
      // Could auto-restore state here if needed
    }
  });

  // Expose recovery API
  window.yazeRecovery = {
    isHealthy: () => moduleHealthy,
    checkHealth: isModuleHealthy,
    handleCrash: handleCrash,
    attemptRecovery: attemptRecovery,
    trackRomLoad: trackRomLoad,
    startHealthCheck: startHealthCheck,
    stopHealthCheck: stopHealthCheck,
    getRecoveryState: loadRecoveryState,
    clearRecoveryState: clearRecoveryState
  };

  console.log('[Recovery] WASM recovery system initialized');

})();
