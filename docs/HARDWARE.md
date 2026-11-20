# Hardware: what the Cardputer Zero is, and what this OS supports

Evidence tags used below:

- **doc** – stated by M5Stack product documentation / store listings.
- **src** – read from source code (device-tree overlay, driver, kernel).
- **img** – verified in the generated image by `tests/check-image.py`.
- **emu** – exercised under QEMU (`tests/qemu-boot.py`). QEMU emulates a Raspberry Pi 3B,
  not the Cardputer Zero: none of the board peripherals exist there.
- **hw** – confirmed on a physical unit. **Nothing in this document carries this tag yet.**

## Platform

| Item | Value | Evidence |
|---|---|---|
| Module | Raspberry Pi Compute Module 0 (CM0) | doc |
| SoC | RP3A0 SiP (BCM2710A1 die, "BCM2837"), 4x Cortex-A53 @ 1 GHz, ARMv8-A | doc |
| RAM | 512 MB LPDDR2 | doc |
| Wireless | 2.4 GHz 802.11 b/g/n + Bluetooth 4.2 (SDIO + UART) | doc |
| Boot | VideoCore firmware from the FAT partition of the microSD card (`bootcode.bin` -> `start.elf` -> kernel) | src (pi-gen), img |
| Device tree | `bcm2710-rpi-cm0.dtb` (present in raspberrypi/linux 6.12+ and in the boot firmware repo) | src, img |
| Stock OS | Debian trixie arm64 (Raspberry Pi OS desktop via `CardputerZero/pi-gen`), Raspberry Pi kernel 6.18 | src |

Architecture decision: **AArch64 only.** The stock system, every published M5Stack package
(`Architecture: arm64`) and the CM0 device tree are 64-bit; an ARM32 build would save some
RAM but could not run any upstream binary.

## Board wiring (from `cardputerzero-v5-overlay.dts`, the production "v0.5" board)

| Function | Device | Bus / pins | Driver used | Source |
|---|---|---|---|---|
| LCD 320x170 RGB565 | ST7789V(P3) | SPI0 CS0 (GPIO8), DC GPIO25, 60 MHz requested | `panel-mipi-dbi-m` (M5 variant of DRM `panel-mipi-dbi`) + init blob `cardputerzero,st7789v_lcd.bin` | M5 out-of-tree |
| Backlight | PWM on IO expander ch.3, 50 kHz | via M5IOE1 | `pwm_bl_m5stack` | M5 out-of-tree |
| IO expander | M5IOE1 (PY32 MCU) @ I2C1 0x4f: 16 GPIO + PWM | I2C1 | `m5ioe1` | M5 out-of-tree |
| Keyboard 46 keys | TCA8418 @ I2C1 0x34, IRQ GPIO27, 3 keymap layers (base/Sym/Fn), layer LEDs on expander | I2C1 | `tca8418_keypad_m5stack` | M5 out-of-tree |
| Audio codec | ES8389/ES8390 @ I2C1 0x10, I2S, MCLK 12.288 MHz; speaker amp enable GPIO24; headphone detect GPIO17 | I2S + I2C1 | `es8389_m5stack`, `cardputerzero-audio` (sound card), `simple-amplifier` | M5 out-of-tree |
| Fuel gauge | BQ27220 @ I2C1 0x55, 10 mOhm sense, 1750 mAh design | I2C1 | `bq27xxx_battery(_i2c)` (M5 copy) | M5 out-of-tree |
| RTC | Epson RX8130 @ I2C1 0x32 | I2C1 | in-kernel `rtc-ds1307` | mainline |
| IMU | BMI270 @ I2C1 0x68 | I2C1 | `bmi270_core/_i2c` (M5 copy) + `bmi270-init-data.fw` | M5 out-of-tree |
| Magnetometer | BMM150 behind the BMI270 aux bus | via BMI270 | `bmc150_magn_bmi270` | M5 out-of-tree |
| Power-fail / power key | GPIO6 falling edge; disable line on expander IO11 | GPIO | `powerfail_suo` | M5 out-of-tree |
| Switched 5 V rails, Grove function pin | GPIO4, expander IO0/IO3/IO12 exposed as LEDs `grove_fun`, `ext_usb_gpio_fun`, `ext_5v_out`, `grove_5v_out` | GPIO | `leds-gpio` | mainline |
| Expansion SPI chip select | SPI0 CS2 through GPIO22 | GPIO | `gpio-forwarder` | M5 out-of-tree |
| Camera | IMX219 8 MP on CSI | CSI-2 | `imx219` + `bcm2835-unicam` | Raspberry Pi kernel |
| IR receiver / transmitter | GPIO13 (in, pull-up) / GPIO12 (out) | GPIO | `gpio-ir-recv`, `gpio-ir-tx` | mainline |
| USB | DWC2 OTG controller; on-board GL852G hub; SR9900A 10/100 Ethernet | USB | `dwc2`, `sr9700`/`sr9800`/`asix` | mainline |
| Wi-Fi / BT | SDIO + UART | SDIO/UART | `brcmfmac`, `hci_uart` (bcm) + firmware from `brcmfmac_sdio-firmware-rpi` | mainline |

