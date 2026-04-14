# Unlock GNOME Keyring with LUKS Keyfile

Automatically unlock the GNOME login keyring at session start using your LUKS disk encryption keyfile. No password prompts. Security is tied to your disk encryption.

## The Problem

When you use LUKS full-disk encryption with a **keyfile** (rather than a passphrase typed at boot) and configure **autologin**, apps like Brave, Chrome, and anything using `libsecret` will prompt for the keyring password on every launch. This happens because GNOME Keyring normally derives its unlock password from your login password — which autologin never provides.

## The Solution

Derive a deterministic password from the same LUKS keyfile used to decrypt your disk. The keyring is created with this derived password, and an XDG autostart script unlocks it at session start before any apps run.

## How It Works

```
Boot
 │
 ├── LUKS decrypts /home using keyfile on SD card
 │     └── initramfs includes passdev keyscript (survives kernel updates)
 │
 ├── Autologin (lightdm/gdm)
 │
 ├── XDG autostart: LUKSBridge runs as user
 │     ├── Waits up to 15s for USB/SD card to mount
 │     ├── Derives password: sha256sum(/mnt/key/home.key)
 │     └── gnome-keyring-daemon --replace --unlock
 │
 └── Keyring unlocked → apps access secrets without prompts
```

> **Why not PAM?** `pam_exec.so` runs as root before the user session exists. The keyring daemon it starts runs in root's session context, not the user's — so the user session daemon stays locked. XDG autostart runs in the correct user session and works reliably.

## Prerequisites

- Linux with GNOME Keyring
- LUKS encrypted partition unlocked via keyfile at boot
- Autologin configured (lightdm, gdm, sddm, etc.)
- Keyfile accessible at a known path after login (e.g. mounted USB/SD card)

```bash
sudo apt install gnome-keyring libsecret-tools
```

## Installation

### Deploy the Unlock Script

Copy `LUKSBridge` to `/usr/local/bin/` and edit the `KEY_FILE` path:

```bash
sudo cp ./LUKSBridge /usr/local/bin/LUKSBridge

# Edit LUKSBridge and point to your actual keyfile
sudo nano /usr/local/bin/LUKSBridge
```

The script waits up to 15 seconds for the keyfile to appear, then derives the password and unlocks the daemon with `--replace --unlock`.

### Add XDG Autostart Entry

Create `/home/$USER/.config/autostart/unlock-keyring.desktop`:

```ini
[Desktop Entry]
Type=Application
Name=Unlock Keyring from LUKS
Exec=/usr/local/bin/LUKSBridge
Hidden=false
NoDisplay=true
X-GNOME-Autostart-Phase=Initialization
X-GNOME-AutoRestart=false
```

```bash
mkdir -p ~/.config/autostart
cp unlock-keyring.desktop ~/.config/autostart/
```

### Install the initramfs Hook

This ensures the `passdev` keyscript is re-bundled into initramfs whenever a kernel update rebuilds it. Without this, a new kernel would lose the script needed to read your keyfile off a removable device, and LUKS decryption at boot would fail.

```bash
sudo cp passdev-keyscript /etc/initramfs-tools/hooks/
sudo chmod +x /etc/initramfs-tools/hooks/passdev-keyscript
sudo update-initramfs -u
```

### Create the Keyring with the Derived Password

The keyring file must be encrypted with the **same derived password** the unlock script will use. 

Calculate it first:

```bash
KEY_HASH=$(sha256sum /mnt/key/home.key | awk '{print $1}')
```

Then recreate the login keyring:

```bash
# Stop any running keyring daemon
pkill gnome-keyring-daemon 2>/dev/null || true

# Back up existing keyring (it contains your current secrets if any)
cp ~/.local/share/keyrings/login.keyring \
   ~/.local/share/keyrings/login.keyring.bak 2>/dev/null || true

# Start a fresh daemon unlocked with the derived password
echo "$KEY_HASH" | gnome-keyring-daemon --replace --unlock

# Set login as the default keyring
echo "login" > ~/.local/share/keyrings/default
```

