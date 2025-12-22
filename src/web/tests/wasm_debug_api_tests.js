/**
 * WASM Debug API Test Suite
 *
 * Tests for the yazeDebug API, window.yaze.control, window.yaze.gui, and window.aiTools.
 * Run these tests in the browser console after the WASM module has loaded.
 *
 * Usage:
 *   await window.runWasmDebugApiTests()
 *
 * Or run individual test groups:
 *   window.wasmDebugTests.runControlAPITests()
 *   window.wasmDebugTests.runGuiAPITests()
 *   window.wasmDebugTests.runAiToolsTests()
 */

(function() {
  'use strict';

  // Test utilities
  const TestUtils = {
    passed: 0,
    failed: 0,
    skipped: 0,
    results: [],

    reset: function() {
      this.passed = 0;
      this.failed = 0;
      this.skipped = 0;
      this.results = [];
    },

    assert: function(condition, testName, details) {
      if (condition) {
        this.passed++;
        this.results.push({ name: testName, status: 'PASS', details: details });
        console.log(`✅ PASS: ${testName}`);
      } else {
        this.failed++;
        this.results.push({ name: testName, status: 'FAIL', details: details });
        console.error(`❌ FAIL: ${testName}`, details);
      }
    },

    skip: function(testName, reason) {
      this.skipped++;
      this.results.push({ name: testName, status: 'SKIP', details: reason });
      console.warn(`⏭️ SKIP: ${testName} - ${reason}`);
    },

    assertEqual: function(actual, expected, testName) {
      const condition = actual === expected;
      this.assert(condition, testName, { actual, expected });
    },

    assertType: function(value, expectedType, testName) {
      const actualType = typeof value;
      this.assert(actualType === expectedType, testName, { actualType, expectedType, value });
    },

    assertExists: function(value, testName) {
      this.assert(value !== undefined && value !== null, testName, { value });
    },

    assertFunction: function(obj, funcName, testName) {
      const exists = obj && typeof obj[funcName] === 'function';
      this.assert(exists, testName, { funcName, hasObj: !!obj, type: obj ? typeof obj[funcName] : 'undefined' });
    },

    assertArray: function(value, testName) {
      this.assert(Array.isArray(value), testName, { isArray: Array.isArray(value), value });
    },

    assertObject: function(value, testName) {
      this.assert(value !== null && typeof value === 'object' && !Array.isArray(value), testName, { type: typeof value, value });
    },

    assertProperty: function(obj, prop, testName) {
      this.assert(obj && prop in obj, testName, { hasProp: obj && prop in obj, prop, keys: obj ? Object.keys(obj) : [] });
    },

    summary: function() {
      console.log('\n=== Test Summary ===');
      console.log(`Total: ${this.passed + this.failed + this.skipped}`);
      console.log(`Passed: ${this.passed}`);
      console.log(`Failed: ${this.failed}`);
      console.log(`Skipped: ${this.skipped}`);

      if (this.failed > 0) {
        console.log('\nFailed tests:');
        this.results.filter(r => r.status === 'FAIL').forEach(r => {
          console.log(`  - ${r.name}: ${JSON.stringify(r.details)}`);
        });
      }

      return {
        total: this.passed + this.failed + this.skipped,
        passed: this.passed,
        failed: this.failed,
        skipped: this.skipped,
        results: this.results
      };
    }
  };

  // Test: Module readiness
  function testModuleReadiness() {
    console.log('\n--- Module Readiness Tests ---');

    TestUtils.assertExists(window.Module, 'Module exists on window');

    if (window.Module) {
      TestUtils.assertExists(window.Module.calledRun, 'Module.calledRun exists');
      TestUtils.assertEqual(window.Module.calledRun, true, 'Module has finished running');
    } else {
      TestUtils.skip('Module.calledRun check', 'Module not loaded');
    }
  }

  // Test: window.yaze namespace
  function testYazeNamespace() {
    console.log('\n--- Yaze Namespace Tests ---');

    TestUtils.assertExists(window.yaze, 'window.yaze namespace exists');

    if (window.yaze) {
      TestUtils.assertExists(window.yaze.control, 'window.yaze.control exists');
      TestUtils.assertExists(window.yaze.gui, 'window.yaze.gui exists');
      TestUtils.assertExists(window.yaze.editor, 'window.yaze.editor exists');
      TestUtils.assertExists(window.yaze.data, 'window.yaze.data exists');
      TestUtils.assertExists(window.yaze.session, 'window.yaze.session exists');
    } else {
      TestUtils.skip('Yaze submodules', 'window.yaze not available');
    }
  }

  // Test: Control API functions exist
  function testControlAPIFunctions() {
    console.log('\n--- Control API Function Tests ---');

    if (!window.yaze || !window.yaze.control) {
      TestUtils.skip('Control API tests', 'window.yaze.control not available');
      return;
    }

    const control = window.yaze.control;

    // Core editor functions
    TestUtils.assertFunction(control, 'switchEditor', 'switchEditor function exists');
    TestUtils.assertFunction(control, 'getCurrentEditor', 'getCurrentEditor function exists');
    TestUtils.assertFunction(control, 'getAvailableEditors', 'getAvailableEditors function exists');

    // Panel functions
    TestUtils.assertFunction(control, 'openPanel', 'openPanel function exists');
    TestUtils.assertFunction(control, 'closePanel', 'closePanel function exists');
    TestUtils.assertFunction(control, 'togglePanel', 'togglePanel function exists');
    TestUtils.assertFunction(control, 'getVisiblePanels', 'getVisiblePanels function exists');
    TestUtils.assertFunction(control, 'getAvailablePanels', 'getAvailablePanels function exists');

    // Layout functions
    TestUtils.assertFunction(control, 'setPanelLayout', 'setPanelLayout function exists');
    TestUtils.assertFunction(control, 'getAvailableLayouts', 'getAvailableLayouts function exists');

    // ROM functions
    TestUtils.assertFunction(control, 'getRomStatus', 'getRomStatus function exists');

    // Ready check
    TestUtils.assertFunction(control, 'isReady', 'isReady function exists');
  }

  // Test: Control API returns valid data
  function testControlAPIData() {
    console.log('\n--- Control API Data Tests ---');

    if (!window.yaze || !window.yaze.control) {
      TestUtils.skip('Control API data tests', 'window.yaze.control not available');
      return;
    }

    const control = window.yaze.control;

    // Test isReady
    if (typeof control.isReady === 'function') {
      const isReady = control.isReady();
      TestUtils.assertType(isReady, 'boolean', 'isReady returns boolean');
    }

    // Test getCurrentEditor
    if (typeof control.getCurrentEditor === 'function') {
      const editor = control.getCurrentEditor();
      TestUtils.assertObject(editor, 'getCurrentEditor returns object');
      if (editor && !editor.error) {
        TestUtils.assertProperty(editor, 'name', 'Editor has name property');
      }
    }

    // Test getAvailableEditors
    if (typeof control.getAvailableEditors === 'function') {
      const editors = control.getAvailableEditors();
      TestUtils.assertArray(editors, 'getAvailableEditors returns array');
      if (Array.isArray(editors)) {
        TestUtils.assert(editors.length > 0, 'Has available editors', { count: editors.length });
      }
    }

    // Test getRomStatus
    if (typeof control.getRomStatus === 'function') {
      const romStatus = control.getRomStatus();
      TestUtils.assertObject(romStatus, 'getRomStatus returns object');
      if (romStatus && !romStatus.error) {
        TestUtils.assertProperty(romStatus, 'loaded', 'ROM status has loaded property');
      }
    }

    // Test getVisiblePanels
    if (typeof control.getVisiblePanels === 'function') {
      const cards = control.getVisiblePanels();
      TestUtils.assertArray(cards, 'getVisiblePanels returns array');
    }

    // Test getAvailablePanels
    if (typeof control.getAvailablePanels === 'function') {
      const cards = control.getAvailablePanels();
      TestUtils.assertArray(cards, 'getAvailablePanels returns array');
    }
  }

  // Test: GUI Automation API
  function testGuiAPIFunctions() {
    console.log('\n--- GUI API Function Tests ---');

    if (!window.yaze || !window.yaze.gui) {
      TestUtils.skip('GUI API tests', 'window.yaze.gui not available');
      return;
    }

    const gui = window.yaze.gui;

    TestUtils.assertFunction(gui, 'discover', 'discover function exists');
    TestUtils.assertFunction(gui, 'click', 'click function exists');
    TestUtils.assertFunction(gui, 'doubleClick', 'doubleClick function exists');
    TestUtils.assertFunction(gui, 'drag', 'drag function exists');
    TestUtils.assertFunction(gui, 'getElementBounds', 'getElementBounds function exists');
    TestUtils.assertFunction(gui, 'waitForElement', 'waitForElement function exists');
    TestUtils.assertFunction(gui, 'takeScreenshot', 'takeScreenshot function exists');
    TestUtils.assertFunction(gui, 'getSelection', 'getSelection function exists');
    TestUtils.assertFunction(gui, 'setSelection', 'setSelection function exists');
    TestUtils.assertFunction(gui, 'pressKey', 'pressKey function exists');
    TestUtils.assertFunction(gui, 'type', 'type function exists');
    TestUtils.assertFunction(gui, 'scroll', 'scroll function exists');
    TestUtils.assertFunction(gui, 'getCanvasInfo', 'getCanvasInfo function exists');
    TestUtils.assertFunction(gui, 'isReady', 'isReady function exists');
    TestUtils.assertFunction(gui, 'updateCanvasState', 'updateCanvasState function exists');
    TestUtils.assertFunction(gui, 'getAvailablePanels', 'GUI getAvailablePanels exists');
    TestUtils.assertFunction(gui, 'showPanel', 'showPanel function exists');
    TestUtils.assertFunction(gui, 'hidePanel', 'hidePanel function exists');
    TestUtils.assertFunction(gui, 'startAutoUpdate', 'startAutoUpdate function exists');
    TestUtils.assertFunction(gui, 'stopAutoUpdate', 'stopAutoUpdate function exists');
  }

  // Test: GUI API data
  function testGuiAPIData() {
    console.log('\n--- GUI API Data Tests ---');

    if (!window.yaze || !window.yaze.gui) {
      TestUtils.skip('GUI API data tests', 'window.yaze.gui not available');
      return;
    }

    const gui = window.yaze.gui;

    // Test getCanvasInfo
    if (typeof gui.getCanvasInfo === 'function') {
      const canvasInfo = gui.getCanvasInfo();
      TestUtils.assertObject(canvasInfo, 'getCanvasInfo returns object');
      if (canvasInfo && !canvasInfo.error) {
        TestUtils.assertProperty(canvasInfo, 'width', 'Canvas info has width');
        TestUtils.assertProperty(canvasInfo, 'height', 'Canvas info has height');
      }
    }

    // Test takeScreenshot
    if (typeof gui.takeScreenshot === 'function') {
      const screenshot = gui.takeScreenshot();
      TestUtils.assertObject(screenshot, 'takeScreenshot returns object');
      if (screenshot && !screenshot.error) {
        TestUtils.assertProperty(screenshot, 'dataUrl', 'Screenshot has dataUrl');
        TestUtils.assert(screenshot.dataUrl.startsWith('data:image/'), 'Screenshot dataUrl is valid', { prefix: screenshot.dataUrl.substring(0, 20) });
      }
    }

    // Test discover
    if (typeof gui.discover === 'function') {
      const elements = gui.discover();
      TestUtils.assertObject(elements, 'discover returns object');
      if (elements && !elements.error) {
        TestUtils.assertProperty(elements, 'elements', 'Discover result has elements array');
      }
    }

    // Test updateCanvasState
    if (typeof gui.updateCanvasState === 'function') {
      const state = gui.updateCanvasState();
      TestUtils.assertObject(state, 'updateCanvasState returns object');
    }
  }

  // Test: AI Tools API
  function testAiToolsFunctions() {
    console.log('\n--- AI Tools Function Tests ---');

    if (!window.aiTools) {
      TestUtils.skip('AI Tools tests', 'window.aiTools not available');
      return;
    }

    TestUtils.assertFunction(window.aiTools, 'getAppState', 'getAppState function exists');
    TestUtils.assertFunction(window.aiTools, 'getEditorState', 'getEditorState function exists');
    TestUtils.assertFunction(window.aiTools, 'getVisiblePanels', 'getVisiblePanels function exists');
    TestUtils.assertFunction(window.aiTools, 'getAvailablePanels', 'getAvailablePanels function exists');
    TestUtils.assertFunction(window.aiTools, 'navigateTo', 'navigateTo function exists');
    TestUtils.assertFunction(window.aiTools, 'jumpToRoom', 'jumpToRoom function exists');
    TestUtils.assertFunction(window.aiTools, 'jumpToMap', 'jumpToMap function exists');
    TestUtils.assertFunction(window.aiTools, 'getRoomData', 'getRoomData function exists');
    TestUtils.assertFunction(window.aiTools, 'getMapData', 'getMapData function exists');
    TestUtils.assertFunction(window.aiTools, 'dumpAPIReference', 'dumpAPIReference function exists');
  }

  // Test: AI Tools data
  function testAiToolsData() {
    console.log('\n--- AI Tools Data Tests ---');

    if (!window.aiTools) {
      TestUtils.skip('AI Tools data tests', 'window.aiTools not available');
      return;
    }

    // Test getAppState
    const appState = window.aiTools.getAppState();
    TestUtils.assertObject(appState, 'getAppState returns object');
    if (appState) {
      TestUtils.assertProperty(appState, 'timestamp', 'App state has timestamp');
      TestUtils.assertProperty(appState, 'moduleReady', 'App state has moduleReady');
      TestUtils.assertProperty(appState, 'yazeApiReady', 'App state has yazeApiReady');
    }

    // Test dumpAPIReference
    const apiRef = window.aiTools.dumpAPIReference();
    TestUtils.assertObject(apiRef, 'dumpAPIReference returns object');
    if (apiRef) {
      TestUtils.assertProperty(apiRef, 'modules', 'API reference has modules');
      TestUtils.assertProperty(apiRef, 'editors', 'API reference has editors list');
    }
  }

  // Test: yazeDebug compatibility API
  function testYazeDebugAPI() {
    console.log('\n--- yazeDebug Compatibility API Tests ---');

    if (!window.yazeDebug) {
      TestUtils.skip('yazeDebug tests', 'window.yazeDebug not available');
      return;
    }

    // Check for expected functions
    TestUtils.assertFunction(window.yazeDebug, 'dumpAll', 'yazeDebug.dumpAll exists');

    // Check for cards namespace
    if (window.yazeDebug.cards) {
      TestUtils.assertFunction(window.yazeDebug.cards, 'show', 'yazeDebug.cards.show exists');
      TestUtils.assertFunction(window.yazeDebug.cards, 'hide', 'yazeDebug.cards.hide exists');
      TestUtils.assertFunction(window.yazeDebug.cards, 'showGroup', 'yazeDebug.cards.showGroup exists');
    } else {
      TestUtils.skip('yazeDebug.cards tests', 'cards namespace not available');
    }

    // Check for async wrappers
    TestUtils.assertFunction(window.yazeDebug, 'switchToEditorAsync', 'switchToEditorAsync exists');
  }

  // Test: Editor switching (non-destructive)
  async function testEditorSwitching() {
    console.log('\n--- Editor Switching Tests ---');

    if (!window.yaze || !window.yaze.control || typeof window.yaze.control.switchEditor !== 'function') {
      TestUtils.skip('Editor switching tests', 'switchEditor not available');
      return;
    }

    const control = window.yaze.control;

    // Get current editor to restore later
    const originalEditor = control.getCurrentEditor();
    if (!originalEditor || originalEditor.error) {
      TestUtils.skip('Editor switching', 'Cannot get current editor');
      return;
    }

    // Test switching to Dungeon
    control.switchEditor('Dungeon');
    await new Promise(r => setTimeout(r, 100)); // Small delay for switch

    const afterSwitch = control.getCurrentEditor();
    TestUtils.assert(afterSwitch && afterSwitch.name === 'Dungeon', 'Can switch to Dungeon editor', { currentEditor: afterSwitch });

    // Restore original editor
    if (originalEditor.name && originalEditor.name !== 'Dungeon') {
      control.switchEditor(originalEditor.name);
    }
  }

  // Test: Panel operations (non-destructive)
  async function testPanelOperations() {
    console.log('\n--- Panel Operations Tests ---');

    if (!window.yaze || !window.yaze.control) {
      TestUtils.skip('Panel operations tests', 'window.yaze.control not available');
      return;
    }

    const control = window.yaze.control;

    // Get initial card state
    const initialPanels = control.getVisiblePanels();
    if (!Array.isArray(initialPanels)) {
      TestUtils.skip('Panel operations', 'Cannot get visible cards');
      return;
    }

    // Test togglePanel (if a card is available)
    const availablePanels = control.getAvailablePanels();
    if (Array.isArray(availablePanels) && availablePanels.length > 0) {
      const testPanel = availablePanels[0];
      const cardId = testPanel.id || testPanel;

      // Toggle card
      const toggleResult = control.togglePanel(cardId);
      TestUtils.assertObject(toggleResult, 'togglePanel returns object');

      // Toggle back to restore state
      await new Promise(r => setTimeout(r, 50));
      control.togglePanel(cardId);
    } else {
      TestUtils.skip('Panel toggle test', 'No available cards');
    }
  }

  // Run all tests
  async function runAllTests() {
    console.log('========================================');
    console.log('  WASM Debug API Test Suite');
    console.log('========================================');

    TestUtils.reset();

    // Module and namespace tests
    testModuleReadiness();
    testYazeNamespace();

    // Control API tests
    testControlAPIFunctions();
    testControlAPIData();

    // GUI API tests
    testGuiAPIFunctions();
    testGuiAPIData();

    // AI Tools tests
    testAiToolsFunctions();
    testAiToolsData();

    // yazeDebug compatibility
    testYazeDebugAPI();

    // Interactive tests (non-destructive)
    await testEditorSwitching();
    await testPanelOperations();

    return TestUtils.summary();
  }

  // Expose test functions
  window.wasmDebugTests = {
    TestUtils: TestUtils,
    testModuleReadiness: testModuleReadiness,
    testYazeNamespace: testYazeNamespace,
    testControlAPIFunctions: testControlAPIFunctions,
    testControlAPIData: testControlAPIData,
    testGuiAPIFunctions: testGuiAPIFunctions,
    testGuiAPIData: testGuiAPIData,
    testAiToolsFunctions: testAiToolsFunctions,
    testAiToolsData: testAiToolsData,
    testYazeDebugAPI: testYazeDebugAPI,
    testEditorSwitching: testEditorSwitching,
    testPanelOperations: testPanelOperations,
    runAll: runAllTests,

    // Test groups
    runControlAPITests: function() {
      TestUtils.reset();
      testControlAPIFunctions();
      testControlAPIData();
      return TestUtils.summary();
    },

    runGuiAPITests: function() {
      TestUtils.reset();
      testGuiAPIFunctions();
      testGuiAPIData();
      return TestUtils.summary();
    },

    runAiToolsTests: function() {
      TestUtils.reset();
      testAiToolsFunctions();
      testAiToolsData();
      return TestUtils.summary();
    }
  };

  // Main entry point
  window.runWasmDebugApiTests = runAllTests;

  console.log('[wasm_debug_api_tests] Tests loaded. Run: await window.runWasmDebugApiTests()');

})();
