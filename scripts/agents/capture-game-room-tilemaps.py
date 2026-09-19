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

Output directory:
  room_XXX.tilemap  16384 bytes: TILEMAPA (BG1) then TILEMAPB (BG2),
                    little-endian 16-bit tile words, row-major 64x64 each.
  manifest.json     ROM SHA-1, entrance, and a status for every room.

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

    def frame(self):
        return self.send({"type": "STATE"})["frame"]


def load_room(mesen, room):
    """Loads `room`; returns (status, frames_run)."""
    mesen.send({"type": "PAUSE"})
    mesen.send({"type": "CHEAT", "action": "clear"})
    table = ENTRANCE_ROOM_TABLE + 2 * ENTRANCE
    for offset, byte in ((0, room & 0xFF), (1, room >> 8)):
        mesen.send({"type": "CHEAT", "action": "add",
                    "code": f"{table + offset:06X}{byte:02X}",
                    "format": "par"})
    mesen.write(0x7EF3CC, 0x00)       # no follower
    mesen.write(0x7E010E, ENTRANCE)
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
    args = parser.parse_args()

    rooms = (range(ROOM_COUNT) if args.rooms == "all" else
             [int(r, 0) for r in args.rooms.split(",")])
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    manifest_path = out / "manifest.json"
    manifest = {"version": 1, "rooms": {}}
    if manifest_path.exists():
        manifest = json.loads(manifest_path.read_text())
    rom_sha1 = hashlib.sha1(Path(args.rom).read_bytes()).hexdigest()
    if manifest.get("rom_sha1", rom_sha1) != rom_sha1:
        sys.exit(f"{out} holds captures for ROM {manifest['rom_sha1']}, "
                 f"not {rom_sha1}; use another --out")
    manifest.update({
        "rom_sha1": rom_sha1,
        "entrance": f"0x{ENTRANCE:02X}",
        "tilemaps": {"bg1": "0x7E2000", "bg2": "0x7E4000"},
        "source": "Mesen2-OOS",
    })

    mesen = Mesen(args.socket)
    for room in rooms:
        key = f"0x{room:03X}"
        path = out / f"room_{room:03X}.tilemap"
        try:
            status, frames = load_room(mesen, room)
        except (RuntimeError, OSError) as err:
            status, frames = f"error: {err}", 0
        entry = {"status": status, "frames": frames,
                 "captured_utc": datetime.datetime.now(
                     datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}
        if status == "ok":
            data = mesen.read(TILEMAP_BG1, TILEMAP_BYTES) + mesen.read(
                TILEMAP_BG2, TILEMAP_BYTES)
            path.write_bytes(data)
            entry["file"] = path.name
            entry["sha1"] = hashlib.sha1(data).hexdigest()
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