> **Note:** If you have existing secrets (e.g. Brave/Chrome passwords stored in the keyring), you'll need to re-add them after recreating the keyring. The old `.bak` file can be inspected manually if it was unencrypted or weakly protected.

### Re-add Browser Secrets (if needed)

For Chromium-based browsers (Brave, Chrome, Chromium):

```bash
# Check what was stored
secret-tool search --all xdg:schema chrome_libsecret_os_crypt_password_v2

# Re-store if missing (replace 'brave' with 'chrome' or 'chromium' as needed)
echo -n "your-encryption-key" | secret-tool store \
    --label="Chromium Safe Storage" \
    xdg:schema chrome_libsecret_os_crypt_password_v2 \
    application brave
```

## Verifying It Works

```bash
# Check the keyring is unlocked (returns false = unlocked, true = locked)
busctl --user call org.freedesktop.secrets \
    /org/freedesktop/secrets/collection/login \
    org.freedesktop.DBus.Properties Get ss \
    org.freedesktop.Secret.Collection Locked

# List all stored secrets
secret-tool search --all xdg:schema chrome_libsecret_os_crypt_password_v2

# Check syslog for unlock activity
journalctl --user -t unlock-keyring
```

Log out and back in to confirm the full flow works end-to-end before relying on it.

## Troubleshooting

### Keyring still locked after login

Check the autostart entry ran:

```bash
journalctl --user -t unlock-keyring
```

Check the keyfile is accessible:

```bash
ls -la /path/to/key.key
```

Test the script manually:

```bash
pkill gnome-keyring-daemon
/usr/local/bin/LUKSBridge
busctl --user call org.freedesktop.secrets /org/freedesktop/secrets/collection/login \
    org.freedesktop.DBus.Properties Get ss org.freedesktop.Secret.Collection Locked
```

### Browser still prompts for password after keyring is unlocked

The browser's internal encryption key must be re-entered after recreating the keyring. For Chromium-based browsers, the `Chromium Safe Storage` secret in the keyring is used to encrypt `Login Data`. If that secret is missing or wrong, the browser prompts for a new password and re-encrypts from scratch — your saved passwords remain accessible, just re-encrypted under the new key.

### LUKS fails to decrypt after kernel update

If LUKS can no longer read the keyfile off the removable device after a kernel update:

1. Verify the initramfs hook is installed: `ls /etc/initramfs-tools/hooks/passdev-keyscript`
2. Rebuild initramfs manually: `sudo update-initramfs -u`
3. Check `passdev` is present in the new initramfs: `lsinitramfs /boot/initrd.img-$(uname -r) | grep passdev`

### gnome-keyring-daemon exits 0 but keyring stays locked

This is expected behaviour — the daemon always exits 0 regardless of whether the password was correct. The only way to check if unlock worked is via DBus (see verification commands above) or by testing a `secret-tool lookup`.

## Files

| File                     | Purpose                                                            |
| ------------------------ | ------------------------------------------------------------------ |
| `LUKSBridge`             | Template unlock script (copy to `/usr/local/bin/`)                 |
| `unlock-keyring.desktop` | Create XDG autostart entry  (`~/.config/autostart/`)               |
| `passdev-keyscript`      | initramfs hook — keeps `passdev` in initramfs after kernel updates |

## Security Considerations

| Aspect              | Note                                                       |
| ------------------- | ---------------------------------------------------------- |
| Password derivation | `sha256sum` of full keyfile content                        |
| Security level      | Same as your LUKS disk encryption                          |
| Key compromise      | If the LUKS keyfile leaks, the keyring is also compromised |
| Deterministic       | Same keyfile always produces the same keyring password     |
| Physical access     | USB/SD card removal = keyring won't unlock (intentional)   |

## Uninstall

```bash
# Remove autostart entry (reverts to system default behaviour)
rm ~/.config/autostart/unlock-keyring.desktop

# Remove unlock script
sudo rm /usr/local/bin/LUKSBridge

# Remove initramfs hook and rebuild
sudo rm /etc/initramfs-tools/hooks/passdev-keyscript
sudo update-initramfs -u

# Optionally restore old keyring
mv ~/.local/share/keyrings/login.keyring.bak \
   ~/.local/share/keyrings/login.keyring
```
