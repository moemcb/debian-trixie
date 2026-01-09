# SecureVault Makefile
# Build: make
# Install: make install

CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wpedantic -std=c11 -I./include
LDFLAGS_COMMON = -lsodium -lpthread

# GTK3 flags
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LDFLAGS = $(shell pkg-config --libs gtk+-3.0)

# Installation paths
PREFIX ?= /mnt/key/securevault
BINDIR = $(PREFIX)
CONFIGDIR = $(PREFIX)

# Source files
SRC_VAULT = src/vault.c
SRC_DAEMON = src/daemon.c $(SRC_VAULT)
SRC_CLI = src/cli.c $(SRC_VAULT)
SRC_GUI = src/gui.c

# Targets
TARGETS = sv securevaultd securevault-gtk

.PHONY: all clean install uninstall cli daemon gui

all: $(TARGETS)

# CLI tool (includes vault code)
sv: $(SRC_CLI)
	$(CC) $(CFLAGS) -o $@ $(SRC_CLI) $(LDFLAGS_COMMON)

# Daemon (includes vault code)
securevaultd: $(SRC_DAEMON)
	$(CC) $(CFLAGS) -o $@ $(SRC_DAEMON) $(LDFLAGS_COMMON)

# GTK GUI
securevault-gtk: $(SRC_GUI) $(SRC_VAULT)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -o $@ $(SRC_GUI) $(SRC_VAULT) $(LDFLAGS_COMMON) $(GTK_LDFLAGS)

# Individual targets
cli: sv
daemon: securevaultd
gui: securevault-gtk

# Debug builds
debug: CFLAGS = -g -O0 -Wall -Wextra -std=c11 -I./include -DDEBUG
debug: all

# Install to SD card
install: all
	@echo "Installing to $(PREFIX)..."
	@mkdir -p $(BINDIR)
	@mkdir -p $(BINDIR)/scripts
	@mkdir -p $(CONFIGDIR)
	cp sv $(BINDIR)/
	cp securevaultd $(BINDIR)/
	cp securevault-gtk $(BINDIR)/
	cp scripts/sv-login.sh $(BINDIR)/
	cp scripts/sv-backup.sh $(BINDIR)/scripts/
	cp scripts/sv-import.py $(BINDIR)/scripts/
	cp scripts/sv-screenlock-monitor.sh $(BINDIR)/scripts/
	cp scripts/sv-pam-unlock.sh $(BINDIR)/scripts/
	cp config/config.toml.example $(CONFIGDIR)/config.toml.example
	chmod 700 $(BINDIR)
	chmod 700 $(BINDIR)/sv
	chmod 700 $(BINDIR)/securevaultd
	chmod 700 $(BINDIR)/securevault-gtk
	chmod 700 $(BINDIR)/sv-login.sh
	chmod 700 $(BINDIR)/scripts/*
	@echo ""
	@echo "Installation complete!"
	@echo ""
	@echo "Next steps:"
	@echo "  1. Initialize vault:    $(BINDIR)/sv init"
	@echo "  2. Start daemon:        $(BINDIR)/sv daemon"
	@echo "  3. Add to ~/.bashrc:    source $(BINDIR)/sv-login.sh"
	@echo ""

# Install systemd user service
install-service:
	@mkdir -p ~/.config/systemd/user
	cp config/securevault.service ~/.config/systemd/user/
	systemctl --user daemon-reload
	@echo "Service installed. Enable with: systemctl --user enable securevault"

# Install desktop files for XFCE
install-desktop:
	@mkdir -p ~/.config/autostart
	@mkdir -p ~/.local/share/applications
	cp config/securevault-daemon.desktop ~/.config/autostart/
	cp config/securevault-gtk.desktop ~/.local/share/applications/
	@echo "Desktop files installed"
	@echo "  - Daemon will autostart on login"
	@echo "  - GUI available in applications menu"

# Create symlinks in user bin
install-links:
	@mkdir -p ~/.local/bin
	ln -sf $(BINDIR)/sv ~/.local/bin/sv
	ln -sf $(BINDIR)/securevault-gtk ~/.local/bin/securevault-gtk
	@echo "Symlinks created in ~/.local/bin"

# Full installation (everything)
install-all: install install-service install-desktop install-links
	@echo ""
	@echo "Full installation complete!"

uninstall:
	rm -f $(BINDIR)/sv
	rm -f $(BINDIR)/securevaultd
	rm -f $(BINDIR)/securevault-gtk
	rm -f $(BINDIR)/sv-login.sh

clean:
	rm -f $(TARGETS)
	rm -f *.o

# Help
help:
	@echo "SecureVault Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all             Build all components"
	@echo "  cli             Build CLI only"
	@echo "  daemon          Build daemon only"
	@echo "  gui             Build GTK GUI only"
	@echo "  debug           Build with debug symbols"
	@echo ""
	@echo "Installation:"
	@echo "  install         Install to $(PREFIX)"
	@echo "  install-service Install systemd user service"
	@echo "  install-links   Create symlinks in ~/.local/bin"
	@echo "  uninstall       Remove installed files"
	@echo ""
	@echo "Options:"
	@echo "  PREFIX=/path    Set installation prefix"
	@echo ""
	@echo "Dependencies:"
	@echo "  libsodium-dev   Cryptographic library"
	@echo "  libgtk-3-dev    GTK3 for GUI (optional)"
	@echo "  xclip           Clipboard support"
	@echo ""
