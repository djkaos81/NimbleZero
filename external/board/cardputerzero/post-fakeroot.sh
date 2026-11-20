#!/bin/sh
# Runs on the final rootfs copy just before the filesystem image is created
# (after Buildroot has created the users and applied permissions).
T="$1"
if [ -x "$T/sbin/ldconfig" ] && [ -x "$T/usr/bin/dpkg" ]; then
	# Debian multiarch library directories, so libraries shipped inside .deb packages resolve
	# (Buildroot refuses an /etc/ld.so.conf in the persistent target directory.)
	printf '/usr/local/lib\n/usr/lib/aarch64-linux-gnu\n/lib/aarch64-linux-gnu\n' > "$T/etc/ld.so.conf"
fi
# Accounts.  root has no usable password; "zero" has none until the first boot asks for
# one.  The password file must be writable on a read-only system partition, so it lives
# on the data partition: /etc/shadow -> /data/etc/shadow, seeded from /etc/shadow.dist
# by S12nz-data.
if [ -f "$T/etc/shadow" ] && [ ! -L "$T/etc/shadow" ] && [ -e "$T/etc/inittab" ]; then
	sed -i 's|^root:[^:]*:|root:*:|; s|^zero:[^:]*:|zero:!:|' "$T/etc/shadow"
	mv "$T/etc/shadow" "$T/etc/shadow.dist"; chmod 600 "$T/etc/shadow.dist"
	ln -s /data/etc/shadow "$T/etc/shadow"
fi
# PulseAudio runs as one system service (S50pulseaudio).  It only accepts clients whose user
# is in the group pulse-access; without this every program got "Access denied" (hardware
# report on rc10: Cardputer TV, "PulseAudio outdev ... Context failed").  Apps run as zero
# (both launchers start them as that user); root is added for programs run with sudo.  A client must also never start a
# private second service next to the system one.
if grep -q '^pulse-access:' "$T/etc/group" 2>/dev/null; then
	for u in pulse zero root; do
		grep -q "^pulse-access:[^:]*:[^:]*:\(.*,\)\?$u\(,.*\)\?$" "$T/etc/group" || \
			sed -i "s/^\(pulse-access:[^:]*:[^:]*:.\+\)$/\1,$u/; s/^\(pulse-access:[^:]*:[^:]*:\)$/\1$u/" "$T/etc/group"
	done
	[ -f "$T/etc/pulse/client.conf" ] && sed -i 's/^; *autospawn *=.*/autospawn = no/' "$T/etc/pulse/client.conf"
fi
if [ -d "$T/etc/sudoers.d" ]; then chown 0:0 "$T"/etc/sudoers.d/* 2>/dev/null; chmod 0440 "$T"/etc/sudoers.d/* 2>/dev/null; fi
[ -d "$T/home/zero" ] && chown -R 1000:1000 "$T/home/zero"
exit 0
