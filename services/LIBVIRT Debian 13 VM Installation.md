# LIBVIRT Debian 13 VM Installation

This guide shows how to:

✅ Create a Debian 13 virtual machine using **VNC for initial installation**  
✅ Access the installer remotely via **SSH tunnel + VNC**  
✅ Enable a **persistent serial console**  
✅ Remove VNC entirely after install  
✅ Prepare the VM for headless server operation

The result is a **clean, headless VM** managed entirely with `virsh console`.

---

## Prerequisites

Host system requirements:

- Debian 13 host
- libvirt + KVM installed
- working virtual network
- ISO available locally

Install required packages if needed:

```bash
sudo apt install qemu-kvm libvirt-daemon-system virtinst virt-manager
```

Verify libvirt:

```bash
virsh list --all
```

---

## Create the VM (Initial VNC Install)

We first create the VM using **VNC graphics** because Debian installer requires a display.

```bash
virt-install \  
  --name debian13 \  
  --memory 8192 \  
  --vcpus 4 \  
  --cpu host-passthrough \  
  --disk path=/path/to/libvirt-images/debian13.qcow2,size=40,format=qcow2,bus=virtio \ 
  --os-variant debian13 \  
  --network network=default,model=virtio \  
  --graphics vnc,listen=127.0.0.1 \  
  --cdrom /path/to/debian-13.3.0-amd64-netinst.iso
```

- Creates a VM named **debian13**
- Uses VirtIO disk + network drivers
- Exposes VNC only on localhost
- Boots directly into Debian installer

---

## Connect to the Installer via SSH Tunnel

Since VNC listens only on localhost of the hypervisor, tunnel it.

From your workstation:

```bash
ssh -L 5900:localhost:5900 user@vm-host
```

Now connect using any VNC viewer:

You should see the Debian installer.

---

## Install Debian Normally

Proceed with a standard install:

Recommended options:

- Minimal system
- SSH server ✔
- No desktop environment
- Use VirtIO disk defaults

Finish installation and reboot.

---

## Enable Serial Console Inside the VM (IMPORTANT)

Before removing VNC, configure Debian to allow console access.

**Login to the VM through VNC.**

---

### Enable getty on serial console

```bash
sudo systemctl enable serial-getty@ttyS0.service
```

---

### Enable kernel serial output

Edit GRUB:

```bash
sudo nano /etc/default/grub
```

Find:

> GRUB_CMDLINE_LINUX_DEFAULT="quiet"

Change to:

> GRUB_CMDLINE_LINUX_DEFAULT="console=tty0 console=ttyS0,115200n8"

---

### Enable GRUB serial terminal

Add:

> GRUB_TERMINAL="serial"
> GRUB_SERIAL_COMMAND="serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1"

---

### Update GRUB

```bash
sudo update-grub
```

Shutdown the VM:

```bash
sudo shutdown
```

---

## Enable Serial Console in libvirt

Now modify the VM definition.

```bash
virsh edit debian13
```

Add inside `<devices>` tags if needed:

```txt
<console type='pty'>
     <target type='serial' port='0'/> 
</console>  
```

And

```txt
<serial type='pty'>
    <target port='0'/>
</serial>
```

Remove VNC Graphics for Headless Mode

> <graphics type='vnc' ... />

and any `<video>` sections if present.

**Save and Exit**

---

## Boot Using Serial Console

Start the VM:

```bash
virsh start debian13
```

Connect:

```bash
virsh console debian13
```

You should now see:

> Connected to domain `debain13`
> 
> Escape character is ^] (Ctrl + ])

Hit enter too bump to the login

Login with the credentials set during installation.

---

## Quick Command Reference

Start VM:

```bash
virsh start debian13
```

Autostart VM

```bash
virsh autostart debian13
```

Console access:

```bash
virsh console debian13
```

Exit console:

> CTRL + ]

Additional Storage:

```bash
virsh attach-device debian13 /path/to/storage.xml --config---
```

Using **VNC only for installation** avoids common problems:

- no permanent graphical device
- lower overhead
- cleaner VM definition
- easier remote automation
- behaves like real server hardware
