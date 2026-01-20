# Silence rtw88 Firmware Spam (RTL8821CE)

This a **clean, supported fix** for suppressing excessive kernel log spam produced by the `rtw88` Wi-Fi driver on systems using the **Realtek RTL8821CE** chipset.

##### The issue manifests as repeated kernel messages such as:

```bash
rtw_8821ce 0000:01:00.0: unhandled firmware c2h interrupt
```

These messages are **harmless**, but they can:

- Spam the TTY during boot
- Break clean text-based boot splash screens (e.g. Plymouth)
- Flood `dmesg` / `journalctl -k`

---

## Affected Hardware

- **Chipset:** Realtek RTL8821CE
- **Driver:** `rtw88` (in-tree kernel driver)
- **PCI ID:** `10ec:c821`

---

## Root Cause

The `rtw88` driver logs certain **firmware C2H (Command-to-Host) events** at warning level, even though they are non-fatal and expected on this chipset.

This is **debug-level noise**, not a hardware or driver failure.

---

## Correct Solution (Upstream-Supported)

The `rtw88` driver provides a `debug_mask` module parameter that allows disabling this firmware debug output cleanly.

### ✅ Fix

Create a modprobe configuration file:

```bash
sudo nano /etc/modprobe.d/rtw88.conf
```

Add the following content:

```bash
# Silence rtw88 firmware spam (RTL8821CE)
options rtw88_core debug_mask=0
```

Apply the Change

Rebuild the initramfs so the option is applied early during boot:

```bash
sudo update-initramfs -u
```

Then reboot:

```bash
sudo reboot
```

---

Verification

After reboot, confirm that the spam is gone:

```bash
dmesg | grep rtw_8821ce
```

Expected output should be limited to normal initialization messages, for example:

```bash
rtw_8821ce 0000:01:00.0: enabling device
rtw_8821ce 0000:01:00.0: Firmware version X.Y.Z
```

##### No repeated firmware c2h interrupt messages should appear.

---

**Notes**

> This fix does not disable the driver
> 
> It does **NOT** hide real errors
> 
> It **IS** safe across kernel updates




