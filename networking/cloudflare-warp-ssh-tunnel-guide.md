## VPS Secure Shell Access via Cloudflare Warp Debian 13 (trixie)<br>
##### **Prerequisites**

- **Cloudflare Zero Trust account** created and configured.

- **Cloudflare WARP Client** installed on the device that will access the private network.

- **Administrator / root access** on the target device.

- **Login capability to the Cloudflare Dashboard** (Zero Trust → Networks).

---

### Cloudflare Split Tunnels
> Ensure traffic routes correctly through WARP + Tunnel.

##### Include Mode

- **Only specified IP ranges or domains are routed through WARP.**

- All other traffic bypasses the tunnel and uses the regular internet connection.

- Useful for secure access to **private internal networks** or **specific resources only**.

- Example: include `10.0.0.0/24` → only internal systems route through Cloudflare.

##### Exclude Mode

- **All traffic goes through WARP by default**, except for the IP ranges or domains you exclude.

- Excluded traffic stays local or uses the public internet normally.

- Common for **local LAN access** or **latency-sensitive services** like gaming or streaming.

- Example: exclude `192.168.1.0/24` so local devices remain accessible.

--- 
<br>

### Install Latest Cloudflared Version on Target

> Login to target (ssh root@target) Leave the session open as a fail-safe until the setup is complete.
<br>

```bash
wget https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb
```

```bash
dpkg -i cloudflared-linux-amd64.deb
```
<br>


#### Verify Install and Login

```bash
cloudflared --version
```

> cloudflared version 2025.11.1 (built 2025-11-07-16:59 UTC)  
<br>

```bash
cloudflared login 
```

> ⚠️ **Action Required:**  
> Cloudflared will display a unique login URL in your terminal.  
> **Copy the link and open it in your web browser to complete authentication.**  
> The setup cannot continue until the device is authorised.
> 
> ---
> 
> **Copy and paste the link into your browser, Leave the terminal session open.**
> 
> > Please open the following URL and log in with your Cloudflare account:
> > 
> > https://dash.cloudflare.com/argotunnel?aud=&callback=https%3A%2F%2Flogin.cloudflareaccess.org0VjOjwpFa26ZyqbtKn2P8ZW4LE0TeZ9mKXFStMeckWqCzGfMF
> > Leave cloudflared running to download the cert automatically.
> 
> **On successful login**
> 
> > 2025-11-16T22:06:31Z INF You have successfully logged in.
> > If you wish to copy your credentials to a server, they have been saved to:
> > /root/.cloudflared/cert.pem

<br>

Create a tunnel, Replace [TUNNEL_NAME] with your desired name. i.e my-cloud

```bash
cloudflared tunnel create [TUNNEL_NAME]
```

Note the created tunnel ID, You will need it for creating config.

##### Expected Output of tunnel create command:

> Tunnel credentials written to /root/.cloudflared/[TUNNEL-ID].json. cloudflared chose this file based on where your origin certificate was found. Keep this file secret. To revoke these credentials, delete the tunnel. 
> 
> Created tunnel [TUNNEL_NAME] with id [TUNNEL-ID]  <-- Note this generated ID.

##### 

##### Create and Edit Configuration File

```bash
mkdir -p /etc/cloudflared
nano /etc/cloudflared/config.yml
```

##### Config File Contents

----------------------------------------------------
```
 tunnel: [TUNNEL-ID]
 credentials-file: /root/.cloudflared/[TUNNEL-ID].json
 
 ingress:
   - service: http_status:404
```
----------------------------------------------------

Save and Exit.



##### Setting Up the service

```
cloudflared service install
```

##### Expected Output if Successful

> 2025-11-16T16:53:27Z INF Using Systemd<br>
> 2025-11-16T16:53:28Z INF Linux service for cloudflared installed successfully


##### Create a route for the tunnel

```
cloudflared tunnel route ip add 10.0.0.1/32 [TUNNEL_NAME]
```

###### Expected Output

> Successfully added route for 10.0.0.1/32 over tunnel [TUNNEL-ID]

##### Add CIDR IP to local adaptor

⚠️ **Important:**  
You must add the Cloudflare CIDR IP range to the Netplan network interface configuration.  
Without this entry, routing may fail and connectivity issues may occur.

```
nano /etc/netplan/50-cloud-init.yaml
```

##### Add your chosen CIDR IP

---

 network:<br>
   &nbsp;&nbsp;&nbsp;&nbsp;version: 2<br>
   &nbsp;&nbsp;&nbsp;&nbsp;ethernets:<br>
     &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;all-en:<br>
       &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;match:<br>
         &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;name: "en*"<br>
       &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;dhcp4: true<br>
       &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;dhcp6: true<br>
       &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<mark>addresses:</mark><br>
         &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<mark>- 10.0.0.1/32</mark><br>

------------------------------------

