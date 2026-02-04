"""
Vanilla ALTTP (US) ROM Profile

Complete location data for the original A Link to the Past (US version).
Contains 12 dungeons, numerous caves, houses, shops, and special areas.

Room data sourced from the ALTTP disassembly and z3ed ROM analysis.
"""

from .base import RomProfile, LocationConfig, LocationType
from .detect import register_profile


def _build_vanilla_locations() -> dict:
    """Build the vanilla ALTTP location configurations."""

    locations = {}

    # =========================================================================
    # DUNGEONS - Light World
    # =========================================================================

    locations["hyrule_castle"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Hyrule Castle",
        entrance_ids=[0x04, 0x05, 0x03],  # Main, Side, Sanctuary link
        ow_screen=0x1B,
        dungeon_id="HC",
        boss="Ball and Chain Guard",
        dungeon_item="Boomerang (via chest)",
        entrance_room=0x61,  # Castle Entrance
        boss_room=0x55,
        floors={
            "1F": {
                "name": "First Floor",
                "grid": "castle layout",
                "rooms": [0x61, 0x71, 0x72, 0x60, 0x70, 0x62],
            },
            "B1": {
                "name": "Basement",
                "grid": "linear",
                "rooms": [0x80, 0x81, 0x82, 0x51, 0x41, 0x32, 0x01],
            },
        },
        all_rooms=[
            0x61, 0x71, 0x72, 0x60, 0x70, 0x62,  # 1F
            0x80, 0x81, 0x82, 0x51, 0x41, 0x32, 0x01,  # B1 to Sanctuary
            0x55,  # Boss
        ],
        room_names={
            0x61: "Castle Entrance",
            0x71: "Hall 1 Left",
            0x72: "Hall 1 Right",
            0x60: "Guard Room West",
            0x70: "Guard Room East",
            0x62: "Throne Room",
            0x80: "Basement Entry",
            0x81: "Dark Hall",
            0x82: "Escape Route",
            0x51: "Jail Cell",
            0x41: "Secret Passage",
            0x32: "Rat Room",
            0x01: "Uncle's Room",
            0x55: "Ball & Chain Guard",
        },
    )

    locations["eastern_palace"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Eastern Palace",
        entrance_ids=[0x08],
        ow_screen=0x1E,
        dungeon_id="EP",
        boss="Armos Knights",
        dungeon_item="Bow",
        big_chest_room=0xA9,
        entrance_room=0xC9,
        boss_room=0xC8,
        floors={
            "1F": {
                "name": "First Floor",
                "grid": "H-shape",
                "rooms": [0xA8, 0xA9, 0xAA, 0xB8, 0xB9, 0xBA],
            },
            "2F": {
                "name": "Second Floor",
                "grid": "3x2",
                "rooms": [0xD8, 0xD9, 0xDA],
            },
            "3F": {
                "name": "Boss Floor",
                "grid": "linear",
                "rooms": [0xC8, 0xC9],
            },
        },
        all_rooms=[
            0xC9, 0xC8,  # Entry + Boss
            0xA8, 0xA9, 0xAA,  # 1F top
            0xB8, 0xB9, 0xBA,  # 1F bottom
            0xD8, 0xD9, 0xDA,  # 2F
        ],
        room_names={
            0xC9: "Entrance",
            0xA8: "Cannonball Room",
            0xA9: "Big Chest Room",
            0xAA: "NE Room",
            0xB8: "SW Room",
            0xB9: "Center Hall",
            0xBA: "SE Room",
            0xD8: "Dark Room",
            0xD9: "Key Room",
            0xDA: "Eyegore Room",
            0xC8: "Armos Knights (Boss)",
        },
    )

    locations["desert_palace"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Desert Palace",
        entrance_ids=[0x09, 0x0A, 0x0B, 0x0C],  # Main + 3 side entrances
        ow_screen=0x30,
        dungeon_id="DP",
        boss="Lanmolas",
        dungeon_item="Power Glove",
        big_chest_room=0x73,
        entrance_room=0x84,
        boss_room=0x33,
        floors={
            "1F": {
                "name": "Main Floor",
                "grid": "complex",
                "rooms": [0x83, 0x84, 0x85, 0x73, 0x74, 0x75, 0x63],
            },
            "B1": {
                "name": "Basement / Boss",
                "grid": "linear",
                "rooms": [0x43, 0x53, 0x33],
            },
        },
        all_rooms=[
            0x83, 0x84, 0x85,  # South row
            0x73, 0x74, 0x75,  # Middle row
            0x63, 0x53, 0x43, 0x33,  # North path to boss
        ],
        room_names={
            0x84: "Entrance",
            0x83: "West Entry",
            0x85: "East Entry",
            0x73: "Big Chest Room",
            0x74: "Torch Room",
            0x75: "Map Room",
            0x63: "Beamos Room",
            0x53: "Pre-Boss",
            0x43: "Key Room",
            0x33: "Lanmolas (Boss)",
        },
    )

    locations["tower_of_hera"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Tower of Hera",
        entrance_ids=[0x33],
        ow_screen=0x03,
        dungeon_id="TH",
        boss="Moldorm",
        dungeon_item="Moon Pearl",
        big_chest_room=0x27,
        entrance_room=0x77,
        boss_room=0x07,
        floors={
            "1F": {
                "name": "Entry",
                "grid": "single",
                "rooms": [0x77],
            },
            "2F-4F": {
                "name": "Climbing Floors",
                "grid": "vertical",
                "rooms": [0x87, 0x31, 0xA7, 0x17],
            },
            "5F": {
                "name": "Big Chest Floor",
                "grid": "single",
                "rooms": [0x27],
            },
            "6F": {
                "name": "Boss Floor",
                "grid": "single",
                "rooms": [0x07],
            },
        },
        all_rooms=[0x77, 0x87, 0x31, 0xA7, 0x17, 0x27, 0x07],
        room_names={
            0x77: "Entrance",
            0x87: "2F Beetles",
            0x31: "3F Star Tiles",
            0xA7: "4F Mini-Moldorm",
            0x17: "5F Hardhat Beetles",
            0x27: "Big Chest Room",
            0x07: "Moldorm (Boss)",
        },
    )

    # =========================================================================
    # DUNGEONS - Dark World
    # =========================================================================

    locations["palace_of_darkness"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Palace of Darkness",
        entrance_ids=[0x0D, 0x0E, 0x0F, 0x10],
        ow_screen=0x5E,
        dungeon_id="PD",
        boss="Helmasaur King",
        dungeon_item="Hammer",
        big_chest_room=0x1A,
        entrance_room=0x09,
        boss_room=0x5A,
        floors={
            "1F": {
                "name": "Main Floor",
                "grid": "complex",
                "rooms": [0x09, 0x0A, 0x19, 0x1A, 0x1B, 0x2A, 0x2B, 0x3A, 0x3B, 0x0B],
            },
            "B1": {
                "name": "Basement",
                "grid": "linear",
                "rooms": [0x4A, 0x6A, 0x5A],
            },
        },
        all_rooms=[
            0x09, 0x0A, 0x0B,  # Entry row
            0x19, 0x1A, 0x1B,  # Second row
            0x2A, 0x2B,  # Third row
            0x3A, 0x3B,  # Fourth row
            0x4A, 0x6A, 0x5A,  # Basement
        ],
        room_names={
            0x09: "Entrance",
            0x0A: "Lobby",
            0x0B: "East Side Room",
            0x19: "Dark Maze",
            0x1A: "Big Chest Room",
            0x1B: "Statue Push Room",
            0x2A: "Rupee Room",
            0x2B: "Turtle Room",
            0x3A: "Warp Room",
            0x3B: "Shooter Room",
            0x4A: "Dark Room",
            0x6A: "Boss Hallway",
            0x5A: "Helmasaur King (Boss)",
        },
    )

    locations["swamp_palace"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Swamp Palace",
        entrance_ids=[0x35, 0x36],
        ow_screen=0x7B,
        dungeon_id="SP",
        boss="Arrghus",
        dungeon_item="Hookshot",
        big_chest_room=0x36,
        entrance_room=0x28,
        boss_room=0x06,
        floors={
            "B1": {
                "name": "Entry Level",
                "grid": "linear",
                "rooms": [0x28, 0x38, 0x37],
            },
            "B2": {
                "name": "Main Level",
                "grid": "complex",
                "rooms": [0x26, 0x35, 0x36, 0x46, 0x66, 0x76],
            },
            "B3": {
                "name": "Boss Level",
                "grid": "linear",
                "rooms": [0x16, 0x06],
            },
        },
        all_rooms=[
            0x28, 0x38, 0x37,  # B1
            0x26, 0x35, 0x36, 0x46, 0x66, 0x76,  # B2
            0x16, 0x06,  # B3
        ],
        room_names={
            0x28: "Entrance",
            0x38: "Waterway",
            0x37: "Key Room",
            0x26: "Compass Room",
            0x35: "Big Key Room",
            0x36: "Big Chest Room",
            0x46: "Statue Room",
            0x66: "Flooded Room",
            0x76: "Water Drain Room",
            0x16: "Pre-Boss",
            0x06: "Arrghus (Boss)",
        },
    )

    locations["skull_woods"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Skull Woods",
        entrance_ids=[0x28, 0x29, 0x2A, 0x2B],  # Multiple forest entrances
        ow_screen=0x40,
        dungeon_id="SW",
        boss="Mothula",
        dungeon_item="Fire Rod",
        big_chest_room=0x58,
        entrance_room=0x67,
        boss_room=0x29,
        floors={
            "B1": {
                "name": "Main Floor",
                "grid": "scattered",
                "rooms": [0x56, 0x57, 0x58, 0x59, 0x66, 0x67, 0x68],
            },
            "B2": {
                "name": "Boss Area",
                "grid": "linear",
                "rooms": [0x39, 0x49, 0x29],
            },
        },
        all_rooms=[
            0x56, 0x57, 0x58, 0x59,  # Upper rooms
            0x66, 0x67, 0x68,  # Lower rooms
            0x39, 0x49, 0x29,  # Boss path
        ],
        room_names={
            0x67: "Main Entrance",
            0x56: "SW Pit Room",
            0x57: "Key Room",
            0x58: "Big Chest Room",
            0x59: "Compass Room",
            0x66: "Gibdo Room",
            0x68: "Moving Wall Room",
            0x39: "Fire Path",
            0x49: "Pre-Boss",
            0x29: "Mothula (Boss)",
        },
    )

    locations["thieves_town"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Thieves' Town",
        entrance_ids=[0x2C, 0x2D],
        ow_screen=0x58,
        dungeon_id="TT",
        boss="Blind the Thief",
        dungeon_item="Titan's Mitt",
        big_chest_room=0x44,
        entrance_room=0xDB,
        boss_room=0xAC,
        floors={
            "1F": {
                "name": "Main Floor",
                "grid": "complex",
                "rooms": [0xDB, 0xCB, 0xCC, 0xBC, 0xBB, 0xAB],
            },
            "B1": {
                "name": "Prison",
                "grid": "linear",
                "rooms": [0x44, 0x45, 0x64, 0x65],
            },
            "B2": {
                "name": "Boss Level",
                "grid": "single",
                "rooms": [0xAC],
            },
        },
        all_rooms=[
            0xDB, 0xCB, 0xCC, 0xBC, 0xBB, 0xAB,  # 1F
            0x44, 0x45, 0x64, 0x65,  # B1
            0xAC,  # Boss
        ],
        room_names={
            0xDB: "Entrance",
            0xCB: "Lobby",
            0xCC: "Cell Block",
            0xBC: "Big Key Room",
            0xBB: "Hidden Switch",
            0xAB: "Attic Access",
            0x44: "Big Chest Room (Blind's Cell)",
            0x45: "Conveyor Room",
            0x64: "Spike Room",
            0x65: "Jail Cells",
            0xAC: "Blind the Thief (Boss)",
        },
    )

    locations["ice_palace"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Ice Palace",
        entrance_ids=[0x2F, 0x30, 0x31, 0x32],
        ow_screen=0x75,
        dungeon_id="IP",
        boss="Kholdstare",
        dungeon_item="Blue Mail",
        big_chest_room=0x9F,
        entrance_room=0x0E,
        boss_room=0xDE,
        floors={
            "B1": {
                "name": "Entry Level",
                "grid": "single",
                "rooms": [0x0E],
            },
            "B2-B3": {
                "name": "Main Floors",
                "grid": "complex",
                "rooms": [0x1E, 0x2E, 0x3E, 0x4E, 0x5E, 0x6E, 0x7E],
            },
            "B4-B5": {
                "name": "Deep Floors",
                "grid": "linear",
                "rooms": [0x8E, 0x9E, 0x9F, 0xAE, 0xBE],
            },
            "B6": {
                "name": "Boss Floor",
                "grid": "linear",
                "rooms": [0xCE, 0xDE],
            },
        },
        all_rooms=[
            0x0E,  # B1
            0x1E, 0x2E, 0x3E, 0x4E, 0x5E, 0x6E, 0x7E,  # B2-B3
            0x8E, 0x9E, 0x9F, 0xAE, 0xBE,  # B4-B5
            0xCE, 0xDE,  # B6
        ],
        room_names={
            0x0E: "Entrance",
            0x1E: "Pengator Room",
            0x2E: "Bomb Floor",
            0x3E: "Ice Bridge",
            0x4E: "Conveyor Room",
            0x5E: "Switch Room",
            0x6E: "Block Push",
            0x7E: "Ice Fall Room",
            0x8E: "Spike Room",
            0x9E: "Map Room",
            0x9F: "Big Chest Room",
            0xAE: "Ice T Room",
            0xBE: "Tall Room",
            0xCE: "Pre-Boss",
            0xDE: "Kholdstare (Boss)",
        },
    )

    locations["misery_mire"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Misery Mire",
        entrance_ids=[0x13, 0x14, 0x15, 0x16, 0x17, 0x18],
        ow_screen=0x70,
        dungeon_id="MM",
        boss="Vitreous",
        dungeon_item="Cane of Somaria",
        big_chest_room=0xC3,
        entrance_room=0x98,
        boss_room=0x90,
        floors={
            "1F": {
                "name": "Main Floor",
                "grid": "complex",
                "rooms": [0x97, 0x98, 0xA2, 0xA3, 0xB2, 0xB3, 0xC2, 0xC3, 0xD2],
            },
            "B1": {
                "name": "Basement",
                "grid": "complex",
                "rooms": [0x91, 0x93, 0xA1, 0xA0, 0xB1, 0xC1, 0xD1],
            },
            "B2": {
                "name": "Boss Level",
                "grid": "single",
                "rooms": [0x90],
            },
        },
        all_rooms=[
            0x97, 0x98, 0xA2, 0xA3, 0xB2, 0xB3, 0xC2, 0xC3, 0xD2,  # 1F
            0x91, 0x93, 0xA1, 0xA0, 0xB1, 0xC1, 0xD1,  # B1
            0x90,  # Boss
        ],
        room_names={
            0x98: "Entrance",
            0x97: "West Room",
            0xA2: "Map Room",
            0xA3: "Wizzrobe Room",
            0xB2: "Spike Room",
            0xB3: "Compass Room",
            0xC2: "Torch Room",
            0xC3: "Big Chest Room",
            0xD2: "Hub Room",
            0x91: "Dark Room",
            0x93: "Switch Room",
            0xA1: "Block Room",
            0xA0: "Conveyor Room",
            0xB1: "Big Key Room",
            0xC1: "Warp Room",
            0xD1: "Pre-Boss",
            0x90: "Vitreous (Boss)",
        },
    )

    locations["turtle_rock"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Turtle Rock",
        entrance_ids=[0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22],
        ow_screen=0x47,
        dungeon_id="TR",
        boss="Trinexx",
        dungeon_item="Mirror Shield",
        big_chest_room=0x24,
        entrance_room=0xD6,
        boss_room=0xA4,
        floors={
            "1F": {
                "name": "Entry Level",
                "grid": "complex",
                "rooms": [0xD6, 0xC6, 0xC7, 0xB6, 0xB7],
            },
            "B1": {
                "name": "Main Level",
                "grid": "complex",
                "rooms": [0x13, 0x14, 0x23, 0x24, 0x04, 0x15],
            },
            "B2": {
                "name": "Deep Level",
                "grid": "complex",
                "rooms": [0xD5, 0xC5, 0xB5, 0xB4, 0xA4],
            },
        },
        all_rooms=[
            0xD6, 0xC6, 0xC7, 0xB6, 0xB7,  # 1F
            0x13, 0x14, 0x23, 0x24, 0x04, 0x15,  # B1
            0xD5, 0xC5, 0xB5, 0xB4, 0xA4,  # B2
        ],
        room_names={
            0xD6: "Entrance",
            0xC6: "Chain Chomp Room",
            0xC7: "Roller Room",
            0xB6: "Crystal Switch",
            0xB7: "Double Roller",
            0x13: "Pipe Room",
            0x14: "Compass Room",
            0x23: "Big Chest Room",
            0x24: "Big Key Room",
            0x04: "Torch Room",
            0x15: "Laser Eye Room",
            0xD5: "Dark Maze",
            0xC5: "Laser Bridge",
            0xB5: "Switch Room",
            0xB4: "Pre-Boss",
            0xA4: "Trinexx (Boss)",
        },
    )

    locations["ganons_tower"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Ganon's Tower",
        entrance_ids=[0x24, 0x25, 0x26, 0x27],
        ow_screen=0x43,
        dungeon_id="GT",
        boss="Agahnim 2 -> Ganon",
        dungeon_item="Red Mail",
        big_chest_room=0x8C,
        entrance_room=0x0C,
        boss_room=0x0D,
        floors={
            "1F": {
                "name": "Entry",
                "grid": "single",
                "rooms": [0x0C],
            },
            "2F": {
                "name": "Ice/Fire Split",
                "grid": "split",
                "rooms": [0x1C, 0x1D, 0x3D, 0x4C, 0x4D, 0x5D, 0x6C, 0x6D],
            },
            "3F": {
                "name": "Convergence",
                "grid": "complex",
                "rooms": [0x8C, 0x8D, 0x9C, 0x9D, 0x3C],
            },
            "4F-5F": {
                "name": "Upper Floors",
                "grid": "complex",
                "rooms": [0x5C, 0x7C, 0x7D, 0xA5, 0xB5],
            },
            "6F": {
                "name": "Boss Floor",
                "grid": "linear",
                "rooms": [0x1D, 0x0D],
            },
        },
        all_rooms=[
            0x0C,  # Entry
            0x1C, 0x1D, 0x3D, 0x4C, 0x4D, 0x5D, 0x6C, 0x6D,  # Split paths
            0x8C, 0x8D, 0x9C, 0x9D, 0x3C,  # Convergence
            0x5C, 0x7C, 0x7D, 0xA5, 0xB5,  # Upper
            0x0D,  # Boss
        ],
        room_names={
            0x0C: "Entrance",
            0x1C: "Ice Path",
            0x1D: "Fire Path",
            0x3D: "Mini-Helmasaur",
            0x4C: "Torch Room",
            0x4D: "Conveyor Room",
            0x5D: "Spike Room",
            0x6C: "Warp Room",
            0x6D: "Switch Room",
            0x8C: "Big Chest Room",
            0x8D: "Map Room",
            0x9C: "Moldorm 2",
            0x9D: "Tile Room",
            0x3C: "Firebar Room",
            0x5C: "Armos Room",
            0x7C: "Lanmolas 2",
            0x7D: "Gauntlet",
            0xA5: "Stairs",
            0xB5: "Pre-Boss",
            0x0D: "Agahnim 2 (Boss)",
        },
    )

    # =========================================================================
    # SANCTUARY AND CASTLE ESCAPE
    # =========================================================================

    locations["sanctuary"] = LocationConfig(
        type=LocationType.SPECIAL,
        name="Sanctuary",
        entrance_ids=[0x02],
        ow_screen=0x13,
        purpose="Zelda safe haven / Cemetery church",
        all_rooms=[0x12],
        room_names={
            0x12: "Sanctuary",
        },
    )

    locations["castle_tower"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Castle Tower (Agahnim)",
        entrance_ids=[0x06, 0x07],
        ow_screen=0x1B,
        dungeon_id="AT",
        boss="Agahnim",
        entrance_room=0xE0,
        boss_room=0x20,
        floors={
            "1F-3F": {
                "name": "Tower Climb",
                "grid": "vertical",
                "rooms": [0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x30, 0x20],
            },
        },
        all_rooms=[0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x30, 0x20],
        room_names={
            0xE0: "Entrance",
            0xD0: "Dark Room",
            0xC0: "Ball & Chain",
            0xB0: "Sword Room",
            0xA0: "Snake Room",
            0x30: "Red Knight Room",
            0x20: "Agahnim (Boss)",
        },
    )

    # =========================================================================
    # CAVES - Light World
    # =========================================================================

    locations["lost_woods_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Lost Woods Cave",
        entrance_ids=[0x00],
        region="Lost Woods",
        ow_screens=[0x00],
        cave_type="passage",
        contents="Piece of Heart",
    )

    locations["kakariko_well"] = LocationConfig(
        type=LocationType.CAVE,
        name="Kakariko Well",
        entrance_ids=[0x2F],
        region="Kakariko",
        ow_screens=[0x2A],
        cave_type="secret",
        contents="Piece of Heart, Bombs",
    )

    locations["blind_hideout"] = LocationConfig(
        type=LocationType.CAVE,
        name="Blind's Hideout",
        entrance_ids=[0x30],
        region="Kakariko",
        ow_screens=[0x29],
        cave_type="secret",
        contents="Rupees",
    )

    locations["hyrule_castle_secret"] = LocationConfig(
        type=LocationType.CAVE,
        name="Hyrule Castle Secret Entrance",
        entrance_ids=[0x31],
        region="Hyrule Castle",
        ow_screens=[0x1B],
        cave_type="passage",
        connects=["Castle moat", "Throne room"],
    )

    locations["waterfall_fairy"] = LocationConfig(
        type=LocationType.CAVE,
        name="Waterfall of Wishing",
        entrance_ids=[0x3C],
        region="Zora's River",
        ow_screens=[0x0F],
        cave_type="fairy",
        contents="Upgraded items (throw in)",
    )

    locations["kings_tomb"] = LocationConfig(
        type=LocationType.CAVE,
        name="King's Tomb",
        entrance_ids=[0x34],
        region="Cemetery",
        ow_screens=[0x14],
        cave_type="special",
        contents="Cape",
    )

    locations["old_man_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Old Man Cave",
        entrance_ids=[0x48, 0x49, 0x4A],
        region="Death Mountain",
        ow_screens=[0x03, 0x0A, 0x0B],
        cave_type="passage",
        connects=["Mountain base", "Old man house", "DM East"],
    )

    locations["spectacle_rock_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Spectacle Rock Cave",
        entrance_ids=[0x39],
        region="Death Mountain",
        ow_screens=[0x03],
        cave_type="treasure",
        contents="Piece of Heart",
    )

    locations["paradox_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Paradox Cave",
        entrance_ids=[0x50, 0x51],
        region="Death Mountain East",
        ow_screens=[0x05, 0x07],
        cave_type="passage",
        connects=["Lower east", "Upper east"],
        contents="Pieces of Heart, Rupees",
    )

    locations["spiral_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Spiral Cave",
        entrance_ids=[0x52, 0x53],
        region="Death Mountain East",
        ow_screens=[0x05],
        cave_type="passage",
        connects=["Lower", "Upper (Hera path)"],
    )

    locations["mimic_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Mimic Cave",
        entrance_ids=[0x37],
        region="Death Mountain",
        ow_screens=[0x07],
        cave_type="special",
        contents="Piece of Heart (DW access required)",
    )

    locations["ice_rod_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Ice Rod Cave",
        entrance_ids=[0x40],
        region="Southeast",
        ow_screens=[0x37],
        cave_type="treasure",
        contents="Ice Rod",
    )

    locations["bonk_rocks"] = LocationConfig(
        type=LocationType.CAVE,
        name="Bonk Rocks",
        entrance_ids=[0x42],
        region="Sanctuary Area",
        ow_screens=[0x13],
        cave_type="secret",
        contents="Piece of Heart",
    )

    locations["checkerboard_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Checkerboard Cave",
        entrance_ids=[0x43],
        region="Desert Area",
        ow_screens=[0x30],
        cave_type="special",
        contents="Piece of Heart (DW mirror required)",
    )

    locations["mini_moldorm_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Mini Moldorm Cave",
        entrance_ids=[0x44],
        region="Southeast",
        ow_screens=[0x35],
        cave_type="treasure",
        contents="Rupees, Piece of Heart",
    )

    # =========================================================================
    # CAVES - Dark World
    # =========================================================================

    locations["spike_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Spike Cave",
        entrance_ids=[0x54],
        region="Dark Death Mountain",
        ow_screens=[0x43],
        cave_type="treasure",
        contents="Cape, Piece of Heart (Hammer + Cape required)",
    )

    locations["hookshot_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Hookshot Cave",
        entrance_ids=[0x55, 0x56],
        region="Dark Death Mountain",
        ow_screens=[0x45],
        cave_type="treasure",
        connects=["Back entrance", "Bonkable rocks"],
        contents="Pieces of Heart",
    )

    locations["superbunny_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Superbunny Cave",
        entrance_ids=[0x58, 0x59],
        region="Dark Death Mountain",
        ow_screens=[0x45, 0x47],
        cave_type="passage",
        contents="Pieces of Heart",
    )

    locations["dark_death_mountain_shop"] = LocationConfig(
        type=LocationType.SHOP,
        name="Dark Death Mountain Shop",
        entrance_ids=[0x57],
        ow_screen=0x45,
        inventory="General items",
    )

    locations["bumper_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Bumper Cave",
        entrance_ids=[0x5A, 0x5B],
        region="Dark World Northwest",
        ow_screens=[0x4A],
        cave_type="passage",
        connects=["Lower", "Upper (to cape area)"],
    )

    locations["chest_game"] = LocationConfig(
        type=LocationType.SPECIAL,
        name="Treasure Chest Game",
        entrance_ids=[0x5C],
        ow_screen=0x46,
        purpose="Minigame",
        contents="Piece of Heart",
    )

    locations["c_shaped_house"] = LocationConfig(
        type=LocationType.CAVE,
        name="C-Shaped House",
        entrance_ids=[0x5D],
        region="Outcasts",
        ow_screens=[0x58],
        cave_type="secret",
        contents="Rupees",
    )

    locations["hammer_pegs_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Hammer Pegs Cave",
        entrance_ids=[0x5E],
        region="Outcasts",
        ow_screens=[0x52],
        cave_type="treasure",
        contents="Piece of Heart (Titan's Mitt + Hammer)",
    )

    # =========================================================================
    # HOUSES - Light World
    # =========================================================================

    locations["links_house_vanilla"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Link's House",
        entrance_ids=[0x01],
        ow_screen=0x2C,
        notable="Starting location",
    )

    locations["sahasrahlas_hut"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Sahasrahla's Hut",
        entrance_ids=[0x38],
        ow_screen=0x1E,
        notable="Sage - gives Pegasus Boots",
    )

    locations["sick_kid_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Sick Kid's House",
        entrance_ids=[0x39],
        ow_screen=0x29,
        notable="Trade Bug Net for bottle",
    )

    locations["library"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Library",
        entrance_ids=[0x3A],
        ow_screen=0x29,
        notable="Book of Mudora (Pegasus Boots)",
    )

    locations["tavern"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Village of Outcasts Tavern",
        entrance_ids=[0x3B],
        ow_screen=0x29,
        notable="Back room chest",
    )

    locations["old_man_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Old Man's House",
        entrance_ids=[0x4B],
        ow_screen=0x0A,
        notable="Magic Mirror location",
    )

    locations["fortune_teller_lw"] = LocationConfig(
        type=LocationType.SHOP,
        name="Fortune Teller (Light)",
        entrance_ids=[0x45],
        ow_screen=0x22,
        inventory="Fortunes",
    )

    # =========================================================================
    # HOUSES - Dark World
    # =========================================================================

    locations["arrow_game"] = LocationConfig(
        type=LocationType.SPECIAL,
        name="Arrow Game",
        entrance_ids=[0x5F],
        ow_screen=0x69,
        purpose="Minigame - Piece of Heart",
    )

    locations["digging_game"] = LocationConfig(
        type=LocationType.SPECIAL,
        name="Digging Game",
        entrance_ids=[0x60],
        ow_screen=0x68,
        purpose="Minigame - Piece of Heart",
    )

    locations["fortune_teller_dw"] = LocationConfig(
        type=LocationType.SHOP,
        name="Fortune Teller (Dark)",
        entrance_ids=[0x61],
        ow_screen=0x62,
        inventory="Fortunes",
    )

    locations["dark_world_shop"] = LocationConfig(
        type=LocationType.SHOP,
        name="Dark World Shop",
        entrance_ids=[0x62],
        ow_screen=0x58,
        inventory="Red shield, Bombs, Bee",
    )

    locations["bomb_shop"] = LocationConfig(
        type=LocationType.SHOP,
        name="Bomb Shop",
        entrance_ids=[0x63],
        ow_screen=0x6C,
        inventory="Super Bombs",
    )

    # =========================================================================
    # FAIRY FOUNTAINS
    # =========================================================================

    locations["pyramid_fairy"] = LocationConfig(
        type=LocationType.CAVE,
        name="Pyramid Fairy",
        entrance_ids=[0x64],
        region="Pyramid",
        ow_screens=[0x5B],
        cave_type="fairy",
        contents="Silver Arrows (Super Bomb entrance)",
    )

    locations["lake_hylia_fairy"] = LocationConfig(
        type=LocationType.CAVE,
        name="Lake Hylia Fairy",
        entrance_ids=[0x46],
        region="Lake Hylia",
        ow_screens=[0x35],
        cave_type="fairy",
        contents="Healing",
    )

    locations["desert_fairy"] = LocationConfig(
        type=LocationType.CAVE,
        name="Desert Fairy",
        entrance_ids=[0x47],
        region="Desert",
        ow_screens=[0x30],
        cave_type="fairy",
        contents="Healing",
    )

    # =========================================================================
    # SHOPS - Light World
    # =========================================================================

    locations["kakariko_shop"] = LocationConfig(
        type=LocationType.SHOP,
        name="Kakariko Shop",
        entrance_ids=[0x41],
        ow_screen=0x28,
        inventory="Shields, Bombs, Arrows",
    )

    locations["lake_hylia_shop"] = LocationConfig(
        type=LocationType.SHOP,
        name="Lake Hylia Shop",
        entrance_ids=[0x36],
        ow_screen=0x35,
        inventory="Hearts, Bombs, Arrows",
    )

    locations["witch_hut"] = LocationConfig(
        type=LocationType.SHOP,
        name="Witch's Hut",
        entrance_ids=[0x3D],
        ow_screen=0x16,
        inventory="Potions (bring mushroom)",
    )

    return locations


# Build the vanilla ALTTP profile
VANILLA_PROFILE = RomProfile(
    name="A Link to the Past (US)",
    short_name="vanilla",
    description="Original US release of The Legend of Zelda: A Link to the Past",
    rom_title_pattern=r"THE LEGEND OF ZELDA|ZELDA3|ZELDA 3",
    locations=_build_vanilla_locations(),
    docs_subdir="Docs/World",
)

# Register for auto-detection
register_profile(VANILLA_PROFILE)
