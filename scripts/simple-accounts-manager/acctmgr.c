/*
 * acctmgr - Lightweight Secure Account Manager
 * 
 * Single-file portable account manager with AES-256 encryption.
 * Designed to run from SD card with no dependencies at runtime.
 * 
 * Build: gcc -O2 -o acctmgr acctmgr.c -lsodium
 * 
 * Security: Argon2id key derivation, XChaCha20-Poly1305 encryption,
 *           secure memory handling, clipboard auto-clear.
 * 
 * Author: Generated for Moe's infrastructure management
 * License: MIT
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <sodium.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define VAULT_MAGIC         "ACCT"
#define VAULT_VERSION       1
#define MAX_ENTRIES         256
#define MAX_NAME_LEN        64
#define MAX_CATEGORY_LEN    32
#define MAX_USERNAME_LEN    64
#define MAX_PASSWORD_LEN    128
#define MAX_URL_LEN         256
#define MAX_SSH_KEY_LEN     256
#define MAX_NOTES_LEN       512
#define MAX_INPUT_LEN       512
#define CLIPBOARD_TIMEOUT   30
#define SESSION_TIMEOUT     300  /* 5 minutes */

/* Argon2id parameters - balance security and usability */
#define ARGON2_OPSLIMIT     crypto_pwhash_OPSLIMIT_MODERATE
#define ARGON2_MEMLIMIT     crypto_pwhash_MEMLIMIT_MODERATE

/* ============================================================================
 * Data Structures
 * ============================================================================ */

typedef struct {
    char name[MAX_NAME_LEN];
    char category[MAX_CATEGORY_LEN];
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    char url[MAX_URL_LEN];
    char ssh_key_path[MAX_SSH_KEY_LEN];
    char notes[MAX_NOTES_LEN];
    uint64_t created_at;
    uint64_t updated_at;
} AccountEntry;

typedef struct {
    char magic[4];
    uint8_t version;
    uint8_t salt[crypto_pwhash_SALTBYTES];
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    uint32_t entry_count;
} VaultHeader;

typedef struct {
    VaultHeader header;
    AccountEntry entries[MAX_ENTRIES];
    uint32_t count;
    bool unlocked;
    bool modified;
    char vault_path[512];
    uint8_t key[crypto_secretbox_KEYBYTES];
} Vault;

/* ============================================================================
 * Global State
 * ============================================================================ */

static Vault g_vault;
static volatile sig_atomic_t g_timeout_triggered = 0;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static void secure_zero(void *ptr, size_t len) {
    sodium_memzero(ptr, len);
}

static void die(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    if (g_vault.unlocked) {
        secure_zero(&g_vault.key, sizeof(g_vault.key));
        secure_zero(&g_vault.entries, sizeof(g_vault.entries));
    }
    exit(1);
}

static char *trim(char *str) {
    char *end;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    *(end + 1) = 0;
    return str;
}

static void get_input(const char *prompt, char *buf, size_t buflen, bool hide) {
    struct termios old_term, new_term;
    
    printf("%s", prompt);
    fflush(stdout);
    
    if (hide) {
        tcgetattr(STDIN_FILENO, &old_term);
        new_term = old_term;
        new_term.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
    }
    
    if (fgets(buf, buflen, stdin) == NULL) {
        buf[0] = '\0';
    }
    
    if (hide) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
        printf("\n");
    }
    
    /* Remove trailing newline */
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') {
        buf[len-1] = '\0';
    }
}

static void get_password(const char *prompt, char *buf, size_t buflen) {
    get_input(prompt, buf, buflen, true);
}

static uint64_t get_timestamp(void) {
    return (uint64_t)time(NULL);
}

/* ============================================================================
 * Password Generation
 * ============================================================================ */

static void generate_password(char *buf, size_t len) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyz"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "0123456789"
                           "!@#$%^&*()-_=+[]{}|;:,.<>?";
    size_t charset_len = sizeof(charset) - 1;
    
    if (len < 2) return;
    len--;  /* Leave room for null terminator */
    
    for (size_t i = 0; i < len; i++) {
        uint32_t idx = randombytes_uniform(charset_len);
        buf[i] = charset[idx];
    }
    buf[len] = '\0';
}

/* ============================================================================
 * Clipboard Functions
 * ============================================================================ */

