#!/usr/bin/env python3
"""
SecureVault Import Tool

Import credentials from various password manager export formats.
Supported: CSV (generic), Bitwarden, KeePass, 1Password

Usage:
    ./sv-import.py <format> <file.csv>
    
Examples:
    ./sv-import.py csv passwords.csv
    ./sv-import.py bitwarden bitwarden_export.csv
    ./sv-import.py keepass keepass_export.csv
"""

import sys
import csv
import subprocess
import json
from getpass import getpass

SV_BIN = "/mnt/key/securevault/sv"

# Category mapping based on common folder/type names
CATEGORY_MAP = {
    'login': 'websites',
    'website': 'websites',
    'web': 'websites',
    'server': 'servers',
    'ssh': 'ssh',
    'api': 'api_keys',
    'token': 'api_keys',
    'crypto': 'crypto',
    'bitcoin': 'crypto',
    'ethereum': 'crypto',
    'exchange': 'crypto',
    'email': 'email',
    'mail': 'email',
    'database': 'database',
    'db': 'database',
    'recovery': 'recovery',
    'backup': 'recovery',
    '2fa': 'recovery',
    'note': 'notes',
    'secure note': 'notes',
}

def guess_category(name, folder='', item_type=''):
    """Guess category based on name, folder, or type."""
    search_text = f"{name} {folder} {item_type}".lower()
    
    for keyword, category in CATEGORY_MAP.items():
        if keyword in search_text:
            return category
    
    return 'general'

def detect_env_var(name):
    """Detect if entry should be exported as env var."""
    name_upper = name.upper().replace(' ', '_').replace('-', '_')
    
    # Common API key patterns
    patterns = ['API_KEY', 'API_TOKEN', 'SECRET_KEY', 'ACCESS_KEY', 'AUTH_TOKEN']
    for pattern in patterns:
        if pattern in name_upper:
            return name_upper
    
    # Specific services
    services = {
        'anthropic': 'ANTHROPIC_API_KEY',
        'openai': 'OPENAI_API_KEY',
        'cloudflare': 'CLOUDFLARE_API_TOKEN',
        'github': 'GITHUB_TOKEN',
        'aws': 'AWS_SECRET_ACCESS_KEY',
        'digitalocean': 'DO_API_TOKEN',
        'stripe': 'STRIPE_SECRET_KEY',
    }
    
    name_lower = name.lower()
    for service, env_name in services.items():
        if service in name_lower:
            return env_name
    
    return ''

def run_sv_add(entry):
    """Add entry using sv command."""
    # Build entry via stdin to sv add would be complex
    # Instead, output commands to run manually or use direct IPC
    print(f"\n# Entry: {entry['name']}")
    print(f"# Category: {entry['category']}")
    
    # For now, output as shell commands with sv
    cmd = f"""sv add << 'EOF'
{entry['name']}
{entry['category']}
{entry.get('username', '')}
{entry.get('password', '')}
{entry.get('url', '')}


{entry.get('totp', '')}
{entry.get('notes', '')}

EOF"""
    print(cmd)

def import_generic_csv(filename):
    """Import generic CSV with columns: name,username,password,url,notes"""
    entries = []
    
    with open(filename, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Try to find common column names
            name = row.get('name') or row.get('title') or row.get('Name') or row.get('Title') or ''
            username = row.get('username') or row.get('login') or row.get('Username') or row.get('Login') or ''
            password = row.get('password') or row.get('Password') or ''
            url = row.get('url') or row.get('URL') or row.get('website') or row.get('Website') or ''
            notes = row.get('notes') or row.get('Notes') or row.get('comment') or ''
            folder = row.get('folder') or row.get('Folder') or row.get('group') or ''
            
            if not name:
                continue
            
            entry = {
                'name': name,
                'username': username,
                'password': password,
                'url': url,
                'notes': notes,
                'category': guess_category(name, folder),
                'env_name': detect_env_var(name),
            }
            entries.append(entry)
    
    return entries

def import_bitwarden(filename):
    """Import Bitwarden CSV export."""
    entries = []
    
    with open(filename, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            name = row.get('name', '')
            if not name:
                continue
            
            entry = {
                'name': name,
                'username': row.get('login_username', ''),
                'password': row.get('login_password', ''),
                'url': row.get('login_uri', ''),
                'notes': row.get('notes', ''),
                'totp': row.get('login_totp', ''),
                'category': guess_category(name, row.get('folder', ''), row.get('type', '')),
                'env_name': detect_env_var(name),
            }
            entries.append(entry)
    
    return entries

def import_keepass(filename):
    """Import KeePass CSV export."""
    entries = []
    
    with open(filename, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            name = row.get('Title', '')
            if not name:
                continue
            
            entry = {
                'name': name,
                'username': row.get('Username', ''),
                'password': row.get('Password', ''),
                'url': row.get('URL', ''),
                'notes': row.get('Notes', ''),
                'category': guess_category(name, row.get('Group', '')),
                'env_name': detect_env_var(name),
            }
            entries.append(entry)
    
    return entries

def import_1password(filename):
    """Import 1Password CSV export."""
    entries = []
    
    with open(filename, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            name = row.get('Title', '')
            if not name:
                continue
            
            entry = {
                'name': name,
                'username': row.get('Username', ''),
                'password': row.get('Password', ''),
                'url': row.get('URL', row.get('Websites', '')),
                'notes': row.get('Notes', row.get('notesPlain', '')),
                'totp': row.get('OTPAuth', ''),
                'category': guess_category(name, row.get('Tags', ''), row.get('Type', '')),
                'env_name': detect_env_var(name),
            }
            entries.append(entry)
    
    return entries

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    
    format_type = sys.argv[1].lower()
    filename = sys.argv[2]
    
    importers = {
        'csv': import_generic_csv,
        'generic': import_generic_csv,
        'bitwarden': import_bitwarden,
        'keepass': import_keepass,
        '1password': import_1password,
    }
    
    if format_type not in importers:
        print(f"Unknown format: {format_type}")
        print(f"Supported formats: {', '.join(importers.keys())}")
        sys.exit(1)
    
    print(f"Importing from {filename} ({format_type} format)...")
    
    try:
        entries = importers[format_type](filename)
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)
    
    print(f"Found {len(entries)} entries")
    print("\n" + "="*60)
    print("IMPORT COMMANDS")
    print("="*60)
    print("# Run these commands after unlocking vault (sv unlock)")
    print("# Review and modify as needed before running")
    
    for entry in entries:
        run_sv_add(entry)
    
    print("\n" + "="*60)
    print(f"# Total: {len(entries)} entries")
    print("# Pipe this output to bash after review: ./sv-import.py ... | bash")

if __name__ == '__main__':
    main()
