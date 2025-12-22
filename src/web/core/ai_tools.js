/**
 * AI Tools for Gemini Antigravity
 * Provides high-level state inspection and control for AI agents.
 */
(function() {
  'use strict';

  // Helper to close all dropdown menus (copied from shell.html context)
  function closeAllDropdowns() {
    var menus = ['help-menu', 'rom-files-menu', 'editor-menu', 'emulator-menu', 'layout-menu', 'ai-tools-menu'];
    menus.forEach(function(id) {
      var menu = document.getElementById(id);
      if (menu) menu.classList.remove('show');
    });
  }

  // Helper to switch editor (uses global function if available, or implements fallback)
  function switchToEditor(editorName) {
    if (window.switchToEditor) {
      window.switchToEditor(editorName);
      return;
    }
    
    if (window.yaze && window.yaze.control && window.yaze.control.switchEditor) {
      window.yaze.control.switchEditor(editorName);
    } else if (typeof Module !== 'undefined' && Module.controlSwitchEditor) {
      Module.controlSwitchEditor(editorName);
    }
  }

  // AI Tools object for Gemini Antigravity
  var aiTools = {
    // Get full application state
    getAppState: function() {
      closeAllDropdowns();
      var state = {
        timestamp: new Date().toISOString(),
        moduleReady: typeof Module !== 'undefined' && Module.calledRun,
        yazeApiReady: !!(window.yaze && window.yaze.control),
        editor: null,
        rom: null,
        cards: null
      };

      if (window.yaze && window.yaze.control) {
        state.editor = window.yaze.control.getCurrentEditor();
        state.rom = window.yaze.control.getRomStatus ? window.yaze.control.getRomStatus() : null;
        state.cards = window.yaze.control.getVisiblePanels ? window.yaze.control.getVisiblePanels() : null;
      }

      console.log('=== AI Tools: App State ===');
      console.log(JSON.stringify(state, null, 2));
      return state;
    },

    // Get current editor state
    getEditorState: function() {
      closeAllDropdowns();
      var state = { error: 'API not ready' };

      if (window.yaze && window.yaze.editor && window.yaze.editor.getSnapshot) {
        state = window.yaze.editor.getSnapshot();
      }

      console.log('=== AI Tools: Editor State ===');
      console.log(JSON.stringify(state, null, 2));
      return state;
    },

    // List visible cards
    getVisiblePanels: function() {
      closeAllDropdowns();
      var cards = [];

      if (window.yaze && window.yaze.control && window.yaze.control.getVisiblePanels) {
        cards = window.yaze.control.getVisiblePanels();
      }

      console.log('=== AI Tools: Visible Panels ===');
      console.log(JSON.stringify(cards, null, 2));
      return cards;
    },

    // List all available cards
    getAvailablePanels: function() {
      closeAllDropdowns();
      var cards = [];

      if (window.yaze && window.yaze.control && window.yaze.control.getAvailablePanels) {
        cards = window.yaze.control.getAvailablePanels();
      }

      console.log('=== AI Tools: Available Panels ===');
      console.log(JSON.stringify(cards, null, 2));
      return cards;
    },

    // Show a card by prompting for ID
    showPanel: function() {
      closeAllDropdowns();
      var cards = this.getAvailablePanels();
      var cardList = cards.map ? cards.map(function(c) { return c.id || c; }).join(', ') : 'API not ready';
      var cardId = prompt('Enter card ID to show:\n\nAvailable: ' + cardList);

      if (cardId && window.yaze && window.yaze.control && window.yaze.control.openPanel) {
        var result = window.yaze.control.openPanel(cardId);
        console.log('Show card:', cardId, result);
      }
    },

    // Hide a card by prompting for ID
    hidePanel: function() {
      closeAllDropdowns();
      var cards = this.getVisiblePanels();
      var cardList = cards.map ? cards.map(function(c) { return c.id || c; }).join(', ') : 'API not ready';
      var cardId = prompt('Enter card ID to hide:\n\nVisible: ' + cardList);

      if (cardId && window.yaze && window.yaze.control && window.yaze.control.closePanel) {
        var result = window.yaze.control.closePanel(cardId);
        console.log('Hide card:', cardId, result);
      }
    },

    // Navigate to a specific location
    navigateTo: function() {
      closeAllDropdowns();
      var location = prompt('Enter navigation target:\n\nExamples:\n- room:5 (dungeon room)\n- map:32 (overworld map)\n- editor:Graphics');

      if (!location) return;

      var parts = location.split(':');
      if (parts.length !== 2) {
        alert('Invalid format. Use: type:id (e.g., room:5)');
        return;
      }

      var type = parts[0].toLowerCase();
      var id = parts[1];

      if (type === 'room') {
        this.jumpToRoom(parseInt(id));
      } else if (type === 'map') {
        this.jumpToMap(parseInt(id));
      } else if (type === 'editor') {
        switchToEditor(id);
      } else {
        alert('Unknown navigation type: ' + type);
      }
    },

    // Jump to dungeon room
    jumpToRoom: function(roomId) {
      if (window.yaze && window.yaze.control && window.yaze.control.triggerMenuAction) {
        // Switch to dungeon editor first
        window.yaze.control.switchEditor('Dungeon');
        // Then navigate to room (via menu action or direct call)
        // Note: We need a way to set the room ID directly. 
        // For now, we just log it, but ideally we'd have a control API for this.
        console.log('Jump to room:', roomId);
        
        // Try to use the exposed selectDungeonRoom if available (it was missing before, but maybe added?)
        if (window.yaze.selectDungeonRoom) {
            window.yaze.selectDungeonRoom(roomId);
        } else if (Module.selectDungeonRoom) {
            Module.selectDungeonRoom(roomId);
        }
      }
    },

    // Jump to overworld map
    jumpToMap: function(mapId) {
      if (window.yaze && window.yaze.control && window.yaze.control.triggerMenuAction) {
        window.yaze.control.switchEditor('Overworld');
        console.log('Jump to map:', mapId);
      }
    },

    // Get current room data (dungeon)
    getRoomData: function() {
      closeAllDropdowns();
      var data = { error: 'API not ready' };

      if (window.yaze && window.yaze.editor && window.yaze.editor.getCurrentRoom) {
        data = window.yaze.editor.getCurrentRoom();
      }

      console.log('=== AI Tools: Room Data ===');
      console.log(JSON.stringify(data, null, 2));
      return data;
    },

    // Get current map data (overworld)
    getMapData: function() {
      closeAllDropdowns();
      var data = { error: 'API not ready' };

      if (window.yaze && window.yaze.editor && window.yaze.editor.getCurrentMap) {
        data = window.yaze.editor.getCurrentMap();
      }

      console.log('=== AI Tools: Map Data ===');
      console.log(JSON.stringify(data, null, 2));
      return data;
    },

    // Dump complete API reference for AI assistants
    dumpAPIReference: function() {
      closeAllDropdowns();
      var apiRef = {
        description: 'YAZE Web Control API Reference',
        namespace: 'window.yaze',
        modules: {
          control: {
            description: 'Editor and UI control functions',
            functions: [
              'switchEditor(name) - Switch to editor by name',
              'getCurrentEditor() - Get current active editor',
              'getAvailableEditors() - List all editors',
              'openPanel(id) - Show a card panel',
              'closePanel(id) - Hide a card panel',
              'togglePanel(id) - Toggle card visibility',
              'getVisiblePanels() - List open cards',
              'getAvailablePanels() - List all cards',
              'setPanelLayout(name) - Apply layout preset',
              'getAvailableLayouts() - List layout presets',
              'triggerMenuAction(path) - Trigger menu action',
              'getRomStatus() - Get ROM loaded state',
              'saveRom() - Save ROM changes'
            ]
          },
          editor: {
            description: 'Editor state access',
            functions: [
              'getSnapshot() - Get overall editor state',
              'getCurrentRoom() - Get dungeon room info',
              'getCurrentMap() - Get overworld map info',
              'getSelection() - Get selected objects'
            ]
          },
          data: {
            description: 'Read-only data access',
            functions: [
              'getRoomTiles(roomId) - Get room tile data',
              'getRoomObjects(roomId) - Get room objects',
              'getRoomProperties(roomId) - Get room metadata',
              'getMapTiles(mapId) - Get map tile data',
              'getMapEntities(mapId) - Get map entities',
              'getMapProperties(mapId) - Get map metadata',
              'getPalette(group, id) - Get palette colors'
            ]
          },
          gui: {
            description: 'GUI Automation',
            functions: [
              'discover() - Get UI element tree',
              'click(target) - Click element or coords',
              'takeScreenshot() - Capture canvas',
              'getSelection() - Get selected items',
              'setSelection(ids) - Set selection'
            ]
          }
        },
        editors: [
          'Overworld', 'Dungeon', 'Graphics', 'Palette',
          'Sprite', 'Screen', 'Assembly', 'Music',
          'Message', 'Hex', 'Settings', 'Agent', 'Emulator'
        ],
        layouts: [
          'overworld_default', 'dungeon_default', 'graphics_default',
          'debug_default', 'minimal', 'all_cards'
        ]
      };

      console.log('=== AI Tools: API Reference ===');
      console.log(JSON.stringify(apiRef, null, 2));

      // Also copy to clipboard if available
      if (navigator.clipboard) {
        navigator.clipboard.writeText(JSON.stringify(apiRef, null, 2))
          .then(function() { console.log('API reference copied to clipboard'); })
          .catch(function() {});
      }

      return apiRef;
    }
  };

  // Expose aiTools globally for easy access
  window.aiTools = aiTools;
  console.log('[yaze] AI Tools loaded (window.aiTools)');

})();
