#!/bin/bash
# LUKS Keyring Auto-Unlock Installer
# Run with: sudo ./install.sh /path/to/your/keyfile

set -e

KEYFILE="${1:-/mnt/key/home.key}"
SCRIPT_PATH="/usr/local/bin/unlock-keyring-from-luks"
PAM_FILE="/etc/pam.d/lightdm-autologin"

echo "LUKS Keyring Auto-Unlock Installer"
echo "==================================="
echo ""

# Check root
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: Run as root (sudo ./install.sh)"
    exit 1
fi

# Check keyfile
if [ ! -f "$KEYFILE" ]; then
    echo "ERROR: Keyfile not found: $KEYFILE"
    echo "Usage: sudo ./install.sh /path/to/your/keyfile"
    exit 1
fi

echo "[1/4] Creating unlock script..."
cat > "$SCRIPT_PATH" << SCRIPT
#!/bin/bash
# Derives keyring password from LUKS keyfile and unlocks gnome-keyring

KEYFILE="$KEYFILE"

if [ "\$PAM_TYPE" != "open_session" ]; then
    exit 0
fi

if [ ! -f "\$KEYFILE" ]; then
    logger -t unlock-keyring "LUKS keyfile not found: \$KEYFILE"
    exit 0
fi

DERIVED_PASS=\$(head -c 256 "\$KEYFILE" | sha256sum | cut -d' ' -f1)
echo -n "\$DERIVED_PASS" | gnome-keyring-daemon --unlock > /dev/null 2>&1
exit 0
SCRIPT

chmod +x "$SCRIPT_PATH"
echo "    Created: $SCRIPT_PATH"

echo "[2/4] Configuring PAM..."
if [ -f "$PAM_FILE" ]; then
    if grep -q "unlock-keyring-from-luks" "$PAM_FILE"; then
        echo "    PAM already configured"
    else
        sed -i '/@include common-session/a session optional pam_exec.so /usr/local/bin/unlock-keyring-from-luks' "$PAM_FILE"
        echo "    Updated: $PAM_FILE"
    fi
else
    echo "    WARNING: $PAM_FILE not found"
    echo "    Manually add to your display manager's PAM config:"
    echo "    session optional pam_exec.so /usr/local/bin/unlock-keyring-from-luks"
fi

echo "[3/4] Calculating derived password..."
DERIVED_PASS=$(head -c 256 "$KEYFILE" | sha256sum | cut -d' ' -f1)
echo "    Password derived (first 8 chars): ${DERIVED_PASS:0:8}..."

echo "[4/4] Instructions for keyring setup..."
echo ""
echo "Run these commands as your regular user:"
echo ""
echo "  # Back up existing keyring"
echo "  mv ~/.local/share/keyrings/login.keyring ~/.local/share/keyrings/login.keyring.bak"
echo ""
echo "  # Start keyring with derived password"
echo "  echo -n '$DERIVED_PASS' | gnome-keyring-daemon --unlock --components=secrets"
echo ""
echo "  # Set default keyring"
echo "  echo 'login' > ~/.local/share/keyrings/default"
echo ""
echo "==================================="
echo "Installation complete!"
echo ""
echo "Test with: sudo PAM_TYPE=open_session $SCRIPT_PATH"
echo "Then log out and back in to verify."
