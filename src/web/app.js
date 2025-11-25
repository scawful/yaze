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

// Cached regex for setStatus progress parsing (avoid recompilation per call)
const PROGRESS_REGEX = /([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/;

// Worker spawn tracking to prevent infinite loops
window.YAZE_WORKER_SPAWN_COUNT = 0;
window.YAZE_WORKER_SPAWN_LIMIT = 24; // Max worker spawns (4 pool + async map loading needs ~16)
window.YAZE_WORKER_SPAWN_WINDOW_MS = 3000; // Reset count after this period
window.YAZE_WORKER_LAST_SPAWN = 0;
window.YAZE_WORKER_ABORTED = false;
window.YAZE_WORKER_ERRORS = 0;

// Intercept Worker constructor to prevent infinite spawn loops
(function interceptWorkerSpawns() {
  const OriginalWorker = window.Worker;

  window.Worker = function(url, options) {
    const now = Date.now();

    // Reset counter if enough time has passed (workers are stable)
    if (now - window.YAZE_WORKER_LAST_SPAWN > window.YAZE_WORKER_SPAWN_WINDOW_MS) {
      window.YAZE_WORKER_SPAWN_COUNT = 0;
      window.YAZE_WORKER_ERRORS = 0;
    }
    window.YAZE_WORKER_LAST_SPAWN = now;
    window.YAZE_WORKER_SPAWN_COUNT++;

    // Check if we've hit the limit
    if (window.YAZE_WORKER_SPAWN_COUNT > window.YAZE_WORKER_SPAWN_LIMIT) {
      if (!window.YAZE_WORKER_ABORTED) {
        window.YAZE_WORKER_ABORTED = true;
        console.error('[WORKER] Too many worker spawns (' + window.YAZE_WORKER_SPAWN_COUNT + '/' + window.YAZE_WORKER_SPAWN_LIMIT + ')');
        console.error('[WORKER] Errors: ' + window.YAZE_WORKER_ERRORS + '. Aborting to prevent hang.');

        // Show error to user after a small delay to allow DOM to be ready
        setTimeout(function() {
          if (typeof showFatalError === 'function') {
            showFatalError('Worker Spawn Error',
              'Too many web workers spawned (' + window.YAZE_WORKER_SPAWN_COUNT + '). ' +
              'Try: 1) Clear browser cache, 2) Disable extensions, 3) Use Chrome/Edge.');
          }
        }, 100);
      }
      // Return a minimal dummy worker
      var dummy = Object.create(OriginalWorker.prototype);
      dummy.postMessage = dummy.terminate = function() {};
      dummy.addEventListener = dummy.removeEventListener = function() {};
      return dummy;
    }

    // Only log yaze workers, not service workers
    if (url && url.includes('yaze')) {
      console.log('[WORKER] Spawn #' + window.YAZE_WORKER_SPAWN_COUNT + ': ' + url);
    }

    // Create the actual worker
    var worker = new OriginalWorker(url, options);

    // Track errors to detect crash loops
    worker.addEventListener('error', function(e) {
      window.YAZE_WORKER_ERRORS++;
      console.warn('[WORKER] Error in worker:', e.message || e);
    });

    return worker;
  };

  // Preserve prototype chain
  window.Worker.prototype = OriginalWorker.prototype;
  Object.setPrototypeOf(window.Worker, OriginalWorker);
})();

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

    // WORKAROUND: Ensure all UI events have defined integer properties to prevent
    // SAFE_HEAP assertion failures in Emscripten's SDL/HTML5 event handler.
    // The error "attempt to write non-integer (undefined) into integer heap" occurs
    // when browser events have undefined properties that SDL expects to be integers.

    // Cache to avoid re-sanitizing the same event object
    var sanitizedEvents = new WeakMap();

    // Properties to check for each event type category
    var propertiesToCheck = {
      mouse: ['clientX', 'clientY', 'offsetX', 'offsetY', 'pageX', 'pageY', 'screenX', 'screenY', 'movementX', 'movementY', 'x', 'y', 'button', 'buttons', 'which', 'detail'],
      wheel: ['deltaX', 'deltaY', 'deltaZ', 'deltaMode', 'wheelDelta', 'wheelDeltaX', 'wheelDeltaY'],
      keyboard: ['keyCode', 'charCode', 'which', 'location'],
      pointer: ['pointerId', 'width', 'height', 'tiltX', 'tiltY', 'twist', 'pressure']
    };

    // Helper to safely define integer property with default value
    function ensureIntProperty(event, prop, defaultValue) {
      var val = event[prop];
      if (typeof val !== 'number' || isNaN(val) || !isFinite(val)) {
        try {
          Object.defineProperty(event, prop, { value: defaultValue | 0, writable: false, configurable: true });
        } catch (e) {
          // Property might be non-configurable, ignore
        }
      }
    }

    // Helper to sanitize all common integer properties on an event
    function sanitizeEventIntegers(e) {
      // Skip if already sanitized (WeakMap allows garbage collection)
      if (sanitizedEvents.has(e)) return;
      sanitizedEvents.set(e, true);

      var eventType = e.type;

      // Determine which properties to check based on event type
      var props;
      if (eventType.startsWith('mouse') || eventType === 'click' || eventType === 'dblclick' || eventType === 'contextmenu') {
        props = propertiesToCheck.mouse;
      } else if (eventType.startsWith('wheel') || eventType === 'DOMMouseScroll') {
        props = propertiesToCheck.mouse.concat(propertiesToCheck.wheel);
      } else if (eventType.startsWith('key')) {
        props = propertiesToCheck.keyboard;
      } else if (eventType.startsWith('pointer')) {
        props = propertiesToCheck.mouse.concat(propertiesToCheck.pointer);
      } else {
        props = propertiesToCheck.mouse;
      }

      // Sanitize each property
      for (var i = 0; i < props.length; i++) {
        var prop = props[i];
        var defaultValue = (prop === 'width' || prop === 'height') ? 1 : 0;
        if (prop === 'pressure') {
          // Pressure is a float
          if (typeof e.pressure !== 'number' || isNaN(e.pressure)) {
            try {
              Object.defineProperty(e, 'pressure', { value: 0.0, writable: false, configurable: true });
            } catch (ex) {}
          }
        } else {
          ensureIntProperty(e, prop, defaultValue);
        }
      }
    }

    // Apply to all relevant event types - use capture phase to run before Emscripten
    // ONLY on canvas (document-level listeners are redundant and cause performance issues)
    var eventTypes = [
      'mousemove', 'mousedown', 'mouseup', 'mouseenter', 'mouseleave', 'mouseover', 'mouseout',
      'click', 'dblclick', 'contextmenu',
      'wheel', 'mousewheel', 'DOMMouseScroll',
      'pointerdown', 'pointerup', 'pointermove', 'pointerenter', 'pointerleave', 'pointerover', 'pointerout', 'pointercancel',
      'keydown', 'keyup', 'keypress',
      'focus', 'blur', 'focusin', 'focusout'
    ];

    eventTypes.forEach(function(eventType) {
      canvas.addEventListener(eventType, sanitizeEventIntegers, true);
    });

    // Expose cleanup function for event listener removal (prevents memory leaks on canvas disposal)
    window.cleanupCanvasEventSanitization = function() {
      eventTypes.forEach(function(eventType) {
        canvas.removeEventListener(eventType, sanitizeEventIntegers, true);
      });
    };

    return canvas;
  })(),
  setStatus: function(text) {
    if (!Module.setStatus.last) Module.setStatus.last = { time: Date.now(), text: '' };
    if (text === Module.setStatus.last.text) return;
    
    var m = text.match(PROGRESS_REGEX);
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
    if (statusElement) statusElement.textContent = text;
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

    // Try to trigger emergency save before showing error
    try {
      if (typeof Module._yazeEmergencySave === 'function') {
        Module._yazeEmergencySave();
        console.log('[WASM] Emergency save triggered on abort');
      } else if (typeof Module.ccall === 'function') {
        try {
          Module.ccall('yazeEmergencySave', null, [], []);
          console.log('[WASM] Emergency save triggered via ccall');
        } catch (saveErr) {
          console.warn('[WASM] ccall emergency save failed:', saveErr);
        }
      }
    } catch (e) {
      console.warn('[WASM] Emergency save failed:', e);
    }

    // Also try to sync IDBFS
    try {
      if (typeof FS !== 'undefined' && FS.syncfs) {
        FS.syncfs(false, function(err) {
          if (err) console.warn('[WASM] IDBFS sync on abort failed:', err);
          else console.log('[WASM] IDBFS synced on abort');
        });
      }
    } catch (e) {
      console.warn('[WASM] IDBFS sync failed:', e);
    }

    Module.setStatus('Fatal error: ' + what);
    showCrashRecoveryDialog(what || 'Unknown error');
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

      // Check if we're recovering from a crash
      checkCrashRecovery();

      console.log('[WASM] Initialization complete');
    } catch (err) {
      console.error('[WASM] Initialization error:', err);
      showFatalError('Initialization Failed', err.message);
    }
  }
};

