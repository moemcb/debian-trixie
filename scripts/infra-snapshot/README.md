# infra-snapshot

A lightweight infrastructure snapshot tool that captures system state in JSON or Markdown format.

## Features

- **System Information**: Hostname, OS, kernel, uptime
- **Hardware**: CPU model/cores, RAM, swap, storage devices, disk usage
- **Networking**: Interfaces, routes, listening ports
- **Security**: Firewall detection (nftables/iptables), fail2ban status
- **Containers**: Docker container inventory
- **Automation**: Cron jobs, systemd timers
- **Services**: Running systemd services

## Requirements

**Required**: `jq`, `ip`, `lsblk`, `ss`

**Optional**: `docker`, `nft`/`iptables`, `fail2ban-client`

```bash
# Debian/Ubuntu
apt install jq iproute2 util-linux iproute2
```

## Installation

```bash
curl -o /usr/local/bin/infra-snapshot https://github.com/moemcb/debian-trixie/raw/branch/main/scripts/infra-snapshot/infra-snapshot
chmod +x /usr/local/bin/infra-snapshot
```

## Usage

```bash
# Generate JSON snapshot
infra-snapshot --json

# Generate Markdown report
infra-snapshot --md

# Generate both formats
infra-snapshot --both

# Custom output name
infra-snapshot --json -o my-server

# Convert existing JSON to Markdown
infra-snapshot --from-json server.json

# Validate JSON file
infra-snapshot --validate server.json

# Verbose output
infra-snapshot --json -v
```

## Output Examples

### JSON Structure

```json
{
  "meta": {
    "version": "2.0",
    "generated": "2025-12-28T15:00:00+00:00",
    "host": "server-name"
  },
  "machine": {
    "hostname": "server-name",
    "os": "Debian GNU/Linux 13 (trixie)",
    "kernel": "6.12.57+deb13-amd64",
    "uptime": "up 5 days, 3 hours"
  },
  "hardware": {
    "cpu": { "model": "...", "cores": 4, "architecture": "x86_64" },
    "memory": { "ram_total": "16Gi", "swap_total": "8Gi" },
    "storage": { "blockdevices": [...] },
    "disk_usage": [...]
  },
  "networking": {
    "interfaces": [...],
    "routes": [...],
    "listening": [...]
  },
  "security": {
    "firewall": "nftables",
    "fail2ban_installed": true
  },
  "containers": {
    "docker": [...]
  },
  "automation": {
    "cron_jobs": 5,
    "systemd_timers": 12
  },
  "services": {
    "running": [...]
  }
}
```

### Combining Multiple Servers

```bash
# Generate snapshots on each server
infra-snapshot --json -o server1
infra-snapshot --json -o server2

# Combine into single file
jq -s 'reduce .[] as $item ({}; . + {($item.meta.host): $item})' \
  server1.json server2.json > all-servers.json
```

## Options

| Option | Description |
|--------|-------------|
| `--json` | Output JSON format (default) |
| `--md` | Output Markdown format |
| `--both` | Output both JSON and Markdown |
| `--from-json FILE` | Convert JSON to Markdown |
| `--validate FILE` | Validate JSON structure |
| `-o, --output NAME` | Output filename (without extension) |
| `-v, --verbose` | Show detailed progress |
| `--version` | Show version |
| `-h, --help` | Show help |

## Automation

### Cron (Daily Snapshots)

```bash
# /etc/cron.d/infra-snapshot
0 2 * * * root /usr/local/bin/infra-snapshot --json -o /var/log/infra-snapshot-$(date +\%F)
```

### Systemd Timer

```ini
# /etc/systemd/system/infra-snapshot.service
[Unit]
Description=Infrastructure Snapshot

[Service]
Type=oneshot
ExecStart=/usr/local/bin/infra-snapshot --json -o /var/log/infra-snapshot

# /etc/systemd/system/infra-snapshot.timer
[Unit]
Description=Daily Infrastructure Snapshot

[Timer]
OnCalendar=daily
Persistent=true

[Install]
WantedBy=timers.target
```

## License

MIT
