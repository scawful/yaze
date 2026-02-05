const { test, expect } = require('@playwright/test');
const fs = require('fs');
const path = require('path');

const baseUrl = process.env.YAZE_WASM_URL || 'https://yaze.halext.org';

async function waitForWasmReady(page, timeout = 120000) {
  await page.waitForFunction(
    () => {
      if (window.yaze && window.yaze.core && window.yaze.core.state &&
          window.yaze.core.state.wasmReady) {
        return true;
      }
      if (typeof Module !== 'undefined') {
        return Module.calledRun === true ||
          typeof Module.ccall === 'function' ||
          typeof Module._Z3edProcessCommand === 'function';
      }
      return false;
    },
    null,
    { timeout }
  );
}

test.describe('yaze wasm smoke', () => {
  test('terminal + filesystem operations', async ({ page }) => {
    test.setTimeout(180000);

    await page.goto(baseUrl, { waitUntil: 'domcontentloaded' });

    const overlay = page.locator('#loading-overlay');
    await overlay.waitFor({ state: 'hidden', timeout: 120000 });

    const terminalToggle = page.locator(
      '.nav-btn.panel-toggle[data-panel="terminal"]'
    );
    if (await terminalToggle.count()) {
      await expect(terminalToggle.first()).toBeVisible({ timeout: 60000 });
      await terminalToggle.first().click();
    } else {
      await page.waitForFunction(
        () => window.panels && window.panels.container,
        null,
        { timeout: 60000 }
      );
      await page.evaluate(() => window.panels.open('terminal'));
    }

    const input = page.locator('#terminal-input');
    await expect(input).toBeVisible({ timeout: 60000 });

    await page.waitForFunction(
      () => window.z3edTerminal && window.z3edTerminal.isModuleReady === true,
      null,
      { timeout: 120000 }
    );

    await input.fill('/help');
    await input.press('Enter');
    await expect(page.locator('#terminal-output')).toContainText(
      'z3ed Web Terminal Help',
      { timeout: 10000 }
    );

    await input.fill('/version');
    await input.press('Enter');
    await expect(page.locator('#terminal-output')).toContainText(
      'z3ed Web Terminal v1.0.0',
      { timeout: 10000 }
    );

    await page.waitForFunction(
      () => window.FilesystemManager && window.FilesystemManager.ready === true,
      null,
      { timeout: 120000 }
    );

    const fsResult = await page.evaluate(() => {
      const path = '/projects/playwright-smoke.txt';
      if (!window.FilesystemManager || !FilesystemManager.ensureReady(false)) {
        return { ok: false, error: 'FilesystemManager not ready' };
      }
      const wrote = FilesystemManager.writeFile(path, 'playwright smoke');
      const exists = FilesystemManager.fileExists(path);
      const content = FilesystemManager.readFile(path, 'utf8');
      const deleted = FilesystemManager.deleteFile(path);
      return { ok: true, wrote, exists, content, deleted };
    });

    expect(fsResult.ok).toBe(true);
    expect(fsResult.wrote).toBe(true);
    expect(fsResult.exists).toBe(true);
    expect(fsResult.content).toContain('playwright smoke');
    expect(fsResult.deleted).toBe(true);
  });

  test('debug api tests', async ({ page }) => {
    test.setTimeout(240000);

    await page.goto(baseUrl, { waitUntil: 'domcontentloaded' });

    const overlay = page.locator('#loading-overlay');
    await overlay.waitFor({ state: 'hidden', timeout: 120000 });

    await waitForWasmReady(page, 120000);

    const debugScriptPath = path.join(
      __dirname,
      '..',
      '..',
      'src',
      'web',
      'tests',
      'wasm_debug_api_tests.js'
    );
    const debugScript = fs.readFileSync(debugScriptPath, 'utf8');

    await page.addScriptTag({ content: debugScript });
    await page.waitForFunction(
      () => typeof window.runWasmDebugApiTests === 'function',
      null,
      { timeout: 30000 }
    );

    let results;
    try {
      results = await page.evaluate(async () => {
        if (typeof window.runWasmDebugApiTests !== 'function') {
          return { failed: 1, passed: 0, error: 'runWasmDebugApiTests missing' };
        }
        try {
          const testResults = await window.runWasmDebugApiTests();
          // Return only serializable properties
          return {
            failed: testResults?.failed ?? 0,
            passed: testResults?.passed ?? 0,
            skipped: testResults?.skipped ?? 0,
            total: testResults?.total ?? 0,
            error: null
          };
        } catch (e) {
          return { failed: 1, passed: 0, error: e.message || String(e) };
        }
      });
    } catch (evalError) {
      results = { failed: 1, passed: 0, error: `page.evaluate failed: ${evalError.message}` };
    }

    expect(results).toBeDefined();
    if (results.error) {
      console.log('Debug API test error:', results.error);
    }
    console.log(`Debug API tests: ${results.passed} passed, ${results.failed} failed, ${results.skipped || 0} skipped`);
    expect(results.failed).toBe(0);
  });
});