Save and Exit.

##### Apply the address to Netplan

```
netplan apply
```

##### Check the address was added successfully

```
ip addr show
```

---

 1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1000<br>
     &nbsp;&nbsp;&nbsp;&nbsp;link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00<br>
     &nbsp;&nbsp;&nbsp;&nbsp;inet 127.0.0.1/8 scope host lo<br>
        &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;valid_lft forever preferred_lft forever<br>
     &nbsp;&nbsp;&nbsp;&nbsp;inet6 ::1/128 scope host noprefixroute<br> 
        &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;valid_lft forever preferred_lft forever<br>
 2: ens6: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP group default qlen 1000<br>
     &nbsp;&nbsp;&nbsp;&nbsp;link/ether 02:01:f2:0c:a8:ed brd ff:ff:ff:ff:ff:ff<br>
     &nbsp;&nbsp;&nbsp;&nbsp;altname enp0s6<br>
     &nbsp;&nbsp;&nbsp;&nbsp;altname enx0201f20ca8ed<br>
     &nbsp;&nbsp;&nbsp;&nbsp;inet <mark>10.0.0.1/32</mark> scope global ens6<br>
        &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;valid_lft forever preferred_lft forever<br>
     &nbsp;&nbsp;&nbsp;&nbsp;inet 87.106.36.9/32 metric 100 scope global dynamic ens6<br>
        &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;valid_lft 592sec preferred_lft 592sec<br>
     &nbsp;&nbsp;&nbsp;&nbsp;inet6 fe80::1:f2ff:fe0c:a8ed/64 scope link proto kernel_ll<br> 
        &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;valid_lft forever preferred_lft forever<br>

---

You Should see your CIDR IP attached to your device.

##### Setup Routes and Targets in the Zero Trust Dashboard

1. **Navigate to the Tunnels Page**  
   Go to **Networks → Tunnels** in the Cloudflare Zero Trust dashboard. 
   There, you should see your tunnel listed. 
   Check its status — it should show as running and healthy.



2. **Create a Policy**
   
   **Navigate to Access → Policies**. Add or Edit an Existing Policy
   
   - Give the Policy a Name (e.g. Allow SSH)
   
   - Set Action to Allow
   
   - Session Duration (your preference)
   
   - Add at Least one Include Rule (i.e. EMAILS)
   
   - Additionally add Warp Authentication (your warp client needs to be enrolled)
     
     > Add Require
     > Selector = Warp    Value = Warp



3. **Create a CIDR Route**
- Next, switch to **Networks → Routes**. Click on **Create CIDR route**.

- Enter the CIDR IP (e.g. 10.0.0.1/32 or Block (e.g. 10.0.0.0/24) that you want to tunnel.

- For the “Tunnel” dropdown, select the tunnel created earlier.

- Save / Apply.



4. **Add a Target for Your Network**
- Then, go to **Networks → Targets**

- Click **Add Target**.

- Give it a **Hostname** (e.g. `MY_SERVERNAME` or `MY_SERVICE`).

- In the “IPv4” dropdown select the **CIDR 10.0.0.1/32** 

- Save / Apply.
  

Test SSH Access

```bash
ssh root@10.0.0.1
```

On successful login.
```
 Linux master-database 6.12.57+deb13-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.12.57-1 ($DATE$) x86_64
 The programs included with the Debian GNU/Linux system are free software;
 the exact distribution terms for each program are described in the
 individual files in /usr/share/doc/*/copyright.
 Debian GNU/Linux comes with ABSOLUTELY NO WARRANTY, to the extent
 permitted by applicable law.
 
 root@10.0.0.1:~#
```


5. **Additional Security (optional)**

Bind SSH Listen Address to the CIDR IP and close public ports

```
nano /etc/ssh/sshd_config
```

Adjust the SSH `ListenAddress` setting to restrict which interface the SSH daemon will bind to.
```
 #ListenAddress 0.0.0.0
 ListenAddress 10.0.0.1
```
Restart SSH

```
systemctl restart ssh.service
```

Test access `ssh root@10.0.0.1` 

On Success you can now close public facing port for ssh access (optional).



6. **Edit client ssh config (optional)**

```
nano ~/.ssh/config
```

> #Name the Connection<br>
> 
> Host connection-Name<br>
>     Hostname 10.0.0.1<br>
>     User root<br>

Save and Exit



Login with connection name.

```
ssh connection-name
```

On successful login

> Linux master-database 6.12.57+deb13-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.12.57-1 (DATE) x86_64<br>
> The programs included with the Debian GNU/Linux system are free software;<br>
> the exact distribution terms for each program are described in the<br>
> individual files in /usr/share/doc/*/copyright.<br>
> Debian GNU/Linux comes with ABSOLUTELY NO WARRANTY, to the extent<br>
> permitted by applicable law.<br>
> root@connection-name:~#
