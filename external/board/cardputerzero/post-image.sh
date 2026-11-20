#!/bin/bash
# Assemble sdcard.img: FAT boot partition (firmware, kernel, DTBs, overlays) + ext4 rootfs.
set -euo pipefail
BOARD_DIR="$(dirname "$0")"
GENIMAGE_CFG="${BINARIES_DIR}/genimage.cfg"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"

install -m 0644 "${BOARD_DIR}/config-safe.txt" "${BOARD_DIR}/cmdline-safe.txt" "${BINARIES_DIR}/rpi-firmware/"
# M5Stack board overlays built by the nz-drivers package
cp "${BINARIES_DIR}"/nz-overlays/*.dtbo "${BINARIES_DIR}/rpi-firmware/overlays/"
# marker files read by the first-boot scripts (see docs/BOOT.md)
: > "${BINARIES_DIR}/rpi-firmware/nz-debug"
cat > "${BINARIES_DIR}/rpi-firmware/wifi.conf.example" <<'W'
# Rename to wifi.conf to join a network on boot (wpa_supplicant syntax).
country=US
network={
	ssid="YourNetwork"
	psk="YourPassword"
}
W
cp "${TARGET_DIR}/etc/nz-release" "${BINARIES_DIR}/rpi-firmware/nz-release.txt"

FILES=()
for i in "${BINARIES_DIR}"/*.dtb "${BINARIES_DIR}"/rpi-firmware/*; do
	FILES+=( "${i#"${BINARIES_DIR}"/}" )
done
FILES+=( "$(sed -n 's/^kernel=//p' "${BINARIES_DIR}/rpi-firmware/config.txt")" )
BOOT_FILES=$(printf '\\t\\t\\t"%s",\\n' "${FILES[@]}")
sed "s|#BOOT_FILES#|${BOOT_FILES}|" "${BOARD_DIR}/genimage.cfg.in" > "${GENIMAGE_CFG}"

# empty placeholder for the data partition (formatted on the device at first start)
rm -f "${BINARIES_DIR}/data.img"; truncate -s 16M "${BINARIES_DIR}/data.img"

ROOTPATH_TMP="$(mktemp -d)"
trap 'rm -rf "${ROOTPATH_TMP}"' EXIT
rm -rf "${GENIMAGE_TMP}"
genimage --rootpath "${ROOTPATH_TMP}" --tmppath "${GENIMAGE_TMP}" \
	--inputpath "${BINARIES_DIR}" --outputpath "${BINARIES_DIR}" --config "${GENIMAGE_CFG}"

# The image file stops where the data partition starts: the partition table still
# describes it (type DA), but writing the image to a card does not touch its contents.
# A card that already holds a NimbleZero data partition therefore keeps the user's
# settings and files across a reflash (S12nz-data adopts it at the next start).
P3_START=$(od -A n -t u4 -j $((446 + 2 * 16 + 8)) -N 4 "${BINARIES_DIR}/sdcard.img" | tr -d ' ')
truncate -s $((P3_START * 512)) "${BINARIES_DIR}/sdcard.img"
echo "sdcard.img: data partition starts at sector ${P3_START}; image ends there"
