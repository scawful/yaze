/**
 * Filesystem Manager for YAZE Web
 * Handles persistent filesystem initialization, ROM loading, and save management.
 */

var FilesystemManager = {
  ready: false,
  initPromise: null,
  initAttempts: 0,

  // Standard directories used by the app
  directories: {
    roms: '/roms',
    saves: '/saves',
    config: '/config',
    projects: '/projects',
    prompts: '/prompts',
    recent: '/recent',
    temp: '/temp'
  },

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
    // Immediate synchronous check and assignment to prevent race
    if (this.initPromise) return this.initPromise;

    // Create promise immediately before any async operations
    var self = this;
    var resolveInit, rejectInit;
    this.initPromise = new Promise(function(resolve, reject) {
      resolveInit = resolve;
      rejectInit = reject;
    });

    // Now do async work using resolveInit/rejectInit
    waitForModule(function() {
      // If C++ already signaled ready, we are done
      if (self.ready) {
        resolveInit();
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
        self.initAttempts++;
        if (self.initAttempts < 50) { // Increased retries significantly
          console.warn('FS unavailable; retrying...', self.initAttempts);
          self.initPromise = null;
          setTimeout(function() {
            self.initPersistentFS().then(resolveInit).catch(rejectInit);
          }, 100);
          return;
        }
        rejectInit(new Error('FS unavailable during init'));
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
        self.ready = true;
        resolveInit();
        return;
      }

      console.log('[WASM] Waiting for C++ FS initialization...');

      // Poll for ready or /roms existence
      var checkInterval = setInterval(function() {
        if (self.ready) {
          clearInterval(checkInterval);
          checkInterval = null;
          resolveInit();
          return;
        }

        try {
          FS.stat('/roms');
          console.log('[WASM] Detected /roms, marking FS ready');
          self.ready = true;
          clearInterval(checkInterval);
          checkInterval = null;
          resolveInit();
        } catch(e) {
          // Still waiting
        }
      }, 200);

      // Timeout after 15 seconds
      setTimeout(function() {
        if (!self.ready) {
          if (checkInterval !== null) {
            clearInterval(checkInterval);
            checkInterval = null;
          }
          console.warn('[WASM] FS init timed out, forcing ready (might be MEMFS)');
          // Try to create /roms if it doesn't exist
          try { FS.mkdir('/roms'); } catch(e) {}
          self.ready = true; // Fallback
          resolveInit();
        }
      }, 15000);
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

    // Emit FS_READY event for components waiting on filesystem
    if (window.yaze && window.yaze.events && window.yaze.events.emit) {
      window.yaze.events.emit(window.yaze.events.FS_READY);
      console.log('[FilesystemManager] FS_READY event emitted');
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
    var self = this;
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

        // Small delay to yield to browser before blocking WASM call
        setTimeout(function() {
          self._executeRomLoad(filename, null);
        }, 10);

      } catch(err) {
        console.error("File system error:", err);
        alert("Failed to save ROM to filesystem: " + err);
      }
    };
    reader.onerror = (err) => {
      console.error("FileReader error:", err);
      alert("Failed to read ROM file");
    };
    reader.readAsArrayBuffer(file);
  },

  /**
   * Handles ROM data that's already been read.
   * @param {string} filename - The filename for the ROM
   * @param {Uint8Array} data - The ROM data
   */
  handleRomData: function(filename, data) {
    if (!this.ensureReady()) return;

    var self = this;
    var fullPath = '/roms/' + filename;

    try {
      try { FS.mkdir('/roms'); } catch(e) {}
      FS.writeFile(fullPath, data);
      console.log("Wrote " + data.length + " bytes to " + fullPath);

      // Verify file was written correctly
      try {
        var stats = FS.stat(fullPath);
        if (!stats || stats.size !== data.length) {
          throw new Error("File size mismatch");
        }
      } catch (verifyErr) {
        console.error("File verification failed:", verifyErr);
        alert("Failed to verify ROM file: " + verifyErr);
        return;
      }

      // Small delay to yield to browser before blocking WASM call
      setTimeout(function() {
        self._executeRomLoad(fullPath, null);
      }, 10);

    } catch(err) {
      console.error("File system error:", err);
      alert("Failed to save ROM to filesystem: " + err);
    }
  },

  /**
   * Internal: Execute the actual ROM load (called after UI yield)
   * Uses async flow to ensure filesystem is synced before C++ access.
   * @param {string} filename - Path to the ROM file
   * @returns {Promise<boolean>} - Resolves true on success, false on failure
   * @private
   */
  _executeRomLoad: async function(filename) {
    var self = this;
    
    try {
      // 1. Check if file exists before trying to load
      if (!this.fileExists(filename)) {
        console.warn('[FilesystemManager] ROM file not found:', filename);
        alert('ROM file not found: ' + filename.split('/').pop() +
              '\n\nThe file may have been deleted or browser storage was cleared. Please upload the ROM again.');
        return false;
      }

      // 2. Read file data for validation
      var data;
      try {
        data = FS.readFile(filename);
      } catch (readErr) {
        console.error('[FilesystemManager] Failed to read ROM file:', readErr);
        alert('Failed to read ROM file: ' + readErr.message);
        return false;
      }

      // 3. Validate ROM data before passing to C++
      var validation = this.validateRomData(data, filename);
      if (!validation.valid) {
        console.error('[FilesystemManager] ROM validation failed:', validation.error);
        alert('Invalid ROM file: ' + validation.error);
        
        // Report to crash reporter if available
        if (typeof window.yazeAddProblem === 'function') {
          window.yazeAddProblem(
            'ROM validation failed: ' + validation.error,
            'ROM',
            'error'
          );
        }
        return false;
      }

      console.log('[FilesystemManager] ROM validated successfully:', validation.info);

      // 4. Ensure filesystem is fully synced before C++ access
      // This prevents race conditions where C++ reads stale or incomplete data
      await new Promise(function(resolve) {
        if (typeof FS !== 'undefined' && FS.syncfs) {
          FS.syncfs(false, function(err) {
            if (err) {
              console.warn('[FilesystemManager] FS sync before load failed:', err);
            }
            resolve();
          });
        } else {
          resolve();
        }
      });

      // 5. Call C++ ROM loader (must use async: true to properly handle Asyncify operations)
      // The C++ code may internally call idb_load_string or other async functions during ROM loading
      if (Module.ccall) {
        // Use ccall with async:true for proper Asyncify handling
        await Module.ccall('LoadRomFromWeb', null, ['string'], [filename], {async: true});
        console.log('[FilesystemManager] LoadRomFromWeb completed via async ccall');
      } else if (Module._LoadRomFromWeb) {
        // Fallback: Properly allocate and convert string for direct function call
        // Note: This path may still have async issues if C++ triggers Asyncify
        var len = Module.lengthBytesUTF8(filename) + 1;
        var filenamePtr = Module._malloc(len);
        if (!filenamePtr) {
          throw new Error('Failed to allocate memory for filename');
        }
        try {
          Module.stringToUTF8(filename, filenamePtr, len);
          Module._LoadRomFromWeb(filenamePtr);
          console.log('[FilesystemManager] LoadRomFromWeb called via direct function');
        } finally {
          Module._free(filenamePtr);
        }
      } else {
        console.error('[FilesystemManager] LoadRomFromWeb function not available');
        alert('ROM loading not ready yet. Please wait for the app to initialize.');
        return false;
      }

      // 6. Track as recent file for quick access (skip sync - we handle it below)
      self.addRecentFile(filename, 'rom', true);

      // 7. Defer persistence to avoid overlapping with any pending Asyncify operations
      // The C++ side handles its own IndexedDB persistence; we just need to sync the recent files
      // Using setTimeout to ensure any C++ async operations have fully completed
      setTimeout(function() {
        if (self.ready && typeof FS !== 'undefined' && FS.syncfs) {
          FS.syncfs(false, function(err) {
            if (err) console.warn('[FilesystemManager] FS sync after ROM load failed:', err);
          });
        }
      }, 100);

      return true;

    } catch (wasmErr) {
      console.error('[FilesystemManager] WASM error loading ROM:', wasmErr);
      
      // Build detailed error message
      var errorMsg = 'Failed to load ROM: ';
      if (wasmErr.message) {
        errorMsg += wasmErr.message;
        
        // Check for specific error patterns
        if (wasmErr.message.includes('out of bounds') || wasmErr.message.includes('memory access')) {
          errorMsg += '\n\nThis may indicate ROM data corruption or an incompatible ROM format.';
        }
      } else if (wasmErr.toString) {
        errorMsg += wasmErr.toString();
      } else {
        errorMsg += 'Memory access error (ROM may be corrupted or invalid)';
      }
      
      // Report to crash reporter
      if (typeof window.yazeAddProblem === 'function') {
        window.yazeAddProblem(
          'ROM load error: ' + (wasmErr.message || wasmErr.toString()),
          'WASM',
          'error'
        );
      }
      
      alert(errorMsg);
      
      if (wasmErr.stack) {
        console.error('[FilesystemManager] Stack trace:', wasmErr.stack);
      }
      
      return false;
    }
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
  },

  /**
   * Syncs all persistent directories to IndexedDB.
   */
  syncAll: function() {
    if (!this.ensureReady()) return Promise.reject(new Error('Filesystem not ready'));
    return new Promise((resolve, reject) => {
      FS.syncfs(false, function(err) {
        if (err) {
          console.warn('[FilesystemManager] Failed to sync filesystem:', err);
          reject(err);
        } else {
          console.log('[FilesystemManager] All directories synced to persistent storage.');
          resolve();
        }
      });
    });
  },

  // ========================
  // Config File Operations
  // ========================

  /**
   * Saves a config file.
   * @param {string} name - Config file name (without path)
   * @param {string|Object} data - Data to save (string or JSON object)
   */
  saveConfig: function(name, data) {
    if (!this.ensureReady()) return false;
    try {
      var content = typeof data === 'string' ? data : JSON.stringify(data, null, 2);
      var path = this.directories.config + '/' + name;
      FS.writeFile(path, content);
      this.syncAll();
      console.log('[FilesystemManager] Config saved:', name);
      return true;
    } catch (e) {
      console.error('[FilesystemManager] Failed to save config:', name, e);
      return false;
    }
  },

  /**
   * Loads a config file.
   * @param {string} name - Config file name (without path)
   * @param {boolean} parseJson - Whether to parse as JSON
   * @returns {string|Object|null}
   */
  loadConfig: function(name, parseJson) {
    if (!this.ensureReady()) return null;
    try {
      var path = this.directories.config + '/' + name;
      var content = FS.readFile(path, { encoding: 'utf8' });
      return parseJson ? JSON.parse(content) : content;
    } catch (e) {
      // File may not exist
      return null;
    }
  },

  /**
   * Lists all config files.
   * @returns {string[]}
   */
  listConfigs: function() {
    if (!this.ensureReady()) return [];
    try {
      return FS.readdir(this.directories.config).filter(f => f !== '.' && f !== '..');
    } catch (e) {
      return [];
    }
  },

  // ========================
  // Project File Operations
  // ========================

  /**
   * Saves a project file.
   * @param {string} name - Project name
   * @param {string|Object} data - Project data
   */
  saveProject: function(name, data) {
    if (!this.ensureReady()) return false;
    try {
      var content = typeof data === 'string' ? data : JSON.stringify(data, null, 2);
      var path = this.directories.projects + '/' + name + '.yproj';
      FS.writeFile(path, content);
      this.syncAll();
      console.log('[FilesystemManager] Project saved:', name);
      return true;
    } catch (e) {
      console.error('[FilesystemManager] Failed to save project:', name, e);
      return false;
    }
  },

  /**
   * Loads a project file.
   * @param {string} name - Project name
   * @returns {Object|null}
   */
  loadProject: function(name) {
    if (!this.ensureReady()) return null;
    try {
      var path = this.directories.projects + '/' + name + '.yproj';
      var content = FS.readFile(path, { encoding: 'utf8' });
      return JSON.parse(content);
    } catch (e) {
      return null;
    }
  },

  /**
   * Lists all project files.
   * @returns {string[]}
   */
  listProjects: function() {
    if (!this.ensureReady()) return [];
    try {
      return FS.readdir(this.directories.projects)
        .filter(f => f.endsWith('.yproj'))
        .map(f => f.replace('.yproj', ''));
    } catch (e) {
      return [];
    }
  },

  // ========================
  // Agent Prompt Operations
  // ========================

  /**
   * Saves an agent prompt file.
   * @param {string} name - Prompt name
   * @param {string} content - Prompt content
   */
  savePrompt: function(name, content) {
    if (!this.ensureReady()) return false;
    try {
      var path = this.directories.prompts + '/' + name + '.md';
      FS.writeFile(path, content);
      this.syncAll();
      console.log('[FilesystemManager] Prompt saved:', name);
      return true;
    } catch (e) {
      console.error('[FilesystemManager] Failed to save prompt:', name, e);
      return false;
    }
  },

  /**
   * Loads an agent prompt file.
   * @param {string} name - Prompt name
   * @returns {string|null}
   */
  loadPrompt: function(name) {
    if (!this.ensureReady()) return null;
    try {
      var path = this.directories.prompts + '/' + name + '.md';
      return FS.readFile(path, { encoding: 'utf8' });
    } catch (e) {
      return null;
    }
  },

  /**
   * Lists all agent prompt files.
   * @returns {string[]}
   */
  listPrompts: function() {
    if (!this.ensureReady()) return [];
    try {
      return FS.readdir(this.directories.prompts)
        .filter(f => f.endsWith('.md'))
        .map(f => f.replace('.md', ''));
    } catch (e) {
      return [];
    }
  },

  // ========================
  // Recent Files Operations
  // ========================

  /**
   * Adds a file to recent files list.
   * @param {string} filename - Full path or name of the file
   * @param {string} type - Type of file ('rom', 'project', etc.)
   * @param {boolean} skipSync - If true, skip the syncAll call (caller will handle sync)
   */
  addRecentFile: function(filename, type, skipSync) {
    if (!this.ensureReady()) return;
    try {
      var recentPath = this.directories.recent + '/recent.json';
      var recent = [];
      try {
        var content = FS.readFile(recentPath, { encoding: 'utf8' });
        recent = JSON.parse(content);
      } catch (e) {
        // File doesn't exist yet
      }

      // Remove existing entry if present
      recent = recent.filter(r => r.filename !== filename);

      // Add new entry at the beginning
      recent.unshift({
        filename: filename,
        type: type || 'unknown',
        timestamp: Date.now()
      });

      // Limit to 20 entries
      if (recent.length > 20) {
        recent = recent.slice(0, 20);
      }

      FS.writeFile(recentPath, JSON.stringify(recent, null, 2));
      // Only sync if not explicitly skipped (caller may defer sync to avoid Asyncify conflicts)
      if (!skipSync) {
        this.syncAll();
      }
    } catch (e) {
      console.error('[FilesystemManager] Failed to update recent files:', e);
    }
  },

  /**
   * Gets the list of recent files.
   * @param {number} maxCount - Maximum number of files to return
   * @returns {Array}
   */
  getRecentFiles: function(maxCount) {
    if (!this.ensureReady()) return [];
    try {
      var recentPath = this.directories.recent + '/recent.json';
      var content = FS.readFile(recentPath, { encoding: 'utf8' });
      var recent = JSON.parse(content);
      return (maxCount ? recent.slice(0, maxCount) : recent);
    } catch (e) {
      return [];
    }
  },

  /**
   * Clears the recent files list.
   */
  clearRecentFiles: function() {
    if (!this.ensureReady()) return;
    try {
      var recentPath = this.directories.recent + '/recent.json';
      FS.writeFile(recentPath, '[]');
      this.syncAll();
    } catch (e) {
      console.error('[FilesystemManager] Failed to clear recent files:', e);
    }
  },

  // ========================
  // Generic File Operations
  // ========================

  /**
   * Reads a file from the virtual filesystem.
   * @param {string} path - Full path to the file
   * @param {string} encoding - 'utf8' for text, 'binary' for Uint8Array
   * @returns {string|Uint8Array|null}
   */
  readFile: function(path, encoding) {
    if (!this.ensureReady()) return null;
    try {
      return FS.readFile(path, { encoding: encoding || 'utf8' });
    } catch (e) {
      console.error('[FilesystemManager] Failed to read file:', path, e);
      return null;
    }
  },

  /**
   * Writes a file to the virtual filesystem.
   * @param {string} path - Full path to the file
   * @param {string|Uint8Array} data - Data to write
   * @returns {boolean}
   */
  writeFile: function(path, data) {
    if (!this.ensureReady()) return false;
    try {
      FS.writeFile(path, data);
      return true;
    } catch (e) {
      console.error('[FilesystemManager] Failed to write file:', path, e);
      return false;
    }
  },

  /**
   * Checks if a file exists.
   * @param {string} path - Full path to the file
   * @returns {boolean}
   */
  fileExists: function(path) {
    if (!this.ensureReady()) return false;
    try {
      FS.stat(path);
      return true;
    } catch (e) {
      return false;
    }
  },

  /**
   * Validates ROM file data before passing to C++.
   * Checks file size, format, and basic integrity.
   * @param {Uint8Array} data - ROM file data
   * @param {string} filename - Original filename for extension detection
   * @returns {Object} - { valid: boolean, error: string|null, info: Object }
   */
  validateRomData: function(data, filename) {
    var result = {
      valid: false,
      error: null,
      info: {
        size: data ? data.length : 0,
        hasHeader: false,
        headerSize: 0,
        mappingMode: 'unknown'
      }
    };

    // Check if data exists
    if (!data) {
      result.error = 'ROM data is null or undefined';
      return result;
    }

    // Check minimum size (32KB for smallest valid SNES ROM)
    if (data.length < 0x8000) {
      result.error = 'ROM file too small: ' + data.length + ' bytes (minimum 32KB required)';
      return result;
    }

    // Get file extension
    var ext = filename.split('.').pop().toLowerCase();

    // For SFC/SMC files, perform SNES-specific validation
    if (ext === 'sfc' || ext === 'smc') {
      // Check for SMC header (512 bytes)
      // SMC header present if (size % 1024) == 512
      var hasSmcHeader = (data.length % 1024) === 512;
      result.info.hasHeader = hasSmcHeader;
      result.info.headerSize = hasSmcHeader ? 512 : 0;

      // Get actual ROM size without header
      var romSize = hasSmcHeader ? data.length - 512 : data.length;

      // ALTTP ROM should be 1MB (0x100000) without header
      // But allow variations (0.5MB to 6MB for hacked ROMs)
      if (romSize < 0x80000) {
        result.error = 'ROM too small: ' + (romSize / 1024).toFixed(0) + 'KB (minimum 512KB for SNES ROM)';
        return result;
      }

      if (romSize > 0x600000) {
        result.error = 'ROM too large: ' + (romSize / 1024 / 1024).toFixed(1) + 'MB (maximum 6MB supported)';
        return result;
      }

      // Try to detect mapping mode from header
      var headerOffset = hasSmcHeader ? 512 : 0;

      // Check LoROM header at 0x7FC0 or HiROM header at 0xFFC0
      var loRomOffset = headerOffset + 0x7FC0;
      var hiRomOffset = headerOffset + 0xFFC0;

      // Validate offsets are within bounds
      if (loRomOffset + 32 <= data.length) {
        // Check for valid LoROM mapping byte at 0x7FD5
        var loMappingByte = data[headerOffset + 0x7FD5];
        if ((loMappingByte & 0x01) === 0) {
          result.info.mappingMode = 'LoROM';
        }
      }

      if (hiRomOffset + 32 <= data.length) {
        // Check for valid HiROM mapping byte at 0xFFD5
        var hiMappingByte = data[headerOffset + 0xFFD5];
        if ((hiMappingByte & 0x01) === 1) {
          result.info.mappingMode = 'HiROM';
        }
      }

      // ALTTP is LoROM, 1MB
      if (result.info.mappingMode === 'unknown') {
        console.warn('[ROM] Could not detect mapping mode, assuming LoROM');
        result.info.mappingMode = 'LoROM';
      }
    }

    // Check for ZIP files (not directly loadable)
    if (ext === 'zip') {
      // Check ZIP magic bytes (PK\x03\x04)
      if (data.length >= 4 && data[0] === 0x50 && data[1] === 0x4B && data[2] === 0x03 && data[3] === 0x04) {
        result.error = 'ZIP files must be extracted first. Please upload the .sfc or .smc file inside.';
        return result;
      }
    }

    // All checks passed
    result.valid = true;
    console.log('[ROM] Validation passed:', result.info);
    return result;
  },

  /**
   * Lists files in a directory.
   * @param {string} path - Directory path
   * @returns {string[]}
   */
  listDirectory: function(path) {
    if (!this.ensureReady()) return [];
    try {
      return FS.readdir(path).filter(f => f !== '.' && f !== '..');
    } catch (e) {
      return [];
    }
  },

  // ========================
  // File Management Operations
  // ========================

  /**
   * Deletes a file from the virtual filesystem.
   * @param {string} path - Full path to the file
   * @returns {boolean} - True if successful
   */
  deleteFile: function(path) {
    if (!this.ensureReady()) return false;
    try {
      FS.unlink(path);
      this.syncAll();
      console.log('[FilesystemManager] Deleted file:', path);
      return true;
    } catch (e) {
      console.error('[FilesystemManager] Failed to delete file:', path, e);
      return false;
    }
  },

  /**
   * Renames/moves a file in the virtual filesystem.
   * @param {string} oldPath - Current file path
   * @param {string} newPath - New file path
   * @returns {boolean} - True if successful
   */
  renameFile: function(oldPath, newPath) {
    if (!this.ensureReady()) return false;
    try {
      FS.rename(oldPath, newPath);
      this.syncAll();
      console.log('[FilesystemManager] Renamed file:', oldPath, '->', newPath);
      return true;
    } catch (e) {
      console.error('[FilesystemManager] Failed to rename file:', oldPath, e);
      return false;
    }
  },

  /**
   * Gets file information (size, modification time).
   * @param {string} path - Full path to the file
   * @returns {Object|null} - File info object or null if error
   */
  getFileInfo: function(path) {
    if (!this.ensureReady()) return null;
    try {
      var stat = FS.stat(path);
      return {
        size: stat.size,
        mtime: new Date(stat.mtime),
        isDirectory: FS.isDir(stat.mode)
      };
    } catch (e) {
      return null;
    }
  },

  /**
   * Gets detailed file listing for a directory.
   * @param {string} dirPath - Directory path
   * @returns {Array} - Array of file info objects
   */
  getDirectoryListing: function(dirPath) {
    if (!this.ensureReady()) return [];
    try {
      var files = FS.readdir(dirPath).filter(f => f !== '.' && f !== '..');
      var self = this;
      return files.map(function(filename) {
        var fullPath = dirPath + '/' + filename;
        var info = self.getFileInfo(fullPath);
        return {
          name: filename,
          path: fullPath,
          size: info ? info.size : 0,
          mtime: info ? info.mtime : null,
          isDirectory: info ? info.isDirectory : false
        };
      });
    } catch (e) {
      console.error('[FilesystemManager] Failed to list directory:', dirPath, e);
      return [];
    }
  },

  /**
   * Gets storage usage estimate using browser Storage API.
   * @returns {Promise<Object>} - Storage info with usage and quota
   */
  getStorageUsage: function() {
    return new Promise(function(resolve) {
      if (navigator.storage && navigator.storage.estimate) {
        navigator.storage.estimate().then(function(estimate) {
          resolve({
            usage: estimate.usage || 0,
            quota: estimate.quota || 0,
            percent: estimate.quota ? Math.round((estimate.usage / estimate.quota) * 100) : 0
          });
        }).catch(function() {
          resolve({ usage: 0, quota: 0, percent: 0 });
        });
      } else {
        resolve({ usage: 0, quota: 0, percent: 0 });
      }
    });
  },

  /**
   * Downloads a file from the virtual filesystem to user's computer.
   * @param {string} path - Full path to the file
   * @param {string} downloadName - Optional name for the downloaded file
   */
  downloadFile: function(path, downloadName) {
    if (!this.ensureReady()) return;
    try {
      var content = FS.readFile(path);
      var blob = new Blob([content], { type: 'application/octet-stream' });
      var url = URL.createObjectURL(blob);

      var filename = downloadName || path.split('/').pop();
      var a = document.createElement('a');
      a.style.display = 'none';
      a.href = url;
      a.download = filename;
      document.body.appendChild(a);
      a.click();

      setTimeout(function() {
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
      }, 1000);

      console.log('[FilesystemManager] Downloaded file:', path);
    } catch (e) {
      console.error('[FilesystemManager] Failed to download file:', path, e);
      alert('Failed to download file: ' + e.message);
    }
  }
};