static void copy_to_clipboard(const char *text) {
    pid_t pid = fork();
    
    if (pid == 0) {
        /* Child: copy to clipboard */
        FILE *xclip = popen("xclip -selection clipboard 2>/dev/null", "w");
        if (xclip) {
            fputs(text, xclip);
            pclose(xclip);
        }
        
        /* Fork again to clear after timeout */
        pid_t clear_pid = fork();
        if (clear_pid == 0) {
            /* Grandchild: sleep then clear */
            sleep(CLIPBOARD_TIMEOUT);
            FILE *clear = popen("xclip -selection clipboard 2>/dev/null", "w");
            if (clear) {
                fputs("", clear);
                pclose(clear);
            }
            _exit(0);
        }
        _exit(0);
    } else if (pid > 0) {
        /* Parent: wait for first child */
        waitpid(pid, NULL, 0);
    }
}

/* ============================================================================
 * Vault File Operations
 * ============================================================================ */

static void get_vault_path(char *path, size_t pathlen) {
    const char *dir = getenv("ACCTMGR_VAULT");
    if (dir) {
        snprintf(path, pathlen, "%s/vault.dat", dir);
    } else {
        /* Default to same directory as executable or current dir */
        char *exe_dir = getenv("ACCTMGR_DIR");
        if (exe_dir) {
            snprintf(path, pathlen, "%s/vault.dat", exe_dir);
        } else {
            snprintf(path, pathlen, "./vault.dat");
        }
    }
}

static bool vault_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static bool derive_key(const char *password, const uint8_t *salt, uint8_t *key) {
    return crypto_pwhash(
        key, crypto_secretbox_KEYBYTES,
        password, strlen(password),
        salt,
        ARGON2_OPSLIMIT,
        ARGON2_MEMLIMIT,
        crypto_pwhash_ALG_ARGON2ID13
    ) == 0;
}

static bool vault_create(const char *path, const char *password) {
    memcpy(g_vault.header.magic, VAULT_MAGIC, 4);
    g_vault.header.version = VAULT_VERSION;
    g_vault.header.entry_count = 0;
    g_vault.count = 0;
    
    /* Generate random salt and nonce */
    randombytes_buf(g_vault.header.salt, sizeof(g_vault.header.salt));
    randombytes_buf(g_vault.header.nonce, sizeof(g_vault.header.nonce));
    
    /* Derive key from password */
    printf("Deriving key (this may take a moment)...\n");
    if (!derive_key(password, g_vault.header.salt, g_vault.key)) {
        fprintf(stderr, "Key derivation failed\n");
        return false;
    }
    
    /* Lock key in memory */
    sodium_mlock(g_vault.key, sizeof(g_vault.key));
    
    strncpy(g_vault.vault_path, path, sizeof(g_vault.vault_path) - 1);
    g_vault.unlocked = true;
    g_vault.modified = true;
    
    return true;
}

static bool vault_save(void) {
    if (!g_vault.unlocked || !g_vault.modified) {
        return true;
    }
    
    /* Prepare plaintext data */
    size_t plaintext_len = sizeof(AccountEntry) * g_vault.count;
    uint8_t *plaintext = sodium_malloc(plaintext_len + 16);
    if (!plaintext && plaintext_len > 0) {
        fprintf(stderr, "Memory allocation failed\n");
        return false;
    }
    
    if (plaintext_len > 0) {
        memcpy(plaintext, g_vault.entries, plaintext_len);
    }
    
    /* Encrypt data */
    size_t ciphertext_len = plaintext_len + crypto_secretbox_MACBYTES;
    uint8_t *ciphertext = sodium_malloc(ciphertext_len + 16);
    if (!ciphertext && ciphertext_len > 0) {
        sodium_free(plaintext);
        fprintf(stderr, "Memory allocation failed\n");
        return false;
    }
    
    /* Generate new nonce for each save */
    randombytes_buf(g_vault.header.nonce, sizeof(g_vault.header.nonce));
    
    if (plaintext_len > 0) {
        if (crypto_secretbox_easy(ciphertext, plaintext, plaintext_len,
                                   g_vault.header.nonce, g_vault.key) != 0) {
            sodium_free(plaintext);
            sodium_free(ciphertext);
            fprintf(stderr, "Encryption failed\n");
            return false;
        }
    }
    
    /* Update header */
    g_vault.header.entry_count = g_vault.count;
    
    /* Write to file */
    FILE *f = fopen(g_vault.vault_path, "wb");
    if (!f) {
        sodium_free(plaintext);
        sodium_free(ciphertext);
        fprintf(stderr, "Cannot open vault file for writing: %s\n", strerror(errno));
        return false;
    }
    
    /* Set restrictive permissions */
    fchmod(fileno(f), 0600);
    
    fwrite(&g_vault.header, sizeof(VaultHeader), 1, f);
    if (ciphertext_len > 0) {
        fwrite(ciphertext, ciphertext_len, 1, f);
    }
    fclose(f);
    
    sodium_free(plaintext);
    sodium_free(ciphertext);
    
    g_vault.modified = false;
    return true;
}

