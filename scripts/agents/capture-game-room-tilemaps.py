#!/usr/bin/env python3
"""Capture each dungeon room's tilemaps from the real game in Mesen2-OOS.

The game builds a room's full 64x64 tilemaps in WRAM when it loads the room:
TILEMAPA ($7E2000) holds BG1 and TILEMAPB ($7E4000) holds BG2 (usdasm
wram.asm; the BG assignment was confirmed by comparing room 0x001 with yaze).
Reading those buffers gives ground truth for every placed object's tiles,
flips, palette bits and priority, with no camera, sprite or HUD in the way.

Room loading follows test/fixtures/visual/dungeon/README.md: a Pro Action
Replay code makes EntranceData ($02C813 + 2 * entrance) return the target
room, then module 0x05 loads it. The ROM file is never modified.

The entrance decides the dungeon's main graphics set (EntranceData.main_GFX),
floor and dungeon ID, so --entrance auto (the default) loads each room
through an entrance of its own dungeon: one that leads straight into the room
if there is one, else one whose dungeon's pause-menu map lists the room, else
the choice made for a neighbouring room (16-wide room grid). The manifest
records the entrance, how it was chosen, and its main_GFX per room.
--entrance 0x34 reproduces the earlier fixed-entrance captures.

Output directory:
  room_XXX.tilemap  16384 bytes: TILEMAPA (BG1) then TILEMAPB (BG2),
                    little-endian 16-bit tile words, row-major 64x64 each.
  manifest.json     ROM SHA-1, entrance, and a status for every room.
With --full, also the whole machine state each room loaded into:
  room_XXX.wram     131072 bytes, $7E0000-$7FFFFF
  room_XXX.vram     65536 bytes
  room_XXX.cgram    512 bytes
  room_XXX.oam      544 bytes
With --pre-write ADDR=VALUE (repeatable), those bytes are written before each
room loads. --room-flags LO,HI fills every room's persistent flag word
($7EF000 + 2*room, 296 words) before each load; Module05_LoadFile keeps them
(it never copies SRAM), but every load writes the visited room's word back,
so the whole table is rewritten each time. --par CODE (repeatable) adds Pro
Action Replay codes, e.g. 01B6B300 (draw shutters open) or 01C30080 (skip room
tags). Capture each state variant into its own --out folder.

Use an isolated Mesen instance loaded with the same ROM yaze will compare
against, for example:
  oracle-of-secrets/scripts/Mesen2/mesen2_launch_instance.sh \\
      --instance yaze-objcov --rom <rom> --headless --no-save-settings
  scripts/agents/capture-game-room-tilemaps.py \\
      --socket /tmp/mesen2-yaze-objcov.sock --rom <rom> --out <dir>

Rooms whose load ends in a different room, or never settles in the
underworld module, are recorded with an error status and no tilemap file.
"""

import argparse
import datetime
import hashlib
import json
import socket
import sys
import time
from pathlib import Path

ENTRANCE = 0x34
ENTRANCE_ROOM_TABLE = 0x02C813
ENTRANCE_MAIN_GFX = 0x02D381
ENTRANCE_DUNGEON_ID = 0x02D48B
ENTRANCE_COUNT = 0x85
DUNGEON_MAP_ROOMS_PTR = 0x57605   # PC, 14 bank-$0A pointers
DUNGEON_MAP_FLOORS = 0x575D9      # PC, 14 words: basements | floors << 4
TILEMAP_BG1 = 0x7E2000
TILEMAP_BG2 = 0x7E4000
TILEMAP_BYTES = 0x2000
ROOM_COUNT = 296
MODULE_UNDERWORLD = 0x07
SETTLE_POLLS = 10          # consecutive settled polls before capture
MAX_FRAMES_PER_ROOM = 900


class Mesen:
    def __init__(self, path):
        self.path = path

    def send(self, cmd, timeout=30.0):
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect(self.path)
        try:
            sock.sendall((json.dumps(cmd) + "\n").encode())
            buf = b""
            while b"\n" not in buf:
                chunk = sock.recv(1 << 20)
                if not chunk:
                    break
                buf += chunk
        finally:
            sock.close()
        resp = json.loads(buf.decode().strip())
        if not resp.get("success", False):
            raise RuntimeError(f"{cmd['type']} failed: {resp.get('error')}")
        return resp.get("data")

    def write(self, addr, value):
        self.send({"type": "WRITE", "addr": hex(addr), "value": hex(value)})

    def read(self, addr, length):
        data = self.send(
            {"type": "READBLOCK", "addr": hex(addr), "len": str(length)})
        return bytes.fromhex(data)

    def read_memtype(self, memtype, length):
        data = self.send({"type": "READBLOCK", "addr": "0x0",
                          "len": str(length), "memtype": memtype})
        return bytes.fromhex(data)

    def frame(self):
        return self.send({"type": "STATE"})["frame"]


