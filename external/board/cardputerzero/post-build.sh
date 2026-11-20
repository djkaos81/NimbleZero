#!/bin/bash
# Target rootfs fix-ups common to all Cardputer Zero configurations.
set -euo pipefail
BOARD_DIR="$(dirname "$0")"
KVER=$(ls "${TARGET_DIR}/lib/modules" | head -n1)

# The M5Stack out-of-tree IMU drivers replace the in-tree ones of the same name.
find "${TARGET_DIR}/lib/modules/${KVER}/kernel/drivers/iio" \
	\( -name 'bmi270*.ko*' -o -name 'bmc150_magn*.ko*' \) -delete 2>/dev/null || true
"${HOST_DIR}/sbin/depmod" -a -b "${TARGET_DIR}" "${KVER}"

# Build identification
{
	echo "NAME=\"NimbleZero\""
	echo "VARIANT=${NZ_VARIANT:-$(basename "${BR2_CONFIG%/.config}")}"
	echo "VERSION=$(git -C "${BR2_EXTERNAL_NZ_PATH}" describe --tags --always --dirty 2>/dev/null || echo unknown)"
	echo "BUILDROOT=${BR2_VERSION_FULL:-unknown}"
	echo "KERNEL=${KVER}"
	echo "BUILD_DATE=$(date -u -d "@${SOURCE_DATE_EPOCH:-$(date +%s)}" +%Y-%m-%dT%H:%M:%SZ)"
} > "${TARGET_DIR}/etc/nz-release"

mkdir -p "${TARGET_DIR}/boot" "${TARGET_DIR}/data"
# boot partition: read-only unless a script needs to write a log (same for every init system)
sed -i '/[[:space:]]\/boot[[:space:]]/d' "${TARGET_DIR}/etc/fstab"
echo '/dev/mmcblk0p1	/boot		vfat	ro,noatime,nofail	0	0' >> "${TARGET_DIR}/etc/fstab"

# root filesystem: no access-time updates (fewer SD-card writes)
sed -i 's|^\(/dev/root[[:space:]]*/[[:space:]]*[a-z0-9]*[[:space:]]*\)rw,noauto|\1rw,noatime,noauto|' "${TARGET_DIR}/etc/fstab"

if [ -e "${TARGET_DIR}/etc/inittab" ]; then
	# BusyBox-init images run from a READ-ONLY root filesystem: the power switch can cut
	# power at any time.  Persistent state lives on the data partition (S12nz-data).
	sed -i 's|^\(/dev/root[[:space:]]*/[[:space:]]*[a-z0-9]*[[:space:]]*\)rw,|\1ro,|' "${TARGET_DIR}/etc/fstab"
	sed -i '\|/bin/mount -o remount,rw /$|d' "${TARGET_DIR}/etc/inittab"
	rm -f "${TARGET_DIR}/etc/init.d/S01seedrng"      # hardware RNG seeds the kernel; no seed file to write
	ln -snf /data/etc/dropbear "${TARGET_DIR}/etc/dropbear"
fi

# BusyBox-init images: no cron (nothing is scheduled; it would wake the CPU every
# minute) and log to /var/log/messages, which is in RAM
rm -f "${TARGET_DIR}/etc/init.d/S50crond"
if [ -e "${TARGET_DIR}/etc/init.d/S01syslogd" ]; then
	mkdir -p "${TARGET_DIR}/etc/default"
	# /var/log is in RAM; keep at most two 256 KB files (this BusyBox has no ring-buffer mode)
	echo 'SYSLOGD_ARGS="-s 256 -b 1"' > "${TARGET_DIR}/etc/default/syslogd"
fi

if [ -e "${TARGET_DIR}/etc/inittab" ]; then
	# BusyBox init: launcher (or login) on the LCD; login on the UART.  nz-console waits quietly when a port does not exist, so a
	# missing UART can never turn into a respawn loop that burns power.
	sed -i '/nz-console/d;/^tty1::/d;/^[^#].*getty.*GENERIC_SERIAL/d' "${TARGET_DIR}/etc/inittab"
	cat >> "${TARGET_DIR}/etc/inittab" <<'EOF'
tty1::respawn:/usr/bin/nz-console tty1
::respawn:/usr/bin/nz-console ttyS0
EOF
	sed -i '/nz-halt-msg/d' "${TARGET_DIR}/etc/inittab"      # rc3-rc5 hook, removed
	rm -f "${TARGET_DIR}/usr/bin/nz-halt-msg"
fi

# dpkg-enabled images: register the firmware runtime and protect it (nz-compat)
if [ -x "${TARGET_DIR}/usr/bin/dpkg" ]; then
	"${BR2_EXTERNAL_NZ_PATH}/package/nz-compat/files/seed-dpkg.sh" \
		"${BR2_EXTERNAL_NZ_PATH}/package/nz-compat/files/provided.map"
fi

# Read-only-root images: /var/lib becomes a bind mount of the data partition at boot
# (programs keep their state there), but the package database must stay on the same
# partition as the files it describes.  Keep it at /var/dpkg-db and reach it through a
# link that the data copy of /var/lib inherits.
if [ -e "${TARGET_DIR}/etc/inittab" ] && [ -d "${TARGET_DIR}/var/lib/dpkg" ] && [ ! -L "${TARGET_DIR}/var/lib/dpkg" ]; then
	rm -rf "${TARGET_DIR}/var/dpkg-db"
	mv "${TARGET_DIR}/var/lib/dpkg" "${TARGET_DIR}/var/dpkg-db"
	ln -s ../dpkg-db "${TARGET_DIR}/var/lib/dpkg"
