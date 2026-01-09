# Debian 13 (Trixie) Server Hardening Scripts [![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Comprehensive security hardening scripts for Debian 13 Trixie servers.

Based on CIS Benchmarks and NIST SP 800-63B-4 (2025) guidelines.

## Warning

**Always ensure you have console/physical access before running these scripts.** 

SSH configuration changes can lock you out if misconfigured.


## Scripts

| Script                         | Architecture    | Use Case                             |
| ------------------------------ | --------------- | ------------------------------------ |
| `baseline-protection-arm64` | ARM64 (aarch64) | Raspberry Pi 4, ARM servers          |
| `baseline-protection-x86_64`   | x86_64          | Standard servers, VPS, media servers |
| `baseline-protection-ionos`   | VPS          | IONOS Debian Trixie VPS Image |

## Features

- **CIS Benchmark Compliant** - Based on CIS Debian Linux Benchmark
- **NIST 2025 Password Policy** - 15+ character minimum, no complexity rules, no expiration
- **Cloudflare Tunnel Safe** - Detects and preserves cloudflared/WARP DNS configuration
- **Strong SSH Hardening** - Key-only auth, modern ciphers, rate limiting
- **Defense in Depth** - UFW firewall, fail2ban, AppArmor, auditd
- **File Integrity** - AIDE monitoring, debsums verification
- **Automatic Updates** - Unattended security upgrades

## Requirements

- Debian 13 (Trixie)
- Root access
- SSH key already configured for the admin user
- Internet connection (for package installation)

## Usage

### ARM64 (Raspberry Pi)

```bash
sudo ./baseline-protection-arm64 [TUNNEL_IP] [USERNAME]

# Examples:
sudo ./baseline-protection-arm64 10.0.0.1 pi
```

### x86_64 (Standard Server)

```bash
sudo ./server-harden-basic-x64.sh [TUNNEL_IP] [USERNAME] [LOCAL_NETWORK]

# Examples:
sudo ./baseline-protection-x86_64 10.0.0.1 admin
sudo ./baseline-protection-x86_64 10.0.0.1 user 192.168.1.0/24
```

### Parameters

| Parameter       | Default                      | Description                             |
| --------------- | ---------------------------- | --------------------------------------- |
| `TUNNEL_IP`     | *optional*                   | Cloudflare tunnel IP (skip if not used) |
| `USERNAME`      | `$SUDO_USER` or `pi`/`admin` | Non-root user for SSH access            |
| `LOCAL_NETWORK` | `192.168.0.0/24`             | Local network CIDR (x64 only)           |

## What Gets Configured

### Security Packages Installed

- `ufw` - Firewall
- `fail2ban` - Intrusion prevention
- `apparmor` - Mandatory access control
- `auditd` - System auditing
- `aide` - File integrity monitoring
- `lynis` - Security auditing
- `rkhunter` - Rootkit detection
- `unattended-upgrades` - Automatic security updates

### SSH Hardening

- Listens on all interfaces (allows recovery access)
- Key-only authentication (passwords disabled)
- Root login disabled
- Modern ciphers only (ChaCha20, AES-GCM)
- Rate limiting (MaxAuthTries: 3, MaxSessions: 2)

### Firewall (UFW)

- Default deny incoming
- SSH allowed from:
  - Cloudflare tunnel network (10.0.0.0/8)
  - Local network (192.168.0.0/16)
- Additional ports configurable

### Kernel Hardening

- ASLR enabled
- IP forwarding disabled
- IPv6 disabled
- SYN cookies enabled
- Reverse path filtering
- ICMP redirects blocked
- Kernel pointer restriction

### DNS Security

- Cloudflare tunnel detection (preserves existing config)
- DNSSEC + DNS-over-TLS (if systemd-resolved available)
- Fallback to static DNS (1.1.1.1, 8.8.8.8)

## Architecture Differences

| Feature              | ARM64              | x64         |
| -------------------- | ------------------ | ----------- |
| Auditd buffer        | 4096               | 8192        |
| Process limit        | 256                | 512         |
| File limit           | 32768              | 65535       |
| AIDE scope           | Critical dirs only | Full system |
| Compiler restriction | Skipped            | Optional    |



## Post-Installation

```bash
# Verify SSH access (test before logging out!)
ssh -i ~/.ssh/key user@server

# Check firewall status
sudo ufw status verbose

# Check fail2ban
sudo fail2ban-client status sshd

# Run security audit
sudo lynis audit system

# Initialize AIDE database (takes time)
sudo aideinit

# Check security report
sudo /usr/local/bin/security-check.sh
```

## Backups

All original configuration files are backed up to:

```
/home/<user>/security-backup-YYYYMMDD-HHMMSS/
```

Includes:

- `sshd_config.bak`
- `sysctl.conf.bak`
- `login.defs.bak`
- `audit.rules.bak`
- `suid-sgid-files.txt`
- `hardening-report.txt`

## Security Standards

- [CIS Debian Linux Benchmark](https://www.cisecurity.org/benchmark/debian_linux)
- [NIST SP 800-63B-4](https://pages.nist.gov/800-63-4/) (Digital Identity Guidelines)
- [NIST SP 800-123](https://csrc.nist.gov/publications/detail/sp/800-123/final) (Server Security)
