"""
ROM Detection Utilities

Auto-detect ROM type from the SNES ROM header and return the appropriate profile.
The ROM title is stored at offset 0x7FC0 in the SNES header (21 bytes).
"""

import re
import os
from typing import Optional, List

from .base import RomProfile

# Will be populated after profile modules are loaded
REGISTERED_PROFILES: List[RomProfile] = []


def _read_rom_title(rom_path: str) -> str:
    """
    Read the ROM title from the SNES header at offset 0x7FC0.

    The title is 21 bytes, padded with spaces.
    Returns the stripped title string.
    """
    try:
        with open(rom_path, 'rb') as f:
            # Check for header (512 bytes) by checking file size
            f.seek(0, 2)
            file_size = f.tell()

            # SMC header is 512 bytes and only present if (size % 1024) == 512
            has_header = (file_size % 1024) == 512
            offset = 0x7FC0 + (512 if has_header else 0)

            f.seek(offset)
            title_bytes = f.read(21)
            return title_bytes.decode('ascii', errors='ignore').strip()
    except Exception:
        return ""


def detect_rom_profile(rom_path: str) -> Optional[RomProfile]:
    """
    Auto-detect ROM type from title and return appropriate profile.

    Scans the ROM title at 0x7FC0 and matches against registered profiles.
    Returns None if no profile matches (caller should use default).

    Args:
        rom_path: Path to the SNES ROM file

    Returns:
        Matching RomProfile or None if no match found
    """
    if not os.path.exists(rom_path):
        return None

    title = _read_rom_title(rom_path)
    if not title:
        return None

    # Match against registered profiles
    for profile in REGISTERED_PROFILES:
        if re.match(profile.rom_title_pattern, title, re.IGNORECASE):
            return profile

    return None


def get_profile_by_name(name: str) -> Optional[RomProfile]:
    """
    Get a profile by its short name.

    Args:
        name: Short name like "vanilla" or "oracle"

    Returns:
        Matching RomProfile or None
    """
    name_lower = name.lower()
    for profile in REGISTERED_PROFILES:
        if profile.short_name.lower() == name_lower:
            return profile
    return None


def list_profiles() -> List[str]:
    """List all available profile short names."""
    return [p.short_name for p in REGISTERED_PROFILES]


def register_profile(profile: RomProfile) -> None:
    """Register a profile for auto-detection."""
    # Avoid duplicates
    for existing in REGISTERED_PROFILES:
        if existing.short_name == profile.short_name:
            return
    REGISTERED_PROFILES.append(profile)
