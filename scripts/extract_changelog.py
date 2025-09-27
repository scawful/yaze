#!/usr/bin/env python3
"""
Extract changelog section for a specific version from docs/C1-changelog.md
Usage: python3 extract_changelog.py <version>
Example: python3 extract_changelog.py 0.3.0
"""

import re
import sys
import os

def extract_version_changelog(version_num, changelog_file):
    """Extract changelog section for specific version"""
    try:
        with open(changelog_file, 'r') as f:
            content = f.read()
        
        # Find the section for this version
        version_pattern = rf"## {re.escape(version_num)}\s*\([^)]+\)"
        next_version_pattern = r"## \d+\.\d+\.\d+\s*\([^)]+\)"
        
        # Find start of current version section
        version_match = re.search(version_pattern, content)
        if not version_match:
            return f"Changelog section not found for version {version_num}."
        
        start_pos = version_match.end()
        
        # Find start of next version section
        remaining_content = content[start_pos:]
        next_match = re.search(next_version_pattern, remaining_content)
        
        if next_match:
            end_pos = start_pos + next_match.start()
            section_content = content[start_pos:end_pos].strip()
        else:
            section_content = remaining_content.strip()
        
        return section_content
        
    except Exception as e:
        return f"Error reading changelog: {str(e)}"

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 extract_changelog.py <version>")
        sys.exit(1)
    
    version_num = sys.argv[1]
    changelog_file = "docs/C1-changelog.md"
    
    # Check if changelog file exists
    if not os.path.exists(changelog_file):
        print(f"Error: Changelog file {changelog_file} not found")
        sys.exit(1)
    
    # Extract changelog content
    changelog_content = extract_version_changelog(version_num, changelog_file)
    
    # Generate full release notes
    release_notes = f"""# Yaze v{version_num} Release Notes

{changelog_content}

## Download Instructions

### Windows
- Download `yaze-windows-x64.zip` for 64-bit Windows
- Download `yaze-windows-x86.zip` for 32-bit Windows  
- Extract and run `yaze.exe`

### macOS
- Download `yaze-macos.dmg`
- Mount the DMG and drag Yaze to Applications
- You may need to allow the app in System Preferences > Security & Privacy

### Linux
- Download `yaze-linux-x64.tar.gz`
- Extract: `tar -xzf yaze-linux-x64.tar.gz`
- Run: `./yaze`

## System Requirements
- **Windows**: Windows 10 or later (64-bit recommended)
- **macOS**: macOS 10.15 (Catalina) or later  
- **Linux**: Ubuntu 20.04 or equivalent, with X11 or Wayland

## Support
For issues and questions, please visit our [GitHub Issues](https://github.com/scawful/yaze/issues) page.
"""

    print(release_notes)

if __name__ == "__main__":
    main()
