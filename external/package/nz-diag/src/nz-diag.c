// SPDX-License-Identifier: GPL-2.0-only
/*
 * nz-diag - Cardputer Zero diagnostic frontend.
 *
 * A small keyboard-driven status / hardware-probe UI that talks to the kernel
 * through standard interfaces only: fbdev (the DRM fbdev emulation of the SPI
 * LCD), evdev, sysfs and procfs.  It is not a launcher.
 *
 *   nz-diag              interactive UI on the LCD
 *   nz-diag --report     print the hardware probe report to stdout and exit
 *                        (exit status = number of expected devices missing)
 *   nz-diag --activate   switch the panel pipeline on (modeset + unblank) and exit
 *   nz-diag --keymap     load the Sym-layer table into the text console and exit
 *   nz-diag --bench      framebuffer fill benchmark (mmap and write() paths), exit
 *   NZ_FB=/dev/fbN       framebuffer to use; NZ_FB_WRITE=1  flush with write()
 *
 * Keys: 1-5 or Left/Right/Tab = page, S = save report to /boot/nz-logs,
 *       +/- = backlight, Esc or Q = quit to shell.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <linux/font.h>
#include <linux/keyboard.h>
#include <termios.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include "nz_keymap.h"

/* ---------- framebuffer ---------- */
static struct {
	int fd, w, h, bpp, stride;
	uint8_t *mem; size_t len;
	uint16_t *back;                 /* RGB565 back buffer, w*h */
	char name[32];
} fb = { .fd = -1 };

#define RGB(r, g, b) ((uint16_t)((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | ((b) >> 3)))
#define C_BG RGB(0, 0, 0)
#define C_FG RGB(230, 230, 230)
#define C_DIM RGB(120, 120, 120)
#define C_OK RGB(60, 220, 90)
#define C_BAD RGB(255, 70, 60)
#define C_WARN RGB(255, 190, 40)
#define C_HEAD RGB(40, 90, 200)

