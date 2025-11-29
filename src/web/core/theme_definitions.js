/**
 * @fileoverview Unified theme definitions for yaze web and ImGui
 *
 * This file serves as the single source of truth for all themes available in yaze.
 * Themes defined here can be used in both the web UI (CSS variables) and the
 * ImGui C++ UI (via WASM bindings).
 *
 * Theme Structure:
 * - Each theme has CSS variable mappings and ImGui color mappings
 * - Themes are synchronized bidirectionally between web and ImGui
 */

(function() {
  'use strict';

  /**
   * CSS variable to theme property mapping
   * Maps CSS custom property names to theme JSON keys from C++
   */
  const CSS_TO_THEME_MAP = {
    '--bg-app': 'background',
    '--bg-header': 'menu_bar_bg',
    '--bg-panel': 'surface',
    '--bg-input': 'frame_bg',
    '--bg-hover': 'button_hovered',
    '--bg-selected': 'button_active',
    '--text-primary': 'text_primary',
    '--text-secondary': 'text_secondary',
    '--text-muted': 'text_disabled',
    '--text-accent': 'primary',
    '--accent-primary': 'primary',
    '--accent-hover': 'button_hovered',
    '--accent-dim': 'secondary',
    '--border-color': 'border',
    '--border-focus': 'accent',
    '--status-success': 'success',
    '--status-warning': 'warning',
    '--status-error': 'error'
  };

  /**
   * Theme definitions
   * Each theme includes both CSS values and ImGui-compatible color values
   */
  const THEMES = {
    // YAZE Classic (matches ImGui YAZE Tre theme)
    'yaze_classic': {
      name: 'YAZE Classic',
      description: 'The original YAZE green theme',
      author: 'yaze',
      css: {
        '--bg-app': '#080808',
        '--bg-header': '#111111',
        '--bg-panel': '#0a0a0a',
        '--bg-input': '#1c1c24',
        '--bg-hover': '#2d422d',
        '--bg-selected': '#3d5a3d',
        '--text-primary': '#e6e6e6',
        '--text-secondary': '#c8c8c8',
        '--text-muted': '#999999',
        '--text-accent': '#5c735c',
        '--accent-primary': '#5c735c',
        '--accent-hover': '#7d927d',
        '--accent-dim': '#2e422e',
        '--border-color': '#5c735c',
        '--border-focus': '#597759',
        '--status-success': '#5c735c',
        '--status-warning': '#ffc832',
        '--status-error': '#dc3232',
        '--control-radius': '5px'
      },
      imgui: {
        primary: [92, 115, 92, 255],
        secondary: [71, 92, 71, 255],
        accent: [89, 119, 89, 255],
        background: [8, 8, 8, 255],
        text_primary: [230, 230, 230, 255],
        text_secondary: [200, 200, 200, 255],
        text_disabled: [153, 153, 153, 255],
        error: [220, 50, 50, 255],
        warning: [255, 200, 50, 255],
        success: [92, 115, 92, 255],
        info: [70, 170, 255, 255]
      }
    },

    // Hacker Green (default web theme)
    'hacker_green': {
      name: 'Hacker Green',
      description: 'Minimalist dark theme with neon green accents',
      author: 'yaze',
      css: {
        '--bg-app': '#0d0d0d',
        '--bg-header': '#111111',
        '--bg-panel': '#111111',
        '--bg-input': '#1a1a1a',
        '--bg-hover': '#1f1f1f',
        '--bg-selected': '#222222',
        '--text-primary': '#cccccc',
        '--text-secondary': '#888888',
        '--text-muted': '#555555',
        '--text-accent': '#00cc66',
        '--accent-primary': '#00cc66',
        '--accent-hover': '#00ff88',
        '--accent-dim': '#008844',
        '--border-color': '#333333',
        '--border-focus': '#00cc66',
        '--status-success': '#00cc66',
        '--status-warning': '#ffaa00',
        '--status-error': '#ff4444',
        '--control-radius': '2px'
      },
      imgui: {
        primary: [0, 204, 102, 255],
        secondary: [0, 136, 68, 255],
        accent: [0, 255, 136, 255],
        background: [13, 13, 13, 255],
        text_primary: [204, 204, 204, 255],
        text_secondary: [136, 136, 136, 255],
        text_disabled: [85, 85, 85, 255],
        error: [255, 68, 68, 255],
        warning: [255, 170, 0, 255],
        success: [0, 204, 102, 255],
        info: [70, 170, 255, 255]
      }
    },

    // Modern (VS Code style)
    'modern': {
      name: 'Modern Dark',
      description: 'VS Code inspired dark theme',
      author: 'yaze',
      css: {
        '--bg-app': '#1e1e1e',
        '--bg-header': '#252526',
        '--bg-panel': '#1e1e1e',
        '--bg-input': '#3c3c3c',
        '--bg-hover': '#2a2d2e',
        '--bg-selected': '#37373d',
        '--text-primary': '#cccccc',
        '--text-secondary': '#999999',
        '--text-muted': '#666666',
        '--text-accent': '#4ec9b0',
        '--accent-primary': '#007acc',
        '--accent-hover': '#0098ff',
        '--accent-dim': '#005f9e',
        '--border-color': '#333333',
        '--border-focus': '#007acc',
        '--status-success': '#4ec9b0',
        '--status-warning': '#cca700',
        '--status-error': '#f48771',
        '--control-radius': '4px'
      },
      imgui: {
        primary: [0, 122, 204, 255],
        secondary: [0, 95, 158, 255],
        accent: [0, 152, 255, 255],
        background: [30, 30, 30, 255],
        text_primary: [204, 204, 204, 255],
        text_secondary: [153, 153, 153, 255],
        text_disabled: [102, 102, 102, 255],
        error: [244, 135, 113, 255],
        warning: [204, 167, 0, 255],
        success: [78, 201, 176, 255],
        info: [78, 201, 176, 255]
      }
    },

    // Halext Purple
    'halext': {
      name: 'Halext Purple',
      description: 'Premium purple theme',
      author: 'halext.org',
      css: {
        '--bg-app': '#0f0b15',
        '--bg-header': '#16121d',
        '--bg-panel': '#130f1a',
        '--bg-input': '#231e30',
        '--bg-hover': '#2d2640',
        '--bg-selected': '#3d2e5e',
        '--text-primary': '#f0f0f0',
        '--text-secondary': '#a090b0',
        '--text-muted': '#605070',
        '--text-accent': '#a78bfa',
        '--accent-primary': '#8b5cf6',
        '--accent-hover': '#a78bfa',
        '--accent-dim': '#6d28d9',
        '--border-color': '#2d2640',
        '--border-focus': '#8b5cf6',
        '--status-success': '#34d399',
        '--status-warning': '#fbbf24',
        '--status-error': '#f87171',
        '--control-radius': '6px'
      },
      imgui: {
        primary: [139, 92, 246, 255],
        secondary: [109, 40, 217, 255],
        accent: [167, 139, 250, 255],
        background: [15, 11, 21, 255],
        text_primary: [240, 240, 240, 255],
        text_secondary: [160, 144, 176, 255],
        text_disabled: [96, 80, 112, 255],
        error: [248, 113, 113, 255],
        warning: [251, 191, 36, 255],
        success: [52, 211, 153, 255],
        info: [139, 92, 246, 255]
      }
    }
  };

  /**
   * Get a theme by name
   * @param {string} themeName - The theme identifier
   * @returns {Object|null} Theme definition or null if not found
   */
  function getTheme(themeName) {
    return THEMES[themeName] || null;
  }

  /**
   * Get all available theme names
   * @returns {string[]} Array of theme identifiers
   */
  function getAvailableThemes() {
    return Object.keys(THEMES);
  }

  /**
   * Apply a theme's CSS variables to the document
   * @param {string} themeName - The theme identifier
   */
  function applyTheme(themeName) {
    const theme = THEMES[themeName];
    if (!theme || !theme.css) {
      console.warn('Theme not found:', themeName);
      return false;
    }

    const root = document.documentElement;

    // Clear any inline styles from C++ sync
    Object.keys(CSS_TO_THEME_MAP).forEach(prop => {
      root.style.removeProperty(prop);
    });
    root.style.removeProperty('--control-radius');

    // Set theme attribute for CSS selector-based theming
    document.body.setAttribute('data-theme', themeName === 'hacker_green' ? 'default' : themeName);

    // Apply CSS variables
    Object.entries(theme.css).forEach(([prop, value]) => {
      root.style.setProperty(prop, value);
    });

    // Store preference
    localStorage.setItem('yaze-theme', themeName);

    // Notify C++ if WASM is ready
    if (typeof Module !== 'undefined' && Module.controlSetTheme) {
      try {
        Module.controlSetTheme(themeName);
      } catch (e) {
        console.warn('Failed to sync theme to C++:', e);
      }
    }

    console.log('Theme applied:', theme.name);
    return true;
  }

  /**
   * Apply theme from C++ exported JSON data
   * @param {string} jsonStr - JSON string from C++ ExportCurrentThemeJson
   */
  function applyThemeFromCpp(jsonStr) {
    try {
      const themeData = JSON.parse(jsonStr);
      const colors = themeData.colors;
      const root = document.documentElement;

      if (colors) {
        // Map C++ color names to CSS variables
        Object.entries(CSS_TO_THEME_MAP).forEach(([cssVar, themeKey]) => {
          if (colors[themeKey]) {
            root.style.setProperty(cssVar, colors[themeKey]);
          }
        });
      }

      if (themeData.style) {
        const rounding = themeData.style.frame_rounding || 5;
        root.style.setProperty('--control-radius', rounding + 'px');
      }

      document.body.setAttribute('data-theme', 'synced');
      localStorage.setItem('yaze-theme', 'synced');

      console.log('Theme synced from C++:', themeData.name || 'Unknown');
      return true;
    } catch (e) {
      console.error('Error applying C++ theme data:', e);
      return false;
    }
  }

  /**
   * Sync theme with C++ (pull from C++ and apply to CSS)
   * @returns {boolean} True if sync was successful
   */
  function syncFromCpp() {
    if (typeof Module !== 'undefined' && Module.settingsGetCurrentThemeData) {
      try {
        const data = Module.settingsGetCurrentThemeData();
        if (data) {
          return applyThemeFromCpp(data);
        }
      } catch (e) {
        console.warn('Failed to sync theme from C++:', e);
      }
    }
    return false;
  }

  /**
   * Convert CSS color to ImGui RGBA array
   * @param {string} cssColor - CSS color (hex or rgb)
   * @returns {number[]} [r, g, b, a] array
   */
  function cssColorToImgui(cssColor) {
    // Handle hex colors
    if (cssColor.startsWith('#')) {
      const hex = cssColor.slice(1);
      const r = parseInt(hex.substr(0, 2), 16);
      const g = parseInt(hex.substr(2, 2), 16);
      const b = parseInt(hex.substr(4, 2), 16);
      return [r, g, b, 255];
    }
    // Handle rgb/rgba
    const match = cssColor.match(/rgba?\((\d+),\s*(\d+),\s*(\d+)(?:,\s*([\d.]+))?\)/);
    if (match) {
      return [
        parseInt(match[1]),
        parseInt(match[2]),
        parseInt(match[3]),
        match[4] ? Math.round(parseFloat(match[4]) * 255) : 255
      ];
    }
    return [128, 128, 128, 255]; // Default gray
  }

  // Export to global namespace
  window.YazeThemes = {
    themes: THEMES,
    cssToThemeMap: CSS_TO_THEME_MAP,
    getTheme: getTheme,
    getAvailableThemes: getAvailableThemes,
    applyTheme: applyTheme,
    applyThemeFromCpp: applyThemeFromCpp,
    syncFromCpp: syncFromCpp,
    cssColorToImgui: cssColorToImgui
  };

  // Also add to yaze namespace if available
  if (window.yaze) {
    window.yaze.themes = window.YazeThemes;
  }

})();