Module.setStatus('Downloading...');

window.onerror = function(message, source, lineno, colno, error) {
  console.error('[WASM] Uncaught error:', message, 'at', source, ':', lineno, ':', colno);

  // Check if this is a WASM abort/fatal error
  var errorStr = String(message || '');
  var isFatal = errorStr.indexOf('RuntimeError') !== -1 ||
                errorStr.indexOf('Aborted') !== -1 ||
                errorStr.indexOf('abort') !== -1 ||
                errorStr.indexOf('unreachable') !== -1;

  if (isFatal) {
    // Try emergency save
    try {
      if (typeof Module !== 'undefined' && typeof Module._yazeEmergencySave === 'function') {
        Module._yazeEmergencySave();
      }
    } catch (e) {}

    // Show crash dialog for fatal errors
    var fullError = errorStr;
    if (source) fullError += '\nSource: ' + source + ':' + lineno + ':' + colno;
    if (error && error.stack) fullError += '\nStack:\n' + error.stack;
    showCrashRecoveryDialog(fullError);
  }

  Module.setStatus('Exception thrown, see JavaScript console');
  if (spinnerElement) spinnerElement.style.display = 'none';
  Module.setStatus = function(text) {
    if (text) Module.printErr('[post-exception status] ' + text);
  };
};

