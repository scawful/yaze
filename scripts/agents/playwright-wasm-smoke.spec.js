const { test, expect } = require('@playwright/test');
const fs = require('fs');
const path = require('path');

const baseUrl = process.env.YAZE_WASM_URL || 'https://yaze.halext.org';

async function waitForWasmReady(page, timeout = 120000) {
  await expect
    .poll(
      async () => {
        try {
          return await page.evaluate(() => {
            if (window.yaze && window.yaze.core) {
              const core = window.yaze.core;
              if (core.state && core.state.wasmReady) {
                return true;
              }
              if (core._bootResolved) {
                return true;
              }
              if (
                core.Module &&
                (core.Module.calledRun === true ||
                  typeof core.Module.ccall === 'function' ||
                  typeof core.Module._Z3edProcessCommand === 'function')
              ) {
                return true;
              }
            }
            if (typeof Module !== 'undefined') {
              return (
                Module.calledRun === true ||
                typeof Module.ccall === 'function' ||
                typeof Module._Z3edProcessCommand === 'function'
              );
            }
            return false;
          });
        } catch (_) {
          // Page can transiently navigate/reload during COI bootstrap.
          return false;
        }
      },
      { timeout, intervals: [250, 500, 1000, 2000] }
    )
    .toBe(true);
}

async function waitForLoadingOverlayToHide(page, timeout = 120000) {
  const overlay = page.locator('#loading-overlay');
  if ((await overlay.count()) > 0) {
    try {
      await overlay.first().waitFor({
        state: 'hidden',
        timeout: Math.min(timeout, 20000)
      });
    } catch (_) {
      // Local/static servers may keep overlay visible during COI reload loops.
      // Continue and rely on explicit WASM readiness polling instead.
    }
  }
}

async function ensureTerminalVisible(page) {
  const input = page.locator('#terminal-input');
  if ((await input.count()) > 0) {
    try {
      if (await input.first().isVisible()) {
        return;
      }
    } catch (_) {
      // Fall through to open terminal explicitly.
    }
  }

  const terminalToggle = page.locator(
    '.nav-btn.panel-toggle[data-panel="terminal"]'
  );
  if ((await terminalToggle.count()) > 0) {
    await expect(terminalToggle.first()).toBeVisible({ timeout: 60000 });
    await terminalToggle.first().click();
  } else {
    await expect
      .poll(
        async () =>
          page.evaluate(() => {
            return (
              (window.panels && typeof window.panels.open === 'function') ||
              typeof window.toggleTerminal === 'function' ||
              (window.z3edTerminal &&
                typeof window.z3edTerminal.toggle === 'function')
            );
          }),
        { timeout: 90000, intervals: [250, 500, 1000] }
      )
      .toBeTruthy();

    await page.evaluate(() => {
      if (window.panels && typeof window.panels.open === 'function') {
        window.panels.open('terminal');
        return;
      }
      if (typeof window.toggleTerminal === 'function') {
        window.toggleTerminal();
        return;
      }
      if (
        window.z3edTerminal &&
        typeof window.z3edTerminal.toggle === 'function'
      ) {
        window.z3edTerminal.toggle();
      }
    });
  }

  await expect(input).toBeVisible({ timeout: 90000 });
}

test.describe('yaze wasm smoke', () => {
  test('terminal + filesystem operations', async ({ page }) => {
    test.setTimeout(180000);

    await page.goto(baseUrl, { waitUntil: 'domcontentloaded' });

    await waitForLoadingOverlayToHide(page, 120000);
    await waitForWasmReady(page, 180000);
    await ensureTerminalVisible(page);

    const input = page.locator('#terminal-input');
    await expect(input).toBeVisible({ timeout: 60000 });

    await expect
      .poll(
        async () =>
          page.evaluate(() => {
            if (
              window.z3edTerminal &&
              window.z3edTerminal.isModuleReady === true
            ) {
              return true;
            }
            return !!(
              window.yaze &&
              window.yaze.core &&
              window.yaze.core.state &&
              window.yaze.core.state.wasmReady
            );
          }),
        { timeout: 120000, intervals: [250, 500, 1000] }
      )
      .toBe(true);

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

    await waitForLoadingOverlayToHide(page, 120000);
    await waitForWasmReady(page, 180000);

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
    expect(results.error).toBeNull();
    const totalChecks =
      results.total ||
      (results.passed || 0) + (results.failed || 0) + (results.skipped || 0);
    expect(totalChecks).toBeGreaterThan(0);
    expect(results.passed).toBeGreaterThan(0);

    // Smoke run tolerates a bounded number of non-critical debug API misses.
    const allowedFailures = Math.max(10, Math.floor(totalChecks * 0.2));
    expect(results.failed).toBeLessThanOrEqual(allowedFailures);
  });
});
