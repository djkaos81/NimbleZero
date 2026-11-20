# NimbleZero first public release: work plan

Status: **approved 2026-11-18 with three changes (nz prefix, data kept across reflash, Store); built as v0.1.0-rc8 (emulator-tested, not yet on hardware); see `RELEASE-NOTES.md`.** Decisions below are the owner's (2026-11-18) unless
marked *default*, which are choices made to fill a gap and can be changed.

- Name: **NimbleZero**. Licence of the project's own code: **GPL-2.0**.
- Base: rc7 (last image confirmed on hardware, hw:unit1).
- Size guide from the owner: installed system under about 200 MB is still slim. rc7 compat
  installs about 102 MB of files.

## In this release

### 1. Libraries
- PulseAudio, ffmpeg, libv4l.
- ffmpeg: match Debian's build as far as Buildroot 2026.02.3 has the packages (GPL parts
  on, "nonfree" off). The build report will list every Debian-enabled library that could
  not be matched.
- Extend the dpkg "provided" map so M5 apps that depend on `libpulse0`, `ffmpeg` and the
  libav* packages install; re-run the five packages that were refused before.

### 2. Start-up
- No diagnostic screen at start. Boot goes straight to the NimbleZero launcher, or the M5
  launcher when it is installed and enabled. Diagnostics stays as a launcher entry.

### 3. Users and SSH
- Default user **zero** (UID 1000, the UID the M5 packages expect); home `/home/zero`.
- Launcher, apps and the console run as `zero`, not root.
- Root has no password and cannot log in over SSH. *Default:* `zero` gets root rights
  through `sudo` with its own password.
- First boot asks for a password for `zero`. *Default:* it can be skipped, and SSH stays
  off until a password exists.
- Settings changes that need root (Wi-Fi, services, power off, packages) go through one
  small privileged helper with a fixed list of actions.

### 4. Launcher
- F / X / Z / C work as up / down / left / right without Fn.
- Battery level in the top bar.
- **Options** menu. *Default contents:* brightness, screen-off time, volume, Wi-Fi country,
  time zone, boot log on/off, which launcher starts at boot, change password, about
  (version, address, storage).
- **Services** menu: SSH on/off. *Default:* also network time on/off.
- **Store** (owner decision): browse and install Cardputer Zero apps from the launcher,
  without the command line or the M5 launcher. Sources: M5Stack's app index
  (cardputerzero.github.io/packages, the catalogue of the M5 Store) and M5Stack's
  official app repositories. Apps whose dependencies this OS lacks are listed as such.

### 5. System
- Clock from the network (NTP) once Wi-Fi is up; written back to the hardware clock.
- Wi-Fi country setting (stored on the data partition, applied at boot).
- Boot log stays **on** for this release, with the Options switch to turn it off.
- Branding: os-release, hostname, image and document names say NimbleZero.
- Command prefix changes from `cz` to `nz` (owner decision): `nz-m5`, `nz-rw`, `nz-diag`, ...
  The SDK header follows (`nzui.h`). The kernel release string keeps its `-v8-cz` suffix
  in this build (changing it means rebuilding the kernel and every driver).
- The data partition survives a reflash (owner decision): the image file ends where the
  data partition starts, and the first start adopts an existing NimbleZero data
  filesystem instead of formatting. A clean start is requested with an empty file
  `nz-reset-data` on the boot partition, or Options > Erase all data.

### 6. Licence compliance check
- Buildroot `legal-info` for each released image: licence texts, manifest, sources.
- A check that fails the build on a package with no licence entry, a "nonfree" flag, or a
  redistribution restriction (firmware blobs listed separately with their terms).
- GPL-2.0 `COPYING`, per-file licence tags on the project's own code, third-party notices,
  and a statement that this is unofficial and not an M5Stack product.

### 7. Tests and documents
- Emulator: first-boot password flow, SSH refuses root and is off without a password,
  launcher runs as `zero`, F/Z/X/C keys, options and services menus, package installs as
  `zero` through the helper, ffmpeg/PulseAudio smoke tests, licence check.
- Hardware checklist rewritten for this release. No hardware result is claimed until the
  owner reports it.
- Documents updated with the rc7 hardware confirmations still pending.

## Not in this release
- Camera app and Bluetooth: next version.
- Other units / second card testing: not required now.
- `compat_systemd`: kept in the tree, not rebuilt, not released (still the rc1 build).

## To-do list (later)
- Clean rebuild from the tagged source on the owner's PC.
- Turn the boot log off by default.
- Battery / idle measurement on stock firmware and on this OS (`measure.sh`).
- Bug round: see `BUG-LIST.md`.

## Decided
- **B1** (card stops booting after removing or upgrading a running program and using the
  switch) is fixed in this release ("All good" to the plan that recommended it).

## Publishing (owner decisions of 2026-11-20)

- The first public release is **v0.1.0**.
- Three public repositories:
  - **NimbleZero**: the main repository, where the build images are kept (source,
    documentation, releases with images and source archives);
  - **NimbleZero-dependencies**: libraries, runtimes and the like (the add-on packages
    planned for a later version);
  - **NimbleZero-apps**: NimbleZero's own apps, and possibly others designed for NimbleZero.
- The binary-only firmware stays in the image (boot firmware, Wi-Fi/Bluetooth firmware for
  every Compute Module 0 radio variant, Bosch IMU data); terms reviewed, texts shipped.
  Only the 43455/43456 Wi-Fi families, which no Compute Module 0 carries, are removed.
- Doom is not in the image.

## Direction for the minimal image (owner decision of 2026-11-20; not built yet)

Not for v0.1.0. From the next version on:

- **`minimal` contains** the interface (launcher, Options, Services), Wi-Fi setup, and
  everything that is a system-level utility or configuration. The aim: the launcher and
  every quality-of-life improvement over a plain terminal prompt, with the user in control
  of all other software on the device.
- **`minimal` does not contain** other apps, libraries and runtimes. Those are offered
  through the NimbleZero-dependencies and NimbleZero-apps repositories.
- **Everything the main image has beyond `minimal` must be installable from those
  repositories**, so that minimal + packages can reach the main image.

Consequences to work out before building (proposed, not yet agreed):

- `minimal` needs a way to install packages: the package manager and the Store (or a
  smaller installer) belong to it.
- The parts of today's main image that are not system-level have to exist as packages in
  NimbleZero-dependencies: Python with its modules, ffmpeg, PulseAudio, SDL2, fonts, image
  libraries, camera tools, and the runtime for M5Stack app packages. Buildroot does not
  produce installable packages by itself; that packaging is the main piece of work.
- Both images should use the same card layout (system partition size, data partition
  start), so that minimal has room for installed software and a card can move between the
  two without losing its data partition. Today they differ.

Decided by the owner (2026-11-20):

- **The runtime for M5Stack app packages is a package, not part of the base.** NimbleZero
  is building its own ecosystem; some M5Stack compatibility is desirable but will probably
  never be complete, and users must be able to do without it entirely.
- **Sound service: deferred.** Some sound service will probably be wanted; where it
  belongs is decided together with the PipeWire evaluation in the next release.
