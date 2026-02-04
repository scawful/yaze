"""
Oracle of Secrets ROM Profile

Location data for the Oracle of Secrets ALTTP ROM hack.
Contains 7 dungeons, 6 shrines, numerous caves, houses, shops, and special areas.
"""

from .base import RomProfile, LocationConfig, LocationType
from .detect import register_profile


def _build_oracle_locations() -> dict:
    """Build the Oracle of Secrets location configurations."""

    locations = {}

    # =========================================================================
    # DUNGEONS (7)
    # =========================================================================

    locations["goron_mines"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Goron Mines",
        entrance_ids=[0x27],
        ow_screen=0x36,
        dungeon_id="D6",
        boss="King Dodongo (King Helmasaur reskin)",
        dungeon_item="Hammer, Fire Shield",
        big_chest_room=0x88,
        miniboss=("Lanmolas", 0x78),
        entrance_room=0x98,
        boss_room=0xC8,
        floors={
            "F1": {
                "name": "Main Level",
                "grid": "3x3",
                "rooms": [0x77, 0x78, 0x79, 0x87, 0x88, 0x89, 0x97, 0x98, 0x99],
            },
            "B1": {
                "name": "Basement 1",
                "grid": "2x2 + side",
                "rooms": [0x69, 0xA8, 0xA9, 0xB8, 0xB9],
            },
            "B2": {
                "name": "Basement 2 + Boss",
                "grid": "linear",
                "rooms": [0xC8, 0xD7, 0xD8, 0xD9, 0xDA],
            },
        },
        all_rooms=[
            0x77, 0x78, 0x79, 0x87, 0x88, 0x89, 0x97, 0x98, 0x99,
            0x69, 0xA8, 0xA9, 0xB8, 0xB9,
            0xC8, 0xD7, 0xD8, 0xD9, 0xDA,
        ],
        room_names={
            0x77: "NW Hall", 0x78: "Lanmolas Miniboss", 0x79: "NE Hall",
            0x87: "West Hall", 0x88: "Big Chest", 0x89: "East Hall",
            0x97: "SW Hall", 0x98: "Entrance", 0x99: "SE Hall",
            0x69: "B1 Side", 0xA8: "B1 NW", 0xA9: "B1 NE",
            0xB8: "B1 SW", 0xB9: "B1 SE",
            0xC8: "Boss (King Dodongo)", 0xD7: "B2 West", 0xD8: "Pre-Boss",
            0xD9: "B2 Mid", 0xDA: "B2 East",
        },
    )

    locations["mushroom_grotto"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Mushroom Grotto",
        entrance_ids=[0x26],
        ow_screen=0x10,
        dungeon_id="D1",
        entrance_room=0x4A,  # ROM dungeon_id=0x0C
        boss="TBD",
        dungeon_item="TBD",
        notes="First dungeon, forest themed",
    )

    locations["tail_palace"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Tail Palace",
        entrance_ids=[0x15],
        ow_screen=0x2F,
        dungeon_id="D2",
        entrance_room=0x5F,  # ROM dungeon_id=0x0A
        boss="TBD",
        dungeon_item="TBD",
        notes="Second dungeon, toad/frog themed",
    )

    locations["kalyxo_castle"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Kalyxo Castle",
        entrance_ids=[0x28, 0x2B, 0x2A, 0x32],  # West, Main, Basement, Prison
        ow_screen=0x0B,
        dungeon_id="D3",
        entrance_room=0x56,  # ROM dungeon_id=0x10 (main entrance)
        boss="TBD",
        dungeon_item="TBD",
        notes="Third dungeon, castle with multiple entrances",
    )

    locations["zora_temple"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Zora Temple",
        entrance_ids=[0x25, 0x4E],  # Main, Waterfall
        ow_screen=0x1E,
        dungeon_id="D4",
        entrance_room=0x28,  # ROM dungeon_id=0x16
        boss="TBD",
        dungeon_item="TBD",
        notes="Fourth dungeon, water themed",
    )

    locations["glacia_estate"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Glacia Estate",
        entrance_ids=[0x34],
        ow_screen=0x06,
        dungeon_id="D5",
        entrance_room=0xDB,  # ROM dungeon_id=0x12
        boss="TBD",
        dungeon_item="TBD",
        notes="Fifth dungeon, ice/snow themed",
    )

    locations["dragon_ship"] = LocationConfig(
        type=LocationType.DUNGEON,
        name="Dragon Ship",
        entrance_ids=[0x35],
        ow_screen=0x30,
        dungeon_id="D7",
        entrance_room=0xD6,  # ROM dungeon_id=0x18
        boss="TBD",
        dungeon_item="TBD",
        notes="Seventh dungeon, naval/ship themed",
    )

    # =========================================================================
    # SHRINES (6)
    # =========================================================================

    locations["shrine_of_power"] = LocationConfig(
        type=LocationType.SHRINE,
        name="Shrine of Power",
        entrance_ids=[0x03, 0x05, 0x09, 0x0B],
        ow_screen=0x4B,
        reward="Pendant of Power",
        boss="Lanmolas",
    )

    locations["shrine_of_courage"] = LocationConfig(
        type=LocationType.SHRINE,
        name="Shrine of Courage",
        entrance_ids=[0x0C],
        ow_screen=0x50,
        reward="Pendant of Courage",
    )

    locations["shrine_of_wisdom"] = LocationConfig(
        type=LocationType.SHRINE,
        name="Shrine of Wisdom",
        entrance_ids=[0x33],
        ow_screen=0x63,
        reward="Pendant of Wisdom",
    )

    locations["shrine_of_origins"] = LocationConfig(
        type=LocationType.SHRINE,
        name="Shrine of Origins",
        reward="TBD",
    )

    locations["shrine_of_sky"] = LocationConfig(
        type=LocationType.SHRINE,
        name="Shrine of Sky",
        reward="TBD",
    )

    locations["shrine_of_sea"] = LocationConfig(
        type=LocationType.SHRINE,
        name="Shrine of Sea",
        reward="TBD",
    )

    # =========================================================================
    # CAVES - Snowpeak Region
    # =========================================================================

    locations["snow_mountain_cave_main"] = LocationConfig(
        type=LocationType.CAVE,
        name="Snow Mountain Cave",
        entrance_ids=[0x1E, 0x1F, 0x20],
        region="Snowpeak",
        ow_screens=[0x0D, 0x05],
        cave_type="passage",
        connects=["Mountain Start", "Portal", "Mountain End"],
    )

    locations["snow_mountain_cave_east_peak"] = LocationConfig(
        type=LocationType.CAVE,
        name="Snow Mountain Cave East Peak",
        entrance_ids=[0x16, 0x17],
        region="Snowpeak",
        ow_screens=[0x07, 0x05],
        cave_type="passage",
        connects=["East Peak", "To East Peak"],
    )

    locations["snow_mountain_peak_connector"] = LocationConfig(
        type=LocationType.CAVE,
        name="Snow Mountain Peak Connector",
        entrance_ids=[0x30, 0x31],
        region="Snowpeak",
        ow_screens=[0x07, 0x04],
        cave_type="passage",
        connects=["East Peak", "West Peak"],
    )

    # =========================================================================
    # CAVES - Beach Region
    # =========================================================================

    locations["beach_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Beach Cave",
        entrance_ids=[0x1A, 0x1B, 0x1C],
        region="Beach",
        ow_screens=[0x32, 0x33],
        cave_type="passage",
        connects=["Beach Route", "Beach End", "Beach Intro"],
    )

    # =========================================================================
    # CAVES - Kalyxo Field Region
    # =========================================================================

    locations["kalyxo_field_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Kalyxo Field Cave",
        entrance_ids=[0x21, 0x22, 0x23],
        region="Kalyxo Field",
        ow_screens=[0x25],
        cave_type="passage",
        connects=["Start", "River", "End"],
    )

    # =========================================================================
    # CAVES - Forest/Toadstool Region
    # =========================================================================

    locations["toadstool_log_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Toadstool Woods Log Cave",
        entrance_ids=[0x2C],
        region="Forest",
        ow_screens=[0x18],
        cave_type="passage",
        connects=["Log Entrance"],
    )

    locations["tail_palace_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Tail Palace Cave Route",
        entrance_ids=[0x2E, 0x2F],
        region="Forest",
        ow_screens=[0x2D, 0x2E],
        cave_type="passage",
        connects=["Route Start", "Route End"],
    )

    # =========================================================================
    # CAVES - Mountain Region
    # =========================================================================

    locations["mountain_witch_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Mountain to Witch Shop Cave",
        entrance_ids=[0x06, 0x07],
        region="Mountain",
        ow_screens=[0x15, 0x0D],
        cave_type="passage",
        connects=["Mountain Start", "Witch Shop End"],
    )

    locations["rock_heart_piece_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Rock Heart Piece Cave",
        entrance_ids=[0x51],
        region="Mountain",
        ow_screens=[0x15],
        cave_type="treasure",
        contents="Heart Piece",
    )

    # =========================================================================
    # CAVES - Graveyard Region
    # =========================================================================

    locations["hidden_grave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Hidden Grave",
        entrance_ids=[0x5B],
        region="Graveyard",
        ow_screens=[0x0F],
        cave_type="secret",
    )

    locations["graveyard_waterfall"] = LocationConfig(
        type=LocationType.CAVE,
        name="Graveyard Waterfall",
        entrance_ids=[0x5C],
        region="Graveyard",
        ow_screens=[0x0F],
        cave_type="special",
    )

    # =========================================================================
    # CAVES - Special
    # =========================================================================

    locations["half_magic_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="1/2 Magic Cave",
        entrance_ids=[0x11],
        region="Kalyxo",
        ow_screens=[0x0B],
        cave_type="upgrade",
        contents="Magic Upgrade",
    )

    locations["master_sword_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Master Sword Cave",
        entrance_ids=[0x18, 0x19, 0x2D],
        region="Forest",
        ow_screens=[0x40],
        cave_type="special",
        contents="Master Sword",
    )

    locations["deluxe_fairy_fountain"] = LocationConfig(
        type=LocationType.CAVE,
        name="Deluxe Fairy Fountain",
        entrance_ids=[0x13, 0x14, 0x3A, 0x3B],
        region="Mountain",
        ow_screens=[0x15, 0x1D],
        cave_type="fairy",
        contents="Great Fairy",
    )

    locations["lava_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Lava Cave",
        entrance_ids=[0x4F, 0x52],
        region="Lava Land",
        ow_screens=[0x43],
        cave_type="passage",
        connects=["Start", "End"],
    )

    locations["cave_of_secrets"] = LocationConfig(
        type=LocationType.CAVE,
        name="Cave of Secrets",
        entrance_ids=[0x50],
        region="Hall of Secrets",
        ow_screens=[0x0E],
        cave_type="special",
    )

    locations["healing_fairy_cave"] = LocationConfig(
        type=LocationType.CAVE,
        name="Healing Fairy Cave",
        entrance_ids=[0x38],
        region="Various",
        ow_screens=[0x11],
        cave_type="fairy",
        contents="Healing Fairy",
    )

    # =========================================================================
    # HOUSES
    # =========================================================================

    locations["links_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Link's House",
        entrance_ids=[0x01],
        ow_screen=0x32,
        notable="Starting location",
    )

    locations["mushroom_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Mushroom House",
        entrance_ids=[0x0D],
        ow_screen=0x18,
        notable="Forest village",
    )

    locations["old_woman_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Old Woman House",
        entrance_ids=[0x0E],
        ow_screen=0x18,
        notable="Forest village",
    )

    locations["ranch_shed"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Ranch Shed",
        entrance_ids=[0x3E],
        ow_screen=0x00,
        notable="Lon Lon Ranch",
    )

    locations["ocarina_girls_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Ocarina Girl's House",
        entrance_ids=[0x3F],
        ow_screen=0x00,
        notable="Ranch area",
    )

    locations["sick_boys_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Sick Boy's House",
        entrance_ids=[0x40],
        ow_screen=0x23,
        notable="Village",
    )

    locations["village_tavern"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Village Tavern",
        entrance_ids=[0x42],
        ow_screen=0x23,
        notable="Village",
    )

    locations["village_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Village House",
        entrance_ids=[0x44],
        ow_screen=0x23,
        notable="Village",
    )

    locations["zora_princess_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Zora Princess House",
        entrance_ids=[0x45],
        ow_screen=0x1E,
        notable="Zora domain",
    )

    locations["village_library"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Village Library",
        entrance_ids=[0x49],
        ow_screen=0x23,
        notable="Village",
    )

    locations["chicken_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Chicken House",
        entrance_ids=[0x4B],
        ow_screen=0x00,
        notable="Ranch area",
    )

    locations["mines_shed"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Mines Shed",
        entrance_ids=[0x5F],
        ow_screen=0x36,
        notable="Goron Mines area",
    )

    locations["west_hotel"] = LocationConfig(
        type=LocationType.HOUSE,
        name="West Hotel",
        entrance_ids=[0x60],
        ow_screen=0x0A,
        notable="Western region",
    )

    locations["village_mayors_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Village Mayor's House",
        entrance_ids=[0x61],
        ow_screen=0x23,
        notable="Village",
    )

    locations["smiths_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Smith's House",
        entrance_ids=[0x64],
        ow_screen=0x22,
        notable="Blacksmith",
    )

    locations["chest_minigame"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Chest Minigame",
        entrance_ids=[0x67],
        ow_screen=0x18,
        notable="Minigame",
    )

    locations["bonzai_house"] = LocationConfig(
        type=LocationType.HOUSE,
        name="Bonzai House",
        entrance_ids=[0x68],
        ow_screen=0x18,
        notable="Forest village",
    )

    # =========================================================================
    # SHOPS
    # =========================================================================

    locations["village_shop"] = LocationConfig(
        type=LocationType.SHOP,
        name="Village Shop",
        entrance_ids=[0x46],
        ow_screen=0x23,
        inventory="General items",
    )

    locations["witch_shop"] = LocationConfig(
        type=LocationType.SHOP,
        name="Witch Shop",
        entrance_ids=[0x4C],
        ow_screen=0x0D,
        inventory="Potions",
    )

    locations["happy_mask_shop"] = LocationConfig(
        type=LocationType.SHOP,
        name="Happy Mask Shop",
        entrance_ids=[0x6B],
        ow_screen=0x2D,
        inventory="Masks",
    )

    locations["fortune_teller"] = LocationConfig(
        type=LocationType.SHOP,
        name="Fortune Teller",
        entrance_ids=[0x65, 0x66],
        inventory="Fortunes",
    )

    # =========================================================================
    # SPECIAL AREAS
    # =========================================================================

    locations["hall_of_secrets"] = LocationConfig(
        type=LocationType.SPECIAL,
        name="Hall of Secrets",
        entrance_ids=[0x02, 0x12],
        ow_screen=0x0E,
        purpose="Farore's Sanctuary",
    )

    locations["fortress_of_secrets"] = LocationConfig(
        type=LocationType.SPECIAL,
        name="Fortress of Secrets",
        entrance_ids=[0x37],
        ow_screen=0x5E,
        purpose="Final dungeon",
    )

    locations["final_boss_route"] = LocationConfig(
        type=LocationType.SPECIAL,
        name="Final Boss Route",
        entrance_ids=[0x24],
        ow_screen=0x46,
        purpose="Endgame path",
    )

    locations["archery_minigame"] = LocationConfig(
        type=LocationType.SPECIAL,
        name="Archery Minigame",
        entrance_ids=[0x59],
        ow_screen=0x1A,
        purpose="Minigame",
    )

    return locations


# Build the Oracle of Secrets profile
ORACLE_PROFILE = RomProfile(
    name="Oracle of Secrets",
    short_name="oracle",
    description="Oracle of Secrets ALTTP ROM hack with custom dungeons and storyline",
    rom_title_pattern=r"ORACLE\s*OF\s*SECRETS",  # Match "ORACLE OF SECRETS" in ROM title
    locations=_build_oracle_locations(),
    docs_subdir="Docs/World",
)

# Register for auto-detection
register_profile(ORACLE_PROFILE)
