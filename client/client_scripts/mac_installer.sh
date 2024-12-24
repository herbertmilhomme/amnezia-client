#!/bin/bash

EXTRACT_DIR="$1"
INSTALLER_PATH="$2"

# Create and clean extract directory
rm -rf "$EXTRACT_DIR"
mkdir -p "$EXTRACT_DIR"

# Mount the DMG
hdiutil attach "$INSTALLER_PATH" -mountpoint "$EXTRACT_DIR/mounted_dmg" -nobrowse -quiet
if [ $? -ne 0 ]; then
    echo "Failed to mount DMG"
    exit 1
fi

# Copy the app to /Applications
cp -R "$EXTRACT_DIR/mounted_dmg/AmneziaVPN.app" /Applications/
if [ $? -ne 0 ]; then
    echo "Failed to copy AmneziaVPN.app to /Applications"
    hdiutil detach "$EXTRACT_DIR/mounted_dmg" -quiet
    exit 1
fi

# Unmount the DMG
hdiutil detach "$EXTRACT_DIR/mounted_dmg" -quiet
if [ $? -ne 0 ]; then
    echo "Failed to unmount DMG"
    exit 1
fi

# Optional: Remove the DMG file
rm "$INSTALLER_PATH"

echo "Installation completed successfully"
exit 0 