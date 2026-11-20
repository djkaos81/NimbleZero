# NimbleZero v0.1.0: first public release

Unofficial firmware for the M5Stack Cardputer Zero. Not an M5Stack product.
Licence of the project's own code: GPL-2.0 (`COPYING`).

**Status: v0.1.0 is rc11 with three small changes (next section). rc11 has run on two
devices** (owner's reports of 2026-11-19; details in `HARDWARE-TEST-CHECKLIST.md`). Unit 1: checklist A, B, C1-C5, C7 and E pass, PulseAudio
works; two apps fail (Recorder playback, `fontpreview`). Unit 2 (a Cardputer Zero Lite):
boots, 62 frames/s in the Diagnostics benchmark without display problems, Doom runs. **v0.1.0 itself: starts and Wi-Fi connects on both devices (owner's report of 2026-11-20).** Not
yet run on hardware: the display's no-sync fallback. The repeat of the rc7 failure
(checklist D) passed on unit 1. Everything else in this file that is not marked as a hardware report comes from
the emulator, which has no SPI display, Wi-Fi, sound card, battery gauge or network.

## v0.1.0 on top of rc11 (owner decisions of 2026-11-20)

- **Version v0.1.0.** No functional change to anything tested in rc11.
- **Wi-Fi/Bluetooth firmware set reduced** to what a Compute Module 0 can carry (43439,
  43436s and the 43430 names they are reached through). The 43455 and 43456 families
  (Pi 3B+/4/5, CM4, 400, 500) are removed: 1.2 MB. Wi-Fi was the one function this could
  affect; it works on both tested devices, the Cardputer Zero and the Cardputer Zero Lite
  (owner report of 2026-11-20).
- **Bosch's licence notice added** for the motion-sensor configuration data
  (`/lib/firmware/bmi270-init-data.fw`). The file is byte-identical to the array in Bosch
  Sensortec's own BSD-3-Clause driver package (`docs/third-party/bosch-bmi270/`).
- The Synaptics licence for the 43436s firmware was read (same shape as the Cypress one:
  distribution in object code form for use with the vendor's chips). Not legal advice.
- Public `README.md`, `KNOWN-ISSUES.md` with a "what does not work" section, and the
  three-repository layout (`RELEASE-PLAN.md`).

## rc11 on top of rc10 (owner decisions of 2026-11-19)

- **Display: 61.8 frames per second without tearing** (rc10: 38.9 with strong tearing), and
  programs that draw into `/dev/fb0` (launcher, console, M5Stack apps) are no longer held to
  20 updates per second. What changed and how to go back: `DISPLAY.md`. Measured on unit 1
  with the test kit; the image's own copy of these settings has not run on hardware.
- New command `nz-panel` (switches, measurements) and `nz-paneltest` (test pictures).
- **PulseAudio fixed (B12).** Programs got "Access denied" from the sound service because
  neither `zero` nor `root` was in the group `pulse-access` (hardware report: Cardputer TV,
  "PulseAudio outdev ... Context failed"). Emulator: `pactl` and ffmpeg's PulseAudio output
  work for both users. The emulator has no sound card; sound from the speaker through
  PulseAudio is not verified.
- Doom is not part of the image (owner decision); it stays in the display test kit.

## rc10 on top of rc9 (owner decision of 2026-11-19)

- **libpng**, **libjpeg under the library name Debian programs link** (`libjpeg.so.62`;
  rc9 had the same library as `libjpeg.so.8`) and **SDL2** with its ttf, image and mixer
  libraries.
- With the same index copy the Store lists **46 of the 101 index apps as installable**
  (rc9: 32, rc8: 18); 55 still need other software.
- **SDL2 has no display driver in this image.** The ones it could use here need X11,
  Wayland, or the GPU stack (KMS/DRM with GBM and EGL), none of which the image has. SDL2
  apps that draw to the framebuffer themselves and use SDL2 for sound and input can work
  (two such apps install and find every library: `fontpreview`, `refract-pocket`). An app
  that opens an SDL window, such as `tic80-c0` which asks for the KMS/DRM driver, installs
  but cannot show anything. The Store cannot tell these apart: "can be installed" is not
  "works". Neither kind has been run on a display.
- Size: about 241 MB installed (rc9: 239 MB).
- Everything else is as in rc9. Add-on packages from a repository, CJK fonts, pygame and the
  hosting decisions are for a future version (owner decision).

## rc9 on top of rc8 (owner decisions of 2026-11-19)

- **Python 3.14.5** with Pillow 12.0, NumPy 2.4, evdev, qrcode and requests, plus the
  DejaVu fonts at the path Debian apps expect. Debian's `dist-packages` directory is on
  Python's search path. With the same index copy, the Store now lists 32 index apps as
  installable (rc8: 18); 69 still need other software. Most frequent remaining needs in
  the index: `libjpeg62-turbo` (this image has libjpeg-turbo under a different library
  name), `fonts-noto-cjk`, SDL2, `python3-pygame`, systemd, NetworkManager, BlueZ.
  qrcode, requests and the fonts are small and the same apps ask for them.
- **Size:** the installed system is now about 239 MB (rc8: 158 MB, rc7: 102 MB). That is
  above the 200 MB you called slim. Python is 63 MB of it and the fonts 10 MB.
- **SDK is GPL-2.0** like everything else (was MIT). A program that includes `nzui.h` must
  therefore be distributed under GPL-2.0-compatible terms.
- **Source archives** for every package are published with the release, with the
  Buildroot tree.
- Everything below describes rc8 and still holds for rc9 unless this section says otherwise.

## What changed since rc7

### Start-up and accounts
- No diagnostic screen at start. The LCD goes to the launcher (Diagnostics is an entry in it).
- The default user is **zero** (UID 1000). The launcher and every app started from it run
  as `zero`, not root.
- **First boot asks for a password for `zero`** on the LCD (typed twice, at least 6
  characters). It is the password for SSH and for `sudo`. Enter alone skips it; SSH then
  stays off until a password is set (Options > Password).
- **root has no password** and cannot log in anywhere: not on the LCD, not on the UART, and
  the SSH server refuses root even if a password were set (`dropbear -w`).
- `sudo` asks for the password of `zero`. One program needs no password: the settings
  helper `nz-admin`, which accepts a fixed list of actions and checks every argument.
- The password file is on the data partition (`/data/etc/shadow`).

### Launcher
- **F / X / Z / C work as arrows without Fn:** F up, X down, Z page up, C page down
  (in the launcher and in every menu).
- Battery gauge in the top bar: outline, coloured fill, per cent, `+` while charging.
- **O** opens **Options**: brightness, screen-off time, volume, Wi-Fi network, Wi-Fi
  country, time zone, launcher at start (NimbleZero or M5), boot log on/off, password,
  services, about, erase all data.
- **Services**: SSH server on/off (kept across restarts), clock from the network on/off,
  set the clock now, Wi-Fi setup.
- **Store**: browse and install Cardputer Zero apps without the command line or the M5
  launcher (next section).

### Store
- Two sources, both M5Stack's own:
  1. the Cardputer Zero app index, `cardputerzero.github.io/packages` (the catalogue the
     M5 Store uses; 101 apps when the test copy was saved on 2026-11-19);
  2. M5Stack's official apps and the M5 launcher, from the releases of
     `github.com/CardputerZero/<repository>` (15 entries, fixed list).
- The list has three parts: installed, can be installed, and "need software this OS lacks"
  with the missing package names. Most index apps are in the third part: they need
  Python 3 with Pillow/NumPy/evdev, SDL2, fonts or other Debian packages this image does
  not contain (numbers under "Test results").
- Downloads from the index are checked against the index's SHA-256 before installing. A
  download address outside M5Stack's repositories is refused. System packages cannot be
  removed from the Store.
- Apps install into the system partition (768 MB, about 530 MB free in this image).

### Libraries
- PulseAudio 17.0 (daemon, `pactl`, `libpulse`), libv4l and `v4l2-ctl` (v4l-utils 1.28.1).
- ffmpeg 6.1.5 with `ffprobe`, the GPL parts and no "nonfree" parts; TLS through GnuTLS
  as in Debian. External libraries: x264, x265, libvpx, dav1d, Opus, LAME, Vorbis, Theora,
  Speex, OpenJPEG, libass, FreeType, HarfBuzz, FriBidi, libxml2, soxr, libdrm, ALSA,
  PulseAudio, V4L2.
- **Differences from Debian's ffmpeg** (see `M5-COMPAT.md`): Debian 13 ships ffmpeg 7.1,
  Buildroot 2026.02.3 ships 6.1.5, so the library names differ (`libavcodec.so.60` here,
  `.61` in Debian). Apps that run the `ffmpeg` program work; an app linked directly against
  Debian's libav libraries would not load. `ffplay` is not built (needs SDL2). Several
  libraries that Debian enables, as far as I know its build, have no ffmpeg option in this
  Buildroot (libaom, libwebp, SVT-AV1, rav1e, zimg, vid.stab, Xvid, libjxl, librist, SRT
  and others); the comparison was not checked against the Debian package itself.
- The M5Stack apps refused before for `ffmpeg` / `libpulse0` / `pulseaudio-utils` now
  install: Music, Recorder, Files, Piano, Keyboard Guide.

### System
- Clock from the network: once per boot after the first DHCP lease (BusyBox `ntpd`,
  `pool.ntp.org`), written to the hardware clock. Time zones from tzdata.
- Wi-Fi country setting (regulatory domain), applied at boot and written into the saved
  network configuration.
- Boot log stays on; Options switches it off. (To-do: off by default later.)
- USB serial gadget removed (the USB-C port is charge-only by its label; a PC saw nothing).
- Commands are now `nz-*` (were `cz-*`): `nz-m5`, `nz-rw`, `nz-diag`, `nz-admin`, ...
  The SDK header is `nzui.h`. Boot partition label `NZBOOT`, log folder `nz-logs/`.
  The kernel release string still ends in `-v8-cz` (unchanged kernel build).

### Your data survives a reflash
- The image file ends where the data partition starts. Writing a new image replaces the
  boot and system partitions only.
- At the next start an existing NimbleZero data partition (also one made by rc7) is
  adopted: password, Wi-Fi networks, settings and `/home` stay. Apps installed with the
  Store or `dpkg` live in the system partition and are gone after a reflash.
- To start clean: Options > Erase all data, or put an empty file named `nz-reset-data` on
  the boot partition from a PC.
- This relies on the data partition starting at the same place in every release (sector
  1712128 for `compat`, 663552 for `minimal`). Changing the system partition size later
  would break it; a flashed `compat` card must be reflashed with `compat`.

### Bug fixes
- **B1/B2 (card stopped booting after removing a running program and using the switch).**
  Two changes:
  1. after a package operation, programs still running from replaced or removed files are
     restarted (services) or closed (others), so the system partition goes back to
     read-only at once;
  2. the system partition is created without ext4's `orphan_file` feature. With it, a
     forced read-only by the switch while a deleted file is still open leaves a filesystem
     the kernel refuses to mount read-only; without it the next start cleans up normally.
- A leftover "M5 launcher enabled" link no longer leaves the screen blank when the M5
  launcher is not installed.
- B4 (diagnostic drawn over the M5 launcher at boot) and B5 (USB serial gadget) are gone
  with the changes above. B6: `v4l2-ctl` is now in the image for camera tests.

## Test results (emulator: QEMU raspi3b, not hardware evidence)

Images: the `compat` and `minimal` builds from tag `v0.1.0`. Only the `compat` build is
published, as `nimblezero-v0.1.0.img.xz` (the file was renamed after testing; its content
and checksum are those of the tested `nimblezero-compat-v0.1.0` image).
The reports and logs these results come from (`docs/results/` in the text below) are
kept by the maintainer and are not part of the public repository.

v0.1.0 results:

| Test | Result |
|---|---|
| Structural image check | compat 96/96, minimal 88/88 (3 new: Wi-Fi firmware names for every Compute Module 0 variant resolve; no 43455/43456 files; IMU data matches the recorded Bosch file and its notice is in the image) |
| Licence check | compat 11/11 (115 packages), minimal 9/9 (24 packages) |
| Boot 1 (compat) | first run 69/71, second run 71/71. The two failures of the first run were menu checks where a navigation key press did not arrive (the Services menu was not open when the check ran): the emulated keyboard losing a key, as seen before. |
| Boot 2: after the forced switch-off | 25/25 |
| Boot 3: after writing the image over the card again | 25/25 |
| Boot 4: after an erase request | 24/24 |
| Display settings at start | compat 31/31; minimal 30/31, then 32/32 with an added check (see below) |
| minimal: boot, password skipped | 22/23, 22/23, then 23/23 (see below) |
| Launcher-input suite | 34/34 |
| Package-lifecycle suite | 68/68 (the M5 launcher did not crash in this run) |

The minimal failures were test timing, not the image. (1) "boot partition read-only": the
check ran while the boot log was still being written in the background, which keeps the boot
partition writable for a few seconds after start-up; the harness now waits for that, and an
added check confirms the partition is read-only afterwards. (2) "first-start storage step
under 10 s" took 11.5 s once and 3.3 s in the next run; the emulator host was slower than
usual during the first run.

The rc11 table follows for comparison.

rc11 results:

| Test | Result |
|---|---|
| Structural image check | compat 93/93, minimal 85/85 (10 and 8 new checks: LCD clock and sync pin in the merged device tree, display settings file, module switches, PulseAudio group). The rc10 image fails 11 of the new compat checks, as it should. |
| Licence check | compat 11/11 (115 packages), minimal 9/9 (24 packages) |
| Boot 1 (compat): first boot, accounts, menus, Store, multimedia incl. the corrected PulseAudio tests, Python, package operations | 71/71 |
| Boot 2: after the forced switch-off that made rc7 unbootable | 25/25 |
| Boot 3: after writing the image over the card again | 25/25 |
| Boot 4: after an erase request | 24/24 |
| Display settings at start (driver loaded with the image's switches; override gives vendor behaviour) | compat 31/31, minimal 31/31. The emulator has no LCD: this says nothing about the picture. |
| minimal: boot, password skipped | 23/23 |
| Launcher-input suite | 34/34 |
| Package-lifecycle suite | 67/68. The failure: the M5 launcher 0.6.106 crashed twice in the emulator after keyboard navigation (BUG-LIST B8, as in every run since rc8; not seen on hardware). |

Emulator notes: the emulated USB keyboard lost a key press while the harness typed the
first-boot password in several runs; the harness now types the pair again when the prompt
rejects it. During kit testing the emulated SD controller stalled on single-sector reads of
the FAT partition; the rc11 runs above were not affected.

The rc10 table follows for comparison.

rc10 results:

| Test | Result |
|---|---|
| Structural image check | compat 83/83, minimal 77/77 |
| Licence check | compat 11/11 (115 packages), minimal 9/9 (24 packages) |
| Boot 1 (compat): first boot, accounts, menus, Store, multimedia, Python, PNG/JPEG/SDL2, package operations on running programs | 68/68 |
| Boot 2: after the forced switch-off that made rc7 unbootable | 25/25 |
| Boot 3: after writing the image over the card again | 25/25 |
| Boot 4: after an erase request | 24/24 |
| minimal: boot, password skipped | 23/23 |
| Launcher-input suite | 31/31 |
| Package-lifecycle suite | 64/65. The failure: the M5 launcher 0.6.106 crashed 3 times in the emulator after keyboard navigation (BUG-LIST B8, as in the rc8 and rc9 runs at varying steps; not seen on hardware with rc7). |

The rc9 table follows for comparison.


rc9 results:

| Test | Result |
|---|---|
| Structural image check | compat 83/83, minimal 77/77 |
| Licence check | compat 11/11 (110 packages), minimal 9/9 (24 packages) |
| Boot 1 (compat): first boot with a typed password, accounts, SSH, sudo, settings helper, launcher keys, Options, Services, Store, multimedia, Python, package operations on running programs | 65/65 |
| Boot 2: after a forced switch-off with a deleted file still open on the system partition (the rc7 failure) | 25/25: boots, nothing lost |
| Boot 3: after writing the image over the card again | 25/25: data partition adopted, password, settings and files kept |
| Boot 4: after an erase request | 24/24: formatted again, password prompt again |
| minimal: boot, password skipped | 23/23 (a first attempt, run while two other emulators were busy, showed emulated SD-card controller timeouts and failed 3 checks; alone it passes) |
| Launcher-input suite | 31/31 |
| Package-lifecycle suite | 63/65. Both failures: the M5 launcher 0.6.106 crashed repeatedly (4 segmentation faults) after its upgrade in the emulator. In the rc8 run it was 0.6.105 that crashed at start and 0.6.106 that ran. BUG-LIST B8: intermittent since rc1, never seen on hardware (rc7). This run was alone on the host, so load is not the whole explanation. Unknown on hardware for rc8/rc9: checklist test D1. |

Python: imports of ssl, sqlite3, zlib, bz2, lzma, curses, readline, decimal, unicodedata,
expat, PIL, numpy, evdev, qrcode, requests succeed as user `zero`; Pillow drew text with
the DejaVu font from Debian's path and wrote PNG and JPEG; `airfiles` from the index
installed and rendered its home screen to a file.

The rc8 results below were obtained with the same tests before Python was added.

Details worth knowing:
- Password: stored as a SHA-512 hash; SSH accepted `zero` with it and closed the connection
  for root; `sudo` worked with it and not with a wrong one; changing it needed the old one.
- Settings helper: eight malformed or out-of-range requests were all refused.
- Launcher keys: X, F, C and Z each started the expected entry of a marked list, as user `zero`.
- Store with the saved index: 101 apps; in rc8 18 index apps and the 15 official entries
  listed as installable, 83 as needing more software (rc9: 32 and 69). `kanstl` was chosen, installed and removed
  again with the keyboard alone; a download with one changed byte was refused; an address
  outside M5Stack's repositories was refused; removing `libc6` was refused.
- ffmpeg encoded test video with x264 and AAC and read it back; https is among its
  protocols. "PulseAudio started for `zero` and answered `pactl`" in the rc8-rc10 runs was
  a private service started by the test itself; the system service refused every program
  (bug B12, fixed in rc11).
- Running program removed: closed, system partition read-only again. Running service
  upgraded: restarted on the new file. Running service removed: stopped. M5 launcher
  installed, running, removed with `dpkg`: stopped, read-only again.
- B1 control experiment: the same deterministic sequence on the rc7 image ends in the
  kernel panic at the next boot; on a copy of the rc7 image with only `orphan_file` removed
  from the system partition it boots (`docs/results/rc8/b1-experiment`).
- Emulator artefact: the emulated USB keyboard drops the first key after a long idle
  time, which made the typed passwords "differ" until the test pressed Shift first.
  The prompt itself handled the mismatch correctly (it asked again).
- The earlier package-lifecycle and launcher-input suites: launcher-input 31/31; package lifecycle 62/65. The
  three failures are the M5 launcher (APPLaunch 0.6.105) crashing with a segmentation
  fault in the emulator shortly after start (BUG-LIST B8: intermittent, seen since rc1,
  more often when the host is busy, which it was; not seen on hardware with rc7). In the
  release run the M5 launcher 0.6.106 did start and was running when it was removed. I
  cannot rule out that rc8 made this worse; test D1 of the checklist answers it on
  hardware. The hostile-package tests, firmware-file comparison (3594 files unchanged) and
  "system partition read-only again" all pass.

## Not verified

Reported from hardware with **rc11**: on unit 1, checklist blocks A and B, the Store
(C1-C5, C7), data kept across a reflash (E), sound through PulseAudio (Cardputer TV); on
unit 2 (Cardputer Zero Lite), start, display at 62 frames/s, Doom. With rc10 plus the
display test kit on unit 1: the display figures in `DISPLAY.md`.

Not verified on hardware:

- v0.1.0 beyond "starts and Wi-Fi connects"; Wi-Fi on a Compute Module 0 with the Synaptics
  43436s chip.
- The display's no-sync fallback; `nz-panel auto` on rc11; the display on unit 3; long
  runs, heat, battery consumption with the faster panel link.
- Music (official M5 app) through PulseAudio. Recorder records but cannot play back (B14).
- Battery measurement and camera (F).
- The M5 launcher under the new accounts. Its own Store asks for the sudo password now.
- The launcher's clock after choosing a time zone.
- `compat_systemd` was not rebuilt (still the rc1 build) and is not part of this release.

## Decisions taken while building

- The M5 launcher takes over at boot once it is installed, unless Options says otherwise.
- The Store may install official M5 apps and index apps without a password (through the
  helper). Removing or installing anything else needs `sudo` with the password.

## Publishing obligations (licences)

- `scripts/check-licenses.py` passes for both images (report: `*.licenses.json`).
- The image carries all licence texts and a manifest in `/usr/share/licenses`.
- A public release of the image must come with the complete corresponding source: this
  repository at tag `v0.1.0`, Buildroot 2026.02.3, and the source archives Buildroot
  saved. All three are published with the release. The prebuilt
  toolchain's sources are published by Bootlin and are not in that folder.
- Binary-only parts redistributed unmodified under their vendors' terms: Raspberry Pi boot
  firmware (Broadcom / Raspberry Pi licence), the Wi-Fi/Bluetooth firmware and its settings
  files (Cypress, Synaptics and Broadcom agreements), and Bosch's BMI270 configuration data
  (BSD-3-Clause). The terms were read on 2026-11-19/20 and each permits redistribution for
  use with the vendor's hardware; the texts are in the image and in
  `nimblezero-v0.1.0-licences.tar.gz`, to be published next to the download. This is a
  record of what was checked, not legal advice.
