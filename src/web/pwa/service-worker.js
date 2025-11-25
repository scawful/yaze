/**
 * Service Worker for YAZE Web
 * Provides offline support and caching for the web application
 */

const CACHE_NAME = 'yaze-cache-v3';
const RUNTIME_CACHE = 'yaze-runtime-v3';

// List of assets to pre-cache during installation
// Using relative paths for GitHub Pages subdirectory support
// NOTE: yaze.data (~22MB) is NOT precached - it's cached on-demand during fetch
// to avoid installation timeouts and failed service worker activations
const PRECACHE_ASSETS = [
  './',
  './index.html',
  './yaze.js',
  './yaze.wasm',
  // './yaze.data - cached on-demand, too large for precache
  './app.js',
  // PWA files
  './pwa/coi-serviceworker.js',
  './pwa/offline.html',
  // Core infrastructure
  './core/config.js',
  './core/filesystem_manager.js',
  './core/loading_indicator.js',
  './core/error_handler.js',
  // UI Components
  './components/shortcuts_overlay.js',
  './components/terminal.js',
  './components/collab_console.js',
  './components/touch_gestures.js',
  './components/drop_zone.js',
  // Styles
  './styles/main.css',
  './styles/loading_indicator.css',
  './styles/error_handler.css',
  './styles/shortcuts_overlay.css',
  './styles/terminal.css',
  './styles/collab_console.css',
  './styles/touch_gestures.css',
  './styles/drop_zone.css'
];

// Install event - pre-cache all static assets
self.addEventListener('install', (event) => {
  console.log('[ServiceWorker] Install');

  event.waitUntil(
    caches.open(CACHE_NAME)
      .then((cache) => {
        console.log('[ServiceWorker] Pre-caching assets');
        // Try to cache each asset, but don't fail if some are missing
        return Promise.allSettled(
          PRECACHE_ASSETS.map((url) => {
            return cache.add(url).catch((error) => {
              console.warn(`[ServiceWorker] Failed to cache ${url}:`, error);
            });
          })
        );
      })
      .then(() => {
        console.log('[ServiceWorker] Pre-caching complete');
        // Don't call skipWaiting() here - let the user decide when to update
        // This prevents conflicts with the COI service worker and avoids
        // unexpected reloads. skipWaiting() is called via message event instead.
      })
  );
});

// Activate event - clean up old caches
self.addEventListener('activate', (event) => {
  console.log('[ServiceWorker] Activate');

  event.waitUntil(
    caches.keys()
      .then((cacheNames) => {
        return Promise.all(
          cacheNames
            .filter((cacheName) => {
              // Delete old cache versions
              return cacheName.startsWith('yaze-') &&
                     cacheName !== CACHE_NAME &&
                     cacheName !== RUNTIME_CACHE;
            })
            .map((cacheName) => {
              console.log('[ServiceWorker] Deleting old cache:', cacheName);
              return caches.delete(cacheName);
            })
        );
      })
      .then(() => {
        console.log('[ServiceWorker] Claiming clients');
        // Take control of all clients immediately
        return self.clients.claim();
      })
  );
});

// Fetch event - cache-first strategy with network fallback
self.addEventListener('fetch', (event) => {
  const { request } = event;
  const url = new URL(request.url);

  // Skip cross-origin requests
  if (url.origin !== location.origin) {
    return;
  }

  // Special handling for navigation requests (HTML pages)
  if (request.mode === 'navigate') {
    event.respondWith(
      caches.match(request)
        .then((cachedResponse) => {
          if (cachedResponse) {
            return cachedResponse;
          }

          // Try to fetch from network
          return fetch(request)
            .then((networkResponse) => {
              // Cache the new response for future use
              if (networkResponse && networkResponse.status === 200) {
                const responseToCache = networkResponse.clone();
                caches.open(RUNTIME_CACHE)
                  .then((cache) => {
                    cache.put(request, responseToCache);
                  });
              }
              return networkResponse;
            })
            .catch(() => {
              // If both cache and network fail, show offline page if available
              return caches.match('./offline.html');
            });
        })
    );
    return;
  }

  // For WASM and data files, use cache-first strategy
  if (request.url.endsWith('.wasm') ||
      request.url.endsWith('.data') ||
      request.url.endsWith('.js')) {
    event.respondWith(
      caches.match(request)
        .then((cachedResponse) => {
          if (cachedResponse) {
            // Return cached version
            return cachedResponse;
          }

          // Not in cache, fetch from network
          return fetch(request)
            .then((networkResponse) => {
              // Cache successful responses
              if (networkResponse && networkResponse.status === 200) {
                const responseToCache = networkResponse.clone();
                caches.open(CACHE_NAME)
                  .then((cache) => {
                    cache.put(request, responseToCache);
                  });
              }
              return networkResponse;
            });
        })
    );
    return;
  }

  // For all other requests, use network-first strategy
  event.respondWith(
    fetch(request)
      .then((networkResponse) => {
        // Cache successful responses
        if (networkResponse && networkResponse.status === 200) {
          const responseToCache = networkResponse.clone();
          caches.open(RUNTIME_CACHE)
            .then((cache) => {
              cache.put(request, responseToCache);
            });
        }
        return networkResponse;
      })
      .catch(() => {
        // If network fails, try cache
        return caches.match(request);
      })
  );
});

// Message event - handle cache updates
self.addEventListener('message', (event) => {
  if (event.data && event.data.type === 'SKIP_WAITING') {
    console.log('[ServiceWorker] Skip waiting requested');
    self.skipWaiting();
  }

  if (event.data && event.data.type === 'CACHE_VERSION') {
    event.ports[0].postMessage({ version: CACHE_NAME });
  }
});