#!/usr/bin/env python3
"""Boot a Cardputer Zero SD image under QEMU (raspi3b machine) and run smoke tests.

WHAT THIS IS: the real kernel Image, the real root filesystem and the real init
from the image, on QEMU's emulated BCM2837 board (same SoC family as the CM0's
RP3A0).  SUBSTITUTIONS (QEMU cannot emulate the Cardputer Zero itself):
  * device tree: bcm2710-rpi-3-b.dtb from the image + its disable-bt overlay (so the
    PL011 is a console instead of the Bluetooth UART); no firmware stage, no board overlays
  * console: PL011 (ttyAMA0) instead of the mini-UART; display: the firmware
    framebuffer (bcm2708_fb) forced to 320x170 instead of the SPI LCD
  * keyboard: USB HID instead of the TCA8418; no I2C/SPI peripherals, Wi-Fi, audio
  * timing: TCG emulation - boot times and benchmarks are NOT hardware numbers
It therefore validates kernel+userspace boot, init scripts, the diagnostic UI's
fbdev/evdev paths and the instrumentation - not the board drivers.

usage: qemu-boot.py sdcard.img outdir [--script file] [--usb-drive img]...
  script lines: "name :: shell command" (pass = exit 0), "@shot NAME", "@key K...", "@sleep S"
"""
import sys, os, subprocess, tempfile, time, socket, struct, zlib, re, shutil, select, json

img, out = sys.argv[1], sys.argv[2]
extra = open(sys.argv[sys.argv.index("--script") + 1]).read() if "--script" in sys.argv else ""
drives = [sys.argv[i + 1] for i, a in enumerate(sys.argv) if a == "--usb-drive"]
os.makedirs(out, exist_ok=True)
for f in ("script.log",):
    if os.path.exists(os.path.join(out, f)): os.remove(os.path.join(out, f))
tmp = tempfile.mkdtemp(prefix="nzqemu.")
with open(img, "rb") as f:
    mbr = f.read(512)
lba = struct.unpack("<I", mbr[446 + 8:446 + 12])[0]
for f in ["Image", "bcm2710-rpi-3-b.dtb", "overlays/disable-bt.dtbo"]:
    subprocess.run(["mcopy", "-n", "-i", f"{img}@@{lba * 512}", f"::{f}", os.path.join(tmp, os.path.basename(f))], check=True)
# what the firmware would do for "dtoverlay=disable-bt": give the PL011 back to the console
subprocess.run(["fdtoverlay", "-i", os.path.join(tmp, "bcm2710-rpi-3-b.dtb"), "-o", os.path.join(tmp, "qemu.dtb"), os.path.join(tmp, "disable-bt.dtbo")], check=True)
work = os.path.join(tmp, "sd.img"); shutil.copy(img, work)
size = os.path.getsize(work); p2 = 1
while p2 < size: p2 <<= 1
os.truncate(work, p2)                       # QEMU SD cards must be a power of two
mon = os.path.join(tmp, "mon.sock")
cmdline = ("root=/dev/mmcblk0p2 rootfstype=ext4 rootwait ro console=tty1 console=ttyAMA0,115200 fbcon=font:6x8 "
           "bcm2708_fb.fbwidth=320 bcm2708_fb.fbheight=170 bcm2708_fb.fbdepth=16 nz.qemu=1 loglevel=4")
if os.environ.get("NZ_ROOT_RW"): cmdline = cmdline.replace(" rootwait ro ", " rootwait rw ")   # recovery experiment
cmd = ["qemu-system-aarch64", "-M", "raspi3b", "-kernel", os.path.join(tmp, "Image"), "-dtb", os.path.join(tmp, "qemu.dtb"),
       "-drive", f"file={work},if=sd,format=raw", "-append", cmdline, "-serial", "stdio", "-display", "none",
       "-monitor", f"unix:{mon},server,nowait", "-device", "usb-kbd", "-no-reboot"]
for i, d in enumerate(drives):
    cmd += ["-drive", f"file={d},if=none,id=usb{i},format=raw,readonly=on", "-device", f"usb-storage,drive=usb{i}"]
t0 = time.time()
q = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
log = open(os.path.join(out, "serial.log"), "wb")
buf = b""
def expect(pat, timeout):
    global buf
    end = time.time() + timeout
    while time.time() < end:
        m = re.search(pat, buf)
        if m:
            pre = buf[:m.end()]; buf = buf[m.end():]; return pre
        r, _, _ = select.select([q.stdout], [], [], 1)
        if r:
            d = os.read(q.stdout.fileno(), 65536)
            if not d: break
            log.write(d); log.flush(); buf += d
        if q.poll() is not None: break
    return None