All M5Stack drivers come unmodified from `m5stack/m5stack-linux-dtoverlays` (GPL-2.0), pinned
at commit `4f7aeee9`. That tree targets the current Raspberry Pi kernel (6.18): the IMU code
uses the string form of `MODULE_IMPORT_NS`, which needs 6.13 or newer, and other drivers carry
`>= 6.18` guards. This is why the kernel here is 6.18 rather than Buildroot's default 6.12.

## Support matrix

"Built" means the driver, firmware and device-tree node are in the image and the structural
checks pass. It does **not** mean the function has been seen working.

| Function | Minimal image | Compat image | Status |
|---|---|---|---|
| Boot firmware, kernel, CM0 DTB, rootfs | yes | yes | img; kernel + userspace boot **emu** (on Pi 3B DTB) |
| Internal LCD + backlight | built, loaded first in boot | same | **works, hw:unit1 (rc2)**: upright, colours correct by eye, backlight keys and idle-off work |
| Framebuffer console (6x8 font, 53x21 chars) | yes | yes | emu (on QEMU framebuffer) |
| Keyboard, Fn/Sym layers, layer LEDs | built | built | **works, hw:unit1 (rc5)**: keys, Fn layer, LEDs, and every Sym symbol |
| Diagnostic UI (`nz-diag`) | yes | yes | works, hw:unit1 (rc2) |
| Battery gauge | built | built | hw:unit1: plausible readings (95 %, 4.04 V, -284 mA); charging seen (rc5) |
| Audio out / microphone (ALSA; `amixer`, `aplay`) | built | built | hw:unit1 (rc5): **speaker output works**; microphone untested (mixer-route error at driver load) |
| Wi-Fi (wpa_supplicant) | built, configured from `/boot/wifi.conf` | same + setup app | **works, hw:unit1 (rc5)**: joined a WPA network with the setup app; SSH over it |
| Bluetooth | kernel + firmware only, no BlueZ userspace | same | hw:unit1: controller initialised; not usable without userspace |
| USB host (internal hub, on-board Ethernet, storage, HID, serial adapters) | built | built | hw:unit1: internal hub and on-board 10/100 Ethernet (`r8152`, `eth0`) enumerate; link/traffic unverified. USB HID + storage **emu** |
| USB device mode (serial gadget console) | `g_serial` loaded at boot | same | img; unverified; host/device switching on this board is not documented in source |
| RTC | built (`hwclock`) | built | hw:unit1: driver bound; time-keeping unverified |
| Power switch / power-fail handler | built | built | **works, hw:unit1 (rc5)**: four switch-offs, no filesystem recovery needed at the next start |
| IMU + magnetometer (IIO) | built | built | hw:unit1: both IIO devices present; readings unverified |
| Camera (V4L2 raw capture) | kernel side built; no libcamera | same | hw:unit1 (rc2): imx219 sensor detected; capture unverified |
| IR rx/tx (LIRC devices) | built; no `ir-ctl` userspace | same | **works, hw:unit1 (rc6)** with the M5Stack IR-Remote app: recorded a signal and sent it at 38 kHz |
| Expansion: I2C (`i2c-dev`), SPI (`spidev` overlay shipped, not enabled), UART, switched 5 V rails | built | built | img; unverified |
| HDMI | `vc4-kms-v3d` only in the safe-mode config | same | unverified |
| Suspend to RAM | not available on BCM283x kernels (no `mem` sleep state) | same | src |

## Things that cannot be tested without the device

Display init sequence and orientation, SPI clock actually achieved, backlight polarity and
range, key map correctness, audio routing and levels, charging behaviour, real boot time,
idle current, Wi-Fi association, thermal behaviour. See `HARDWARE-TEST-CHECKLIST.md`.
