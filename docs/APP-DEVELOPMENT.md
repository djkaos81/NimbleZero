# Developing and porting applications

Cardputer Zero OS is an ordinary AArch64 glibc Linux system. An application is any program;
what makes it show up in the launcher is a freedesktop `.desktop` file. There is no
proprietary application API.

| Interface | What to use |
|---|---|
| Display | Linux framebuffer `/dev/fb0` (alias `/dev/fb_lcd`), 320x170 RGB565, or DRM/KMS on the same device (`/dev/dri/card*`) |
| Keyboard | evdev (`/dev/input/event*`), or a terminal on the text console (`/dev/tty1`, 53x21 characters) |
| Audio | ALSA (`hw:` card "ES8389-Audio") |
| Sensors | IIO (`/sys/bus/iio/devices`), battery via `/sys/class/power_supply`, backlight via `/sys/class/backlight` |
| Expansion | `/dev/i2c-1`, `spidev`, UART, LEDs in `/sys/class/leds` (switched 5 V rails) |
| Network | BSD sockets; `wpa_cli`, `udhcpc`, Dropbear `dbclient`/`scp` |

## 1. Toolchain / SDK

```sh
./build.sh sdk compat          # -> artifacts/nz-sdk-compat-<host>.tar.gz   (relocatable)
mkdir ~/nz-sdk && tar -xzf artifacts/nz-sdk-compat-*.tar.gz -C ~/nz-sdk --strip-components=1
~/nz-sdk/relocate-sdk.sh
export PATH=~/nz-sdk/bin:$PATH
aarch64-linux-gcc --version    # gcc 14.3, glibc 2.41 sysroot with every library of the image
```

The SDK is Buildroot's standard `make sdk` output: cross gcc/g++, the sysroot with the
headers and libraries of the image (including `nzui.h`), `pkg-config` wrappers and a CMake
toolchain file (`share/buildroot/toolchainfile.cmake`).

Any AArch64 glibc toolchain whose glibc is not newer than 2.41 also works for programs that
only use libc/libstdc++ (this is how the upstream M5Stack apps run unchanged).

## 2. Three ways to write a program

**a) Terminal program** – any conventional Linux CLI/curses program. Give it a launcher entry
with `Terminal=true`; the launcher hands it the text console. `htop`, `nano` and the shell in
the image are exactly this. Mind the 53x21 character screen.

**b) Graphical program with `nzui.h`** (`sdk/nzui/`, GPL-2.0, single header, no dependencies; a program that includes it must be distributed under GPL-2.0-compatible terms):

```c
#define NZUI_IMPLEMENTATION
#include "nzui.h"
...
nzui_init("title"); nzui_clear(NZUI_BLACK); nzui_text(4, 4, NZUI_WHITE, "hi"); nzui_flush();
while (nzui_poll(&ev, 1000) >= 0) { /* ev.key is a Linux KEY_* code */ }
nzui_quit();
```

The same source builds for the device (fbdev + evdev) and, with `-DNZUI_SDL`, for a desktop
window. `sdk/template/` is a ready project (`make sim`, `make`, `make deb`);
`sdk/examples/snake.c` is a complete small game; `external/package/nz-launcher` and
`nz-diag` are larger examples.

**c) Anything else that draws on fbdev/KMS** – LVGL (fbdev or DRM backend), SDL2 with the
KMSDRM or fbdev driver, Qt linuxfb, raw mmap. Enable the library in a defconfig fragment
(Buildroot ships `sdl2`, `lvgl` is a header/source drop-in) and link against it.

## 3. Desktop simulator

```sh
cd sdk/template && make sim && ./hello-sim           # 3x scaled window, PC keyboard
SDL_VIDEODRIVER=dummy NZUI_KEYS="up,up,q" NZUI_SCREENSHOT=out.ppm ./hello-sim   # headless, scripted, for CI
```

The simulator shares the drawing and input code path of `nzui.h` only. It does not emulate
the kernel, the LCD timing, the keyboard layers or any sensor. For system-level testing use
`tests/qemu-boot.py` (see `TESTING.md`), and the device itself for everything hardware.

