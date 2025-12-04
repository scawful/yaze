/**
 * GUI Automation Layer for LLM Agents with DOM access
 *
 * Provides programmatic control over the ImGui-based yaze editor interface.
 * This layer bridges JavaScript and the WASM module to enable automated
 * UI interactions for testing, scripting, and AI agent integration.
 *
 * Usage:
 *   window.yaze.gui.discover()      - List all interactive UI elements
 *   window.yaze.gui.click(target)   - Click an element by ID or coordinates
 *   window.yaze.gui.takeScreenshot() - Capture canvas as base64 PNG
 */

(function() {
  'use strict';

  // Ensure yaze namespace exists
  if (typeof window.yaze === 'undefined') {
    window.yaze = {};
  }

  /**
   * GUI Automation API
   */
  window.yaze.gui = {
    /**
     * Discover clickable UI elements within the ImGui canvas
     * Returns element IDs, positions, and descriptions
     * @returns {Object} List of UI elements with metadata
     */
    discover: function() {
      if (typeof Module === 'undefined' || !Module.guiGetUIElementTree) {
        return { error: 'Module not ready', elements: [] };
      }

      try {
        const result = Module.guiGetUIElementTree();
        return JSON.parse(result);
      } catch (e) {
        console.error('[yaze.gui] discover error:', e);
        return { error: e.message, elements: [] };
      }
    },

    /**
     * Simulate a click at coordinates or on an element by ID
     * @param {string|Object} target - Element ID string or {x, y} coordinates
     * @returns {Object} Result with success status and click location
     */
    click: function(target) {
      if (typeof target === 'string') {
        // Element ID - resolve to coordinates
        const elem = this.getElementBounds(target);
        if (!elem || elem.error) {
          return { error: 'Element not found: ' + target };
        }
        target = {
          x: elem.x + elem.width / 2,
          y: elem.y + elem.height / 2
        };
      }

      if (!target || typeof target.x !== 'number' || typeof target.y !== 'number') {
        return { error: 'Invalid target - must be element ID or {x, y} object' };
      }

      const canvas = document.getElementById('canvas');
      if (!canvas) {
        return { error: 'Canvas element not found' };
      }

      // Get canvas position for proper coordinate mapping
      const rect = canvas.getBoundingClientRect();
      const clientX = rect.left + target.x;
      const clientY = rect.top + target.y;

      // Dispatch mouse events sequence
      const eventTypes = ['mousedown', 'mouseup', 'click'];
      eventTypes.forEach(type => {
        const event = new MouseEvent(type, {
          clientX: clientX,
          clientY: clientY,
          bubbles: true,
          cancelable: true,
          view: window,
          button: 0
        });
        canvas.dispatchEvent(event);
      });

      return {
        success: true,
        x: target.x,
        y: target.y,
        clientX: clientX,
        clientY: clientY
      };
    },

    /**
     * Simulate a double-click at coordinates or on an element
     * @param {string|Object} target - Element ID string or {x, y} coordinates
     * @returns {Object} Result with success status
     */
    doubleClick: function(target) {
      const firstClick = this.click(target);
      if (firstClick.error) return firstClick;

      // Dispatch dblclick event
      const canvas = document.getElementById('canvas');
      if (canvas) {
        const event = new MouseEvent('dblclick', {
          clientX: firstClick.clientX,
          clientY: firstClick.clientY,
          bubbles: true,
          cancelable: true,
          view: window,
          button: 0
        });
        canvas.dispatchEvent(event);
      }

      return { success: true, x: firstClick.x, y: firstClick.y };
    },

    /**
     * Simulate a drag operation
     * @param {Object} from - Starting {x, y} coordinates
     * @param {Object} to - Ending {x, y} coordinates
     * @param {number} steps - Number of intermediate steps (default 10)
     * @returns {Object} Result with success status
     */
    drag: function(from, to, steps) {
      steps = steps || 10;

      const canvas = document.getElementById('canvas');
      if (!canvas) {
        return { error: 'Canvas element not found' };
      }

      const rect = canvas.getBoundingClientRect();

      // Mouse down at start
      canvas.dispatchEvent(new MouseEvent('mousedown', {
        clientX: rect.left + from.x,
        clientY: rect.top + from.y,
        bubbles: true,
        button: 0
      }));

      // Intermediate move events
      for (let i = 1; i <= steps; i++) {
        const progress = i / steps;
        const x = from.x + (to.x - from.x) * progress;
        const y = from.y + (to.y - from.y) * progress;

        canvas.dispatchEvent(new MouseEvent('mousemove', {
          clientX: rect.left + x,
          clientY: rect.top + y,
          bubbles: true,
          button: 0,
          buttons: 1  // Left button held
        }));
      }

      // Mouse up at end
      canvas.dispatchEvent(new MouseEvent('mouseup', {
        clientX: rect.left + to.x,
        clientY: rect.top + to.y,
        bubbles: true,
        button: 0
      }));

      return { success: true, from: from, to: to };
    },

    /**
     * Get bounds of UI element by ID
     * @param {string} elementId - The element ID to query
     * @returns {Object} Bounds {x, y, width, height, visible} or error
     */
    getElementBounds: function(elementId) {
      if (typeof Module === 'undefined' || !Module.guiGetUIElementBounds) {
        return { error: 'Module not ready' };
      }

      try {
        const result = Module.guiGetUIElementBounds(elementId);
        return JSON.parse(result);
      } catch (e) {
        console.error('[yaze.gui] getElementBounds error:', e);
        return { error: e.message };
      }
    },

    /**
     * Wait for an element to appear (with timeout)
     * @param {string} elementId - The element ID to wait for
     * @param {number} timeoutMs - Maximum time to wait (default 5000ms)
     * @returns {Promise<Object>} Element bounds or null if not found
     */
    waitForElement: async function(elementId, timeoutMs) {
      timeoutMs = timeoutMs || 5000;
      const start = Date.now();

      while (Date.now() - start < timeoutMs) {
        const elem = this.getElementBounds(elementId);
        if (elem && !elem.error && elem.visible) {
          return elem;
        }
        await new Promise(resolve => setTimeout(resolve, 100));
      }

      return null;
    },

    /**
     * Take a screenshot of the canvas
     * @param {string} format - Image format ('png' or 'jpeg', default 'png')
     * @param {number} quality - JPEG quality 0-1 (default 0.92)
     * @returns {string} Base64-encoded image data URL
     */
    takeScreenshot: function(format, quality) {
      format = format || 'png';
      quality = quality || 0.92;

      const canvas = document.getElementById('canvas');
      if (!canvas) {
        return { error: 'Canvas element not found' };
      }

      try {
        const mimeType = format === 'jpeg' ? 'image/jpeg' : 'image/png';
        const dataUrl = canvas.toDataURL(mimeType, quality);
        return {
          success: true,
          dataUrl: dataUrl,
          format: format,
          width: canvas.width,
          height: canvas.height
        };
      } catch (e) {
        return { error: e.message };
      }
    },

    /**
     * Get the current selection in the active editor
     * @returns {Object} Selection data
     */
    getSelection: function() {
      if (typeof Module === 'undefined' || !Module.editorGetSelection) {
        return { error: 'Module not ready' };
      }

      try {
        return JSON.parse(Module.editorGetSelection());
      } catch (e) {
        return { error: e.message };
      }
    },

    /**
     * Set selection programmatically
     * @param {Array<string|number>} ids - IDs of items to select
     * @returns {Object} Result with success status
     */
    setSelection: function(ids) {
      if (typeof Module === 'undefined' || !Module.guiSetSelection) {
        return { error: 'Module not ready' };
      }

      try {
        const result = Module.guiSetSelection(JSON.stringify(ids));
        return JSON.parse(result);
      } catch (e) {
        return { error: e.message };
      }
    },

    /**
     * Send a keyboard event to the canvas
     * @param {string} key - Key to press (e.g., 'Enter', 'a', 'Escape')
     * @param {Object} modifiers - Optional {ctrl, shift, alt, meta} booleans
     * @returns {Object} Result with success status
     */
    pressKey: function(key, modifiers) {
      modifiers = modifiers || {};

      const canvas = document.getElementById('canvas');
      if (!canvas) {
        return { error: 'Canvas element not found' };
      }

      const eventInit = {
        key: key,
        code: key.length === 1 ? 'Key' + key.toUpperCase() : key,
        bubbles: true,
        cancelable: true,
        ctrlKey: !!modifiers.ctrl,
        shiftKey: !!modifiers.shift,
        altKey: !!modifiers.alt,
        metaKey: !!modifiers.meta
      };

      canvas.dispatchEvent(new KeyboardEvent('keydown', eventInit));
      canvas.dispatchEvent(new KeyboardEvent('keyup', eventInit));

      return { success: true, key: key, modifiers: modifiers };
    },

    /**
     * Type a string of text (simulates keyboard input)
     * @param {string} text - Text to type
     * @param {number} delayMs - Delay between keystrokes (default 50ms)
     * @returns {Promise<Object>} Result with success status
     */
    type: async function(text, delayMs) {
      delayMs = delayMs || 50;

      for (let i = 0; i < text.length; i++) {
        this.pressKey(text[i]);
        if (delayMs > 0) {
          await new Promise(resolve => setTimeout(resolve, delayMs));
        }
      }

      return { success: true, text: text };
    },

    /**
     * Scroll the canvas by a delta amount
     * @param {number} deltaX - Horizontal scroll amount
     * @param {number} deltaY - Vertical scroll amount
     * @returns {Object} Result with success status
     */
    scroll: function(deltaX, deltaY) {
      const canvas = document.getElementById('canvas');
      if (!canvas) {
        return { error: 'Canvas element not found' };
      }

      canvas.dispatchEvent(new WheelEvent('wheel', {
        deltaX: deltaX || 0,
        deltaY: deltaY || 0,
        bubbles: true,
        cancelable: true
      }));

      return { success: true, deltaX: deltaX, deltaY: deltaY };
    },

    /**
     * Get canvas dimensions and position
     * @returns {Object} Canvas info {width, height, x, y}
     */
    getCanvasInfo: function() {
      const canvas = document.getElementById('canvas');
      if (!canvas) {
        return { error: 'Canvas element not found' };
      }

      const rect = canvas.getBoundingClientRect();
      return {
        width: canvas.width,
        height: canvas.height,
        clientWidth: rect.width,
        clientHeight: rect.height,
        x: rect.left,
        y: rect.top
      };
    },

    /**
     * Check if the GUI automation API is ready
     * @returns {boolean} True if ready
     */
    isReady: function() {
      return typeof Module !== 'undefined' &&
             typeof Module.guiGetUIElementTree === 'function';
    },

    /**
     * Update canvas data-* attributes with current editor state
     * Call this periodically (e.g., in requestAnimationFrame) for agent discoverability
     * @returns {Object} Result with updated state or error
     */
    updateCanvasState: function() {
      const canvas = document.getElementById('canvas');
      if (!canvas) {
        return { error: 'Canvas element not found' };
      }

      // Get editor state from control API
      if (typeof Module !== 'undefined' && Module.controlGetCurrentEditor) {
        try {
          const editorResult = JSON.parse(Module.controlGetCurrentEditor());
          if (editorResult && editorResult.name) {
            canvas.dataset.editorType = editorResult.name;
          }
        } catch (e) {
          // Silently ignore if not available
        }
      }

      // Get visible cards
      if (typeof Module !== 'undefined' && Module.controlGetVisiblePanels) {
        try {
          const cardsResult = JSON.parse(Module.controlGetVisiblePanels());
          if (Array.isArray(cardsResult)) {
            canvas.dataset.visiblePanels = cardsResult.join(',');
          }
        } catch (e) {
          // Silently ignore if not available
        }
      }

      // Get ROM status
      if (typeof Module !== 'undefined' && Module.controlGetRomStatus) {
        try {
          const romResult = JSON.parse(Module.controlGetRomStatus());
          if (romResult) {
            canvas.dataset.romLoaded = romResult.loaded ? 'true' : 'false';
          }
        } catch (e) {
          // Silently ignore if not available
        }
      }

      // Get session info
      if (typeof Module !== 'undefined' && Module.controlGetSessionInfo) {
        try {
          const sessionResult = JSON.parse(Module.controlGetSessionInfo());
          if (sessionResult && typeof sessionResult.session_id !== 'undefined') {
            canvas.dataset.sessionId = sessionResult.session_id;
          }
        } catch (e) {
          // Silently ignore if not available
        }
      }

      return {
        success: true,
        editorType: canvas.dataset.editorType,
        visiblePanels: canvas.dataset.visiblePanels,
        romLoaded: canvas.dataset.romLoaded,
        sessionId: canvas.dataset.sessionId
      };
    },

    /**
     * Get all available cards with their metadata
     * @returns {Object} Panels array with id, name, visible, enabled, etc.
     */
    getAvailablePanels: function() {
      if (typeof Module === 'undefined' || !Module.controlGetAvailablePanels) {
        return { error: 'Module not ready', cards: [] };
      }

      try {
        return JSON.parse(Module.controlGetAvailablePanels());
      } catch (e) {
        return { error: e.message, cards: [] };
      }
    },

    /**
     * Show a specific card by ID
     * @param {string} cardId - The card identifier
     * @returns {Object} Result with success status
     */
    showPanel: function(cardId) {
      if (typeof Module === 'undefined' || !Module.controlOpenPanel) {
        return { error: 'Module not ready' };
      }

      try {
        return JSON.parse(Module.controlOpenPanel(cardId));
      } catch (e) {
        return { error: e.message };
      }
    },

    /**
     * Hide a specific card by ID
     * @param {string} cardId - The card identifier
     * @returns {Object} Result with success status
     */
    hidePanel: function(cardId) {
      if (typeof Module === 'undefined' || !Module.controlClosePanel) {
        return { error: 'Module not ready' };
      }

      try {
        return JSON.parse(Module.controlClosePanel(cardId));
      } catch (e) {
        return { error: e.message };
      }
    },

    /**
     * Start automatic canvas state updates
     * @param {number} intervalMs - Update interval in milliseconds (default 500)
     */
    startAutoUpdate: function(intervalMs) {
      intervalMs = intervalMs || 500;
      const self = this;

      if (this._autoUpdateInterval) {
        clearInterval(this._autoUpdateInterval);
      }

      this._autoUpdateInterval = setInterval(function() {
        self.updateCanvasState();
        if (window.yaze.gui.widgetOverlay) {
          window.yaze.gui.updateWidgetOverlay();
        }
      }, intervalMs);

      return { success: true, intervalMs: intervalMs };
    },

    /**
     * Stop automatic canvas state updates
     */
    stopAutoUpdate: function() {
      if (this._autoUpdateInterval) {
        clearInterval(this._autoUpdateInterval);
        this._autoUpdateInterval = null;
      }
      return { success: true };
    },

    _autoUpdateInterval: null
  };

  console.log('[yaze] GUI Automation API loaded (window.yaze.gui)');
})();