def snes_to_pc(address):
    return ((address & 0x7F0000) >> 1) | (address & 0x7FFF)


def choose_entrances(rom):
    """Returns {room: (entrance, method)} for every room."""
    def byte(snes):
        return rom[snes_to_pc(snes)]

    def word(snes):
        pc = snes_to_pc(snes)
        return rom[pc] | rom[pc + 1] << 8

    entrances = [{"id": e, "room": word(ENTRANCE_ROOM_TABLE + 2 * e),
                  "dungeon": byte(ENTRANCE_DUNGEON_ID + e)}
                 for e in range(ENTRANCE_COUNT)]
    choice = {}
    for room in range(ROOM_COUNT):
        # Entrances 0x00/0x01 start a new game (Link's house) and do not load
        # through the normal underworld path.
        direct = [e for e in entrances if e["room"] == room and e["id"] > 0x01]
        if direct:
            choice[room] = (direct[0]["id"], "direct")
    for d in range(14):
        pointer = rom[DUNGEON_MAP_ROOMS_PTR + 2 * d] | (
            rom[DUNGEON_MAP_ROOMS_PTR + 2 * d + 1] << 8)
        pc = snes_to_pc(0x0A0000 | pointer)
        floors_byte = rom[DUNGEON_MAP_FLOORS + 2 * d]
        floors = (floors_byte & 0x0F) + (floors_byte >> 4)
        rooms = {rom[pc + i] for i in range(floors * 25)} - {0x0F}
        # Map index d is entrance dungeon ID 2*d in vanilla.
        candidates = [e for e in entrances if e["dungeon"] == 2 * d]
        if not candidates:
            continue
        for room in sorted(rooms):
            choice.setdefault(room, (candidates[0]["id"], f"dungeon-map {d}"))
    changed = True
    while changed:
        changed = False
        for room in range(ROOM_COUNT):
            if room in choice:
                continue
            for n in (room - 1, room + 1, room - 16, room + 16):
                same_row = n // 16 == room // 16 or abs(n - room) == 16
                if 0 <= n < ROOM_COUNT and same_row and n in choice:
                    choice[room] = (choice[n][0], f"neighbour 0x{n:03X}")
                    changed = True
                    break
    for room in range(ROOM_COUNT):
        choice.setdefault(room, (ENTRANCE, "fallback"))
    return choice


