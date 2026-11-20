# Display path (rc11)

The LCD is an ST7789 panel, 320x170, on SPI. The GPU is not connected to it. From rc11 the
image runs the panel differently from the vendor setup. Everything below was measured on
one device (hw:unit1, 2026-11-19); results and raw logs are in the maintainer's test logs (not in the public repository).

| | Vendor setup (rc10) | rc11 |
|---|---|---|
| Panel clock | 60 MHz requested, 50 MHz actual | 100 MHz |
| Full frames per second, program drawing directly | 38.9 | 61.8 (the panel's refresh rate) |
| Tearing | strong | none seen (bars, Doom, normal use) |
| Programs drawing into `/dev/fb0` | at most 20 updates/s | every picture of a 57/s test program arrived |

## What changed, and why

1. **Clock 100 MHz** (`dtoverlay=nz-panel-100m`). Sending one frame takes 8.9 ms.
2. **Sync pin** (`dtoverlay=nz-panel-te`): the panel's "tearing effect" output is on GPIO 5.
   The vendor device tree has the line commented out. Measured: one pulse every 16.17 ms.
3. **Scan order.** The vendor setup writes the picture across the panel's own scan
   direction, which tears whatever the timing. The driver now rotates the picture in
   software and writes it along the scan direction (orientation variant 2 of 4). A write
   that starts on the sync pulse and is faster than the scan never meets it.
4. **Cached picture buffers.** Reading a frame out of the vendor driver's write-combined
   buffer took 7.4-8.6 ms; from cached memory about 1 ms.
5. **Background sending.** The update call returns after the copy; a worker waits for the
   pulse and sends. A program has about 15 ms to draw instead of about 6.
6. **`/dev/fb0` pace.** The kernel passes changes on after 50 ms; the image sets 8 ms.
7. **Small changes are sent as small pieces.** Pieces of 1/16 of the screen or more start
   on the sync pulse.

If the sync pin gives no pulses, the driver gives up waiting after five misses and sends
without synchronisation (`te_given_up=1` in the counters). Emulator-untestable and not yet
exercised on hardware.

## Switches

`/etc/modprobe.d/nz-panel.conf` sets them at start; the module's own defaults are the vendor
behaviour. At run time: `sudo nz-panel` (lists the commands), for example

    sudo nz-panel status        clocks, sync pin, counters
    sudo nz-panel auto          the standard measurement series (saved in ~/panel-auto.txt)
    sudo nz-panel stock         vendor behaviour until restart
    sudo nz-panel default       back to the start values

## Going back to the vendor behaviour

- For as long as the file exists: empty file `nz-display-original` on the boot partition
  (`NZBOOT`), or `sudo nz-panel original-at-start on` (`off` removes it). Takes effect at
  the next start.
- Until the next restart: `sudo nz-panel stock`.
- Clock and sync pin: remove the two `dtoverlay=nz-panel-...` lines from `config.txt`.
- `config-safe.txt` (see BOOT.md) uses the vendor clock and the vendor behaviour.

## Not known

- Unit 2 (a Cardputer Zero Lite) ran rc11 on 2026-11-19: 62 frames/s in the Diagnostics
  benchmark, no display problems, Doom fine (owner's report; no `nz-panel auto` log).
  Unit 3 has not been tried. Two devices say little about the margin of the 100 MHz clock
  across all units.
- Long runs, heat, battery consumption (the panel link is busier than before).
- Programs that use `/dev/fb0` are shown whenever the kernel picks their changes up; a
  program caught in the middle of drawing can show a half-drawn picture for one refresh.
  `/dev/fb0` has no "frame finished" signal.
