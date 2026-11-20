# Performance: measurements, display analysis, optimizations

## Hardware numbers (hw:unit1, compat image, 2026-11-18)

| Quantity | Hardware | Image |
|---|---|---|
| power switch -> diagnostic screen (stopwatch) | about 10 s | rc5 |
| of which Linux (kernel start -> init scripts finished) | 4.04-4.09 s over four boots | rc5 |
| of which before Linux (Raspberry Pi firmware stage), by subtraction | roughly 5-6 s, **not yet optimised** | rc5 |
| memory in use shortly after boot | 54-59 MB of 464 MB | rc2-rc5 |
| display: full-frame updates reaching the panel | 41.0 per second (SPI at 41.7 MHz allows ~47) | rc5 |
| power, diagnostic page, screen on, idle | about 1.15 W (284 mA at 4.04 V, one fuel-gauge reading) | rc2 |
| rough battery life at that draw | about 6 h from the reported 1750 mAh | rc2 |

Reading against the project's goals: the display link, not software, limits frame rate
(41 of a possible ~47 frames/s); Linux start-up is now the smaller half of the boot time.

There is no stock-image measurement from this unit, so nothing here is a comparison with
the stock system yet. The power figure is one fuel-gauge reading, not an average.


## Read this first

- The table directly above is the only hardware data. **Every other number below was
  measured under QEMU** (`docs/TESTING.md`), on a Pi 3B machine model with TCG emulation.
  Those are valid for *comparing configurations with each other* (memory, process count,
  wakeups, writes), not as boot times, CPU loads or power figures of a Cardputer Zero.
- There are **no measurements of the stock M5Stack image**. The stock column further down
  is assembled from its build recipe (what is installed and enabled), not from measurement.

## Instrumentation

