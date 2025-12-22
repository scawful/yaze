/**
 * General Shell UI Logic
 * Handles settings, menus, theme syncing, and other global UI interactions.
 */

// Settings Modal Logic
function showSettings() {
  document.getElementById('settings-modal').style.display = 'flex';
  // Load and sync all settings from localStorage
  loadSettingsFromStorage();
}

function hideSettings() {
  document.getElementById('settings-modal').style.display = 'none';
}

function loadSettingsFromStorage() {
  // Theme
  var theme = localStorage.getItem('yaze-theme') || 'default';
  document.getElementById('settings-theme').value = theme;

  // Font size
  var fontSize = localStorage.getItem('yaze-font-size') || '12';
  document.getElementById('settings-font-size').value = fontSize;
  document.getElementById('font-size-display').textContent = fontSize + 'px';

  // Terminal height
  var termHeight = localStorage.getItem('yaze-terminal-height') || '250';
  document.getElementById('settings-terminal-height').value = termHeight;
  document.getElementById('terminal-height-display').textContent = termHeight + 'px';

  // Touch mode
  var touchMode = localStorage.getItem('yaze-touch-mode') === 'true';
  document.getElementById('setting-touch').checked = touchMode;

  // Pixel grid
  var pixelGrid = localStorage.getItem('yaze-pixel-grid') === 'true';
  document.getElementById('setting-pixel-grid').checked = pixelGrid;
}

function applyThemeSetting(theme) {
  if (theme === 'synced') {
    syncTheme();
  } else {
    setTheme(theme);
  }
}

function applyFontSizeSetting(size) {
  document.getElementById('font-size-display').textContent = size + 'px';
  document.body.style.fontSize = size + 'px';
  localStorage.setItem('yaze-font-size', size);
}

function applyTerminalHeightSetting(height) {
  document.getElementById('terminal-height-display').textContent = height + 'px';
  if (panels && panels.container) {
    panels.height = parseInt(height);
    if (!panels.container.classList.contains('panel-collapsed')) {
      panels.container.style.height = height + 'px';
    }
  }
  localStorage.setItem('yaze-terminal-height', height);
}

function applyTouchModeSetting(enabled) {
  document.body.classList.toggle('yaze-touch-mode', enabled);
  localStorage.setItem('yaze-touch-mode', enabled);
}

function applyPixelGridSetting(enabled) {
  var canvas = document.getElementById('canvas');
  if (canvas) {
    canvas.style.imageRendering = enabled ? 'auto' : 'pixelated';
  }
  localStorage.setItem('yaze-pixel-grid', enabled);
}

function resetSettings() {
  if (!confirm('Reset all settings to defaults?')) return;

  // Clear all settings from localStorage
  localStorage.removeItem('yaze-theme');
  localStorage.removeItem('yaze-font-size');
  localStorage.removeItem('yaze-terminal-height');
  localStorage.removeItem('yaze-touch-mode');
  localStorage.removeItem('yaze-pixel-grid');

  // Apply defaults
  setTheme('default');
  applyFontSizeSetting('12');
  applyTerminalHeightSetting('250');
  applyTouchModeSetting(false);
  applyPixelGridSetting(false);

  // Reload settings UI
  loadSettingsFromStorage();
}

// Theme management
function setTheme(theme) {
  // Use unified theme definitions if available
  if (window.YazeThemes && window.YazeThemes.applyTheme) {
    // Map legacy theme names to new unified names
    var themeMap = {
      'default': 'hacker_green',
      'modern': 'modern',
      'halext': 'halext',
      'synced': 'synced'
    };
    var unifiedTheme = themeMap[theme] || theme;

    if (unifiedTheme === 'synced') {
      syncTheme();
      return;
    }

    if (window.YazeThemes.applyTheme(unifiedTheme)) {
      return;
    }
  }

  // Fallback: Clear inline styles from sync
  // Note: We reset properties we override
  var root = document.documentElement;
  var props = ['--bg-app', '--bg-header', '--bg-panel', '--bg-input', '--bg-hover', '--bg-selected',
               '--text-primary', '--text-secondary', '--text-muted', '--text-accent',
               '--accent-primary', '--accent-hover', '--accent-dim',
               '--border-color', '--border-focus',
               '--status-success', '--status-warning', '--status-error', '--control-radius'];
  props.forEach(p => root.style.removeProperty(p));

  if (theme === 'default') {
    document.body.removeAttribute('data-theme');
    localStorage.removeItem('yaze-theme');
  } else if (theme === 'synced') {
     document.body.setAttribute('data-theme', 'synced');
     localStorage.setItem('yaze-theme', 'synced');
     // Attempt sync if possible
     syncTheme();
  } else {
    document.body.setAttribute('data-theme', theme);
    localStorage.setItem('yaze-theme', theme);
  }
}

