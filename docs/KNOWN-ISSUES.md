# Known issues, limitations and remaining risks

## v0.1.0: what does not work

Evidence: **hw** = reported from a real device by the project owner (unit 1: Cardputer
Zero; unit 2: Cardputer Zero Lite); **emu** = emulator only. Numbers refer to `BUG-LIST.md`.

| Area | Issue | Evidence |
|---|---|---|
| Apps | 55 of the 101 apps in M5Stack's app index cannot be installed: they need CJK fonts, pygame, systemd, a Wayland desktop, PipeWire, NetworkManager, BlueZ, Node.js or other Debian packages. The Store names what is missing (B9). | emu, index copy of 2026-11-19 |
| Apps | An app that opens an SDL window shows nothing (for example `tic80-c0`): SDL2 has no display driver here. The Store still lists such apps as installable (B11). | emu |
| Apps | `fontpreview` installs and then exits: it needs the Noto CJK fonts and does not declare that (B13). | hw unit 1, emu |
| Apps | Recorder (official M5Stack app) records and saves a file but cannot play it back. Cause unknown (B14). | hw unit 1 |
| Apps | AdvanceWebUI downloads but does not start. Cause unknown (B3). | hw unit 1 (rc7) |
| Sound | Cardputer TV: sound lags the picture, more than on the stock firmware (B15). | hw unit 1 |
| Clock | The clock shows UTC until a time zone is chosen under Options. The first start does not ask for one. | hw unit 1 |
| Network info | Its Wi-Fi section says "not running" because the app runs as the normal user (B10). | source reading |
| M5Stack launcher | Crashes now and then in the emulator after keyboard navigation; never seen on a device (B8). | emu |
| Missing features | Bluetooth; a camera app (`v4l2-ctl` and ffmpeg are present); GPU acceleration (OpenGL ES); CJK fonts. | by design for this release |
| USB-C port | Charging only. A PC does not see the device. | hw unit 1 |

Not verified on any device: the display fallback for a unit whose panel gives no
synchronisation pulses; long runs, heat and battery life with the faster display link; a
third unit; a Compute Module 0 with the Synaptics 43436s Wi-Fi chip (unit 1 has a 43439;
the firmware files for both are in the image). The 100 MHz display clock has been run on
two devices only; the way back is in `DISPLAY.md`.

Everything below is the project's history: results of the release candidates on hardware,
defects found and fixed, and the risk lists written before each first boot.

## Hardware results so far

All from **one unit** (CM0 Rev 1.0, 512 MB, full model with camera/IMU/IR), reported by the
owner as boot logs (kept by the maintainer), photos of the diagnostic pages and written
observations. Evidence tag **hw:unit1**. "Bound" means the driver attached; it is not
"seen working".

