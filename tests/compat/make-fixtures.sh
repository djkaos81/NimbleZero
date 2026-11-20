#!/bin/sh
# Build the fixture disk for tests/run-release-tests.sh (needs network, mtools, dosfstools).
#   usage: make-fixtures.sh M5_DEB_DIR OUT.img
# M5_DEB_DIR: upstream M5Stack .deb files (applaunch 0.6.106, ir-remote, music, recorder,
# files, piano, keyboard-guide), downloaded from github.com/CardputerZero releases.
set -e
D=$1; O=$2; W=$(mktemp -d)
I=https://cardputerzero.github.io/packages/dists/stable/main/binary-arm64/Packages
curl -fsSL -o "$W/Packages" "$I"
mkdir "$W/pool"
for p in kanstl_0.1.0_arm64.deb esp32c5-spectrum_0.1.0_arm64.deb airfiles_0.4.0_arm64.deb fontpreview_0.4.7-1_arm64.deb refract-pocket_6.5_arm64.deb; do
	curl -fsSL -o "$W/pool/$p" "https://github.com/CardputerZero/packages/releases/download/apt-pool/$p"
done
rm -f "$O"; truncate -s 96M "$O"; mkfs.vfat -n NZTEST "$O" >/dev/null
mmd -i "$O" ::pool; mcopy -i "$O" "$W/Packages" ::; mcopy -i "$O" "$W"/pool/*.deb ::pool/
mcopy -i "$O" "$D"/applaunch_0.6.106-m5stack1_arm64.deb "$D"/m5cardputerzero-ir-remote_0.1.4_m5stack1_arm64.deb \
	"$D"/m5cardputerzero-music_0.1.7_m5stack1_arm64.deb "$D"/m5cardputerzero-recorder_0.1.8_m5stack1_arm64.deb \
	"$D"/m5cardputerzero-files_1.0.3_m5stack1_arm64.deb "$D"/m5cardputerzero-piano_0.1.1_m5stack1_arm64.deb \
	"$D"/m5cardputerzero-keyboard-guide_1.0.1_m5stack1_arm64.deb ::
rm -rf "$W"; echo "$O"
