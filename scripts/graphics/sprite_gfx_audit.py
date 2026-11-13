#!/usr/bin/env python3
"""Flag dungeon rooms where a placed sprite's spriteset sheets or room OBJ palette differ from vanilla.

No ROM table says which sheets a sprite draws from, so the rules come from a reference ROM: a sprite id
placed in --min-rooms or more reference rooms needs a slot's sheet when every one of those rooms loads
the same sheet in that slot, and the same for the room-dependent OBJ palettes 0, 5 and 6.
"""

import argparse
import collections
from concurrent.futures import ThreadPoolExecutor
import csv
import json
from pathlib import Path
import subprocess
import sys

ROOM_COUNT = 0x128
SPRITE_COUNT = 0xF3
OAM_PROP_PC = 0x6B359  # SpriteData_OAMProp ($0D:B359): vhoopppc-style default per sprite id
PALETTE_SET_PC = 0x75460  # UnderworldPaletteSets: 4 bytes per room palette set
# OBJ palette -> (palette set byte, z3ed group); 1-4 and 7 are global, so no room can get them wrong
ROOM_PALETTES = {0: (1, "sprites_aux1"), 5: (2, "sprites_aux3"), 6: (3, "sprites_aux3")}


class Z3ed:
    def __init__(self, path, rom):
        self.path, self.rom = path, rom

    def __call__(self, command, *args):
        result = subprocess.run([self.path, command, f"--rom={self.rom}", *args, "--format", "json"],
                                capture_output=True, text=True)
        if result.returncode != 0:
            sys.exit(f"{self.path} {command} {' '.join(args)} failed: {result.stderr.strip()[:200]}")
        out = result.stdout
        return json.loads(out[out.index("{"):])


class RomData:
    """Every dungeon sprite placement in a ROM, with the sheets and OBJ palettes its room loads."""

    def __init__(self, z3ed, rom, jobs):
        self.rom = rom
        self.bytes = Path(rom).read_bytes()
        z3 = Z3ed(z3ed, rom)
        inventory = z3("gfx-sheet-inventory")
        self.spritesets = {int(k): v for k, v in inventory["spritesets"].items()}
        self.sheet_sha1 = {s["gfx"]: s["sha1"] for s in inventory["sheets"] if s.get("sha1")}
        self.colors = {}
        self._z3 = z3

        def room(room_id):
            header = z3("dungeon-room-header", "--room", hex(room_id))["Room Header Debug"]["decoded"]
            sprites = z3("dungeon-list-sprites", "--room", hex(room_id))["Dungeon Room Sprites"]["sprites"]
            return room_id, header, sprites

        with ThreadPoolExecutor(jobs) as pool:
            rooms = list(pool.map(room, range(ROOM_COUNT)))
        self.placements = []
        for room_id, header, sprites in rooms:
            sheets = self.spritesets.get(header["spriteset"] + 0x40, [0, 0, 0, 0])
            at = PALETTE_SET_PC + header["palette"] * 4
            palette_set = self.bytes[at:at + 4]
            seen = set()
            for sprite in sprites:
                sprite_id = int(sprite["sprite_id"], 16)
                overlord = (sprite["subtype"] & 7) == 7 and 0 < sprite_id <= 0x1A  # same test as zelda3::Sprite
                if overlord or sprite_id >= SPRITE_COUNT or sprite_id in seen:
                    continue
                seen.add(sprite_id)
                obj_palette = (self.bytes[OAM_PROP_PC + sprite_id] >> 1) & 7
                aux = palette_set[ROOM_PALETTES[obj_palette][0]] if obj_palette in ROOM_PALETTES else None
                self.placements.append(dict(room=room_id, sprite=sprite_id, name=sprite["name"],
                                            spriteset=header["spriteset"], palette_set=header["palette"],
                                            sheets=sheets, obj_palette=obj_palette, aux=aux))

    def palette_colors(self, group, index):
        if (group, index) not in self.colors:
            out = self._z3("palette-get-colors", "--group", group, "--index", str(index))
            self.colors[group, index] = out["Palette Colors"].get("colors", [])
        return self.colors[group, index]

    def same_sheet(self, a, b):
        sha1 = self.sheet_sha1.get(a + 0x73)
        return a == b or (sha1 is not None and sha1 == self.sheet_sha1.get(b + 0x73))