| Quantity | Tool | In emulation | Needs hardware |
|---|---|---|---|
| Kernel start -> init, per-init-script time, init complete | `nz-perf` (`dmesg`, `/run/nz/rcS-times`), `systemd-analyze` | relative only | absolute values; firmware + bootloader time needs a stopwatch/UART timestamps |
| Memory (used, slab, userspace PSS/RSS) | `nz-perf` | yes | CMA/GPU split on the real firmware |
| Running processes, loaded modules | `nz-perf`, `ps` | yes | module set differs (board drivers load) |
| CPU idle %, context switches, interrupts, forks per second | `nz-perf -i N` | yes for userspace-caused wakeups | interrupt rate of real peripherals; cpuidle residency (no cpuidle driver exists in QEMU's machine) |
| Storage activity | `nz-perf` (`/proc/diskstats`) | yes | – |
| Application start latency | `nz-diag` (exec -> first frame), `nz-perf` (fork+exec) | relative only | absolute |
| Display throughput | `nz-diag --bench` (mmap vs `write()`), page 4 key `B` | framebuffer memcpy speed only | SPI flush rate, tearing, real frame rate |
| Input latency | `nz-diag` (key event -> frame drawn) | software path only | panel latency |
| Power-management interfaces | `nz-perf` (`pm_*` fields) | lists what exists | everything |
| Suspend/resume | – | – | BCM283x kernels expose no `mem` state; "screen off + idle" is the available low-power mode |

## Measurements (v0.1.0-rc1, QEMU)

"idle" = login prompt only, nothing drawing on the LCD, 40 s settle, 30 s sample.
"interrupts/s" is dominated by the emulated machine's timer/USB polling and is not
meaningful for the device. Times are emulator times.

| metric | minimal, idle | compat (A), idle | compat (A), native launcher | compat (A), APPLaunch 0.6.106 | compat_systemd (B), idle | compat_systemd (B), APPLaunch 0.6.106 |
|---|---|---|---|---|---|---|
| RAM used (MemTotal-MemAvailable), kB | 38992 | 39008 | 40336 | 48188 | 46276 | 59024 |
| userspace PSS, kB | 4484 | 7676 | 7732 | 24984 | 21015 | 42873 |
| kernel slab, kB | 26924 | 16916 | 16980 | 21108 | 20040 | 25376 |
| userspace processes | 11 | 16 | 17 | 19 | 11 | 15 |
| loaded modules | 22 | 22 | 22 | 22 | 24 | 24 |
| CPU idle % | 99.29 | 99.27 | 99.36 | 97.58 | 99.22 | 97.6 |
| context switches/s | 32.5 | 24.7 | 23.9 | 195.0 | 24.5 | 201.5 |
| interrupts/s | 13268.5 | 11012.7 | 13194.8 | 13150.1 | 13641.7 | 12497.9 |
| forks/s | 0.47 | 0.47 | 0.47 | 0.93 | 0.47 | 0.8 |
| SD writes kB/s (30 s window) | 0.33 | 1.2 | 1.6 | 4.67 | 1.6 | 7.73 |
| SD written since boot, kB | 424 | 1381 | 1421 | 105910 | 3903 | 110873 |
| kernel start -> init, s (emulated) | 1.75825 | 1.878997 | 1.908229 | 1.995144 | 1.856372 | 1.87725 |
| init scripts done, s (emulated) | 8.67 | 13.83 | 13.97 | 16.13 | 28.17 | 28.25 |
| rootfs used, kB | 37983 | 88600 | 88600 | 189928 | 111092 | 220612 |
| fork+exec /bin/true, ms (emulated) | 42.0 | 43.0 | 54.4 | 40.8 | 38.6 | 47.4 |

APPLaunch process alone: PSS 17.3 MB (A) / 17.9 MB (B), 12 threads.

Reading the table:

- **minimal vs compat (idle):** the compat image carries dpkg, D-Bus, eudev and the extra
  libraries, but none of that runs at idle except `dbus-daemon` and `udevd`.
- **native launcher vs APPLaunch:** APPLaunch redraws continuously (LVGL timer loop) and
  runs a dozen threads; the native launcher sleeps in `poll()`.
- **config A vs config B:** see `ARCHITECTURE-DECISION.md`.

## The internal LCD and the "30 Hz" mode

Findings from source (`cardputerzero-v5-overlay.dts`, `panel-mipi-dbi-m.c`, kernel 6.18
`drm_mipi_dbi.c`, `drm_fbdev_dma.c`, `spi-bcm2835.c`); **none confirmed on hardware**:

1. **Where 30 Hz comes from.** The overlay's `panel-timing` has `clock-frequency = 1968000`
   with 320 x (170+35) pixels: 1 968 000 / 65 600 = 30.0 Hz. A MIPI-DBI panel has no pixel
   clock; the driver says so itself ("the driver doesn't use the pixel clock"). The value
   only determines the refresh rate DRM *reports*. It is a device-tree property, not an OS
   property, and changing the OS does not change it.
2. **What actually limits frame rate: SPI.** One full frame is 320x170x16 bit = 870 400 bit.
   The overlay asks for 60 MHz. The BCM2835 SPI driver rounds the divider up to an even
   number: with `enable_uart=1` the core clock is fixed at 250 MHz, giving 250/6 =
   **41.7 MHz -> at most ~47 full frames/s**; without the mini-UART the core clock is
   400 MHz, giving 400/8 = 50 MHz -> ~57 frames/s. Transfers of 96 bytes or more use DMA, so
   CPU cost per frame is small. A 60 Hz *full-frame* mode is therefore not reachable on this
   bus at this clock; 30 Hz leaves headroom. (Core-clock values are the documented Pi 3 /
   Zero 2 behaviour; not measured on a CM0.)
3. **fbdev clients are capped near 20 fps.** DRM's fbdev emulation flushes mmap'd writes
   through deferred I/O with a fixed delay of `HZ/20` = 50 ms. APPLaunch, the M5 apps,
   `nzui.h` and `nz-diag` all draw through the mmap'd framebuffer, so they cannot exceed
   ~20 flushes per second whatever the panel mode says.
4. **Ways to go faster, in order of effort** (all need hardware to evaluate):
   - `write()` to the framebuffer instead of mmap: the damage is queued immediately instead
     of after 50 ms. `nz-diag --bench` measures both paths (`NZ_FB_WRITE=1` selects it).
   - Use DRM/KMS directly (dumb buffer + `DRM_IOCTL_MODE_DIRTYFB` or page flips): each
     commit is sent synchronously; the driver completes the vblank event right after the
     transfer, so the ceiling is the SPI rate (~47 fps), and partial `DIRTYFB` rectangles
     transfer only the changed area.
   - Raise `panel-timing/clock-frequency` (e.g. 3 148 800 for 48 Hz) if a KMS client paces
     itself on the reported mode.
   - Drop `enable_uart=1` on a finished system to get the 50 MHz SPI clock.
   None of these is applied in the images; the stock timing and overlay are used unchanged.

## Optimizations implemented (and why)

| Change | Evidence / reason | Where |
|---|---|---|
| Kernel modules built: ~1330 (bcm2711_defconfig) -> 453 configured, ~290 installed | allowlist; DVB/tuner/HAT drivers can never be used; module tree 27 MB -> 22 MB; shorter builds | `scripts/gen-kernel-fragment.py` |
| I2C and SPI bus drivers built in; display path loaded before coldplug | LCD does not wait for `mdev`/`udev` module loading (coldplug was the longest init step: 1.9 s mdev / 4.1 s eudev, emulated) | kernel fragment, `S00nz-early` |
| Modules uncompressed | no decompression on a 1 GHz A53; BusyBox needs no codec | kernel fragment |
| `gpu_mem=32`, no `vc4-kms-v3d`, no HDMI stack | stock reserves 128 MB + 64 MB CMA for a desktop compositor that is not used here | `config.txt` |
| No cron daemon | nothing scheduled; removes a once-a-minute wakeup | `post-build.sh` |
| syslog to a 256 kB RAM ring (`logread`) instead of files | no log writes to the SD card | `post-build.sh` |
| Root mounted `noatime`; ext4 inode tables and journal initialised at build time | first-boot background writes: 12.9 MB written by the time the system idled before the change, 0.4 MB after (minimal image, emulated) | `post-build.sh`, `common.frag` |
| Console supervisor sleeps 60 s on a missing UART/USB-serial port instead of respawning a getty | BusyBox init would otherwise fork once a second forever | `nz-console` |
| Boot partition mounted read-only; boot log written in the background 4 s after init, only while `nz-debug` exists | no FAT writes in normal use; never delays the UI | `S00nz-early`, `S99nz-bootlog` |
| Wi-Fi/DHCP/USB-gadget setup in the background | console is not blocked by network bring-up | `S45nz-net` |
| Launcher: backlight off after 120 s idle, no redraw while off, one wakeup per minute while on | screen-off is the only real low-power state on this SoC | `nz-launcher.c` |
| Native launcher instead of a permanently animating UI | see context-switch rows in the table | architecture |

Considered and **not** done: switching the CPU governor (rpi default `ondemand` kept – no
data), lowering `arm_freq`, disabling cores, zram, read-only root. Each trades something
and should be decided with current measurements from the device.

## Stock software stack (reference data, not measured)

From `CardputerZero/pi-gen` (`stage2/05-cardputerzero`, `docs/CARDPUTERZERO-IMAGE-DESIGN.md`):
Raspberry Pi OS **desktop** (stages 0-4, X/Wayland session, about 1720 packages according to
M5Stack's own note), systemd, NetworkManager, PipeWire + rtkit, plymouth splash, `adbd`,
`fb-mirror.py`, APPLaunch as a user service, `gpu_mem=128`, `vc4-kms-v3d,cma-64`,
build-essential and kernel headers kept on the image. A Raspberry Pi OS desktop image is
several GB on disk. No boot-time, memory or power figures for it are available to this
project; obtaining them is step E2 of the hardware checklist.