static double now_s(void)
{
	struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Switch the panel's display pipeline on.
 *
 * The SPI panel is a DRM "tiny" device; its fbdev node is an emulation whose CRTC
 * stays disabled until somebody commits a modeset.  fbcon does that when it binds,
 * but only for the framebuffer it is mapped to (fb0).  When the firmware
 * framebuffer is fb0, the panel (fb1) is never lit: writes succeed, nothing shows
 * (first hardware boot, 2026-11-18).  Three independent triggers, all harmless
 * when the pipeline is already on:
 *   1. FBIOPUT_VSCREENINFO with FB_ACTIVATE_FORCE -> fb_set_par -> modeset commit
 *   2. FBIOBLANK unblank                           -> DPMS on
 *   3. open+close of the DRM node                  -> drm_lastclose restores fbdev
 */
static void fb_activate(int fd)
{
	struct fb_var_screeninfo v;
	if (!ioctl(fd, FBIOGET_VSCREENINFO, &v)) {
		v.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
		if (ioctl(fd, FBIOPUT_VSCREENINFO, &v)) perror("nz-diag: FBIOPUT_VSCREENINFO");
	}
	if (ioctl(fd, FBIOBLANK, FB_BLANK_UNBLANK) && errno != EINVAL) perror("nz-diag: FBIOBLANK");
	int d = open("/dev/dri/cardputer-zero-internal", O_RDWR | O_CLOEXEC);
	if (d >= 0) close(d);
}

static int fb_open(void)
{
	static const char *cand[] = { "/dev/fb_lcd", "/dev/fb0", "/dev/fb1", NULL };
	const char *env = getenv("NZ_FB");
	struct fb_var_screeninfo v; struct fb_fix_screeninfo f;

	for (int i = env ? -1 : 0; i < 0 || cand[i]; i++) {
		const char *p = i < 0 ? env : cand[i];
		int fd = open(p, O_RDWR | O_CLOEXEC);
		if (fd < 0) continue;
		if (ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f) ||
		    (v.bits_per_pixel != 16 && v.bits_per_pixel != 32)) { close(fd); continue; }
		fb.fd = fd; fb.w = v.xres; fb.h = v.yres; fb.bpp = v.bits_per_pixel;
		fb.stride = f.line_length; fb.len = (size_t)f.line_length * v.yres;
		snprintf(fb.name, sizeof(fb.name), "%.15s", f.id);
		fb.mem = mmap(NULL, fb.len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (fb.mem == MAP_FAILED) { fb.mem = NULL; }
		fb.back = calloc((size_t)fb.w * fb.h, 2);
		fb_activate(fd);
		return 0;
	}
	return -1;
}

/* 0: mmap + memcpy (flushed by the kernel's deferred-I/O worker, 50 ms cadence on
 * DRM fbdev emulation); 1: write() of the whole frame (queues the flush at once). */
static int fb_use_write;

static void fb_flush(void)
{
	if (fb.fd < 0) return;
	if (fb_use_write && fb.bpp == 16) {
		if (fb.stride == fb.w * 2) { if (pwrite(fb.fd, fb.back, (size_t)fb.w * fb.h * 2, 0) < 0) {} }
		else for (int y = 0; y < fb.h; y++)
			if (pwrite(fb.fd, fb.back + (size_t)y * fb.w, (size_t)fb.w * 2, (off_t)y * fb.stride) < 0) break;
	} else if (fb.bpp == 16 && fb.mem) {
		for (int y = 0; y < fb.h; y++)
			memcpy(fb.mem + (size_t)y * fb.stride, fb.back + (size_t)y * fb.w, (size_t)fb.w * 2);
	} else if (fb.mem) {
		for (int y = 0; y < fb.h; y++) {
			uint32_t *d = (uint32_t *)(fb.mem + (size_t)y * fb.stride);
			uint16_t *s = fb.back + (size_t)y * fb.w;
			for (int x = 0; x < fb.w; x++) {
				uint16_t c = s[x];
				d[x] = 0xff000000u | ((c >> 8 & 0xf8) << 16) | ((c >> 3 & 0xfc) << 8) | ((c << 3) & 0xf8);
			}
		}
	} else if (fb.bpp == 16) {               /* no mmap: plain write() */
		lseek(fb.fd, 0, SEEK_SET);
		for (int y = 0; y < fb.h; y++)
			if (pwrite(fb.fd, fb.back + (size_t)y * fb.w, (size_t)fb.w * 2, (off_t)y * fb.stride) < 0) break;
	}
}

static void rect(int x, int y, int w, int h, uint16_t c)
{
	for (int j = y < 0 ? 0 : y; j < y + h && j < fb.h; j++)
		for (int i = x < 0 ? 0 : x; i < x + w && i < fb.w; i++)
			fb.back[j * fb.w + i] = c;
}

static void glyph(int x, int y, unsigned char ch, uint16_t c)
{
	const unsigned char *g = (const unsigned char *)font_6x8.data + ch * 8;
	for (int j = 0; j < 8; j++)
		for (int i = 0; i < 6; i++)
			if ((g[j] & (0x80 >> i)) && x + i >= 0 && x + i < fb.w && y + j >= 0 && y + j < fb.h)
				fb.back[(y + j) * fb.w + x + i] = c;
}

__attribute__((format(printf, 4, 5)))
static void text(int x, int y, uint16_t c, const char *fmt, ...)
{
	char buf[160]; va_list ap;
	va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
	for (char *p = buf; *p; p++, x += 6) glyph(x, y, (unsigned char)*p, c);
}

/* ---------- small sysfs helpers ---------- */
static int rd(const char *path, char *buf, size_t n)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC); ssize_t r;
	if (fd < 0) { buf[0] = 0; return -1; }
	r = read(fd, buf, n - 1); close(fd);
	if (r < 0) r = 0;
	buf[r] = 0;
	while (r > 0 && (buf[r - 1] == '\n' || buf[r - 1] == ' ')) buf[--r] = 0;
	return (int)r;
}
static long rdl(const char *path, long def)
{
	char b[64]; return rd(path, b, sizeof(b)) > 0 ? strtol(b, NULL, 10) : def;
}
static int exists(const char *p) { struct stat st; return stat(p, &st) == 0; }

