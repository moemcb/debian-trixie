#!/bin/bash
#
# SecureVault Backup Script
#
# Creates encrypted backups of the vault to specified location.
# The backup is already encrypted (vault.enc), but this adds
# a timestamp and optional secondary encryption for offsite storage.
#
# Usage:
#   ./sv-backup.sh [backup_dir]
#
# Examples:
#   ./sv-backup.sh                    # Backup to default location
#   ./sv-backup.sh /mnt/backup        # Backup to specific directory
#   ./sv-backup.sh s3://bucket/path   # Backup to S3 (requires aws cli)
#

set -euo pipefail

# Configuration
SV_BASE="/mnt/key/securevault"
VAULT_FILE="$SV_BASE/vault.enc"
DEFAULT_BACKUP_DIR="$HOME/.securevault-backups"
BACKUP_DIR="${1:-$DEFAULT_BACKUP_DIR}"
DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_NAME="securevault_backup_$DATE"
KEEP_BACKUPS=30  # Number of backups to keep

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

# Check vault exists
if [[ ! -f "$VAULT_FILE" ]]; then
    log_error "Vault file not found: $VAULT_FILE"
    exit 1
fi

# Get vault size
VAULT_SIZE=$(du -h "$VAULT_FILE" | cut -f1)
log_info "Vault size: $VAULT_SIZE"

# Handle S3 backup
if [[ "$BACKUP_DIR" == s3://* ]]; then
    log_info "Backing up to S3: $BACKUP_DIR"
    
    if ! command -v aws &> /dev/null; then
        log_error "AWS CLI not found. Install with: pip install awscli"
        exit 1
    fi
    
    # Upload to S3
    aws s3 cp "$VAULT_FILE" "$BACKUP_DIR/$BACKUP_NAME.enc" \
        --storage-class STANDARD_IA
    
    log_info "Backup complete: $BACKUP_DIR/$BACKUP_NAME.enc"
    exit 0
fi

# Local backup
mkdir -p "$BACKUP_DIR"
chmod 700 "$BACKUP_DIR"

# Copy vault file
BACKUP_FILE="$BACKUP_DIR/$BACKUP_NAME.enc"
cp "$VAULT_FILE" "$BACKUP_FILE"
chmod 600 "$BACKUP_FILE"

log_info "Backup created: $BACKUP_FILE"

# Create checksum
sha256sum "$BACKUP_FILE" > "$BACKUP_FILE.sha256"
log_info "Checksum: $BACKUP_FILE.sha256"

# Optional: Create additional encrypted archive with different password
read -p "Create secondary encrypted archive? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    ARCHIVE_FILE="$BACKUP_DIR/$BACKUP_NAME.tar.gz.gpg"
    
    # Create tarball and encrypt with GPG
    tar -czf - -C "$SV_BASE" vault.enc | \
        gpg --symmetric --cipher-algo AES256 -o "$ARCHIVE_FILE"
    
    chmod 600 "$ARCHIVE_FILE"
    log_info "Encrypted archive: $ARCHIVE_FILE"
fi

# Cleanup old backups
if [[ -d "$BACKUP_DIR" ]]; then
    BACKUP_COUNT=$(find "$BACKUP_DIR" -name "securevault_backup_*.enc" | wc -l)
    
    if [[ $BACKUP_COUNT -gt $KEEP_BACKUPS ]]; then
        log_info "Cleaning old backups (keeping last $KEEP_BACKUPS)..."
        
        find "$BACKUP_DIR" -name "securevault_backup_*.enc" -type f | \
            sort | head -n -$KEEP_BACKUPS | \
            while read -r old_backup; do
                rm -f "$old_backup" "${old_backup}.sha256"
                log_info "Removed: $(basename "$old_backup")"
            done
    fi
fi

# Summary
echo
log_info "Backup Summary:"
echo "  Source: $VAULT_FILE"
echo "  Backup: $BACKUP_FILE"
echo "  Size:   $VAULT_SIZE"
echo "  Date:   $(date)"
echo
log_info "To restore: cp $BACKUP_FILE $VAULT_FILE"
