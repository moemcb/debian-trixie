# Utility Scripts Collection [![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A growing collection of small, focused scripts for Linux systems administration,
automation, and tooling.

This repository is intended as a **toolbox** rather than a single project — each
script is designed to do one job well and stay out of the way.

---

## Scope

Scripts in this repository may cover:

- System inspection & diagnostics
- Infrastructure snapshotting
- Automation helpers
- Network and service utilities
- Docker and container tooling
- CLI quality-of-life tools

Not every script depends on the others. Each should be usable on its own.

---

## Design Principles

- **Minimal dependencies**  
  Prefer standard GNU/Linux tools.

- **Readable over clever**  
  Scripts should be understandable months later.

- **Safe by default**  
  No destructive actions without explicit intent.

- **Composable**  
  Output should be usable by other scripts or tools.

---

## Usage

Most scripts are intended to be run directly:

```bash
chmod +x script.sh
./script.sh
```
