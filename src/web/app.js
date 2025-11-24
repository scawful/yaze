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

      if (loadingOverlay) loadingOverlay.style.display = 'none';

      // Hide welcome screen and show canvas
      if (typeof hideWelcomeScreen === 'function') hideWelcomeScreen();

      // Resize canvas to fill container
      resizeCanvasToContainer();

      // Verify critical functions are available
      if (typeof Module._LoadRomFromWeb === 'undefined' && !Module.ccall) {
        throw new Error('Critical WASM function missing: LoadRomFromWeb');
      }

      // Initialize terminal if available
      if (typeof Z3edTerminal !== 'undefined') {
        window.z3edTerminal = new Z3edTerminal('panel-terminal');
        window.z3edTerminal.printInfo('z3ed Web Terminal ready. Type /help for commands.');
      }

      wasmReady = true;
      initPersistentFS().catch((err) => {
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

function ensureFSReady(showAlert = true) {
  if (fsReady && typeof FS !== 'undefined') return true;
  if (fsInitPromise) {
    console.warn('FS not ready yet; still initializing.');
    if (showAlert) {
      // Show a non-blocking status message instead of silent failure
      var status = document.getElementById('header-status');
      if (status) {
        status.textContent = 'File system initializing... please wait';
        status.style.color = '#ffaa00';
        setTimeout(function() {
          status.textContent = 'Ready';
          status.style.color = '';
        }, 3000);
      }
    }
    return false;
  }
  if (showAlert) {
    alert('WASM not ready yet. Please wait for the app to finish loading, then retry.');
  }
  return false;
}

function initPersistentFS() {
  if (fsInitPromise) return fsInitPromise;
  fsInitPromise = new Promise((resolve, reject) => {
    waitForModule(() => {
      if (typeof FS === 'undefined') {
        fsInitAttempts++;
        if (fsInitAttempts < 5) {
          console.warn('FS unavailable; retrying...', fsInitAttempts);
          fsInitPromise = null;
          setTimeout(initPersistentFS, 100);
          return;
        }
        reject(new Error('FS unavailable during init'));
        return;
      }

      // Check if /roms already exists (C++ may have already set up the FS)
      var romsExists = false;
      try {
        FS.stat('/roms');
        romsExists = true;
      } catch (e) {
        // Directory doesn't exist
      }

      if (romsExists) {
        // C++ already mounted IDBFS, just mark as ready
        console.log('[WASM] FS already initialized by C++ runtime');
        fsReady = true;
        resolve();
        return;
      }

      // IDBFS not available yet - might need to wait
      if (typeof IDBFS === 'undefined') {
        fsInitAttempts++;
        if (fsInitAttempts < 5) {
          console.warn('IDBFS unavailable; retrying...', fsInitAttempts);
          fsInitPromise = null;
          setTimeout(initPersistentFS, 100);
          return;
        }
        // Fall back to in-memory FS
        console.warn('[WASM] IDBFS unavailable, using in-memory FS');
        fsReady = true;
        resolve();
        return;
      }

      try {
        try { FS.mkdir('/roms'); } catch (e) {}
        try { FS.mkdir('/saves'); } catch (e) {}
        try { FS.mount(IDBFS, {}, '/roms'); } catch (e) { console.warn('Mount /roms failed (maybe already mounted):', e); }
        try { FS.mount(IDBFS, {}, '/saves'); } catch (e) { console.warn('Mount /saves failed (maybe already mounted):', e); }
      } catch (err) {
        console.error('FS init error:', err);
        reject(err);
        return;
      }

      FS.syncfs(true, function(err) {
        if (err) {
          console.error('FS sync (pull) failed:', err);
          reject(err);
          return;
        }
        fsReady = true;
        console.log('[WASM] FS ready (IDBFS mounted)');
        resolve();
      });
    });
  });
  return fsInitPromise;
}

// ROM Upload Handling
var romInput = document.getElementById('rom-input');
if (romInput) {
  romInput.addEventListener('change', function(e) {
    var file = e.target.files[0];
    if (!file) return;
    if (!ensureFSReady()) return;
    
    var reader = new FileReader();
    reader.onload = function(e) {
      var data = new Uint8Array(e.target.result);
      var filename = '/roms/' + file.name;
      
      try {
        try { FS.mkdir('/roms'); } catch(e) {}
        FS.writeFile(filename, data);
        console.log("Wrote " + data.length + " bytes to " + filename);
        
        // Verify file was written correctly
        try {
          var stats = FS.stat(filename);
          if (!stats) {
            throw new Error("File write verification failed - stats is null");
          }
          if (stats.size !== data.length) {
            throw new Error("File size mismatch: expected " + data.length + " bytes, got " + stats.size);
          }
          console.log("File verified: " + filename + " (" + stats.size + " bytes)");
        } catch (verifyErr) {
          console.error("File verification failed:", verifyErr);
          alert("Failed to verify ROM file: " + verifyErr);
          return;
        }
        
        // Check if ccall is available, fallback to direct function call
        try {
          if (Module.ccall) {
            var result = Module.ccall('LoadRomFromWeb', 'null', ['string'], [filename]);
            console.log("LoadRomFromWeb called, result:", result);
          } else if (Module._LoadRomFromWeb) {
            // Use UTF8ToString to convert filename if needed
            var filenamePtr = Module.stringToUTF8 ? Module.stringToUTF8(filename) : filename;
            Module._LoadRomFromWeb(filenamePtr);
            console.log("LoadRomFromWeb called via direct function");
          } else {
            console.error("LoadRomFromWeb function not available");
            alert("ROM loading not ready yet. Please wait for the app to initialize.");
            return;
          }

          // Persist to IndexedDB
          if (fsReady) {
            FS.syncfs(false, function(err) {
              if (err) console.warn('FS sync (push) failed after ROM load:', err);
            });
          }
        } catch (wasmErr) {
          console.error("WASM error loading ROM:", wasmErr);
          var errorMsg = "Failed to load ROM: ";
          if (wasmErr.message) {
            errorMsg += wasmErr.message;
          } else if (wasmErr.toString) {
            errorMsg += wasmErr.toString();
          } else {
            errorMsg += "Memory access error (ROM may be corrupted or invalid)";
          }
          alert(errorMsg);
          // Try to get more details from the error
          if (wasmErr.stack) {
            console.error("Stack trace:", wasmErr.stack);
          }
        }
      } catch (err) {
        console.error("Error writing file:", err);
        var errorMsg = "Failed to load ROM: ";
        if (err.message) {
          errorMsg += err.message;
        } else {
          errorMsg += err.toString();
        }
        alert(errorMsg);
      }
    };
    reader.readAsArrayBuffer(file);
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
  setTimeout(resizeCanvasToContainer, 100);
});

// Canvas resize function to fill container
function resizeCanvasToContainer() {
  var canvas = document.getElementById('canvas');
  var container = document.getElementById('canvas-container');
  if (!canvas || !container) return;
  
  // Get container dimensions (already accounts for header and status bar via flexbox)
  var containerRect = container.getBoundingClientRect();
  var width = Math.floor(containerRect.width);
  var height = Math.floor(containerRect.height);
  
  if (width <= 0 || height <= 0) return; // Skip if container not visible
  
  // Check if resize is actually needed
  // In fullscreen, force update to ensure GL context matches window
  if (!document.fullscreenElement && canvas.width === width && canvas.height === height) return;

  // Set canvas size to match container exactly
  canvas.width = width;
  canvas.height = height;
  canvas.style.width = width + 'px';
  canvas.style.height = height + 'px';
  
  // Notify Emscripten of the resize
  if (Module) {
    // Standard Emscripten resize hook
    if (typeof Module.setCanvasSize === 'function') {
      Module.setCanvasSize(width, height);
    }
    // Internal helper if available
    if (typeof Module._emscripten_set_canvas_element_size === 'function') {
      Module._emscripten_set_canvas_element_size('#canvas', width, height);
    }
  }
  
  // console.log('Canvas resized to:', width, 'x', height);
}

// Use throttled resize for real-time feedback and debounced resize for final adjustment
const throttledResize = throttle(resizeCanvasToContainer, 100);
const debouncedResize = debounce(resizeCanvasToContainer, 250);

// Resize canvas on window resize
window.addEventListener('resize', function() {
  throttledResize();  // Immediate feedback
  debouncedResize();  // Final adjustment
});

// UI Helpers
function hideWelcomeScreen() {
  var welcome = document.getElementById('welcome-screen');
  if (welcome) welcome.style.display = 'none';
  var canvas = document.getElementById('canvas');
  if (canvas) canvas.style.display = 'block';
}

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
setTimeout(resizeCanvasToContainer, 100);

// Save Download Handling
window.downloadSaves = function() {
  if (!ensureFSReady()) return;
  
  FS.syncfs(false, function(err) {
    if (err) {
      console.error("Sync failed:", err);
      alert("Failed to sync saves: " + err);
      return;
    }
    
    try {
      var files = FS.readdir('/saves');
      var found = false;
      
      files.forEach(function(file) {
        if (file === '.' || file === '..') return;
        if (file.endsWith('.srm') || file.endsWith('.sav')) {
          found = true;
          var content = FS.readFile('/saves/' + file);
          var blob = new Blob([content], {type: 'application/octet-stream'});
          var url = URL.createObjectURL(blob);
          
          var a = document.createElement('a');
          a.style.display = 'none';
          a.href = url;
          a.download = file;
          document.body.appendChild(a);
          a.click();
          window.setTimeout(function() {
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
          }, 1000);
        }
      });
      
      if (!found) {
        alert("No save files found in /saves");
      }
      
    } catch (e) {
      console.error("Error reading saves:", e);
      alert("Error listing saves: " + e);
    }
  });
};

// Persist saves (can be called from WASM via ccall if needed)
window.flushSaves = function() {
  if (!ensureFSReady()) return;
  FS.syncfs(false, function(err) {
    if (err) {
      console.warn('Failed to sync saves:', err);
    } else {
      console.log('Saves synced to persistent storage.');
    }
  });
};


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

    // Check for reload loop
    const now = Date.now();
    if (now - this.reloadTimestamp < 5000) { // 5 seconds
      this.reloadCount++;
      if (this.reloadCount >= this.maxReloads) {
        console.error('[PWA] Too many reloads, aborting update');
        alert('Service worker update failed. Please reload manually.');
        this.updatePending = false;
        return;
      }
    } else {
      this.reloadCount = 0;
    }

    this.reloadTimestamp = now;
    this.updatePending = false;

    console.log('[PWA] Reloading for service worker update');
    window.location.reload();
  },

  requestUpdate: function(newWorker) {
    this.updatePending = true;
    newWorker.postMessage({ type: 'SKIP_WAITING' });

    // Auto-reload after 2 seconds if user doesn't dismiss
    setTimeout(() => {
      if (this.updatePending) {
        console.log('[PWA] Auto-reloading for update');
        this.handleControllerChange();
      }
    }, 2000);
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
      const registration = await navigator.serviceWorker.register('service-worker.js');
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
  var input = document.getElementById('terminal-input');
  if (input) {
    input.addEventListener('keydown', function(e) {
      if (e.key === 'Enter') {
        e.preventDefault();
        submitTerminalCommand();
      }
    });
  }

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
