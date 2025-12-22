/*! coi-serviceworker v0.1.7 - Guido Zuidhof and contributors, licensed under MIT */
/*
 * This service worker enables SharedArrayBuffer support on hosts that don't
 * support setting COOP/COEP headers (like GitHub Pages).
 *
 * It intercepts all requests and adds the required headers:
 * - Cross-Origin-Opener-Policy: same-origin
 * - Cross-Origin-Embedder-Policy: require-corp
 */

// Allow users to reset COI state via URL parameter: ?reset-coi=1
if (typeof window !== 'undefined' && new URLSearchParams(window.location.search).has('reset-coi')) {
    console.log('[COI] Resetting COI state...');
    window.sessionStorage.removeItem("coiReloadedBySelf");
    window.sessionStorage.removeItem("coiAttempted");
    // Remove the parameter and reload
    const url = new URL(window.location);
    url.searchParams.delete('reset-coi');
    window.location.replace(url);
}

// BLOCKING CHECK: If we need COI setup, stop everything immediately
if (typeof window !== 'undefined' && typeof SharedArrayBuffer === 'undefined') {
    const n = navigator;
    const reloadedBySelf = window.sessionStorage.getItem("coiReloadedBySelf");

    if (reloadedBySelf) {
        // Already reloaded - give the SW time to take control
        // Don't immediately fail; the deferred code will handle further checks
        console.log('[COI] Waiting for service worker to take control...');
        // Don't set YAZE_COI_FAILED here - let the page continue and check again
    } else if (n.serviceWorker?.controller) {
        // SW is controlling but SAB not available, reload to apply headers
        console.log('[COI] Blocking: SW controlling, reloading for headers...');
        window.sessionStorage.setItem("coiReloadedBySelf", "true");
        window.stop();
        window.location.reload();
    } else if (n.serviceWorker) {
        // No SW controller yet - register and reload SYNCHRONOUSLY
        // Use document.write to prevent rest of page from loading
        console.log('[COI] First visit: Registering service worker...');
        window.sessionStorage.setItem("coiReloadedBySelf", "true");

        // Create non-blocking overlay instead of document.write
        const overlay = document.createElement('div');
        overlay.id = 'coi-setup-overlay';
        overlay.style.cssText = `
            position: fixed; top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(13,13,13,0.98); z-index: 999999;
            display: flex; align-items: center; justify-content: center;
            color: #0c6; font-family: monospace; font-size: 14px;
        `;
        overlay.innerHTML = `
            <div style="text-align: center;">
                <div class="coi-spinner" style="
                    border: 2px solid #333;
                    border-top: 2px solid #0c6;
                    border-radius: 50%;
                    width: 24px;
                    height: 24px;
                    animation: coi-spin 1s linear infinite;
                    margin: 0 auto 12px;
                "></div>
                <style>
                    @keyframes coi-spin {
                        to { transform: rotate(360deg); }
                    }
                </style>
                <div>Initializing secure context...</div>
                <div style="font-size: 11px; margin-top: 12px; opacity: 0.7;">
                    This is required for SharedArrayBuffer support
                </div>
            </div>
        `;
        (document.body || document.documentElement).appendChild(overlay);

        n.serviceWorker.register(window.document.currentScript.src)
            .then(() => {
                console.log('[COI] SW registered, reloading in 1s...');
                setTimeout(() => window.location.reload(), 1000);
            })
            .catch(err => {
                console.error('[COI] SW registration failed:', err);
                overlay.innerHTML = `
                    <div style="text-align: center; color: #f44;">
                        <div>Failed to enable SharedArrayBuffer support</div>
                        <div style="font-size: 12px; margin-top: 12px; opacity: 0.8;">
                            ${err.message || err}
                        </div>
                        <button onclick="location.reload()" style="
                            margin-top: 16px; padding: 8px 16px; cursor: pointer;
                            background: #333; color: #fff; border: 1px solid #555;
                            border-radius: 4px; font-family: monospace;
                        ">Retry</button>
                    </div>
                `;
            });

        // Don't throw - let page continue loading in background
    }
}

