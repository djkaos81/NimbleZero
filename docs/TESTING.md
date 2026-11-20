# Testing: what is checked, how, and what it does not prove

| Layer | Tool | Runs where | Proves | Does not prove |
|---|---|---|---|---|
| Kernel configuration | `tests/check-kernel-config.sh` | build host | every option the board needs survived the module allowlist | that the drivers work |
| Image structure | `tests/check-image.py` | build host, no root | MBR, FAT boot files, `config.txt` vs shipped overlays, board overlays resolve against the CM0 device tree, arm64 `Image`, kernel/modules version match, required drivers present, ext4 clean, every ELF is AArch64 with its libraries present | that the firmware accepts it or that it boots on a CM0 |
| System boot + userspace | `tests/qemu-boot.py` | QEMU `raspi3b` | the real kernel, rootfs and init boot to a login prompt; init scripts, SSH, diagnostic UI on a 320x170 framebuffer, key handling, `nz-perf` | anything about the Cardputer board: QEMU has no SPI LCD, TCA8418, codec, gauge, Wi-Fi |
| Package lifecycle | `tests/compat/lifecycle.script` (via `qemu-boot.py`) | QEMU | real dpkg with real upstream `.deb`s: install, upgrade, config preservation, removal, interrupted install, firmware protection | upstream automatic updaters, AppStore |
| systemd variant | `tests/compat/systemd.script` | QEMU | upstream postinst's systemd path, `systemctl --user` service, upgrade restart | polkit-triggered updates |
| Console input hand-over | `tests/compat/launcher-input.script` | QEMU | keys pressed in the launcher are not read by the next console program (fails on rc2) | - |
| UI code | `nzui.h` SDL backend (`make sim`, headless with `NZUI_KEYS`/`NZUI_SCREENSHOT`) | desktop | application drawing/input logic | LCD timing, keyboard layers |
| Measurements | `nz-perf`, `tests/bench-*.script`, `tests/bench-report.py` | QEMU | relative differences in memory, process count, wakeups, storage writes between configurations | absolute boot time, CPU load or any power figure |

## QEMU substitutions (stated once, apply to every QEMU result)

QEMU 8.2 `-M raspi3b` emulates a Raspberry Pi 3B (BCM2837, the same SoC family as the CM0's
RP3A0) with 1 GB RAM. The harness takes `Image`, `bcm2710-rpi-3-b.dtb` and
`overlays/disable-bt.dtbo` **out of the image under test**, applies that overlay (what the
firmware would do for `dtoverlay=disable-bt`) so the PL011 becomes a console, and boots the
unmodified SD image as the SD card. Differences from the device:

- device tree of a Pi 3B, no VideoCore firmware stage, no `config.txt` processing;
- console on `ttyAMA0` (kernel parameter `nz.qemu=1` enables a login there);
- display: firmware framebuffer `bcm2708_fb` forced to 320x170x16, not the SPI panel;
- input: QEMU USB keyboard; for APPLaunch it is symlinked to the path of the board keyboard;
- no I2C/SPI devices, Wi-Fi, Bluetooth, audio, battery, RTC; 1 GB RAM instead of 512 MB;
- TCG emulation on 2 shared host cores: all times are emulator times.

## Running

```sh
tests/check-image.py artifacts/cardputerzero-compat.img
tests/qemu-boot.py artifacts/cardputerzero-minimal.img results/min/qemu
# lifecycle: needs a FAT image with the .debs (upstream ones + tests/compat/make-test-debs.sh output)
tests/qemu-boot.py artifacts/cardputerzero-compat.img results/compat/lifecycle \
    --script tests/compat/lifecycle.script --usb-drive debs.img
```

`qemu-boot.py` writes `serial.log`, `qemu-results.json`, `script.log`, PNG screenshots and
`nz-perf.json` into the output directory. Recorded runs for the delivered images are in
`docs/results/`.

## Recorded results

rc1 and rc2 (and the first hardware boot log): see `docs/results/SUMMARY.md` (generated from the JSON reports).


## Release tests (rc8)

```sh
tests/compat/make-fixtures.sh <dir with the M5Stack .deb files> fixtures.img   # needs network
tests/run-release-tests.sh nimblezero-compat.img fixtures.img results/rc8/release
scripts/check-licenses.py out/compat --json nimblezero-compat.licenses.json     # after make legal-info
```

`run-release-tests.sh` boots one emulated card four times: first boot with a password typed
at the prompt, launcher keys, Options, Services, Store, package operations on running
programs, ending like the POWER switch with a deleted file still open; second boot; boot
after writing the image over the card again; boot after an erase request. The harness gets
a root shell on the emulator's serial port (only when the kernel command line has
`nz.qemu=1` and the board model is QEMU's); on a real device root cannot log in.
Known emulator artefacts: the USB keyboard drops the first key after a long idle time (the
scripts press Shift first); the M5 launcher sometimes crashes when the host is busy.

## Test logs

The maintainer's recorded results and logs (`docs/results/` wherever this documentation
refers to it) are not part of the public repository. The scripts that produce them are in
`tests/`.
