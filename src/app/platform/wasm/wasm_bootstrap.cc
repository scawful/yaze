#include "app/platform/wasm/wasm_bootstrap.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <fstream>
#include <algorithm>
#include <vector>
#include "app/platform/wasm/wasm_config.h"
#include "app/platform/wasm/wasm_drop_handler.h"
#include "util/log.h"

namespace {

bool g_filesystem_ready = false;
std::function<void(std::string)> g_rom_load_handler;
std::string g_pending_rom_load;

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
void LoadRomFromWeb(const char* filename) {
  if (!filename) {
    LOG_ERROR("Wasm", "LoadRomFromWeb called with null filename");
    return;
  }
  
  yaze::app::wasm::TriggerRomLoad(filename);
}

} // extern "C"

EM_JS(void, MountFilesystems, (), {
  // Create directories
  FS.mkdir('/roms');
  FS.mkdir('/saves');

  // Mount filesystems
  FS.mount(MEMFS, {}, '/roms');
  
  // Check if IDBFS is available (try multiple ways to access it)
  var idbfs = null;
  if (typeof IDBFS !== 'undefined') {
    idbfs = IDBFS;
  } else if (typeof Module !== 'undefined' && typeof Module.IDBFS !== 'undefined') {
    idbfs = Module.IDBFS;
  } else if (typeof FS !== 'undefined' && typeof FS.filesystems !== 'undefined' && FS.filesystems.IDBFS) {
    idbfs = FS.filesystems.IDBFS;
  }
  
  if (idbfs !== null) {
    try {
      FS.mount(idbfs, {}, '/saves');
      
      // Sync from IDBFS to memory
      FS.syncfs(true, function(err) {
        if (err) {
          console.error("Failed to sync IDBFS: " + err);
        } else {
          console.log("IDBFS synced successfully");
        }
        // Signal C++ that we are ready regardless of success/fail
        Module._SetFileSystemReady();
      });
    } catch (e) {
      console.error("Error mounting IDBFS: " + e);
      // Fallback to MEMFS
      FS.mount(MEMFS, {}, '/saves');
      Module._SetFileSystemReady();
    }
  } else {
    // Fallback to MEMFS if IDBFS is not available
    console.warn("IDBFS not available, using MEMFS for /saves (no persistence)");
    FS.mount(MEMFS, {}, '/saves');
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
  g_rom_load_handler = handler;
  
  // Flush pending load if any
  if (!g_pending_rom_load.empty()) {
    LOG_INFO("Wasm", "Flushing pending ROM load: %s", g_pending_rom_load.c_str());
    handler(g_pending_rom_load);
    g_pending_rom_load.clear();
  }
}

void TriggerRomLoad(const std::string& path) {
  if (g_rom_load_handler) {
    g_rom_load_handler(path);
  } else {
    LOG_INFO("Wasm", "Queuing ROM load (handler not ready): %s", path.c_str());
    g_pending_rom_load = path;
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