let coepCredentialless = false;
if (typeof window === 'undefined') {
    self.addEventListener("install", () => self.skipWaiting());
    self.addEventListener("activate", (e) => e.waitUntil(self.clients.claim()));

    self.addEventListener("message", (ev) => {
        if (!ev.data) {
            return;
        } else if (ev.data.type === "deregister") {
            self.registration
                .unregister()
                .then(() => {
                    return self.clients.matchAll();
                })
                .then((clients) => {
                    clients.forEach((client) => client.navigate(client.url));
                });
        } else if (ev.data.type === "coepCredentialless") {
            coepCredentialless = ev.data.value;
        }
    });

    self.addEventListener("fetch", function (event) {
        const r = event.request;
        // Skip invalid or empty URLs
        if (!r.url || r.url === '' || r.url === 'about:blank') {
            return;
        }
        if (r.cache === "only-if-cached" && r.mode !== "same-origin") {
            return;
        }
        // For worker scripts, we need to ensure proper content-type
        const isWorker = r.url.endsWith('.worker.js') || r.destination === 'worker';

        const request =
            coepCredentialless && r.mode === "no-cors"
                ? new Request(r, {
                      credentials: "omit",
                  })
                : r;
        event.respondWith(
            fetch(request)
                .then(async (response) => {
                    if (response.status === 0) {
                        return response;
                    }

                    const newHeaders = new Headers(response.headers);
                    newHeaders.set(
                        "Cross-Origin-Embedder-Policy",
                        coepCredentialless ? "credentialless" : "require-corp"
                    );
                    newHeaders.set("Cross-Origin-Opener-Policy", "same-origin");
                    // Required for Firefox require-corp mode to allow subresources
                    newHeaders.set("Cross-Origin-Resource-Policy", "same-origin");

                    // Ensure worker scripts have correct MIME type
                    if (isWorker) {
                        newHeaders.set("Content-Type", "application/javascript");
                    }

                    // Use blob() to properly handle the response body
                    const blob = await response.blob();
                    return new Response(blob, {
                        status: response.status,
                        statusText: response.statusText,
                        headers: newHeaders,
                    });
                })
                .catch((e) => {
                    // Only log non-empty URLs to reduce noise
                    if (r.url && r.url !== '') {
                        console.warn("[COI] Fetch failed for:", r.url, e.message || e);
                    }
                    // CRITICAL: Must return a response, not undefined
                    return new Response(null, {
                        status: 502,
                        statusText: "COI Fetch Error",
                        headers: new Headers({
                            "Cross-Origin-Embedder-Policy": coepCredentialless ? "credentialless" : "require-corp",
                            "Cross-Origin-Opener-Policy": "same-origin",
                            "Cross-Origin-Resource-Policy": "same-origin"
                        })
                    });
                })
        );
    });
} else {
    (() => {
        const reloadedBySelf = window.sessionStorage.getItem("coiReloadedBySelf");
        // DON'T remove the flag yet - only remove it once COI is confirmed working
        // This prevents the reload loop where the flag gets cleared before the reload completes
        const coepDegrading = (reloadedBySelf === "coepDegrade");

        // Clear session flags on successful COI setup
        if (window.crossOriginIsolated) {
            window.sessionStorage.removeItem("coiAttempted");
            window.sessionStorage.removeItem("coiReloadedBySelf");
        }

        // Detect Firefox - it needs require-corp, not credentialless
        const isFirefox = navigator.userAgent.includes('Firefox');

        // You can customize the behavior via these options:
        const coi = {
            // Skip registration if already handled by blocking check above
            shouldRegister: () => !reloadedBySelf && typeof SharedArrayBuffer === 'undefined',
            shouldDeregister: () => false,
            // Firefox needs require-corp mode, Chrome/Edge can use credentialless
            coepCredentialless: () => !isFirefox,
            coepDegrade: () => true,
            doReload: () => {
                window.sessionStorage.setItem("coiAttempted", "true");
                window.location.reload();
            },
            quiet: false,
            ...window.coi
        };

        if (!coi.quiet) {
            console.log('[COI] Browser:', isFirefox ? 'Firefox' : 'Chromium');
            console.log('[COI] Using COEP mode:', isFirefox ? 'require-corp' : 'credentialless');
        }

        const n = navigator;
        const controlling = n.serviceWorker && n.serviceWorker.controller;

        // Record the fact that we're unregistering, so that on the next reload we don't register again.
        if (controlling && coi.shouldDeregister()) {
            if(!coi.quiet) console.log("Unregistering COI Service Worker...");
            n.serviceWorker.controller.postMessage({ type: "deregister" });
            return;
        }

        // If this is the first load, check whether the browser supports SharedArrayBuffer
        // without COOP/COEP headers, in which case there's nothing to do.
        if (
            !reloadedBySelf &&
            window.crossOriginIsolated !== false &&
            window.SharedArrayBuffer !== undefined
        ) {
            // Clear any previous attempt markers - COI is working
            window.sessionStorage.removeItem("coiAttempted");
            return;
        }

        // If COI is now working after our reload, clear session markers
        if (window.crossOriginIsolated && window.SharedArrayBuffer !== undefined) {
            window.sessionStorage.removeItem("coiAttempted");
            window.sessionStorage.removeItem("coiReloadedBySelf");
            if (!coi.quiet) console.log("COI: SharedArrayBuffer enabled successfully!");
            return;
        }

        // In some environments (e.g. Chrome incognito), the coi service worker breaks some
        // cross-origin requests. Setting coepDegrade tells the service worker to use
        // credentialless mode for COEP. This is less restrictive but should still enable
        // SharedArrayBuffer.
        if (n.serviceWorker && coi.shouldRegister()) {
            if (!coi.quiet) {
                console.log("COI: Registering service worker for SharedArrayBuffer support...");
            }
            n.serviceWorker
                .register(window.document.currentScript.src)
                .then(
                    (registration) => {
                        if(!coi.quiet) console.log("COI: Service worker registered:", registration.scope);

                        registration.addEventListener("updatefound", () => {
                            if(!coi.quiet) console.log("COI: Checking for updates...");

                            if (
                                !registration.active ||
                                registration.waiting ||
                                registration.installing
                            ) {
                                // First load, use the new service worker right away
                                if (!coi.quiet) console.log("COI: Initial installation, will reload...");
                            }
                        });

                        // Reload after initial registration to enable COOP/COEP headers
                        if (!reloadedBySelf) {
                            // Wait for SW to be ready, then reload
                            const waitAndReload = () => {
                                if (!coi.quiet) console.log("COI: Service worker ready, reloading...");
                                window.sessionStorage.setItem("coiReloadedBySelf", coi.coepDegrade() ? "coepDegrade" : "true");
                                coi.doReload();
                            };

                            if (registration.active && !controlling) {
                                // SW is active but not controlling - reload now
                                waitAndReload();
                            } else if (registration.installing || registration.waiting) {
                                // Wait for SW to activate
                                const sw = registration.installing || registration.waiting;
                                sw.addEventListener('statechange', () => {
                                    if (sw.state === 'activated' && !navigator.serviceWorker.controller) {
                                        waitAndReload();
                                    }
                                });
                            }
                        }
                    },
                    (err) => {
                        if (!coi.quiet) console.error("COI: Service worker registration failed:", err);
                    }
                );

            n.serviceWorker.controller?.postMessage({
                type: "coepCredentialless",
                value: coi.coepCredentialless() || coepDegrading,
            });
        } else if (n.serviceWorker) {
            // shouldRegister() returned false - we already reloaded
            if (!coi.quiet) console.log("COI: Skipping registration (already reloaded)");

            // Send config to controller if available
            n.serviceWorker.controller?.postMessage({
                type: "coepCredentialless",
                value: coi.coepCredentialless() || coepDegrading,
            });

            // If we've reloaded but SW still not controlling, wait for it
            if (reloadedBySelf && !n.serviceWorker.controller) {
                if (!coi.quiet) console.log("COI: Waiting for SW to take control...");

                // Listen for controller change
                n.serviceWorker.addEventListener('controllerchange', () => {
                    if (!coi.quiet) console.log("COI: Controller changed, checking SAB...");

                    // Give browser a moment to update isolation state
                    setTimeout(() => {
                        if (typeof SharedArrayBuffer !== 'undefined') {
                            if (!coi.quiet) console.log("COI: SharedArrayBuffer now available!");
                            window.sessionStorage.removeItem("coiReloadedBySelf");
                            window.sessionStorage.removeItem("coiAttempted");
                            window.location.reload();
                        } else {
                            // SW is controlling but SAB still not available
                            // This means COI actually failed
                            console.error('[COI] Failed: SW controlling but SharedArrayBuffer unavailable');
                            window.YAZE_COI_FAILED = true;
                            window.sessionStorage.removeItem("coiReloadedBySelf");
                        }
                    }, 100);
                });

                // Also set a timeout in case controllerchange never fires
                setTimeout(() => {
                    if (!n.serviceWorker.controller && typeof SharedArrayBuffer === 'undefined') {
                        console.error('[COI] Timeout: SW failed to take control');
                        window.YAZE_COI_FAILED = true;
                        window.sessionStorage.removeItem("coiReloadedBySelf");
                    }
                }, 5000);
            } else if (reloadedBySelf && n.serviceWorker.controller && typeof SharedArrayBuffer === 'undefined') {
                // SW is controlling but COI failed
                console.error('[COI] Failed: SW controlling but SharedArrayBuffer unavailable. Browser may not support credentialless COEP.');
                window.YAZE_COI_FAILED = true;
                window.sessionStorage.removeItem("coiReloadedBySelf");
            }
        } else {
            if (!coi.quiet) {
                console.warn("COI: Service workers are not supported in this environment.");
            }
        }
    })();
}
