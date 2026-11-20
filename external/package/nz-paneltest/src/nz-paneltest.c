/* SPDX-License-Identifier: GPL-2.0-only
 * nz-paneltest - pictures for judging the Cardputer Zero LCD path by eye.
 *
 *   nz-paneltest card [SECONDS]   still picture: a large letter F, the word-free corner
 *                                 marks RED top-left, GREEN top-right, BLUE bottom-left,
 *                                 WHITE bottom-right, and a one-pixel frame at the edges
 *   nz-paneltest bars [SECONDS]   a white vertical bar and a white horizontal bar moving
 *                                 at a fixed step per frame, sent as fast as the display
 *                                 path accepts frames; a torn picture shows as a bar that
 *                                 is broken or stepped.  Prints frames per second.
 *   nz-paneltest bars SECONDS WORK_MS   the same, but the program keeps the CPU busy for
 *                                 WORK_MS before each frame, like a program that needs
 *                                 that long to draw.
 *   nz-paneltest fbbars [SECONDS]   the bars drawn into /dev/fb0 about 60 times per second,
 *                                 the way the launcher, the console and most apps draw.  How
 *                                 many of those pictures reach the panel is decided by the
 *                                 kernel; read it from the driver's counters.
 *   nz-paneltest fbblink [SECONDS]  a small block in the middle of /dev/fb0 switched on and
 *                                 off 30 times per second (a small update, like a cursor)
 * Output: the panel's DRM device (one DIRTYFB per frame, like nz-doom); /dev/fb0 if that
 * fails (then the frame rate is that of the console layer, not of the panel).
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

static int drm_fd = -1, W, H, pitch, bpp = 16;
static uint32_t fb_id; static uint8_t *px = MAP_FAILED;

static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

static int drm_open(const char *path)
{
	struct drm_mode_card_res res = {0}; struct drm_mode_get_connector conn = {0};
	struct drm_mode_create_dumb cd = {0}; struct drm_mode_fb_cmd fbc = {0};
	struct drm_mode_map_dumb md = {0}; struct drm_mode_crtc crtc = {0};
	struct drm_mode_modeinfo modes[8]; uint32_t crtcs[4], conns[4], encs[4];
	int fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0) return -1;
	res.crtc_id_ptr = (uintptr_t)crtcs; res.count_crtcs = 4;
	res.connector_id_ptr = (uintptr_t)conns; res.count_connectors = 4;
	res.encoder_id_ptr = (uintptr_t)encs; res.count_encoders = 4;
	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) || !res.count_crtcs || !res.count_connectors) goto bad;
	conn.connector_id = conns[0]; conn.modes_ptr = (uintptr_t)modes; conn.count_modes = 8;
	if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) || !conn.count_modes) goto bad;
	cd.width = modes[0].hdisplay; cd.height = modes[0].vdisplay; cd.bpp = 16;
	if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd)) goto bad;
	fbc.width = cd.width; fbc.height = cd.height; fbc.pitch = cd.pitch; fbc.bpp = 16; fbc.depth = 16; fbc.handle = cd.handle;
	if (ioctl(fd, DRM_IOCTL_MODE_ADDFB, &fbc)) goto bad;
	md.handle = cd.handle;
	if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &md)) goto bad;
	px = mmap(NULL, cd.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, md.offset);
	if (px == MAP_FAILED) goto bad;
	crtc.crtc_id = crtcs[0]; crtc.fb_id = fbc.fb_id; crtc.set_connectors_ptr = (uintptr_t)conns;
	crtc.count_connectors = 1; crtc.mode = modes[0]; crtc.mode_valid = 1;
	if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc)) goto bad;
	drm_fd = fd; fb_id = fbc.fb_id; W = cd.width; H = cd.height; pitch = cd.pitch;
	return 0;
bad:
	fprintf(stderr, "nz-paneltest: %s: %s\n", path, strerror(errno)); close(fd); return -1;
}

static int fb_open(void)
{
	struct fb_var_screeninfo v; struct fb_fix_screeninfo f; int fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
	if (fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f)) return -1;
	if (v.bits_per_pixel != 16 && v.bits_per_pixel != 32) return -1;
	px = mmap(NULL, (size_t)f.line_length * v.yres, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (px == MAP_FAILED) return -1;
	W = v.xres; H = v.yres; pitch = f.line_length; bpp = v.bits_per_pixel;
	return 0;
}

static void put(int x, int y, uint16_t c)
{
	if (x < 0 || y < 0 || x >= W || y >= H) return;
	if (bpp == 16) ((uint16_t *)(px + (size_t)y * pitch))[x] = c;
	else ((uint32_t *)(px + (size_t)y * pitch))[x] = ((c & 0xF800) << 8) | ((c & 0x07E0) << 5) | ((c & 0x1F) << 3);
}
static void rect(int x, int y, int w, int h, uint16_t c) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) put(i, j, c); }
static void flush(void) { if (drm_fd >= 0) { struct drm_mode_fb_dirty_cmd d = { .fb_id = fb_id }; ioctl(drm_fd, DRM_IOCTL_MODE_DIRTYFB, &d); } }

static void card(void)
{
	rect(0, 0, W, H, 0x0000);
	rect(0, 0, W, 1, 0xFFFF); rect(0, H - 1, W, 1, 0xFFFF); rect(0, 0, 1, H, 0xFFFF); rect(W - 1, 0, 1, H, 0xFFFF);
	rect(4, 4, 40, 24, 0xF800);                 /* red: top left */
	rect(W - 44, 4, 40, 24, 0x07E0);            /* green: top right */
	rect(4, H - 28, 40, 24, 0x001F);            /* blue: bottom left */
	rect(W - 44, H - 28, 40, 24, 0xFFFF);       /* white: bottom right */
	/* letter F, 60 wide, 110 high, in the middle */
	int x = W / 2 - 30, y = H / 2 - 55;
	rect(x, y, 16, 110, 0xFFE0); rect(x, y, 60, 16, 0xFFE0); rect(x, y + 44, 44, 16, 0xFFE0);
}

