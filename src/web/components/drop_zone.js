/**
 * Yaze Drop Zone Handler
 * Provides drag and drop ROM loading for the WASM build
 */

(function() {
  'use strict';

  // Configuration
  const config = {
    validExtensions: ['sfc', 'smc', 'zip', 'srm', 'sav', 'yproj'],
    maxFileSize: 10 * 1024 * 1024, // 10MB
    messages: {
      dropHere: 'Drop file here',
      supported: 'ROMs (.sfc), Saves (.srm), Projects (.yproj)',
      loading: 'Processing file...',
      invalidType: 'Invalid file type.',
      tooLarge: 'File too large. Maximum size is 10MB.',
      readError: 'Failed to read file.',
      multipleFiles: 'Please drop only one file at a time.'
    }
  };

  // State
  let state = {
    initialized: false,
    dragCounter: 0,
    overlay: null,
    callbacks: {
      onDrop: null,
      onError: null,
      onDragEnter: null,
      onDragLeave: null
    }
  };

  /**
   * Initialize the drop zone
   * @param {Object} options Configuration options
   */
  window.YazeDropZone = {
    init: function(options = {}) {
      if (state.initialized) {
        console.warn('YazeDropZone already initialized');
        return;
      }

      // Merge options with defaults
      Object.assign(config, options.config || {});
      Object.assign(state.callbacks, options.callbacks || {});

      // Create overlay
      createOverlay();

      // Register event handlers
      registerEventHandlers();

      state.initialized = true;
      console.log('YazeDropZone initialized');
    },

    /**
     * Set a callback function
     * @param {string} name Callback name
     * @param {Function} callback Callback function
     */
    setCallback: function(name, callback) {
      if (state.callbacks.hasOwnProperty(name)) {
        state.callbacks[name] = callback;
      }
    },

    /**
     * Enable or disable the drop zone
     * @param {boolean} enabled
     */
    setEnabled: function(enabled) {
      if (state.overlay) {
        state.overlay.style.pointerEvents = enabled ? 'auto' : 'none';
      }
    },

    /**
     * Show a message in the overlay
     * @param {string} message
     * @param {string} info Additional info text
     */
    showMessage: function(message, info) {
      if (state.overlay) {
        const textEl = state.overlay.querySelector('.yaze-drop-text');
        const infoEl = state.overlay.querySelector('.yaze-drop-info');
        if (textEl) textEl.textContent = message;
        if (infoEl) infoEl.textContent = info || '';
      }
    },

    /**
     * Clean up and remove drop zone
     */
    destroy: function() {
      if (!state.initialized) return;

      // Remove event handlers
      document.removeEventListener('dragenter', handleDragEnter);
      document.removeEventListener('dragover', handleDragOver);
      document.removeEventListener('dragleave', handleDragLeave);
      document.removeEventListener('drop', handleDrop);

      // Remove overlay
      if (state.overlay && state.overlay.parentNode) {
        state.overlay.parentNode.removeChild(state.overlay);
      }

      state.initialized = false;
      state.overlay = null;
      console.log('YazeDropZone destroyed');
    }
  };

  /**
   * Create the drop zone overlay element
   */
  function createOverlay() {
    // Check if overlay already exists
    state.overlay = document.getElementById('yaze-drop-overlay');
    if (state.overlay) {
      return;
    }

    // Create new overlay
    state.overlay = document.createElement('div');
    state.overlay.id = 'yaze-drop-overlay';
    state.overlay.className = 'yaze-drop-overlay';
    state.overlay.innerHTML = `
      <div class="yaze-drop-content">
        <div class="yaze-drop-icon">📁</div>
        <div class="yaze-drop-text">${config.messages.dropHere}</div>
        <div class="yaze-drop-info">${config.messages.supported}</div>
        <div class="yaze-drop-progress" style="display: none;">
          <div class="yaze-drop-progress-bar"></div>
        </div>
      </div>
    `;

    document.body.appendChild(state.overlay);
  }

  /**
   * Register drag and drop event handlers
   */
  function registerEventHandlers() {
    document.addEventListener('dragenter', handleDragEnter, false);
    document.addEventListener('dragover', handleDragOver, false);
    document.addEventListener('dragleave', handleDragLeave, false);
    document.addEventListener('drop', handleDrop, false);
  }

  /**
   * Check if a file has a valid ROM extension
   * @param {string} filename
   * @returns {boolean}
   */
  function isValidRomFile(filename) {
    const ext = filename.toLowerCase().split('.').pop();
    return config.validExtensions.includes(ext);
  }

  /**
   * Check if drag event contains files
   * @param {DragEvent} e
   * @returns {boolean}
   */
  function containsFiles(e) {
    if (e.dataTransfer.types) {
      for (let i = 0; i < e.dataTransfer.types.length; i++) {
        if (e.dataTransfer.types[i] === 'Files') {
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Format file size for display
   * @param {number} bytes
   * @returns {string}
   */
  function formatFileSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
  }

  /**
   * Handle drag enter event
   * @param {DragEvent} e
   */
  function handleDragEnter(e) {
    if (containsFiles(e)) {
      e.preventDefault();
      e.stopPropagation();

      state.dragCounter++;
      if (state.dragCounter === 1) {
        state.overlay.classList.add('yaze-drop-active');
        if (state.callbacks.onDragEnter) {
          state.callbacks.onDragEnter();
        }
      }
    }
  }

  /**
   * Handle drag over event
   * @param {DragEvent} e
   */
  function handleDragOver(e) {
    if (containsFiles(e)) {
      e.preventDefault();
      e.stopPropagation();
      e.dataTransfer.dropEffect = 'copy';
    }
  }

  /**
   * Handle drag leave event
   * @param {DragEvent} e
   */
  function handleDragLeave(e) {
    e.preventDefault();
    e.stopPropagation();

    state.dragCounter--;
    if (state.dragCounter === 0) {
      state.overlay.classList.remove('yaze-drop-active');
      if (state.callbacks.onDragLeave) {
        state.callbacks.onDragLeave();
      }
    }
  }

  /**
   * Handle drop event
   * @param {DragEvent} e
   */
  function handleDrop(e) {
    e.preventDefault();
    e.stopPropagation();

    state.dragCounter = 0;
    state.overlay.classList.remove('yaze-drop-active');

    const files = e.dataTransfer.files;
    if (!files || files.length === 0) {
      showError('No files dropped');
      return;
    }

    if (files.length > 1) {
      showError(config.messages.multipleFiles);
      return;
    }

    const file = files[0];
    processFile(file);
  }

  /**
   * Process a dropped file
   * @param {File} file
   */
  function processFile(file) {
    // Validate file type
    if (!isValidRomFile(file.name)) {
      showError(config.messages.invalidType);
      return;
    }

    // Validate file size
    if (file.size > config.maxFileSize) {
      showError(config.messages.tooLarge);
      return;
    }

    // Ensure FS is ready before processing
  if (typeof FilesystemManager !== 'undefined' && typeof FilesystemManager.ensureReady === 'function') {
    if (!FilesystemManager.ensureReady(true)) {
      // If not ready, try again in a moment
      console.log('[DropZone] FS not ready, retrying in 500ms...');
      setTimeout(() => processFile(file), 500);
      return;
    }
  }

    // Show loading state
    showLoading(file.name, file.size);

    // Read file
    const reader = new FileReader();

    reader.onload = function() {
      hideLoading();

      // Convert ArrayBuffer to Uint8Array
      const data = new Uint8Array(reader.result);

      if (state.callbacks.onDrop) {
        state.callbacks.onDrop(file.name, data);
      }

      // Delegate to FilesystemManager based on extension
      if (typeof FilesystemManager !== 'undefined') {
        const ext = file.name.split('.').pop().toLowerCase();
        try {
          if (['sfc', 'smc', 'zip'].includes(ext)) {
             // ROM File
             if (typeof Module !== 'undefined') {
                FilesystemManager.handleRomData(file.name, data);
                console.log('[DropZone] ROM data passed to FilesystemManager');
             }
          } else if (['srm', 'sav'].includes(ext)) {
             // Save File - Write to /.yaze/saves and sync
             const savePath = '/.yaze/saves/' + file.name;
             FilesystemManager.writeFile(savePath, data);
             FilesystemManager.syncAll();
             alert('Save file imported: ' + file.name + '\nReload ROM to apply.');
          } else if (['yaze', 'zsproj', 'yproj'].includes(ext)) {
             // Project File - Write to /.yaze/projects
             const projPath = '/.yaze/projects/' + file.name;
             FilesystemManager.writeFile(projPath, data);
             FilesystemManager.syncAll();
             alert('Project imported: ' + file.name);
          }
        } catch (err) {
          console.error('[DropZone] Error processing file:', err);
          showError('Failed to process file: ' + err.message);
        }
      }
    };

    reader.onerror = function() {
      hideLoading();
      showError(config.messages.readError + ': ' + file.name);
    };

    reader.onprogress = function(e) {
      if (e.lengthComputable) {
        updateProgress(e.loaded / e.total);
      }
    };

    reader.readAsArrayBuffer(file);
  }

  /**
   * Show loading state
   * @param {string} filename
   * @param {number} filesize
   */
  function showLoading(filename, filesize) {
    state.overlay.classList.add('yaze-drop-loading');
    const textEl = state.overlay.querySelector('.yaze-drop-text');
    const infoEl = state.overlay.querySelector('.yaze-drop-info');
    const progressEl = state.overlay.querySelector('.yaze-drop-progress');

    if (textEl) textEl.textContent = config.messages.loading;
    if (infoEl) infoEl.textContent = `${filename} (${formatFileSize(filesize)})`;
    if (progressEl) progressEl.style.display = 'block';
  }

  /**
   * Update progress bar
   * @param {number} progress Value between 0 and 1
   */
  function updateProgress(progress) {
    const progressBar = state.overlay.querySelector('.yaze-drop-progress-bar');
    if (progressBar) {
      progressBar.style.width = (progress * 100) + '%';
    }
  }

  /**
   * Hide loading state
   */
  function hideLoading() {
    setTimeout(function() {
      state.overlay.classList.remove('yaze-drop-loading');
      const textEl = state.overlay.querySelector('.yaze-drop-text');
      const infoEl = state.overlay.querySelector('.yaze-drop-info');
      const progressEl = state.overlay.querySelector('.yaze-drop-progress');

      if (textEl) textEl.textContent = config.messages.dropHere;
      if (infoEl) infoEl.textContent = config.messages.supported;
      if (progressEl) {
        progressEl.style.display = 'none';
        const progressBar = progressEl.querySelector('.yaze-drop-progress-bar');
        if (progressBar) progressBar.style.width = '0%';
      }
    }, 500);
  }

  /**
   * Show error message
   * @param {string} message
   */
  function showError(message) {
    if (state.callbacks.onError) {
      state.callbacks.onError(message);
    }

    // Show error in console and alert user
    console.error('YazeDropZone:', message);

    // Show user-friendly error message
    if (message && message.length > 0) {
      alert('Drop error: ' + message);
    }
  }

  // NOTE: Auto-initialization is disabled because the C++ WasmDropHandler
  // (in wasm_drop_handler.cc) sets up its own drop zone via setupDropZone_impl.
  // Having both active would cause duplicate handling of drop events.
  // If you need JS-only drop handling, call YazeDropZone.init() manually.
  //
  // The C++ handler is preferred because it integrates directly with the
  // ROM loading pipeline via LoadRomFromWeb.

})();
