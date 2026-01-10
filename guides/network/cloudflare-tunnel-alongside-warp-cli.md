# Setting Up WARP-CLI Alongside Cloudflared Tunnel

**OS:** Debian Trixie (13)


**Prerequisite:** Cloudflared tunnel already installed and working as systemd service


**Goal:** Add WARP-CLI for outbound SSH tunneling without breaking cloudflared



---

## Step 1: Install WARP-CLI

### Add the Cloudflare repository

```bash
# Install gpg if Needed
apt install gpg
# Add Cloudflare GPG key and Repository
curl -fsSL https://pkg.cloudflareclient.com/pubkey.gpg | sudo gpg --yes --dearmor --output /usr/share/keyrings/cloudflare-warp-archive-keyring.gpg
echo "deb [arch=amd64 signed-by=/usr/share/keyrings/cloudflare-warp-archive-keyring.gpg] https://pkg.cloudflareclient.com/ $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/cloudflare-client.list
sudo apt update
sudo apt install cloudflare-warp
```

### Verify installation

```bash
warp-cli --version
systemctl status warp-svc
```

### Enable IP forwarding

```bash
sudo sysctl -w net.ipv4.ip_forward=1
```

---

## Step 2: Create new WARP Connector

1. **Open the Cloudflare Zero Trust Dashboard**  
   Navigate to **Networks → Connectors** in the left-hand menu.

2. **Add a New WARP Connector**  
   Click **Add new WARP connector**.  
   Keep all the default settings as they are, then select **Next Step**.

3. **Initialise the WARP Connector**  
   After creating the connector, you will be shown a command that needs to be run on your server to initialise the new WARP instance.
   
   Run the following on your machine:

```bash
warp-cli connector new your_long_cloudflare_token
```

Accept the terms of service when prompted. **!!! DO NOT CONNECT YET !!!**

### Verify registration

```bash
warp-cli registration show
```

---

## Step 3: Configure WARP Split-Tunnel Exclusions

**Important:** Do this BEFORE connecting WARP for the first time.

### Add Cloudflare edge IP ranges

These are the IPs cloudflared uses for its tunnel connections:

```bash
warp-cli tunnel ip add-range 198.41.192.0/24
warp-cli tunnel ip add-range 198.41.200.0/24
warp-cli tunnel ip add-range 198.41.0.0/16
warp-cli tunnel ip add-range 162.159.0.0/16
```

### Verify exclusions

```bash
warp-cli tunnel ip list | grep "CLI exclude"
```

Should show:

```
198.41.192.0/24 (CLI exclude)
198.41.200.0/24 (CLI exclude)
198.41.0.0/16 (CLI exclude)
162.159.0.0/16 (CLI exclude)
```

---

## Step 4: Create Firewall Bypass Rules

WARP's split-tunnel only affects routing. WARP also creates a firewall that will block cloudflared's UDP traffic. We need bypass rules.

### Create the bypass script

```bash
sudo tee /usr/local/bin/cloudflared-warp-bypass.sh << 'EOF'
#!/bin/bash
# Allow cloudflared tunnel traffic to bypass WARP's firewall
set -euo pipefail

# Only attempt deletion if the table actually exists
if nft -t list table inet cloudflared_bypass >/dev/null 2>&1; then
    nft delete table inet cloudflared_bypass
fi

nft -f - << 'NFTABLES'
table inet cloudflared_bypass {
    chain output {
        type filter hook output priority -10; policy accept;

        # Cloudflared primary QUIC ports
        ip daddr 198.41.192.0/24 udp dport 7844 accept
        ip daddr 198.41.200.0/24 udp dport 7844 accept
        ip daddr 198.41.0.0/16   udp dport 7844 accept
        ip daddr 162.159.0.0/16   udp dport { 7844, 443 } accept   # 443 is fallback for some regions

        # Optional: also allow Cloudflare Spectrum / Magic Transit ranges if you ever use them
        # ip daddr 172.64.0.0/13   udp dport 7844 accept
    }
}
NFTABLES

logger -t cloudflared-warp-bypass "Bypass rules applied successfully"
echo "Cloudflared WARP bypass rules active"
EOF
```
```bash
sudo chmod +x /usr/local/bin/cloudflared-warp-bypass.sh
```

### Create systemd service

```bash
sudo tee /etc/systemd/system/cloudflared-warp-bypass.service << 'EOF'
[Unit]
Description=Cloudflared WARP Bypass Firewall Rules
After=network-pre.target
Before=warp-svc.service cloudflared.service
DefaultDependencies=no

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/local/bin/cloudflared-warp-bypass.sh
ExecStop=/usr/sbin/nft delete table inet cloudflared_bypass

[Install]
WantedBy=multi-user.target
WantedBy=warp-svc.service
WantedBy=cloudflared.service
EOF
```

### Enable and start

```bash
sudo systemctl daemon-reload
sudo systemctl enable cloudflared-warp-bypass.service
sudo systemctl start cloudflared-warp-bypass.service
```

### Verify it's running

```bash
sudo systemctl status cloudflared-warp-bypass.service
sudo nft list table inet cloudflared_bypass
```

---

## Step 5: Connect WARP

Now it's safe to connect:

```bash
warp-cli connect
```

### Verify connection

```bash
warp-cli status
```

Should show:

```
Status update: Connected
Network: healthy
```

---

## Step 6: Verify Everything Works

### Check cloudflared is still healthy

```bash
# Should show no "operation not permitted" errors
journalctl -u cloudflared --since '2 minutes ago' | grep -i 'operation not permitted' || echo "All good!"

# Check tunnel connections are registered
journalctl -u cloudflared --since '5 minutes ago' | grep "Registered"
```

### Test SSH through cloudflared tunnel

From another machine, SSH to your server through the tunnel. It should work.

### Check WARP is routing traffic

```bash
# Your outbound IP should be a Cloudflare IP
curl -s https://cloudflare.com/cdn-cgi/trace | grep -E "ip=|warp="
```

Should show `warp=on` or `warp=plus`.

---

## Summary of Services

After setup, you should have these services running:

| Service                           | Purpose               | Status          |
| --------------------------------- | --------------------- | --------------- |
| `cloudflared.service`             | Inbound tunnel access | Running         |
| `warp-svc.service`                | WARP daemon           | Running         |
| `cloudflared-warp-bypass.service` | Firewall bypass       | Active (exited) |

Check all at once:

```bash
systemctl status cloudflared warp-svc cloudflared-warp-bypass --no-pager
```

---

## Security Note

The bypass rules only allow outbound UDP to Cloudflare's own infrastructure (198.41.x.x, 162.159.x.x) on specific ports. This doesn't weaken security - it just lets cloudflared talk to its tunnel endpoints while WARP protects everything else.
