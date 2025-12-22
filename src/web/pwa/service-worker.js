/**
 * Service Worker for YAZE Web
 * Provides offline support and caching for the web application
 */

const CACHE_NAME = 'yaze-cache-v4';
const RUNTIME_CACHE = 'yaze-runtime-v4';
const CACHE_METADATA = 'yaze-metadata-v1';

// Maximum number of entries in the runtime cache before LRU eviction
const MAX_RUNTIME_CACHE_SIZE = 50;

// TTL for runtime cache entries (24 hours in milliseconds)
const CACHE_TTL_MS = 24 * 60 * 60 * 1000;

// Assets that benefit from stale-while-revalidate (non-critical, frequently updated)
const STALE_WHILE_REVALIDATE_PATTERNS = [
  /\.css$/,
  /\/styles\//,
  /\/icons\//,
  /\/assets\//
];

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
  // PWA files (coi-serviceworker.js is at root for proper scope)
  './coi-serviceworker.js',
  './pwa/offline.html',
  // Core infrastructure (namespace.js must load first)
  './core/namespace.js',
  './core/config.js',
  './core/filesystem_manager.js',
  './core/loading_indicator.js',
  './core/error_handler.js',
  './core/crash_reporter.js',
  './core/wasm_recovery.js',
  './core/debug.js',
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
              // Delete old cache versions (keep current versions of all caches)
              return cacheName.startsWith('yaze-') &&
                     cacheName !== CACHE_NAME &&
                     cacheName !== RUNTIME_CACHE &&
                     cacheName !== CACHE_METADATA;
            })
            .map((cacheName) => {
              console.log('[ServiceWorker] Deleting old cache:', cacheName);
              return caches.delete(cacheName);
            })
        );
      })
      .then(() => {
        // Run initial TTL eviction on activation
        return evictExpiredEntries();
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
              // Cache the new response for future use (with LRU eviction)
              if (networkResponse && networkResponse.status === 200) {
                const responseToCache = networkResponse.clone();
                addToRuntimeCacheWithEviction(request, responseToCache);
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

  // For CSS, icons, and assets - use stale-while-revalidate for faster UI response
  if (shouldUseStaleWhileRevalidate(request.url)) {
    event.respondWith(staleWhileRevalidate(request));
    return;
  }

  // For all other requests, use network-first strategy
  event.respondWith(
    fetch(request)
      .then((networkResponse) => {
        // Cache successful responses (with LRU eviction)
        if (networkResponse && networkResponse.status === 200) {
          const responseToCache = networkResponse.clone();
          addToRuntimeCacheWithEviction(request, responseToCache);
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

/**
 * Trim the runtime cache to enforce LRU eviction policy
 * Removes oldest entries when cache exceeds MAX_RUNTIME_CACHE_SIZE
 */
async function trimRuntimeCache() {
  const cache = await caches.open(RUNTIME_CACHE);
  const keys = await cache.keys();

  if (keys.length > MAX_RUNTIME_CACHE_SIZE) {
    const entriesToDelete = keys.length - MAX_RUNTIME_CACHE_SIZE;
    console.log(`[ServiceWorker] Trimming runtime cache: removing ${entriesToDelete} oldest entries`);

    // Delete oldest entries (first in the list are oldest due to insertion order)
    for (let i = 0; i < entriesToDelete; i++) {
      await cache.delete(keys[i]);
    }
  }
}

/**
 * Add to runtime cache with LRU eviction and TTL tracking
 * @param {Request} request - The request to cache
 * @param {Response} response - The response to cache
 */
async function addToRuntimeCacheWithEviction(request, response) {
  const cache = await caches.open(RUNTIME_CACHE);
  await cache.put(request, response);

  // Store timestamp for TTL tracking
  await setCacheTimestamp(request.url, Date.now());

  await trimRuntimeCache();
  await evictExpiredEntries();
}

/**
 * Store timestamp for a cached URL
 * @param {string} url - The URL being cached
 * @param {number} timestamp - The timestamp when cached
 */
async function setCacheTimestamp(url, timestamp) {
  const metadataCache = await caches.open(CACHE_METADATA);
  const metadata = new Response(JSON.stringify({ timestamp }));
  await metadataCache.put(url + '__meta', metadata);
}

/**
 * Get timestamp for a cached URL
 * @param {string} url - The URL to check
 * @returns {Promise<number|null>} Timestamp or null if not found
 */
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

/**
 * Check if a URL matches stale-while-revalidate patterns
 * @param {string} url - URL to check
 * @returns {boolean} True if should use stale-while-revalidate
 */
function shouldUseStaleWhileRevalidate(url) {
  return STALE_WHILE_REVALIDATE_PATTERNS.some(pattern => pattern.test(url));
}

/**
 * Evict cache entries older than TTL
 */
async function evictExpiredEntries() {
  const cache = await caches.open(RUNTIME_CACHE);
  const metadataCache = await caches.open(CACHE_METADATA);
  const keys = await cache.keys();
  const now = Date.now();
  let evictedCount = 0;

  for (const request of keys) {
    const timestamp = await getCacheTimestamp(request.url);
    if (timestamp && (now - timestamp) > CACHE_TTL_MS) {
      await cache.delete(request);
      await metadataCache.delete(request.url + '__meta');
      evictedCount++;
    }
  }

  if (evictedCount > 0) {
    console.log(`[ServiceWorker] TTL eviction: removed ${evictedCount} expired entries`);
  }
}

/**
 * Stale-while-revalidate: Return cached response immediately, update in background
 * @param {Request} request - The request to handle
 * @returns {Promise<Response>} The response
 */
async function staleWhileRevalidate(request) {
  const cache = await caches.open(RUNTIME_CACHE);
  const cachedResponse = await cache.match(request);

  // Always fetch in background to update cache
  const fetchPromise = fetch(request).then(async (networkResponse) => {
    if (networkResponse && networkResponse.status === 200) {
      const responseToCache = networkResponse.clone();
      await addToRuntimeCacheWithEviction(request, responseToCache);
    }
    return networkResponse;
  }).catch((error) => {
    console.warn('[ServiceWorker] Background fetch failed:', error);
    return null;
  });

  // Return cached response immediately if available, otherwise wait for network
  if (cachedResponse) {
    // Fire and forget the background update
    fetchPromise;
    return cachedResponse;
  }

  return fetchPromise;
}