def send(s): q.stdin.write(s.encode()); q.stdin.flush()
def monitor(c):
    s = socket.socket(socket.AF_UNIX); s.connect(mon); s.settimeout(5)
    try: s.recv(4096)
    except socket.timeout: pass
    s.sendall((c + "\n").encode()); time.sleep(1.0)
    try: s.recv(65536)
    except socket.timeout: pass
    s.close()
def png(ppm, dst):
    d = open(ppm, "rb").read(); parts = d.split(b"\n", 3)
    w, h = map(int, parts[1].split()); px = parts[3]
    raw = b"".join(b"\x00" + px[y * w * 3:(y + 1) * w * 3] for y in range(h))
    ch = lambda t, c: struct.pack(">I", len(c)) + t + c + struct.pack(">I", zlib.crc32(t + c) & 0xffffffff)
    open(dst, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + ch(b"IDAT", zlib.compress(raw, 9)) + ch(b"IEND", b""))
    return w, h
def shot(name):
    p = os.path.join(tmp, name + ".ppm"); monitor(f"screendump {p}")
    return png(p, os.path.join(out, name + ".png")) if os.path.exists(p) else None
def sh(c, timeout=120):
    send(c + "; echo __RC=$?__\n")
    o = expect(rb"__RC=(\d+)__\r?\n", timeout)
    if o is None: return None, ""
    txt = o.decode(errors="replace")
    rc = int(re.search(r"__RC=(\d+)__", txt.split(c[-20:])[-1] if c[-20:] in txt else txt).group(1))
    txt = txt.replace("\r", "")
    if "echo __RC=$?__" in txt: txt = txt.rsplit("echo __RC=$?__", 1)[1]     # drop the tty echo of the command
    return rc, re.sub(r"__RC=\d+__\s*$", "", txt).lstrip("\n")

res = {"image": os.path.basename(img), "qemu": subprocess.run(["qemu-system-aarch64", "--version"], capture_output=True, text=True).stdout.splitlines()[0], "tests": []}
def T(name, ok, detail=""):
    res["tests"].append({"test": name, "ok": bool(ok), "detail": detail[:400]}); print(("PASS " if ok else "FAIL ") + name, "-", detail[:160].replace("\n", " | "))
