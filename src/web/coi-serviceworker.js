/*! coi-serviceworker v0.1.7 - Guido Zuidhof and contributors, licensed under MIT */
/*
 * This service worker enables SharedArrayBuffer support on hosts that don't
 * support setting COOP/COEP headers (like GitHub Pages).
 *
 * It intercepts all requests and adds the required headers:
 * - Cross-Origin-Opener-Policy: same-origin
 * - Cross-Origin-Embedder-Policy: require-corp
 */

// BLOCKING CHECK: If we need COI setup, stop everything immediately
if (typeof window !== 'undefined' && typeof SharedArrayBuffer === 'undefined') {
    const n = navigator;
    const reloadedBySelf = window.sessionStorage.getItem("coiReloadedBySelf");

    if (reloadedBySelf) {
        // Already reloaded but still no SAB - COI failed, let error show
        console.error('[COI] Failed: SharedArrayBuffer unavailable after COI setup.');
        window.YAZE_COI_FAILED = true;
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

        n.serviceWorker.register(window.document.currentScript.src).then(() => {
            console.log('[COI] SW registered, reloading...');
            window.location.reload();
        }).catch(err => {
            console.error('[COI] SW registration failed:', err);
        });

        // CRITICAL: Stop the page from loading further while SW registers
        // This prevents yaze.js from loading before COI is ready
        document.write('<html><head><meta charset="utf-8"><title>yaze - Loading...</title>' +
            '<style>body{background:#0d0d0d;color:#0c6;font-family:monospace;display:flex;' +
            'align-items:center;justify-content:center;height:100vh;margin:0;}' +
            '.loading{text-align:center}.spinner{border:2px solid #333;border-top:2px solid #0c6;' +
            'border-radius:50%;width:24px;height:24px;animation:spin 1s linear infinite;margin:0 auto 12px;}' +
            '@keyframes spin{to{transform:rotate(360deg)}}</style></head>' +
            '<body><div class="loading"><div class="spinner"></div>Initializing secure context...</div></body></html>');
        document.close();
        throw new Error('COI setup in progress - blocking page load');
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
        window.sessionStorage.removeItem("coiReloadedBySelf");
        const coepDegrading = (reloadedBySelf === "coepDegrade");

        // Clear coiAttempted on successful COI setup
        if (window.crossOriginIsolated) {
            window.sessionStorage.removeItem("coiAttempted");
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

        // If COI is now working after our reload, clear the attempt marker
        if (window.crossOriginIsolated && window.SharedArrayBuffer !== undefined) {
            window.sessionStorage.removeItem("coiAttempted");
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
            // shouldRegister() returned false - we already reloaded, just send config
            if (!coi.quiet) console.log("COI: Skipping registration (already reloaded)");
            n.serviceWorker.controller?.postMessage({
                type: "coepCredentialless",
                value: coi.coepCredentialless() || coepDegrading,
            });
        } else {
            if (!coi.quiet) {
                console.warn("COI: Service workers are not supported in this environment.");
            }
        }
    })();
}
