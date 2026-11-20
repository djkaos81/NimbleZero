#!/bin/sh
# NimbleZero display test kit for rc10 (both variants), build 4.  Run on the device:
#   sudo sh install.sh install [60|70|100] [note]   test driver + panel clock (default 100 MHz)
#                                                   + sync pin overlay ("note" = without it) + Doom
#   sudo sh install.sh boot-default on|off          on: the smooth setting is active from power-on,
#                                                   for everything on the screen; off: switches
#                                                   start at 0 again (the default)
#   sudo sh install.sh restore                      original driver and boot settings back
#   sudo sh install.sh status
# Nothing here touches the data partition except the folder /home/zero/doom.
set -u
HERE=$(cd "$(dirname "$0")" && pwd); F=$HERE/files
KREL=6.18.60-v8-cz
CONF=/etc/modprobe.d/nz-panel.conf
M=/lib/modules/$KREL/updates/panel-mipi-dbi-m.ko
STOCK=1368429706b84e784c6d9764a042d614b39765ba885538e1e263a29c7e31e629
TEST=0e744a826daed08d55f7814641747f9603a63e5e7ce871662de36f566b12c152
TEST3=d4f0528679e644b36e5a8c7490faa8c184768ee2ad2f670c933ec6fb7662bb4c   # build 3 (background sending)
TEST2=dda460bbbc50152bf582a9caf080cf8cc1571f92d2ad54ec3fa7fd2935e48de8   # build 2 (cached buffers)
TEST1=f1cbf3d58ac0322ff88d28b950b83cc240150e380e515a1f772250721bf47b89   # first build of the kit (2026-11-19, without cached buffers)
PARTIAL=""
die() { echo "install.sh: $*" >&2; [ -n "$PARTIAL" ] && echo "install.sh: the installation is incomplete.  Put things back with:  sudo sh install.sh restore" >&2; exit 1; }
sum() { sha256sum "$1" 2>/dev/null | cut -d' ' -f1; }
[ "$(id -u)" = 0 ] || die "run with sudo"
state() { case "$(sum $M)" in $STOCK) echo original ;; $TEST) echo test ;; $TEST1) echo "test (first build)" ;; $TEST2) echo "test (build 2)" ;; $TEST3) echo "test (build 3)" ;; *) echo unknown ;; esac; }
bootcfg() {  # $1 = lines to add after the board overlay ("" = none)
	mount -o remount,rw /boot || die "cannot make the boot partition writable"
	[ -e /boot/config-before-nz-kit.txt ] || cp /boot/config.txt /boot/config-before-nz-kit.txt
	awk -v add="$1" '/^dtoverlay=nz-panel-/ {next} {print}
		/^dtoverlay=cardputerzero-v5-overlay/ && add != "" { n = split(add, a, " "); for (i = 1; i <= n; i++) print "dtoverlay=" a[i]; done = 1 }
		END { if (add != "" && !done) exit 3 }' /boot/config.txt > /boot/config.new
	rc=$?
	if [ $rc = 0 ] && [ -s /boot/config.new ]; then cat /boot/config.new > /boot/config.txt; fi
	rm -f /boot/config.new; sync; mount -o remount,ro /boot
	[ $rc = 0 ] || die "config.txt has no line dtoverlay=cardputerzero-v5-overlay; boot settings left unchanged"
}
case "${1:-}" in
status)
	echo "kernel: $(uname -r)   image: $(sed -n 's/^VERSION=//p' /etc/nz-release 2>/dev/null)"
	echo "display driver on the card: $(state)"
	echo "display driver running: $([ -e /sys/module/panel_mipi_dbi_m/parameters/nz_path ] && echo test || echo original)"
	echo "boot settings: $(grep '^dtoverlay=nz-panel-' /boot/config.txt | tr '\n' ' ')"
	echo "Doom: $([ -x /home/zero/doom/nz-doom ] && echo installed || echo "not installed")"
	echo "smooth setting from power-on: $([ -e $CONF ] && echo on || echo off)"
	;;
boot-default)
	case "${2:-}" in
	on)
		[ "$(state)" = test ] || die "install build 4 of the test driver first (sudo sh install.sh install)"
		nz-rw sh -c "mkdir -p /etc/modprobe.d && echo 'options panel_mipi_dbi_m nz_path=1 nz_native=2 nz_te=1 nz_cached=1 nz_async=1 nz_fbdev_ms=16' > $CONF" || die "writing to the system partition failed"
		echo "From the next start the smooth setting is active for everything on the screen.  Restart now (sudo reboot)."
		echo "Back to switches-off at start:  sudo sh install.sh boot-default off"
		;;
	off)
		nz-rw rm -f $CONF || die "writing to the system partition failed"
		echo "Switches start at 0 again from the next start."
		;;
	*) die "boot-default on|off" ;;
	esac
	;;
