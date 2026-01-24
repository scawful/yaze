-- yaze_bridge.lua
-- Bidirectional Bridge between Mesen 2 and Yaze (Zelda 3 Editor)
--
-- Features:
-- - Symbol sync from Yaze to Mesen2
-- - PC sync (Mesen2 breakpoint hit → Yaze disassembly navigation)
-- - Breakpoint sync (notify Yaze when breakpoints hit)
-- - Real-time state updates
--
-- Usage:
-- 1. Run Yaze in server mode: ./yaze --server --rom_file zelda3.sfc
-- 2. Load this script in Mesen 2 (Debug > Script Window)
--

local socket = require("socket")
local json = require("json")

-- ============================================================================
-- Configuration
-- ============================================================================

local YazeBridge = {
  host = "127.0.0.1",
  port = 8080,
  
  -- Sync intervals (in frames, 60fps)
  symbol_sync_interval = 60 * 60,  -- Every 60 seconds
  state_sync_interval = 30,        -- Every 0.5 seconds
  
  -- State tracking
  last_symbol_sync = 0,
  last_state_sync = 0,
  last_pc = 0,
  connected = false,
  
  -- Feature toggles
  enable_pc_sync = true,
  enable_breakpoint_notifications = true,
  enable_state_sync = true,
}

-- ============================================================================
-- Logging
-- ============================================================================

function YazeBridge:log(msg)
  emu.log("YazeBridge: " .. msg)
end

function YazeBridge:debug(msg)
  -- Uncomment for verbose logging:
  -- emu.log("YazeBridge [DEBUG]: " .. msg)
end

-- ============================================================================
-- HTTP Communication
-- ============================================================================

function YazeBridge:http_request(method, path, body)
  local client = socket.tcp()
  client:settimeout(0.3)
  
  local res = client:connect(self.host, self.port)
  if not res then
    self.connected = false
    return nil
  end
  self.connected = true
  
  local request = method .. " " .. path .. " HTTP/1.0\r\n"
  request = request .. "Host: " .. self.host .. "\r\n"
  request = request .. "Content-Type: application/json\r\n"
  
  if body then
    request = request .. "Content-Length: " .. #body .. "\r\n"
  end
  
  request = request .. "\r\n"
  
  if body then
    request = request .. body
  end
  
  client:send(request)
  
  local response = ""
  while true do
    local s, status, partial = client:receive()
    response = response .. (s or partial)
    if status == "closed" then break end
  end
  client:close()
  
  -- Extract body
  local _, body_start = response:find("\r\n\r\n")
  if body_start then
    return response:sub(body_start + 1)
  end
  return nil
end

function YazeBridge:http_get(path)
  return self:http_request("GET", path, nil)
end

function YazeBridge:http_post(path, data)
  return self:http_request("POST", path, json.encode(data))
end

-- ============================================================================
-- Symbol Sync (Yaze → Mesen2)
-- ============================================================================

function YazeBridge:sync_symbols()
  self:log("Syncing symbols from Yaze...")
  
  local data = self:http_get("/api/v1/symbols?format=mesen")
  if not data then
    self:log("Failed to fetch symbols")
    return
  end
  
  local count = 0
  -- Mesen .mlb format: "PRG:address:name" or "address:name"
  for line in data:gmatch("[^\r\n]+") do
    -- Try format with PRG prefix first
    local addr_str, name = line:match("^PRG:([0-9A-Fa-f]+):(.+)$")
    if not addr_str then
      -- Try format without PRG prefix
      addr_str, name = line:match("^([0-9A-Fa-f]+):(.+)$")
    end
    
    if addr_str and name then
      local addr = tonumber(addr_str, 16)
      if addr then
        emu.setLabel(addr, name)
        count = count + 1
      end
    end
  end
  
  self:log("Applied " .. count .. " symbols")
end

-- ============================================================================
-- PC Sync (Mesen2 → Yaze)
-- ============================================================================

function YazeBridge:notify_pc_change(pc)
  if not self.enable_pc_sync then return end
  
  self:debug("PC changed to: " .. string.format("0x%06X", pc))
  
  self:http_post("/api/v1/navigate", {
    address = pc,
    source = "mesen2"
  })
end

-- ============================================================================
-- Breakpoint Notifications (Mesen2 → Yaze)
-- ============================================================================

function YazeBridge:on_breakpoint_hit()
  if not self.enable_breakpoint_notifications then return end
  
  local state = emu.getState()
  local cpu = state.cpu
  
  local pc = cpu.k * 0x10000 + cpu.pc
  
  self:log("Breakpoint hit at: " .. string.format("0x%06X", pc))
  
  self:http_post("/api/v1/breakpoint/hit", {
    address = pc,
    cpu_state = {
      A = cpu.a,
      X = cpu.x,
      Y = cpu.y,
      SP = cpu.sp,
      D = cpu.d,
      DBR = cpu.db,
      P = cpu.ps,
      K = cpu.k,
      PC = cpu.pc
    },
    source = "mesen2"
  })
  
  -- Also sync PC
  self:notify_pc_change(pc)
end

-- ============================================================================
-- Game State Sync (Mesen2 → Yaze)
-- ============================================================================

function YazeBridge:sync_game_state()
  if not self.enable_state_sync then return end
  
  -- Read key ALTTP memory locations
  local link_x = emu.read16(0x7E0022, emu.memType.snesMemory)
  local link_y = emu.read16(0x7E0020, emu.memType.snesMemory)
  local health = emu.read(0x7E0F36, emu.memType.snesMemory)
  local game_mode = emu.read(0x7E0010, emu.memType.snesMemory)
  
  self:http_post("/api/v1/state/update", {
    link = {
      x = link_x,
      y = link_y
    },
    health = health,
    game_mode = game_mode,
    frame = emu.frameCount(),
    source = "mesen2"
  })
end

-- ============================================================================
-- Frame Update Loop
-- ============================================================================

function YazeBridge:on_frame_end()
  local frame = emu.frameCount()
  
  -- Symbol sync (infrequent)
  if frame - self.last_symbol_sync >= self.symbol_sync_interval then
    self:sync_symbols()
    self.last_symbol_sync = frame
  end
  
  -- State sync (frequent)
  if frame - self.last_state_sync >= self.state_sync_interval then
    self:sync_game_state()
    self.last_state_sync = frame
  end
end

-- ============================================================================
-- Step Event (PC change detection)
-- ============================================================================

function YazeBridge:on_step()
  local state = emu.getState()
  local pc = state.cpu.k * 0x10000 + state.cpu.pc
  
  if pc ~= self.last_pc then
    self.last_pc = pc
    -- Only notify on significant changes (during debugging)
    if emu.isExecutionStopped() then
      self:notify_pc_change(pc)
    end
  end
end

-- ============================================================================
-- Initialization
-- ============================================================================

function YazeBridge:init()
  self:log("Initializing...")
  
  -- Register callbacks
  emu.addEventCallback(function() self:on_frame_end() end, emu.eventType.endFrame)
  emu.addEventCallback(function() self:on_step() end, emu.eventType.codeBreak)
  
  -- Initial symbol sync
  self:sync_symbols()
  
  self:log("Ready! Yaze server: " .. self.host .. ":" .. self.port)
  self:log("Features: PC sync=" .. tostring(self.enable_pc_sync) ..
           ", Breakpoint notify=" .. tostring(self.enable_breakpoint_notifications) ..
           ", State sync=" .. tostring(self.enable_state_sync))
end

-- Start the bridge
YazeBridge:init()

return YazeBridge
