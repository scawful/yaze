/*! coi-serviceworker v0.1.7 - Guido Zuidhof and contributors, licensed under MIT */
/*
 * This service worker enables SharedArrayBuffer support on hosts that don't
 * support setting COOP/COEP headers (like GitHub Pages).
 *
 * It intercepts all requests and adds the required headers:
 * - Cross-Origin-Opener-Policy: same-origin
 * - Cross-Origin-Embedder-Policy: require-corp
 */

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

    // Throttle cache eviction to avoid O(n) operations on every write
    let lastEvictionTime = 0;
    const EVICTION_THROTTLE_MS = 60000; // Run eviction at most once per minute

    async function addToRuntimeCacheWithEviction(request, response) {
        const cache = await caches.open(RUNTIME_CACHE);
        await cache.put(request, response);
        await setCacheTimestamp(request.url, Date.now());

        // Only run eviction if throttle period has passed
        const now = Date.now();
        if (now - lastEvictionTime > EVICTION_THROTTLE_MS) {
            lastEvictionTime = now;
            // Run eviction async without blocking the response
            trimRuntimeCache().catch(e => console.warn('[SW] Trim failed:', e));
            evictExpiredEntries().catch(e => console.warn('[SW] Eviction failed:', e));
        }
    }

    async function addCoiHeaders(response, isWorker) {
        if (!response || response.status === 0) {
            return response;
        }

        const newHeaders = new Headers(response.headers);
        newHeaders.set("Cross-Origin-Embedder-Policy", "require-corp");
        newHeaders.set("Cross-Origin-Opener-Policy", "same-origin");
        newHeaders.set("Cross-Origin-Resource-Policy", "same-origin");

        if (isWorker) {
            newHeaders.set("Content-Type", "application/javascript");
        }

        // Use streaming response.body instead of buffering with blob()
        // This avoids memory spikes for large assets like yaze.wasm
        return new Response(response.body, {
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
            return addCoiHeaders(cachedResponse, isWorker);
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

        const url = new URL(r.url);
        if (url.origin !== self.location.origin) {
            event.respondWith(
                fetchWithCoi(r, isWorker).catch((e) => {
                    if (r.url && r.url !== '') {
                        console.warn("[COI] Fetch failed for:", r.url, e.message || e);
                    }
                    return new Response(null, {
                        status: 502,
                        statusText: "COI Fetch Error",
                        headers: new Headers({
                            "Cross-Origin-Embedder-Policy": "require-corp",
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
                        return addCoiHeaders(cachedResponse, isWorker);
                    }
                    return fetchWithCoi(r, isWorker)
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
                        return addCoiHeaders(cachedResponse, isWorker);
                    }
                    return fetchWithCoi(r, isWorker)
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
            event.respondWith(staleWhileRevalidate(cacheKey, r, isWorker));
            return;
        }

        event.respondWith(
            fetchWithCoi(r, isWorker)
                .then(async (networkResponse) => {
                    if (networkResponse && networkResponse.status === 200) {
                        await addToRuntimeCacheWithEviction(
                            cacheKey,
                            networkResponse.clone()
                        );
                    }
                    return networkResponse;
                })
                .catch(async () => addCoiHeaders(await caches.match(cacheKey), isWorker))
        );
    });
} else {
    // Page side. A document's isolation is fixed when it loads, so the only way
    // to gain SharedArrayBuffer is a navigation served by the controlling worker.
    // Reloads are counted per tab so an unsupported browser fails instead of looping.
    (() => {
        const RELOADS_KEY = "coiReloads";
        const MAX_RELOADS = 2;

        if (new URLSearchParams(window.location.search).has("reset-coi")) {
            window.sessionStorage.removeItem(RELOADS_KEY);
            const url = new URL(window.location);
            url.searchParams.delete("reset-coi");
            window.location.replace(url);
            return;
        }

        if (window.crossOriginIsolated && typeof SharedArrayBuffer !== "undefined") {
            window.sessionStorage.removeItem(RELOADS_KEY);
            return;
        }

        const sw = navigator.serviceWorker;
        const reloads = Number(window.sessionStorage.getItem(RELOADS_KEY) || 0);
        if (!sw || reloads >= MAX_RELOADS) {
            console.error(sw
                ? "[COI] Failed: page still not cross-origin isolated after " + reloads + " reloads"
                : "[COI] Failed: service workers are unavailable");
            window.sessionStorage.removeItem(RELOADS_KEY);
            window.YAZE_COI_FAILED = true;
            return;
        }

        const reload = () => {
            window.sessionStorage.setItem(RELOADS_KEY, String(reloads + 1));
            window.location.reload();
        };

        if (sw.controller) {
            console.log("[COI] Reloading so the service worker can add COOP/COEP headers");
            reload();
            return;
        }

        const overlay = document.createElement("div");
        overlay.id = "coi-setup-overlay";
        overlay.style.cssText = `
            position: fixed; top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(13,13,13,0.98); z-index: 999999;
            display: flex; align-items: center; justify-content: center;
            color: #0c6; font-family: monospace; font-size: 14px; text-align: center;
        `;
        overlay.innerHTML = `
            <div>
                <div>Initializing secure context...</div>
                <div style="font-size: 11px; margin-top: 12px; opacity: 0.7;">
                    This is required for SharedArrayBuffer support
                </div>
            </div>
        `;
        (document.body || document.documentElement).appendChild(overlay);

        console.log("[COI] Registering service worker...");
        sw.register(window.document.currentScript.src).then(() => sw.ready).then(reload, (err) => {
            console.error("[COI] Service worker registration failed:", err);
            overlay.style.color = "#f44";
            overlay.firstElementChild.textContent =
                "Failed to enable SharedArrayBuffer support: " + (err.message || err);
            window.YAZE_COI_FAILED = true;
        });
    })();
}
