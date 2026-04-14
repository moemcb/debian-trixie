# Running Libvirt VMs with Cloudflare WARP

This guide documents how to run isolated libvirt/KVM virtual machines on a Debian server that also runs Cloudflare WARP (Zero Trust tunnel).

## The Problem

Cloudflare WARP creates restrictive nftables rules and hijacks routing to send all traffic through its tunnel. This breaks libvirt VM networking because:

1. **nftables INPUT chain** (policy DROP) blocks traffic from VM bridge interfaces
2. **nftables OUTPUT chain** (policy DROP) blocks responses back to VMs
3. **Policy routing** sends VM traffic into WARP tunnel instead of direct internet
4. **Return routing** sends host→VM traffic into WARP instead of the bridge

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Host (Debian)                           │
│                                                                 │
│  ┌─────────┐     ┌──────────┐     ┌─────────────────────────┐   │
│  │   VM    │────▶│ virbr99  │────▶│  nftables + iptables    │   │
│  │10.10.0.x│     │10.10.0.1 │     │  (cloudflare-warp +     │   │
│  └─────────┘     └──────────┘     │   libvirt chains)       │   │
│       │               │           └───────────┬─────────────┘   │
│       │               │                       │                 │
│       │               │           ┌───────────▼─────────────┐   │
│       │               │           │    Policy Routing       │   │
│       │               │           │  ┌─────────────────┐    │   │
│       │               │           │  │ from 10.10.0/24 │    │   │
│       │               │           │  │ → table 200     │    │   │
│       │               │           │  │ (bypass WARP)   │    │   │
│       │               │           │  └─────────────────┘    │   │
│       │               │           └───────────┬─────────────┘   │
│       │               │                       │                 │
│  ┌────▼───────────────▼───────────────────────▼──────────────┐  │
│  │                      eth0                                 │  │
│  │                   192.168.1.10                            │  │
│  └──────────────────────┬────────────────────────────────────┘  │
└─────────────────────────┼───────────────────────────────────────┘
                          │
                          ▼
                   ┌──────────────┐
                   │ Router/GW    │
                   │ 192.168.1.1  │
                   └──────────────┘
                          │
                          ▼
                      Internet
```

## Prerequisites

- Debian 12+ with libvirt/QEMU/KVM installed
- Cloudflare WARP installed and running
- UFW firewall (optional, but assumed in this guide)

## Before following this guide, Check Hardware Support

```bash
egrep -c '(vmx|svm)' /proc/cpuinfo
```

If the result is `greater than 0`, you are good to go.

## Setup Host System & Install Libvirt

> ⚠️ **Important Security Notes** 
> Disable VM escape vectors: 
> 
> ```bash
> sudo nano /etc/libvirt/qemu.conf
> ```
> 
> **Set:**
> 
>     `user = "libvirt-qemu"`  
> 
>     `group = "libvirt-qemu"`
> 
> **Restart:**
> 
> ```bash
> sudo systemctl restart libvirtd
> ```
> 
> **Disable shared folders unless needed, Avoid SPICE/VNC exposure**
> **Keep QEMU updated (VM escapes are real)**

**Enable IP Forwarding:**

```bash
sudo nano /etc/ufw/sysctl.conf
```

> **Uncomment or Add:** `net/ipv4/ip_forward=1`

**Apply and Verify:**

```bash
sudo systctl -p
sudo sysctl net.ipv4.ip_forward
```

**Expected Output:**

> net.ipv4.ip_forward = 1

**Change UFW Default Forwarding policy:**

```bash
sudo nano /etc/default/ufw
```

> Change `DEFAULT_FORWARD_POLICY="DROP"`  
> 
> To `DEFAULT_FORWARD_POLICY="ACCEPT"`

**Reload UFW**

```bash
sudo ufw reload
```

---

```bash
sudo apt update
sudo apt install qemu-kvm libvirt-daemon-system libvirt-clients virtinst bridge-utils

# Enable libvirtd
sudo systemctl enable --now libvirtd
```

---

**Enable and Start Serial Console:**

```bash
sudo systemctl enable serial-getty@ttyS0.service
sudo systemctl start serial-getty@ttyS0.service
```

## Create a custom Isolated VM Network

**Create** `/tmp/isolated-net.xml`

```xml
<network>
  <name>isolated-net</name>
  <forward mode='nat'/>
  <bridge name='virbr99' stp='on' delay='0'/>
  <ip address='10.10.0.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.10.0.100' end='10.10.0.200'/>
    </dhcp>
  </ip>
</network>
```

**Create systemd UFW “post-up” hook**

```bash
[Unit]
Description=UFW-compatible NAT for isolated VMs
After=network.target ufw.service
Wants=ufw.service

[Service]
Type=oneshot
ExecStart=/usr/sbin/ufw route allow in on virbr99 out on eth0
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

