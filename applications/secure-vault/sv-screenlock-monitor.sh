#!/bin/bash
#
# SecureVault Screen Lock Integration
#
# Monitors for screen lock events and locks the vault automatically.
# Works with XFCE4 (xfce4-screensaver), xscreensaver, and systemd-logind.
#
# Installation:
#   1. Copy to /mnt/key/securevault/scripts/
#   2. Add to XFCE autostart or run manually
#
# Usage:
#   ./sv-screenlock-monitor.sh &
#

SV_BIN="/mnt/key/securevault/sv"
LOG_FILE="/tmp/sv-screenlock.log"

log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$LOG_FILE"
}

lock_vault() {
    log "Screen locked - locking vault"
    "$SV_BIN" lock 2>/dev/null
}

# Method 1: XFCE4 screensaver via dbus
monitor_xfce4() {
    log "Starting XFCE4 screensaver monitor"
    
    dbus-monitor --session "interface='org.xfce.ScreenSaver'" 2>/dev/null | \
    while read -r line; do
        if echo "$line" | grep -q "member=ActiveChanged"; then
            read -r next_line
            if echo "$next_line" | grep -q "boolean true"; then
                lock_vault
            fi
        fi
    done
}

# Method 2: systemd-logind (works with most DEs)
monitor_logind() {
    log "Starting systemd-logind monitor"
    
    dbus-monitor --system "interface='org.freedesktop.login1.Session'" 2>/dev/null | \
    while read -r line; do
        if echo "$line" | grep -q "member=Lock"; then
            lock_vault
        fi
    done
}

# Method 3: xscreensaver
monitor_xscreensaver() {
    log "Starting xscreensaver monitor"
    
    xscreensaver-command -watch 2>/dev/null | \
    while read -r line; do
        if echo "$line" | grep -q "^LOCK"; then
            lock_vault
        fi
    done
}

# Method 4: Generic X idle detection (fallback)
monitor_idle() {
    log "Starting idle monitor (5 min timeout)"
    
    IDLE_TIMEOUT=300000  # 5 minutes in milliseconds
    
    while true; do
        IDLE_TIME=$(xprintidle 2>/dev/null || echo 0)
        
        if [[ $IDLE_TIME -gt $IDLE_TIMEOUT ]]; then
            # Check if vault is unlocked before locking
            if "$SV_BIN" status 2>/dev/null | grep -q "unlocked=1"; then
                lock_vault
            fi
            # Wait for activity before checking again
            sleep 60
        fi
        
        sleep 10
    done
}

# Detect which monitor to use
main() {
    log "Screen lock monitor starting"
    
    # Check for XFCE4
    if pgrep -x "xfce4-session" > /dev/null; then
        monitor_xfce4 &
        log "Using XFCE4 monitor"
    fi
    
    # Check for xscreensaver
    if pgrep -x "xscreensaver" > /dev/null; then
        monitor_xscreensaver &
        log "Using xscreensaver monitor"
    fi
    
    # Always start logind monitor (most reliable)
    if systemctl is-active systemd-logind > /dev/null 2>&1; then
        monitor_logind &
        log "Using systemd-logind monitor"
    fi
    
    # Keep script running
    wait
}

# Handle termination
cleanup() {
    log "Screen lock monitor stopping"
    kill $(jobs -p) 2>/dev/null
    exit 0
}

trap cleanup SIGTERM SIGINT

main
