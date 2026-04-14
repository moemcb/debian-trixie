# Move Docker Volumes Location

This guide shows how to safely relocate Docker volumes to another disk or mount point without rebuilding containers.

Useful when:

- Root disk is running out of space
- Moving Docker data to SSD / HDD storage
- Separating OS and container data
- Infrastructure automation setups

---

## Overview

Docker stores named volumes by default at:

> /var/lib/docker/volumes

Instead of reconfiguring Docker itself, we:

1. Stop Docker
2. Copy existing volume data
3. Bind-mount a new storage location
4. Persist the mount via `/etc/fstab`
5. Restart Docker

Result:

Docker → /var/lib/docker/volumes → bind mount → new disk

No container changes required.

---

## ⚠️ Prerequisites

- Root or sudo access
- New disk mounted and writable
- Docker installed and working
- Enough space on the destination drive

Verify new mount:

```bash
df -h
```

Example destination:

> /mnt/docker-volumes

---

## Define Paths

Adjust these variables to match your system.

> DOCKER_VOLUMES="/var/lib/docker/volumes"  
> MOUNT_DIR="/path/to/new/volumes"

Example:

> MOUNT_DIR="/mnt/docker-volumes"

---

## Stop Docker

Prevent data corruption while migrating.

```bash
systemctl stop docker
```

(Optional but recommended)

```bash
systemctl stop docker.socket
```

---

## Copy Existing Volume Data

If migrating existing containers:

```bash
cp -a -r "$DOCKER_VOLUMES/." "$MOUNT_DIR"
```

### Why `-a`?

Preserves:

- permissions
- ownership
- symlinks
- timestamps

---

## Replace Original Volume Directory

Remove old directory:

```bash
rm -rf "$DOCKER_VOLUMES"
```

Recreate mount point:

```bash
mkdir -p "$DOCKER_VOLUMES"
```

---

## Bind Mount the New Location

```bash
mount --bind "$MOUNT_DIR" "$DOCKER_VOLUMES"
```

Verify:

```bash
mount | grep docker
```

You should see:

> /mnt/docker-volumes on /var/lib/docker/volumes type none (bind)

---

## Step 6 — Persist Mount After Reboot

Add to `/etc/fstab`:

```bash
echo "$MOUNT_DIR $DOCKER_VOLUMES none bind,nobootwait 0 2" >> /etc/fstab
```

Test configuration:

```bash
mount -a
```

No errors = good.

---

## Restart Docker

```bash
systemctl start docker
```

Confirm containers:

```bash
docker ps
```

---

## Verification

Check where volumes live:

```bash
docker volume inspect <volume-name>
```

Disk usage should now appear on the new drive:

```bash
df -h
```

---

## Optional Improvements

### Use UUID Instead of Path Mounts

If the destination disk changes device names:

```bash
blkid
```

Mount disk via UUID in `/etc/fstab`.

---

### Prevent Accidental Startup Issues

If your storage mounts late during boot:

```bash
x-systemd.requires-mounts-for=/mnt/docker-volumes
```

Example:

> /mnt/docker-volumes /var/lib/docker/volumes none bind,x-systemd.requires-mounts-for=/mnt/docker-volumes 0 2

---

## Rollback Procedure

If something goes wrong:

```bash
systemctl stop docker \ 
umount /var/lib/docker/volumes \  
rm -rf /var/lib/docker/volumes \ 
mv /var/lib/docker/volumes.bak /var/lib/docker/volumes \ 
systemctl start docker
```

---

## Summary

✅ No Docker reconfiguration  
✅ No container rebuilds  
✅ Safe data migration  
✅ Persistent across reboots
