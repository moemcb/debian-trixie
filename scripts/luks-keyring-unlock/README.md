# LUKS Keyring Auto-Unlock

Automatically unlock GNOME Keyring at login using your LUKS disk encryption keyfile. No password prompts, security tied to disk encryption.

## Use Case

- You use LUKS full-disk encryption with a keyfile
- You use autologin (no password at login)
- Apps like Brave/Chrome prompt for keyring password because GNOME Keyring can't auto-unlock without a login password

This solution derives the keyring password from your LUKS keyfile, so the keyring automatically unlocks when your encrypted disk is mounted.

## How It Works

```
Boot
  │
  ├── LUKS decrypts /home using /path/to/keyfile
  │
  ├── Autologin (lightdm/gdm)
  │
  ├── XDG autostart runs unlock script (overrides default gnome-keyring-secrets.desktop)
  │   └── Script: sha256(first 256 bytes of keyfile) → derived password
  │   └── Runs: gnome-keyring-daemon --unlock --components=secrets
  │
  └── Keyring unlocked → Apps access secrets without prompts
```

## Prerequisites

- Linux with GNOME Keyring
- LUKS encrypted partition with keyfile
- Autologin configured (lightdm, gdm, etc.)

### Required Packages

```bash
# Debian/Ubuntu
sudo apt install gnome-keyring libsecret-tools

# Arch
sudo pacman -S gnome-keyring libsecret

# Fedora
sudo dnf install gnome-keyring libsecret
```

## Installation

### 1. Create the Unlock Script

```bash
mkdir -p ~/.local/bin

cat > ~/.local/bin/start-keyring-unlocked.sh << 'EOF'
#!/bin/bash
# Start gnome-keyring-daemon unlocked using LUKS-derived password

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
EOF

chmod +x ~/.local/bin/start-keyring-unlocked.sh
```

**Important:** Edit the script and change `KEYFILE` to your actual LUKS keyfile path.

### 2. Override the Default Keyring Autostart

```bash
mkdir -p ~/.config/autostart

cat > ~/.config/autostart/gnome-keyring-secrets.desktop << EOF
[Desktop Entry]
Type=Application
Name=Secret Storage Service
Comment=GNOME Keyring: Secret Service (LUKS unlocked)
Exec=$HOME/.local/bin/start-keyring-unlocked.sh secrets
OnlyShowIn=GNOME;Unity;MATE;XFCE;
X-GNOME-Autostart-Phase=PreDisplayServer
X-GNOME-AutoRestart=false
X-GNOME-Autostart-Notify=true
NoDisplay=true
Hidden=false
EOF
```

This overrides `/etc/xdg/autostart/gnome-keyring-secrets.desktop` with our version that unlocks automatically.

### 3. Create the Keyring with the Derived Password

The keyring file must be encrypted with the same derived password:

```bash
# Calculate derived password
KEYFILE="/mnt/key/home.key"  # Your keyfile path
DERIVED_PASS=$(head -c 256 "$KEYFILE" | sha256sum | cut -d' ' -f1)

# Stop any running keyring daemon
pkill gnome-keyring

# Back up existing keyrings
mv ~/.local/share/keyrings/login.keyring ~/.local/share/keyrings/login.keyring.bak 2>/dev/null

# Start keyring daemon with derived password (creates new keyring)
echo -n "$DERIVED_PASS" | gnome-keyring-daemon --unlock --components=secrets

# Set login as default keyring
echo "login" > ~/.local/share/keyrings/default

# Verify it works
secret-tool search --all xdg:schema chrome_libsecret_os_crypt_password_v2
```

### 4. Add Your Secrets

For Chromium/Brave browser password encryption:

```bash
echo -n "your-encryption-key" | secret-tool store \
    --label="Chromium Safe Storage" \
    xdg:schema chrome_libsecret_os_crypt_password_v2 \
    application brave
```

For other browsers, use `application chrome` or `application chromium`.

### 5. Test

```bash
# Kill keyring and let the script restart it
pkill gnome-keyring
rm -rf /run/user/$(id -u)/keyring

# Run the script manually
~/.local/bin/start-keyring-unlocked.sh secrets

# Verify keyring is unlocked
secret-tool lookup xdg:schema chrome_libsecret_os_crypt_password_v2 application brave

# Full test: log out and back in, then check Brave
```

## Migrating from Corrupted Keyring

If your keyring file is corrupted (shows as "ASCII text" instead of "GNOME keyring"):

```bash
# Check if corrupted
file ~/.local/share/keyrings/*.keyring

# Corrupted keyrings may have readable secrets:
grep -E "secret=|display-name=" ~/.local/share/keyrings/YourKeyring.keyring

# Extract secrets and add to new keyring using secret-tool store
```

## Troubleshooting

### Keyring not unlocking after reboot

```bash
# Check if script ran
cat /tmp/unlock-keyring.log  # If you added logging

# Verify keyfile is accessible at boot
ls -la /mnt/key/home.key

# Test script manually
pkill gnome-keyring
~/.local/bin/start-keyring-unlocked.sh secrets
secret-tool search --all xdg:schema chrome_libsecret_os_crypt_password_v2
```

### "--start is incompatible with --unlock" error

Use only `--unlock` (not `--start --unlock`). The `--unlock` flag both starts the daemon AND unlocks it.

### Secrets not accessible

```bash
# Check default keyring points to 'login'
cat ~/.local/share/keyrings/default

# Verify keyring format is correct
file ~/.local/share/keyrings/login.keyring
# Should show: "GNOME keyring, major version 0, minor version 0, crypto type 0 (AES)..."

# List all secrets
secret-tool search --all xdg:schema chrome_libsecret_os_crypt_password_v2
```

### Browser still prompts for password

1. Ensure Chromium Safe Storage key exists in keyring
2. Restart the browser after keyring is unlocked
3. The browser's `Login Data` SQLite file must match the encryption key

## Security Considerations

| Aspect | Note |
|--------|------|
| Password derivation | SHA256 of first 256 bytes of keyfile |
| Security level | Same as your LUKS encryption |
| Key compromise | If LUKS keyfile leaks, keyring is also compromised |
| Deterministic | Same keyfile always produces same keyring password |

## Files

| File | Purpose |
|------|---------|
| `~/.local/bin/start-keyring-unlocked.sh` | Unlock script |
| `~/.config/autostart/gnome-keyring-secrets.desktop` | Autostart override |
| `~/.local/share/keyrings/login.keyring` | Encrypted keyring |
| `~/.local/share/keyrings/default` | Points to "login" |

## Why Not PAM?

PAM-based unlock (`pam_exec.so`) runs as root before the user's session starts. The gnome-keyring-daemon started by PAM runs as root, separate from the user's session daemon. This approach (XDG autostart override) runs in the user's session and works correctly.

## Uninstall

```bash
# Remove script
rm ~/.local/bin/start-keyring-unlocked.sh

# Remove autostart override (reverts to system default)
rm ~/.config/autostart/gnome-keyring-secrets.desktop

# Optionally restore old keyring
mv ~/.local/share/keyrings/login.keyring.bak ~/.local/share/keyrings/login.keyring
```

## License

MIT - Do whatever you want with it.

## Credits

Solution developed for Debian 13 + XFCE + lightdm autologin with LUKS keyfile encryption.
