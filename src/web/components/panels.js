/**
 * VSCode-style panel system
 */
var panels = {
  container: null,
  activeTab: 'terminal',
  height: 250,

  init: function() {
    this.container = document.getElementById('panel-container');
    this.setupResize();
    this.updateToggles();
  },

  toggle: function(panel) {
    if (this.container.classList.contains('panel-collapsed')) {
      this.open(panel);
    } else if (this.activeTab === panel) {
      this.close();
    } else {
      this.switchTab(panel);
    }
  },

  open: function(panel) {
    this.container.classList.remove('panel-collapsed');
    this.container.style.height = this.height + 'px';
    if (panel) this.switchTab(panel);
    this.updateToggles();
    if ((panel || this.activeTab) === 'terminal') {
      focusTerminalInput();
    }
    this.animateResize();
  },

  close: function() {
    this.height = this.container.offsetHeight || 250;
    this.container.classList.add('panel-collapsed');
    this.updateToggles();
    this.animateResize();
  },

  animateResize: function() {
    var start = Date.now();
    var duration = 200; // Match CSS transition
    function step() {
      var now = Date.now();
      // Use scheduleResize if available (uses ResizeObserver/RAF), otherwise fallback
      if (typeof scheduleResize === 'function') {
        scheduleResize();
      } else if (typeof resizeCanvasToContainer === 'function') {
        resizeCanvasToContainer();
      }
      if (now - start < duration) {
        requestAnimationFrame(step);
      }
    }
    requestAnimationFrame(step);
  },

  switchTab: function(panel) {
    this.activeTab = panel;
    // Update tab buttons
    document.querySelectorAll('.panel-tab').forEach(function(tab) {
      tab.classList.toggle('active', tab.dataset.panel === panel);
    });
    // Update panel views
    document.querySelectorAll('.panel-view').forEach(function(view) {
      view.classList.toggle('active', view.id === 'panel-' + panel);
    });
    this.updateToggles();
    if (panel === 'terminal') {
      focusTerminalInput();
    }

    // Refresh palette inspector if problems tab is active
    if (panel === 'problems' && typeof paletteInspector !== 'undefined') {
      paletteInspector.refresh();
    }
  },

  clear: function() {
    if (this.activeTab === 'terminal') {
      var output = document.getElementById('terminal-output');
      if (output) output.innerHTML = '';
    } else if (this.activeTab === 'problems') {
      var list = document.getElementById('problems-list');
      if (list) list.innerHTML = '<div class="problems-empty">No problems detected</div>';
      this.clearProblemBadge();
    } else if (this.activeTab === 'output') {
      var log = document.getElementById('output-log');
      if (log) log.innerHTML = '';
    }
  },

  maximize: function() {
    this.container.style.height = '70vh';
    this.height = this.container.offsetHeight;
    setTimeout(resizeCanvasToContainer, 200);
  },

  updateToggles: function() {
    var isOpen = !this.container.classList.contains('panel-collapsed');
    document.querySelectorAll('.panel-toggle').forEach(function(btn) {
      var isActive = isOpen && btn.dataset.panel === panels.activeTab;
      btn.classList.toggle('active', isActive);
    });
  },

  setupResize: function() {
    var handle = document.getElementById('panel-resize-handle');
    var self = this;
    var startY, startHeight;

    handle.addEventListener('mousedown', function(e) {
      startY = e.clientY;
      startHeight = self.container.offsetHeight;
      document.body.style.cursor = 'ns-resize';
      document.addEventListener('mousemove', onMouseMove);
      document.addEventListener('mouseup', onMouseUp);
      e.preventDefault();
    });

    function onMouseMove(e) {
      var delta = startY - e.clientY;
      var newHeight = Math.min(Math.max(startHeight + delta, 100), window.innerHeight * 0.7);
      self.container.style.height = newHeight + 'px';
      requestAnimationFrame(resizeCanvasToContainer); // Resize during drag
    }

    function onMouseUp() {
      self.height = self.container.offsetHeight;
      document.body.style.cursor = '';
      document.removeEventListener('mousemove', onMouseMove);
      document.removeEventListener('mouseup', onMouseUp);
      resizeCanvasToContainer(); // Final resize
    }
  },

  // API for adding problems/output
  addProblem: function(message, type) {
    var list = document.getElementById('problems-list');
    var empty = list.querySelector('.problems-empty');
    if (empty) empty.remove();
    var item = document.createElement('div');
    item.className = 'problem-item' + (type === 'warning' ? ' warning' : type === 'info' ? ' info' : '');
    item.textContent = message;
    list.appendChild(item);
    this.updateProblemBadge();
  },

  addOutput: function(message) {
    var log = document.getElementById('output-log');
    log.textContent += message + '\n';
    log.scrollTop = log.scrollHeight;
  },

  // Update problem count badge on the Problems tab
  updateProblemBadge: function() {
    var list = document.getElementById('problems-list');
    var tab = document.querySelector('.panel-tab[data-panel="problems"]');
    if (!list || !tab) return;

    // Count problems
    var errorCount = list.querySelectorAll('.problem-item:not(.warning):not(.info)').length;
    var warningCount = list.querySelectorAll('.problem-item.warning').length;

    // Remove existing badge
    var existingBadge = tab.querySelector('.badge');
    if (existingBadge) existingBadge.remove();

    // Add badge if there are problems
    if (errorCount > 0) {
      var badge = document.createElement('span');
      badge.className = 'badge';
      badge.textContent = errorCount;
      tab.appendChild(badge);
    } else if (warningCount > 0) {
      var badge = document.createElement('span');
      badge.className = 'badge warning';
      badge.textContent = warningCount;
      tab.appendChild(badge);
    }
  },

  // Clear problem badge
  clearProblemBadge: function() {
    var tab = document.querySelector('.panel-tab[data-panel="problems"]');
    if (tab) {
      var badge = tab.querySelector('.badge');
      if (badge) badge.remove();
    }
  }
};

// Initialize panels on load
document.addEventListener('DOMContentLoaded', function() {
  panels.init();
});

// Legacy toggleTerminal for backwards compat
window.toggleTerminal = function() {
  panels.toggle('terminal');
};

function focusTerminalInput() {
  var input = document.getElementById('terminal-input');
  if (input) {
    input.focus();
    var len = input.value.length;
    if (typeof input.setSelectionRange === 'function') {
      input.setSelectionRange(len, len);
    }
  }
}