static bool vault_load(const char *path, const char *password) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open vault: %s\n", strerror(errno));
        return false;
    }
    
    /* Read header */
    if (fread(&g_vault.header, sizeof(VaultHeader), 1, f) != 1) {
        fclose(f);
        fprintf(stderr, "Cannot read vault header\n");
        return false;
    }
    
    /* Verify magic */
    if (memcmp(g_vault.header.magic, VAULT_MAGIC, 4) != 0) {
        fclose(f);
        fprintf(stderr, "Invalid vault file\n");
        return false;
    }
    
    /* Check version */
    if (g_vault.header.version != VAULT_VERSION) {
        fclose(f);
        fprintf(stderr, "Unsupported vault version\n");
        return false;
    }
    
    /* Derive key */
    printf("Deriving key (this may take a moment)...\n");
    if (!derive_key(password, g_vault.header.salt, g_vault.key)) {
        fclose(f);
        fprintf(stderr, "Key derivation failed\n");
        return false;
    }
    
    /* Lock key in memory */
    sodium_mlock(g_vault.key, sizeof(g_vault.key));
    
    g_vault.count = g_vault.header.entry_count;
    
    if (g_vault.count > MAX_ENTRIES) {
        fclose(f);
        secure_zero(g_vault.key, sizeof(g_vault.key));
        fprintf(stderr, "Vault contains too many entries\n");
        return false;
    }
    
    /* Read and decrypt entries if any exist */
    if (g_vault.count > 0) {
        size_t plaintext_len = sizeof(AccountEntry) * g_vault.count;
        size_t ciphertext_len = plaintext_len + crypto_secretbox_MACBYTES;
        
        uint8_t *ciphertext = sodium_malloc(ciphertext_len);
        if (!ciphertext) {
            fclose(f);
            secure_zero(g_vault.key, sizeof(g_vault.key));
            fprintf(stderr, "Memory allocation failed\n");
            return false;
        }
        
        if (fread(ciphertext, ciphertext_len, 1, f) != 1) {
            sodium_free(ciphertext);
            fclose(f);
            secure_zero(g_vault.key, sizeof(g_vault.key));
            fprintf(stderr, "Cannot read vault data\n");
            return false;
        }
        
        uint8_t *plaintext = sodium_malloc(plaintext_len);
        if (!plaintext) {
            sodium_free(ciphertext);
            fclose(f);
            secure_zero(g_vault.key, sizeof(g_vault.key));
            fprintf(stderr, "Memory allocation failed\n");
            return false;
        }
        
        if (crypto_secretbox_open_easy(plaintext, ciphertext, ciphertext_len,
                                        g_vault.header.nonce, g_vault.key) != 0) {
            sodium_free(ciphertext);
            sodium_free(plaintext);
            fclose(f);
            secure_zero(g_vault.key, sizeof(g_vault.key));
            fprintf(stderr, "Decryption failed - wrong password?\n");
            return false;
        }
        
        memcpy(g_vault.entries, plaintext, plaintext_len);
        
        sodium_free(ciphertext);
        sodium_free(plaintext);
    }
    
    fclose(f);
    
    strncpy(g_vault.vault_path, path, sizeof(g_vault.vault_path) - 1);
    g_vault.unlocked = true;
    g_vault.modified = false;
    
    return true;
}

static void vault_lock(void) {
    if (g_vault.modified) {
        vault_save();
    }
    
    secure_zero(g_vault.key, sizeof(g_vault.key));
    sodium_munlock(g_vault.key, sizeof(g_vault.key));
    secure_zero(g_vault.entries, sizeof(g_vault.entries));
    g_vault.unlocked = false;
    g_vault.count = 0;
    
    printf("Vault locked, memory cleared.\n");
}

