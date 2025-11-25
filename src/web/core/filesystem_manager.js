/**
 * Filesystem Manager for YAZE Web
 * Handles persistent filesystem initialization, ROM loading, and save management.
 */

var FilesystemManager = {
  ready: false,
  initPromise: null,
  initAttempts: 0,

  /**
   * Checks if the filesystem is ready and shows a status message if not.
   * @param {boolean} showAlert - Whether to show an alert or status message.
   * @returns {boolean} - True if ready, false otherwise.
   */
  ensureReady: function(showAlert = true) {
    // Try to get FS from Module if not globally available
    var fs = (typeof FS !== 'undefined') ? FS :
             (typeof Module !== 'undefined' && Module.FS) ? Module.FS :
             (typeof window.Module !== 'undefined' && window.Module.FS) ? window.Module.FS : null;

    if (fs && typeof window.FS === 'undefined') {
      window.FS = fs; // Expose globally for convenience
    }

    if (this.ready && fs) return true;

    // Check if /roms exists (fast path)
    try {
      if (fs && fs.stat('/roms')) {
        this.ready = true;
        return true;
      }
    } catch(e) {}

    if (this.initPromise) {
      console.warn('FS not ready yet; still initializing.');
      if (showAlert) {
        // Show a non-blocking status message instead of silent failure
        var status = document.getElementById('header-status');
        if (status) {
          status.textContent = 'File system initializing... please wait';
          status.style.color = '#ffaa00';
          setTimeout(() => {
            if (this.ready) {
              status.textContent = 'Ready';
              status.style.color = '';
            }
          }, 3000);
        }
      }
      return false;
    }
    if (showAlert) {
      alert('WASM not ready yet. Please wait for the app to finish loading, then retry.');
    }
    return false;
  },

  /**
   * Initializes the persistent filesystem.
   * @returns {Promise} - Resolves when FS is ready.
   */
  initPersistentFS: function() {
    if (this.initPromise) return this.initPromise;
    
    this.initPromise = new Promise((resolve, reject) => {
      waitForModule(() => {
        // If C++ already signaled ready, we are done
        if (this.ready) {
          resolve();
          return;
        }

        // Try to get FS from Module (MODULARIZE mode)
        var fs = (typeof FS !== 'undefined') ? FS :
                 (typeof Module !== 'undefined' && Module.FS) ? Module.FS :
                 (typeof window.Module !== 'undefined' && window.Module.FS) ? window.Module.FS : null;

        if (fs && typeof window.FS === 'undefined') {
          console.log('[FilesystemManager] Exposing Module.FS globally');
          window.FS = fs;
        }

        if (!fs) {
          this.initAttempts++;
          if (this.initAttempts < 50) { // Increased retries significantly
            console.warn('FS unavailable; retrying...', this.initAttempts);
            this.initPromise = null;
            setTimeout(() => this.initPersistentFS().then(resolve).catch(reject), 100);
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
          this.ready = true;
          resolve();
          return;
        }
        
        console.log('[WASM] Waiting for C++ FS initialization...');
        
        // Poll for ready or /roms existence
        var checkInterval = setInterval(() => {
          if (this.ready) {
            clearInterval(checkInterval);
            resolve();
            return;
          }
          
          try {
            FS.stat('/roms');
            console.log('[WASM] Detected /roms, marking FS ready');
            this.ready = true;
            clearInterval(checkInterval);
            resolve();
          } catch(e) {
            // Still waiting
          }
        }, 200);
        
        // Timeout after 15 seconds
        setTimeout(() => {
          if (!this.ready) {
            clearInterval(checkInterval);
            console.warn('[WASM] FS init timed out, forcing ready (might be MEMFS)');
            // Try to create /roms if it doesn't exist
            try { FS.mkdir('/roms'); } catch(e) {}
            this.ready = true; // Fallback
            resolve();
          }
        }, 15000);
      });
    });
    return this.initPromise;
  },

  /**
   * C++ calls this when it has finished mounting filesystems.
   */
  onFileSystemReady: function() {
    console.log('[WASM] C++ signaled FileSystem Ready');
    this.ready = true;
    
    // Update UI if needed
    var status = document.getElementById('header-status');
    if (status) {
      status.textContent = 'Ready';
      status.style.color = '';
    }
    
    // Refresh ROM list if menu is open
    if (typeof updateRomFilesList === 'function') {
      updateRomFilesList();
    }
  },

  /**
   * Handles ROM file upload from input element.
   * @param {File} file - The file object.
   */
  handleRomUpload: function(file) {
    if (!file) return;
    if (!this.ensureReady()) return;
    
    var reader = new FileReader();
    reader.onload = (e) => {
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
            // Use null (not 'null' string) for void return type
            Module.ccall('LoadRomFromWeb', null, ['string'], [filename]);
            console.log("LoadRomFromWeb called via ccall");
          } else if (Module._LoadRomFromWeb) {
            // Properly allocate and convert string for direct function call
            var len = Module.lengthBytesUTF8(filename) + 1;
            var filenamePtr = Module._malloc(len);
            if (!filenamePtr) {
              throw new Error("Failed to allocate memory for filename");
            }
            Module.stringToUTF8(filename, filenamePtr, len);
            Module._LoadRomFromWeb(filenamePtr);
            Module._free(filenamePtr);
            console.log("LoadRomFromWeb called via direct function");
          } else {
            console.error("LoadRomFromWeb function not available");
            alert("ROM loading not ready yet. Please wait for the app to initialize.");
            return;
          }

          // Persist to IndexedDB
          if (this.ready) {
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
  },

  /**
   * Downloads save files from the virtual filesystem.
   */
  downloadSaves: function() {
    if (!this.ensureReady()) return;
    
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
  },

  /**
   * Flushes saves to persistent storage.
   */
  flushSaves: function() {
    if (!this.ensureReady()) return;
    FS.syncfs(false, function(err) {
      if (err) {
        console.warn('Failed to sync saves:', err);
      } else {
        console.log('Saves synced to persistent storage.');
      }
    });
  }
};

// Global helper for waitForModule if not already defined
function waitForModule(cb) {
  if (typeof Module !== 'undefined') {
    cb();
  } else {
    setTimeout(() => waitForModule(cb), 30);
  }
}

// Bind to Module.onFileSystemReady when Module is available
waitForModule(() => {
  if (typeof Module !== 'undefined') {
    // Ensure FS is globally available (MODULARIZE mode puts it on Module.FS)
    var exposeFS = function() {
      if (typeof FS === 'undefined') {
        if (Module.FS) {
          console.log('[FilesystemManager] Aliasing Module.FS to window.FS');
          window.FS = Module.FS;
        } else if (window.Module && window.Module.FS) {
          console.log('[FilesystemManager] Aliasing window.Module.FS to window.FS');
          window.FS = window.Module.FS;
        }
      }
    };

    // Try immediately and also after a delay (module init may not be complete)
    exposeFS();
    setTimeout(exposeFS, 100);
    setTimeout(exposeFS, 500);

    // Hook into existing onFileSystemReady if it exists
    var original = Module.onFileSystemReady;
    Module.onFileSystemReady = function() {
      exposeFS(); // Ensure FS is available when C++ signals ready
      if (original) original();
      FilesystemManager.onFileSystemReady();
    };
  }
});

// Expose globally
window.FilesystemManager = FilesystemManager;
window.ensureFSReady = FilesystemManager.ensureReady.bind(FilesystemManager);
window.downloadSaves = FilesystemManager.downloadSaves.bind(FilesystemManager);
window.flushSaves = FilesystemManager.flushSaves.bind(FilesystemManager);