def rules(placements, min_rooms):
    by_sprite = collections.defaultdict(list)
    for p in placements:
        by_sprite[p["sprite"]].append(p)
    out = {}
    for sprite_id, ps in by_sprite.items():
        if len(ps) < min_rooms:
            continue
        slots = {slot: ps[0]["sheets"][slot] for slot in range(4)
                 if ps[0]["sheets"][slot] and all(p["sheets"][slot] == ps[0]["sheets"][slot] for p in ps)}
        aux = ps[0]["aux"] if ps[0]["aux"] is not None and all(p["aux"] == ps[0]["aux"] for p in ps) else None
        if slots or aux is not None:
            out[sprite_id] = dict(rooms=len(ps), slots=slots, aux=aux)
    return out


def audit(rom, rule_set, skip):
    flags = []
    for p in rom.placements:
        rule = rule_set.get(p["sprite"])
        if not rule or p["sprite"] in skip:
            continue
        issues = [dict(kind="sheet", slot=slot, has=p["sheets"][slot], usual=sheet)
                  for slot, sheet in rule["slots"].items() if not rom.same_sheet(p["sheets"][slot], sheet)]
        if rule["aux"] is not None and p["aux"] is not None and p["aux"] != rule["aux"]:
            group = ROOM_PALETTES[p["obj_palette"]][1]
            if rom.palette_colors(group, p["aux"]) != rom.palette_colors(group, rule["aux"]):
                issues.append(dict(kind="palette", obj_palette=p["obj_palette"], group=group,
                                   has=p["aux"], usual=rule["aux"]))
        if issues:
            flags.append(dict(room=p["room"], sprite=p["sprite"], name=p["name"], spriteset=p["spriteset"],
                              palette_set=p["palette_set"], sheets=p["sheets"], rule_rooms=rule["rooms"],
                              issues=issues))
    return flags


def describe(issue):
    if issue["kind"] == "sheet":
        return f"slot {issue['slot']} ${issue['has']:02X} (usually ${issue['usual']:02X})"
    return f"OBJ palette {issue['obj_palette']} {issue['group']}:{issue['has']} (usually :{issue['usual']})"


def registry_ids(path):
    with open(path, newline="") as f:
        return {int(row["id"].strip().lstrip("$").replace("0x", ""), 16) for row in csv.DictReader(f) if row.get("id")}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--rom", required=True, help="ROM to audit (use the patched ROM: custom sprites set OBJ palettes in ASM)")
    parser.add_argument("--reference", required=True, help="ROM the rules come from, normally vanilla")
    parser.add_argument("--sprite-registry", help="CSV with an id column; those ids are custom art, so vanilla rules skip them")
    parser.add_argument("--min-rooms", type=int, default=3, help="reference rooms a sprite id needs before it gets a rule (default 3)")
    parser.add_argument("--out", help="write rules and flags as JSON")
    parser.add_argument("--z3ed", default="z3ed")
    parser.add_argument("--jobs", type=int, default=8)
    args = parser.parse_args()

    skip = registry_ids(args.sprite_registry) if args.sprite_registry else set()
    reference = RomData(args.z3ed, args.reference, args.jobs)
    rom = RomData(args.z3ed, args.rom, args.jobs)
    rule_set = rules(reference.placements, args.min_rooms)
    flags = audit(rom, rule_set, skip)

    for f in flags:
        print(f"room 0x{f['room']:03X}  sprite 0x{f['sprite']:02X} {f['name']:<16} spriteset {f['spriteset']:<3} "
              + "; ".join(describe(i) for i in f["issues"]))
    kinds = collections.Counter(i["kind"] for f in flags for i in f["issues"])
    print(f"{len(rule_set)} rules from {len(reference.placements)} reference placements; "
          f"{len(rom.placements)} placements audited, {len(skip & {p['sprite'] for p in rom.placements})} registry ids skipped; "
          f"{len(flags)} flagged ({kinds['sheet']} sheet, {kinds['palette']} palette)")
    if args.out:
        Path(args.out).write_text(json.dumps(dict(
            rom=args.rom, reference=args.reference, min_rooms=args.min_rooms,
            rules={f"0x{k:02X}": v for k, v in sorted(rule_set.items())}, flags=flags), indent=1) + "\n")


if __name__ == "__main__":
    main()