| Image | Result |
|---|---|
| rc1 | booted, LCD dark, Wi-Fi chip failed to start |
| rc2 | **LCD works** (upright, colours correct by eye), all 16 hardware probes pass, Wi-Fi chip starts (`wlan0`), Fn layer and layer LEDs work, launcher works, Snake "smooth", kill chord works, backlight idle-off works, power switch turns the unit off |
| rc3 | booted twice, **unusable**: blank screen for 10 s on the first start and 24 s on the second; the owner could not reach the launcher. Cause: defect L below. The logs do confirm on hardware: read-only root, Sym table loaded (0 errors), LCD driver up at 1.7 s and console on it at 2.0 s (was 3.7 s), log daemon running |
| rc4 | booted, **LCD dark** for over a minute although the log shows the system fully up (all 16 probes pass, data partition created correctly at 14 GB in 6.5 s). Cause: defect M below; it also explains the dark screen in rc3 |
| rc5 | **works** (5 boots logged). LCD lit; Sym keys all correct; Wi-Fi setup joined a network and did not save a wrong passphrase; SSH over Wi-Fi; sound output; charging; four power-switch cycles with no filesystem recovery needed afterwards; data partition 14 GB; panel benchmark 41.0 full frames/s. Defects: N and O below |
| rc6 | works. `nz-m5 install IR-Remote` downloaded and installed the upstream M5Stack package on the device; the app runs from the native launcher; it recorded an infrared signal and sent it (IR receive and transmit confirmed). Defect: P below |
| rc7 | works. No log text over a graphical app; Snake and Diagnostics run; the power-off instruction is readable; microphone records (`arecord`); normal boot takes about 7 s from the switch (the 10 s reported for rc5 was a first start). M5 launcher installed with `nz-m5 install-launcher` runs with no crash seen, most of its built-in apps work, its Store connects; several Store apps are refused for missing dependencies; AdvanceWebUI downloads but does not start; with the M5 launcher installed the diagnostic screen is drawn on top until it is left. A PC detects nothing on the USB-C or USB-A port. **Removing the M5 launcher and switching off left the card unbootable** (BUG-LIST B1) |
| rc8, rc9 | never booted on hardware (replaced by rc10) |
| rc10 (NimbleZero) | **works** (hw:unit1, owner report 2026-11-19). Checklist A all pass: start-up messages, first-boot password prompt, launcher running as the normal user on the real display, battery gauge. B1-B4 pass: F/X/Z/C keys, Options (brightness, screen-off, time zone entry), volume, Wi-Fi network and country. B5: clock is set from the network but shows UTC (to-do list). B6-B8 pass: SSH as `zero`, root refused, SSH off/on from Services, `sudo` with the password, Snake/Diagnostics/Terminal/Power off. Not yet reported: Store (C), the rc7 failure repeated (D), reflash (E) |
| rc11 | **works** on two devices (owner reports 2026-11-19). Unit 1: checklist A and B pass; Store C1-C5 and C7 pass, Recorder playback and `fontpreview` fail (B14, B13); data kept across a reflash; M5 launcher removed with `dpkg`, POWER switch, starts again (the rc7 failure does not recur); PulseAudio works (Cardputer TV with sound). Unit 2 (Lite): starts, 62 frames/s in the Diagnostics benchmark, no display problems, Doom from the test kit runs. Display measurements with rc10 + test kit on unit 1: `DISPLAY.md`. |

Measured (hw:unit1):

| Quantity | Value | Image |
|---|---|---|
| power switch -> diagnostic screen (stopwatch) | about 10 s | rc5 |
| kernel start -> init scripts finished | 4.04-4.09 s (four boots); kernel -> `/sbin/init` 1.15-1.17 s | rc5 |
| time before the kernel starts (firmware stage), by subtraction | roughly 5-6 s | rc5 |
| LCD driver up / console on it | 1.96-2.03 s / 2.28-2.33 s after kernel start | rc5 |
| first start, storage step (16 GB card) | 6.5 s (rc4), 18.3 s (rc5): varies | rc4, rc5 |
| panel update rate, full 320x170 frames over SPI | 41.0 frames/s | rc5 |
| memory in use ~9 s after boot (compat) | 54-59 MB of 464 MB | rc2-rc5 |
| battery draw, diagnostic page, screen on, CPU idle at 600 MHz, Wi-Fi not associated | 284 mA at 4.04 V = about 1.15 W (fuel gauge, one reading) | rc2 |
| battery | gauge reports 1750 mAh design capacity | rc2 |

Not yet observed on hardware: microphone, camera capture, IR, USB device mode on a PC,
the rc6 power-off message (not reported), anything new in rc7.

### Defects found on hardware and their status

