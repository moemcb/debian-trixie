#!/bin/bash
# Start gnome-keyring-daemon unlocked using LUKS-derived password
# Place in ~/.local/bin/ and reference from autostart

KEYFILE="/mnt/key/home.key"  # <-- CHANGE THIS to your keyfile path
COMPONENT="${1:-secrets}"

if [ -f "$KEYFILE" ]; then
    DERIVED_PASS=$(head -c 256 "$KEYFILE" | sha256sum | cut -d' ' -f1)
    # --unlock starts AND unlocks the daemon
    exec sh -c "echo -n '$DERIVED_PASS' | gnome-keyring-daemon --unlock --components=$COMPONENT"
else
    # Fallback - will prompt for password
    exec gnome-keyring-daemon --start --components="$COMPONENT"
fi
