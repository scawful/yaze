#include "app/platform/wasm/wasm_bootstrap.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <fstream>
#include <algorithm>
#include <mutex>
#include <queue>
#include <vector>
#include "app/platform/wasm/wasm_config.h"
#include "app/platform/wasm/wasm_drop_handler.h"
#include "util/log.h"

namespace {

bool g_filesystem_ready = false;
std::function<void(std::string)> g_rom_load_handler;
std::queue<std::string> g_pending_rom_loads;
std::mutex g_rom_load_mutex;

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
void SetFileSystemReady() {
  g_filesystem_ready = true;
  LOG_INFO("Wasm", "Filesystem sync complete.");
  
  // Notify JS that FS is ready
  EM_ASM({
    if (Module.onFileSystemReady) {
      Module.onFileSystemReady();
    }
  });
}

EMSCRIPTEN_KEEPALIVE
void SyncFilesystem() {
  // Sync all IDBFS mounts to IndexedDB for persistence
  EM_ASM({
    if (typeof FS !== 'undefined' && FS.syncfs) {
      FS.syncfs(false, function(err) {
        if (err) {
          console.error('[WASM] Failed to sync filesystem:', err);
        } else {
          console.log('[WASM] Filesystem synced successfully');
        }
      });
    }
  });
}

EMSCRIPTEN_KEEPALIVE
void LoadRomFromWeb(const char* filename) {
  if (!filename) {
    LOG_ERROR("Wasm", "LoadRomFromWeb called with null filename");
    return;
  }

  std::string path(filename);

  // Validate path is not empty
  if (path.empty()) {
    LOG_ERROR("Wasm", "LoadRomFromWeb called with empty filename");
    return;
  }

  // Validate path doesn't contain path traversal
  if (path.find("..") != std::string::npos) {
    LOG_ERROR("Wasm", "LoadRomFromWeb: path traversal not allowed: %s", filename);
    return;
  }

  // Validate reasonable path length (max 512 chars)
  if (path.length() > 512) {
    LOG_ERROR("Wasm", "LoadRomFromWeb: path too long (%zu chars)", path.length());
    return;
  }

  yaze::app::wasm::TriggerRomLoad(path);
}

} // extern "C"

EM_JS(void, MountFilesystems, (), {
  if (typeof FS === 'undefined') {
    if (typeof Module !== 'undefined') {
      Module.YAZE_FS_MOUNT_ATTEMPTS = (Module.YAZE_FS_MOUNT_ATTEMPTS || 0) + 1;
      if (Module.YAZE_FS_MOUNT_ATTEMPTS < 50) {
        console.warn('[WASM] FS not ready, retrying mount (' + Module.YAZE_FS_MOUNT_ATTEMPTS + ')');
        setTimeout(MountFilesystems, 50);
        return;
      }
    }
    console.error('[WASM] FS unavailable, skipping filesystem mounts');
    if (typeof Module !== 'undefined' && Module._SetFileSystemReady) {
      Module._SetFileSystemReady();
    }
    return;
  }

  function isErrno(err, code) {
    if (!err) return false;
    if (err.code === code) return true;
    if (typeof err.errno === 'number' && FS.ERRNO_CODES && FS.ERRNO_CODES[code] === err.errno) {
      return true;
    }
    return false;
  }

  function pathExists(path) {
    try {
      FS.stat(path);
      return true;
    } catch (e) {
      return false;
    }
  }

  function ensureDir(dir) {
    try {
      var stat = FS.stat(dir);
      if (FS.isDir(stat.mode)) return true;

      var backup = dir + '.bak';
      if (pathExists(backup)) {
        var suffix = 1;
        while (suffix < 5 && pathExists(backup + suffix)) {
          suffix++;
        }
        backup = backup + suffix;
      }
      try {
        FS.rename(dir, backup);
        console.warn('[WASM] Renamed file at ' + dir + ' to ' + backup);
      } catch (renameErr) {
        console.warn('[WASM] Failed to rename file at ' + dir + ': ' + renameErr);
        return false;
      }
    } catch (e) {
      if (!isErrno(e, 'ENOENT')) {
        console.warn('[WASM] Failed to stat directory ' + dir + ': ' + e);
      }
    }

    try {
      FS.mkdir(dir);
      return true;
    } catch (e) {
      if (isErrno(e, 'EEXIST')) return true;
      console.warn('[WASM] Failed to create directory ' + dir + ': ' + e);
      return false;
    }
  }

  // Create all required directories
  var directories = [
    '/roms',       // ROM files (IDBFS - persistent for session restore)
    '/saves',      // Save files (IDBFS - persistent)
    '/config',     // Configuration files (IDBFS - persistent)
    '/projects',   // Project files (IDBFS - persistent)
    '/prompts',    // Agent prompts (IDBFS - persistent)
    '/recent',     // Recent files metadata (IDBFS - persistent)
    '/temp'        // Temporary files (MEMFS - non-persistent)
  ];

  directories.forEach(function(dir) {
    ensureDir(dir);
  });

  // Mount MEMFS for temporary files only
  try {
    FS.mount(MEMFS, {}, '/temp');
  } catch (e) {
    if (!isErrno(e, 'EBUSY') && !isErrno(e, 'EEXIST')) {
      console.warn('[WASM] Failed to mount MEMFS for /temp: ' + e);
    }
  }

  // Check if IDBFS is available (try multiple ways to access it)
  var idbfs = null;
  if (typeof IDBFS !== 'undefined') {
    idbfs = IDBFS;
  } else if (typeof Module !== 'undefined' && typeof Module.IDBFS !== 'undefined') {
    idbfs = Module.IDBFS;
  } else if (typeof FS !== 'undefined' && typeof FS.filesystems !== 'undefined' && FS.filesystems.IDBFS) {
    idbfs = FS.filesystems.IDBFS;
  }

  // Persistent directories to mount with IDBFS
  var persistentDirs = ['/roms', '/saves', '/config', '/projects', '/prompts', '/recent'];
  var mountedCount = 0;
  var totalToMount = persistentDirs.length;

  if (idbfs !== null) {
    persistentDirs.forEach(function(dir) {
      try {
        FS.mount(idbfs, {}, dir);
        mountedCount++;
      } catch (e) {
        console.error("Error mounting IDBFS for " + dir + ": " + e);
        // Fallback to MEMFS for this directory
        try {
          FS.mount(MEMFS, {}, dir);
        } catch (e2) {
          // May already be mounted
        }
        mountedCount++;
      }
    });

    // Sync all IDBFS mounts from IndexedDB to memory
    FS.syncfs(true, function(err) {
      if (err) {
        console.error("Failed to sync IDBFS: " + err);
      } else {
        console.log("IDBFS synced successfully");
      }
      // Signal C++ that we are ready regardless of success/fail
      Module._SetFileSystemReady();
    });
  } else {
    // Fallback to MEMFS if IDBFS is not available
    console.warn("IDBFS not available, using MEMFS for all directories (no persistence)");
    persistentDirs.forEach(function(dir) {
      try {
        FS.mount(MEMFS, {}, dir);
      } catch (e) {
        // May already be mounted
      }
    });
    Module._SetFileSystemReady();
  }
});

