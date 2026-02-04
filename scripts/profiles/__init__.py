"""
ROM Profile System for Location Mapper

This module provides a profile-based architecture for managing location
data across different ALTTP ROM variants (vanilla, Oracle of Secrets, etc.).

Usage:
    from profiles import detect_rom_profile, VANILLA_PROFILE, ORACLE_PROFILE

    # Auto-detect ROM type
    profile = detect_rom_profile("/path/to/rom.sfc")

    # Or use a specific profile
    from profiles.vanilla_alttp import VANILLA_PROFILE
    from profiles.oracle_of_secrets import ORACLE_PROFILE
"""

from .base import RomProfile, LocationConfig, LocationType
from .detect import detect_rom_profile, REGISTERED_PROFILES
from .vanilla_alttp import VANILLA_PROFILE
from .oracle_of_secrets import ORACLE_PROFILE

__all__ = [
    "RomProfile",
    "LocationConfig",
    "LocationType",
    "detect_rom_profile",
    "REGISTERED_PROFILES",
    "VANILLA_PROFILE",
    "ORACLE_PROFILE",
]
