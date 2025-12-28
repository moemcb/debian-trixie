# Automatic SMB/CIFS Mount with VPN Failover

> A comprehensive solution for automatically mounting Samba/CIFS shares with intelligent network detection and VPN failover support.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Troubleshooting](#troubleshooting)
- [Server Setup](#server-setup)
- [Advanced Configuration](#advanced-configuration)
- [Quick Reference Card](#quick-reference-card)
- [FAQ](#faq)
- [Security Considerations](#security-considerations)

---

## Overview

This solution provides a robust, production-ready system for automatically mounting Samba/CIFS shares with intelligent network detection. Perfect for home servers, remote work setups, and hybrid network environments where you need seamless access to network shares both locally and remotely.

### Features

✅ **Automatic mounting** of SMB/CIFS shares on login  
✅ **Smart network detection** - tries local network first, then VPN  
✅ **VPN failover** - automatically enables VPN when remote  
✅ **Clean shutdown** - prevents boot/shutdown delays  
✅ **Configurable shares** - easily add/remove shares  
✅ **Systemd integration** - reliable service management  
✅ **Comprehensive logging** - easy debugging and monitoring  
✅ **Support for multiple VPN types** - Cloudflare Warp, WireGuard, OpenVPN  

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Client System                                          │
│                                                         │
│  ┌──────────────────────┐                               │
│  │ smb-mount-manager    │ ← Main mount script           │
│  │ (smart failover)     │                               │
│  └──────────┬───────────┘                               │
│             │                                           │
│  ┌──────────▼───────────┐    ┌─────────────────────┐    │
│  │ Try Primary Server   │───▶│ Local Network       │    │
│  │ (e.g. 192.168.x.x)   │    │ Direct Connection   │    │
│  └──────────┬───────────┘    └─────────────────────┘    │
│             │ (if fails)                                │
│  ┌──────────▼───────────┐    ┌─────────────────────┐    │
│  │ Try Secondary Server │───▶│ VPN Network         │    │
│  │ (e.g. 10.0.0.x)      │    │ (Warp/WireGuard)    │    │
│  └──────────────────────┘    └─────────────────────┘    │
│                                                         │
│  ┌────────────────────────────────────────────────┐     │
│  │ Systemd Services:                              │     │
│  │ • User service: mounts on login                │     │
│  │ • Timer: checks every 5 min                    │     │
│  │ • System service: clean shutdown               │     │
│  └────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────┘
```

---

## Prerequisites

### Server Side

- Samba server configured and running
- Shares configured in `/etc/samba/smb.conf`
- User credentials set up (`smbpasswd -a username`)
- Firewall allows SMB ports (445, 139)

### Client Side

- Linux system with systemd
- `cifs-utils` package installed
- VPN client (if using failover) - e.g., `warp-cli`, WireGuard, OpenVPN
- `root` or `sudo` access for initial setup

---

## Installation

### Step 1: Install Dependencies

```bash
sudo apt update
sudo apt install cifs-utils
```

For Cloudflare Warp (optional):

```bash
# Follow Cloudflare's official installation guide
https://developers.cloudflare.com/warp-client/get-started/linux/
```

### Step 2: Create Credentials File

```bash
sudo nano /etc/samba/creds
```

Add your credentials:

```
username=YOUR_USERNAME
password=YOUR_PASSWORD
```

Secure the file:

```bash
sudo chmod 600 /etc/samba/creds
sudo chown root:root /etc/samba/creds
```

### Step 3: Create Mount Manager Script

Create `/usr/local/bin/smb-mount-manager`:

```bash
sudo nano /usr/local/bin/smb-mount-manager
```

Paste the following script (customise the configuration section):

```bash
#!/bin/bash

# SMB Mount Manager with VPN Failover
# Automatically mounts Samba shares from home server
# Uses VPN when not on local network

set -uo pipefail

# ═══════════════════════════════════════════════════════
# CONFIGURATION - CUSTOMIZE THIS SECTION
# ═══════════════════════════════════════════════════════

# Server addresses
SERVER_PRIMARY="192.168.1.100"    # Local network IP
SERVER_SECONDARY="10.0.0.1"       # VPN network IP (or same as primary)
SERVER=""                         # Will be determined dynamically

# Credentials and mount settings
CREDENTIALS="/etc/samba/creds"
MOUNT_BASE="/home/$USER"
SMB_PORT=445

# Timeouts
WARP_TIMEOUT=15
CONNECTION_TIMEOUT=2
MAX_RETRIES=3

# VPN command (customize for your VPN)
# Examples:
#   Cloudflare Warp: "warp-cli connect"
#   WireGuard: "wg-quick up wg0"
#   OpenVPN: "sudo systemctl start openvpn@client"
VPN_CONNECT_CMD="warp-cli connect"
VPN_STATUS_CMD="warp-cli status"
VPN_STATUS_CHECK="Connected"  # String to check in status output

# Share definitions: "ShareName:MountPoint:AlwaysMount"
# AlwaysMount=1 means mount automatically (even without args)
# AlwaysMount=0 means mount only when explicitly requested
SHARES=(
    "Documents:${MOUNT_BASE}/Documents:1"
    "Downloads:${MOUNT_BASE}/Downloads:0"
)

# ═══════════════════════════════════════════════════════
# END CONFIGURATION
# ═══════════════════════════════════════════════════════

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() {
    echo -e "${GREEN}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $*"
}

error() {
    echo -e "${RED}[$(date '+%Y-%m-%d %H:%M:%S')] ERROR:${NC} $*" >&2
}

warn() {
    echo -e "${YELLOW}[$(date '+%Y-%m-%d %H:%M:%S')] WARNING:${NC} $*"
}

# Test SMB connectivity to a server
test_smb_connection() {
    local host=$1
    local timeout=${2:-$CONNECTION_TIMEOUT}

    if timeout "$timeout" bash -c "echo >/dev/tcp/$host/$SMB_PORT" 2>/dev/null; then
        return 0
    fi
    return 1
}

# Check if VPN is connected
is_vpn_connected() {
    $VPN_STATUS_CMD 2>/dev/null | grep -q "$VPN_STATUS_CHECK"
}

# Enable VPN
enable_vpn() {
    log "Enabling VPN..."

    if is_vpn_connected; then
        log "VPN already connected"
        return 0
    fi

    $VPN_CONNECT_CMD &>/dev/null

    local count=0
    while [ $count -lt $WARP_TIMEOUT ]; do
        if is_vpn_connected; then
            log "VPN connected successfully"
            sleep 2
            return 0
        fi
        sleep 1
        ((count++))
    done

    error "Failed to connect VPN within ${WARP_TIMEOUT}s"
    return 1
}

# Determine which server to use
select_server() {
    log "Determining best server to use..."

    # Try primary server
    if test_smb_connection "$SERVER_PRIMARY"; then
        SERVER="$SERVER_PRIMARY"
        log "Using primary server: $SERVER_PRIMARY (local network)"
        return 0
    fi

    log "Primary server not available, trying secondary..."

    # Try secondary server directly
    if test_smb_connection "$SERVER_SECONDARY"; then
        SERVER="$SERVER_SECONDARY"
        log "Using secondary server: $SERVER_SECONDARY"
        return 0
    fi

    # Try enabling VPN and retry
    warn "Secondary server not reachable, attempting VPN connection..."
    if ! enable_vpn; then
        error "Failed to enable VPN"
        return 1
    fi

    sleep 2

    if test_smb_connection "$SERVER_SECONDARY" 5; then
        SERVER="$SERVER_SECONDARY"
        log "Using secondary server via VPN: $SERVER_SECONDARY"
        return 0
    fi

    error "No server reachable via any method"
    return 1
}

# Ensure mount point exists
ensure_mount_point() {
    local mount_point=$1
    if [ ! -d "$mount_point" ]; then
        mkdir -p "$mount_point"
        log "Created mount point: $mount_point"
    fi
}

# Check if share is already mounted
is_mounted() {
    local mount_point=$1
    mount | grep -q " $mount_point "
}

# Mount a single share
mount_share() {
    local share_name=$1
    local mount_point=$2

    ensure_mount_point "$mount_point"

    if is_mounted "$mount_point"; then
        warn "$share_name already mounted at $mount_point"
        return 0
    fi

    log "Mounting $share_name to $mount_point..."

    local retry=0
    while [ $retry -lt $MAX_RETRIES ]; do
        if sudo mount -t cifs "//${SERVER}/${share_name}" "$mount_point" \
            -o "credentials=${CREDENTIALS},iocharset=utf8,uid=$(id -u),gid=$(id -g),_netdev,vers=3.1.1,seal"; then
            log "Successfully mounted $share_name"
            return 0
        fi

        ((retry++))
        if [ $retry -lt $MAX_RETRIES ]; then
            warn "Mount failed, retrying ($retry/$MAX_RETRIES)..."
            sleep 2
        fi
    done

    error "Failed to mount $share_name after $MAX_RETRIES attempts"
    return 1
}

# Unmount a single share
unmount_share() {
    local mount_point=$1
    local share_name=$2

    if ! is_mounted "$mount_point"; then
        warn "$share_name not mounted at $mount_point"
        return 0
    fi

    log "Unmounting $share_name from $mount_point..."
    if sudo umount "$mount_point"; then
        log "Successfully unmounted $share_name"
        return 0
    else
        error "Failed to unmount $share_name"
        return 1
    fi
}

# Mount all shares or specific ones
mount_shares() {
    local specific_shares=("$@")
    local mounted_count=0
    local failed_count=0

    if ! select_server; then
        error "No server available for mounting"
        return 1
    fi

    for share_def in "${SHARES[@]}"; do
        IFS=':' read -r share_name mount_point always_mount <<< "$share_def"

        local should_mount=false

        if [ ${#specific_shares[@]} -eq 0 ]; then
            [ "$always_mount" = "1" ] && should_mount=true
        else
            for requested in "${specific_shares[@]}"; do
                if [ "$requested" = "$share_name" ]; then
                    should_mount=true
                    break
                fi
            done
        fi

        if [ "$should_mount" = true ]; then
            if mount_share "$share_name" "$mount_point"; then
                ((mounted_count++))
            else
                ((failed_count++))
            fi
        fi
    done

    log "Mount summary: $mounted_count succeeded, $failed_count failed"
    [ $failed_count -eq 0 ]
}

# Unmount all shares
unmount_all() {
    log "Unmounting all shares..."
    local unmounted_count=0

    for share_def in "${SHARES[@]}"; do
        IFS=':' read -r share_name mount_point _ <<< "$share_def"
        if unmount_share "$mount_point" "$share_name"; then
            ((unmounted_count++))
        fi
    done

    log "Unmounted $unmounted_count shares"
}

# Show status
show_status() {
    echo "════════════════════════════════════════"
    echo "  SMB Mount Manager - Status"
    echo "════════════════════════════════════════"
    echo

    echo "Server Status:"
    echo "  Primary ($SERVER_PRIMARY): $(test_smb_connection "$SERVER_PRIMARY" && echo "✓ Available" || echo "✗ Unavailable")"
    echo "  Secondary ($SERVER_SECONDARY): $(test_smb_connection "$SERVER_SECONDARY" && echo "✓ Available" || echo "✗ Unavailable")"
    echo

    if is_vpn_connected; then
        echo "VPN Status: ✓ Connected"
    else
        echo "VPN Status: ✗ Disconnected"
    fi
    echo

    echo "Mounted Shares:"
    local has_mounts=false
    for share_def in "${SHARES[@]}"; do
        IFS=':' read -r share_name mount_point always_mount <<< "$share_def"
        if is_mounted "$mount_point"; then
            echo "  ✓ $share_name → $mount_point"
            has_mounts=true
        else
            echo "  ✗ $share_name (not mounted)"
        fi
    done

    if [ "$has_mounts" = false ]; then
        echo "  (none)"
    fi
    echo
}

# Main command handler
case "${1:-}" in
    mount)
        shift
        mount_shares "$@"
        ;;
    unmount)
        unmount_all
        ;;
    status)
        show_status
        ;;
    help|--help|-h)
        cat << EOF
SMB Mount Manager - Automatic Samba share mounting with VPN failover

USAGE:
    smb-mount-manager <command> [arguments]

COMMANDS:
    mount [ShareName...]    Mount auto-mount shares or specific shares
    unmount                 Unmount all shares
    status                  Show server availability and mount status
    help                    Show this help message

EXAMPLES:
    smb-mount-manager mount              # Mount all auto-mount shares
    smb-mount-manager mount Documents    # Mount specific share
    smb-mount-manager unmount            # Unmount all
    smb-mount-manager status             # Check status

CONFIGURATION:
    Edit /usr/local/bin/smb-mount-manager to customize:
    - SERVER_PRIMARY / SERVER_SECONDARY
    - SHARES array
    - VPN commands
    - Mount options

LOGS:
    journalctl --user -u smb-auto-mount.service -f

EOF
        ;;
    *)
        error "Unknown command: ${1:-}"
        echo "Use 'smb-mount-manager help' for usage information"
        exit 1
        ;;
esac
```

Make it executable:

```bash
sudo chmod +x /usr/local/bin/smb-mount-manager
```

### Step 4: Create Systemd Services

#### User Service (Auto-mount on Login)

Create `~/.config/systemd/user/smb-auto-mount.service`:

```bash
mkdir -p ~/.config/systemd/user
nano ~/.config/systemd/user/smb-auto-mount.service
```

```ini
[Unit]
Description=Auto-mount SMB shares
After=network-online.target
Wants=network-online.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/smb-mount-manager mount
RemainAfterExit=yes
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=default.target
```

#### User Timer (Periodic Check)

Create `~/.config/systemd/user/smb-auto-mount.timer`:

```bash
nano ~/.config/systemd/user/smb-auto-mount.timer
```

```ini
[Unit]
Description=Check SMB mounts every 5 minutes
Requires=smb-auto-mount.service

[Timer]
OnBootSec=2min
OnUnitActiveSec=5min
Unit=smb-auto-mount.service

[Install]
WantedBy=timers.target
```

#### System Service (Clean Shutdown)

Create `/etc/systemd/system/smb-unmount-shutdown.service`:

```bash
sudo nano /etc/systemd/system/smb-unmount-shutdown.service
```

```ini
[Unit]
Description=Unmount SMB shares before shutdown
DefaultDependencies=no
Before=shutdown.target reboot.target halt.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/smb-mount-manager unmount
TimeoutStartSec=10s

[Install]
WantedBy=shutdown.target reboot.target halt.target
```

### Step 5: Enable Services

```bash
# Reload systemd
systemctl --user daemon-reload
sudo systemctl daemon-reload

# Enable and start user services
systemctl --user enable --now smb-auto-mount.service
systemctl --user enable --now smb-auto-mount.timer

# Enable system service
sudo systemctl enable smb-unmount-shutdown.service

# Verify services
systemctl --user status smb-auto-mount.service
systemctl --user list-timers
```

---

## Configuration

### VPN Configuration Examples

#### Cloudflare Warp

```bash
VPN_CONNECT_CMD="warp-cli connect"
VPN_STATUS_CMD="warp-cli status"
VPN_STATUS_CHECK="Connected"
```

#### WireGuard

```bash
VPN_CONNECT_CMD="wg-quick up wg0"
VPN_STATUS_CMD="wg show wg0"
VPN_STATUS_CHECK="interface: wg0"
```

#### OpenVPN

```bash
VPN_CONNECT_CMD="sudo systemctl start openvpn@client"
VPN_STATUS_CMD="systemctl is-active openvpn@client"
VPN_STATUS_CHECK="active"
```

#### No VPN (Same Network)

```bash
SERVER_PRIMARY="192.168.1.100"
SERVER_SECONDARY="192.168.1.100"  # Same as primary
```

### Share Configuration Examples

```bash
SHARES=(
    "work:/home/$USER/Work:1"           # Auto-mount
    "media:/mnt/media:0"                # Manual only
    "backup:/mnt/backup:0"              # Manual only
    "shared:/home/$USER/Shared:1"       # Auto-mount
)
```

---

## Usage

### Common Commands

| Command                                           | Description                               |
| ------------------------------------------------- | ----------------------------------------- |
| `smb-mount-manager status`                        | Show mount status and server availability |
| `smb-mount-manager mount`                         | Mount auto-mount shares                   |
| `smb-mount-manager mount ShareName`               | Mount specific share                      |
| `smb-mount-manager unmount`                       | Unmount all shares                        |
| `systemctl --user restart smb-auto-mount.service` | Restart mount service                     |
| `journalctl --user -u smb-auto-mount.service -f`  | View live logs                            |

### View Logs

```bash
# User service logs
journalctl --user -u smb-auto-mount.service -f

# System service logs
sudo journalctl -u smb-unmount-shutdown.service

# All mount activity
journalctl -f | grep -i "mount\|cifs"
```

### Service Management

```bash
# Restart user service
systemctl --user restart smb-auto-mount.service

# Disable auto-mounting
systemctl --user stop smb-auto-mount.timer
systemctl --user disable smb-auto-mount.timer

# Re-enable auto-mounting
systemctl --user enable --now smb-auto-mount.timer
```

---

## Troubleshooting

### Troubleshooting Flowchart

```
Mount Fails?
  │
  ├─→ Test connectivity: ping SERVER_IP
  │   Failed? → Check network/VPN
  │   Success? ↓
  │
  ├─→ Test SMB port: nc -zv SERVER_IP 445
  │   Failed? → Check firewall/Samba running
  │   Success? ↓
  │
  ├─→ Test credentials: smbclient -L //SERVER_IP -U user
  │   Failed? → Check /etc/samba/creds
  │   Success? ↓
  │
  └─→ Check logs: journalctl --user -u smb-auto-mount.service
```

### Common Issues & Fixes

| Issue                    | Fix                                            |
| ------------------------ | ---------------------------------------------- |
| Permission denied        | Check credentials file and server permissions  |
| Connection timeout       | Check firewall, verify server is running       |
| Double mounting          | Remove pam_mount entries, check fstab          |
| Slow shutdown            | Verify smb-unmount-shutdown.service is enabled |
| VPN not connecting       | Check VPN service, verify VPN_*_CMD variables  |
| Shares not auto-mounting | Check AlwaysMount flag in SHARES array         |

### Shares Not Mounting

1. **Check server connectivity:**
   
   ```bash
   ping SERVER_IP
   nc -zv SERVER_IP 445
   ```

2. **Test credentials:**
   
   ```bash
   smbclient -L //SERVER_IP -U username
   ```

3. **Check permissions:**
   
   ```bash
   ls -l /etc/samba/creds
   # Should be: -rw------- 1 root root
   ```

4. **View detailed errors:**
   
   ```bash
   sudo mount -t cifs //SERVER_IP/ShareName /mnt/test \
     -o credentials=/etc/samba/creds,vers=3.1.1
   ```

### VPN Not Connecting

1. **Test VPN manually:**
   
   ```bash
   # For Warp
   warp-cli connect
   warp-cli status
   
   # For WireGuard
   wg-quick up wg0
   wg show
   ```

2. **Check VPN configuration in script:**
   
   - Verify `VPN_CONNECT_CMD` is correct
   - Verify `VPN_STATUS_CMD` and `VPN_STATUS_CHECK` match your VPN

### Emergency Commands

```bash
# Force unmount everything
sudo umount -a -t cifs -l

# Kill all CIFS-related processes
sudo killall mount.cifs

# Check what's using a mount point
sudo lsof +D /mount/point

# Restart all services
systemctl --user restart smb-auto-mount.service
sudo systemctl restart smb-unmount-shutdown.service
```

---

## Server Setup

### Basic Samba Setup

On the server:

```bash
# Install Samba
sudo apt update
sudo apt install samba

# Create share directory
sudo mkdir -p /srv/samba/ShareName
sudo chown username:username /srv/samba/ShareName

# Add to /etc/samba/smb.conf
[ShareName]
   path = /srv/samba/ShareName
   writable = yes
   valid users = username

# Add user and restart
sudo smbpasswd -a username
sudo systemctl restart smbd
```

### Example Share Configuration

```ini
[Documents]
   path = /srv/samba/documents
   browseable = yes
   writable = yes
   valid users = username
   create mask = 0664
   directory mask = 0775
```

### Firewall Configuration

```bash
# UFW
sudo ufw allow samba

# Or manually
sudo ufw allow 445/tcp
sudo ufw allow 139/tcp
```

### Security Hardening

In `/etc/samba/smb.conf`:

```ini
[global]
   # Require SMB3 (more secure)
   min protocol = SMB3_00

   # Require encryption
   smb encrypt = required
   server signing = mandatory

   # Limit to specific networks
   interfaces = lo eth0 192.168.1.0/24
   bind interfaces only = yes

   # Security
   ntlm auth = ntlmv2-only
   lanman auth = no
```

---

## Advanced Configuration

### Mount Options Reference

| Option         | Description                       |
| -------------- | --------------------------------- |
| `vers=3.1.1`   | SMB protocol version (3.0, 3.1.1) |
| `seal`         | Encryption required               |
| `uid=$(id -u)` | Mount with user ownership         |
| `gid=$(id -g)` | Mount with group ownership        |
| `_netdev`      | Wait for network before mounting  |
| `nobrl`        | Disable byte-range locks          |
| `cache=loose`  | More caching (better performance) |
| `cache=strict` | Less caching (better safety)      |

### Custom Mount Options

Modify mount command in the script:

```bash
sudo mount -t cifs "//${SERVER}/${share_name}" "$mount_point" \
  -o "credentials=${CREDENTIALS},iocharset=utf8,uid=$(id -u),\
      gid=$(id -g),_netdev,vers=3.1.1,seal,nobrl,cache=loose"
```

Options explained:

- `vers=3.1.1` - SMB protocol version
- `seal` - Encryption
- `nobrl` - Disable byte-range locks (better for some apps)
- `cache=loose` - Better performance (use with caution)

### Performance Tuning

```bash
# For better performance (read-heavy)
-o cache=loose,nobrl

# For better reliability (write-heavy)
-o cache=strict

# For maximum security
-o vers=3.1.1,seal,sec=krb5

# For compatibility
-o vers=3.0,sec=ntlmssp
```

### Notification on Mount

Add to the script:

```bash
# After successful mount
notify-send "SMB Mount" "$share_name mounted successfully"
```

Install required package:

```bash
sudo apt install libnotify-bin
```

---

## Quick Reference Card

### Configuration Checklist

- [ ] `/etc/samba/creds` exists with correct permissions (600)
- [ ] `SERVER_PRIMARY` set to local network IP
- [ ] `SERVER_SECONDARY` set to VPN IP (or same as primary)
- [ ] `SHARES` array configured with your shares
- [ ] VPN commands configured (if using VPN failover)
- [ ] User systemd services created and enabled
- [ ] System shutdown service created and enabled
- [ ] No conflicting pam_mount or fstab entries

### File Locations

| File                                               | Purpose                       |
| -------------------------------------------------- | ----------------------------- |
| `/usr/local/bin/smb-mount-manager`                 | Main script                   |
| `/etc/samba/creds`                                 | Credentials (600 permissions) |
| `~/.config/systemd/user/smb-auto-mount.service`    | User mount service            |
| `~/.config/systemd/user/smb-auto-mount.timer`      | User timer                    |
| `/etc/systemd/system/smb-unmount-shutdown.service` | System shutdown service       |

### Service Status Checks

```bash
# User services
systemctl --user status smb-auto-mount.service
systemctl --user status smb-auto-mount.timer
systemctl --user list-timers

# System service
sudo systemctl status smb-unmount-shutdown.service

# View all mounts
mount | grep cifs

# Test mount manually
sudo mount -t cifs //SERVER/Share /mnt/test \
  -o credentials=/etc/samba/creds,vers=3.1.1
```

### Testing Checklist

- [ ] Shares mount on login
- [ ] Shares mount after 5 minutes if disconnected
- [ ] Correct server is selected (primary vs secondary)
- [ ] VPN connects when remote
- [ ] Shutdown/reboot is fast (no delays)
- [ ] No duplicate mounts
- [ ] Shares accessible and writable
- [ ] Logs show no errors

---

## FAQ

**Q: Can I use this with multiple users?**  
A: Yes, install the user service for each user, or create a system-wide service in `/etc/systemd/system/`.

**Q: Does this work with NFS?**  
A: The script is CIFS-specific, but you can adapt it for NFS by changing mount commands.

**Q: Can I mount to system directories like `/media`?**  
A: Yes, but use system services instead of user services, and adjust paths accordingly.

**Q: How do I debug mount failures?**  
A: Use `journalctl --user -u smb-auto-mount.service -f` and test mount commands manually.

**Q: Can I use this with Active Directory?**  
A: Yes, add `sec=krb5` to mount options and configure Kerberos authentication.

**Q: What if I don't use VPN?**  
A: Set both `SERVER_PRIMARY` and `SERVER_SECONDARY` to the same IP address.

---

## Security Considerations

1. **Credentials File**: Always store at `/etc/samba/creds` with `600` permissions
2. **Use Encryption**: Always use `seal` option for SMB encryption
3. **Network Isolation**: Consider using VPN for all remote access
4. **Firewall**: Only allow SMB from trusted networks
5. **Regular Updates**: Keep Samba and client tools updated
6. **Protocol Version**: Use SMB 3.0 or higher for better security
7. **Authentication**: Consider Kerberos for enterprise environments

---

## Performance Tips

1. **Use SMB3**: Faster and more secure than SMB2
2. **Adjust Cache**: Use `cache=loose` for read-heavy workloads
3. **Increase MTU**: Match MTU between client and server
4. **Use Wired Connection**: Better performance than WiFi
5. **Disable Oplocks**: If experiencing corruption with multi-client access

---

## Uninstallation

```bash
# Disable services
systemctl --user disable --now smb-auto-mount.service
systemctl --user disable --now smb-auto-mount.timer
sudo systemctl disable --now smb-unmount-shutdown.service

# Remove files
rm ~/.config/systemd/user/smb-auto-mount.{service,timer}
sudo rm /etc/systemd/system/smb-unmount-shutdown.service
sudo rm /usr/local/bin/smb-mount-manager
sudo rm /etc/samba/creds

# Reload systemd
systemctl --user daemon-reload
sudo systemctl daemon-reload
```

---

## Contributing

Found an improvement or bug? Contributions are welcome!

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

---

## License

This project is provided as-is for educational purposes. Use at your own risk.

**MIT License** - Feel free to modify and distribute.

---

## Credits

Developed as a solution for automatic home server mounting with remote access failover.

**Version**: 1.0  
**Last Updated**: November 2025  
**Tested On**: Debian 13 (trixie)

---
