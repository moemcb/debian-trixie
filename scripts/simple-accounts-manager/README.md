# acctmgr - Lightweight Secure Account Manager

A portable, encrypted account manager that runs from SD card. Single binary, no runtime dependencies.

## Features

- **AES-256 encryption** via XChaCha20-Poly1305
- **Argon2id key derivation** (memory-hard, GPU-resistant)
- **Secure memory handling** - keys locked in RAM, wiped on exit
- **Clipboard integration** - auto-clears after 30 seconds
- **Session timeout** - auto-locks after 5 minutes of inactivity
- **Single static binary** - runs anywhere on Linux

## Build Requirements

```bash
# Debian/Ubuntu
sudo apt install build-essential libsodium-dev

# Then build
make
```

## Usage

```bash
# Run from current directory
./acctmgr

# Or specify vault location
./acctmgr /path/to/vault.dat

# Or set via environment
export ACCTMGR_VAULT=/media/sdcard/vault
./acctmgr
```

## Commands

| Command | Description |
|---------|-------------|
| `add` | Add new account |
| `ls [category]` | List accounts (optionally filter by category) |
| `show <n>` | Show account details |
| `cp <n> <field>` | Copy field to clipboard (pass\|user\|url\|ssh) |
| `edit <n>` | Edit account |
| `rm <n>` | Delete account |
| `search <query>` | Search accounts |
| `gen [length]` | Generate password (default: 20 chars) |
| `save` | Save vault |
| `lock` / `quit` | Lock vault and exit |
| `help` | Show help |

## Example Session

```
$ ./acctmgr
Account Manager v1.0
Vault: ./vault.dat
Creating new vault.
Set master password: ********
Confirm password: ********
Deriving key (this may take a moment)...

Vault unlocked (0 entries). Type 'help' for commands.

acctmgr> add
Name: DigitalOcean Master DB
Category [general]: servers
Username: root
Password (g=generate, or enter manually): g
Generated: kX9#mP2$vL8@nQ4&wR6j!
URL: 
SSH key path: ~/.ssh/do_master
Notes: Master Database VPS
Added entry #1: DigitalOcean Master DB

acctmgr> ls
SERVERS
    1. DigitalOcean Master DB (root)

acctmgr> cp 1 pass
✓ Copied to clipboard (clears in 30s)

acctmgr> quit
Vault locked, memory cleared.
```

## Install to SD Card

```bash
# Build and install
make install DEST=/media/moe/sdcard

# Or create portable package
make package
# Then extract acctmgr-portable.tar.gz to SD card
```

## Security Notes

**Protected against:**
- Theft of SD card (data encrypted at rest)
- Shoulder surfing (passwords never displayed)
- Memory forensics (sensitive data zeroed on exit)
- Swap file leaks (keys locked in RAM)

**Not protected against:**
- Keyloggers on compromised host
- Weak master password
- Physical access while unlocked

**Recommendations:**
- Use a strong master password (20+ chars)
- Keep SD card physically secure
- Keep backup copy in secure location
- Use different master password than any stored accounts

## File Structure

```
sdcard/
└── vault/
    ├── acctmgr        # Binary (~100KB)
    └── vault.dat      # Encrypted data
```

## Environment Variables

| Variable | Description |
|----------|-------------|
| `ACCTMGR_VAULT` | Directory for vault.dat file |
| `ACCTMGR_DIR` | Alternative directory for vault |

## License

MIT