/* find first entry in dir whose <entry>/<attr> contains needle; returns 1 if found */
static int sys_find(const char *dir, const char *attr, const char *needle, char *out, size_t n)
{
	DIR *d = opendir(dir); struct dirent *e; char p[300], b[200]; int found = 0;
	if (!d) return 0;
	while (!found && (e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		snprintf(p, sizeof(p), "%s/%s/%s", dir, e->d_name, attr);
		if (rd(p, b, sizeof(b)) >= 0 && strstr(b, needle)) {
			if (out) snprintf(out, n, "%s", e->d_name);
			found = 1;
		}
	}
	closedir(d);
	return found;
}

/* ---------- hardware probe ---------- */
struct probe { const char *name; int ok; char detail[64]; int optional; };
#define NPROBE 16
static struct probe probes[NPROBE];
static int nprobe;

static void padd(const char *name, int ok, int optional, const char *fmt, ...)
{
	struct probe *p = &probes[nprobe++]; va_list ap;
	p->name = name; p->ok = ok; p->optional = optional;
	va_start(ap, fmt); vsnprintf(p->detail, sizeof(p->detail), fmt, ap); va_end(ap);
}

static int i2c_bound(const char *addr, char *drv, size_t n)
{
	char p[128], l[256]; ssize_t r;
	snprintf(p, sizeof(p), "/sys/bus/i2c/devices/1-00%s/driver", addr);
	r = readlink(p, l, sizeof(l) - 1);
	if (r < 0) {
		snprintf(p, sizeof(p), "/sys/bus/i2c/devices/1-00%s", addr);
		snprintf(drv, n, "%s", exists(p) ? "no driver" : "absent");
		return 0;
	}
	l[r] = 0;
	snprintf(drv, n, "%s", strrchr(l, '/') ? strrchr(l, '/') + 1 : l);
	return 1;
}

static void run_probes(void)
{
	char b[128], d[64]; int ok;
	nprobe = 0;

	ok = sys_find("/sys/class/graphics", "name", "", b, sizeof(b)) && fb.fd >= 0;
	padd("LCD fb", fb.fd >= 0, 0, fb.fd >= 0 ? "%s %dx%d/%d" : "no framebuffer", fb.name, fb.w, fb.h, fb.bpp);
	ok = sys_find("/sys/class/backlight", "max_brightness", "", b, sizeof(b));
	padd("Backlight", ok, 0, ok ? "%s" : "none", b);
	ok = sys_find("/sys/class/input", "name", "tca8418", b, sizeof(b)) ||
	     sys_find("/sys/class/input", "name", "TCA8418", b, sizeof(b));
	padd("Keyboard", ok, 0, ok ? "tca8418 %s" : "tca8418 not found", b);
	ok = i2c_bound("4f", d, sizeof(d)); padd("IO expander", ok, 0, "0x4f %s", d);
	ok = sys_find("/sys/class/power_supply", "type", "Battery", b, sizeof(b));
	padd("Fuel gauge", ok, 0, ok ? "%s" : "no battery supply", b);
	ok = rd("/proc/asound/cards", b, sizeof(b)) > 0 && strstr(b, "ES8389");
	padd("Audio", ok, 0, ok ? "ES8389 card" : "no ES8389 card");
	ok = exists("/sys/class/rtc/rtc0"); rd("/sys/class/rtc/rtc0/name", b, sizeof(b));
	padd("RTC", ok, 0, ok ? "%.40s" : "none", b);
	ok = exists("/sys/class/net/wlan0"); padd("Wi-Fi", ok, 0, ok ? "wlan0" : "no wlan0");
	ok = exists("/sys/class/bluetooth/hci0"); padd("Bluetooth", ok, 0, ok ? "hci0" : "no hci");
	ok = exists("/sys/class/udc") && sys_find("/sys/class/udc", "state", "", b, sizeof(b));
	padd("USB OTG", ok || exists("/sys/bus/usb/devices/usb1"), 0, ok ? "udc %s" : exists("/sys/bus/usb/devices/usb1") ? "host" : "none", b);
	ok = sys_find("/sys/bus/iio/devices", "name", "bmi270", b, sizeof(b)); padd("IMU", ok, 1, ok ? "bmi270 %s" : "bmi270 n/a", b);
	ok = sys_find("/sys/bus/iio/devices", "name", "bmm150", b, sizeof(b)) || sys_find("/sys/bus/iio/devices", "name", "bmc150", b, sizeof(b));
	padd("Magnetometer", ok, 1, ok ? "%s" : "bmm150 n/a", b);
	ok = sys_find("/sys/class/video4linux", "name", "unicam", b, sizeof(b)) || sys_find("/sys/class/video4linux", "name", "imx", b, sizeof(b));
	padd("Camera", ok, 1, ok ? "%s" : "no sensor", b);
	ok = sys_find("/sys/class/rc", "uevent", "gpio_ir_recv", b, sizeof(b)) || sys_find("/sys/class/rc", "uevent", "gpio-ir", b, sizeof(b));
	padd("IR rx", ok, 1, ok ? "%s" : "n/a", b);
	ok = sys_find("/sys/class/rc", "uevent", "gpio-ir-tx", b, sizeof(b)) || sys_find("/sys/class/rc", "uevent", "gpio_ir_tx", b, sizeof(b));
	padd("IR tx", ok, 1, ok ? "%s" : "n/a", b);
	ok = exists("/sys/class/leds/grove_5v_out"); padd("Ext power sw", ok, 0, ok ? "leds grove/ext" : "n/a");
}

static int report(FILE *f)
{
	struct utsname u; struct sysinfo si; char b[256]; int missing = 0;
	uname(&u); sysinfo(&si); run_probes();
	rd("/sys/firmware/devicetree/base/model", b, sizeof(b));
	fprintf(f, "nz-diag hardware report\nmodel: %s\nkernel: %s %s\nuptime: %lds  mem: %lu/%lu MiB free\n",
		b, u.release, u.machine, si.uptime, si.freeram * si.mem_unit >> 20, si.totalram * si.mem_unit >> 20);
	if (rd("/etc/nz-release", b, sizeof(b)) > 0) fprintf(f, "%s\n", b);
	for (int i = 0; i < nprobe; i++) {
		struct probe *p = &probes[i];
		fprintf(f, "%-13s %-8s %s\n", p->name, p->ok ? "OK" : p->optional ? "absent" : "MISSING", p->detail);
		if (!p->ok && !p->optional) missing++;
	}
	fprintf(f, "result: %d required device(s) missing\n", missing);
	return missing;
}

/* ---------- input ---------- */
#define MAXIN 16
static struct pollfd in[MAXIN]; static int nin;
static void input_open(void)
{
	DIR *d = opendir("/dev/input"); struct dirent *e; char p[280];
	for (int i = 0; i < nin; i++) close(in[i].fd);
	nin = 0;
	if (!d) return;
	while ((e = readdir(d)) && nin < MAXIN) {
		if (strncmp(e->d_name, "event", 5)) continue;
		snprintf(p, sizeof(p), "/dev/input/%s", e->d_name);
		int fd = open(p, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd >= 0) { in[nin].fd = fd; in[nin].events = POLLIN; nin++; }
	}
	closedir(d);
}

/* ---------- pages ---------- */
static int page, quit;
static unsigned char seen[KEY_MAX / 8 + 1]; static int nseen, lastcode = -1, lastval; static long nevents;
static double t_start, t_first, last_latency_ms;
static double bench_fps, bench_mbs;

static void header(const char *title)
{
	rect(0, 0, fb.w, 10, C_HEAD);
	text(2, 1, C_FG, "NZ diag %d/5 %s", page + 1, title);
	text(fb.w - 6 * 12 - 2, 1, C_FG, "1-5 S Q:quit");
}

static void bar(int x, int y, int w, int h, int pct, uint16_t c)
{
	rect(x, y, w, h, C_DIM); rect(x + 1, y + 1, w - 2, h - 2, C_BG);
	rect(x + 1, y + 1, (w - 2) * (pct < 0 ? 0 : pct > 100 ? 100 : pct) / 100, h - 2, c);
}

static void page_status(void)
{
	struct sysinfo si; struct utsname u; char b[128], ps[64] = ""; int y = 13;
	struct ifaddrs *ifa, *i;
	header("status"); sysinfo(&si); uname(&u);
	text(2, y, C_FG, "%s  up %ldm%02lds  load %.2f", u.release, si.uptime / 60, si.uptime % 60, si.loads[0] / 65536.0); y += 10;
	long tot = si.totalram * si.mem_unit >> 10, avail = 0; FILE *f = fopen("/proc/meminfo", "r");
	if (f) { while (fgets(b, sizeof(b), f)) if (sscanf(b, "MemAvailable: %ld", &avail) == 1) break; fclose(f); }
	text(2, y, C_FG, "mem  %ld/%ld MiB used", (tot - avail) >> 10, tot >> 10);
	bar(190, y, 126, 8, tot ? (int)((tot - avail) * 100 / tot) : 0, C_OK); y += 10;
	long temp = rdl("/sys/class/thermal/thermal_zone0/temp", -1), freq = rdl("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", -1);
	rd("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", b, sizeof(b));
	text(2, y, C_FG, "cpu  %ld.%ldC  %ld MHz  %s", temp / 1000, temp % 1000 / 100, freq / 1000, b); y += 10;
	if (sys_find("/sys/class/power_supply", "type", "Battery", ps, sizeof(ps))) {
		char p[160]; long cap, uv, ua;
		snprintf(p, sizeof(p), "/sys/class/power_supply/%s/capacity", ps); cap = rdl(p, -1);
		snprintf(p, sizeof(p), "/sys/class/power_supply/%s/voltage_now", ps); uv = rdl(p, 0);
		snprintf(p, sizeof(p), "/sys/class/power_supply/%s/current_now", ps); ua = rdl(p, 0);
		snprintf(p, sizeof(p), "/sys/class/power_supply/%s/status", ps); rd(p, b, sizeof(b));
		text(2, y, C_FG, "batt %ld%% %ld.%02ldV %ldmA %s", cap, uv / 1000000, uv % 1000000 / 10000, ua / 1000, b);
		bar(230, y, 86, 8, (int)cap, cap < 20 ? C_BAD : C_OK);
	} else text(2, y, C_WARN, "batt (no fuel gauge)");
	y += 10;
	if (!getifaddrs(&ifa)) {
		for (i = ifa; i && y < fb.h - 28; i = i->ifa_next) {
			if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK)) continue;
			inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, b, sizeof(b));
			text(2, y, C_OK, "net  %s %s", i->ifa_name, b); y += 10;
		}
		freeifaddrs(ifa);
	}
	if (y < 63) { text(2, y, C_DIM, "net  no IPv4 address (see /boot/wifi.conf.example)"); y += 10; }
	rd("/etc/nz-release", b, sizeof(b));
	for (char *l = strtok(b, "\n"); l && y < fb.h - 18; l = strtok(NULL, "\n")) { text(2, y, C_DIM, "%.52s", l); y += 9; }
	text(2, fb.h - 9, C_DIM, "first frame %.0f ms after exec, key->frame %.1f ms", (t_first - t_start) * 1e3, last_latency_ms);
}

