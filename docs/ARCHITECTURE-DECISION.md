# Architecture decision: M5Stack compatibility and init system

Status: decided for v0.1.0-rc1 on emulator and source evidence. **To be revisited after
the first hardware measurements** (see the end of this document).

## Decision

1. **The OS is an independent Buildroot system with its own keyboard-first launcher
   (`nz-launcher`) and ordinary Linux application interfaces.** It does not depend on any
   M5Stack software.
2. **M5Stack *application packages* stay supported through a thin optional layer** (real
   dpkg + a seeded dependency database + a handful of libraries). It costs nothing
   measurable at idle and lets self-contained upstream apps install and update from their
   `.deb` without rebuilding the OS.
3. **Full stock-ecosystem parity (APPLaunch as the shell, AppStore, automatic updates) is
   not pursued further.** APPLaunch can be installed and runs, as an option, with known gaps.
4. **Init system: BusyBox init** (configuration A). The systemd variant is kept buildable
   for comparison but is not the release candidate.
5. **Recommended image for first hardware tests: `compat`.** Alternative: `minimal`.

## Evidence against the criteria

Numbers: `PERFORMANCE.md` (QEMU; relative comparisons only). Requirements and tests:
`M5-COMPAT.md`.

| Criterion | Desired | Finding | Verdict |
|---|---|---|---|
| Compatibility | representative applications run unchanged | Self-contained apps: 2 of 7 examined app packages install as-is (Compass, IR-Remote; IR-Remote seen running); the other 5 are blocked only by declared `libpulse0`/`ffmpeg` dependencies. APPLaunch: installs and renders unmodified, but crashed intermittently in 2 of 4 full lifecycle runs, and Store, Wi-Fi/Bluetooth settings and updaters cannot work without APT repositories, polkit, NetworkManager, BlueZ. | **apps: mostly met, fixable; APPLaunch as the system shell: not met** |
| Updates | upstream releases install without rebuilding the OS | Real upgrades of upstream packages with dpkg pass (APPLaunch 0.6.105 -> 0.6.106, Compass 0.2.4 -> 1.0.1), config preserved, interrupted installs recover. The upstream *automatic* paths do not: `apt-get install` expects Debian repositories to resolve dependencies – and letting APT do that would replace the Buildroot glibc with Debian's. | **manual/package-level: met; automatic upstream updater: not met** |
| Performance | small difference from minimal | Compat *layer* at idle: 39.0 MB vs 39.0 MB used (0 %), +3.2 MB userspace PSS (`dbus-daemon`, `udevd`); with the native launcher 40.3 MB (+3.4 %). With APPLaunch running: 48.2 MB (**+24 %**), 195 vs 24 context switches/s (**8x**). | layer: met (<5 %); APPLaunch frontend: not met (>20 %) |
| Battery | no substantial avoidable idle overhead | Not measurable without hardware. Proxy: APPLaunch keeps ~200 context switches/s and 2.4 % CPU of four emulated cores while showing a static carousel; the native launcher blocks in `poll()` and switches the backlight off. | proxy says not met for APPLaunch; **unverified** |
| Maintenance | no recurring manual patching for ordinary upstream releases | App packages: none needed. APPLaunch: its scripts are followed unmodified today (the upstream "offline enable" path is honoured), but each release can add calls the shims do not cover, and a missing D-Bus service already produced a crash loop. | apps: met; APPLaunch: at risk |
| Complexity | manageable dependencies and services | Layer A: dpkg, 8 libraries, D-Bus, about 430 lines of shell. Full parity would add APT + Debian repositories, systemd + logind + PAM, polkit, NetworkManager, BlueZ, PipeWire – i.e. most of the Debian base the project set out to avoid. | layer: met; parity: not met |

The compatibility that *is* cheap (app packages) is kept. The compatibility that is
expensive (APPLaunch as the mandatory shell with its Debian-shaped update machinery) fails
on compatibility, performance and complexity at once, so the fallback of the project brief
applies: an independent launcher, a simple installable format, an SDK, a simulator, and
ported applications. Those exist in this release:

