#!/bin/bash
# Build-time step for dpkg-enabled images (called from post-build.sh).
#  1. Registers the Buildroot-provided runtime in the dpkg database as held
#     "firmware" packages, so that real .deb packages with ordinary Debian
#     Depends: lines install without APT.
#  2. Records every firmware file as owned by the essential package "nz-firmware"
#     (/var/lib/dpkg/info/nz-firmware.list).  dpkg refuses packages that ship one
#     of those files, and the /usr/bin/dpkg guard refuses them even when forced.
set -euo pipefail
MAP="$1"; T="${TARGET_DIR}"
ST="$T/var/lib/dpkg/status"; : > "$ST"
ver_of() {
	case "$1" in
	@glibc) strings "$T/lib/libc.so.6" | sed -n 's/^GNU C Library.*release version \([0-9.]*\).*/\1/p' | head -n1 ;;
	@gcc) # ask the cross compiler (the libraries on the target are stripped of their version note)
		cc=$(ls "${HOST_DIR}"/bin/*-linux-gcc 2>/dev/null | head -n1)
		v=$([ -n "$cc" ] && "$cc" -dumpfullversion 2>/dev/null)
		[ -n "$v" ] || v=$(strings "$T/usr/lib/libstdc++.so.6" | sed -n 's/^GCC: (.*) \([0-9.]*\)$/\1/p' | head -n1)
		echo "$v" ;;
	*) { ls -d "${BUILD_DIR}/$1"-[0-9]* 2>/dev/null || true; } | head -n1 | sed "s|.*/$1-||" ;;
	esac
}
# a package name may appear only once (dpkg refuses a status file with two entries of a name)
dups=$(grep -v '^#' "$MAP" | cut -d'|' -f1 | tr ',' '\n' | grep . | sort | uniq -d)
[ -z "$dups" ] || { echo "seed-dpkg: duplicate names in $MAP: $dups" >&2; exit 1; }
n=0
while IFS='|' read -r names probe src; do
	case "$names" in ''|\#*) continue ;; esac
	[ -e "$T$probe" ] || [ -L "$T$probe" ] || continue
	v=$(ver_of "$src")
	[ -n "$v" ] || { echo "seed-dpkg: no version found for $names ($src)" >&2; exit 1; }
	name=${names%%,*}; prov=$(echo "$names" | cut -s -d, -f2- | sed 's/,/, /g')
	{
		echo "Package: $name"
		echo "Status: hold ok installed"
		echo "Priority: required"
		echo "Section: firmware"
		echo "Maintainer: NimbleZero <root@localhost>"
		echo "Architecture: arm64"
		echo "Multi-Arch: foreign"
		echo "Version: ${v}-nz1"
		[ -z "$prov" ] || echo "Provides: $prov"
		echo "Description: provided by the Buildroot firmware (${src#@})"
		echo " This entry only records that the component exists in the read-only"
		echo " system image.  It owns no files and cannot be upgraded with dpkg."
		echo
	} >> "$ST"
	: > "$T/var/lib/dpkg/info/$name.list"; : > "$T/var/lib/dpkg/info/$name.md5sums"
	n=$((n+1))
done < "$MAP"
echo arm64 > "$T/var/lib/dpkg/arch"

# Firmware file inventory (everything the image ships outside package-managed areas)
LIST=$(mktemp)
(cd "$T" && find bin sbin lib lib64 usr etc opt -xdev \( -type f -o -type l \) 2>/dev/null) |
	grep -vE '^etc/dpkg/|^usr/share/APPLaunch/|^usr/local/|^opt/' | LC_ALL=C sort > "$LIST"
echo "etc/ld.so.conf" >> "$LIST"            # written later by post-fakeroot.sh

# The files are owned by an essential package: an ordinary install of a package
# that ships one of them is refused by dpkg itself ("trying to overwrite ...,
# which is also in package nz-firmware").  Forced installs are stopped earlier by
# the guard in /usr/bin/dpkg, which uses the same inventory.
rm -f "$T/etc/dpkg/dpkg.cfg.d/nz-protect"
{
	echo "Package: nz-firmware"
	echo "Essential: yes"
	echo "Status: hold ok installed"
	echo "Priority: required"
	echo "Section: firmware"
	echo "Maintainer: NimbleZero <root@localhost>"
	echo "Architecture: arm64"
	echo "Version: $(sed -n 's/^VERSION=//p' "$T/etc/nz-release" | tr -c 'A-Za-z0-9.+~\n' '.' | sed 's/^[^0-9]*/0+/')"
	echo "Description: NimbleZero system image (owner of all firmware files)"
	echo
} >> "$ST"
{
	echo "/."
	(sed 's|^|/|' "$LIST"; sed 's|^|/|; s|/[^/]*$||' "$LIST" | awk '{p=$0; while (p != "") {print p; sub(/\/[^\/]*$/, "", p)}}') | LC_ALL=C sort -u
} > "$T/var/lib/dpkg/info/nz-firmware.list"
: > "$T/var/lib/dpkg/info/nz-firmware.md5sums"
rm -f "$LIST"

cat > "$T/etc/dpkg/dpkg.cfg.d/nz-defaults" <<'C'
# Never stop for conffile prompts on a device without a real terminal.
force-confold
force-confdef
C
echo "seed-dpkg: registered $n firmware packages, $(wc -l < "$T/var/lib/dpkg/info/nz-firmware.list") paths owned by nz-firmware"