static void page_hw(void)
{
	header("hardware"); run_probes();
	for (int i = 0; i < nprobe; i++) {
		struct probe *p = &probes[i]; int col = i / 8, row = i % 8;
		int x = 2 + col * 160, y = 13 + row * 19;
		rect(x, y + 1, 5, 6, p->ok ? C_OK : p->optional ? C_WARN : C_BAD);
		text(x + 8, y, C_FG, "%s", p->name);
		text(x + 8, y + 9, p->ok ? C_DIM : p->optional ? C_WARN : C_BAD, "%.24s", p->detail);
	}
}

static void page_keys(void)
{
	header("keyboard");
	text(2, 14, C_FG, "press every key; distinct keys seen: %d", nseen);
	text(2, 26, C_FG, "events: %ld   last: code %d (0x%x) %s", nevents, lastcode, lastcode < 0 ? 0 : lastcode,
	     lastval == 0 ? "up" : lastval == 1 ? "down" : "repeat");
	for (int c = 0, n = 0; c < KEY_MAX; c++)
		if (seen[c / 8] & (1 << (c % 8))) { text(2 + (n % 13) * 24, 42 + (n / 13) * 9, C_OK, "%3d", c); n++; }
	text(2, fb.h - 9, C_DIM, "Fn/Sym layers and LEDs are handled by the driver");
}

