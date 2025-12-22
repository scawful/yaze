/**
 * @fileoverview Widget Overlay System for Agent Discoverability
 *
 * Creates invisible DOM elements that mirror ImGui widget state,
 * enabling external agents and tools to discover and interact
 * with the UI programmatically through standard DOM APIs.
 *
 * Usage:
 *   const overlay = new WidgetOverlay();
 *   overlay.update(Module.guiGetUIElementTree());  // Call each frame
 */

class WidgetOverlay {
  /**
   * @param {string} containerId - ID for the overlay container element
   */
  constructor(containerId = 'yaze-widget-overlay') {
    this.containerId = containerId;
    this.widgets = new Map();
    this.lastUpdate = 0;
    this.updateInterval = 100; // Update every 100ms max
    this.enabled = true;

    this._createContainer();
  }

  /**
   * Create the overlay container element
   * @private
   */
  _createContainer() {
    // Remove existing container if present
    const existing = document.getElementById(this.containerId);
    if (existing) {
      existing.remove();
    }

    this.container = document.createElement('div');
    this.container.id = this.containerId;
    this.container.className = 'yaze-widget-overlay-container';
    this.container.style.cssText = `
      position: absolute;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      pointer-events: none;
      z-index: 1000;
      overflow: hidden;
    `;
    this.container.setAttribute('aria-hidden', 'true');

    // Insert after canvas container
    const canvasContainer = document.getElementById('canvas-container');
    if (canvasContainer) {
      canvasContainer.appendChild(this.container);
    } else {
      document.body.appendChild(this.container);
    }
  }

  /**
   * Update the overlay with current widget state
   * @param {Object|string} widgetTree - Widget tree from C++ API (JSON string or object)
   */
  update(widgetTree) {
    if (!this.enabled) return;

    // Rate limit updates
    const now = Date.now();
    if (now - this.lastUpdate < this.updateInterval) {
      return;
    }
    this.lastUpdate = now;

    // Parse if string
    let tree = widgetTree;
    if (typeof widgetTree === 'string') {
      try {
        tree = JSON.parse(widgetTree);
      } catch (e) {
        console.warn('WidgetOverlay: Failed to parse widget tree', e);
        return;
      }
    }

    if (!tree || !tree.elements) {
      return;
    }

    // Track which widgets we've seen this update
    const seenWidgets = new Set();

    // Sync each widget
    for (const widget of tree.elements) {
      this._syncWidget(widget);
      seenWidgets.add(widget.id);
    }

    // Remove stale widgets
    this._pruneStale(seenWidgets);
  }

  /**
   * Sync a single widget element with the DOM
   * @param {Object} widget - Widget data from C++ API
   * @private
   */
  _syncWidget(widget) {
    let el = this.widgets.get(widget.id);

    if (!el) {
      // Create new element
      el = document.createElement('div');
      el.className = 'yaze-widget';
      this.container.appendChild(el);
      this.widgets.set(widget.id, el);
    }

    // Update data attributes for agent discoverability
    el.dataset.widgetId = widget.id;
    el.dataset.widgetType = widget.type || 'unknown';
    el.dataset.label = widget.label || '';
    el.dataset.visible = widget.visible !== false ? 'true' : 'false';
    el.dataset.enabled = widget.enabled !== false ? 'true' : 'false';
    el.dataset.window = widget.window || '';

    // Additional metadata
    if (widget.imgui_id) {
      el.dataset.imguiId = widget.imgui_id;
    }
    if (widget.description) {
      el.dataset.description = widget.description;
    }
    if (widget.last_seen_frame) {
      el.dataset.lastSeenFrame = widget.last_seen_frame;
    }

    // Update position and size
    if (widget.bounds) {
      const b = widget.bounds;
      el.style.cssText = `
        position: absolute;
        left: ${b.x || 0}px;
        top: ${b.y || 0}px;
        width: ${b.width || 0}px;
        height: ${b.height || 0}px;
        pointer-events: none;
      `;

      // Add size attributes for easier querying
      el.dataset.x = b.x || 0;
      el.dataset.y = b.y || 0;
      el.dataset.width = b.width || 0;
      el.dataset.height = b.height || 0;
    }
  }

  /**
   * Remove widgets that weren't seen in the last update
   * @param {Set<string>} seenWidgets - Set of widget IDs that are still valid
   * @private
   */
  _pruneStale(seenWidgets) {
    for (const [id, el] of this.widgets) {
      if (!seenWidgets.has(id)) {
        el.remove();
        this.widgets.delete(id);
      }
    }
  }

  /**
   * Get a widget element by ID
   * @param {string} id - Widget ID
   * @returns {HTMLElement|null}
   */
  getWidget(id) {
    return this.widgets.get(id) || null;
  }

  /**
   * Query widgets by type
   * @param {string} type - Widget type (e.g., 'button', 'canvas', 'input')
   * @returns {Array<HTMLElement>}
   */
  getWidgetsByType(type) {
    return Array.from(this.container.querySelectorAll(`[data-widget-type="${type}"]`));
  }

  /**
   * Query widgets by window
   * @param {string} windowName - Window name
   * @returns {Array<HTMLElement>}
   */
  getWidgetsByWindow(windowName) {
    return Array.from(this.container.querySelectorAll(`[data-window="${windowName}"]`));
  }

  /**
   * Query visible widgets
   * @returns {Array<HTMLElement>}
   */
  getVisibleWidgets() {
    return Array.from(this.container.querySelectorAll('[data-visible="true"]'));
  }

  /**
   * Enable or disable the overlay
   * @param {boolean} enabled
   */
  setEnabled(enabled) {
    this.enabled = enabled;
    this.container.style.display = enabled ? 'block' : 'none';
  }

  /**
   * Clear all widget elements
   */
  clear() {
    for (const el of this.widgets.values()) {
      el.remove();
    }
    this.widgets.clear();
  }

  /**
   * Export current widget state as JSON
   * @returns {Object}
   */
  exportState() {
    const widgets = [];
    for (const [id, el] of this.widgets) {
      widgets.push({
        id: id,
        type: el.dataset.widgetType,
        label: el.dataset.label,
        visible: el.dataset.visible === 'true',
        enabled: el.dataset.enabled === 'true',
        window: el.dataset.window,
        bounds: {
          x: parseFloat(el.dataset.x) || 0,
          y: parseFloat(el.dataset.y) || 0,
          width: parseFloat(el.dataset.width) || 0,
          height: parseFloat(el.dataset.height) || 0
        }
      });
    }
    return {
      count: widgets.length,
      widgets: widgets,
      timestamp: Date.now()
    };
  }
}

// Create global instance and attach to window.yaze namespace
(function() {
  // Wait for namespace to be available
  function initOverlay() {
    if (typeof window.yaze === 'undefined') {
      window.yaze = {};
    }
    if (typeof window.yaze.gui === 'undefined') {
      window.yaze.gui = {};
    }

    // Create singleton instance
    const overlay = new WidgetOverlay();

    // Expose overlay instance and class
    window.yaze.gui.widgetOverlay = overlay;
    window.yaze.gui.WidgetOverlay = WidgetOverlay;

    // Auto-update function that can be called from requestAnimationFrame
    window.yaze.gui.updateWidgetOverlay = function() {
      if (typeof Module !== 'undefined' && Module.guiGetUIElementTree) {
        try {
          const tree = Module.guiGetUIElementTree();
          overlay.update(tree);
        } catch (e) {
          // Silently fail if API not available yet
        }
      }
    };

    console.log('WidgetOverlay initialized');
  }

  // Initialize when DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initOverlay);
  } else {
    initOverlay();
  }
})();
