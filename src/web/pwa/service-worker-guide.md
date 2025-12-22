# Service Worker Implementation Guide for YAZE Web

## Overview
This implementation provides offline support for the YAZE web application through Service Workers, enabling the app to work without an internet connection after initial caching.

## Files Created

### 1. `service-worker.js`
- **Purpose**: Main service worker that handles caching and offline functionality
- **Cache Strategy**: Cache-first for static assets, network-first for dynamic content
- **Version**: Uses `yaze-cache-v1` for easy cache invalidation on updates
- **Features**:
  - Pre-caches essential assets during installation
  - Handles fetch events with intelligent fallback strategies
  - Cleans up old cache versions automatically
  - Supports cache updates via messages

### 2. `manifest.json`
- **Purpose**: PWA manifest file that enables app installation
- **Features**:
  - Defines app metadata (name, description, icons)
  - Sets display mode to `standalone` for app-like experience
  - Specifies theme colors matching YAZE's design
  - Includes placeholder paths for icons and screenshots

### 3. `offline.html`
- **Purpose**: Fallback page shown when offline and resources aren't cached
- **Features**:
  - User-friendly offline message
  - Retry button to reload when connection returns
  - Auto-reload when connection is restored
  - Tips for enabling offline mode

### 4. Updated `shell.html`
- **Added Features**:
  - PWA meta tags for mobile compatibility
  - Service worker registration script
  - Online/offline status indicator
  - Update notification system
  - Automatic reload on service worker updates

## Cached Resources

The service worker pre-caches the following resources:
- `/` (root)
- `/index.html`
- `/shell.html`
- `/yaze.js`
- `/yaze.wasm`
- `/yaze.data` (if present)
- `/loading_indicator.css`
- `/loading_indicator.js`
- `/error_handler.css`
- `/error_handler.js`

## How It Works

### Installation Flow
1. User visits the web app
2. Browser registers the service worker
3. Service worker installs and pre-caches essential assets
4. App becomes available offline after successful caching

### Update Flow
1. Service worker checks for updates periodically (every hour)
2. When updates are found, new service worker installs in background
3. User sees update notification
4. User can choose to update immediately or dismiss
5. On update, page reloads with new version

### Offline Behavior
1. When offline, cached resources are served from local cache
2. Offline indicator appears in the UI
3. If a resource isn't cached, offline.html is shown as fallback
4. App automatically reconnects when network returns

## Build Integration

To include these files in your WASM build:

1. Copy web assets during build:
```bash
# In your build script or CMakeLists.txt
cp src/web/service-worker.js <build_output>/
cp src/web/manifest.json <build_output>/
cp src/web/offline.html <build_output>/
```

2. Ensure your web server serves files with correct MIME types:
- `service-worker.js`: `application/javascript`
- `manifest.json`: `application/manifest+json`
- `.wasm` files: `application/wasm`

## Testing Offline Functionality

1. **Chrome DevTools**:
   - Open DevTools > Application > Service Workers
   - Check "Offline" to simulate offline mode
   - View cached resources in Application > Cache Storage

2. **Manual Testing**:
   - Load the app while online
   - Disconnect from network
   - Verify app still works with cached resources
   - Reconnect and verify update flow

## Deployment Considerations

### HTTPS Required
Service Workers only work on HTTPS (or localhost). Ensure your deployment uses HTTPS.

### Cache Versioning
Update `CACHE_NAME` in service-worker.js when deploying new versions:
```javascript
const CACHE_NAME = 'yaze-cache-v2'; // Increment version
```

### Icon Assets
Create icon files in `/icons/` directory:
- icon-72x72.png
- icon-96x96.png
- icon-128x128.png
- icon-144x144.png
- icon-152x152.png
- icon-192x192.png (required for iOS)
- icon-384x384.png
- icon-512x512.png

### Path Adjustments
If deploying to a subdirectory, update paths in:
- `manifest.json`: Update `start_url` and `scope`
- `service-worker.js`: Update cache paths
- `shell.html`: Update service worker registration path

## Browser Support

- **Chrome/Edge**: Full support (v40+)
- **Firefox**: Full support (v44+)
- **Safari**: Partial support (v11.3+, limited PWA features)
- **Mobile browsers**: Good support on Android Chrome, limited on iOS Safari

## Troubleshooting

### Service Worker Not Registering
- Verify HTTPS is enabled (or using localhost)
- Check browser console for errors
- Ensure paths are correct in registration script

### Cache Not Working
- Clear browser cache and re-register service worker
- Check Application > Cache Storage in DevTools
- Verify all file paths in PRECACHE_ASSETS exist

### Update Notifications Not Showing
- Ensure service worker version changed
- Check if browser blocks notifications
- Verify update detection logic in shell.html

## Future Enhancements

1. **Selective Caching**: Cache ROM files in IndexedDB for larger storage
2. **Background Sync**: Queue changes when offline, sync when online
3. **Push Notifications**: Notify users of new features or updates
4. **Cache Strategies**: Implement more sophisticated caching based on resource type
5. **Workbox Integration**: Consider using Google's Workbox for advanced features