static void bench(void)
{
	double t0 = now_s(), t; long frames = 0;
	while ((t = now_s()) - t0 < 1.5) {
		rect(0, 0, fb.w, fb.h, RGB((frames * 7) & 255, (frames * 13) & 255, (frames * 29) & 255));
		fb_flush(); frames++;
	}
	bench_fps = frames / (t - t0);
	bench_mbs = bench_fps * fb.w * fb.h * (fb.bpp / 8) / 1e6;
}

/* Real panel update rate.  Writing to the fbdev node only copies memory; the SPI
 * transfer happens later in a kernel worker.  A DRM DIRTYFB call on our own buffer is a
 * blocking commit, so timing it measures what the panel actually receives.
 * Sets panel_fps (0 = not measured) and panel_err. */
static double panel_fps; static char panel_err[40];
static void bench_panel(void)
{
	struct drm_mode_card_res res = {0}; struct drm_mode_get_connector conn = {0};
	struct drm_mode_create_dumb cd = {0}; struct drm_mode_fb_cmd fbc = {0};
	struct drm_mode_map_dumb md = {0}; struct drm_mode_crtc crtc = {0};
	struct drm_mode_modeinfo modes[8]; uint32_t crtcs[4], conns[4], encs[4];
	uint16_t *px = MAP_FAILED; size_t len = 0;
	int fd = open("/dev/dri/cardputer-zero-internal", O_RDWR | O_CLOEXEC);
	panel_fps = 0; panel_err[0] = 0;
#define PFAIL(m) do { snprintf(panel_err, sizeof(panel_err), "%s: %s", m, strerror(errno)); goto out; } while (0)
	if (fd < 0) PFAIL("open drm");
	res.crtc_id_ptr = (uintptr_t)crtcs; res.count_crtcs = 4;
	res.connector_id_ptr = (uintptr_t)conns; res.count_connectors = 4;
	res.encoder_id_ptr = (uintptr_t)encs; res.count_encoders = 4;
	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) || !res.count_crtcs || !res.count_connectors) PFAIL("resources");
	conn.connector_id = conns[0]; conn.modes_ptr = (uintptr_t)modes; conn.count_modes = 8;
	if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) || !conn.count_modes) PFAIL("connector");
	cd.width = modes[0].hdisplay; cd.height = modes[0].vdisplay; cd.bpp = 16;
	if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd)) PFAIL("dumb");
	fbc.width = cd.width; fbc.height = cd.height; fbc.pitch = cd.pitch; fbc.bpp = 16; fbc.depth = 16; fbc.handle = cd.handle;
	if (ioctl(fd, DRM_IOCTL_MODE_ADDFB, &fbc)) PFAIL("addfb");
	md.handle = cd.handle;
	if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &md)) PFAIL("map");
	len = cd.size; px = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, md.offset);
	if (px == MAP_FAILED) PFAIL("mmap");
	crtc.crtc_id = crtcs[0]; crtc.fb_id = fbc.fb_id; crtc.set_connectors_ptr = (uintptr_t)conns;
	crtc.count_connectors = 1; crtc.mode = modes[0]; crtc.mode_valid = 1;
	if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc)) PFAIL("setcrtc");
	{
		double t0 = now_s(), t; long frames = 0;
		while ((t = now_s()) - t0 < 2.0) {
			struct drm_mode_fb_dirty_cmd d = { .fb_id = fbc.fb_id };
			uint16_t c = RGB((frames * 7) & 255, (frames * 13) & 255, (frames * 29) & 255);
			for (size_t i = 0; i < len / 2; i++) px[i] = c;
			if (ioctl(fd, DRM_IOCTL_MODE_DIRTYFB, &d)) PFAIL("dirtyfb");
			frames++;
		}
		panel_fps = frames / (t - t0);
	}