## 4. Packaging and installing

An application package is a normal Debian package:

```sh
sdk/tools/nz-mkdeb --name myapp --version 1.0 --exec ./myapp --title "My App" --comment "Does things"
scp myapp_1.0_arm64.deb root@cardputerzero:/tmp/ && ssh root@cardputerzero dpkg -i /tmp/myapp_1.0_arm64.deb
```

- Files go to `/opt/<name>/` plus one `/usr/share/applications/<name>.desktop`.
- `dpkg -r myapp` removes it, `dpkg -i` of a higher version upgrades it, conffiles and
  maintainer scripts work as on Debian.
- Declare real dependencies. `Depends:` names are checked against the firmware inventory
  (`dpkg -l`, section "firmware"). There is no APT repository; a dependency the image does
  not provide makes the install fail with a message naming it.
- A package cannot replace a file of the system image (the `dpkg` guard refuses it). System
  components change by flashing a new image or by adding the package to the Buildroot
  configuration.
- Without packaging: copy the binary anywhere and drop a `.desktop` file into
  `/usr/local/share/applications/`; press `R` in the launcher.

On the minimal image there is no dpkg and no launcher: copy binaries and run them from the
shell.

## 5. Porting an existing Linux program

1. If Buildroot already has it: add `BR2_PACKAGE_<NAME>=y` to
   `external/configs/fragments/compat-libs.frag`, run `scripts/gen-defconfigs.sh`, rebuild.
   Add a `.desktop` entry (see `external/package/nz-apps/files/`).
2. If not: write a small Buildroot package in `external/package/<name>/` (`dpkg` and
   `nz-diag` in this tree are short examples of an autotools and a plain-make package), or
   cross-compile it with the SDK and package it with `nz-mkdeb`.
3. Check the UI fits: 320x170 pixels or 53x21 characters, keyboard only, arrows on the Fn
   layer.

## 6. M5Stack application packages

Upstream `m5cardputerzero-*` packages install with `dpkg -i` or `nz-m5 install <Repo>` when
their dependencies are present, and appear in the native launcher because it also reads
`/usr/share/APPLaunch/applications`. See `M5-COMPAT.md` for what works.

## Things to know on the device (since rc3)

- **The system partition is read-only.** An app must keep its settings and data under
  `$HOME` (which is on the data partition) or `/data`, never next to its binary. Packages
  install normally: `dpkg` opens the system partition for the duration of the install.
- **Text input**: the keyboard's Sym layer sends private key codes. On the text console
  they are already translated. A `nzui` app that reads raw key codes can use
  `nz_key_char(code, shift)` from `sdk/nzui/nz_keymap.h` to get the character.
- A `nzui` app that hands the console to a child process must call `nzui_suspend()`
  first; it drops keystrokes that would otherwise be read by the child as input.
- **Output of graphical apps** (`Terminal=false`, the default) is not shown: the launcher
  sends stdout and stderr to `/tmp/nz-app.log` (overwritten per launch) so it cannot be
  drawn over the app's screen. Read it from SSH or the Terminal app. `Terminal=true` apps
  get the text console as before.


## rc8 notes for app authors

- Apps run as user `zero` (groups audio, video, input, dialout, gpio, sudo, tty), with
  `HOME=/home/zero` and `XDG_RUNTIME_DIR=/run/user/1000`. Write only below `$HOME`,
  `/tmp` or `/data` paths you own. Output of a graphical app goes to `/tmp/nz-app.log`.
- Something that needs root must ask the user to run it with `sudo`, or use one of the
  fixed actions of `nz-admin` (`nz-admin` without arguments lists them).
- `nz-pick TITLE < lines` gives shell scripts a full-screen list menu (prints the chosen
  row number; F/X/Z/C and arrows, Enter, Esc).
- The header is `nzui.h` (was `czui.h`); every `czui_`/`CZUI_` name became `nzui_`/`NZUI_`.
  The SDK header, template and example are GPL-2.0 like the rest of the project (they were MIT up to rc8).