async function syncTheme() {
  // Use unified theme system if available
  if (window.YazeThemes && window.YazeThemes.syncFromCpp) {
    if (window.YazeThemes.syncFromCpp()) {
      return true;
    }
  }

  // Fallback to direct Module call
  if (typeof Module !== 'undefined' && Module.settingsGetCurrentThemeData) {
    try {
      var data = await Module.settingsGetCurrentThemeData();
      if (data) {
        applyThemeFromData(data);
        // Ensure storage is updated
        document.body.setAttribute('data-theme', 'synced');
        localStorage.setItem('yaze-theme', 'synced');
        
        // Update selector if not already set
        var select = document.getElementById('settings-theme');
        if (select && select.value !== 'synced') {
           select.value = 'synced';
        }
        return true;
      }
    } catch (e) {
      console.warn('Failed to sync theme from C++:', e);
    }
  }
  return false;
}

function applyThemeFromData(jsonStr) {
  try {
    var themeData = JSON.parse(jsonStr);
    var colors = themeData.colors;
    var root = document.documentElement;

    // Set CSS variables from C++ theme
    if (colors) {
      root.style.setProperty('--bg-app', colors.background);
      root.style.setProperty('--bg-header', colors.header || colors.menu_bar_bg);
      root.style.setProperty('--bg-panel', colors.surface || colors.window_bg);
      root.style.setProperty('--bg-input', colors.frame_bg);
      root.style.setProperty('--bg-hover', colors.button_hovered);
      root.style.setProperty('--bg-selected', colors.button_active);

      root.style.setProperty('--text-primary', colors.text_primary);
      root.style.setProperty('--text-secondary', colors.text_secondary);
      root.style.setProperty('--text-muted', colors.text_disabled);
      root.style.setProperty('--text-accent', colors.text_highlight || colors.accent);

      root.style.setProperty('--accent-primary', colors.primary);
      root.style.setProperty('--accent-hover', colors.button_hovered);
      root.style.setProperty('--accent-dim', colors.secondary);

      root.style.setProperty('--border-color', colors.border);
      root.style.setProperty('--border-focus', colors.accent);

      root.style.setProperty('--status-success', colors.success);
      root.style.setProperty('--status-warning', colors.warning);
      root.style.setProperty('--status-error', colors.error);
    }

    if (themeData.style) {
       var rounding = themeData.style.frame_rounding || 2;
       root.style.setProperty('--control-radius', rounding + 'px');
    }
    
    console.log("Theme synced from C++: " + (themeData.name || 'Unknown'));
  } catch (e) {
    console.error("Error applying theme data:", e);
  }
}

function loadUserFont() {
  var input = document.getElementById('font-input');
  if (!input || !input.files || !input.files[0]) {
    alert("Please select a font file (.ttf or .otf)");
    return;
  }
  
  var file = input.files[0];
  var reader = new FileReader();
  
  reader.onload = async function(e) {
    var arrayBuffer = e.target.result;
    var uint8Array = new Uint8Array(arrayBuffer);
    
    var binaryString = "";
    // Chunk processing
    var chunkSize = 0x8000;
    for (var i = 0; i < uint8Array.length; i += chunkSize) {
      binaryString += String.fromCharCode.apply(null, uint8Array.subarray(i, i + chunkSize));
    }
    
    if (Module.settingsLoadFont) {
      try {
        var resultStr = await Module.settingsLoadFont(file.name, binaryString, 16.0);
        var result = JSON.parse(resultStr);
        if (result.success) {
            alert("Font loaded successfully! Note: Texture atlas rebuild required.");
        } else {
            alert("Failed to load font: " + result.error);
        }
      } catch(e) {
          alert("Error loading font: " + e.message);
      }
    } else {
      alert("Module.settingsLoadFont not available");
    }
  };
  reader.readAsArrayBuffer(file);
}