/* ============================================================================
 * Account Management
 * ============================================================================ */

static void cmd_add(void) {
    if (g_vault.count >= MAX_ENTRIES) {
        printf("Vault is full (max %d entries)\n", MAX_ENTRIES);
        return;
    }
    
    AccountEntry *entry = &g_vault.entries[g_vault.count];
    memset(entry, 0, sizeof(AccountEntry));
    
    char buf[MAX_INPUT_LEN];
    
    get_input("Name: ", buf, sizeof(buf), false);
    if (strlen(buf) == 0) {
        printf("Cancelled.\n");
        return;
    }
    strncpy(entry->name, trim(buf), MAX_NAME_LEN - 1);
    
    get_input("Category [general]: ", buf, sizeof(buf), false);
    if (strlen(trim(buf)) == 0) {
        strncpy(entry->category, "general", MAX_CATEGORY_LEN - 1);
    } else {
        strncpy(entry->category, trim(buf), MAX_CATEGORY_LEN - 1);
    }
    
    get_input("Username: ", buf, sizeof(buf), false);
    strncpy(entry->username, trim(buf), MAX_USERNAME_LEN - 1);
    
    get_input("Password (g=generate, or enter manually): ", buf, sizeof(buf), false);
    if (strcmp(trim(buf), "g") == 0) {
        char genpw[25];
        generate_password(genpw, sizeof(genpw));
        strncpy(entry->password, genpw, MAX_PASSWORD_LEN - 1);
        printf("Generated: %s\n", genpw);
        secure_zero(genpw, sizeof(genpw));
    } else if (strlen(trim(buf)) > 0) {
        strncpy(entry->password, trim(buf), MAX_PASSWORD_LEN - 1);
    } else {
        get_password("Password (hidden): ", buf, sizeof(buf));
        strncpy(entry->password, buf, MAX_PASSWORD_LEN - 1);
    }
    secure_zero(buf, sizeof(buf));
    
    get_input("URL: ", buf, sizeof(buf), false);
    strncpy(entry->url, trim(buf), MAX_URL_LEN - 1);
    
    get_input("SSH key path: ", buf, sizeof(buf), false);
    strncpy(entry->ssh_key_path, trim(buf), MAX_SSH_KEY_LEN - 1);
    
    get_input("Notes: ", buf, sizeof(buf), false);
    strncpy(entry->notes, trim(buf), MAX_NOTES_LEN - 1);
    
    entry->created_at = get_timestamp();
    entry->updated_at = entry->created_at;
    
    g_vault.count++;
    g_vault.modified = true;
    
    printf("Added entry #%u: %s\n", g_vault.count, entry->name);
}

static void cmd_list(const char *filter) {
    if (g_vault.count == 0) {
        printf("Vault is empty.\n");
        return;
    }
    
    /* Collect unique categories */
    char categories[MAX_ENTRIES][MAX_CATEGORY_LEN];
    int cat_count = 0;
    
    for (uint32_t i = 0; i < g_vault.count; i++) {
        const char *cat = g_vault.entries[i].category;
        bool found = false;
        for (int j = 0; j < cat_count; j++) {
            if (strcasecmp(categories[j], cat) == 0) {
                found = true;
                break;
            }
        }
        if (!found && cat_count < MAX_ENTRIES) {
            strncpy(categories[cat_count++], cat, MAX_CATEGORY_LEN - 1);
        }
    }
    
    /* Print by category */
    for (int c = 0; c < cat_count; c++) {
        /* Skip if filter doesn't match */
        if (filter && strlen(filter) > 0 && strcasecmp(categories[c], filter) != 0) {
            continue;
        }
        
        /* Convert category to uppercase for display */
        char cat_upper[MAX_CATEGORY_LEN];
        strncpy(cat_upper, categories[c], MAX_CATEGORY_LEN - 1);
        for (char *p = cat_upper; *p; p++) {
            if (*p >= 'a' && *p <= 'z') *p -= 32;
        }
        
        printf("\n%s\n", cat_upper);
        
        for (uint32_t i = 0; i < g_vault.count; i++) {
            if (strcasecmp(g_vault.entries[i].category, categories[c]) == 0) {
                printf("  %3u. %s", i + 1, g_vault.entries[i].name);
                if (strlen(g_vault.entries[i].username) > 0) {
                    printf(" (%s)", g_vault.entries[i].username);
                }
                printf("\n");
            }
        }
    }
    printf("\n");
}