// Also handle unhandled promise rejections
window.addEventListener('unhandledrejection', function(event) {
  console.error('[WASM] Unhandled promise rejection:', event.reason);

  var reasonStr = String(event.reason || '');
  if (reasonStr.indexOf('RuntimeError') !== -1 || reasonStr.indexOf('Aborted') !== -1) {
    try {
      if (typeof Module !== 'undefined' && typeof Module._yazeEmergencySave === 'function') {
        Module._yazeEmergencySave();
      }
    } catch (e) {}
    showCrashRecoveryDialog('Unhandled Promise Rejection: ' + reasonStr);
  }
});

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
  if (typeof Module !== 'undefined' && window.YAZE_MODULE_READY) {
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
    // Clear existing content safely
    statusEl.textContent = '';

    // Create elements programmatically to avoid XSS
    const titleDiv = document.createElement('div');
    titleDiv.style.cssText = 'color: #f44; font-weight: bold;';
    titleDiv.textContent = title;

    const messageDiv = document.createElement('div');
    messageDiv.style.marginTop = '8px';
    messageDiv.textContent = message;

    const reloadBtn = document.createElement('button');
    reloadBtn.style.cssText = 'margin-top: 16px; padding: 8px 16px; cursor: pointer; background: #333; color: #fff; border: 1px solid #555; border-radius: 4px;';
    reloadBtn.textContent = 'Reload Page';
    reloadBtn.addEventListener('click', function() { location.reload(); });

    statusEl.appendChild(titleDiv);
    statusEl.appendChild(messageDiv);
    statusEl.appendChild(reloadBtn);
  }

  if (loadingOv) {
    loadingOv.style.display = 'flex';
  }
}

