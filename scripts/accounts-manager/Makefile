# Account Manager Makefile
# Build: make
# Install to SD card: make install DEST=/media/moe/sdcard

CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wpedantic -std=c11
LDFLAGS = -lsodium

# For static build (larger binary, but no runtime dependencies)
# Requires: apt install libsodium-dev
# CC = gcc
# CFLAGS = -O2 -Wall -Wextra -Wpedantic -std=c11 -static
# LDFLAGS = -lsodium -lpthread

TARGET = acctmgr
SRC = acctmgr.c

# Default destination for install
DEST ?= .

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)
	@echo ""
	@echo "Build complete: ./$(TARGET)"
	@echo "Run with: ./$(TARGET) [vault_path]"

# Debug build with sanitizers
debug: CFLAGS = -g -O0 -Wall -Wextra -fsanitize=address,undefined
debug: $(TARGET)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	@mkdir -p $(DEST)/vault
	cp $(TARGET) $(DEST)/vault/
	chmod 700 $(DEST)/vault
	chmod 700 $(DEST)/vault/$(TARGET)
	@echo ""
	@echo "Installed to $(DEST)/vault/"
	@echo "Run with: $(DEST)/vault/$(TARGET)"

# Create a portable package
package: $(TARGET)
	@mkdir -p dist/vault
	cp $(TARGET) dist/vault/
	cp README.md dist/vault/ 2>/dev/null || true
	chmod 700 dist/vault
	chmod 700 dist/vault/$(TARGET)
	tar -czvf acctmgr-portable.tar.gz -C dist vault
	rm -rf dist
	@echo ""
	@echo "Created: acctmgr-portable.tar.gz"
	@echo "Extract to SD card and run ./vault/acctmgr"
