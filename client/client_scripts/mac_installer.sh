#!/bin/bash

EXTRACT_DIR="$1"
INSTALLER_PATH="$2"

# Create and clean extract directory
rm -rf "$EXTRACT_DIR"
mkdir -p "$EXTRACT_DIR"

# Mount the DMG
MOUNT_POINT="$EXTRACT_DIR/mounted_dmg"
hdiutil attach "$INSTALLER_PATH" -mountpoint "$MOUNT_POINT"
if [ $? -ne 0 ]; then
    echo "Failed to mount DMG"
    exit 1
fi

# Check if the application exists in the mounted DMG
if [ ! -d "$MOUNT_POINT/AmneziaVPN.app" ]; then
    echo "Error: AmneziaVPN.app not found in the mounted DMG."
    hdiutil detach "$MOUNT_POINT" #-quiet
    exit 1
fi

# Run the application
echo "Running AmneziaVPN.app from the mounted DMG..."
open "$MOUNT_POINT/AmneziaVPN.app"

# Get the PID of the app launched from the DMG
APP_PATH="$MOUNT_POINT/AmneziaVPN.app"
PID=$(pgrep -f "$APP_PATH")

if [ -z "$PID" ]; then
    echo "Failed to retrieve PID for AmneziaVPN.app"
    hdiutil detach "$MOUNT_POINT"
    exit 1
fi

# Wait for the specific PID to exit
echo "Waiting for AmneziaVPN.app to exit..."
while kill -0 "$PID" 2>/dev/null; do
    sleep 1
done

# Unmount the DMG
hdiutil detach "$EXTRACT_DIR/mounted_dmg"
if [ $? -ne 0 ]; then
    echo "Failed to unmount DMG"
    exit 1
fi

# Optional: Remove the DMG file
rm "$INSTALLER_PATH"

echo "Installation completed successfully"
exit 0 