// Initialize all settings on page load
(function initSettings() {
  // Theme
  const savedTheme = localStorage.getItem('yaze-theme');
  if (savedTheme) {
    setTheme(savedTheme);
  }

  // Font size
  const savedFontSize = localStorage.getItem('yaze-font-size');
  if (savedFontSize) {
    document.body.style.fontSize = savedFontSize + 'px';
  }

  // Terminal height
  const savedTermHeight = localStorage.getItem('yaze-terminal-height');
  if (savedTermHeight && panels) {
    panels.height = parseInt(savedTermHeight);
  }

  // Touch mode
  const savedTouchMode = localStorage.getItem('yaze-touch-mode') === 'true';
  if (savedTouchMode) {
    document.body.classList.add('yaze-touch-mode');
  }

  // Pixel grid
  const savedPixelGrid = localStorage.getItem('yaze-pixel-grid') === 'true';
  if (savedPixelGrid) {
    const canvas = document.getElementById('canvas');
    if (canvas) {
      canvas.style.imageRendering = 'auto';
    }
  }
  
  // Poll for WASM readiness to sync theme
  var syncInterval = setInterval(function() {
      if (typeof Module !== 'undefined' && Module.settingsGetCurrentThemeData) {
          clearInterval(syncInterval);
          if (localStorage.getItem('yaze-theme') === 'synced') {
              syncTheme();
          }
      }
  }, 500);
})();

// Helper to close all dropdown menus
function closeAllDropdowns() {
  var menus = ['help-menu', 'rom-files-menu', 'editor-menu', 'emulator-menu', 'layout-menu', 'ai-tools-menu'];
  menus.forEach(function(id) {
    var menu = document.getElementById(id);
    if (menu) menu.classList.remove('show');
  });
}

// Editor menu toggle
function toggleEditorMenu(e) {
  if (e) e.stopPropagation();
  var menu = document.getElementById('editor-menu');
  var wasOpen = menu.classList.contains('show');
  closeAllDropdowns();
  if (!wasOpen) {
    menu.classList.add('show');
    updateEditorMenuState();
  }
}

// Emulator menu toggle
function toggleEmulatorMenu(e) {
  if (e) e.stopPropagation();
  var menu = document.getElementById('emulator-menu');
  var wasOpen = menu.classList.contains('show');
  closeAllDropdowns();
  if (!wasOpen) menu.classList.add('show');
}

// Layout menu toggle
function toggleLayoutMenu(e) {
  if (e) e.stopPropagation();
  var menu = document.getElementById('layout-menu');
  var wasOpen = menu.classList.contains('show');
  closeAllDropdowns();
  if (!wasOpen) menu.classList.add('show');
}

// AI Tools menu toggle
function toggleAIToolsMenu(e) {
  if (e) e.stopPropagation();
  var menu = document.getElementById('ai-tools-menu');
  var wasOpen = menu.classList.contains('show');
  closeAllDropdowns();
  if (!wasOpen) menu.classList.add('show');
}

// Switch to a specific editor using yaze.control API
async function switchToEditor(editorName) {
  closeAllDropdowns();
  if (window.yaze && window.yaze.control && window.yaze.control.switchEditor) {
    try {
      var result = await window.yaze.control.switchEditor(editorName);
      console.log('Switch to editor:', editorName, result);
      updateCurrentEditorDisplay();
    } catch (e) {
      console.error('Error switching editor:', e);
      if (window.showYazeToast) window.showYazeToast('Failed to switch editor: ' + e.message, 'error');
    }
  } else {
    console.warn('yaze.control API not available');
    // Fallback: Try direct Module call
    if (typeof Module !== 'undefined' && Module.controlSwitchEditor) {
      Module.controlSwitchEditor(editorName);
      updateCurrentEditorDisplay();
    } else {
        if (window.showYazeToast) window.showYazeToast('Editor switching not available yet', 'warning');
    }
  }
}

