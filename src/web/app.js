/**
 * Main application logic for YAZE Web
 * Handles Emscripten Module integration, UI binding, and PWA features.
 */

var statusElement = document.getElementById('status');
var progressElement = document.getElementById('progress');
var spinnerElement = document.getElementById('spinner');
var loadingOverlay = document.getElementById('loading-overlay');
let wasmReady = false;
let fsReady = false;
let fsInitPromise = null;
let fsInitAttempts = 0;

// Check SharedArrayBuffer support before anything else
(function checkCrossOriginIsolation() {
  const hasSharedArrayBuffer = typeof SharedArrayBuffer !== 'undefined';
  const isCrossOriginIsolated = window.crossOriginIsolated === true;

  console.log('[COI] SharedArrayBuffer:', hasSharedArrayBuffer);
  console.log('[COI] crossOriginIsolated:', isCrossOriginIsolated);
  console.log('[COI] Service Worker controller:', !!navigator.serviceWorker?.controller);

  if (!hasSharedArrayBuffer) {
    console.warn('[COI] SharedArrayBuffer not available. Attempting COI setup...');

    // Check if we're in the middle of COI setup
    const coiAttempted = window.sessionStorage.getItem("coiAttempted");
    const coiReloaded = window.sessionStorage.getItem("coiReloadedBySelf");

    if (coiAttempted && coiReloaded) {
      // We've already tried COI and reloaded - show error to user
      console.error('[COI] COI setup failed after reload. SharedArrayBuffer still unavailable.');
      if (statusElement) {
        statusElement.innerHTML = 'Error: SharedArrayBuffer not available.<br>Try a Chromium-based browser or check browser settings.';
        statusElement.style.color = '#f44';
      }
      // Don't attempt to load WASM - it will fail
      window.YAZE_COI_FAILED = true;
    }
  } else {
    console.log('[COI] SharedArrayBuffer available!');
    // Clear any stale COI session markers
    window.sessionStorage.removeItem("coiAttempted");
    window.sessionStorage.removeItem("coiReloadedBySelf");
  }
})();

// Ensure loading overlay is visible initially
if (loadingOverlay) loadingOverlay.style.display = 'flex';

