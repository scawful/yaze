/**
 * Main application logic for YAZE Web
 * Handles Emscripten Module integration, UI binding, and PWA features.
 */

var statusElement = document.getElementById('status');
var progressElement = document.getElementById('progress');
var spinnerElement = document.getElementById('spinner');
var loadingOverlay = document.getElementById('loading-overlay');

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
  onRuntimeInitialized: function() {
     console.log("YAZE Web initialized");
     if (loadingOverlay) loadingOverlay.style.display = 'none';

     // Resize canvas to fill container
     resizeCanvasToContainer();

     if (typeof Z3edTerminal !== 'undefined') {
       window.z3edTerminal = new Z3edTerminal('terminal-container');
       window.z3edTerminal.printInfo('z3ed Web Terminal ready. Type /help for commands.');
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

// ROM Upload Handling
var romInput = document.getElementById('rom-input');
if (romInput) {
  romInput.addEventListener('change', function(e) {
    var file = e.target.files[0];
    if (!file) return;
    
    var reader = new FileReader();
    reader.onload = function(e) {
      var data = new Uint8Array(e.target.result);
      var filename = '/roms/' + file.name;
      
      try {
        try { FS.mkdir('/roms'); } catch(e) {}
        FS.writeFile(filename, data);
        console.log("Wrote " + data.length + " bytes to " + filename);
        
        // Check if ccall is available, fallback to direct function call
        if (Module.ccall) {
          Module.ccall('LoadRomFromWeb', 'null', ['string'], [filename]);
        } else if (Module._LoadRomFromWeb) {
          Module._LoadRomFromWeb(filename);
        } else {
          console.error("LoadRomFromWeb function not available");
          alert("ROM loading not ready yet. Please wait for the app to initialize.");
        }
      } catch (err) {
        console.error("Error writing file:", err);
        alert("Failed to load ROM: " + err);
      }
    };
    reader.readAsArrayBuffer(file);
  });
}

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
  
  // Set canvas size to match container exactly
  canvas.width = width;
  canvas.height = height;
  canvas.style.width = width + 'px';
  canvas.style.height = height + 'px';
  
  // Notify Emscripten/GLFW of the resize if available
  if (Module) {
    // Try Emscripten's canvas resize API
    if (typeof Module._emscripten_set_canvas_element_size === 'function') {
      Module._emscripten_set_canvas_element_size('#canvas', width, height);
    }
    // Also try GLFW window resize if available
    if (typeof Module._glfwSetWindowSize === 'function' && Module.canvas) {
      Module._glfwSetWindowSize(Module.canvas, width, height);
    }
  }
  
  console.log('Canvas resized to:', width, 'x', height);
}

// Resize canvas on window resize
window.addEventListener('resize', function() {
  resizeCanvasToContainer();
});

// Initial resize after a short delay to ensure layout is complete
setTimeout(resizeCanvasToContainer, 100);

// Save Download Handling
window.downloadSaves = function() {
  if (typeof FS === 'undefined') return;
  
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

// Service Worker & PWA Logic
let newServiceWorker;
let swUpdatePending = false;

if ('serviceWorker' in navigator) {
  window.addEventListener('load', () => {
    // Wait for COI service worker to finish setup before registering PWA service worker
    // The COI SW needs to be controlling the page first for SharedArrayBuffer support
    const coiReloading = window.sessionStorage.getItem("coiReloadedBySelf") ||
                         window.sessionStorage.getItem("coiAttempted");

    if (coiReloading) {
      console.log('[PWA] Waiting for COI service worker to complete setup...');
      return; // Don't register PWA SW during COI setup
    }

    // Small delay to ensure COI SW has fully activated
    setTimeout(() => {
      navigator.serviceWorker.register('service-worker.js')
        .then((registration) => {
          console.log('ServiceWorker registration successful');
          setInterval(() => { registration.update(); }, 3600000);
          registration.addEventListener('updatefound', () => {
            const installingWorker = registration.installing;
            installingWorker.addEventListener('statechange', () => {
              if (installingWorker.state === 'installed' && navigator.serviceWorker.controller) {
                newServiceWorker = installingWorker;
                swUpdatePending = true;
                var notif = document.getElementById('update-notification');
                if (notif) notif.classList.add('show');
              }
            });
          });
        })
        .catch(err => console.error('ServiceWorker failed', err));
    }, 100);

    // Guard controllerchange to prevent reload loops
    // Only reload if the user explicitly requested an update
    navigator.serviceWorker.addEventListener('controllerchange', () => {
      if (swUpdatePending) {
        swUpdatePending = false;
        window.location.reload();
      } else {
        console.log('[PWA] Controller changed, but no update pending - skipping reload');
      }
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
  if (newServiceWorker) {
    swUpdatePending = true; // Allow controllerchange to trigger reload
    newServiceWorker.postMessage({ type: 'SKIP_WAITING' });
    var notif = document.getElementById('update-notification');
    if (notif) notif.classList.remove('show');
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