// Update current editor name in header
function updateCurrentEditorDisplay() {
  var nameEl = document.getElementById('current-editor-name');
  if (!nameEl) return;

  if (window.yaze && window.yaze.control && window.yaze.control.getCurrentEditor) {
    var editor = window.yaze.control.getCurrentEditor();
    if (editor && editor.name) {
      nameEl.textContent = editor.name;
    }
  }
}

// Update editor menu to highlight current editor
function updateEditorMenuState() {
  if (!window.yaze || !window.yaze.control) return;

  var current = window.yaze.control.getCurrentEditor();
  if (!current || !current.name) return;

  document.querySelectorAll('.editor-item').forEach(function(item) {
    var isActive = item.dataset.editor === current.name;
    item.classList.toggle('active', isActive);
  });
}

// Trigger emulator action
async function triggerEmulatorAction(action) {
  closeAllDropdowns();
  var actionMap = {
    'show': 'View.ShowEmulator',
    'run': 'Emulator.Run',
    'pause': 'Emulator.Pause',
    'step': 'Emulator.Step',
    'reset': 'Emulator.Reset',
    'memory': 'View.ShowMemoryEditor',
    'disassembly': 'View.ShowDisassembly'
  };

  var menuAction = actionMap[action];
  if (menuAction && window.yaze && window.yaze.control && window.yaze.control.triggerMenuAction) {
    try {
      var result = await window.yaze.control.triggerMenuAction(menuAction);
      console.log('Emulator action:', action, result);
      if (result === false && window.showYazeToast) {
          window.showYazeToast('Emulator not fully initialized or action failed', 'warning');
      }
    } catch (e) {
      console.error('Error triggering emulator action:', e);
      if (window.showYazeToast) window.showYazeToast('Emulator error: ' + e.message, 'error');
    }
  } else {
    console.warn('Action not available:', action);
    if (window.showYazeToast) window.showYazeToast('Emulator not available in Web version yet', 'info');
  }
}

// Set layout preset
async function setLayout(layoutName) {
  closeAllDropdowns();
  if (window.yaze && window.yaze.control && window.yaze.control.setPanelLayout) {
    try {
      var result = await window.yaze.control.setPanelLayout(layoutName);
      console.log('Set layout:', layoutName, result);
    } catch (e) {
      console.error('Error setting layout:', e);
      if (window.showYazeToast) window.showYazeToast('Failed to set layout: ' + e.message, 'error');
    }
  } else {
    console.warn('Layout API not available');
    if (window.showYazeToast) window.showYazeToast('Layout API not ready', 'warning');
  }
}

// Help menu toggle
function toggleHelpMenu(e) {
  if (e) e.stopPropagation();
  var menu = document.getElementById('help-menu');
  menu.classList.toggle('show');
  // Close ROM files menu if open
  var romMenu = document.getElementById('rom-files-menu');
  if (romMenu) romMenu.classList.remove('show');
}

function toggleRomFilesMenu(e) {
  if (e) e.stopPropagation();
  var menu = document.getElementById('rom-files-menu');
  menu.classList.toggle('show');
  // Close help menu if open
  var helpMenu = document.getElementById('help-menu');
  if (helpMenu) helpMenu.classList.remove('show');
  // Refresh ROM info and recent files when opening
  if (menu.classList.contains('show')) {
    updateRomFilesList();
    updateRecentFilesList();
  }
}