int main(int argc, char **argv)
{
	const char *mode = argc > 1 ? argv[1] : ""; double secs = argc > 2 ? atof(argv[2]) : 10;
	double work = argc > 3 ? atof(argv[3]) / 1000.0 : 0; long slow = 0;
	int fbonly = !strncmp(mode, "fb", 2);
	if (strcmp(mode, "card") && strcmp(mode, "bars") && strcmp(mode, "fbbars") && strcmp(mode, "fbblink")) { fprintf(stderr, "usage: nz-paneltest card|bars|fbbars|fbblink [SECONDS] [WORK_MS]\n"); return 2; }
	if (fbonly ? fb_open() : (drm_open("/dev/dri/cardputer-zero-internal") && drm_open("/dev/dri/card0") && fb_open())) { fprintf(stderr, "nz-paneltest: no display\n"); return 1; }
	if (!strcmp(mode, "fbblink")) {
		double b0 = now_s(); long k = 0;
		printf("output: /dev/fb0 %dx%d\n", W, H);
		while (now_s() - b0 < secs) { rect(W / 2 - 4, H / 2 - 8, 8, 16, (k & 1) ? 0x0000 : 0xFFFF); k++; usleep(33000); }
		rect(W / 2 - 4, H / 2 - 8, 8, 16, 0x0000);
		printf("blink: %ld small changes in %.1f s\n", k, now_s() - b0);
		return 0;
	}
	printf("output: %s %dx%d\n", drm_fd >= 0 ? "DRM" : "/dev/fb0 (the rate below is how often this program drew, not what reached the panel)", W, H);
	if (!strcmp(mode, "card")) { card(); flush(); usleep((useconds_t)(secs * 1e6)); return 0; }
	double t0 = now_s(), t, worst = 0, prev = t0; long n = 0;
	while ((t = now_s()) - t0 < secs) {
		int bx = (int)((n * 4) % (W + 16)) - 16, by = (int)((n * 2) % (H + 8)) - 8;
		if (work > 0) { double w0 = now_s(); while (now_s() - w0 < work) ; }
		rect(0, 0, W, H, 0x0000);
		rect(bx, 0, 16, H, 0xFFFF); rect(0, by, W, 8, 0xFFFF);
		flush(); if (drm_fd < 0) usleep(16000);
		n++; double e = now_s(); if (n > 1 && e - prev > worst) worst = e - prev; if (n > 1 && e - prev > 0.020) slow++; prev = e;
	}
	printf("bars: %ld frames in %.1f s = %.1f frames/s, longest frame %.1f ms, %ld frames took longer than 20 ms (work per frame: %.0f ms)\n",
		n, t - t0, n / (t - t0), worst * 1000, slow, work * 1000);
	return 0;
}
