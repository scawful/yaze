# Web App Guide (Preview)

YAZE is available as a **preview** web application that runs in your browser. The web port is under active development - expect bugs and incomplete features.

> **⚠️ Preview Status**: The web app is experimental. For production ROM hacking, use the [desktop build](../build/quick-reference.md).

## Quick Start

### Try it Now

Visit your deployed instance to access the web version. The application is served via GitHub Pages or custom deployment.

### Loading a ROM

1. Click **"Open ROM"** or drag and drop your Zelda 3 ROM file onto the page
2. The ROM is stored locally in your browser using IndexedDB - it never leaves your computer
3. Start editing using a subset of the desktop interface
4. Press **`** (backtick) to open the terminal and run `help` or `/help`

### Saving Your Work

- **Auto-save**: Changes are automatically saved to browser storage (when working)
- **Download ROM**: Click the download button to save your modified ROM to disk
- **Backup recommended**: Download frequently; browser storage can be cleared

## Feature Status

The web version is in preview with varying editor support:

| Feature | Status | Notes |
|---------|--------|-------|
| ROM Loading | ✅ Working | Drag & drop, file picker |
| Overworld Editor | ⚡ Preview | Basic tile editing, incomplete features |
| Dungeon Editor | ⚡ Preview | Room viewing, editing incomplete |
| Palette Editor | ⚡ Preview | Basic palette viewing/editing |
| Graphics Editor | ⚡ Preview | Tile viewing, editing incomplete |
| Sprite Editor | ⚡ Preview | Limited functionality |
| Message Editor | ⚡ Preview | Text viewing, editing incomplete |
| Hex Editor | ✅ Working | Direct ROM editing |
| Asar Patching | ⚡ Preview | Basic assembly patching |
| Emulator | ❌ Not Available | Use desktop build |
| Real-time Collaboration | ⚡ Experimental | Requires server setup |
| Audio Playback | ⚡ Experimental | Limited SPC700 support |

**Legend**: ✅ Working | ⚡ Preview/Incomplete | ❌ Not Available

## AI Features (Preview)

The web app can surface AI chat/agent tools when connected to a collaboration
server with AI enabled.

- Set `ENABLE_AI_AGENT=true` and either `GEMINI_API_KEY` or
  `AI_AGENT_ENDPOINT` (for OpenAI/Anthropic/external agents) on the
  collaboration server.
- If AI is not configured, the UI remains usable but AI responses stay disabled.

See [`docs/public/deployment/collaboration-server-setup.md`](../deployment/collaboration-server-setup.md)
for server configuration details.

## Browser Requirements

### Recommended Browsers

- **Chrome/Edge** 90+ (best performance)
- **Firefox** 88+
- **Safari** 15.4+ (macOS/iOS)

### Required Features

The web app requires modern browser features:
- **WebAssembly**: Core application runtime
- **SharedArrayBuffer**: Multi-threading support (requires HTTPS or localhost)
- **IndexedDB**: Local ROM and project storage
- **File System Access API**: Better file handling (Chrome/Edge)

### Mobile Support

✅ **Tablets**: Full support on iPad and Android tablets
⚠️ **Phones**: Limited - interface designed for larger screens

## Performance Tips

### For Best Performance

1. **Use Chrome or Edge** - Fastest WebAssembly implementation
2. **Close other tabs** - Free up browser memory
3. **Desktop/laptop recommended** - Better than mobile for complex editing
4. **Stable internet** - Only needed for initial load; works offline after

### Troubleshooting Slow Performance

- Clear browser cache and reload
- Disable browser extensions temporarily
- Check browser console for errors (F12)
- Use the debug build for better error messages (see Development section)

## Storage & Privacy

### What's Stored Locally

- ROM files (IndexedDB)
- Project settings and preferences
- Auto-save data
- Recent file history

### Privacy Guarantee

- **All data stays in your browser** - ROMs and edits never uploaded to servers
- **No tracking** - No analytics or user data collection
- **No login required** - No accounts or personal information needed

### Clearing Data

```javascript
// Open browser console (F12) and run:
localStorage.clear();
sessionStorage.clear();
// Then reload the page
```

Or use your browser's "Clear browsing data" feature.

## Keyboard Shortcuts

The web app supports desktop keyboard shortcuts:

| Shortcut | Action |
|----------|--------|
| **Ctrl/Cmd + O** | Open ROM |
| **Ctrl/Cmd + S** | Save/Download ROM |
| **Ctrl/Cmd + Z** | Undo |
| **Ctrl/Cmd + Y** | Redo |
| **`** (backtick) | Toggle terminal |
| **Esc** | Close panels/dialogs |
| **Tab** | Cycle through editors |