out:
	if (px != MAP_FAILED) munmap(px, len);
	if (fd >= 0) close(fd);        /* last close: the kernel restores the fbdev picture */
#undef PFAIL
}

static void page_display(void)
{
	static const uint16_t cols[] = { RGB(255,255,255), RGB(255,255,0), RGB(0,255,255), RGB(0,255,0), RGB(255,0,255), RGB(255,0,0), RGB(0,0,255), RGB(0,0,0) };
	header("display");
	for (int i = 0; i < 8; i++) rect(i * fb.w / 8, 12, fb.w / 8 + 1, 50, cols[i]);
	for (int x = 0; x < fb.w; x++) {
		int v = x * 255 / (fb.w - 1);
		rect(x, 64, 1, 12, RGB(v, 0, 0)); rect(x, 76, 1, 12, RGB(0, v, 0)); rect(x, 88, 1, 12, RGB(0, 0, v)); rect(x, 100, 1, 12, RGB(v, v, v));
	}
	rect(0, 114, fb.w, 1, C_FG); rect(0, fb.h - 1, fb.w, 1, C_FG); rect(0, 114, 1, fb.h - 114, C_FG); rect(fb.w - 1, 114, 1, fb.h - 114, C_FG);
	text(4, 118, C_FG, "%s %dx%d %dbpp stride %d", fb.name, fb.w, fb.h, fb.bpp, fb.stride);
	text(4, 128, C_FG, "B = benchmark   +/- = backlight");
	if (panel_fps > 0) text(4, 140, C_OK, "panel: %.1f full frames/s over SPI", panel_fps);
	else if (panel_err[0]) text(4, 140, C_WARN, "panel: not measured (%s)", panel_err);
	if (bench_fps > 0) text(4, 152, C_DIM, "memory copy only: %.0f frames/s", bench_fps);
}

