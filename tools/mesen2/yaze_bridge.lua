-- yaze_bridge.lua
-- Bridge between Mesen 2 and Yaze (Zelda 3 Editor)
--
-- This script connects to Yaze's API to fetch symbol tables and sync state.
--
-- Usage:
-- 1. Run Yaze in server mode: ./yaze --server --rom_file zelda3.sfc
-- 2. Load this script in Mesen 2 (Debug > Script Window)
--

local socket = require("socket")
local json = require("json") -- Mesen includes a JSON library

local YazeBridge = {}
YazeBridge.host = "127.0.0.1"
YazeBridge.port = 8080
YazeBridge.last_sync_time = 0
YazeBridge.sync_interval = 60 * 60 -- Sync every 60s (in frames)

function YazeBridge:log(msg)
  emu.log("YazeBridge: " .. msg)
end

function YazeBridge:http_get(path)
  local client = socket.tcp()
  client:settimeout(0.5)
  local res = client:connect(self.host, self.port)
  if not res then
    self:log("Connection failed")
    return nil
  end

  client:send("GET " .. path .. " HTTP/1.0\r\nHost: " .. self.host .. "\r\n\r\n")
  
  local response = ""
  while true do
    local s, status, partial = client:receive()
    response = response .. (s or partial)
    if status == "closed" then break end
  end
  client:close()

  -- Simple body extraction (skip headers)
  local _, body_start = response:find("\r\n\r\n")
  if body_start then
    return response:sub(body_start + 1)
  end
  return nil
end

function YazeBridge:sync_symbols()
  self:log("Syncing symbols...")
  -- Fetch symbols in Mesen format (.mlb)
  local data = self:http_get("/api/v1/symbols?format=mesen")
  if data then
    local count = 0
    -- Mesen .mlb format is "PRG:address:name" or "address:name"
    for line in data:gmatch("[^\r\n]+") do
      local addr_str, name = line:match("^(?:PRG:)?([0-9A-Fa-f]+):(%S+)")
      if addr_str and name then
        local addr = tonumber(addr_str, 16)
        -- Mesen 2 uses emu.addLabel(address, name) or similar
        -- Based on Mesen 2 Lua API docs:
        emu.setLabel(addr, name)
        count = count + 1
      end
    end
    self:log("Applied " .. count .. " symbols from Yaze")
  end
end

function YazeBridge:update()
  if emu.frameCount() % self.sync_interval == 0 then
    self:sync_symbols()
  end
end

-- Register callbacks
emu.addEventCallback(function() YazeBridge:update() end, emu.eventType.endFrame)

YazeBridge:log("Loaded. Waiting for Yaze server on " .. YazeBridge.host .. ":" .. YazeBridge.port)

return YazeBridge
