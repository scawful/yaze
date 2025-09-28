#!/bin/bash
set -e

# Create macOS bundle script
# Usage: create-macos-bundle.sh <version> <artifact_name>

VERSION_NUM="$1"
ARTIFACT_NAME="$2"

if [ -z "$VERSION_NUM" ] || [ -z "$ARTIFACT_NAME" ]; then
    echo "Usage: $0 <version> <artifact_name>"
    exit 1
fi

echo "Creating macOS bundle for version: $VERSION_NUM"

# macOS packaging
if [ -d "build/bin/yaze.app" ]; then
    echo "Found macOS bundle, using it directly"
    cp -r build/bin/yaze.app ./Yaze.app
    # Add additional resources to the bundle
    cp -r assets "Yaze.app/Contents/Resources/" 2>/dev/null || echo "assets directory not found"
    # Update Info.plist with correct version
    if [ -f "cmake/yaze.plist.in" ]; then
        sed "s/@yaze_VERSION@/$VERSION_NUM/g" cmake/yaze.plist.in > "Yaze.app/Contents/Info.plist"
    fi
else
    echo "No bundle found, creating manual bundle"
    mkdir -p "Yaze.app/Contents/MacOS"
    mkdir -p "Yaze.app/Contents/Resources"
    cp build/bin/yaze "Yaze.app/Contents/MacOS/"
    cp -r assets "Yaze.app/Contents/Resources/" 2>/dev/null || echo "assets directory not found"
    # Create Info.plist with correct version
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
<key>CFBundleVersion</key>
<string>$VERSION_NUM</string>
<key>CFBundleShortVersionString</key>
<string>$VERSION_NUM</string>
<key>CFBundlePackageType</key>
<string>APPL</string>
</dict>
</plist>
EOF
fi

# Create DMG
mkdir dmg_staging
cp -r Yaze.app dmg_staging/
cp LICENSE dmg_staging/ 2>/dev/null || echo "LICENSE not found"
cp README.md dmg_staging/ 2>/dev/null || echo "README.md not found"
cp -r docs dmg_staging/ 2>/dev/null || echo "docs directory not found"
hdiutil create -srcfolder dmg_staging -format UDZO -volname "Yaze v$VERSION_NUM" "$ARTIFACT_NAME.dmg"

echo "macOS bundle creation completed successfully!"
