"""
Base classes for the ROM Profile System.

This module defines the core data structures used by all ROM profiles:
- LocationType: Enum for categorizing locations
- LocationConfig: Configuration for a single location (dungeon, cave, etc.)
- RomProfile: Complete profile for a ROM variant
"""

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple
from enum import Enum


class LocationType(Enum):
    """Categories of indoor locations in ALTTP."""
    DUNGEON = "dungeon"
    SHRINE = "shrine"
    CAVE = "cave"
    HOUSE = "house"
    SHOP = "shop"
    SPECIAL = "special"


@dataclass
class LocationConfig:
    """
    Configuration for a single location (dungeon, cave, house, etc.).

    This is a flexible dataclass that can represent any indoor location type.
    Different location types use different fields - unused fields are left
    with default values.

    Attributes:
        type: The category of this location (dungeon, cave, house, etc.)
        name: Human-readable display name
        entrance_ids: List of entrance IDs that lead to this location
        all_rooms: List of all room IDs in this location
        room_names: Mapping of room_id -> human-readable name
    """
    type: LocationType
    name: str
    entrance_ids: List[int] = field(default_factory=list)
    all_rooms: List[int] = field(default_factory=list)
    room_names: Dict[int, str] = field(default_factory=dict)

    # Dungeon-specific fields
    dungeon_id: Optional[str] = None
    boss: Optional[str] = None
    dungeon_item: Optional[str] = None
    big_chest_room: Optional[int] = None
    miniboss: Optional[Tuple[str, int]] = None  # (name, room_id)
    entrance_room: Optional[int] = None
    boss_room: Optional[int] = None
    floors: Dict[str, dict] = field(default_factory=dict)

    # Shrine-specific fields
    reward: Optional[str] = None

    # Cave-specific fields
    cave_type: Optional[str] = None  # passage, treasure, fairy, secret, upgrade
    region: Optional[str] = None
    connects: List[str] = field(default_factory=list)
    contents: Optional[str] = None

    # Overworld location fields
    ow_screen: Optional[int] = None
    ow_screens: List[int] = field(default_factory=list)

    # House/shop-specific
    notable: Optional[str] = None
    inventory: Optional[str] = None

    # Special area fields
    purpose: Optional[str] = None

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        result = {
            "type": self.type.value,
            "name": self.name,
            "entrance_ids": self.entrance_ids,
            "all_rooms": self.all_rooms,
            "room_names": {f"0x{k:02X}": v for k, v in self.room_names.items()},
        }

        # Add optional fields if set
        optional_fields = [
            "dungeon_id", "boss", "dungeon_item", "big_chest_room",
            "entrance_room", "boss_room", "reward", "cave_type", "region",
            "connects", "contents", "ow_screen", "notable", "inventory", "purpose"
        ]

        for field_name in optional_fields:
            value = getattr(self, field_name)
            if value is not None:
                if isinstance(value, int) and field_name in ["big_chest_room", "entrance_room", "boss_room", "ow_screen"]:
                    result[field_name] = f"0x{value:02X}"
                else:
                    result[field_name] = value

        if self.ow_screens:
            result["ow_screens"] = [f"0x{s:02X}" for s in self.ow_screens]

        if self.miniboss:
            result["miniboss"] = {"name": self.miniboss[0], "room": f"0x{self.miniboss[1]:02X}"}

        if self.floors:
            result["floors"] = self.floors

        return result


@dataclass
class RomProfile:
    """
    Complete profile for an ALTTP ROM variant.

    A profile contains all location configuration data for a specific ROM,
    along with metadata about the ROM and addresses for data tables.

    Attributes:
        name: Full name (e.g., "Vanilla ALTTP (US)")
        short_name: CLI-friendly name (e.g., "vanilla", "oracle")
        description: Brief description of this ROM variant
        rom_title_pattern: Regex pattern to match the ROM title at 0x7FC0
        locations: Dictionary of location_key -> LocationConfig
    """
    name: str
    short_name: str
    description: str
    rom_title_pattern: str

    # Location data
    locations: Dict[str, LocationConfig] = field(default_factory=dict)

    # ROM addresses - can override vanilla defaults for hacks
    entrance_table_base: int = 0x14813     # Entrance -> Room mapping (kEntranceRoom)
    entrance_dungeon_base: int = 0x1548B   # Entrance -> Dungeon ID (kEntranceDungeon)
    entrance_position_x: int = 0x15063     # Entrance X position
    entrance_position_y: int = 0x14F59     # Entrance Y position
    entrance_camera_x: int = 0x14E4F       # Camera X scroll
    entrance_camera_y: int = 0x14D45       # Camera Y scroll

    # Dungeon room ranges (start/end rooms per dungeon)
    dungeons_start_rooms: int = 0x7939
    dungeons_end_rooms: int = 0x792D
    dungeons_boss_rooms: int = 0x10954

    # Output settings - relative to ROM directory or absolute
    docs_subdir: str = "Docs/World"

    def get_locations_by_type(self, loc_type: LocationType) -> Dict[str, LocationConfig]:
        """Get all locations of a specific type."""
        return {
            key: config for key, config in self.locations.items()
            if config.type == loc_type
        }

    def get_location(self, key: str) -> Optional[LocationConfig]:
        """Get a location by its key."""
        return self.locations.get(key)

    def list_location_keys(self, filter_type: Optional[LocationType] = None) -> List[str]:
        """List all location keys, optionally filtered by type."""
        if filter_type is None:
            return list(self.locations.keys())
        return [
            key for key, config in self.locations.items()
            if config.type == filter_type
        ]

    def to_dict(self) -> dict:
        """Convert profile to dictionary for JSON serialization."""
        return {
            "name": self.name,
            "short_name": self.short_name,
            "description": self.description,
            "rom_title_pattern": self.rom_title_pattern,
            "addresses": {
                "entrance_table_base": f"0x{self.entrance_table_base:05X}",
                "entrance_dungeon_base": f"0x{self.entrance_dungeon_base:05X}",
                "entrance_position_x": f"0x{self.entrance_position_x:05X}",
                "entrance_position_y": f"0x{self.entrance_position_y:05X}",
            },
            "docs_subdir": self.docs_subdir,
            "location_counts": {
                loc_type.value: len(self.get_locations_by_type(loc_type))
                for loc_type in LocationType
            },
        }
