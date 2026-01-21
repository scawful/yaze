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
    const CACHE_NAME = 'yaze-cache-v5';
    const RUNTIME_CACHE = 'yaze-runtime-v5';
    const CACHE_METADATA = 'yaze-metadata-v2';
    const MAX_RUNTIME_CACHE_SIZE = 50;
    const CACHE_TTL_MS = 24 * 60 * 60 * 1000;
    const STALE_WHILE_REVALIDATE_PATTERNS = [
        /\.css$/,
        /\/styles\//,
        /\/icons\//,
        /\/assets\//
    ];

    function scopeUrl(path) {
        return new URL(path, self.registration.scope).toString();
    }

    function getPrecacheAssets() {
        return [
            '',
            'index.html',
            'yaze.js',
            'yaze.wasm',
            'app.js',
            'coi-serviceworker.js',
            'pwa/offline.html',
            'core/namespace.js',
            'core/config.js',
            'core/filesystem_manager.js',
            'core/loading_indicator.js',
            'core/error_handler.js',
            'core/crash_reporter.js',
            'core/wasm_recovery.js',
            'core/debug.js',
            'components/shortcuts_overlay.js',
            'components/terminal.js',
            'components/collab_console.js',
            'components/touch_gestures.js',
            'components/drop_zone.js',
            'styles/main.css',
            'styles/loading_indicator.css',
            'styles/error_handler.css',
            'styles/shortcuts_overlay.css',
            'styles/terminal.css',
            'styles/collab_console.css',
            'styles/touch_gestures.css',
            'styles/drop_zone.css'
        ].map(scopeUrl);
    }

    function shouldUseStaleWhileRevalidate(url) {
        return STALE_WHILE_REVALIDATE_PATTERNS.some(pattern => pattern.test(url));
    }

    async function setCacheTimestamp(url, timestamp) {
        const metadataCache = await caches.open(CACHE_METADATA);
        const metadata = new Response(JSON.stringify({ timestamp }));
        await metadataCache.put(url + '__meta', metadata);
    }

    async function getCacheTimestamp(url) {
        try {
            const metadataCache = await caches.open(CACHE_METADATA);
            const response = await metadataCache.match(url + '__meta');
            if (response) {
                const data = await response.json();
                return data.timestamp;
            }
        } catch (e) {
            console.warn('[ServiceWorker] Error reading cache timestamp:', e);
        }
        return null;
    }

    async function trimRuntimeCache() {
        const cache = await caches.open(RUNTIME_CACHE);
        const keys = await cache.keys();
        if (keys.length > MAX_RUNTIME_CACHE_SIZE) {
            const entriesToDelete = keys.length - MAX_RUNTIME_CACHE_SIZE;
            for (let i = 0; i < entriesToDelete; i++) {
                await cache.delete(keys[i]);
            }
        }
    }

    async function evictExpiredEntries() {
        const cache = await caches.open(RUNTIME_CACHE);
        const metadataCache = await caches.open(CACHE_METADATA);
        const keys = await cache.keys();
        const now = Date.now();
        for (const request of keys) {
            const timestamp = await getCacheTimestamp(request.url);
            if (timestamp && (now - timestamp) > CACHE_TTL_MS) {
                await cache.delete(request);
                await metadataCache.delete(request.url + '__meta');
            }
        }
    }

    async function addToRuntimeCacheWithEviction(request, response) {
        const cache = await caches.open(RUNTIME_CACHE);
        await cache.put(request, response);
        await setCacheTimestamp(request.url, Date.now());
        await trimRuntimeCache();
        await evictExpiredEntries();
    }

    async function addCoiHeaders(response, isWorker) {
        if (!response || response.status === 0) {
            return response;
        }

        const newHeaders = new Headers(response.headers);
        newHeaders.set(
            "Cross-Origin-Embedder-Policy",
            coepCredentialless ? "credentialless" : "require-corp"
        );
        newHeaders.set("Cross-Origin-Opener-Policy", "same-origin");
        newHeaders.set("Cross-Origin-Resource-Policy", "same-origin");

        if (isWorker) {
            newHeaders.set("Content-Type", "application/javascript");
        }

        const blob = await response.blob();
        return new Response(blob, {
            status: response.status,
            statusText: response.statusText,
            headers: newHeaders,
        });
    }

    async function fetchWithCoi(request, isWorker) {
        const response = await fetch(request);
        return addCoiHeaders(response, isWorker);
    }

    async function staleWhileRevalidate(cacheKey, fetchRequest, isWorker) {
        const cache = await caches.open(RUNTIME_CACHE);
        const cachedResponse = await cache.match(cacheKey);
        const fetchPromise = fetchWithCoi(fetchRequest, isWorker)
            .then(async (networkResponse) => {
                if (networkResponse && networkResponse.status === 200) {
                    await addToRuntimeCacheWithEviction(
                        cacheKey,
                        networkResponse.clone()
                    );
                }
                return networkResponse;
            })
            .catch((error) => {
                console.warn('[ServiceWorker] Background fetch failed:', error);
                return null;
            });

        if (cachedResponse) {
            fetchPromise;
            return cachedResponse;
        }

        return fetchPromise;
    }

    self.addEventListener("install", (event) => {
        self.skipWaiting();
        event.waitUntil(
            caches.open(CACHE_NAME)
                .then((cache) => {
                    return Promise.allSettled(
                        getPrecacheAssets().map((url) => {
                            return cache.add(url).catch((error) => {
                                console.warn('[ServiceWorker] Failed to cache', url, error);
                            });
                        })
                    );
                })
        );
    });

    self.addEventListener("activate", (event) => {
        event.waitUntil(
            caches.keys()
                .then((cacheNames) => {
                    return Promise.all(
                        cacheNames
                            .filter((cacheName) => {
                                return cacheName.startsWith('yaze-') &&
                                       cacheName !== CACHE_NAME &&
                                       cacheName !== RUNTIME_CACHE &&
                                       cacheName !== CACHE_METADATA;
                            })
                            .map((cacheName) => caches.delete(cacheName))
                    );
                })
                .then(() => evictExpiredEntries())
                .then(() => self.clients.claim())
        );
    });

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
        } else if (ev.data.type === "SKIP_WAITING") {
            self.skipWaiting();
        } else if (ev.data.type === "CACHE_VERSION" && ev.ports && ev.ports[0]) {
            ev.ports[0].postMessage({ version: CACHE_NAME });
        }
    });

    self.addEventListener("fetch", function (event) {
        const r = event.request;
        if (!r.url || r.url === '' || r.url === 'about:blank') {
            return;
        }
        if (r.cache === "only-if-cached" && r.mode !== "same-origin") {
            return;
        }

        const isWorker = r.url.endsWith('.worker.js') || r.destination === 'worker';
        const cacheKey = r;
        const fetchRequest =
            coepCredentialless && r.mode === "no-cors"
                ? new Request(r, { credentials: "omit" })
                : r;

        const url = new URL(r.url);
        if (url.origin !== self.location.origin) {
            event.respondWith(
                fetchWithCoi(fetchRequest, isWorker).catch((e) => {
                    if (r.url && r.url !== '') {
                        console.warn("[COI] Fetch failed for:", r.url, e.message || e);
                    }
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
            return;
        }

        if (r.mode === 'navigate') {
            event.respondWith(
                caches.match(cacheKey).then((cachedResponse) => {
                    if (cachedResponse) {
                        return cachedResponse;
                    }
                    return fetchWithCoi(fetchRequest, isWorker)
                        .then(async (networkResponse) => {
                            if (networkResponse && networkResponse.status === 200) {
                                await addToRuntimeCacheWithEviction(
                                    cacheKey,
                                    networkResponse.clone()
                                );
                            }
                            return networkResponse;
                        })
                        .catch(() => caches.match(scopeUrl('pwa/offline.html')));
                })
            );
            return;
        }

        if (r.url.endsWith('.wasm') ||
            r.url.endsWith('.data') ||
            r.url.endsWith('.js')) {
            event.respondWith(
                caches.match(cacheKey).then((cachedResponse) => {
                    if (cachedResponse) {
                        return cachedResponse;
                    }
                    return fetchWithCoi(fetchRequest, isWorker)
                        .then(async (networkResponse) => {
                            if (networkResponse && networkResponse.status === 200) {
                                const cache = await caches.open(CACHE_NAME);
                                await cache.put(cacheKey, networkResponse.clone());
                            }
                            return networkResponse;
                        });
                })
            );
            return;
        }

        if (shouldUseStaleWhileRevalidate(r.url)) {
            event.respondWith(staleWhileRevalidate(cacheKey, fetchRequest, isWorker));
            return;
        }

        event.respondWith(
            fetchWithCoi(fetchRequest, isWorker)
                .then(async (networkResponse) => {
                    if (networkResponse && networkResponse.status === 200) {
                        await addToRuntimeCacheWithEviction(
                            cacheKey,
                            networkResponse.clone()
                        );
                    }
                    return networkResponse;
                })
                .catch(() => caches.match(cacheKey))
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