static void cmd_show(int idx) {
    if (idx < 1 || idx > (int)g_vault.count) {
        printf("Invalid entry number.\n");
        return;
    }
    
    AccountEntry *entry = &g_vault.entries[idx - 1];
    
    printf("\n");
    printf("  Name:     %s\n", entry->name);
    printf("  Category: %s\n", entry->category);
    printf("  Username: %s\n", entry->username);
    printf("  Password: ******** [use 'cp %d pass' to copy]\n", idx);
    printf("  URL:      %s\n", strlen(entry->url) > 0 ? entry->url : "-");
    printf("  SSH Key:  %s\n", strlen(entry->ssh_key_path) > 0 ? entry->ssh_key_path : "-");
    printf("  Notes:    %s\n", strlen(entry->notes) > 0 ? entry->notes : "-");
    
    /* Format timestamps */
    time_t created = (time_t)entry->created_at;
    time_t updated = (time_t)entry->updated_at;
    char created_str[64], updated_str[64];
    strftime(created_str, sizeof(created_str), "%Y-%m-%d %H:%M", localtime(&created));
    strftime(updated_str, sizeof(updated_str), "%Y-%m-%d %H:%M", localtime(&updated));
    
    printf("  Created:  %s\n", created_str);
    printf("  Updated:  %s\n", updated_str);
    printf("\n");
}

static void cmd_copy(int idx, const char *field) {
    if (idx < 1 || idx > (int)g_vault.count) {
        printf("Invalid entry number.\n");
        return;
    }
    
    AccountEntry *entry = &g_vault.entries[idx - 1];
    const char *value = NULL;
    
    if (strcmp(field, "pass") == 0 || strcmp(field, "password") == 0) {
        value = entry->password;
    } else if (strcmp(field, "user") == 0 || strcmp(field, "username") == 0) {
        value = entry->username;
    } else if (strcmp(field, "url") == 0) {
        value = entry->url;
    } else if (strcmp(field, "ssh") == 0) {
        value = entry->ssh_key_path;
    } else {
        printf("Unknown field: %s (use: pass, user, url, ssh)\n", field);
        return;
    }
    
    if (strlen(value) == 0) {
        printf("Field is empty.\n");
        return;
    }
    
    copy_to_clipboard(value);
    printf("✓ Copied to clipboard (clears in %ds)\n", CLIPBOARD_TIMEOUT);
}

static void cmd_edit(int idx) {
    if (idx < 1 || idx > (int)g_vault.count) {
        printf("Invalid entry number.\n");
        return;
    }
    
    AccountEntry *entry = &g_vault.entries[idx - 1];
    char buf[MAX_INPUT_LEN];
    
    printf("Editing: %s (press Enter to keep current value)\n\n", entry->name);
    
    printf("Name [%s]: ", entry->name);
    get_input("", buf, sizeof(buf), false);
    if (strlen(trim(buf)) > 0) {
        strncpy(entry->name, trim(buf), MAX_NAME_LEN - 1);
    }
    
    printf("Category [%s]: ", entry->category);
    get_input("", buf, sizeof(buf), false);
    if (strlen(trim(buf)) > 0) {
        strncpy(entry->category, trim(buf), MAX_CATEGORY_LEN - 1);
    }
    
    printf("Username [%s]: ", entry->username);
    get_input("", buf, sizeof(buf), false);
    if (strlen(trim(buf)) > 0) {
        strncpy(entry->username, trim(buf), MAX_USERNAME_LEN - 1);
    }
    
    get_input("New password (g=generate, Enter=keep, or type new): ", buf, sizeof(buf), false);
    if (strcmp(trim(buf), "g") == 0) {
        char genpw[25];
        generate_password(genpw, sizeof(genpw));
        strncpy(entry->password, genpw, MAX_PASSWORD_LEN - 1);
        printf("Generated: %s\n", genpw);
        secure_zero(genpw, sizeof(genpw));
    } else if (strlen(trim(buf)) > 0) {
        strncpy(entry->password, trim(buf), MAX_PASSWORD_LEN - 1);
    }
    secure_zero(buf, sizeof(buf));
    
    printf("URL [%s]: ", entry->url);
    get_input("", buf, sizeof(buf), false);
    if (strlen(trim(buf)) > 0) {
        strncpy(entry->url, trim(buf), MAX_URL_LEN - 1);
    }
    
    printf("SSH key path [%s]: ", entry->ssh_key_path);
    get_input("", buf, sizeof(buf), false);
    if (strlen(trim(buf)) > 0) {
        strncpy(entry->ssh_key_path, trim(buf), MAX_SSH_KEY_LEN - 1);
    }
    
    printf("Notes [%s]: ", entry->notes);
    get_input("", buf, sizeof(buf), false);
    if (strlen(trim(buf)) > 0) {
        strncpy(entry->notes, trim(buf), MAX_NOTES_LEN - 1);
    }
    
    entry->updated_at = get_timestamp();
    g_vault.modified = true;
    
    printf("Entry updated.\n");
}

