#!/bin/bash

EXTRACT_DIR="$1"
INSTALLER_PATH="$2"

# Create and clean extract directory
rm -rf "$EXTRACT_DIR"
mkdir -p "$EXTRACT_DIR"

# Extract ZIP archive
unzip "$INSTALLER_PATH" -d "$EXTRACT_DIR"
if [ $? -ne 0 ]; then
    echo 'Failed to extract ZIP archive'
    exit 1
fi

# Find and extract TAR archive
TAR_FILE=$(find "$EXTRACT_DIR" -name '*.tar' -type f)
if [ -z "$TAR_FILE" ]; then
    echo 'TAR file not found'
    exit 1
fi

tar -xf "$TAR_FILE" -C "$EXTRACT_DIR"
if [ $? -ne 0 ]; then
    echo 'Failed to extract TAR archive'
    exit 1
fi

rm -f "$TAR_FILE"

# Find and run installer
INSTALLER=$(find "$EXTRACT_DIR" -type f -executable)
if [ -z "$INSTALLER" ]; then
    echo 'Installer not found'
    exit 1
fi

"$INSTALLER"
EXIT_CODE=$?

# Cleanup
rm -rf "$EXTRACT_DIR"
exit $EXIT_CODE 