/**
 * Wait for WASM Module to be ready.
 * Uses the boot promise from namespace.js instead of polling.
 * Falls back to polling if namespace not available (backward compatibility).
 * @param {Function} cb - Callback to invoke when Module is ready
 */
function waitForModule(cb) {
  // Prefer boot promise (deterministic, no polling)
  if (window.yaze && window.yaze.core && window.yaze.core.ready) {
    window.yaze.core.ready().then(function() {
      cb();
    });
    return;
  }

  // Fallback to polling if namespace not available
  if (typeof Module !== 'undefined' && window.YAZE_MODULE_READY) {
    cb();
  } else {
    setTimeout(function() { waitForModule(cb); }, 30);
  }
}

// Bind to Module.onFileSystemReady when Module is available
waitForModule(() => {
  if (typeof Module !== 'undefined') {
    // Ensure FS is globally available (MODULARIZE mode puts it on Module.FS)
    var fsExposed = false;
    var exposeFS = function() {
      if (fsExposed) return;
      if (typeof FS === 'undefined') {
        if (Module.FS) {
          console.log('[FilesystemManager] Aliasing Module.FS to window.FS');
          window.FS = Module.FS;
          fsExposed = true;
        } else if (window.Module && window.Module.FS) {
          console.log('[FilesystemManager] Aliasing window.Module.FS to window.FS');
          window.FS = window.Module.FS;
          fsExposed = true;
        }
      } else {
        fsExposed = true;
      }
    };

    // Try immediately and retry only if first attempt didn't succeed
    exposeFS();
    if (!fsExposed) {
      setTimeout(exposeFS, 500);
    }

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