# root has no password: under QEMU the image gives the harness a root shell on the serial port
ok = expect(rb"\n(~ )?# ", 600) is not None
res["time_to_login_prompt_s_emulated"] = round(time.time() - t0, 1)
T("boots to the test shell on the serial console", ok, f"{res['time_to_login_prompt_s_emulated']} s wall clock under TCG (not a hardware figure)")
if ok:
    # replace the interactive shell by a plain line reader: no prompt, no echo, no line editing
    send("stty -echo; exec sh -c 'while IFS= read -r l; do eval \"$l\"; done'\n"); time.sleep(2); sh("true")
    rc, o = sh("cat /etc/nz-release; uname -rm"); T("release info / kernel", rc == 0 and "aarch64" in o, o.strip())
    # The boot log (S99nz-bootlog) is written in the background right after start-up and
    # keeps the boot partition writable while it does; wait for it (up to 60 s) so that this
    # check does not depend on how fast the emulator happens to run.
    sh("i=0; while [ $i -lt 30 ] && grep -q ' /boot vfat rw' /proc/mounts; do sleep 2; i=$((i+1)); done", timeout=90)
    rc, o = sh("cat /proc/cmdline; mount | grep -E ' / |/boot |/data '")
    if "/data type" in o:      # rc3 layout: read-only system partition + data partition
        T("root on mmcblk0p2 read-only, boot read-only, data partition read-write", "root=/dev/mmcblk0p2" in o and " / type ext4 (ro" in o and "/boot type vfat (ro" in o and "/dev/mmcblk0p3 on /data type ext4 (rw" in o, o.strip().split("\n", 1)[-1])
        rc, o = sh("! touch /etc/nz-ro-test 2>/dev/null && nz-rw touch /etc/nz-rw-test && ls /etc/nz-rw-test && nz-rw rm /etc/nz-rw-test && grep ' / ext4 ro' /proc/mounts")
        T("system files refuse writes; nz-rw opens the partition and closes it again", rc == 0, o.strip())
        rc, o = sh("cat /run/nz/data-format.log; echo type=$(nz-growpart --type /dev/mmcblk0 3) bs=$(tune2fs -l /dev/mmcblk0p3 | awk '/^Block size/{print $3}'); df -k /data | tail -n1; grep -q formatted /run/nz/data-format.log && [ $(nz-growpart --type /dev/mmcblk0 3) = 83 ] && tune2fs -l /dev/mmcblk0p3 | grep -q '^Block size: *4096' && [ $(df -k /data | awk 'END{print $2}') -gt 60000 ]")
        if not os.environ.get("NZ_SECOND_BOOT"): T("first start created the data filesystem at the full size of the card and marked it done", rc == 0, o.strip()[-300:])
        rc, o = sh("awk '$3==\"S12nz-data\"{print $2-$1}' /run/nz/rcS-times")
        T("first-start storage step took under 10 s in the emulator", rc == 0 and o.strip() and float(o.strip().split()[-1]) < 10, o.strip())
        rc, o = sh("for d in home root; do [ ! -d /$d ] || grep -q \" /$d \" /proc/mounts || exit 1; done; echo persist > /root/nz-test && ls /data/root/nz-test && rm /root/nz-test")
        T("/root (and /home where present) live on the data partition", rc == 0, o.strip())
        rc, o = sh("grep ' /var/lib ' /proc/mounts && mkdir /var/lib/nz-test && rmdir /var/lib/nz-test && { ls -ld /var/lib/dpkg 2>/dev/null | grep -o 'dpkg.*'; pidof syslogd >/dev/null && echo syslogd-running; }")
        T("/var/lib is writable (data partition); system log daemon running", rc == 0 and "syslogd-running" in o, o.strip())
        rc, o = sh("( command -v nz-m5 >/dev/null || { echo no-nz-m5; exit 0; }; curl -V | grep -i '^Protocols' | grep -q ' https' || exit 1; cd /tmp && openssl req -x509 -newkey rsa:2048 -nodes -keyout k.pem -out c.pem -days 2 -subj /CN=localhost >/dev/null 2>&1 && echo hello-over-tls > index.txt && { openssl s_server -quiet -accept 4433 -key k.pem -cert c.pem -WWW >/dev/null 2>&1 & } ; sleep 4; curl -fsS --cacert c.pem https://localhost:4433/index.txt; r=$?; killall openssl 2>/dev/null; exit $r )", 240)
        T("curl speaks https (TLS handshake and download against a local test server)", rc == 0 and ("hello-over-tls" in o or "no-nz-m5" in o), o.strip()[-200:])
        rc, o = sh("cat /run/nz/keymap"); T("Sym-layer table loaded into the console keymap (read back, 0 errors)", rc == 0 and ", 0 errors" in o and "32 keys" in o, o.strip())
    else:
        T("root on mmcblk0p2 (rw) and boot partition mounted (ro)", "root=/dev/mmcblk0p2" in o and " / type ext4 (rw" in o and "/boot type vfat (ro" in o, o.strip().split("\n", 1)[-1])
    rc, o = sh("ls /lib/modules/$(uname -r)/modules.dep && lsmod | wc -l"); T("modules for the running kernel", rc == 0, o.strip())
    rc, o = sh("nz-diag --report"); T("nz-diag --report runs (devices absent under QEMU are expected)", rc is not None and "hardware report" in o, o.strip()[-300:])
    open(os.path.join(out, "nz-diag-report.txt"), "w").write(o)
    # ---- first boot: password prompt on the LCD, accounts, SSH policy ----
    pw = os.environ.get("NZ_FIRSTBOOT_PW", "")
    def typ(text):
        for ch in text: monitor(f"sendkey {ch}"); time.sleep(0.6)
        monitor("sendkey ret"); time.sleep(1.5)
    rc, o = sh("n=0; while ! ps ax | grep -q '[n]z-firstboot' && [ $n -lt 30 ]; do sleep 1; n=$((n+1)); done; ps ax | grep -q '[n]z-firstboot' && grep '^root:' /etc/shadow | cut -d: -f1,2 && grep '^zero:' /etc/shadow | cut -d: -f1,2")
    first = rc == 0
    if os.environ.get("NZ_SECOND_BOOT"):
        T("later boot: no password prompt", not first, o.strip().replace("\n", " "))
    else:
        T("first boot: password prompt on the LCD; root locked, zero without a password", rc == 0 and "root:*" in o and "zero:!" in o, o.strip().replace("\n", " "))
    s_ = shot("firstboot-prompt"); T("framebuffer screenshot 320x170", s_ == (320, 170), str(s_))
    if first:      # the emulated USB keyboard drops the first key press after a long idle time
        monitor("sendkey shift"); time.sleep(1)
    if first and pw:
        # The emulated USB keyboard sometimes loses a key press.  The prompt then sees two
        # different entries and asks again; type the pair again (up to three attempts).
        for attempt in range(3):
            typ(pw); typ(pw); time.sleep(6)
            rc, o = sh("ls /data/etc/nz/firstboot-done")
            if rc == 0: break
            print(f"note: password entry {attempt + 1} was not accepted (lost key press in the emulator); typing again", flush=True)
            monitor("sendkey shift"); time.sleep(2)
        rc, o = sh("grep -c '^zero:\\$6\\$' /etc/shadow && ls /data/etc/nz/firstboot-done && cat /data/etc/nz/ssh && /etc/init.d/S50dropbear status && tr '\\0' ' ' < /proc/$(cat /var/run/dropbear.pid)/cmdline")
        T("password typed twice on the keyboard: stored as a SHA-512 hash, SSH started (with -w: no root login)", rc == 0 and " -w" in o, o.strip().replace("\n", " "))
        rc, o = sh(f"DROPBEAR_PASSWORD='{pw}' dbclient -y -y zero@127.0.0.1 'id -un; id -u' 2>&1 | tail -n 2; DROPBEAR_PASSWORD='{pw}' dbclient -y -y root@127.0.0.1 id 2>&1 | tail -n 1", 120)
        T("SSH: zero logs in with the password; root is refused", "zero" in o and "1000" in o and "uid=0" not in o, o.strip().replace("\n", " | "))
        rc, o = sh(f"echo '{pw}' | su -s /bin/sh zero -c 'sudo -S -k id -u' 2>/dev/null; echo wrongpw | su -s /bin/sh zero -c 'sudo -S -k id -u' 2>&1 | tail -n 1")
        T("sudo: zero becomes root with the password, not with a wrong one", o.strip().splitlines()[0] == "0" and o.strip().splitlines()[-1] != "0", o.strip().replace("\n", " | "))
        rc, o = sh(f"printf 'badold\\nnewpass99\\nnewpass99\\n' | nz-admin set-password 2>&1 | tail -n 1; printf '{pw}\\nnewpass99\\nnewpass99\\n' | nz-admin set-password 2>&1 | tail -n 1; echo newpass99 | su -s /bin/sh zero -c 'sudo -S -k id -u' 2>/dev/null")
        T("password change needs the current password", "Wrong password" in o and "Password set" in o and o.strip().splitlines()[-1] == "0", o.strip().replace("\n", " | "))
    elif first:
        monitor("sendkey ret"); time.sleep(6)
        rc, o = sh("grep '^zero:' /etc/shadow | cut -d: -f2; ls /data/etc/nz/firstboot-done; /etc/init.d/S50dropbear status; echo dropbear-status=$?; pidof dropbear; true")
        T("password skipped: SSH stays off, the prompt does not return", "!" in o and "firstboot-done" in o and "dropbear-status=1" in o, o.strip().replace("\n", " "))
    rc, o = sh("su -s /bin/sh zero -c 'sudo -n true' 2>&1 | tail -n1; su -s /bin/sh zero -c 'sudo -n /usr/sbin/nz-admin status' | tr '\\n' ' '")
    T("sudo without a password is limited to the settings helper", "password" in o.splitlines()[0] and "launcher=" in o, o.strip()[:300])
    rc, o = sh("for a in 'set brightness 999' 'set wifi-country us' 'set timezone ../../etc/passwd' 'set launcher x' 'store install a;b' 'store install ../x' 'ssh maybe' 'frobnicate'; do nz-admin $a >/dev/null 2>&1 && echo \"ACCEPTED: $a\"; done; echo checked")
    T("settings helper rejects out-of-range and malformed arguments", "ACCEPTED" not in o and "checked" in o, o.strip())
    rc, o = sh("nz-admin set timezone Europe/Berlin && date +%Z && nz-admin set timezone Etc/UTC && date +%Z && nz-admin set wifi-country DE && cat /data/etc/nz/wifi-country && nz-admin set bootlog off && nz-admin status | grep bootlog && nz-admin set bootlog on && nz-admin set screen-off 300 && cat /data/etc/nz/screen-off && nz-admin set screen-off 120")
    T("settings: time zone, Wi-Fi country, boot log switch, screen-off time are stored and applied", rc == 0 and "CE" in o and "UTC" in o and "DE" in o and "bootlog=off" in o, o.strip().replace("\n", " "))
    rc, o = sh("busybox ntpd --help 2>&1 | head -n 2 | tail -n 1; ls /usr/share/udhcpc/default.script.d/nz-time && nz-admin ntp status && { NZ_NTP_SERVER=127.0.0.1 nz-admin ntp sync 2>&1 | tail -n1; true; }", 120)
    T("network time: client present, DHCP hook installed, switch on (no server reachable in the emulator)", rc == 0 and "on" in o, o.strip().replace("\n", " | ")[:300])
    rc, o = sh("grep -c ttyGS0 /etc/inittab; lsmod | grep -c g_serial; cat /proc/cmdline | grep -c nz.diag; true")
    T("no USB serial console, no diagnostic at start", o.split() == ["0", "0", "0"], o.strip().replace("\n", " "))
    rc, o = sh("( command -v nz-launcher >/dev/null || { echo no-launcher; ps ax | grep -c '[g]etty.*tty1'; exit 0; }; n=0; while ! pidof nz-launcher >/dev/null && [ $n -lt 20 ]; do sleep 1; n=$((n+1)); done; p=$(pidof nz-launcher); awk '/^Uid:/{print \"uid=\"$2}' /proc/$p/status; tr '\\0' '\\n' < /proc/$p/environ | grep -E '^(HOME|XDG_RUNTIME_DIR)=' | tr '\\n' ' ' )")
    T("after the prompt: launcher runs as user zero (or a login prompt where there is no launcher)", ("uid=1000" in o and "HOME=/home/zero" in o) or "no-launcher" in o, o.strip().replace("\n", " "))
    time.sleep(2); shot("launcher-home")
    rc, o = sh("cat /proc/bus/input/devices | grep -c Handlers"); T("evdev input device present (USB keyboard)", rc == 0 and o.strip() not in ("", "0"), o.strip())
    rc, o = sh("nz-perf -i 5", 180); 
    try: perf = json.loads(o[o.index("{"):o.rindex("}") + 1]); open(os.path.join(out, "nz-perf.json"), "w").write(json.dumps(perf, indent=1)); T("nz-perf produces valid JSON", True, f"mem_used_kb={perf.get('mem_used_kb')} processes_user={perf.get('processes_user')} idle={perf.get('cpu_idle_pct')}%")
    except Exception as e: T("nz-perf produces valid JSON", False, repr(e) + o[-200:])
    rc, o = sh("ps ax | grep -v ']$' | grep -v 'ps$'"); open(os.path.join(out, "ps.txt"), "w").write(o)
    if extra:
        for line in [l for l in extra.splitlines() if l.strip() and not l.startswith("#")]:
            if line.startswith("@shot "): shot(line.split()[1]); continue
            if line.startswith("@key "):
                for k in line.split()[1:]: monitor(f"sendkey {k}"); time.sleep(1.2)
                continue
            if line.startswith("@sleep "): time.sleep(float(line.split()[1])); continue
            name, _, c = line.partition("::")
            rc, o = sh(c.strip(), 900); T(name.strip(), rc == 0, o.strip()[-400:])
            open(os.path.join(out, "script.log"), "a").write(f"### {name}\n$ {c}\n{o}\n[rc={rc}]\n")
        shot("after-script")
    if os.environ.get("NZ_HARD_OFF"):
        # what the POWER switch does: emergency remount read-only (sysrq-u), then the supply is cut
        sh("echo 1 > /proc/sys/kernel/sysrq; echo u > /proc/sysrq-trigger; sleep 1.5; grep -E ' (/|/data) ext4' /proc/mounts", 30)
    else:
        send("poweroff\n"); expect(rb"(reboot: Power down|System halted|Requesting system poweroff)", 90)
time.sleep(2); q.kill(); log.close()
if os.environ.get("NZ_SAVE_DISK"): shutil.copy(work, os.environ["NZ_SAVE_DISK"])   # card contents for a second boot
res["passed"] = sum(t["ok"] for t in res["tests"]); res["total"] = len(res["tests"])
json.dump(res, open(os.path.join(out, "qemu-results.json"), "w"), indent=1)
shutil.rmtree(tmp, ignore_errors=True)
print(f"\n{res['passed']}/{res['total']} passed; logs in {out}")
sys.exit(0 if res["passed"] == res["total"] and res["total"] else 1)
