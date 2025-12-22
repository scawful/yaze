/**
 * @file debug.js
 * @brief YAZE Debug API (window.yazeDebug)
 * 
 * Consolidated debug interface for AI assistants, automation, and developer tools.
 * Wraps WASM/C++ inspection functions exposed via Embind.
 */

(function() {
  'use strict';

  // Initialize global namespace
  window.yazeDebug = window.yazeDebug || {};

  // ==========================================================================
  // Core Status & Metadata
  // ==========================================================================
  
  Object.assign(window.yazeDebug, {
    version: '1.3.0',
    capabilities: [
      'palette', 'arena', 'graphics', 'timeline', 'pixel-inspector',
      'rom', 'overworld', 'emulator', 'editor', 'workers',
      'cards', 'cards.discover', 'cards.search', 'cards.events',
      'layouts', 'layouts.save', 'layouts.custom',
      'sidebar', 'rightPanel', 'themes'
    ],

    /**
     * Check if WASM module is ready and responsive
     * @returns {boolean}
     */
    isReady: function() {
      return typeof Module !== 'undefined' && Module.calledRun;
    }
  });

  // ==========================================================================
  // Worker Diagnostics
  // ==========================================================================
  
  window.yazeDebug.workers = {
    getStatus: function() {
      return {
        spawnCount: window.YAZE_WORKER_SPAWN_COUNT || 0,
        spawnLimit: window.YAZE_WORKER_SPAWN_LIMIT || 12,
        errorCount: window.YAZE_WORKER_ERRORS || 0,
        aborted: window.YAZE_WORKER_ABORTED || false,
        lastSpawn: window.YAZE_WORKER_LAST_SPAWN || 0,
        timeSinceLastSpawn: window.YAZE_WORKER_LAST_SPAWN ? (Date.now() - window.YAZE_WORKER_LAST_SPAWN) + 'ms' : 'never',
        sharedArrayBuffer: typeof SharedArrayBuffer !== 'undefined',
        crossOriginIsolated: window.crossOriginIsolated === true,
        serviceWorkerController: !!navigator.serviceWorker?.controller,
        hardwareConcurrency: navigator.hardwareConcurrency || 'unknown'
      };
    },
    reset: function() {
      window.YAZE_WORKER_SPAWN_COUNT = 0;
      window.YAZE_WORKER_ERRORS = 0;
      window.YAZE_WORKER_ABORTED = false;
      console.log('[WORKER] Counters reset');
      return this.getStatus();
    }
  };

  // ==========================================================================
  // Editor Control & State
  // ==========================================================================

  // Basic editor wrappers
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
    return { error: 'Module not ready' };
  };

  window.yazeDebug.setAgentMode = function(enabled) {
    if (window.Module && window.Module.executeCommand) {
      console.warn('[yazeDebug] setAgentMode not implemented yet');
      return { error: 'Not implemented' };
    }
    return { error: 'Module not ready' };
  };

  window.yazeDebug.loadRomFromFile = function(filepath) {
    console.log('[yazeDebug] Loading ROM from file:', filepath);
    if (typeof FilesystemManager !== 'undefined' && FilesystemManager._executeRomLoad) {
      FilesystemManager._executeRomLoad(filepath, null);
      return { success: true, message: 'ROM load initiated' };
    }
    return { error: 'FilesystemManager not ready' };
  };

  // Async editor switching
  window.yazeDebug.switchToEditorAsync = function(name) {
    return new Promise((resolve, reject) => {
      if (!window.Module || !window.Module.switchToEditorAsync) {
        reject(new Error('Module not ready'));
        return;
      }

      try {
        const result = JSON.parse(window.Module.switchToEditorAsync(name));

        if (result.error) {
          reject(new Error(result.error));
          return;
        }

        const opId = result.op_id;
        let pollCount = 0;
        const maxPolls = 300; // 5 seconds at 60fps

        const pollInterval = setInterval(() => {
          pollCount++;

          try {
            const status = JSON.parse(window.Module.getOperationStatus(opId));

            if (status.status === 'completed') {
              clearInterval(pollInterval);
              resolve({ success: true, editor: status.editor || name });
            } else if (status.status === 'error') {
              clearInterval(pollInterval);
              reject(new Error(status.error || 'Unknown error'));
            } else if (pollCount >= maxPolls) {
              clearInterval(pollInterval);
              reject(new Error('Operation timed out after 5 seconds'));
            }
          } catch (e) {
            clearInterval(pollInterval);
            reject(new Error('Failed to get operation status: ' + e.message));
          }
        }, 16); // Poll each frame (~60fps)

      } catch (e) {
        reject(new Error('switchToEditorAsync failed: ' + e.message));
      }
    });
  };

  // Detailed editor state (from shell.html)
  window.yazeDebug.editor = {
    getState: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getEditorState()); } catch(e) { return { error: e.message }; }
    },
    executeCommand: function(cmd) {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      return Module.executeCommand(cmd);
    }
  };

  // ==========================================================================
  // UI Component Control (Panels, Sidebar, Panels)
  // ==========================================================================

  window.yazeDebug.cards = {
    show: function(cardId) {
      if (!window.Module || !window.Module.showPanel) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.showPanel(cardId)); } catch (e) { return { error: e.message }; }
    },
    hide: function(cardId) {
      if (!window.Module || !window.Module.hidePanel) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.hidePanel(cardId)); } catch (e) { return { error: e.message }; }
    },
    toggle: function(cardId) {
      if (!window.Module || !window.Module.togglePanel) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.togglePanel(cardId)); } catch (e) { return { error: e.message }; }
    },
    getState: function() {
      if (!window.Module || !window.Module.getPanelState) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.getPanelState()); } catch (e) { return { error: e.message }; }
    },
    getInCategory: function(category) {
      if (!window.Module || !window.Module.getPanelsInCategory) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.getPanelsInCategory(category)); } catch (e) { return { error: e.message }; }
    },
    showGroup: function(groupName) {
      if (!window.Module || !window.Module.showPanelGroup) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.showPanelGroup(groupName)); } catch (e) { return { error: e.message }; }
    },
    hideGroup: function(groupName) {
      if (!window.Module || !window.Module.hidePanelGroup) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.hidePanelGroup(groupName)); } catch (e) { return { error: e.message }; }
    },
    getGroups: function() {
      if (!window.Module || !window.Module.getPanelGroups) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.getPanelGroups()); } catch (e) { return { error: e.message }; }
    },

    // Panel Discovery API
    discover: function() {
      // Get all available cards with full metadata
      var result = {
        timestamp: Date.now(),
        categories: {},
        allPanels: []
      };

      if (!window.Module || !window.Module.getPanelState) {
        return { error: 'Module not ready' };
      }

      try {
        var state = JSON.parse(window.Module.getPanelState());
        if (state.cards) {
          result.allPanels = state.cards;
          // Group by category
          state.cards.forEach(function(card) {
            var cat = card.category || 'Other';
            if (!result.categories[cat]) {
              result.categories[cat] = [];
            }
            result.categories[cat].push(card);
          });
        }
        return result;
      } catch (e) {
        return { error: e.message };
      }
    },

    // Find cards by pattern
    search: function(pattern) {
      var discovered = this.discover();
      if (discovered.error) return discovered;

      var regex = new RegExp(pattern, 'i');
      var matches = discovered.allPanels.filter(function(card) {
        return regex.test(card.id) ||
               regex.test(card.name) ||
               regex.test(card.category || '');
      });

      return { pattern: pattern, matches: matches, count: matches.length };
    },

    // Get visible cards only
    getVisible: function() {
      var discovered = this.discover();
      if (discovered.error) return discovered;

      return discovered.allPanels.filter(function(card) {
        return card.visible === true;
      });
    },

    // Event subscription (stores callbacks, emits when C++ calls back)
    _visibilityCallbacks: [],
    onVisibilityChange: function(callback) {
      if (typeof callback === 'function') {
        this._visibilityCallbacks.push(callback);
      }
    },
    _emitVisibilityChange: function(cardId, visible) {
      this._visibilityCallbacks.forEach(function(cb) {
        try { cb({ cardId: cardId, visible: visible }); } catch (e) { console.error('Panel callback error:', e); }
      });
    }
  };

  // ==========================================================================
  // Layout Presets API
  // ==========================================================================

  window.yazeDebug.layouts = {
    // Get available layout presets
    list: function() {
      // Built-in presets (these match the C++ defaults)
      var presets = [
        { name: 'overworld_default', description: 'Default overworld editing layout', cards: ['map_selector', 'tile_selector', 'entity_editor'] },
        { name: 'dungeon_default', description: 'Default dungeon editing layout', cards: ['room_selector', 'object_editor', 'tile_selector'] },
        { name: 'graphics_default', description: 'Default graphics editing layout', cards: ['sheet_viewer', 'palette_editor', 'tile_editor'] },
        { name: 'debug_default', description: 'Debug-focused layout', cards: ['emulator', 'memory_viewer', 'disassembly'] },
        { name: 'minimal', description: 'Minimal canvas-only layout', cards: [] },
        { name: 'all_cards', description: 'Show all available cards', cards: 'all' }
      ];

      // Try to get from C++ if available
      if (window.Module && window.Module.getAvailableLayouts) {
        try {
          var cppLayouts = JSON.parse(window.Module.getAvailableLayouts());
          if (cppLayouts && cppLayouts.presets) {
            presets = cppLayouts.presets;
          }
        } catch (e) {
          console.warn('Could not get layouts from C++:', e);
        }
      }

      return { presets: presets };
    },

    // Apply a layout preset
    apply: function(presetName) {
      if (window.Module && window.Module.setPanelLayout) {
        try {
          return JSON.parse(window.Module.setPanelLayout(presetName));
        } catch (e) {
          return { error: e.message };
        }
      }

      // Fallback: manual card show/hide
      var presets = this.list().presets;
      var preset = presets.find(function(p) { return p.name === presetName; });

      if (!preset) {
        return { error: 'Preset not found: ' + presetName };
      }

      // Hide all then show preset cards
      if (preset.cards === 'all') {
        // Show all available cards
        var discovered = window.yazeDebug.cards.discover();
        if (!discovered.error) {
          discovered.allPanels.forEach(function(card) {
            window.yazeDebug.cards.show(card.id);
          });
        }
      } else {
        // Apply specific preset
        preset.cards.forEach(function(cardId) {
          window.yazeDebug.cards.show(cardId);
        });
      }

      return { success: true, applied: presetName };
    },

    // Save current layout as a preset (localStorage)
    save: function(name) {
      var visible = window.yazeDebug.cards.getVisible();
      if (!Array.isArray(visible)) {
        return { error: 'Could not get visible cards' };
      }

      var preset = {
        name: name,
        description: 'Custom layout saved at ' + new Date().toISOString(),
        cards: visible.map(function(c) { return c.id; }),
        savedAt: Date.now()
      };

      // Load existing custom presets
      var customPresets = [];
      try {
        var stored = localStorage.getItem('yaze-custom-layouts');
        if (stored) {
          customPresets = JSON.parse(stored);
        }
      } catch (e) { /* ignore */ }

      // Update or add
      var existingIndex = customPresets.findIndex(function(p) { return p.name === name; });
      if (existingIndex >= 0) {
        customPresets[existingIndex] = preset;
      } else {
        customPresets.push(preset);
      }

      localStorage.setItem('yaze-custom-layouts', JSON.stringify(customPresets));
      return { success: true, preset: preset };
    },

    // Get custom saved layouts
    getCustom: function() {
      try {
        var stored = localStorage.getItem('yaze-custom-layouts');
        return stored ? JSON.parse(stored) : [];
      } catch (e) {
        return [];
      }
    },

    // Delete a custom layout
    deleteCustom: function(name) {
      var customPresets = this.getCustom();
      var filtered = customPresets.filter(function(p) { return p.name !== name; });
      localStorage.setItem('yaze-custom-layouts', JSON.stringify(filtered));
      return { success: true, deleted: name };
    }
  };

  window.yazeDebug.sidebar = {
    isTreeView: function() {
      if (!window.Module || !window.Module.isTreeViewMode) return false;
      return window.Module.isTreeViewMode();
    },
    setTreeView: function(treeView) {
      if (!window.Module || !window.Module.setTreeViewMode) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.setTreeViewMode(treeView)); } catch (e) { return { error: e.message }; }
    },
    toggle: function() {
      if (!window.Module || !window.Module.toggleTreeViewMode) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.toggleTreeViewMode()); } catch (e) { return { error: e.message }; }
    },
    getState: function() {
      if (!window.Module || !window.Module.getSidebarState) return { available: false };
      try { return JSON.parse(window.Module.getSidebarState()); } catch (e) { return { error: e.message }; }
    }
  };

  window.yazeDebug.rightPanel = {
    open: function(panelName) {
      if (!window.Module || !window.Module.openRightPanel) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.openRightPanel(panelName)); } catch (e) { return { error: e.message }; }
    },
    close: function() {
      if (!window.Module || !window.Module.closeRightPanel) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.closeRightPanel()); } catch (e) { return { error: e.message }; }
    },
    toggle: function(panelName) {
      if (!window.Module || !window.Module.toggleRightPanel) return { error: 'Module not ready' };
      try { return JSON.parse(window.Module.toggleRightPanel(panelName)); } catch (e) { return { error: e.message }; }
    },
    getState: function() {
      if (!window.Module || !window.Module.getRightPanelState) return { available: false };
      try { return JSON.parse(window.Module.getRightPanelState()); } catch (e) { return { error: e.message }; }
    },
    openProperties: function() { return this.open('properties'); },
    openAgent: function() { return this.open('agent'); }
  };

  // ==========================================================================
  // Emulator & System Debugging
  // ==========================================================================

  window.yazeDebug.emulator = {
    getStatus: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getEmulatorStatus()); } catch(e) { return { error: e.message }; }
    },
    readMemory: function(addr, count) {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.readEmulatorMemory(addr, count)); } catch(e) { return { error: e.message }; }
    },
    getVideoState: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getEmulatorVideoState()); } catch(e) { return { error: e.message }; }
    }
  };

  window.yazeDebug.graphics = {
    getDiagnostics: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getGraphicsDiagnostics()); } catch(e) { return { error: e.message }; }
    },
    detect0xFFPattern: function() {
       var diag = this.getDiagnostics();
       if (diag.error) return false;
       return diag.analysis && diag.analysis.all_sheets_0xFF;
    }
  };

  window.yazeDebug.palette = {
    getEvents: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getDungeonPaletteEvents()); } catch (e) { return { error: e.message }; }
    },
    getFullState: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getFullPaletteState()); } catch (e) { return { error: e.message }; }
    },
    getData: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getPaletteData()); } catch (e) { return { error: e.message }; }
    },
    getComparisons: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getColorComparisons()); } catch (e) { return { error: e.message }; }
    },
    samplePixel: function(x, y) {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.samplePixelAt(x, y)); } catch (e) { return { error: e.message }; }
    },
    clear: function() {
      if (!window.yazeDebug.isReady()) return;
      Module.clearPaletteDebugEvents();
    }
  };

  window.yazeDebug.arena = {
    getStatus: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getArenaStatus()); } catch (e) { return { error: e.message }; }
    },
    getSheetInfo: function(index) {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getGfxSheetInfo(index)); } catch (e) { return { error: e.message }; }
    }
  };

  window.yazeDebug.rom = {
    getStatus: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getRomStatus()); } catch (e) { return { error: e.message }; }
    },
    readBytes: function(address, count) {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try {
        count = count || 16;
        return JSON.parse(Module.readRomBytes(address, count));
      } catch (e) {
        return { error: e.message };
      }
    },
    getPaletteGroup: function(groupId, paletteIndex) {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try {
        return JSON.parse(Module.getRomPaletteGroup(groupId, paletteIndex));
      } catch (e) {
        return { error: e.message };
      }
    }
  };

  window.yazeDebug.overworld = {
    getMapInfo: function(mapId) {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getOverworldMapInfo(mapId)); } catch (e) { return { error: e.message }; }
    },
    getTileInfo: function(mapId, tileX, tileY) {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getOverworldTileInfo(mapId, tileX, tileY)); } catch (e) { return { error: e.message }; }
    }
  };

  // ==========================================================================
  // Analysis Tools
  // ==========================================================================

  window.yazeDebug.timeline = {
    get: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getEventTimeline()); } catch (e) { return { error: e.message }; }
    }
  };

  window.yazeDebug.analysis = {
    getSummary: function() {
      if (!window.yazeDebug.isReady()) return 'Module not ready';
      try { return Module.getDiagnosticSummary(); } catch (e) { return 'Error: ' + e.message; }
    },
    getHypothesis: function() {
      if (!window.yazeDebug.isReady()) return 'Module not ready';
      try { return Module.getHypothesisAnalysis(); } catch (e) { return 'Error: ' + e.message; }
    },
    getFullState: function() {
      if (!window.yazeDebug.isReady()) return { error: 'Module not ready' };
      try { return JSON.parse(Module.getFullDebugState()); } catch (e) { return { error: e.message }; }
    }
  };

  // ==========================================================================
  // Convenience Methods
  // ==========================================================================

  // Get everything for quick AI analysis
  window.yazeDebug.dumpAll = function() {
    return {
      ready: this.isReady(),
      timestamp: Date.now(),
      palette: this.palette.getFullState(),
      arena: this.arena.getStatus(),
      graphics: this.graphics.getDiagnostics(),
      emulator: this.emulator.getStatus(),
      editor: this.editor.getState(),
      rom: this.rom.getStatus(),
      timeline: this.timeline.get(),
      summary: this.analysis.getSummary(),
      hypothesis: this.analysis.getHypothesis()
    };
  };

  // Format state as human-readable string for AI prompts
  window.yazeDebug.formatForAI = function() {
    var state = this.dumpAll();
    if (!state.ready) return 'WASM module not ready. Please load a ROM first.';

    var lines = [
      '=== YAZE Debug State ===',
      'Timestamp: ' + new Date(state.timestamp).toISOString(),
      '',
      '--- ROM Status ---',
      JSON.stringify(state.rom, null, 2),
      '',
      '--- Diagnostic Summary ---',
      state.summary,
      '',
      '--- Hypothesis Analysis ---',
      state.hypothesis,
      '',
      '--- Timeline ---',
      JSON.stringify(state.timeline, null, 2),
      '',
      '--- Arena Status ---',
      JSON.stringify(state.arena, null, 2)
    ];

    return lines.join('\n');
  };

  // ==========================================================================
  // Legacy / Convenience Helpers
  // ==========================================================================

  // Helper for room selection (used by ai_tools.js)
  window.yaze = window.yaze || {};
  window.yaze.selectDungeonRoom = function(roomId) {
    console.log('[yaze] Selecting dungeon room:', roomId);
    if (window.yazeDebug && window.yazeDebug.switchToEditorAsync) {
      window.yazeDebug.switchToEditorAsync('Dungeon').then(() => {
         // TODO: Actually select the room once editor is active
         // We might need to expose a C++ function for this.
         console.warn('[yaze] Room selection logic pending C++ implementation');
      });
    } else {
      console.warn('[yaze] yazeDebug API not ready');
    }
  };

  console.log('[debug.js] yazeDebug API initialized');
})();