def load_room(mesen, room, pre_writes=(), entrance=ENTRANCE, room_flags=None,
              extra_par=()):
    """Loads `room`; returns (status, frames_run)."""
    mesen.send({"type": "PAUSE"})
    if room_flags is not None:
        lo, hi = room_flags
        mesen.send({"type": "WRITEBLOCK", "addr": hex(0x7EF000),
                    "hex": (f"{lo:02X}{hi:02X}" * ROOM_COUNT)})
    for addr, value in pre_writes:
        mesen.write(addr, value)
    mesen.send({"type": "CHEAT", "action": "clear"})
    for code in extra_par:
        mesen.send({"type": "CHEAT", "action": "add", "code": code,
                    "format": "par"})
    table = ENTRANCE_ROOM_TABLE + 2 * entrance
    for offset, byte in ((0, room & 0xFF), (1, room >> 8)):
        mesen.send({"type": "CHEAT", "action": "add",
                    "code": f"{table + offset:06X}{byte:02X}",
                    "format": "par"})
    mesen.write(0x7EF3CC, 0x00)       # no follower
    mesen.write(0x7E010E, entrance)
    mesen.write(0x7E0010, 0x05)       # module: load, then underworld
    mesen.write(0x7E0011, 0x00)
    mesen.write(0x7E001B, 0x00)

    start = mesen.frame()
    settled = 0
    loaded = -1
    mesen.send({"type": "RESUME"})
    try:
        while mesen.frame() - start < MAX_FRAMES_PER_ROOM:
            module, submodule = mesen.read(0x7E0010, 2)
            loaded = int.from_bytes(mesen.read(0x7E00A0, 2), "little")
            if module == MODULE_UNDERWORLD and submodule == 0:
                settled += 1
                if settled >= SETTLE_POLLS:
                    break
            else:
                settled = 0
            time.sleep(0.02)
    finally:
        mesen.send({"type": "PAUSE"})
    frames = mesen.frame() - start
    if settled < SETTLE_POLLS:
        return "not-settled", frames
    if loaded != room:
        return f"loaded-room-0x{loaded:03X}", frames
    return "ok", frames


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--socket", required=True, help="Mesen2 socket path")
    parser.add_argument("--rom", required=True,
                        help="ROM file loaded in Mesen (for its SHA-1)")
    parser.add_argument("--out", required=True, help="output directory")
    parser.add_argument("--rooms", default="all",
                        help="'all' or comma-separated room IDs (0x001,...)")
    parser.add_argument("--entrance", default="auto",
                        help="'auto' (per-room, see above) or a fixed entrance")
    parser.add_argument("--full", action="store_true",
                        help="also save WRAM, VRAM, CGRAM and OAM per room")
    parser.add_argument("--room-flags", default=None, metavar="LO,HI",
                        help="fill every room's flag word, e.g. 0xFF,0xFF")
    parser.add_argument("--par", action="append", default=[],
                        help="extra Pro Action Replay code (repeatable)")
    parser.add_argument("--pre-write", action="append", default=[],
                        metavar="ADDR=VALUE",
                        help="byte to write before each room loads")
    args = parser.parse_args()

    rooms = (range(ROOM_COUNT) if args.rooms == "all" else
             [int(r, 0) for r in args.rooms.split(",")])
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    manifest_path = out / "manifest.json"
    manifest = {"version": 1, "rooms": {}}
    if manifest_path.exists():
        manifest = json.loads(manifest_path.read_text())
    rom_bytes = Path(args.rom).read_bytes()
    rom_sha1 = hashlib.sha1(rom_bytes).hexdigest()
    if args.entrance == "auto":
        entrance_for = choose_entrances(rom_bytes)
    else:
        fixed = int(args.entrance, 0)
        entrance_for = {r: (fixed, "fixed") for r in range(ROOM_COUNT)}
    if manifest.get("rom_sha1", rom_sha1) != rom_sha1:
        sys.exit(f"{out} holds captures for ROM {manifest['rom_sha1']}, "
                 f"not {rom_sha1}; use another --out")
    manifest.update({
        "rom_sha1": rom_sha1,
        "entrance": args.entrance,
        "tilemaps": {"bg1": "0x7E2000", "bg2": "0x7E4000"},
        "source": "Mesen2-OOS",
    })

    pre_writes = []
    for item in args.pre_write:
        addr, value = item.split("=")
        pre_writes.append((int(addr, 0), int(value, 0)))
    if pre_writes:
        manifest["pre_writes"] = args.pre_write
    room_flags = None
    if args.room_flags:
        lo, hi = (int(v, 0) for v in args.room_flags.split(","))
        room_flags = (lo, hi)
        manifest["room_flags"] = args.room_flags
    if args.par:
        manifest["par"] = args.par
    mesen = Mesen(args.socket)
    for room in rooms:
        key = f"0x{room:03X}"
        path = out / f"room_{room:03X}.tilemap"
        try:
            entrance, method = entrance_for[room]
            status, frames = load_room(mesen, room, pre_writes, entrance,
                                       room_flags, args.par)
            if status != "ok" and entrance != ENTRANCE:
                method = f"{method}; {status} -> retried with 0x{ENTRANCE:02X}"
                entrance = ENTRANCE
                status, frames = load_room(mesen, room, pre_writes, entrance,
                                           room_flags, args.par)
        except (RuntimeError, OSError) as err:
            status, frames = f"error: {err}", 0
        entry = {"status": status, "frames": frames,
                 "entrance": f"0x{entrance:02X}", "entrance_method": method,
                 "main_gfx": rom_bytes[snes_to_pc(ENTRANCE_MAIN_GFX + entrance)],
                 "captured_utc": datetime.datetime.now(
                     datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}
        if status == "ok":
            data = mesen.read(TILEMAP_BG1, TILEMAP_BYTES) + mesen.read(
                TILEMAP_BG2, TILEMAP_BYTES)
            path.write_bytes(data)
            entry["file"] = path.name
            entry["sha1"] = hashlib.sha1(data).hexdigest()
            if args.full:
                wram = mesen.read(0x7E0000, 0x10000) + mesen.read(0x7F0000, 0x10000)
                dumps = {
                    "wram": wram,
                    "vram": mesen.read_memtype("vram", 0x10000),
                    "cgram": mesen.read_memtype("cgram", 512),
                    "oam": mesen.read_memtype("oam", 544),
                }
                for kind, blob in dumps.items():
                    (out / f"room_{room:03X}.{kind}").write_bytes(blob)
        elif path.exists():
            path.unlink()
        manifest["rooms"][key] = entry
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
        print(f"{key} {status} ({frames} frames)", flush=True)
    mesen.send({"type": "CHEAT", "action": "clear"})
    ok = sum(1 for e in manifest["rooms"].values() if e["status"] == "ok")
    print(f"{ok}/{len(manifest['rooms'])} rooms captured in {out}")


if __name__ == "__main__":
    main()
