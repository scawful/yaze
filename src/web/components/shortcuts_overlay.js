/**
 * @fileoverview Web-specific keyboard shortcut handling for yaze WASM
 * This file provides browser-side keyboard event handling, shortcut prevention,
 * and an optional HTML overlay for touch-friendly shortcut discovery.
 *
 * Works in coordination with the C++ KeyboardShortcuts class.
 */

(function() {
  'use strict';

  // Track if the overlay is currently visible (synced with C++ side)
  let overlayVisible = false;

  // List of shortcuts to prevent browser defaults for
  const preventDefaultShortcuts = [
    { key: 's', ctrl: true, shift: false, alt: false },   // Save
    { key: 'o', ctrl: true, shift: false, alt: false },   // Open
    { key: 'w', ctrl: true, shift: false, alt: false },   // Close tab
    { key: 'f', ctrl: true, shift: false, alt: false },   // Find
    { key: 'g', ctrl: true, shift: false, alt: false },   // Find next (some browsers)
    { key: 'p', ctrl: true, shift: false, alt: false },   // Print
    { key: 'z', ctrl: true, shift: false, alt: false },   // Undo
    { key: 'y', ctrl: true, shift: false, alt: false },   // Redo
    { key: 'z', ctrl: true, shift: true, alt: false },    // Redo (alt)
    { key: '+', ctrl: true, shift: false, alt: false },   // Zoom in
    { key: '=', ctrl: true, shift: false, alt: false },   // Zoom in (no shift)
    { key: '-', ctrl: true, shift: false, alt: false },   // Zoom out
    { key: '0', ctrl: true, shift: false, alt: false },   // Reset zoom
    { key: 'F11', ctrl: false, shift: false, alt: false }, // Fullscreen
  ];

  /**
   * Check if a keyboard event matches a shortcut definition
   * @param {KeyboardEvent} event
   * @param {Object} shortcut
   * @returns {boolean}
   */
  function matchesShortcut(event, shortcut) {
    const key = event.key.toLowerCase();
    const ctrl = event.ctrlKey || event.metaKey; // Support Cmd on macOS
    const shift = event.shiftKey;
    const alt = event.altKey;

    return key === shortcut.key.toLowerCase() &&
           ctrl === shortcut.ctrl &&
           shift === shortcut.shift &&
           alt === shortcut.alt;
  }

  /**
   * Handle keydown events to prevent browser defaults
   * @param {KeyboardEvent} event
   */
  function handleKeyDown(event) {
    // Skip shortcut handling if user is typing in an input field
    const target = event.target;
    if (target && (
      target.tagName === 'INPUT' ||
      target.tagName === 'TEXTAREA' ||
      target.isContentEditable
    )) {
      // Allow normal typing in input fields
      return;
    }

    // Check if we should prevent this shortcut
    for (const shortcut of preventDefaultShortcuts) {
      if (matchesShortcut(event, shortcut)) {
        event.preventDefault();
        event.stopPropagation();
        return;
      }
    }

    // Handle '?' key to toggle overlay
    if (event.key === '?' || (event.key === '/' && event.shiftKey)) {
      // The C++ side handles the actual toggle, but we might want to
      // show an HTML overlay on touch devices
      if (isTouchDevice()) {
        toggleHTMLOverlay();
        event.preventDefault();
      }
    }

    // Close overlay on Escape
    if (event.key === 'Escape' && overlayVisible) {
      hideHTMLOverlay();
    }
  }

  /**
   * Check if the current device supports touch
   * @returns {boolean}
   */
  function isTouchDevice() {
    return ('ontouchstart' in window) ||
           (navigator.maxTouchPoints > 0) ||
           (navigator.msMaxTouchPoints > 0);
  }

  /**
   * Create and show the HTML shortcuts overlay (for touch devices)
   */
  function showHTMLOverlay() {
    if (document.getElementById('yaze-shortcuts-overlay')) {
      document.getElementById('yaze-shortcuts-overlay').style.display = 'flex';
      overlayVisible = true;
      return;
    }

    const overlay = document.createElement('div');
    overlay.id = 'yaze-shortcuts-overlay';
    overlay.className = 'yaze-shortcuts-overlay';
    overlay.innerHTML = `
      <div class="yaze-shortcuts-modal">
        <div class="yaze-shortcuts-header">
          <h2>Keyboard Shortcuts</h2>
          <button class="yaze-shortcuts-close" aria-label="Close">&times;</button>
        </div>
        <div class="yaze-shortcuts-search">
          <input type="text" placeholder="Search shortcuts..." id="yaze-shortcuts-search-input">
        </div>
        <div class="yaze-shortcuts-content">
          ${generateShortcutsHTML()}
        </div>
        <div class="yaze-shortcuts-footer">
          <span>Press <kbd>?</kbd> to toggle | <kbd>Esc</kbd> to close</span>
        </div>
      </div>
    `;

    document.body.appendChild(overlay);
    overlayVisible = true;

    // Event listeners
    overlay.querySelector('.yaze-shortcuts-close').addEventListener('click', hideHTMLOverlay);
    overlay.addEventListener('click', function(e) {
      if (e.target === overlay) {
        hideHTMLOverlay();
      }
    });

    // Search functionality
    const searchInput = overlay.querySelector('#yaze-shortcuts-search-input');
    searchInput.addEventListener('input', function(e) {
      filterShortcuts(e.target.value);
    });
    searchInput.focus();
  }

  /**
   * Hide the HTML shortcuts overlay
   */
  function hideHTMLOverlay() {
    const overlay = document.getElementById('yaze-shortcuts-overlay');
    if (overlay) {
      overlay.style.display = 'none';
      overlayVisible = false;
    }
  }

  /**
   * Toggle the HTML shortcuts overlay
   */
  function toggleHTMLOverlay() {
    if (overlayVisible) {
      hideHTMLOverlay();
    } else {
      showHTMLOverlay();
    }
  }

  /**
   * Generate HTML for shortcuts (static fallback when C++ data isn't available)
   * @returns {string}
   */
  function generateShortcutsHTML() {
    const isMac = navigator.platform.toUpperCase().indexOf('MAC') >= 0;
    const ctrlKey = isMac ? 'Cmd' : 'Ctrl';

    const categories = [
      {
        name: 'File',
        shortcuts: [
          { keys: `${ctrlKey}+O`, description: 'Open ROM/Project' },
          { keys: `${ctrlKey}+S`, description: 'Save' },
          { keys: `${ctrlKey}+Shift+S`, description: 'Save As...' },
          { keys: `${ctrlKey}+W`, description: 'Close' },
        ]
      },
      {
        name: 'Edit',
        shortcuts: [
          { keys: `${ctrlKey}+Z`, description: 'Undo' },
          { keys: `${ctrlKey}+Y`, description: 'Redo' },
          { keys: `${ctrlKey}+C`, description: 'Copy' },
          { keys: `${ctrlKey}+V`, description: 'Paste' },
          { keys: `${ctrlKey}+X`, description: 'Cut' },
          { keys: `${ctrlKey}+F`, description: 'Find' },
        ]
      },
      {
        name: 'View',
        shortcuts: [
          { keys: 'F11', description: 'Toggle Fullscreen' },
          { keys: `${ctrlKey}+G`, description: 'Toggle Grid' },
          { keys: `${ctrlKey}++`, description: 'Zoom In' },
          { keys: `${ctrlKey}+-`, description: 'Zoom Out' },
          { keys: `${ctrlKey}+0`, description: 'Reset Zoom' },
        ]
      },
      {
        name: 'Navigation',
        shortcuts: [
          { keys: 'Home', description: 'Go to First' },
          { keys: 'End', description: 'Go to Last' },
          { keys: 'Page Up', description: 'Previous' },
          { keys: 'Page Down', description: 'Next' },
          { keys: 'Arrow Keys', description: 'Move Selection' },
        ]
      },
      {
        name: 'Emulator',
        shortcuts: [
          { keys: 'Space', description: 'Play/Pause' },
          { keys: `${ctrlKey}+R`, description: 'Reset' },
          { keys: 'F', description: 'Step Frame' },
        ]
      },
    ];

    let html = '';
    for (const category of categories) {
      html += `
        <div class="yaze-shortcuts-category" data-category="${category.name.toLowerCase()}">
          <h3 class="yaze-shortcuts-category-header">${category.name}</h3>
          <div class="yaze-shortcuts-list">
      `;
      for (const shortcut of category.shortcuts) {
        html += `
          <div class="yaze-shortcut-row" data-searchable="${shortcut.description.toLowerCase()} ${shortcut.keys.toLowerCase()}">
            <span class="yaze-shortcut-keys">${formatKeys(shortcut.keys)}</span>
            <span class="yaze-shortcut-description">${shortcut.description}</span>
          </div>
        `;
      }
      html += `
          </div>
        </div>
      `;
    }
    return html;
  }

  /**
   * Format key combination for display with keyboard styling
   * @param {string} keys
   * @returns {string}
   */
  function formatKeys(keys) {
    return keys.split('+').map(key => `<kbd>${key.trim()}</kbd>`).join(' + ');
  }

  /**
   * Filter shortcuts based on search query
   * @param {string} query
   */
  function filterShortcuts(query) {
    const overlay = document.getElementById('yaze-shortcuts-overlay');
    if (!overlay) return;

    const rows = overlay.querySelectorAll('.yaze-shortcut-row');
    const categories = overlay.querySelectorAll('.yaze-shortcuts-category');
    const lowerQuery = query.toLowerCase().trim();

    // Reset visibility if no query
    if (!lowerQuery) {
      rows.forEach(row => row.style.display = '');
      categories.forEach(cat => cat.style.display = '');
      return;
    }

    // Filter rows
    rows.forEach(row => {
      const searchable = row.dataset.searchable || '';
      const matches = searchable.includes(lowerQuery);
      row.style.display = matches ? '' : 'none';
    });

    // Hide empty categories
    categories.forEach(category => {
      const visibleRows = category.querySelectorAll('.yaze-shortcut-row:not([style*="display: none"])');
      category.style.display = visibleRows.length > 0 ? '' : 'none';
    });
  }

  /**
   * Notify C++ side about overlay state changes
   * @param {boolean} visible
   */
  function notifyOverlayState(visible) {
    if (window.Module && window.Module._setShortcutOverlayVisible) {
      window.Module._setShortcutOverlayVisible(visible ? 1 : 0);
    }
  }

  /**
   * Public API for showing/hiding overlay from C++ via Emscripten
   */
  window.yazeShortcuts = {
    show: function() {
      showHTMLOverlay();
    },
    hide: function() {
      hideHTMLOverlay();
    },
    toggle: function() {
      toggleHTMLOverlay();
    },
    isVisible: function() {
      return overlayVisible;
    },
    // Called from C++ to sync state
    syncState: function(visible) {
      overlayVisible = visible;
      const overlay = document.getElementById('yaze-shortcuts-overlay');
      if (overlay) {
        overlay.style.display = visible ? 'flex' : 'none';
      }
    }
  };

  // Initialize on DOM ready
  function init() {
    // Add keyboard event listener
    document.addEventListener('keydown', handleKeyDown, true);

    // Add styles if not already present
    // CSS is loaded by shell.html, but load dynamically as fallback
    if (!document.getElementById('yaze-shortcuts-styles') &&
        !document.querySelector('link[href*="shortcuts_overlay.css"]')) {
      const link = document.createElement('link');
      link.id = 'yaze-shortcuts-styles';
      link.rel = 'stylesheet';
      link.href = 'styles/shortcuts_overlay.css';
      document.head.appendChild(link);
    }

    console.log('YAZE shortcuts handler initialized');
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
