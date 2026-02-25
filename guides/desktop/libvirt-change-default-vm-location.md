# Change the Default libvirt Storage Location

By default, **libvirt** stores virtual machine disk images in:

/var/lib/libvirt/images

If you prefer to store VM disks elsewhere (for example, on a larger data drive or in your home directory), you can reconfigure the default storage pool.

This guide shows how to safely replace the default storage pool using `virsh`.

---

## 1️⃣ Stop and Remove the Existing Default Pool

```bash
sudo virsh pool-destroy default
sudo virsh pool-undefine default
```

> This removes the pool definition only. 
> Your existing VM disk files will **not** be deleted.

---

### 2️⃣ Create a New Default Pool

Replace `/home/user/vms` with your desired storage location.

```bash
sudo virsh pool-define-as --name default --type dir --target /home/user/vms
```

---

### 3️⃣ Start and Enable Autostart

```bash
sudo virsh pool-start default  
sudo virsh pool-autostart default
```

---

### 4️⃣ Verify Configuration

```bash
virsh pool-list --all
```

**You should see:**

```md
|  Name   | State  | Autostart |
|---------|--------|-----------|
| default | active |    yes    |   
```

---

### 5️⃣ Move Existing VM Disks (If Needed)

If you already have VM disk images in the old location:

```bash
sudo mv /var/lib/libvirt/images/*.qcow2 /home/user/vms/
```

---

### 6️⃣ Update VM Disk Paths (If Required)

If any VM XML definitions still reference the old path:

sudo virsh edit <vm-name>

Update the `<source file='...'>` path inside the `<disk>` section.