async function updateRomFilesList() {
  var listEl = document.getElementById('rom-files-list');
  if (!listEl) return;

  if (!window.yazeDebug || !window.yazeDebug.isReady()) {
    listEl.innerHTML = '<div style="padding: 8px 12px; color: var(--text-muted); font-style: italic;">Module not ready</div>';
    return;
  }

  try {
    var sessionsStr = await Module.getRomSessions();
    var sessions = JSON.parse(sessionsStr);
    var html = '';

    if (sessions.current_rom && sessions.current_rom.loaded) {
      html += '<div class="rom-item active">';
      html += '<span class="material-symbols-outlined rom-icon">description</span>';
      html += '<div class="rom-info">';
      html += '<div class="rom-name">' + (sessions.current_rom.title || 'Unknown') + '</div>';
      html += '<div class="rom-details">' + formatFileSize(sessions.current_rom.size) + ' - ' + (sessions.current_rom.filename || 'N/A') + '</div>';
      html += '</div></div>';
    } else {
      html = '<div style="padding: 8px 12px; color: var(--text-muted); font-style: italic; cursor: pointer;" onclick="document.getElementById(\'rom-input\').click()">No ROM loaded (Click to Open)</div>';
    }

    listEl.innerHTML = html;
  } catch (e) {
    listEl.innerHTML = '<div style="padding: 8px 12px; color: var(--status-error);">Error: ' + e.message + '</div>';
  }
}