fi

# Settings that must be writable live on the data partition (the system partition is read-only)
if [ -e "${TARGET_DIR}/etc/inittab" ]; then
	ln -snf /data/etc/localtime "${TARGET_DIR}/etc/localtime"     # time zone (Options menu); UTC when absent
	mkdir -p "${TARGET_DIR}/home/zero"
fi
rm -f "${TARGET_DIR}/etc/init.d/S49ntp" "${TARGET_DIR}/etc/sudoers.d/nz-sudo-group"

# Licences.  The project's own licence always; the per-package licence texts and the
# manifest once `make legal-info` has run (build.sh builds, runs legal-info and the
# licence check, then builds again so that the image carries them).
LIC="${TARGET_DIR}/usr/share/licenses"
rm -rf "${LIC}"; mkdir -p "${LIC}/NimbleZero"
cp "${BR2_EXTERNAL_NZ_PATH}/../COPYING" "${LIC}/NimbleZero/COPYING"
mkdir -p "${LIC}/toolchain" && cp "${BR2_EXTERNAL_NZ_PATH}"/../docs/third-party/toolchain/* "${LIC}/toolchain/"
cat > "${LIC}/README" <<'L'
NimbleZero is an unofficial firmware for the M5Stack Cardputer Zero. It is not an
M5Stack product and is not endorsed by M5Stack or Raspberry Pi Ltd.

The project's own programs (nz-*) are licensed under the GNU General Public License,
version 2 (NimbleZero/COPYING). Every other component keeps its own licence: see
manifest.csv (package, version, licence, source archive) and the directory named after
each package. Binary-only files redistributed under their vendors' terms (texts
included): the Raspberry Pi boot firmware (rpi-firmware-*/), the Wi-Fi/Bluetooth firmware
(brcmfmac_sdio-firmware-rpi-*/: Broadcom, Cypress and Synaptics agreements) and the
BMI270 motion-sensor configuration data /lib/firmware/bmi270-init-data.fw, which is
Bosch Sensortec's, BSD-3-Clause (bosch-bmi270/).

Complete corresponding source for this image (every source archive, the Buildroot
configuration and this project's tree) is published with the release it came from;
the release tag is in /etc/nz-release.
L
if [ -f "${TARGET_DIR}/lib/firmware/bmi270-init-data.fw" ]; then
	mkdir -p "${LIC}/bosch-bmi270" && cp "${BR2_EXTERNAL_NZ_PATH}"/../docs/third-party/bosch-bmi270/* "${LIC}/bosch-bmi270/"
fi
if [ -f "${BASE_DIR}/legal-info/manifest.csv" ]; then
	cp "${BASE_DIR}/legal-info/manifest.csv" "${LIC}/manifest.csv"
	cp -r "${BASE_DIR}/legal-info/licenses/." "${LIC}/"
fi

# Wi-Fi/Bluetooth firmware: the Raspberry Pi firmware set covers every Pi board.  A
# Compute Module 0 carries a 43439 (Cypress/Infineon) or, going by that set's own
# "0-compute-module" entries, a 43436s (Synaptics); the 43430 names are kept because the
# 43436s files are reached through them.  The 43455/43456 families (Pi 3B+/4/5, CM4, 400,
# 500) cannot be on this board: removed.  hw:unit1 reports BCM43439.
rm -f "${TARGET_DIR}"/lib/firmware/brcm/brcmfmac43455-sdio.* "${TARGET_DIR}"/lib/firmware/brcm/brcmfmac43456-sdio.* \
	"${TARGET_DIR}"/lib/firmware/brcm/BCM4345C*.hcd "${TARGET_DIR}"/lib/firmware/cypress/cyfmac43455-sdio.*

# v4l-utils installs two iconv modules for DVB text that need glibc helper libraries
# this image does not carry; nothing here uses them.
rm -f "${TARGET_DIR}"/usr/lib/gconv/ARIB-STD-B24.so "${TARGET_DIR}"/usr/lib/gconv/EN300-468-TAB00.so
rm -rf "${TARGET_DIR}"/usr/lib/gconv/gconv-modules.d/libdvbv5.conf 2>/dev/null || true

# Python as Debian packages expect it: .deb apps install their modules into
# /usr/lib/python3/dist-packages and look for fonts under /usr/share/fonts/truetype.
for sp in "${TARGET_DIR}"/usr/lib/python3.*/site-packages; do
	[ -d "$sp" ] || continue
	echo /usr/lib/python3/dist-packages > "$sp/nz-debian.pth"
	mkdir -p "${TARGET_DIR}/usr/lib/python3/dist-packages"
done
if [ -d "${TARGET_DIR}/usr/share/fonts/dejavu" ]; then
	mkdir -p "${TARGET_DIR}/usr/share/fonts/truetype"
	ln -snf ../dejavu "${TARGET_DIR}/usr/share/fonts/truetype/dejavu"
fi
