# RTL8821CE Bluetooth Not Detected

## Overview

This document explains a common issue where **Bluetooth disappears after a reboot** on systems using the **Realtek RTL8821CE** Wi-Fi/Bluetooth combo card.

The problem is **not caused by configuration changes, drivers, or system updates** — it is a known hardware/firmware behaviour affecting several laptops.

---

## TL;DR

> If Bluetooth suddenly vanishes on an RTL8821CE system:
> 
> 👉 **Power off completely. Don't reboot.**

---

## Symptoms

After rebooting:

- Bluetooth is missing
- `bluetooth.service` does not start
- No Bluetooth devices appear
- `/sys/class/bluetooth` is empty
- `systemctl status bluetooth` shows the service skipped or inactive

---

## Misleading Kernel Warning

You may notice an unrelated warning such as:

i915 WARN

This is **Intel Type-C PHY initialization noise** and **not related** to the Bluetooth failure.

---

## The Real Cause

Check `dmesg` output:

usb 1-10: new full-speed USB device number 4 using xhci_hcd  
usb 1-10: new full-speed USB device number 5 using xhci_hcd  
usb 1-10: new full-speed USB device number 6 using xhci_hcd  
usb 1-10: new full-speed USB device number 7 using xhci_hcd

### What this means

- USB port **1-10** repeatedly resets
- Device never successfully enumerates
- No Bluetooth HCI interface is created
- `bluetooth.service` correctly skips startup

Port `1-10` corresponds to the **internal USB Bluetooth interface** of the RTL8821CE card.

---

## Why It Happens "Without Changes"

This is a known **RTL8821CE warm-reboot hang**.

Example shutdown log from a working session:

bluetoothd: Failed to set mode: Authentication Failed (0x05)  
bluetooth.service: Deactivated successfully.

The Bluetooth firmware becomes **wedged before shutdown**.

### Key Problem

During a **warm reboot**:

- The internal USB hub remains powered
- The Bluetooth chip is **not power-cycled**
- The device keeps its broken internal state
- Enumeration fails repeatedly

Nothing in software changed — the hardware never reset.

---

## Affected Hardware

Commonly reported on:

- RTL8821CE
- RTL8723DE

Often found in:

- HP 15s laptops
- Lenovo IdeaPad series

This issue appears repeatedly in kernel bug reports and is considered a **firmware/silicon limitation**, not a Linux driver bug.

---

## Immediate Fix (Reliable)

Perform a **full power shutdown**, not a reboot:

sudo poweroff

Then:

1. Wait ~10 seconds
2. Allow motherboard capacitors to discharge
3. Power the system back on

Bluetooth should return normally.

---

## Workarounds Without Powering Off

### 1. Rebind the USB Device

Force the internal USB port to reset:

```bash
echo '1-10' | sudo tee /sys/bus/usb/drivers/usb/unbind  
echo '1-10' | sudo tee /sys/bus/usb/drivers/usb/bind
```

---

### 2. Power-Cycle the USB Controller

```bash
echo 0 | sudo tee /sys/bus/pci/devices/0000:00:14.0/power/control  
echo auto | sudo tee /sys/bus/pci/devices/0000:00:14.0/power/control
```

---

## Long-Term Mitigation

You can reduce the chance of the device becoming stuck by unloading Bluetooth drivers cleanly during shutdown.

### Create Systemd Shutdown Service

Create:

```bash
/etc/systemd/system/btusb-clean-shutdown.service
```

```bash
[Unit] 
Description=Cleanly unload btusb before shutdown
DefaultDependencies=no
Before=shutdown.target reboot.target halt.target

[Service]
Type=oneshot
ExecStart=/sbin/modprobe -r btusb btrtl

[Install]
WantedBy=shutdown.target
```

Enable it:

```bash
sudo systemctl enable btusb-clean-shutdown.service
```

---

## Important Notes

- This **does not permanently fix** the hardware issue.
- A **cold power cycle** remains the only guaranteed recovery.
- The systemd hook only reduces the likelihood of recurrence.

---

## Summary

| Situation     | Result                  |
| ------------- | ----------------------- |
| Warm reboot   | Bluetooth may disappear |
| Full shutdown | Bluetooth restored      |
| USB rebind    | Sometimes works         |
| Shutdown hook | Preventative mitigation |