static void page_log(void)
{
	static char buf[16384]; int n, lines = (fb.h - 12) / 8;
	header("kernel log");
	n = klogctl(3 /* SYSLOG_ACTION_READ_ALL */, buf, sizeof(buf) - 1);
	if (n <= 0) { text(2, 14, C_WARN, "klogctl: %s", strerror(errno)); return; }
	buf[n] = 0;
	char *p = buf + n;
	for (int l = 0; l <= lines && p > buf; l++) { p--; while (p > buf && p[-1] != '\n') p--; }
	for (int y = 12; *p && y < fb.h - 7; y += 8) {
		char *e = strchrnul(p, '\n'), *s = p;
		if (*s == '<') { char *q = strchr(s, '>'); if (q && q < e) s = q + 1; }
		text(2, y, strstr(s, "rror") || strstr(s, "fail") ? C_BAD : C_FG, "%.*s", (int)(e - s > 53 ? 53 : e - s), s);
		p = *e ? e + 1 : e;
	}
}

static void backlight(int delta)
{
	char dir[64], p[160], b[32]; long max, cur;
	if (!sys_find("/sys/class/backlight", "max_brightness", "", dir, sizeof(dir))) return;
	snprintf(p, sizeof(p), "/sys/class/backlight/%s/max_brightness", dir); max = rdl(p, 0);
	snprintf(p, sizeof(p), "/sys/class/backlight/%s/brightness", dir); cur = rdl(p, 0);
	cur += delta * (max / 10 ? max / 10 : 1);
	if (cur < 1) cur = 1;
	if (cur > max) cur = max;
	int fd = open(p, O_WRONLY | O_CLOEXEC);
	if (fd >= 0) { int n = snprintf(b, sizeof(b), "%ld\n", cur); if (write(fd, b, n) < 0) {} close(fd); }
}

static void save_report(void)
{
	FILE *f; mkdir("/boot/nz-logs", 0755);
	f = fopen("/boot/nz-logs/diag-report.txt", "w");
	if (!f) f = fopen("/tmp/diag-report.txt", "w");
	if (f) { report(f); fclose(f); sync(); }
}

static void draw(void)
{
	rect(0, 0, fb.w, fb.h, C_BG);
	switch (page) {
	case 0: page_status(); break;
	case 1: page_hw(); break;
	case 2: page_keys(); break;
	case 3: page_display(); break;
	default: page_log(); break;
	}
	fb_flush();
}

static void key(int code)
{
	switch (code) {
	case KEY_ESC: case KEY_Q: quit = 1; break;
	case KEY_1: case KEY_2: case KEY_3: case KEY_4: case KEY_5: page = code - KEY_1; break;
	case KEY_RIGHT: case KEY_TAB: case KEY_SPACE: page = (page + 1) % 5; break;
	case KEY_LEFT: page = (page + 4) % 5; break;
	case KEY_S: save_report(); break;
	case KEY_B: if (page == 3) { bench(); bench_panel(); fb_activate(fb.fd); } break;
	case KEY_EQUAL: case KEY_KPPLUS: case KEY_BRIGHTNESSUP: backlight(1); break;
	case KEY_MINUS: case KEY_KPMINUS: case KEY_BRIGHTNESSDOWN: backlight(-1); break;
	}
}

/* Text console: give the Sym-layer key codes their symbols (see nz_keymap.h).
 * Plain and Shift produce the symbol; Ctrl produces the control character where one
 * exists (Ctrl+[ = Esc, Ctrl+\ ...); Alt sends Esc-prefix (meta) like the default map. */
static int load_keymap(void)
{
	int fd = open("/dev/tty0", O_RDWR | O_CLOEXEC), n = 0, err = 0;
	if (fd < 0) fd = open("/dev/console", O_RDWR | O_CLOEXEC);
	if (fd < 0) { perror("nz-diag: console"); return 1; }
	for (unsigned i = 0; i < NZ_SYM_KEYS; i++) {
		unsigned char c = nz_sym_keys[i].ch;
		for (int tab = 0; tab < 16; tab++) {
			struct kbentry e = { .kb_table = tab, .kb_index = nz_sym_keys[i].code };
			if (tab & 2) continue;                              /* AltGr tables: leave alone */
			if (ioctl(fd, KDGKBENT, &e) || e.kb_value == K_NOSUCHMAP) continue;
			if (tab & 8) e.kb_value = K(KT_META, c);
			else if ((tab & 4) && c >= '[' && c <= '_') e.kb_value = K(KT_LATIN, c & 0x1f);
			else if ((tab & 4) && c == '@') e.kb_value = K(KT_LATIN, 0);
			else e.kb_value = K(KT_LATIN, c);
			unsigned short want = e.kb_value;
			if (ioctl(fd, KDSKBENT, &e) || ioctl(fd, KDGKBENT, &e) || e.kb_value != want) err++; else n++;
		}
	}
	close(fd);
	printf("sym keymap: %u keys, %d entries set, %d errors\n", (unsigned)NZ_SYM_KEYS, n, err);
	return err ? 1 : 0;
}