function formatFileSize(bytes) {
  if (!bytes) return '0 B';
  var k = 1024;
  var sizes = ['B', 'KB', 'MB', 'GB'];
  var i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function updateRecentFilesList() {
  var listEl = document.getElementById('recent-files-list');
  if (!listEl) return;

  try {
    var recentFiles = FilesystemManager.getRecentFiles(5);
    var html = '';

    if (!recentFiles || recentFiles.length === 0) {
      html = '<div style="padding: 8px 12px; color: var(--text-muted); font-style: italic;">No recent files</div>';
    } else {
      recentFiles.forEach(function(file) {
        var filename = file.filename.split('/').pop();
        var timeAgo = formatTimeAgoShort(file.timestamp);
        html += '<a href="#" class="recent-file-item" onclick="event.stopPropagation(); loadRecentFile(\'' + file.filename.replace(/'/g, "\\'") + '\'); closeAllDropdowns(); return false;" style="display: flex; align-items: center; gap: 8px; padding: 6px 12px;">';
        html += '<span class="material-symbols-outlined" style="font-size: 16px; color: var(--accent-color);">history</span>';
        html += '<span style="flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;">' + filename + '</span>';
        html += '<span style="font-size: 10px; color: var(--text-muted);">' + timeAgo + '</span>';
        html += '</a>';
      });
    }

    listEl.innerHTML = html;
  } catch (e) {
    console.warn('[RecentFiles] Error updating list:', e);
    listEl.innerHTML = '<div style="padding: 8px 12px; color: var(--text-muted); font-style: italic;">No recent files</div>';
  }
}

function formatTimeAgoShort(timestamp) {
  var seconds = Math.floor((Date.now() - timestamp) / 1000);
  if (seconds < 60) return 'now';
  var minutes = Math.floor(seconds / 60);
  if (minutes < 60) return minutes + 'm';
  var hours = Math.floor(minutes / 60);
  if (hours < 24) return hours + 'h';
  var days = Math.floor(hours / 24);
  return days + 'd';
}

function loadRecentFile(filepath) {
  console.log('[RecentFiles] Loading:', filepath);
  if (typeof FilesystemManager !== 'undefined' && FilesystemManager._executeRomLoad) {
    FilesystemManager._executeRomLoad(filepath, null);
  } else {
    console.error('[RecentFiles] FilesystemManager not available');
  }
}

async function showFileManagerDebug() {
  if (!window.yazeDebug || !window.yazeDebug.isReady()) {
    console.log('Module not ready');
    alert('WASM module not ready');
    return;
  }
  try {
    var infoStr = await Module.getFileManagerDebugInfo();
    var info = JSON.parse(infoStr);
    console.log('=== File Manager Debug Info ===');
    console.log(JSON.stringify(info, null, 2));
    alert('File Manager Debug Info logged to console.\n\nGlobal ROM: ' + (info.global_rom_ptr ? 'Yes' : 'No') +
          '\nROM Loaded: ' + (info.rom_loaded ? 'Yes' : 'No') +
          '\nROM Size: ' + formatFileSize(info.rom_size) +
          '\nFilename: ' + (info.rom_filename || 'N/A'));
  } catch (e) {
    console.error('Error getting file manager debug info:', e);
    alert('Error: ' + e.message);
  }
}

function showRomDiagnostics() {
  if (!window.yazeDebug || !window.yazeDebug.isReady()) {
    console.log('Module not ready');
    alert('WASM module not ready');
    return;
  }
  var diag = window.yazeDebug.graphics.getDiagnostics();
  console.log('=== Graphics Diagnostics ===');
  console.log(JSON.stringify(diag, null, 2));
  if (diag.error) {
    alert('Diagnostics Error: ' + diag.error);
  } else {
    var analysis = diag.analysis || {};
    alert('Graphics Diagnostics logged to console.\n\n' +
          'ROM Size: ' + formatFileSize(diag.rom_size) + '\n' +
          'Header Stripped: ' + (diag.header_stripped ? 'Yes' : 'No') + '\n' +
          'Checksum Valid: ' + (diag.checksum_valid ? 'Yes' : 'No') + '\n' +
          'Sheets Loaded: ' + (diag.sheets ? diag.sheets.length : 0) + '\n\n' +
          'Analysis:\n' +
          '  All Sheets 0xFF: ' + (analysis.all_sheets_0xFF ? 'YES (PROBLEM!)' : 'No') + '\n' +
          '  Size Param Zero Bug: ' + (analysis.size_param_zero_bug ? 'YES (PROBLEM!)' : 'No') + '\n' +
          '  Header Misaligned: ' + (analysis.header_misaligned ? 'YES' : 'No') + '\n' +
          '  Suspected Regression: ' + (analysis.suspected_regression ? 'YES' : 'No'));
  }
}

function dumpDebugState() {
  if (!window.yazeDebug) {
    console.log('yazeDebug not available');
    return;
  }
  console.log('=== Full Debug State Dump ===');
  console.log(window.yazeDebug.dumpAll());
  console.log(window.yazeDebug.formatForAI());
  alert('Full debug state dumped to console. Open DevTools (F12) to view.');
}

function hardResetApplication() {
  if (!confirm('This will clear all cached data, unregister service workers, and reload the application. Unsaved changes may be lost. Continue?')) {
    return;
  }

  console.log('[Reset] Starting hard reset...');

  // 1. Clear Storage
  try {
    localStorage.clear();
    sessionStorage.clear();
    console.log('[Reset] Local/Session storage cleared');
  } catch (e) {
    console.error('[Reset] Storage clear failed:', e);
  }

  // 2. Clear Caches & Unregister SW
  const clearPromises = [];

  if ('caches' in window) {
    clearPromises.push(
      caches.keys().then(names => {
        return Promise.all(names.map(name => {
          console.log('[Reset] Deleting cache:', name);
          return caches.delete(name);
        }));
      })
    );
  }

  if ('serviceWorker' in navigator) {
    clearPromises.push(
      navigator.serviceWorker.getRegistrations().then(registrations => {
        return Promise.all(registrations.map(registration => {
          console.log('[Reset] Unregistering SW:', registration.scope);
          return registration.unregister();
        }));
      })
    );
  }

  // Wait for all cleanup tasks
  Promise.all(clearPromises).then(() => {
    console.log('[Reset] Cleanup complete, reloading...');
    // Force reload ignoring cache
    window.location.reload(true);
  }).catch(err => {
    console.error('[Reset] Cleanup failed:', err);
    // Fallback reload
    window.location.reload(true);
  });
}

// Update version badge from WASM when ready
function updateVersionBadge() {
  var badge = document.getElementById('version-badge');
  if (!badge) return;
  if (typeof Module !== 'undefined' && Module.getYazeVersion) {
    try {
      badge.textContent = 'v' + Module.getYazeVersion();
    } catch (e) {
      // Keep default version
    }
  }
}

// Close help menu when clicking outside
document.addEventListener('click', function(e) {
  if (!e.target.closest('.dropdown')) {
    var menu = document.getElementById('help-menu');
    if (menu) menu.classList.remove('show');
    var romMenu = document.getElementById('rom-files-menu');
    if (romMenu) romMenu.classList.remove('show');
  }
});

// About modal
function showAbout() {
  document.getElementById('about-modal').style.display = 'flex';
}
function hideAbout() {
  document.getElementById('about-modal').style.display = 'none';
}

// AI Setup Modal
function showAiSetup() {
  const modal = document.getElementById('ai-setup-modal');
  modal.style.display = 'flex';
  
  // Load existing Client ID
  let clientId = '';
  // Check config
  if (window.YAZE_CONFIG && window.YAZE_CONFIG.ai && window.YAZE_CONFIG.ai.auth) {
    clientId = window.YAZE_CONFIG.ai.auth.clientId || '';
  }
  // Check storage override
  const stored = localStorage.getItem('yaze_ai_client_id');
  if (stored) clientId = stored;
  
  document.getElementById('ai-client-id').value = clientId;
}

function hideAiSetup() {
  document.getElementById('ai-setup-modal').style.display = 'none';
}

function saveAiConfig() {
  const clientId = document.getElementById('ai-client-id').value.trim();
  if (!clientId) {
    alert('Please enter a Client ID');
    return;
  }
  
  // Save to storage
  localStorage.setItem('yaze_ai_client_id', clientId);
  
  // Update runtime config
  if (!window.YAZE_CONFIG) window.YAZE_CONFIG = {};
  if (!window.YAZE_CONFIG.ai) window.YAZE_CONFIG.ai = {};
  if (!window.YAZE_CONFIG.ai.auth) window.YAZE_CONFIG.ai.auth = {};
  
  window.YAZE_CONFIG.ai.auth.clientId = clientId;
  
  // Force update AiManager if initialized
  if (window.yaze && window.yaze.ai) {
    window.yaze.ai.authConfig.clientId = clientId;
  }
  
  alert('Configuration saved. You can now use /login in the terminal.');
  hideAiSetup();
}

// Shortcuts (uses existing overlay)
function showShortcuts() {
  if (window.yazeShortcuts) window.yazeShortcuts.show();
}

// Dungeon palette debugging
var paletteInspector = {
  refresh: function() {
    if (typeof Module === 'undefined' || typeof Module.getDungeonPaletteEvents !== 'function') return;

    var eventsStr;
    try {
      eventsStr = Module.getDungeonPaletteEvents();
    } catch (e) {
      console.warn('Failed to get palette events:', e);
      return;
    }

    // Handle empty or invalid responses - eventsStr could be "", "[]", or invalid
    if (!eventsStr || eventsStr.length === 0 || eventsStr === '[]') {
      // Show empty state in problems panel
      var list = document.getElementById('problems-list');
      if (list) {
        list.innerHTML = '<div class="problems-empty">No palette events yet. Open a dungeon room to see palette debug info.</div>';
      }
      return;
    }

    var events;
    try {
      events = JSON.parse(eventsStr);
    } catch (e) {
      // Only log parse errors for non-empty, non-bracket strings
      if (eventsStr.trim() && eventsStr.trim() !== '[]') {
        console.warn('Failed to parse palette events JSON:', e, 'Raw:', eventsStr);
      }
      return;
    }
    var list = document.getElementById('problems-list');

    if (!list) return;
    list.innerHTML = '';

    if (events.length === 0) {
      list.innerHTML = '<div class="problems-empty">No palette events yet. Open a dungeon room to see palette debug info.</div>';
      return;
    }

    events.forEach(function(event) {
      var item = document.createElement('div');
      item.className = 'problem-item';
      if (event.level === 'error') item.classList.add('error');
      if (event.level === 'warning') item.classList.add('warning');

      var icon = event.level === 'error' ? '❌' : event.level === 'warning' ? '⚠️' : 'ℹ️';

      // Build HTML with optional color swatch
      var swatchHtml = '';
      if (event.sample_rgb && event.sample_rgb.length >= 3) {
        var r = event.sample_rgb[0], g = event.sample_rgb[1], b = event.sample_rgb[2];
        swatchHtml = '<div class="color-swatch" style="background-color: rgb(' + r + ',' + g + ',' + b + ');" title="Sample color: rgb(' + r + ',' + g + ',' + b + ')"></div>';
      }

      item.innerHTML =
        '<span class="problem-icon">' + icon + '</span>' +
        '<span class="problem-location">' + event.location + '</span>' +
        '<span class="problem-message">' + event.message + '</span>' +
        swatchHtml;
      list.appendChild(item);
    });
  },

  clear: function() {
    if (typeof Module !== 'undefined' && typeof Module.clearPaletteDebugEvents === 'function') {
      Module.clearPaletteDebugEvents();
    }
    this.refresh();
  },

  autoRefresh: function() {
    setInterval(this.refresh.bind(this), 1000);
  }
};

// Keep input events inside terminal/collab/pixel-inspector from being captured by global/WASM listeners
// This must run in capture phase (true) to intercept before Emscripten
document.addEventListener('keydown', function(e) {
  var t = e.target;
  if (!t) return;

  // Check if we're in any input context that should capture keyboard
  var isInput = t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.isContentEditable;
  var inTerminal = t.closest && t.closest('#panel-terminal');
  var inCollab = t.closest && (t.closest('.yaze-collab-console') || t.closest('.yaze-chat-input'));
  var inPalette = t.closest && t.closest('#command-palette');
  var inModal = t.closest && t.closest('.modal');

  // Terminal input handles its own capture in terminal.js to preserve Enter/autocomplete
  if (inTerminal) return;

  if (isInput || inCollab || inPalette || inModal ||
      t.id === 'yaze-chat-input' || t.id === 'terminal-input' ||
      t.id === 'palette-input') {
    e.stopPropagation();
    // Also stop immediate propagation to prevent Emscripten handlers
    e.stopImmediatePropagation();
  }
}, true);

// Same for keyup and keypress events
document.addEventListener('keyup', function(e) {
  var t = e.target;
  if (!t) return;
  var isInput = t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.isContentEditable;
  if (t.closest && t.closest('#panel-terminal')) return;

  if (isInput) {
    e.stopPropagation();
    e.stopImmediatePropagation();
  }
}, true);

document.addEventListener('keypress', function(e) {
  var t = e.target;
  if (!t) return;
  var isInput = t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.isContentEditable;
  if (t.closest && t.closest('#panel-terminal')) return;

  if (isInput) {
    e.stopPropagation();
    e.stopImmediatePropagation();
  }
}, true);

// NOTE: Context menu prevention is handled by the unified system in app.js
// which uses capture phase + MutationObserver for dynamic elements.
// No additional handlers are needed here.

// Status bar helpers (called from WASM)
window.yazeStatus = {
  setRomInfo: function(name, size) {
    var statusDiv = document.getElementById('header-status');
    if (statusDiv) {
      statusDiv.textContent = name ? (name + ' (' + (size/1024).toFixed(0) + 'KB)') : 'Ready';
      statusDiv.style.color = name ? 'var(--text-accent)' : 'var(--text-muted)';
    }
  },
  setEditorMode: function(mode) {
    // Optional: append mode if needed, or replace
    // For minimalist style, we might just show the mode
    var statusDiv = document.getElementById('header-status');
    if (statusDiv && mode) {
       statusDiv.textContent = mode;
    }
  },
  setCollabStatus: function(connected, roomName) {
    var statusDiv = document.getElementById('header-status');
    if (statusDiv) {
      if (connected) {
        statusDiv.textContent = 'Collab: ' + (roomName || 'Connected');
        statusDiv.style.color = 'var(--status-success)';
      } else {
        statusDiv.textContent = 'Ready';
        statusDiv.style.color = 'var(--text-muted)';
      }
    }
  }
};