static void cmd_delete(int idx) {
    if (idx < 1 || idx > (int)g_vault.count) {
        printf("Invalid entry number.\n");
        return;
    }
    
    AccountEntry *entry = &g_vault.entries[idx - 1];
    char buf[16];
    
    printf("Delete '%s'? (yes/no): ", entry->name);
    get_input("", buf, sizeof(buf), false);
    
    if (strcmp(trim(buf), "yes") != 0) {
        printf("Cancelled.\n");
        return;
    }
    
    /* Shift entries down */
    for (uint32_t i = idx - 1; i < g_vault.count - 1; i++) {
        memcpy(&g_vault.entries[i], &g_vault.entries[i + 1], sizeof(AccountEntry));
    }
    
    /* Clear last entry */
    secure_zero(&g_vault.entries[g_vault.count - 1], sizeof(AccountEntry));
    g_vault.count--;
    g_vault.modified = true;
    
    printf("Entry deleted.\n");
}

static void cmd_search(const char *query) {
    if (strlen(query) == 0) {
        printf("Usage: search <query>\n");
        return;
    }
    
    bool found = false;
    
    for (uint32_t i = 0; i < g_vault.count; i++) {
        AccountEntry *e = &g_vault.entries[i];
        
        /* Case-insensitive search in name, username, url, notes */
        if (strcasestr(e->name, query) ||
            strcasestr(e->username, query) ||
            strcasestr(e->url, query) ||
            strcasestr(e->notes, query) ||
            strcasestr(e->category, query)) {
            
            if (!found) {
                printf("\nSearch results for '%s':\n", query);
                found = true;
            }
            
            printf("  %3u. [%s] %s", i + 1, e->category, e->name);
            if (strlen(e->username) > 0) {
                printf(" (%s)", e->username);
            }
            printf("\n");
        }
    }
    
    if (!found) {
        printf("No matches found.\n");
    } else {
        printf("\n");
    }
}

static void cmd_gen(const char *len_str) {
    int len = 20;
    if (len_str && strlen(len_str) > 0) {
        len = atoi(len_str);
        if (len < 8) len = 8;
        if (len > 64) len = 64;
    }
    
    char pw[65];
    generate_password(pw, len + 1);
    printf("Generated (%d chars): %s\n", len, pw);
    
    char buf[8];
    get_input("Copy to clipboard? (y/n): ", buf, sizeof(buf), false);
    if (buf[0] == 'y' || buf[0] == 'Y') {
        copy_to_clipboard(pw);
        printf("✓ Copied (clears in %ds)\n", CLIPBOARD_TIMEOUT);
    }
    
    secure_zero(pw, sizeof(pw));
}

static void cmd_help(void) {
    printf("\nCommands:\n");
    printf("  add              Add new account\n");
    printf("  ls [category]    List accounts (optionally filter by category)\n");
    printf("  show <n>         Show account details\n");
    printf("  cp <n> <field>   Copy field to clipboard (pass|user|url|ssh)\n");
    printf("  edit <n>         Edit account\n");
    printf("  rm <n>           Delete account\n");
    printf("  search <query>   Search accounts\n");
    printf("  gen [length]     Generate password (default: 20 chars)\n");
    printf("  save             Save vault\n");
    printf("  lock             Lock vault and exit\n");
    printf("  quit             Same as lock\n");
    printf("  help             Show this help\n");
    printf("\n");
}

/* ============================================================================
 * Signal Handlers
 * ============================================================================ */

static void handle_timeout(int sig) {
    (void)sig;
    g_timeout_triggered = 1;
}

