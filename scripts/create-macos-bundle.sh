#!/bin/bash
set -e

# Create macOS bundle script
# Usage: create-macos-bundle.sh <version> <artifact_name>
#
# Creates a DMG with:
#   - Yaze.app (with assets in Resources/)
#   - z3ed (CLI tool)
#   - README.md
#   - LICENSE
#   - assets/ (for CLI tool access)

VERSION_NUM="$1"
ARTIFACT_NAME="$2"

if [ -z "$VERSION_NUM" ] || [ -z "$ARTIFACT_NAME" ]; then
    echo "Usage: $0 <version> <artifact_name>"
    exit 1
fi

echo "Creating macOS bundle for version: $VERSION_NUM"

# Clean up any previous artifacts
rm -rf Yaze.app dmg_staging

# Find the build directory (support both single-config and multi-config generators)
BUILD_DIR="build"
if [ -f "$BUILD_DIR/bin/Release/yaze" ]; then
    YAZE_BIN="$BUILD_DIR/bin/Release/yaze"
    Z3ED_BIN="$BUILD_DIR/bin/Release/z3ed"
elif [ -f "$BUILD_DIR/bin/yaze" ]; then
    YAZE_BIN="$BUILD_DIR/bin/yaze"
    Z3ED_BIN="$BUILD_DIR/bin/z3ed"
elif [ -d "$BUILD_DIR/bin/yaze.app" ]; then
    YAZE_BIN=""  # Will use bundle directly
    Z3ED_BIN="$BUILD_DIR/bin/z3ed"
else
    echo "ERROR: Cannot find yaze executable in $BUILD_DIR/bin/"
    ls -la "$BUILD_DIR/bin/" 2>/dev/null || echo "Directory doesn't exist"
    exit 1
fi

# macOS packaging
if [ -d "$BUILD_DIR/bin/yaze.app" ]; then
    echo "Found macOS bundle, using it directly"
    cp -r "$BUILD_DIR/bin/yaze.app" ./Yaze.app
else
    echo "Creating manual bundle from executable"
    mkdir -p "Yaze.app/Contents/MacOS"
    mkdir -p "Yaze.app/Contents/Resources"
    cp "$YAZE_BIN" "Yaze.app/Contents/MacOS/yaze"
fi

# Add assets to the bundle's Resources folder
if [ -d "assets" ]; then
    echo "Copying assets to bundle Resources..."
    cp -r assets "Yaze.app/Contents/Resources/"
else
    echo "WARNING: assets directory not found"
fi

# Add icon to bundle
if [ -f "assets/yaze.icns" ]; then
    cp assets/yaze.icns "Yaze.app/Contents/Resources/"
fi

# Create/Update Info.plist with correct version
cat > "Yaze.app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>yaze</string>
    <key>CFBundleIdentifier</key>
    <string>com.yaze.editor</string>
    <key>CFBundleName</key>
    <string>Yaze</string>
    <key>CFBundleDisplayName</key>
    <string>Yaze - Zelda3 Editor</string>
    <key>CFBundleVersion</key>
    <string>$VERSION_NUM</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION_NUM</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleIconFile</key>
    <string>yaze.icns</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
</dict>
</plist>
EOF

# Create DMG staging area with FLAT structure
echo "Creating DMG staging area..."
mkdir -p dmg_staging

# Copy the app bundle
cp -r Yaze.app dmg_staging/

# Copy z3ed CLI tool (if it exists)
if [ -f "$Z3ED_BIN" ]; then
    echo "Including z3ed CLI tool"
    cp "$Z3ED_BIN" dmg_staging/
elif [ -f "$BUILD_DIR/bin/Release/z3ed" ]; then
    echo "Including z3ed CLI tool (Release)"
    cp "$BUILD_DIR/bin/Release/z3ed" dmg_staging/
else
    echo "NOTE: z3ed not found, skipping (may not be built)"
fi

# Copy assets folder for CLI tool access
if [ -d "assets" ]; then
    cp -r assets dmg_staging/
fi

# Copy documentation
cp LICENSE dmg_staging/ 2>/dev/null || echo "LICENSE not found"
cp README.md dmg_staging/ 2>/dev/null || echo "README.md not found"

echo "=== DMG staging contents ==="
ls -la dmg_staging/

# Create DMG
echo "Creating DMG..."
hdiutil create -srcfolder dmg_staging -format UDZO -volname "Yaze $VERSION_NUM" "$ARTIFACT_NAME.dmg"

# Cleanup
rm -rf Yaze.app dmg_staging

echo "macOS bundle creation completed successfully!"
echo "Created: $ARTIFACT_NAME.dmg"
