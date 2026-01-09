#!/bin/bash
#
# SecureVault Login Script
# Source this from ~/.bashrc or ~/.profile
#
# Add to ~/.bashrc:
#   source /mnt/key/securevault/sv-login.sh
#

SV_BASE="/mnt/key/securevault"
SV_BIN="$SV_BASE/sv"
SV_SOCKET="/run/user/$(id -u)/securevault.sock"

# Check if SD card is mounted
if [[ ! -d "$SV_BASE" ]]; then
    return 0  # Silent exit if not mounted
fi

# Check if sv binary exists
if [[ ! -x "$SV_BIN" ]]; then
    echo "[SecureVault] Binary not found: $SV_BIN"
    return 1
fi

# Function to start daemon if not running
sv_ensure_daemon() {
    if [[ ! -S "$SV_SOCKET" ]]; then
        echo "[SecureVault] Starting daemon..."
        "$SV_BIN" daemon
        sleep 1
    fi
}

# Function to check if vault is unlocked
sv_is_unlocked() {
    local status
    status=$("$SV_BIN" status 2>/dev/null | grep "unlocked=1")
    [[ -n "$status" ]]
}

# Function to unlock vault (called manually if PAM didn't auto-unlock)
sv_unlock() {
    sv_ensure_daemon
    "$SV_BIN" unlock
    sv_export_env
}

# Function to export environment variables
sv_export_env() {
    if sv_is_unlocked; then
        eval "$("$SV_BIN" env --export 2>/dev/null)"
    fi
}

# Function to lock vault
sv_lock() {
    "$SV_BIN" lock
}

# Function to show quick status
sv_status() {
    "$SV_BIN" status
}

# Auto-start daemon
sv_ensure_daemon

# If vault is already unlocked (PAM auto-unlock), export env vars
if sv_is_unlocked; then
    sv_export_env
    echo "[SecureVault] Environment variables loaded"
fi

# Add sv to PATH if not already there
if [[ ":$PATH:" != *":$SV_BASE:"* ]]; then
    export PATH="$SV_BASE:$PATH"
fi

# Aliases for convenience
alias svl="sv list"
alias svc="sv cp"
alias sva="sv add"
alias svs="sv search"
alias svg="sv gen"

# Completion for sv command
_sv_completions() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local commands="init daemon unlock lock status list ls add get show cp rm delete search env gen ssh shutdown help"
    local categories="api_keys servers websites crypto recovery notes ssh database email general"
    
    if [[ ${COMP_CWORD} -eq 1 ]]; then
        COMPREPLY=($(compgen -W "$commands" -- "$cur"))
    elif [[ ${COMP_CWORD} -eq 2 ]]; then
        case "${COMP_WORDS[1]}" in
            list|ls)
                COMPREPLY=($(compgen -W "$categories" -- "$cur"))
                ;;
        esac
    fi
}

complete -F _sv_completions sv