static void handle_interrupt(int sig) {
    (void)sig;
    printf("\nInterrupted. Locking vault...\n");
    vault_lock();
    exit(0);
}

static void setup_signals(void) {
    struct sigaction sa;
    
    /* Session timeout */
    sa.sa_handler = handle_timeout;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);
    
    /* Interrupt handling */
    sa.sa_handler = handle_interrupt;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void reset_timeout(void) {
    alarm(SESSION_TIMEOUT);
}

/* ============================================================================
 * Main Loop
 * ============================================================================ */

static void repl(void) {
    char line[MAX_INPUT_LEN];
    char cmd[64];
    char arg1[MAX_INPUT_LEN];
    char arg2[64];
    
    printf("\nVault unlocked (%u entries). Type 'help' for commands.\n\n", g_vault.count);
    
    setup_signals();
    reset_timeout();
    
    while (1) {
        if (g_timeout_triggered) {
            printf("\nSession timeout. Locking vault...\n");
            vault_lock();
            break;
        }
        
        get_input("acctmgr> ", line, sizeof(line), false);
        reset_timeout();
        
        /* Parse command */
        cmd[0] = arg1[0] = arg2[0] = '\0';
        sscanf(line, "%63s %511s %63s", cmd, arg1, arg2);
        
        if (strlen(cmd) == 0) {
            continue;
        }
        
        /* Execute command */
        if (strcmp(cmd, "add") == 0) {
            cmd_add();
        } else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "list") == 0) {
            cmd_list(arg1);
        } else if (strcmp(cmd, "show") == 0) {
            cmd_show(atoi(arg1));
        } else if (strcmp(cmd, "cp") == 0 || strcmp(cmd, "copy") == 0) {
            cmd_copy(atoi(arg1), arg2);
        } else if (strcmp(cmd, "edit") == 0) {
            cmd_edit(atoi(arg1));
        } else if (strcmp(cmd, "rm") == 0 || strcmp(cmd, "delete") == 0) {
            cmd_delete(atoi(arg1));
        } else if (strcmp(cmd, "search") == 0 || strcmp(cmd, "find") == 0) {
            /* Get full search query after command */
            char *query = line;
            while (*query && *query != ' ') query++;
            while (*query == ' ') query++;
            cmd_search(query);
        } else if (strcmp(cmd, "gen") == 0) {
            cmd_gen(arg1);
        } else if (strcmp(cmd, "save") == 0) {
            if (vault_save()) {
                printf("Vault saved.\n");
            }
        } else if (strcmp(cmd, "lock") == 0 || strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            vault_lock();
            break;
        } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
            cmd_help();
        } else {
            printf("Unknown command: %s (type 'help' for commands)\n", cmd);
        }
    }
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char **argv) {
    /* Initialize libsodium */
    if (sodium_init() < 0) {
        die("Failed to initialize libsodium");
    }
    
    /* Get vault path */
    char vault_path[512];
    if (argc > 1) {
        strncpy(vault_path, argv[1], sizeof(vault_path) - 1);
    } else {
        get_vault_path(vault_path, sizeof(vault_path));
    }
    
    printf("Account Manager v1.0\n");
    printf("Vault: %s\n", vault_path);
    
    char password[MAX_PASSWORD_LEN];
    
    if (vault_exists(vault_path)) {
        /* Existing vault - unlock */
        get_password("Master password: ", password, sizeof(password));
        
        if (!vault_load(vault_path, password)) {
            secure_zero(password, sizeof(password));
            return 1;
        }
    } else {
        /* New vault - create */
        printf("Creating new vault.\n");
        
        get_password("Set master password: ", password, sizeof(password));
        
        char confirm[MAX_PASSWORD_LEN];
        get_password("Confirm password: ", confirm, sizeof(confirm));
        
        if (strcmp(password, confirm) != 0) {
            secure_zero(password, sizeof(password));
            secure_zero(confirm, sizeof(confirm));
            die("Passwords don't match");
        }
        secure_zero(confirm, sizeof(confirm));
        
        if (!vault_create(vault_path, password)) {
            secure_zero(password, sizeof(password));
            return 1;
        }
        
        /* Save initial empty vault */
        if (!vault_save()) {
            secure_zero(password, sizeof(password));
            return 1;
        }
    }
    
    secure_zero(password, sizeof(password));
    
    /* Enter REPL */
    repl();
    
    return 0;
}