**Define and start the network** 

```bash
sudo virsh net-define /tmp/isolated-net.xml
sudo virsh net-start isolated-net
sudo virsh net-autostart isolated-net
```

## Step 3: Configure WARP Bypass Rules

WARP's nftables rules block VM traffic. 

Create `/usr/local/bin/libvirt-warp-fix`:

```bash
#!/bin/bash
# Libvirt + Cloudflare WARP Compatibility Fix
set -e

VM_SUBNET="10.10.0.0/24"
VM_BRIDGE="virbr99"
ROUTE_TABLE="200"
GATEWAY="192.168.1.1"      # Your router IP
WAN_IFACE="eth0"          # Your WAN interface

echo "[+] Adding nftables rules for WARP bypass..."

# Allow VM bridge traffic through WARP's firewall
nft insert rule inet cloudflare-warp input iif "$VM_BRIDGE" accept 2>/dev/null || true
nft insert rule inet cloudflare-warp output oif "$VM_BRIDGE" accept 2>/dev/null || true

# Allow established/related connections (critical for return traffic)
nft insert rule inet cloudflare-warp input position 0 ct state established,related accept 2>/dev/null || true

# Allow traffic to/from VM subnet
nft insert rule inet cloudflare-warp output position 0 ip daddr "$VM_SUBNET" accept 2>/dev/null || true
nft insert rule inet cloudflare-warp output position 0 ip saddr "$VM_SUBNET" accept 2>/dev/null || true

echo "[+] Adding routing rules to bypass WARP..."

# Create routing table for VM outbound traffic (bypasses WARP tunnel)
if ! ip route show table $ROUTE_TABLE 2>/dev/null | grep -q default; then
    ip route add default via $GATEWAY dev $WAN_IFACE table $ROUTE_TABLE
fi

# Policy rules: VM traffic uses direct routing, not WARP tunnel
ip rule show | grep -q "from $VM_SUBNET lookup $ROUTE_TABLE" || \
    ip rule add from $VM_SUBNET lookup $ROUTE_TABLE priority 99

ip rule show | grep -q "to $VM_SUBNET lookup main" || \
    ip rule add to $VM_SUBNET lookup main priority 98

echo "[+] Done."
```

Make executable:

```bash
sudo chmod +x /usr/local/bin/libvirt-warp-fix
```

## Step 4: Create Systemd Service

Create `/etc/systemd/system/libvirt-warp-fix.service`:

```ini
[Unit]
Description=Libvirt + Cloudflare WARP Compatibility Fix
After=network.target libvirtd.service warp-svc.service
Wants=libvirtd.service

[Service]
Type=oneshot
ExecStart=/usr/local/bin/libvirt-warp-fix
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

Enable the service:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now libvirt-warp-fix.service
```

## Step 5: UFW Rules (Recommended)

UFW and LAN isolation rules to prevent VMs accessing your home network:

```bash
# Block VM <-> LAN traffic
sudo ufw route deny from 10.10.0.0/24 to 192.168.1.0/24
sudo ufw route deny from 192.168.1.0/24 to 10.10.0.0/24

# Allow VM -> Internet
sudo ufw route allow in on virbr99 out on eth0
sudo ufw reload
```

## Step 6: Create a VM

> By default, libvirt stores VM disk images in /var/lib/libvirt/images.
> 
> If you plan to use non-standard image locations or NAS mounts
> 
> [See this guide](../desktop/libvirt-change-default-vm-location.md)

```bash
# Create VM (headless, serial console)
sudo virt-install \
  --name my-vm \
  --memory 2048 \
  --vcpus 2 \
  --disk path=/var/lib/libvirt/images/debian-vm.qcow2,size=30,format=qcow2,bus=virtio \
  --location https://deb.debian.org/debian/dists/stable/main/installer-amd64/ \
  --os-variant debian12 \
  --network network=isolated-net,model=virtio \
  --graphics none \
  --console pty,target_type=serial \
  --extra-args 'console=ttyS0,115200n8 serial'
```

## Post-Install: Enable Serial Console

The Debian installer uses the serial console during installation,but the **installed system** does not enable a getty on ttyS0 by default. After install, `virsh console` will connect but show no login prompt and accept no input (except `Ctrl+]` to escape).

Fix from the host (requires qemu-guest-agent installed in the VM):

```bash
sudo virsh qemu-agent-command <vm-name> \
  '{"execute":"guest-exec","arguments":{"path":"/bin/systemctl","arg":["enable","--now","serial-getty@ttyS0.service"],"capture-output":true}}'
```

Or if you have SSH access to the VM:

```bash
ssh root@<vm-ip> 'systemctl enable --now serial-getty@ttyS0.service'
```

After this, `virsh console <vm-name>` will show a login prompt for the VM.

Press Enter if the prompt doesn't appear immediately.