| Brief item | Implementation |
|---|---|
| native keyboard-first launcher | `nz-launcher` (fbdev/evdev, 8x8 font, idle screen-off) |
| lightweight application lifecycle manager | in `nz-launcher`: one foreground app, process group, console hand-over, Ctrl+Alt+Backspace kill |
| simple installable application format | a normal `.deb` (`/opt/<name>` + one `.desktop` file), made by `sdk/tools/nz-mkdeb`, installed by real dpkg – not a new proprietary format, and the same one M5Stack apps use |
| standard Linux graphics and input APIs | fbdev/KMS + evdev; nothing custom |
| cross-compilation SDK | Buildroot `make sdk` (`./build.sh sdk compat`), 287 MB relocatable tarball |
| local desktop application simulator | `nzui.h` SDL2 backend, scriptable and headless |
| application templates and packaging tools | `sdk/template`, `sdk/tools/nz-mkdeb` |
| documented porting process | `APP-DEVELOPMENT.md` |
| representative applications | graphical: Snake (nzui), Diagnostics; terminal: shell, htop, nano; networking: Wi-Fi setup, Network info, SSH client |

## systemd (B) versus BusyBox init + shims (A)

Identical kernel, libraries, dpkg layer and workloads; only the init system and what it
drags in differ (`nz_compat_defconfig` vs `nz_compat_systemd_defconfig`).

| | A: BusyBox init + `nz-unit` + shims | B: systemd 259 + logind + PAM + D-Bus |
|---|---|---|
| RAM used at idle | 39.0 MB | 46.3 MB (**+19 %**) |
| userspace PSS at idle | 7.7 MB | 21.0 MB |
| RAM used with APPLaunch | 48.2 MB | 59.0 MB (+22 %) |
| time to "init complete" (emulated) | 13.8 s | 28.2 s (2.0x) |
| root filesystem | 88.6 MB | 111.1 MB (+22.5 MB) |
| context switches/s at idle | 24.7 | 24.5 |
| upstream `applaunch` postinst | takes its documented non-systemd fallback | takes its native path (linger, `user@1000`, enable, start) |
| `systemctl --user is-active/show` used by the updater | answered by a shim | native |
| upgrade restarts the running APPLaunch | no (restart needed) | yes |
| polkit-triggered updater, NetworkManager, BlueZ | missing | **still missing** |
| extra finding | needs D-Bus or APPLaunch crash-loops | needs PAM or `user@1000` never starts |

systemd gives the more faithful service semantics, and an elaborate imitation of it would
be the wrong direction. But the shim that was actually needed is small, and systemd alone
does not unlock the parts that are broken (APT, polkit, NetworkManager, BlueZ, PipeWire).
Paying +19 % idle RAM and a slower boot for semantics that only APPLaunch uses is not
justified while APPLaunch is optional. If APPLaunch parity ever becomes the goal,
configuration B is the base to build it on, not A.

## Against the numeric targets

- "~5 % overhead is favourable": the compat layer under the native launcher is +3.4 % RAM
  over minimal with no extra wakeups – inside the target.
- "~20 % or more is potentially significant": APPLaunch (+24 % RAM, 8x context switches)
  and systemd (+19 % RAM) are both in that range.

## What would change this decision

- Hardware measurements showing APPLaunch's idle draw is negligible next to the backlight.
- M5Stack shipping apps without Debian-only dependencies, or APPLaunch gaining a mode that
  does not require systemd user sessions/polkit/APT.
- A requirement to run the stock AppStore unchanged: then build on configuration B and add
  polkit, NetworkManager, BlueZ, PipeWire and a private APT repository of
  Buildroot-compatible packages.

## Limits of this evidence

No hardware, no power data, no stock-image measurements; QEMU with 1 GB RAM and a
different board model; four APPLaunch lifecycle runs; one release pair per package. The decision is
the best supported one today, not a final one.
