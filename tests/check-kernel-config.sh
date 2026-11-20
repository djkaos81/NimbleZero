#!/bin/sh
# Verify that the kernel configuration the build actually used still contains
# every option the Cardputer Zero needs (the module allowlist in
# scripts/gen-kernel-fragment.py could otherwise drop something silently).
O=$1; C=$(ls "$O"/build/linux-custom/.config 2>/dev/null) || { echo "no kernel config in $O" >&2; exit 1; }
rc=0
for s in ARM64 MMC_BCM2835 MMC_BCM2835_MMC EXT4_FS VFAT_FS DEVTMPFS DRM DRM_MIPI_DBI DRM_FBDEV_EMULATION FRAMEBUFFER_CONSOLE \
	FONT_6x8 BACKLIGHT_CLASS_DEVICE PWM I2C_BCM2835 I2C_CHARDEV SPI_BCM2835 SPI_SPIDEV INPUT_EVDEV INPUT_MATRIXKMAP \
	SND_SOC SND_BCM2835_SOC_I2S SND_SIMPLE_CARD POWER_SUPPLY IIO IIO_BUFFER IIO_TRIGGERED_BUFFER REGMAP_I2C \
	RTC_DRV_DS1307 CFG80211 BRCMFMAC BRCMFMAC_SDIO BT BT_HCIUART BT_HCIUART_BCM USB_DWC2 USB_GADGET USB_G_SERIAL USB_ETH \
	USB_USBNET USB_NET_AX8817X USB_NET_SR9700 USB_NET_SR9800 IR_GPIO_CIR IR_GPIO_TX VIDEO_IMX219 VIDEO_BCM2835_UNICAM \
	LEDS_GPIO GPIOLIB BCM2835_WDT CPU_FREQ CPU_IDLE ZRAM FUSE_FS OVERLAY_FS SQUASHFS IPV6 TUN CGROUPS FB_BCM2708; do
	grep -qE "^CONFIG_$s=[ym]" "$C" || { echo "MISSING kernel option CONFIG_$s"; rc=1; }
done
for s in MODULE_COMPRESS_XZ BATTERY_BQ27XXX; do
	grep -qE "^CONFIG_$s=[ym]" "$C" && { echo "UNEXPECTED kernel option CONFIG_$s"; rc=1; }
done
[ $rc = 0 ] && echo "kernel config check: OK ($(grep -c '=m$' "$C") modules, $(grep -c '=y$' "$C") built-in)"
exit $rc
