/**
 * Command palette (Ctrl+K)
 */
var palette = {
  el: null,
  input: null,
  results: null,
  selectedIndex: 0,
  items: [],

  init: function() {
    this.el = document.getElementById('command-palette');
    this.input = document.getElementById('palette-input');
    this.results = document.getElementById('palette-results');

    this.input.addEventListener('input', () => this.filter());
    this.input.addEventListener('keydown', (e) => this.handleKey(e));
    this.el.addEventListener('click', (e) => {
      if (e.target === this.el) this.hide();
    });
    this.results.addEventListener('click', (e) => {
      var item = e.target.closest('.palette-item');
      if (!item) return;
      this.execute(item.dataset.action, item.dataset.arg || '');
    });
    this.refreshItems();
  },

  refreshItems: function() {
    this.items = Array.from(this.results.querySelectorAll('.palette-item'));
  },

  getFallbackEditors: function() {
    return [
      { name: 'Overworld' },
      { name: 'Dungeon' },
      { name: 'Graphics' },
      { name: 'Palette' },
      { name: 'Sprite' },
      { name: 'Screen' },
      { name: 'Assembly' },
      { name: 'Music' },
      { name: 'Message' },
      { name: 'Hex' },
      { name: 'Settings' }
    ];
  },

  getAvailableEditors: function() {
    if (typeof getAvailableEditorsForShell === 'function') {
      return getAvailableEditorsForShell();
    }

    var control = window.yaze && window.yaze.control ? window.yaze.control : null;
    if (control && typeof control.getAvailableEditors === 'function') {
      try {
        var editors = control.getAvailableEditors();
        if (Array.isArray(editors) && editors.length > 0) {
          return editors;
        }
      } catch (e) {
        console.warn('Failed to query editors for palette:', e);
      }
    }

    return this.getFallbackEditors();
  },

  getAvailableWindows: function() {
    var control = window.yaze && window.yaze.control ? window.yaze.control : null;
    if (control && typeof control.getAvailableWindows === 'function') {
      try {
        var windows = control.getAvailableWindows();
        if (Array.isArray(windows)) {
          return windows.filter(function(windowInfo) {
            return windowInfo && windowInfo.id && windowInfo.display_name &&
                   windowInfo.enabled !== false;
          });
        }
      } catch (e) {
        console.warn('Failed to query windows for palette:', e);
      }
    }
    return [];
  },

  removeDynamicItems: function() {
    this.results.querySelectorAll('.palette-item[data-dynamic="true"]').forEach(function(item) {
      item.remove();
    });
  },

  appendDynamicItem: function(label, action, arg) {
    var item = document.createElement('div');
    item.className = 'palette-item';
    item.dataset.dynamic = 'true';
    item.dataset.action = action;
    if (arg) {
      item.dataset.arg = arg;
    }
    item.textContent = label;
    this.results.appendChild(item);
  },

  refreshDynamicItems: function() {
    this.removeDynamicItems();

    this.getAvailableEditors().forEach((editor) => {
      if (!editor || !editor.name) return;
      this.appendDynamicItem('Editor: ' + editor.name, 'editor-switch', editor.name);
    });

    this.getAvailableWindows().forEach((windowInfo) => {
      var prefix = windowInfo.visible ? 'Window: Focus ' : 'Window: Show ';
      this.appendDynamicItem(prefix + windowInfo.display_name,
          'window-open', windowInfo.id);
    });

    this.refreshItems();
  },

  show: function() {
    this.el.style.display = 'flex';
    this.refreshDynamicItems();
    this.input.value = '';
    this.filter();
    this.input.focus();
  },

  hide: function() {
    this.el.style.display = 'none';
  },

  filter: function() {
    var query = this.input.value.toLowerCase();
    this.items.forEach(item => {
      var match = item.textContent.toLowerCase().includes(query);
      item.classList.toggle('hidden', !match);
    });
    this.selectedIndex = 0;
    this.updateSelection();
  },

  updateSelection: function() {
    var visible = this.items.filter(i => !i.classList.contains('hidden'));
    visible.forEach((item, i) => {
      item.classList.toggle('selected', i === this.selectedIndex);
    });
  },

  handleKey: function(e) {
    var visible = this.items.filter(i => !i.classList.contains('hidden'));
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      this.selectedIndex = Math.min(this.selectedIndex + 1, visible.length - 1);
      this.updateSelection();
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      this.selectedIndex = Math.max(this.selectedIndex - 1, 0);
      this.updateSelection();
    } else if (e.key === 'Enter') {
      e.preventDefault();
      if (visible[this.selectedIndex]) {
        this.execute(visible[this.selectedIndex].dataset.action);
      }
    } else if (e.key === 'Escape') {
      this.hide();
    }
  },

  execute: function(action, arg) {
    this.hide();
    switch(action) {
      case 'open-rom': document.getElementById('rom-input').click(); break;
      case 'save': if(Module && Module._saveRom) Module._saveRom(); break;
      case 'settings': showSettings(); break;
      case 'fullscreen': window.toggleFullscreen(); break;
      case 'terminal': toggleTerminal(); break;
      case 'collab': toggleCollabConsole(); break;
      case 'shortcuts': showShortcuts(); break;
      case 'theme-default': setTheme('default'); break;
      case 'theme-halext': setTheme('halext'); break;
      case 'docs': window.open('./docs/', '_blank'); break;
      case 'about': showAbout(); break;
      case 'file-manager': fileManager.show(); break;
      // Emulator
      case 'emulator-show': triggerEmulatorAction('show'); break;
      case 'emulator-run': triggerEmulatorAction('run'); break;
      case 'emulator-pause': triggerEmulatorAction('pause'); break;
      case 'emulator-reset': triggerEmulatorAction('reset'); break;
      // AI Tools
      case 'ai-app-state': aiTools.getAppState(); break;
      case 'ai-editor-state': aiTools.getEditorState(); break;
      case 'ai-api-ref': aiTools.dumpAPIReference(); break;
      case 'editor-switch':
        if (arg) switchToEditor(arg);
        break;
      case 'window-open':
        if (window.yaze && window.yaze.control && typeof window.yaze.control.openWindow === 'function') {
          window.yaze.control.openWindow(arg);
        } else if (window.showYazeToast) {
          window.showYazeToast('Window API not ready yet', 'warning');
        }
        break;
    }
  }
};

// Initialize palette on load
document.addEventListener('DOMContentLoaded', function() {
  palette.init();
});

// Global keyboard shortcuts
document.addEventListener('keydown', function(e) {
  if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
    e.preventDefault();
    palette.show();
  }
});
