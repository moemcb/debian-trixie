# SecureVault

A secrets management system designed for encrypted boot setups. Stores credentials, API keys, SSH passphrases, and recovery codes securely on SD card with automatic environment variable export.

## Features

- **AES-256 encryption** (XChaCha20-Poly1305)
- **Argon2id key derivation** (256MB memory-hard)
- **Daemon architecture** - unlock once, use everywhere
- **Environment variable export** - API keys available in shell
- **GTK3 GUI** with system tray
- **CLI tool** for scripting and quick access
- **Categories** - organize secrets by type
- **Auto-lock** on screen lock or timeout
- **Clipboard auto-clear** (30 seconds)
- **SSH integration** - quick connect with stored credentials

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    SD CARD (/mnt/key)                       │
├─────────────────────────────────────────────────────────────┤
│  securevault/                                               │
│     ├── vault.enc       Encrypted database                  │
│     ├── sv              CLI binary                          │
│     ├── securevaultd    Daemon binary                       │
│     ├── securevault-gtk GUI binary                          │
│     └── sv-login.sh     Shell integration                   │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ Unix Socket IPC
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    DAEMON (securevaultd)                    │
├─────────────────────────────────────────────────────────────┤
│  • Holds decrypted secrets in locked memory                 │
│  • Serves requests via Unix socket                          │
│  • Auto-locks on timeout or screen lock                     │
│  • Exports environment variables on request                 │
└─────────────────────────────────────────────────────────────┘
         │                    │                    │
         ▼                    ▼                    ▼
    ┌─────────┐         ┌─────────┐         ┌─────────┐
    │   CLI   │         │   GUI   │         │ Scripts │
    │  (sv)   │         │  (gtk)  │         │ (bash)  │
    └─────────┘         └─────────┘         └─────────┘
```

## Installation

### Dependencies

```bash
# Debian/Ubuntu
sudo apt install build-essential libsodium-dev libgtk-3-dev xclip

# Arch
sudo pacman -S base-devel libsodium gtk3 xclip
```

### Build

```bash
git clone <repo> securevault
cd securevault
make
```

### Install to SD Card

```bash
# Install to default location (/mnt/key/securevault)
make install

# Or specify custom location
make install PREFIX=/media/moe/sdcard/securevault
```

### Shell Integration

Add to `~/.bashrc`:

```bash
# Load SecureVault
if [[ -f /mnt/key/securevault/sv-login.sh ]]; then
    source /mnt/key/securevault/sv-login.sh
fi
```

### Systemd Service (Optional)

```bash
make install-service
systemctl --user enable securevault
systemctl --user start securevault
```

## Usage

### Initial Setup

```bash
# Create new vault
sv init

# Start daemon
sv daemon

# Unlock vault
sv unlock
```

### Managing Entries

```bash
# Add new entry
sv add

# List all entries
sv list

# List by category
sv list api_keys
sv list servers

# Search entries
sv search cloudflare

# Copy password to clipboard
sv cp "Anthropic API"

# Copy specific field
sv cp "Anthropic API" username

# Show entry details
sv show "Anthropic API"

# Delete entry
sv rm 5
```

### SSH Integration

```bash
# Connect using stored credentials
sv ssh "DigitalOcean Master"
# Automatically uses SSH key if configured, or copies password
```

### Environment Variables

```bash
# Show configured env vars
sv env

# Export to current shell
eval $(sv env --export)

# Variables are automatically exported on login if vault is unlocked
echo $ANTHROPIC_API_KEY
```

### Password Generation

```bash
# Generate 24-char password
sv gen

# Generate 32-char password
sv gen 32
```

## Categories

| Category   | Use Case                         |
| ---------- | -------------------------------- |
| `api_keys` | API tokens (exports to env vars) |
| `servers`  | VPS credentials, SSH access      |
| `websites` | Web account logins               |
| `crypto`   | Exchange API keys, wallet seeds  |
| `recovery` | 2FA backup codes                 |
| `notes`    | Encrypted notes                  |
| `ssh`      | SSH key passphrases              |
| `database` | Database credentials             |
| `email`    | Email account access             |

## GUI Application

Launch with:

```bash
securevault-gtk
```

Features:

- System tray icon (lock/unlock status)
- Quick search
- Entry browser with categories
- Password generator
- Copy to clipboard (auto-clears)
- Add/edit/delete entries

## Security Model

| Layer          | Protection                     |
| -------------- | ------------------------------ |
| At rest        | XChaCha20-Poly1305             |
| Key derivation | Argon2id (256MB, 3 iterations) |
| In memory      | `mlock()` prevents swap        |
| IPC            | Unix socket + session token    |
| Clipboard      | Auto-clear after 30s           |
| Session        | Auto-lock after 5 min          |

## File Locations

| File                                  | Purpose         |
| ------------------------------------- | --------------- |
| `/mnt/key/securevault/vault.enc`      | Encrypted vault |
| `/run/user/<uid>/securevault.sock`    | Daemon socket   |
| `~/.config/securevault/session.token` | Session auth    |

## PAM Integration

For auto-unlock with login password, the daemon can be started via PAM. Your existing SD card key setup handles boot decryption; SecureVault adds application-level secret management on top.

## Backup

```bash
# The vault.enc file is the only file that needs backup
cp /mnt/key/securevault/vault.enc /backup/vault.enc.$(date +%Y%m%d)
```

## Troubleshooting

**Daemon not running:**

```bash
sv daemon
```

**Can't connect to daemon:**

```bash
# Check socket exists
ls -la /run/user/$(id -u)/securevault.sock

# Check daemon process
pgrep -a securevaultd
```

**Forgot master password:**
There is no recovery. This is by design. Keep a backup of your master password in a secure physical location.

## License

MIT