// Crash Recovery Dialog - shown when WASM aborts
function showCrashRecoveryDialog(errorMessage) {
  // Prevent multiple dialogs
  if (document.getElementById('yaze-crash-dialog')) return;

  // Create overlay
  var overlay = document.createElement('div');
  overlay.id = 'yaze-crash-dialog';
  overlay.style.cssText = 'position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.85);display:flex;align-items:center;justify-content:center;z-index:100000;font-family:-apple-system,BlinkMacSystemFont,sans-serif;';

  // Create dialog
  var dialog = document.createElement('div');
  dialog.style.cssText = 'background:#1e1e1e;border-radius:12px;padding:32px;max-width:500px;width:90%;box-shadow:0 8px 32px rgba(0,0,0,0.5);color:#fff;';

  // Title
  var title = document.createElement('h2');
  title.style.cssText = 'margin:0 0 16px 0;color:#f44;font-size:24px;display:flex;align-items:center;gap:12px;';
  title.innerHTML = '<span style="font-size:32px;">⚠️</span> Editor Crashed';
  dialog.appendChild(title);

  // Message
  var msg = document.createElement('p');
  msg.style.cssText = 'margin:0 0 16px 0;color:#ccc;line-height:1.5;';
  msg.textContent = 'The editor encountered a fatal error and had to stop. Your work may have been auto-saved.';
  dialog.appendChild(msg);

  // Error details (collapsible)
  var details = document.createElement('details');
  details.style.cssText = 'margin:0 0 24px 0;background:#2a2a2a;border-radius:8px;padding:12px;';
  var summary = document.createElement('summary');
  summary.style.cssText = 'cursor:pointer;color:#888;font-size:14px;';
  summary.textContent = 'Technical Details';
  details.appendChild(summary);
  var errorText = document.createElement('pre');
  errorText.style.cssText = 'margin:12px 0 0 0;padding:12px;background:#1a1a1a;border-radius:4px;overflow:auto;max-height:150px;font-size:12px;color:#f88;white-space:pre-wrap;word-break:break-all;';
  errorText.textContent = errorMessage;
  details.appendChild(errorText);
  dialog.appendChild(details);

  // Recovery info
  var recoveryInfo = document.createElement('p');
  recoveryInfo.style.cssText = 'margin:0 0 24px 0;color:#8c8;font-size:14px;background:#1a2a1a;padding:12px;border-radius:8px;';
  recoveryInfo.innerHTML = '💾 <strong>Recovery:</strong> When you reload, the editor will check for auto-saved data and offer to restore your session.';
  dialog.appendChild(recoveryInfo);

  // Buttons
  var btnContainer = document.createElement('div');
  btnContainer.style.cssText = 'display:flex;gap:12px;justify-content:flex-end;';

  var downloadBtn = document.createElement('button');
  downloadBtn.style.cssText = 'padding:12px 20px;border:1px solid #555;background:#333;color:#fff;border-radius:6px;cursor:pointer;font-size:14px;';
  downloadBtn.textContent = '📥 Download Error Log';
  downloadBtn.onclick = function() {
    var log = 'YAZE Crash Report\n';
    log += '==================\n';
    log += 'Time: ' + new Date().toISOString() + '\n';
    log += 'URL: ' + window.location.href + '\n';
    log += 'User Agent: ' + navigator.userAgent + '\n\n';
    log += 'Error:\n' + errorMessage + '\n\n';
    log += 'Console Log (recent):\n';
    // Try to get any stored console logs (circular buffer)
    if (window._yazeConsoleLogs) {
      // Read from circular buffer in chronological order
      var logs = [];
      var startIdx = window._yazeLogIndex;
      for (var i = 0; i < window._yazeConsoleLogs.length; i++) {
        var idx = (startIdx + i) % window._yazeConsoleLogs.length;
        if (window._yazeConsoleLogs[idx]) {
          logs.push(window._yazeConsoleLogs[idx]);
        }
      }
      log += logs.join('\n');
    }
    var blob = new Blob([log], { type: 'text/plain' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = 'yaze-crash-' + Date.now() + '.txt';
    a.click();
    URL.revokeObjectURL(url);
  };
  btnContainer.appendChild(downloadBtn);

  var reloadBtn = document.createElement('button');
  reloadBtn.style.cssText = 'padding:12px 24px;border:none;background:#4a9eff;color:#fff;border-radius:6px;cursor:pointer;font-size:14px;font-weight:600;';
  reloadBtn.textContent = '🔄 Reload Editor';
  reloadBtn.onclick = function() {
    // Set a flag so we know we're recovering from a crash
    try {
      sessionStorage.setItem('yaze_crash_recovery', 'true');
    } catch (e) {}
    location.reload();
  };
  btnContainer.appendChild(reloadBtn);

  dialog.appendChild(btnContainer);
  overlay.appendChild(dialog);
  document.body.appendChild(overlay);

  // Stop propagation of events to prevent further errors
  overlay.addEventListener('click', function(e) { e.stopPropagation(); }, true);
  overlay.addEventListener('keydown', function(e) { e.stopPropagation(); }, true);
}

// Check if we're recovering from a previous crash
function checkCrashRecovery() {
  try {
    var wasCrash = sessionStorage.getItem('yaze_crash_recovery') === 'true';
    var hasRecoveryData = sessionStorage.getItem('yaze_has_recovery') === 'true';
    var hasEmergencySave = localStorage.getItem('yaze_emergency_save') !== null;

    // Clear the crash recovery flag
    sessionStorage.removeItem('yaze_crash_recovery');

    if (wasCrash && (hasRecoveryData || hasEmergencySave)) {
      console.log('[Recovery] Detected crash recovery with available data');
      showRecoveryPromptDialog();
    } else if (hasRecoveryData || hasEmergencySave) {
      // We have recovery data but didn't just crash - might be from a previous session
      console.log('[Recovery] Found recovery data from previous session');
      // Optionally show a subtle notification instead of a full dialog
      if (typeof window.showYazeToast === 'function') {
        window.showYazeToast('Recovery data available. Check File > Restore Session.', 'info', 5000);
      }
    }
  } catch (e) {
    console.warn('[Recovery] Error checking crash recovery:', e);
  }
}

// Show the recovery prompt dialog after a crash
function showRecoveryPromptDialog() {
  var overlay = document.createElement('div');
  overlay.id = 'yaze-recovery-dialog';
  overlay.style.cssText = 'position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.7);display:flex;align-items:center;justify-content:center;z-index:99999;font-family:-apple-system,BlinkMacSystemFont,sans-serif;';

  var dialog = document.createElement('div');
  dialog.style.cssText = 'background:#1e1e1e;border-radius:12px;padding:32px;max-width:450px;width:90%;box-shadow:0 8px 32px rgba(0,0,0,0.5);color:#fff;';

  var title = document.createElement('h2');
  title.style.cssText = 'margin:0 0 16px 0;color:#4a9eff;font-size:22px;display:flex;align-items:center;gap:12px;';
  title.innerHTML = '<span style="font-size:28px;">💾</span> Session Recovery';
  dialog.appendChild(title);

  var msg = document.createElement('p');
  msg.style.cssText = 'margin:0 0 24px 0;color:#ccc;line-height:1.6;';
  msg.textContent = 'The editor was closed unexpectedly. Would you like to restore your previous session?';
  dialog.appendChild(msg);

  var btnContainer = document.createElement('div');
  btnContainer.style.cssText = 'display:flex;gap:12px;justify-content:flex-end;';

  var discardBtn = document.createElement('button');
  discardBtn.style.cssText = 'padding:12px 20px;border:1px solid #555;background:#333;color:#fff;border-radius:6px;cursor:pointer;font-size:14px;';
  discardBtn.textContent = 'Start Fresh';
  discardBtn.onclick = function() {
    // Clear recovery data
    try {
      sessionStorage.removeItem('yaze_has_recovery');
      localStorage.removeItem('yaze_emergency_save');
    } catch (e) {}
    overlay.remove();
    console.log('[Recovery] User chose to start fresh');
  };
  btnContainer.appendChild(discardBtn);

  var restoreBtn = document.createElement('button');
  restoreBtn.style.cssText = 'padding:12px 24px;border:none;background:#4a9eff;color:#fff;border-radius:6px;cursor:pointer;font-size:14px;font-weight:600;';
  restoreBtn.textContent = 'Restore Session';
  restoreBtn.onclick = function() {
    overlay.remove();
    console.log('[Recovery] User chose to restore session');

    // Try to call the C++ recovery function
    try {
      if (typeof Module !== 'undefined' && typeof Module.ccall === 'function') {
        Module.ccall('yazeRecoverSession', null, [], []);
      }
    } catch (e) {
      console.warn('[Recovery] C++ recovery failed, trying JS fallback:', e);
      // Fallback: just notify user that recovery data is available
      if (typeof window.showYazeToast === 'function') {
        window.showYazeToast('Recovery initiated. Reload your ROM to restore changes.', 'success', 5000);
      }
    }
  };
  btnContainer.appendChild(restoreBtn);

  dialog.appendChild(btnContainer);
  overlay.appendChild(dialog);
  document.body.appendChild(overlay);
}

// Console log capture for crash reports
(function captureLogs() {
  window._yazeConsoleLogs = new Array(100);
  window._yazeLogIndex = 0;
  var maxLogs = 100;

  var originalLog = console.log;
  var originalWarn = console.warn;
  var originalError = console.error;

  // Use circular buffer - overwrite oldest entries instead of shift()
  function addLog(prefix, msg) {
    window._yazeConsoleLogs[window._yazeLogIndex] = prefix + msg;
    window._yazeLogIndex = (window._yazeLogIndex + 1) % maxLogs;
  }

  console.log = function() {
    var msg = Array.prototype.slice.call(arguments).join(' ');
    addLog('[LOG] ', msg);
    return originalLog.apply(console, arguments);
  };
  console.warn = function() {
    var msg = Array.prototype.slice.call(arguments).join(' ');
    addLog('[WARN] ', msg);
    return originalWarn.apply(console, arguments);
  };
  console.error = function() {
    var msg = Array.prototype.slice.call(arguments).join(' ');
    addLog('[ERROR] ', msg);
    return originalError.apply(console, arguments);
  };
})();

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
  // Prioritize the new collaboration UI if available
  if (typeof window.toggleCollaborationPanel === 'function') {
    window.toggleCollaborationPanel();
    return;
  }

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
  window.YAZE_MODULE_READY = false;
  var initTimeout = null;
  var maxRetries = 100;  // 10 seconds max (100ms * 100)
  var retryCount = 0;

  function tryInit() {
    if (typeof createYazeModule === 'function') {
      console.log('[WASM] Initializing module...');
      // Clear any pending retries
      if (initTimeout !== null) {
        clearTimeout(initTimeout);
        initTimeout = null;
      }
      createYazeModule(Module).then(function(instance) {
        console.log('[WASM] Module initialized successfully');
        window.Module = instance;
        window.YAZE_MODULE_READY = true;
      }).catch(function(err) {
        console.error('[WASM] Module initialization failed:', err);
        showFatalError('WASM Initialization Failed', err.message || err.toString());
      });
    } else {
      // yaze.js not loaded yet, try again in 100ms
      retryCount++;
      if (retryCount < maxRetries) {
        initTimeout = setTimeout(tryInit, 100);
      } else {
        console.error('[WASM] createYazeModule function not found after ' + (maxRetries * 100) + 'ms');
        showFatalError('WASM Initialization Failed', 'Module loading timed out');
      }
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

// ============================================================================
// Namespace Integration
// Populate window.yaze with exports from this module
// ============================================================================
(function integrateNamespace() {
  'use strict';

  // Wait for yaze namespace to be available
  if (!window.yaze) {
    console.warn('[app.js] yaze namespace not found, skipping integration');
    return;
  }

  // Core state
  window.yaze.core.state.wasmReady = wasmReady;
  window.yaze.core.state.fsReady = fsReady;

  // Module will be assigned after WASM init, set up getter
  Object.defineProperty(window.yaze.core, 'Module', {
    get: function() { return window.Module; },
    configurable: true
  });

  Object.defineProperty(window.yaze.core, 'FS', {
    get: function() { return window.FS || (window.Module && window.Module.FS); },
    configurable: true
  });

  // Utility functions
  window.yaze.utils.resizeCanvas = window.resizeCanvasToContainer;
  window.yaze.utils.toggleFullscreen = window.toggleFullscreen;
  window.yaze.utils.zoom = {
    in: window.zoomIn,
    out: window.zoomOut,
    reset: window.resetZoom,
    get current() { return currentZoom; }
  };
  window.yaze.utils.debounce = debounce;
  window.yaze.utils.throttle = throttle;

  // Service worker manager
  window.yaze.utils.serviceWorker = {
    update: window.updateServiceWorker,
    dismiss: window.dismissUpdate,
    manager: ServiceWorkerManager
  };

  // Debug API
  window.yaze.debug = window.yazeDebug;

  // Terminal UI helpers
  window.yaze.ui.terminalHelpers = {
    toggle: window.toggleTerminal,
    hide: window.hideTerminal,
    maximize: window.maximizeTerminal,
    submit: submitTerminalCommand
  };

  // Emit ready event
  if (window.yaze.events && window.yaze.events.emit) {
    // Defer to ensure all modules have loaded
    setTimeout(function() {
      if (wasmReady) {
        window.yaze.events.emit(window.yaze.events.WASM_READY, { Module: window.Module });
      }
    }, 0);
  }

  console.log('[app.js] Integrated with yaze namespace');
})();
