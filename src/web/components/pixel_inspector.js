/**
 * Pixel Inspector for debugging palette issues
 */
var pixelInspector = {
  enabled: false,
  el: null,

  init: function() {
    this.el = document.getElementById('pixel-inspector');
    var canvas = document.getElementById('canvas');
    if (canvas) {
      canvas.addEventListener('mousemove', this.onMouseMove.bind(this));
      canvas.addEventListener('click', this.onClick.bind(this));
    }
  },

  toggle: function() {
    this.enabled = !this.enabled;
    if (this.el) {
      this.el.style.display = this.enabled ? 'block' : 'none';
    }
    // Update cursor on canvas
    var canvas = document.getElementById('canvas');
    if (canvas) {
      canvas.style.cursor = this.enabled ? 'crosshair' : '';
    }
  },

  onMouseMove: function(e) {
    if (!this.enabled) return;
    if (typeof Module === 'undefined' || typeof Module.samplePixelAt !== 'function') return;

    var canvas = document.getElementById('canvas');
    if (!canvas) return;

    var rect = canvas.getBoundingClientRect();
    var x = Math.floor(e.clientX - rect.left);
    var y = Math.floor(e.clientY - rect.top);

    // Call WASM to sample pixel
    var resultStr = Module.samplePixelAt(x, y);
    if (!resultStr) return;

    try {
      var result = JSON.parse(resultStr);
      this.updateDisplay(result);
    } catch (err) {
      console.error('Pixel inspector parse error:', err);
    }
  },

  onClick: function(e) {
    if (!this.enabled) return;
    // On click, log the current pixel info to console for easy copying
    var xyEl = document.getElementById('inspector-xy');
    var indexEl = document.getElementById('inspector-index');
    var actualEl = document.getElementById('inspector-actual-rgb');
    var expectedEl = document.getElementById('inspector-expected-rgb');
    console.log('Pixel Inspector Snapshot:');
    console.log('  Position:', xyEl ? xyEl.textContent : '-');
    console.log('  Index:', indexEl ? indexEl.textContent : '-');
    console.log('  Actual:', actualEl ? actualEl.textContent : '-');
    console.log('  Expected:', expectedEl ? expectedEl.textContent : '-');
  },

  updateDisplay: function(result) {
    var xyEl = document.getElementById('inspector-xy');
    var indexEl = document.getElementById('inspector-index');
    var actualRgbEl = document.getElementById('inspector-actual-rgb');
    var expectedRgbEl = document.getElementById('inspector-expected-rgb');
    var actualSwatchEl = document.getElementById('inspector-actual-swatch');
    var expectedSwatchEl = document.getElementById('inspector-expected-swatch');
    var statusEl = document.getElementById('inspector-status');

    if (xyEl) xyEl.textContent = result.x + ', ' + result.y;
    if (indexEl) indexEl.textContent = result.palette_index;

    if (result.actual && result.actual.length >= 3) {
      var ar = result.actual[0], ag = result.actual[1], ab = result.actual[2];
      if (actualRgbEl) actualRgbEl.textContent = 'rgb(' + ar + ', ' + ag + ', ' + ab + ')';
      if (actualSwatchEl) actualSwatchEl.style.backgroundColor = 'rgb(' + ar + ',' + ag + ',' + ab + ')';
    }

    if (result.expected && result.expected.length >= 3) {
      var er = result.expected[0], eg = result.expected[1], eb = result.expected[2];
      if (expectedRgbEl) expectedRgbEl.textContent = 'rgb(' + er + ', ' + eg + ', ' + eb + ')';
      if (expectedSwatchEl) expectedSwatchEl.style.backgroundColor = 'rgb(' + er + ',' + eg + ',' + eb + ')';
    }

    if (statusEl) {
      if (result.matches) {
        statusEl.textContent = '✓ Match';
        statusEl.style.color = 'var(--status-success, #4caf50)';
      } else {
        statusEl.textContent = '✗ Mismatch';
        statusEl.style.color = 'var(--status-error, #f44336)';
      }
    }
  }
};

// Initialize pixel inspector on load
document.addEventListener('DOMContentLoaded', function() {
  pixelInspector.init();
});