| # | Found in | Defect | Cause | Status |
|---|---|---|---|---|
| A | rc1 | LCD dark | firmware framebuffer took `fb0`+console; nothing committed a modeset on the panel | fixed in rc2, **confirmed hw:unit1** |
| B | rc1 | Wi-Fi chip `HT Avail timeout` | CM0-specific CYW43439 NVRAM missing | fixed in rc2; chip starts (**hw:unit1**); joining a network not yet confirmed |
| C | rc1 | diagnostic said Bluetooth missing | probe bug | fixed in rc2, confirmed |
| D | rc1 | no `/dev/i2c-1`; camera mux driver not built | `i2c-dev` not loaded; `I2C_MUX_PINCTRL` off | fixed in rc2; camera sensor now detected (**hw:unit1**) |
| E | rc1, rc2 | `failed to set PGAL/PGAR Select` from the vendor audio driver at load | a mixer control it expects is absent | **open**; microphone routing suspect |
| F | rc2 | Sym layer types wrong characters | the vendor driver sends a private key code per symbol and expects userspace to translate; the image had no table | fixed in rc3 (`nz_keymap.h`, console keymap); all 32 pairs checked against the legends in the owner's photo; **confirmed on hardware (rc5): every symbol correct** |
| G | rc2 | Wi-Fi setup skipped the network-name prompt and saved a wrong network | keys pressed in the launcher stayed in the console's line buffer and were read as the first answer; the app also saved before connecting | fixed in rc3: buffer flushed when a UI hands over the console (**reproduced on rc2 and verified fixed in the emulator**: test read `[23a451rjk]` on rc2, `[]` on rc3); app rewritten to save only after the access point accepts, with a menu to replace/forget. **Confirmed on hardware (rc5)**: joined a network; a wrong passphrase was not saved |
| H | rc2 | "Power off" leaves the board powered with LEDs blinking | the CM0 cannot cut its own power; the switch does | rc3 leaves "Slide the POWER switch off" on the LCD |
| I | rc2 | (requirement) the power switch must always be safe | root filesystem was read-write | rc3: root is read-only, persistent data on its own partition (see `BOOT.md`); the vendor driver already remounts filesystems read-only 0.5 s after the switch moves |
| J | rc2 | display page text ran off the screen; benchmark number meaningless (memory copy only) | layout bug; fbdev writes are asynchronous | rc3: text shortened; benchmark now times blocking DRM updates. **The new benchmark has never run** (the emulator has no DRM device) |
| K | rc2 | LCD only starts ~1.5 s after its driver is requested | the SPI controller waited for `gpio_forwarder` (third chip select), loaded late | rc3 loads it first; **effect not measured** |

| L | rc3 | first start slow with a blank screen; switching off during it damaged the data partition (18 s journal recovery on the next start, then "100 % full") | rc3 shipped a small filesystem and grew it online in the background: ~25 s of heavy card I/O on a 16 GB card, and not safe to interrupt. Nothing was shown on the LCD meanwhile | rc4: no resize. The partition ships unformatted (MBR type DA) and is created at full size on first start, then marked done (type 83); a power cut before that repeats the step. No filesystem repair in the boot path: an unusable data partition means temporary storage and a message. "Starting..." is printed once the LCD is up. Emulator-verified, including a simulated interrupted first start and a destroyed data partition; measured on hardware in rc4: 6.5 s on a 16 GB card, filesystem correct afterwards |
| M | rc3, rc4 | LCD dark while every driver reports the display as on | rc3 loaded `gpio_forwarder` early (defect K), which starts the panel 0.13 s after the IO expander powers it instead of 1.7 s after (rc2). The panel evidently ignores its start-up commands that soon after power-up; the kernel cannot tell. This is an inference from the logs of three images (rc2 lit, rc3/rc4 dark, that order being the only display difference), not a measurement | rc5 restores the rc2 order exactly (K reverted). **Confirmed on hardware**: lit in all five rc5 boots, with the panel started 0.36-0.41 s after the IO expander, so the failure threshold lies between 0.13 s and about 0.4 s. The storage step moved after the display start (`S12nz-data`) |
| N | rc1-rc5 | `nz-m5 install ...` fails: "curl: protocol https disabled" | curl was built without a TLS library. Never caught because the emulator has no network, so `nz-m5` had never been run | rc6: OpenSSL added, curl/wget use it. Emulator test added (real TLS handshake and download against a local server); **confirmed on hardware (rc6)**: IR-Remote downloaded, installed and ran |
| O | rc3-rc5 | "Power off": the instruction flashes by, the launcher reappears, then the screen goes dark | `poweroff` returns at once, so the launcher redrew; and the kernel switches the LCD off when it halts, so nothing can remain on screen | rc6: the instruction is shown for 3 s *before* stopping ("When the screen goes dark, slide the POWER switch off") and the script holds the console. Unverified on hardware |
| P | rc1-rc6 | a graphical app's own log text is drawn over its screen (seen with M5Stack IR-Remote) | the launcher started graphical apps with the text console active and their output attached to it | rc7: graphical apps run with the console in graphics mode (blank, no cursor) and their output goes to `/tmp/nz-app.log`. Emulator-verified with a test app (screenshot blank, output in the log). Unverified on hardware |

## Not validated

1. **One unit only**, and rc7 has not been booted on hardware. On hardware nobody has yet
   typed a Sym key, used the new Wi-Fi app, run the panel benchmark, or seen the rc4
   first start; those are emulator- or source-checked only, as marked above.
