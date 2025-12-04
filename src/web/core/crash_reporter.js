/**
 * WASM Crash Reporter - Enhanced error diagnostics
 *
 * Provides detailed crash reports for WASM runtime errors including:
 * - Stack traces with function names (when available)
 * - Memory state and heap info
 * - Application state snapshot
 * - Recent console log history
 * - Build version and commit info
 */

(function() {
  'use strict';

  // Build info - populated by build script or set manually
  const BUILD_INFO = {
    version: '0.3.9',
    commit: window.YAZE_BUILD_COMMIT || 'unknown',
    buildDate: window.YAZE_BUILD_DATE || 'unknown',
    preset: window.YAZE_BUILD_PRESET || 'unknown'
  };

  // Console log history for crash reports
  const LOG_HISTORY_SIZE = 100;
  let logHistory = [];

  // Capture console output for crash reports
  const originalConsole = {
    log: console.log,
    warn: console.warn,
    error: console.error
  };

  function captureLog(type, args) {
    const entry = {
      type: type,
      time: new Date().toISOString(),
      message: Array.from(args).map(arg => {
        try {
          return typeof arg === 'object' ? JSON.stringify(arg) : String(arg);
        } catch (e) {
          return '[Circular]';
        }
      }).join(' ')
    };

    logHistory.push(entry);
    if (logHistory.length > LOG_HISTORY_SIZE) {
      logHistory.shift();
    }
  }

  console.log = function(...args) {
    captureLog('LOG', args);
    originalConsole.log.apply(console, args);
  };

  console.warn = function(...args) {
    captureLog('WARN', args);
    originalConsole.warn.apply(console, args);
  };

  console.error = function(...args) {
    captureLog('ERROR', args);
    originalConsole.error.apply(console, args);
  };

  /**
   * Get WASM memory statistics
   */
  function getMemoryStats() {
    if (typeof Module === 'undefined' || !Module.HEAPU8) {
      return { available: false };
    }

    try {
      const heapSize = Module.HEAPU8.length;
      const usedHeap = Module._emscripten_get_heap_size
        ? Module._emscripten_get_heap_size()
        : heapSize;

      return {
        available: true,
        heapSize: heapSize,
        heapSizeMB: (heapSize / 1024 / 1024).toFixed(2),
        usedHeap: usedHeap,
        usedHeapMB: (usedHeap / 1024 / 1024).toFixed(2)
      };
    } catch (e) {
      return { available: false, error: e.message };
    }
  }

  /**
   * Get application state snapshot
   */
  function getAppState() {
    const state = {
      moduleReady: typeof Module !== 'undefined' && Module.calledRun,
      yazeApiReady: !!(window.yaze && window.yaze.control),
      editor: null,
      romLoaded: false,
      visiblePanels: []
    };

    try {
      if (window.yaze && window.yaze.control) {
        const editor = window.yaze.control.getCurrentEditor();
        if (editor && !editor.error) state.editor = editor.name;

        const rom = window.yaze.control.getRomStatus();
        if (rom && !rom.error) state.romLoaded = rom.loaded;

        const cards = window.yaze.control.getVisiblePanels();
        if (Array.isArray(cards)) state.visiblePanels = cards;
      }
    } catch (e) {
      state.stateError = e.message;
    }

    return state;
  }

  /**
   * Parse WASM stack trace to extract function info
   */
  function parseWasmStack(stack) {
    if (!stack) return [];

    const frames = [];
    const lines = stack.split('\n');

    for (const line of lines) {
      // Match patterns like: @https://...yaze.wasm:wasm-function[5710]:0x5fd3fc
      const wasmMatch = line.match(/wasm-function\[(\d+)\]:0x([0-9a-f]+)/i);
      if (wasmMatch) {
        frames.push({
          type: 'wasm',
          functionIndex: parseInt(wasmMatch[1]),
          offset: wasmMatch[2],
          raw: line.trim()
        });
        continue;
      }

      // Match JavaScript frames
      const jsMatch = line.match(/at\s+(.+?)\s+\((.+):(\d+):(\d+)\)/);
      if (jsMatch) {
        frames.push({
          type: 'js',
          function: jsMatch[1],
          file: jsMatch[2],
          line: parseInt(jsMatch[3]),
          column: parseInt(jsMatch[4]),
          raw: line.trim()
        });
        continue;
      }

      // Generic frame
      if (line.trim()) {
        frames.push({ type: 'unknown', raw: line.trim() });
      }
    }

    return frames;
  }

  /**
   * Generate a full crash report
   */
  function generateCrashReport(error, source, lineno, colno, errorObj) {
    const report = {
      timestamp: new Date().toISOString(),
      url: window.location.href,
      userAgent: navigator.userAgent,
      build: BUILD_INFO,
      error: {
        message: error ? String(error) : 'Unknown error',
        source: source || 'unknown',
        line: lineno,
        column: colno,
        name: errorObj ? errorObj.name : 'Error',
        stack: errorObj ? errorObj.stack : null
      },
      parsedStack: errorObj ? parseWasmStack(errorObj.stack) : [],
      memory: getMemoryStats(),
      appState: getAppState(),
      recentLogs: logHistory.slice(-20) // Last 20 log entries
    };

    // Check for known error patterns
    report.diagnosis = diagnoseError(report);

    return report;
  }

  /**
   * Diagnose known error patterns
   */
  function diagnoseError(report) {
    const diagnosis = {
      category: 'unknown',
      likely_cause: null,
      suggested_fix: null
    };

    const message = report.error.message.toLowerCase();
    const stack = report.error.stack || '';

    // Index out of bounds
    if (message.includes('index out of bounds') || message.includes('memory access out of bounds')) {
      diagnosis.category = 'bounds_violation';
      diagnosis.likely_cause = 'Array or buffer access beyond allocated size';

      // Check for specific patterns in stack
      if (stack.includes('DrawTile') || stack.includes('object_drawer')) {
        diagnosis.likely_cause = 'Tile rendering with invalid tile ID (ID > 0x3FF)';
        diagnosis.suggested_fix = 'Check tile IDs in room/map data for corruption';
      } else if (stack.includes('palette') || stack.includes('SetPalette')) {
        diagnosis.likely_cause = 'Palette with more colors than surface supports';
        diagnosis.suggested_fix = 'Verify palette size matches surface format';
      } else if (stack.includes('Arena') || stack.includes('gfx_sheet')) {
        diagnosis.likely_cause = 'Graphics sheet index out of range (> 222)';
        diagnosis.suggested_fix = 'Check sheet index values in calling code';
      }
    }

    // Memory allocation failure
    else if (message.includes('out of memory') || message.includes('allocation failed')) {
      diagnosis.category = 'memory_exhaustion';
      diagnosis.likely_cause = 'WASM heap exhausted';
      diagnosis.suggested_fix = 'Reduce memory usage or increase ALLOW_MEMORY_GROWTH';
    }

    // Null pointer
    else if (message.includes('null') || message.includes('nullptr')) {
      diagnosis.category = 'null_pointer';
      diagnosis.likely_cause = 'Dereferencing null pointer';
      diagnosis.suggested_fix = 'Add null checks before pointer access';
    }

    return diagnosis;
  }

  /**
   * Format crash report for display
   */
  function formatCrashReportText(report) {
    let text = `YAZE Crash Report
==================
Time: ${report.timestamp}
URL: ${report.url}
User Agent: ${report.userAgent}

Build Info:
  Version: ${report.build.version}
  Commit: ${report.build.commit}
  Date: ${report.build.buildDate}
  Preset: ${report.build.preset}

Error:
  ${report.error.name}: ${report.error.message}
  Source: ${report.error.source}:${report.error.line}:${report.error.column}

Diagnosis:
  Category: ${report.diagnosis.category}
  Likely Cause: ${report.diagnosis.likely_cause || 'Unknown'}
  Suggested Fix: ${report.diagnosis.suggested_fix || 'N/A'}

Stack Trace:
${report.error.stack || '  (no stack trace available)'}

Parsed Stack Frames:
${report.parsedStack.map((f, i) => `  [${i}] ${f.type === 'wasm' ? `WASM func[${f.functionIndex}] @ 0x${f.offset}` : f.raw}`).join('\n')}

Memory:
  Heap Size: ${report.memory.heapSizeMB || 'N/A'} MB
  Used Heap: ${report.memory.usedHeapMB || 'N/A'} MB

App State:
  Module Ready: ${report.appState.moduleReady}
  API Ready: ${report.appState.yazeApiReady}
  Current Editor: ${report.appState.editor || 'None'}
  ROM Loaded: ${report.appState.romLoaded}
  Visible Panels: ${report.appState.visiblePanels.join(', ') || 'None'}

Console Log (recent):
${report.recentLogs.map(l => `[${l.type}] ${l.message}`).join('\n')}
`;
    return text;
  }

  // Problems panel for non-fatal errors
  let problemsPanel = null;
  let problemsList = [];

  function createProblemsPanel() {
    if (problemsPanel) return problemsPanel;

    const panel = document.createElement('div');
    panel.id = 'yaze-problems-panel';
    panel.className = 'yaze-problems-panel';
    panel.innerHTML = `
      <div class="yaze-problems-header">
        <span class="yaze-problems-icon">⚠️</span>
        <span class="yaze-problems-title">Problems</span>
        <span class="yaze-problems-count">0</span>
        <button class="yaze-problems-toggle">▼</button>
        <button class="yaze-problems-clear">Clear</button>
        <button class="yaze-problems-close">×</button>
      </div>
      <div class="yaze-problems-list"></div>
    `;

    // Add styles
    const style = document.createElement('style');
    style.textContent = `
      .yaze-problems-panel {
        position: fixed;
        bottom: 0;
        left: 0;
        right: 0;
        background: #1e1e1e;
        border-top: 2px solid #ff6b6b;
        font-family: monospace;
        font-size: 12px;
        z-index: 10000;
        max-height: 200px;
        transition: max-height 0.3s;
      }
      .yaze-problems-panel.collapsed { max-height: 28px; }
      .yaze-problems-header {
        display: flex;
        align-items: center;
        gap: 8px;
        padding: 4px 8px;
        background: #2d2d2d;
        color: #ff6b6b;
      }
      .yaze-problems-icon { font-size: 14px; }
      .yaze-problems-title { font-weight: bold; }
      .yaze-problems-count {
        background: #ff6b6b;
        color: #1e1e1e;
        border-radius: 10px;
        padding: 0 6px;
        font-size: 11px;
      }
      .yaze-problems-toggle, .yaze-problems-clear, .yaze-problems-close {
        background: none;
        border: none;
        color: #888;
        cursor: pointer;
        margin-left: auto;
      }
      .yaze-problems-toggle:hover, .yaze-problems-clear:hover, .yaze-problems-close:hover { color: #fff; }
      .yaze-problems-list {
        overflow-y: auto;
        max-height: 170px;
        padding: 4px;
      }
      .yaze-problem-item {
        padding: 4px 8px;
        margin: 2px 0;
        background: #252525;
        border-left: 3px solid #ff6b6b;
        color: #ccc;
      }
      .yaze-problem-item.warning { border-left-color: #ffb347; }
      .yaze-problem-item .time { color: #666; margin-right: 8px; }
      .yaze-problem-item .category { color: #888; margin-right: 8px; }
    `;
    document.head.appendChild(style);

    // Event handlers
    panel.querySelector('.yaze-problems-toggle').onclick = () => {
      panel.classList.toggle('collapsed');
    };
    panel.querySelector('.yaze-problems-clear').onclick = () => {
      problemsList = [];
      updateProblemsPanel();
    };
    panel.querySelector('.yaze-problems-close').onclick = () => {
      panel.remove();
      problemsPanel = null;
    };

    document.body.appendChild(panel);
    problemsPanel = panel;
    return panel;
  }

  function updateProblemsPanel() {
    if (!problemsPanel) return;

    const list = problemsPanel.querySelector('.yaze-problems-list');
    const count = problemsPanel.querySelector('.yaze-problems-count');

    count.textContent = problemsList.length;
    list.innerHTML = problemsList.map(p => `
      <div class="yaze-problem-item ${p.severity}">
        <span class="time">${p.time}</span>
        <span class="category">[${p.category}]</span>
        ${p.message}
      </div>
    `).join('');

    if (problemsList.length === 0 && problemsPanel) {
      problemsPanel.remove();
      problemsPanel = null;
    }
  }

  /**
   * Add a problem to the panel (non-blocking)
   */
  function addProblem(message, category, severity) {
    const problem = {
      time: new Date().toLocaleTimeString(),
      message: message,
      category: category || 'Runtime',
      severity: severity || 'error'
    };

    problemsList.push(problem);
    if (problemsList.length > 50) problemsList.shift(); // Keep last 50

    createProblemsPanel();
    updateProblemsPanel();
  }

  /**
   * Display crash report in UI
   */
  function showCrashReport(report) {
    const text = formatCrashReportText(report);

    // Log to console
    originalConsole.error('=== YAZE CRASH REPORT ===');
    originalConsole.error(text);

    // Store for retrieval
    window.lastCrashReport = report;

    // Add to problems panel instead of blocking modal
    addProblem(
      `${report.error.name}: ${report.error.message} - ${report.diagnosis.likely_cause || 'Unknown cause'}`,
      report.diagnosis.category,
      'error'
    );

    // Show toast notification (non-blocking)
    if (window.showYazeToast) {
      window.showYazeToast(
        `Error: ${report.error.message.substring(0, 50)}...`,
        'error',
        5000
      );
    }

    // Copy to clipboard option
    if (navigator.clipboard) {
      navigator.clipboard.writeText(text).then(() => {
        originalConsole.log('Crash report copied to clipboard');
      }).catch(() => {});
    }

    return report;
  }

  /**
   * Global error handler
   */
  window.onerror = function(message, source, lineno, colno, error) {
    // Skip non-WASM errors for detailed reporting
    const isWasmError = (source && source.includes('.wasm')) ||
                        (error && error.stack && error.stack.includes('wasm'));

    if (isWasmError || (message && message.includes('RuntimeError'))) {
      const report = generateCrashReport(message, source, lineno, colno, error);
      showCrashReport(report);
    }

    // Don't prevent default handling
    return false;
  };

  /**
   * Unhandled promise rejection handler
   */
  window.onunhandledrejection = function(event) {
    const error = event.reason;
    if (error && (error.message || '').includes('wasm')) {
      const report = generateCrashReport(
        error.message || 'Unhandled Promise Rejection',
        'promise',
        0, 0,
        error
      );
      showCrashReport(report);
    }
  };

  // Expose for manual crash reporting and problems panel
  window.yazeCrashReporter = {
    generateReport: generateCrashReport,
    showReport: showCrashReport,
    formatText: formatCrashReportText,
    getBuildInfo: () => BUILD_INFO,
    getLogHistory: () => logHistory.slice(),
    getMemoryStats: getMemoryStats,
    getAppState: getAppState,
    // Problems panel API
    addProblem: addProblem,
    clearProblems: () => { problemsList = []; updateProblemsPanel(); },
    getProblems: () => problemsList.slice()
  };

  // Global shortcut for adding problems from anywhere
  window.yazeAddProblem = addProblem;

  console.log('[CrashReporter] Initialized with problems panel and WASM error tracking');

})();