install)
	CLK=${2:-100}; TE=${3:-te}
	case "$CLK" in 60) OV="" ;; 70|100) OV="nz-panel-${CLK}m" ;; *) die "clock must be 60, 70 or 100" ;; esac
	[ "$TE" = te ] && OV="$OV nz-panel-te"
	[ "$(uname -r)" = $KREL ] || die "this kit is for kernel $KREL (NimbleZero rc10); found $(uname -r)"
	[ "$(sum $F/panel-mipi-dbi-m.test.ko)" = $TEST ] && [ "$(sum $F/panel-mipi-dbi-m.stock.ko)" = $STOCK ] || die "kit files are damaged (checksum); copy the kit again"
	[ "$(state)" != unknown ] || die "the display driver on this card is not the rc10 one; nothing changed"
	nz-rw sh -c "mkdir -p /usr/lib/nz-kit && cp $F/panel-mipi-dbi-m.stock.ko /usr/lib/nz-kit/ && cat $F/panel-mipi-dbi-m.test.ko > $M && cp $F/S01nz-kit /etc/init.d/S01nz-kit && chmod 755 /etc/init.d/S01nz-kit && cp $F/nz-paneltest $HERE/nz-panel.sh /usr/lib/nz-kit/ && chmod 755 /usr/lib/nz-kit/nz-paneltest /usr/lib/nz-kit/nz-panel.sh" || die "writing to the system partition failed"
	PARTIAL=1
	[ "$(state)" = test ] || die "the test driver did not arrive intact"
	mount -o remount,rw /boot || die "cannot make the boot partition writable"
	cp $F/nz-panel-70m.dtbo $F/nz-panel-100m.dtbo $F/nz-panel-te.dtbo /boot/overlays/ || die "cannot copy the overlays"
	bootcfg "$OV"
	# read the boot partition back from the card (not from memory) and compare
	sync; echo 3 > /proc/sys/vm/drop_caches
	for o in nz-panel-70m nz-panel-100m nz-panel-te; do cmp -s $F/$o.dtbo /boot/overlays/$o.dtbo || die "overlay $o did not arrive intact on the boot partition"; done
	for o in $OV; do grep -qx "dtoverlay=$o" /boot/config.txt || die "config.txt is missing dtoverlay=$o"; done
	grep -qx 'dtoverlay=cardputerzero-v5-overlay' /boot/config.txt && grep -qx 'kernel=Image' /boot/config.txt || die "config.txt looks damaged; on a PC, copy config-before-nz-kit.txt over config.txt on NZBOOT"
	D=/home/zero/doom; A=/home/zero/.local/share/applications
	mkdir -p $D $A && cp $F/nz-doom $F/freedoom1.wad $F/FREEDOOM-COPYING.txt $D/ && chmod 755 $D/nz-doom || die "cannot install Doom"
	printf '[Desktop Entry]\nType=Application\nName=Doom (Freedoom)\nComment=display test\nExec=/home/zero/doom/nz-doom -iwad /home/zero/doom/freedoom1.wad\nPath=/home/zero/doom\nTerminal=false\n' > $A/nz-doom.desktop
	chown -R zero:zero $D /home/zero/.local
	sync; PARTIAL=""
	echo "Installed: test display driver, boot settings [$OV], Doom."
	echo "Now restart the device (sudo reboot), then run:  sudo sh /usr/lib/nz-kit/nz-panel.sh auto"
	;;
restore)
	[ -s /usr/lib/nz-kit/panel-mipi-dbi-m.stock.ko ] && S=/usr/lib/nz-kit/panel-mipi-dbi-m.stock.ko || S=$F/panel-mipi-dbi-m.stock.ko
	[ "$(sum $S)" = $STOCK ] || die "no intact copy of the original driver found"
	nz-rw sh -c "cat $S > $M; rm -f /etc/init.d/S01nz-kit $CONF" || die "writing to the system partition failed"
	[ "$(state)" = original ] || die "restore did not take; reflashing rc10 puts everything back (your data is kept)"
	bootcfg ""
	sync
	echo "Original display driver and boot settings are back.  Restart the device (sudo reboot)."
	echo "Doom stays in /home/zero/doom (it does not need the test driver)."
	;;
*) sed -n '2,10p' "$0"; exit 2 ;;
esac
