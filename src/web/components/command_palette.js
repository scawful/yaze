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
    this.items = Array.from(this.results.querySelectorAll('.palette-item'));

    this.input.addEventListener('input', () => this.filter());
    this.input.addEventListener('keydown', (e) => this.handleKey(e));
    this.el.addEventListener('click', (e) => {
      if (e.target === this.el) this.hide();
    });
    this.items.forEach(item => {
      item.addEventListener('click', () => this.execute(item.dataset.action));
    });
  },

  show: function() {
    this.el.style.display = 'flex';
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

  execute: function(action) {
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
      // Editor switching
      case 'editor-overworld': switchToEditor('Overworld'); break;
      case 'editor-dungeon': switchToEditor('Dungeon'); break;
      case 'editor-graphics': switchToEditor('Graphics'); break;
      case 'editor-palette': switchToEditor('Palette'); break;
      case 'editor-sprite': switchToEditor('Sprite'); break;
      case 'editor-screen': switchToEditor('Screen'); break;
      case 'editor-assembly': switchToEditor('Assembly'); break;
      case 'editor-music': switchToEditor('Music'); break;
      case 'editor-message': switchToEditor('Message'); break;
      case 'editor-hex': switchToEditor('Hex'); break;
      // Emulator
      case 'emulator-show': triggerEmulatorAction('show'); break;
      case 'emulator-run': triggerEmulatorAction('run'); break;
      case 'emulator-pause': triggerEmulatorAction('pause'); break;
      case 'emulator-reset': triggerEmulatorAction('reset'); break;
      // AI Tools
      case 'ai-app-state': aiTools.getAppState(); break;
      case 'ai-editor-state': aiTools.getEditorState(); break;
      case 'ai-api-ref': aiTools.dumpAPIReference(); break;
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