2. **The one-command build has not been run on the owner's PC / WSL2.** All artifacts were
   built in a hosted Linux container. `./build.sh minimal` was run there once from an empty
   output directory (with `./dl` already populated): it completed, its image passed the
   structural check 60/60 and the QEMU boot test 11/11. `./build.sh compat`,
   `compat_systemd`, `fetch`, `sdk` and `NZ_OFFLINE=1` were not run through `build.sh`
   (the delivered compat images and the SDK were produced with the same `make` commands
   the script wraps).
3. **No power or battery-life data exists.** QEMU cannot provide it. No stock-image
   measurements exist either, so there is no stock baseline.

## Risks for first boot (most likely first)

4. **Board revision.** The image loads the "v0.5" overlays (`cardputerzero-v5-overlay`,
   `bq27220_v5`), which is what M5Stack's own image builder selects. If a unit is the older
   "v0.3" board the LCD/keyboard may not come up; the v3 overlays are on the boot partition
   (`cardputerzero-v3-overlay`, `bq27220_v3`, `spk-gpio24-high-overlay`, `bmi270_overlay`,
   `camera-gpio16-high-overlay`) and can be swapped in `config.txt`.
5. **No vendor `start.elf`.** Stock ships an M5Stack-built `start.elf`/`fixup.dat` (splash
   screen). This OS uses the standard Raspberry Pi firmware of a different date. If the
   vendor build contains CM0-specific behaviour beyond the splash, boot could differ.
6. **`gpu_mem=32` without KMS.** Stock uses `gpu_mem=128` + `vc4-kms-v3d,cma-64`. Untested
   on the CM0; the safe config enables `vc4-kms-v3d` and leaves `gpu_mem` at the default.
7. **`dwc2`, `imx219`, `vc4-kms-v3d` overlays** could not be dry-run with libfdt (a libfdt
   limitation that also affects the stock DTB/overlay pair); only their external references
   were checked.
8. **Kernel module allowlist.** A driver the board needs but nobody listed would be
   missing (this happened once: the camera I2C mux, see D above). `nz-logs/boot-N.txt`
   shows it.
9. **USB-C role.** How the board switches host/device mode is not visible in the overlay.
   On hw:unit1 `g_serial` bound to the device controller and a USB Ethernet adapter also
   enumerated; whether a PC sees the serial gadget was not reported.

## Functional gaps

10. APPLaunch: intermittent segfault when entering some pages (seen in 7 of 10 full lifecycle runs; more often when the emulator host is busy, so it is timing-dependent),
    not root-caused. Store, Wi-Fi/Bluetooth settings and the in-UI updaters do not work
    (need APT repositories, polkit, NetworkManager, BlueZ). Launching an installed app
    *from inside APPLaunch* was not tested.
11. Five of seven examined M5 app packages are refused at install for `libpulse0`/`ffmpeg`
    dependencies (see `M5-COMPAT.md`).
12. Bluetooth has no userspace (no BlueZ). Camera has no libcamera. IR has no `ir-ctl`.
    Audio is plain ALSA.
13. The system partition is a fixed 256 MB (minimal) / 768 MB (compat) and read-only; packages
    install into it through `nz-rw`. The data partition is created at the full size of the
    card on first start (emulator-verified on a padded image; rc3's different mechanism
    failed on a real card, see L). `/home`,
    `/root` and `/var/lib` are writable (data partition); programs that expect to write
    below `/usr`, `/etc` or `/opt` at run time will fail. APPLaunch keeps settings in `/var/lib/applaunch`; other M5Stack
    apps were only checked as far as the lifecycle suite goes.
13a. After installing, upgrading or removing a package while one of its programs is still
    running, the system partition can stay writable until that program exits (the kernel
    refuses to close a partition with deleted-but-open files). `nz-rw` keeps retrying for
    30 minutes. In one of three test runs it was still writable 40 s after the last
    package operation and the process responsible was not identified. The power switch
    forces the read-only remount regardless.
14. Root login with a published default password over SSH. Bench use only.
15. No time source at boot other than the RTC driver; no NTP client is started.
16. The native launcher shows at most 96 entries, no icons, no categories; the kill chord
    (Ctrl+Alt+Backspace) relies on Ctrl and Alt being reported by the keyboard driver.
