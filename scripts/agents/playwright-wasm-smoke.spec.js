const { test, expect } = require('@playwright/test');

const baseUrl = process.env.YAZE_WASM_URL || 'https://yaze.halext.org';

test.describe('yaze wasm smoke', () => {
  test('terminal + filesystem operations', async ({ page }) => {
    test.setTimeout(180000);

    await page.goto(baseUrl, { waitUntil: 'domcontentloaded' });

    const overlay = page.locator('#loading-overlay');
    await overlay.waitFor({ state: 'hidden', timeout: 120000 });

    const terminalToggle = page.locator(
      '.nav-btn.panel-toggle[data-panel="terminal"]'
    );
    await expect(terminalToggle).toBeVisible({ timeout: 60000 });
    await terminalToggle.click();

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
});