**Note**: Some shortcuts may conflict with browser defaults. Use the menu bar as an alternative.

## Advanced Features

### Terminal Access

Press **`** (backtick) to open the integrated terminal. The terminal uses the
currently loaded ROM, so you can omit `--rom` for most commands:

```bash
# Show terminal help
/help

# ROM summary
z3ed rom-info

# Ask AI about ROM data (when AI is configured)
z3ed ai query "What sprites are in room 1?"

# List sprites in a dungeon room
z3ed dungeon-list-sprites --room=1

# Describe an overworld map
z3ed overworld-describe-map --map=0
```

### Collaboration (Experimental)

When enabled, multiple users can edit the same ROM simultaneously:

1. Click **"Start Collaboration"** in the toolbar
2. Share the session URL with collaborators
3. See real-time cursors and changes from other users

**Note**: Requires a collaboration server to be deployed. See deployment docs for setup.

### Developer Tools

Open browser console (F12) for debugging:

```javascript
// Check if WASM module is ready
window.yazeDebug.isReady()

// Get ROM status
window.yazeDebug.rom.getStatus()

// Get current editor state
window.yaze.editor.getActiveEditor()

// Read ROM data
window.yaze.data.getRoomTiles(roomId)
```

See [`docs/internal/wasm-yazeDebug-api-reference.md`](../../internal/wasm-yazeDebug-api-reference.md) for full API documentation.

### Debug API Test Suite

For quick smoke validation of the web build (control/gui/AI APIs), load the
debug test script and run it in the browser console:

```javascript
// Paste the contents of src/web/tests/wasm_debug_api_tests.js into the console,
// then run:
await window.runWasmDebugApiTests();
```

## Known Limitations

### Not Yet Supported

- **Emulator**: No built-in emulator in web version (use desktop)
- **Custom plugins**: Plugin system requires desktop build
- **Large file export**: Browser limits on file size (typically 2GB+)

### Browser-Specific Issues

- **Safari**: Slower performance, some SharedArrayBuffer limitations
- **Firefox**: Clipboard access may require permissions
- **Mobile Chrome**: Touch controls under development

## Offline Usage

After first load, the web app works offline:

1. Visit the site once while online
2. Service worker caches the application
3. Disconnect from internet
4. App continues to function normally

**Note**: Initial load requires internet; updates require reconnection.

## Building from Source

To build and deploy your own instance:

```bash
# Build the web app
./scripts/build-wasm.sh

# Serve locally for testing
./scripts/serve-wasm.sh

# Deploy dist/ folder to your web server
# Artifacts are in build-wasm/dist/
```

See [`docs/internal/agents/wasm-development-guide.md`](../../internal/agents/wasm-development-guide.md) for detailed build instructions.

## Deployment

### GitHub Pages (Automated)

Pushes to `master` automatically build and deploy via GitHub Actions:

```yaml
# See .github/workflows/web-build.yml
```

### Custom Server

Requirements:
- Static file server (nginx, Apache, etc.)
- HTTPS enabled (required for SharedArrayBuffer)
- Proper CORS headers

Minimal nginx config:

```nginx
server {
    listen 443 ssl http2;
    server_name yaze.yourdomain.com;
    
    root /var/www/yaze;
    
    # Required for SharedArrayBuffer
    add_header Cross-Origin-Opener-Policy same-origin;
    add_header Cross-Origin-Embedder-Policy require-corp;
    
    # Cache WASM files
    location ~* \.(wasm|js)$ {
        expires 1y;
        add_header Cache-Control "public, immutable";
    }
}
```

## Getting Help

- **Discord**: [Oracle of Secrets](https://discord.gg/MBFkMTPEmk)
- **Issues**: [GitHub Issues](https://github.com/scawful/yaze/issues)
- **Docs**: [`docs/public/index.md`](../index.md)

## Comparison: Web vs Desktop

| Feature | Web (Preview) | Desktop (Stable) |
|---------|---------------|------------------|
| **Installation** | None required | Download & install |
| **Platforms** | Modern browsers | Windows, macOS, Linux |
| **Performance** | Moderate | Excellent |
| **Editor Completeness** | Preview/Incomplete | Full-featured |
| **Emulator** | ❌ | ✅ |
| **Plugins** | ❌ | ✅ |
| **Stability** | Experimental | Production-ready |
| **Updates** | Automatic | Manual download |
| **Offline** | After first load | Always |
| **Collaboration** | Experimental | Via server |
| **Mobile** | Tablets (limited) | No |

**Recommendation**: 
- **For serious ROM hacking**: Use the desktop build
- **For quick previews or demos**: Web app is suitable
- **For learning/exploration**: Either works, but desktop is more complete