EM_JS(void, SetupYazeGlobalApi, (), {
  if (typeof Module === 'undefined') return;
  
  // Initialize global API for agents/automation
  window.yazeApp = {
    execute: function(cmd) {
      if (Module.executeCommand) {
        return Module.executeCommand(cmd);
      }
      return "Error: bindings not ready";
    },
    
    getState: function() {
      if (Module.getFullDebugState) {
        try { return JSON.parse(Module.getFullDebugState()); } catch(e) { return {}; }
      }
      return {};
    },
    
    getEditorState: function() {
        if (Module.getEditorState) {
            try { return JSON.parse(Module.getEditorState()); } catch(e) { return {}; }
        }
        return {};
    },
    
    loadRom: function(filename) {
       return this.execute("rom load " + filename);
    }
  };
  
  console.log("[yaze] window.yazeApp API initialized for agents");
});

namespace yaze::app::wasm {

bool IsFileSystemReady() {
  return g_filesystem_ready;
}

void SetRomLoadHandler(std::function<void(std::string)> handler) {
  std::lock_guard<std::mutex> lock(g_rom_load_mutex);
  g_rom_load_handler = handler;

  // Flush all pending ROM loads
  while (!g_pending_rom_loads.empty()) {
    std::string path = g_pending_rom_loads.front();
    g_pending_rom_loads.pop();
    LOG_INFO("Wasm", "Flushing pending ROM load: %s", path.c_str());
    handler(path);
  }
}

void TriggerRomLoad(const std::string& path) {
  std::lock_guard<std::mutex> lock(g_rom_load_mutex);
  if (g_rom_load_handler) {
    g_rom_load_handler(path);
  } else {
    LOG_INFO("Wasm", "Queuing ROM load (handler not ready): %s", path.c_str());
    g_pending_rom_loads.push(path);
  }
}

void InitializeWasmPlatform() {
  // Load WASM configuration from JavaScript
  app::platform::WasmConfig::Get().LoadFromJavaScript();
  
  // Setup global API
  SetupYazeGlobalApi();

  // Initialize drop handler for Drag & Drop support
  auto& drop_handler = yaze::platform::WasmDropHandler::GetInstance();
  drop_handler.Initialize("", 
    [](const std::string& filename, const std::vector<uint8_t>& data) {
        // Determine file type from extension
        std::string ext = filename.substr(filename.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == "sfc" || ext == "smc" || ext == "zip") {
            // Write to MEMFS and load
            std::string path = "/roms/" + filename;
            std::ofstream file(path, std::ios::binary);
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            file.close();
            
            LOG_INFO("Wasm", "Wrote dropped ROM to %s (%zu bytes)", path.c_str(), data.size());
            LoadRomFromWeb(path.c_str());
        } 
        else if (ext == "pal" || ext == "tpl") {
            LOG_INFO("Wasm", "Palette drop detected: %s. Feature pending UI integration.", filename.c_str());
        }
    },
    [](const std::string& error) {
        LOG_ERROR("Wasm", "Drop Handler Error: %s", error.c_str());
    }
  );

  // Initialize filesystems asynchronously
  MountFilesystems();
}

} // namespace yaze::app::wasm

#endif // __EMSCRIPTEN__