var Module = {
  print: (function() {
    return function(text) {
      if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
      console.log(text);
    };
  })(),
  printErr: function(text) {
    if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
    console.error(text);
  },
  canvas: (function() {
    var canvas = document.getElementById('canvas');
    canvas.addEventListener("webglcontextlost", function(e) { alert('WebGL context lost. You will need to reload the page.'); e.preventDefault(); }, false);

    // WORKAROUND: Ensure all mouse events have defined coordinates to prevent
    // SAFE_HEAP assertion failures in Emscripten's SDL event handler.
    // Some browser events can have undefined coordinates in edge cases.
    ['mousemove', 'mousedown', 'mouseup', 'mouseenter', 'mouseleave', 'wheel'].forEach(function(eventType) {
      canvas.addEventListener(eventType, function(e) {
        // Validate and fix event coordinates before Emscripten sees them
        if (typeof e.clientX !== 'number' || isNaN(e.clientX)) {
          Object.defineProperty(e, 'clientX', { value: 0, writable: false });
        }
        if (typeof e.clientY !== 'number' || isNaN(e.clientY)) {
          Object.defineProperty(e, 'clientY', { value: 0, writable: false });
        }
        if (typeof e.offsetX !== 'number' || isNaN(e.offsetX)) {
          Object.defineProperty(e, 'offsetX', { value: 0, writable: false });
        }
        if (typeof e.offsetY !== 'number' || isNaN(e.offsetY)) {
          Object.defineProperty(e, 'offsetY', { value: 0, writable: false });
        }
        if (typeof e.movementX !== 'number' || isNaN(e.movementX)) {
          Object.defineProperty(e, 'movementX', { value: 0, writable: false });
        }
        if (typeof e.movementY !== 'number' || isNaN(e.movementY)) {
          Object.defineProperty(e, 'movementY', { value: 0, writable: false });
        }
      }, true); // Use capture phase to run before Emscripten's handlers
    });

    return canvas;
  })(),
  setStatus: function(text) {
    if (!Module.setStatus.last) Module.setStatus.last = { time: Date.now(), text: '' };
    if (text === Module.setStatus.last.text) return;
    
    var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
    var now = Date.now();
    if (m && now - Module.setStatus.last.time < 30) return;
    Module.setStatus.last.time = now;
    Module.setStatus.last.text = text;
    if (m) {
      text = m[1];
      if (progressElement) {
        progressElement.value = parseInt(m[2])*100;
        progressElement.max = parseInt(m[4])*100;
        progressElement.hidden = false;
      }
      if (spinnerElement) spinnerElement.hidden = false;
      if (loadingOverlay) loadingOverlay.style.display = 'flex';
    } else {
      if (progressElement) {
        progressElement.value = null;
        progressElement.max = null;
        progressElement.hidden = true;
      }
      if (!text) {
        if (spinnerElement) spinnerElement.hidden = true;
        // Hide overlay when text is cleared (usually means done)
        if (loadingOverlay) loadingOverlay.style.display = 'none';
      }
    }
    if (statusElement) statusElement.innerHTML = text;
  },
  totalDependencies: 0,
  monitorRunDependencies: function(left) {
    this.totalDependencies = Math.max(this.totalDependencies, left);
    Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
  },
  preRun: [(function() {
    try {
      console.log('[WASM] Pre-run checks starting');

      // Check memory availability
      if (typeof performance !== 'undefined' && performance.memory) {
        const memoryMB = performance.memory.jsHeapSizeLimit / (1024 * 1024);
        if (memoryMB < 512) {
          throw new Error(`Insufficient memory: ${memoryMB.toFixed(0)}MB available, need 512MB`);
        }
      }

      // Check SharedArrayBuffer availability
      if (typeof SharedArrayBuffer === 'undefined') {
        console.warn('[WASM] SharedArrayBuffer not available. Multi-threading disabled.');
      }

      console.log('[WASM] Pre-run checks passed');
    } catch (err) {
      console.error('[WASM] Pre-run check failed:', err);
      showFatalError('Startup Failed', err.message);
    }
  })],
  onAbort: function(what) {
    console.error('[WASM] Abort:', what);
    Module.setStatus('Fatal error: ' + what);
    showFatalError('WASM Aborted', what || 'Unknown error');
  },
  onRuntimeInitialized: function() {
    try {
      console.log('[WASM] Runtime initialized successfully');

      // Hide loading overlay - canvas is already visible
      if (loadingOverlay) loadingOverlay.style.display = 'none';

      // Resize canvas to fill container
      window.resizeCanvasToContainer();

      // Verify critical functions are available
      if (typeof Module._LoadRomFromWeb === 'undefined' && !Module.ccall) {
        throw new Error('Critical WASM function missing: LoadRomFromWeb');
      }

      // Terminal auto-initializes via terminal.js - just print welcome if ready
      if (window.z3edTerminal) {
        window.z3edTerminal.printInfo('WASM ready. Type /help for commands.');
      }

      wasmReady = true;
      FilesystemManager.initPersistentFS().catch((err) => {
        console.warn('Persistent FS init failed; continuing with in-memory FS only.', err);
      });

      console.log('[WASM] Initialization complete');
    } catch (err) {
      console.error('[WASM] Initialization error:', err);
      showFatalError('Initialization Failed', err.message);
    }
  }
};

Module.setStatus('Downloading...');

window.onerror = function(event) {
  Module.setStatus('Exception thrown, see JavaScript console');
  if (spinnerElement) spinnerElement.style.display = 'none';
  Module.setStatus = function(text) {
    if (text) Module.printErr('[post-exception status] ' + text);
  };
};