17. Config A's `systemctl` shim implements a small verb set and no polkit; `nz-unit` has no
    dependency ordering.
18. **`compat_systemd` is still the rc1 build** (dark LCD on hardware, read-write root). It
    was a comparison build and has not been carried forward. Config B (systemd) boots and runs APPLaunch as a user service, but its remaining
    tests were lighter than config A's lifecycle suite; `/boot` logging units were not
    reviewed for ordering on real hardware. In its test runs `/boot` was found mounted
    read-write and `/` without `noatime` (one failing check per run); not fixed.

## Reproducibility

19. Builds are input-pinned but not bit-reproducible (timestamps, ext4 UUID).
20. `deb.debian.org` only keeps the current dpkg point release; if 1.22.22 is superseded
    the dpkg download needs a `snapshot.debian.org` URL or a copy in `dl/`.
21. Early in development, before network access was opened, some sources were rebuilt from
    Ubuntu/GitHub equivalents. Those builds were discarded; every delivered image was built
    from sources that match the pinned hashes.

## rc7: removing a running program, then the POWER switch, leaves a card that does not start (hw:unit1 report + emu reproduction)

- Report (hw:unit1, rc7): `dpkg -r applaunch` with the M5 launcher installed; the device did not start afterwards.
- Reproduced in the emulator on the rc7 compat image (emu:qemu-raspi3b, results not hardware evidence):
  install APPLaunch 0.6.106, start it, `dpkg -r applaunch` while it runs, then the switch-off sequence
  (emergency remount read-only, power cut). Next boot: `EXT4-fs error (device mmcblk0p2):
  ext4_mark_recovery_complete: Orphan file not empty on read-only fs`, `mount failed`, kernel panic
  (no root filesystem). The same sequence ended with `poweroff` instead of the switch boots normally.
- Mechanism: the removed program keeps its deleted files open, so the system partition cannot return to
  read-only (issue 13a). The switch then forces it read-only with deleted-but-open files recorded; the kernel
  refuses to mount that read-only at the next start. One boot with `rw` on the command line starts, but
  the following `ro` boot fails again; a file system check or a reflash is needed.
- Affects any removal or upgrade of a program that is running, followed by the switch. Not yet fixed.
- `tests/qemu-boot.py`: `NZ_HARD_OFF=1` (switch-off sequence instead of poweroff), `NZ_SAVE_DISK=path`
  (keep the card contents for a second boot), `NZ_ROOT_RW=1`.


## rc8 (NimbleZero): what is new and unverified

See `RELEASE-NOTES.md` for the changes and the emulator results, `BUG-LIST.md` for open
bugs. Naming: commands are `nz-*` since rc8; reports above about rc1-rc7 concern the same
programs under their `cz-*` names. Items 12 (no `ir-ctl`, plain ALSA), 14 (root login with a
published password) and 15 (no NTP) of the list above no longer apply to rc8: PulseAudio is
included, root cannot log in, the clock is set from the network. Item 11 is resolved for
the five M5Stack packages named there (emulator). Item 13a is addressed (BUG-LIST B1/B2).
New limits in rc8:

22. Nothing of rc8 has run on hardware. The launcher as an unprivileged user on the SPI
    display is the largest untested change.
23. ffmpeg is 6.1.5, Debian 13 has 7.1: programs linked against Debian's libav libraries
    do not load; `ffplay` is absent.
24. The data partition must keep its start sector in later releases, or a reflash can no
    longer adopt it. A `minimal` image over a `compat` card (or the reverse) formats it.
25. rc8 has no Python; rc9 adds it (with Pillow, NumPy, evdev, qrcode, requests). 69 of 101
    index apps still cannot be installed (BUG-LIST B9). libjpeg is present as `libjpeg.so.8`,
    Debian apps want `libjpeg.so.62`.
26. The Store and the settings helper let the logged-in user install M5Stack apps and
    change settings without a password. Anything running as `zero` can therefore do the
    same; it cannot become root or install from elsewhere without the password.
27. rc10 adds libpng, libjpeg as `libjpeg.so.62` and SDL2 (ttf, image, mixer). SDL2 has no
    display driver (BUG-LIST B11). 46 of 101 index apps are installable; installable does
    not mean an app works.
