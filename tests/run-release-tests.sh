#!/bin/sh
# Emulator test sequence for a release image (compat variant): four boots of one card.
#   usage: tests/run-release-tests.sh sdcard.img fixtures.img OUTDIR
# fixtures.img: tests/compat/make-fixtures.sh (app index, two index apps, M5Stack packages).
#  1  first boot with a password typed at the prompt; launcher keys, menus, Store, package
#     operations on running programs; ends like the POWER switch with a deleted file still open
#  2  the card boots again; nothing lost
#  3  the image is written over the card again ("reflash"): the data partition is adopted
#  4  after an erase request: formatted again, first-boot prompt again
set -u
img=$1; fx=$2; out=$3; T="$(dirname "$0")/qemu-boot.py"; S="$(dirname "$0")/compat"
mkdir -p "$out"
NZ_FIRSTBOOT_PW=nzkey12 NZ_HARD_OFF=1 NZ_SAVE_DISK="$out/card.img" "$T" "$img" "$out/1-release" --script "$S/release.script" --usb-drive "$fx" > "$out/1-release.txt" 2>&1
NZ_SECOND_BOOT=1 NZ_SAVE_DISK="$out/card.img" "$T" "$out/card.img" "$out/2-after-switch-off" --script "$S/second-boot.script" > "$out/2-after-switch-off.txt" 2>&1
dd if="$img" of="$out/card.img" conv=notrunc bs=1M status=none
NZ_SECOND_BOOT=1 NZ_HARD_OFF=1 NZ_SAVE_DISK="$out/card.img" "$T" "$out/card.img" "$out/3-after-reflash" --script "$S/after-reflash.script" > "$out/3-after-reflash.txt" 2>&1
"$T" "$out/card.img" "$out/4-after-erase" --script "$S/after-erase.script" > "$out/4-after-erase.txt" 2>&1
rm -f "$out/card.img"
for f in "$out"/[1-4]-*.txt; do echo "== $f: $(tail -n 1 "$f")"; grep '^FAIL' "$f"; done