int main(int argc, char **argv)
{
	t_start = now_s();
	if (argc > 1 && !strcmp(argv[1], "--report")) { fb_open(); return report(stdout); }
	if (argc > 1 && !strcmp(argv[1], "--keymap")) return load_keymap();
	if (argc > 1 && !strcmp(argv[1], "--activate")) {       /* boot-time: light the panel, exit */
		if (fb_open()) return 100;
		printf("%s %dx%d/%d activated\n", fb.name, fb.w, fb.h, fb.bpp); return 0;
	}
	if (fb_open()) { fprintf(stderr, "nz-diag: no usable framebuffer (set NZ_FB)\n"); report(stderr); return 100; }
	fb_use_write = getenv("NZ_FB_WRITE") != NULL;
	if (argc > 1 && !strcmp(argv[1], "--bench")) {
		bench(); printf("fb=%s %dx%d bpp=%d mmap_fill_fps=%.1f", fb.name, fb.w, fb.h, fb.bpp, bench_fps);
		fb_use_write = 1; bench(); printf(" write_fill_fps=%.1f", bench_fps);
		bench_panel(); printf(" panel_fps=%.1f %s\n", panel_fps, panel_err); fb_activate(fb.fd); return 0;
	}
	int tty = open("/dev/tty", O_RDWR | O_CLOEXEC);
	if (tty >= 0) ioctl(tty, KDSETMODE, KD_GRAPHICS);     /* stop fbcon drawing over us */
	input_open();
	draw(); t_first = now_s();
	FILE *st = fopen("/run/nz-diag.stats", "w");
	if (st) { fprintf(st, "first_frame_ms=%.1f\n", (t_first - t_start) * 1e3); fclose(st); }
	draw();
	double next = now_s() + 1.0, rescan = now_s() + 3.0;
	while (!quit) {
		int to = (int)((next - now_s()) * 1000); if (to < 0) to = 0;
		int r = poll(in, nin, to), dirty = 0; double t_ev = now_s();
		if (r > 0) {
			struct input_event ev[16];
			for (int i = 0; i < nin; i++) {
				ssize_t n;
				if (!(in[i].revents & POLLIN)) continue;
				while ((n = read(in[i].fd, ev, sizeof(ev))) > 0)
					for (unsigned k = 0; k < n / sizeof(ev[0]); k++) {
						if (ev[k].type != EV_KEY) continue;
						nevents++; lastcode = ev[k].code; lastval = ev[k].value; dirty = 1;
						if (ev[k].code < KEY_MAX && !(seen[ev[k].code / 8] & (1 << (ev[k].code % 8)))) { seen[ev[k].code / 8] |= 1 << (ev[k].code % 8); nseen++; }
						if (ev[k].value == 1 && page != 2) key(ev[k].code);
						else if (ev[k].value == 1 && (ev[k].code == KEY_ESC)) key(ev[k].code);
						else if (ev[k].value == 1 && page == 2 && ev[k].code >= KEY_1 && ev[k].code <= KEY_5 && nseen > 40) key(ev[k].code);
					}
			}
		}
		if (dirty || now_s() >= next) {
			draw();
			if (dirty) last_latency_ms = (now_s() - t_ev) * 1e3;
			next = now_s() + 1.0;
		}
		if (now_s() >= rescan) { input_open(); rescan = now_s() + 3.0; }
	}
	rect(0, 0, fb.w, fb.h, C_BG); fb_flush();
	/* Keys pressed while the UI was up also went to the tty's line buffer; without this
	 * the next program on the console (login, a shell script) reads them as its input. */
	if (tty >= 0) { ioctl(tty, KDSETMODE, KD_TEXT); tcflush(tty, TCIFLUSH); close(tty); }
	return 0;
}