// Limit SDL/Emscripten keyboard capture to the canvas so text inputs stay usable
function configureKeyboardCapture() {
  var canvas = document.getElementById('canvas');
  waitForModule(() => {
    if (canvas && typeof Module !== 'undefined') {
      // Set the canvas as the only element that receives keyboard events from SDL
      Module.keyboardListeningElement = canvas;

      // Additionally, we need to prevent SDL from capturing events when
      // the user is typing in an input field
      if (typeof SDL !== 'undefined' && SDL.receiveEvent) {
        var originalReceiveEvent = SDL.receiveEvent;
        SDL.receiveEvent = function(event) {
          // Don't let SDL capture keyboard events from input fields
          if (event.type === 'keydown' || event.type === 'keyup' || event.type === 'keypress') {
            var target = event.target;
            if (target && (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA' || target.isContentEditable)) {
              return; // Don't pass to SDL
            }
          }
          return originalReceiveEvent.call(this, event);
        };
      }
    }
  });
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', configureKeyboardCapture);
} else {
  configureKeyboardCapture();
}

function waitForModule(cb) {
  if (typeof Module !== 'undefined') {
    cb();
  } else {
    setTimeout(() => waitForModule(cb), 30);
  }
}

// ROM Upload Handling
var romInput = document.getElementById('rom-input');
if (romInput) {
  romInput.addEventListener('change', function(e) {
    var file = e.target.files[0];
    FilesystemManager.handleRomUpload(file);
  });
}

// Fullscreen toggle helper
window.toggleFullscreen = function() {
  if (!document.fullscreenElement) {
    document.documentElement.requestFullscreen().catch(err => {
      console.warn(`Error attempting to enable fullscreen: ${err.message}`);
      alert('Fullscreen request was blocked by the browser. Check browser settings or allow fullscreen for this site.');
    });
  } else {
    if (document.exitFullscreen) {
      document.exitFullscreen();
    }
  }
};

// Toggle icon based on fullscreen state
document.addEventListener('fullscreenchange', function() {
  var btn = document.getElementById('fullscreen-btn');
  var icon = btn ? btn.querySelector('.material-symbols-outlined') : null;
  if (icon) {
    icon.textContent = document.fullscreenElement ? 'fullscreen_exit' : 'fullscreen';
  }
});

// Listen for fullscreen changes to trigger resize
document.addEventListener('fullscreenchange', function() {
  // Small delay to allow browser layout to settle
  setTimeout(function() { window.resizeCanvasToContainer(); }, 100);
});

// Canvas resize function to fill container
// Exposed globally for shell.html to use
window.resizeCanvasToContainer = function resizeCanvasToContainer() {
  var canvas = document.getElementById('canvas');
  var container = document.getElementById('canvas-container');
  if (!canvas || !container) return;

  // Get container dimensions (already accounts for header and status bar via flexbox)
  var containerRect = container.getBoundingClientRect();

  // Validate dimensions are defined and numeric
  if (typeof containerRect.width !== 'number' || isNaN(containerRect.width) ||
      typeof containerRect.height !== 'number' || isNaN(containerRect.height)) {
    console.warn('[Resize] Invalid container dimensions, skipping');
    return;
  }

  var width = Math.floor(containerRect.width);
  var height = Math.floor(containerRect.height);

  // Skip if dimensions are invalid or container not visible
  if (width <= 0 || height <= 0 || !isFinite(width) || !isFinite(height)) return;

  // Check if resize is actually needed
  // In fullscreen, force update to ensure GL context matches window
  if (!document.fullscreenElement && canvas.width === width && canvas.height === height) return;

  // Set canvas size to match container exactly
  canvas.width = width;
  canvas.height = height;
  canvas.style.width = width + 'px';
  canvas.style.height = height + 'px';

  // Notify Emscripten of the resize - ensure integers
  if (Module) {
    // Standard Emscripten resize hook
    if (typeof Module.setCanvasSize === 'function') {
      try {
        Module.setCanvasSize(width | 0, height | 0); // Ensure integer with bitwise OR
      } catch (e) {
        console.warn('[Resize] Module.setCanvasSize failed:', e);
      }
    }
    // Internal helper if available
    if (typeof Module._emscripten_set_canvas_element_size === 'function') {
      try {
        Module._emscripten_set_canvas_element_size('#canvas', width | 0, height | 0);
      } catch (e) {
        console.warn('[Resize] _emscripten_set_canvas_element_size failed:', e);
      }
    }
  }

  // console.log('Canvas resized to:', width, 'x', height);
}

// Use throttled resize for real-time feedback and debounced resize for final adjustment
const throttledResize = throttle(window.resizeCanvasToContainer, 100);
const debouncedResize = debounce(window.resizeCanvasToContainer, 250);

// Resize canvas on window resize
window.addEventListener('resize', function(e) {
  // Validate window dimensions before processing resize
  if (typeof window.innerWidth !== 'number' || isNaN(window.innerWidth) ||
      typeof window.innerHeight !== 'number' || isNaN(window.innerHeight)) {
    console.warn('[Resize] Invalid window dimensions, skipping');
    return;
  }
  throttledResize();  // Immediate feedback
  debouncedResize();  // Final adjustment
});

// UI Helpers
function showFatalError(title, message) {
  const statusEl = document.getElementById('status');
  const loadingOv = document.getElementById('loading-overlay');

  if (statusEl) {
    statusEl.innerHTML = `
      <div style="color: #f44; font-weight: bold;">${title}</div>
      <div style="margin-top: 8px;">${message}</div>
      <button onclick="location.reload()" style="margin-top: 16px; padding: 8px 16px; cursor: pointer; background: #333; color: #fff; border: 1px solid #555; border-radius: 4px;">
        Reload Page
      </button>
    `;
  }

  if (loadingOv) {
    loadingOv.style.display = 'flex';
  }
}

// Utility functions for debouncing and throttling
function debounce(func, wait) {
  let timeout;
  return function executedFunction(...args) {
    const later = () => {
      clearTimeout(timeout);
      func(...args);
    };
    clearTimeout(timeout);
    timeout = setTimeout(later, wait);
  };
}

function throttle(func, limit) {
  let inThrottle;
  return function(...args) {
    if (!inThrottle) {
      func.apply(this, args);
      inThrottle = true;
      setTimeout(() => inThrottle = false, limit);
    }
  };
}

// Initial resize after a short delay to ensure layout is complete
setTimeout(window.resizeCanvasToContainer, 100);


// Service Worker & PWA Logic
const ServiceWorkerManager = {
  newWorker: null,
  updatePending: false,
  reloadCount: 0,
  maxReloads: 3,
  reloadTimestamp: 0,

  handleControllerChange: function() {
    if (!this.updatePending) {
      console.log('[PWA] Controller changed without update pending - ignoring');
      return;
    }

    // Check if we're already in a reload cycle (prevents infinite loops)
    if (sessionStorage.getItem('sw_update_reloading')) {
      console.log('[PWA] Already reloading for update - clearing flag');
      sessionStorage.removeItem('sw_update_reloading');
      this.updatePending = false;
      this.newWorker = null;
      return;
    }

    // Check for reload loop
    const now = Date.now();
    if (now - this.reloadTimestamp < 5000) { // 5 seconds
      this.reloadCount++;
      if (this.reloadCount >= this.maxReloads) {
        console.error('[PWA] Too many reloads, aborting update');
        this.updatePending = false;
        this.newWorker = null;
        return;
      }
    } else {
      this.reloadCount = 0;
    }

    this.reloadTimestamp = now;
    this.updatePending = false;

    // Set flag before reload to detect loops
    sessionStorage.setItem('sw_update_reloading', 'true');
    console.log('[PWA] Reloading for service worker update');
    window.location.reload();
  },

  requestUpdate: function(newWorker) {
    // Prevent double-trigger
    if (this.updatePending) {
      console.log('[PWA] Update already pending - ignoring');
      return;
    }
    this.updatePending = true;
    newWorker.postMessage({ type: 'SKIP_WAITING' });
    // Note: Removed auto-reload timer - controllerchange event handles reload
  }
};

if ('serviceWorker' in navigator) {
  window.addEventListener('load', async () => {
    // Better COI detection - wait for SharedArrayBuffer
    const hasSAB = typeof SharedArrayBuffer !== 'undefined';
    const hasController = navigator.serviceWorker.controller;

    if (!hasSAB && !hasController) {
      console.log('[PWA] Waiting for COI setup to complete...');
      return; // COI SW will reload page
    }

    if (!hasSAB && hasController) {
      console.error('[PWA] COI setup failed - SharedArrayBuffer unavailable');
      // Show error to user, don't attempt PWA registration
      return;
    }

    // COI is ready, safe to register PWA SW
    try {
      const registration = await navigator.serviceWorker.register('pwa/service-worker.js');
      console.log('[PWA] Service worker registered successfully');

      // Check for updates every hour
      setInterval(() => { registration.update(); }, 3600000);

      // Handle updates
      registration.addEventListener('updatefound', () => {
        const installingWorker = registration.installing;
        installingWorker.addEventListener('statechange', () => {
          if (installingWorker.state === 'installed' && navigator.serviceWorker.controller) {
            ServiceWorkerManager.newWorker = installingWorker;
            const notif = document.getElementById('update-notification');
            if (notif) notif.classList.add('show');
          }
        });
      });
    } catch (err) {
      console.error('[PWA] Service worker registration failed:', err);
    }

    // Listen for controller changes
    navigator.serviceWorker.addEventListener('controllerchange', () => {
      ServiceWorkerManager.handleControllerChange();
    });
  });
}

function updateOnlineStatus() {
  const indicator = document.getElementById('offline-indicator');
  if (!indicator) return;
  
  if (navigator.onLine) {
    indicator.classList.remove('show');
  } else {
    indicator.classList.add('show');
  }
}
window.addEventListener('online', updateOnlineStatus);
window.addEventListener('offline', updateOnlineStatus);
document.addEventListener('DOMContentLoaded', updateOnlineStatus);

window.updateServiceWorker = function() {
  const notif = document.getElementById('update-notification');
  if (notif) notif.classList.remove('show');

  if (ServiceWorkerManager.newWorker) {
    ServiceWorkerManager.requestUpdate(ServiceWorkerManager.newWorker);
  }
};

window.dismissUpdate = function() {
  var notif = document.getElementById('update-notification');
  if (notif) notif.classList.remove('show');
  // Properly cancel the update - reset state to prevent auto-reload
  ServiceWorkerManager.updatePending = false;
  ServiceWorkerManager.newWorker = null;
  console.log('[PWA] Update dismissed by user');
};

// Touch Mode (simplified)
let isTouchMode = false;
document.addEventListener('touchstart', function() {
  if (!isTouchMode) {
    isTouchMode = true;
    document.body.classList.add('yaze-touch-mode');
  }
}, { passive: true });

document.addEventListener('mousemove', function(e) {
  if (e.movementX !== 0 || e.movementY !== 0) {
    if (isTouchMode) {
      isTouchMode = false;
      document.body.classList.remove('yaze-touch-mode');
    }
  }
});

// Zoom logic shared with shell.html
let currentZoom = 1.0;
const MIN_ZOOM = 0.25;
const MAX_ZOOM = 4.0;

window.zoomIn = function() {
  currentZoom = Math.min(currentZoom * 1.25, MAX_ZOOM);
  updateZoom();
};

window.zoomOut = function() {
  currentZoom = Math.max(currentZoom / 1.25, MIN_ZOOM);
  updateZoom();
};

window.resetZoom = function() {
  currentZoom = 1.0;
  updateZoom();
  if (Module._ResetCanvasView) Module._ResetCanvasView();
};

function updateZoom() {
  const el = document.getElementById('zoom-level');
  if(el) el.textContent = Math.round(currentZoom * 100) + '%';
  if (Module.ccall) {
    try { Module.ccall('SetCanvasZoom', null, ['number'], [currentZoom]); } catch(e){}
  }
}

// Terminal Logic (Submit & Resize)
function submitTerminalCommand() {
  var input = document.getElementById('terminal-input');
  if (input && input.value.trim()) {
    // If z3ed terminal is available, use it
    if (window.z3edTerminal && typeof window.z3edTerminal.executeCommand === 'function') {
      window.z3edTerminal.executeCommand(input.value);
    } else {
      // Fallback: just echo the command
      var output = document.getElementById('terminal-output');
      if (output) {
        var line = document.createElement('div');
        line.className = 'terminal-line terminal-prompt';
        line.textContent = input.value;
        output.appendChild(line);
        output.scrollTop = output.scrollHeight;
      }
    }
    input.value = '';
  }
}

// Toggle logic is global for buttons
window.toggleTerminal = function() {
  var container = document.getElementById('terminal-container');
  if (container) {
    container.classList.toggle('terminal-collapsed');
    container.classList.remove('terminal-hidden');
    var toggle = container.querySelector('.terminal-toggle');
    if (toggle) {
      toggle.textContent = container.classList.contains('terminal-collapsed') ? '▲' : '▼';
    }
  }
};

window.hideTerminal = function() {
  var container = document.getElementById('terminal-container');
  if (container) container.classList.add('terminal-hidden');
};

window.maximizeTerminal = function() {
  var container = document.getElementById('terminal-container');
  if (container) {
    container.classList.remove('terminal-collapsed');
    container.classList.remove('terminal-hidden');
    container.style.height = '70vh';
    var toggle = container.querySelector('.terminal-toggle');
    if (toggle) toggle.textContent = '▼';
  }
};

window.toggleCollabConsole = function() {
  // Check for the global function defined by collab_console.js
  if (window.toggleCollabConsole_impl) {
    window.toggleCollabConsole_impl();
  } else {
    // Fallback if script not ready
    var container = document.querySelector('.yaze-console-container');
    if (container) {
      container.classList.toggle('active');
      document.body.classList.toggle('yaze-console-open');
    }
  }
};

// Event Listeners for Terminal UI
document.addEventListener('DOMContentLoaded', function() {
  // Note: Enter key handling is done by terminal.js Z3edTerminal class
  // Removed duplicate listener to avoid conflicts

  // Terminal resize handle
  var handle = document.getElementById('terminal-resize-handle');
  var container = document.getElementById('terminal-container');
  if (handle && container) {
    let isResizing = false;
    let startY, startHeight;

    handle.addEventListener('mousedown', function(e) {
      isResizing = true;
      startY = e.clientY;
      startHeight = container.offsetHeight;
      document.body.style.cursor = 'ns-resize';
      e.preventDefault();
    });

    document.addEventListener('mousemove', function(e) {
      if (!isResizing) return;
      var delta = startY - e.clientY;
      var newHeight = Math.min(Math.max(startHeight + delta, 120), window.innerHeight * 0.7);
      container.style.height = newHeight + 'px';
    });

    document.addEventListener('mouseup', function() {
      isResizing = false;
      document.body.style.cursor = '';
    });
  }
});

// Initialize the WASM module
// Since we use MODULARIZE=1, we need to call createYazeModule() explicitly
(function initWASM() {
  console.log('[WASM] Waiting for createYazeModule to be available...');

  function tryInit() {
    if (typeof createYazeModule === 'function') {
      console.log('[WASM] Initializing module...');
      createYazeModule(Module).then(function(instance) {
        console.log('[WASM] Module initialized successfully');
        window.Module = instance;
      }).catch(function(err) {
        console.error('[WASM] Module initialization failed:', err);
        showFatalError('WASM Initialization Failed', err.message || err.toString());
      });
    } else {
      // yaze.js not loaded yet, try again in 100ms
      setTimeout(tryInit, 100);
    }
  }

  // Start trying to initialize after DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', tryInit);
  } else {
    tryInit();
  }
})();

// Initialize Debug API wrapper
window.yazeDebug = window.yazeDebug || {};

// Add switchToEditor wrapper
window.yazeDebug.switchToEditor = function(name) {
  if (window.Module && window.Module.switchToEditor) {
    try {
      var result = window.Module.switchToEditor(name);
      console.log('[yazeDebug] switchToEditor result:', result);
      return JSON.parse(result);
    } catch(e) {
      console.error('[yazeDebug] switchToEditor failed:', e);
      return { error: e.toString() };
    }
  }
  console.warn('[yazeDebug] Module.switchToEditor not available');
  return { error: "Module not ready" };
};

// Add setAgentMode wrapper (using executeCommand)
window.yazeDebug.setAgentMode = function(enabled) {
  if (window.Module && window.Module.executeCommand) {
    // We don't have a direct C++ binding for SetAgentMode yet, 
    // but we can try to use executeCommand if we add a CLI command for it.
    // For now, we'll just log.
    console.warn('[yazeDebug] setAgentMode not implemented yet');
    return { error: "Not implemented" };
  }
  return { error: "Module not ready" };
};
