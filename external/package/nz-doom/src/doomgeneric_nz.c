/* SPDX-License-Identifier: GPL-2.0-only
 *
 * NimbleZero platform layer for doomgeneric (https://github.com/ozkl/doomgeneric).
 *
 * Picture: Doom draws 320x200; the Cardputer Zero LCD is 320x170.  Each frame is scaled
 *   to 170 lines (two source lines blended per output line), converted to RGB565 and
 *   handed to the panel through its DRM device with one DIRTYFB call per frame.  That
 *   call returns when the frame has been sent, so the time it takes is the time the
 *   display path needs.  If the DRM device cannot be used, /dev/fb0 is used instead.
 * Keys: read from all evdev keyboards (see keymap below).
 * Sound: none.
 *
 * Extra command line options (all others are Doom's own):
 *   -nzfps 0|1     frame counter in the top right corner (default 1; key 0 toggles)
 *   -nzdev PATH    DRM device (default /dev/dri/cardputer-zero-internal, then card0..3)
 *   -nzfb          use /dev/fb0 even if DRM works
 * On exit a summary line "nz-doom: ..." goes to stderr.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

/* Linux key codes under their own names: doomkeys.h reuses several KEY_ names (KEY_ENTER,
 * KEY_TAB, KEY_MINUS, ...) with Doom's values. */
enum { L_F = KEY_F, L_UP = KEY_UP, L_X = KEY_X, L_DOWN = KEY_DOWN, L_Z = KEY_Z, L_LEFT = KEY_LEFT, L_C = KEY_C, L_RIGHT = KEY_RIGHT, L_ENTER = KEY_ENTER, L_KPENTER = KEY_KPENTER, L_L = KEY_L, L_LEFTCTRL = KEY_LEFTCTRL, L_RIGHTCTRL = KEY_RIGHTCTRL, L_SPACE = KEY_SPACE, L_K = KEY_K, L_COMMA = KEY_COMMA, L_DOT = KEY_DOT, L_LEFTSHIFT = KEY_LEFTSHIFT, L_RIGHTSHIFT = KEY_RIGHTSHIFT, L_LEFTALT = KEY_LEFTALT, L_RIGHTALT = KEY_RIGHTALT, L_ESC = KEY_ESC, L_GRAVE = KEY_GRAVE, L_BACKSPACE = KEY_BACKSPACE, L_TAB = KEY_TAB, L_MINUS = KEY_MINUS, L_EQUAL = KEY_EQUAL, L_0 = KEY_0, L_1 = KEY_1, L_9 = KEY_9, L_Q = KEY_Q, L_P = KEY_P, L_A = KEY_A, L_M = KEY_M, L_MAX = KEY_MAX };
#undef KEY_ENTER
#undef KEY_TAB
#undef KEY_BACKSPACE
#undef KEY_MINUS
#undef KEY_PAUSE
#undef KEY_HOME
#undef KEY_END
#undef KEY_F1
#undef KEY_F2
#undef KEY_F3
#undef KEY_F4
#undef KEY_F5
#undef KEY_F6
#undef KEY_F7
#undef KEY_F8
#undef KEY_F9
#undef KEY_F10
#undef KEY_F11
#undef KEY_F12
#undef KEY_CAPSLOCK
#undef KEY_NUMLOCK

#include "doomgeneric.h"
#include "doomkeys.h"

#define PW 320
#define PH 170

/* ---------- time ---------- */
static uint64_t now_us(void)
{
	struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000u + ts.tv_nsec / 1000;
}

/* ---------- output ---------- */
static int drm_fd = -1, fb_fd = -1, out_w = PW, out_h = PH, out_pitch = PW * 2, out_bpp = 16;
static uint32_t drm_fb_id;
static uint8_t *out_px = MAP_FAILED; static size_t out_len;
static uint16_t frame[PW * PH];
static uint16_t row_a[PH], row_w[PH];        /* source line and weight of the next line (0..256) */
static int show_fps = 1;
static uint64_t n_one, n_two, n_more;        /* frames by time since the previous one: <20 ms, 20-36 ms, longer */
static uint64_t t_first, t_last, n_frames, flush_sum, flush_max, sec_start; static unsigned sec_frames, fps_now;
static const char *out_name = "none";

static int drm_try(const char *path)
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
	if (modes[0].hdisplay < PW || modes[0].vdisplay < PH) goto bad;
	cd.width = modes[0].hdisplay; cd.height = modes[0].vdisplay; cd.bpp = 16;
	if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd)) goto bad;
	fbc.width = cd.width; fbc.height = cd.height; fbc.pitch = cd.pitch; fbc.bpp = 16; fbc.depth = 16; fbc.handle = cd.handle;
	if (ioctl(fd, DRM_IOCTL_MODE_ADDFB, &fbc)) goto bad;
	md.handle = cd.handle;
	if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &md)) goto bad;
	out_px = mmap(NULL, cd.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, md.offset);
	if (out_px == MAP_FAILED) goto bad;
	memset(out_px, 0, cd.size);
	crtc.crtc_id = crtcs[0]; crtc.fb_id = fbc.fb_id; crtc.set_connectors_ptr = (uintptr_t)conns;
	crtc.count_connectors = 1; crtc.mode = modes[0]; crtc.mode_valid = 1;
	if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc)) { munmap(out_px, cd.size); out_px = MAP_FAILED; goto bad; }
	drm_fd = fd; drm_fb_id = fbc.fb_id; out_len = cd.size;
	out_w = cd.width; out_h = cd.height; out_pitch = cd.pitch; out_bpp = 16;
	return 0;
bad:
	fprintf(stderr, "nz-doom: %s: not usable as DRM output (%s)\n", path, strerror(errno));
	close(fd);
	return -1;
}

static int fb_try(void)
{
	struct fb_var_screeninfo v; struct fb_fix_screeninfo f;
	int fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
	if (fd < 0) return -1;
	if (ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f) ||
	    (v.bits_per_pixel != 16 && v.bits_per_pixel != 32) || v.xres < PW || v.yres < PH) { close(fd); return -1; }
	out_len = (size_t)f.line_length * v.yres;
	out_px = mmap(NULL, out_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (out_px == MAP_FAILED) { close(fd); return -1; }
	fb_fd = fd; out_w = v.xres; out_h = v.yres; out_pitch = f.line_length; out_bpp = v.bits_per_pixel;
	return 0;
}

/* 3x5 digits for the frame counter */
static const uint8_t digit[10][5] = {
	{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
	{7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7} };

static void draw_number(unsigned v, int right, int top)
{
	char s[8]; int n = snprintf(s, sizeof(s), "%u", v > 999 ? 999 : v), x0 = right - n * 8 - 2;
	for (int y = top; y < top + 12; y++) for (int x = x0; x < right; x++) frame[y * PW + x] = 0;
	for (int i = 0; i < n; i++)
		for (int r = 0; r < 5; r++) for (int c = 0; c < 3; c++)
			if (digit[s[i] - '0'][r] & (4 >> c))
				for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++)
					frame[(top + 1 + r * 2 + dy) * PW + x0 + 2 + i * 8 + c * 2 + dx] = 0xFFE0;
}

static void summary(void)
{
	static int done; if (done || !n_frames) return; done = 1;
	double secs = (t_last - t_first) / 1e6;
	fprintf(stderr, "nz-doom: output %s, %llu frames in %.1f s = %.1f frames/s; sending a frame took %.2f ms on average, %.2f ms at most\n",
		out_name, (unsigned long long)n_frames, secs, secs > 0 ? (n_frames - 1) / secs : 0.0,
		flush_sum / 1000.0 / n_frames, flush_max / 1000.0);
	fprintf(stderr, "nz-doom: time between frames: under 20 ms %llu, 20-36 ms %llu, longer %llu\n",
		(unsigned long long)n_one, (unsigned long long)n_two, (unsigned long long)n_more);
}

static void on_signal(int s) { (void)s; summary(); _exit(0); }

/* ---------- keys ---------- */
#define MAXIN 16
static int in_fd[MAXIN], n_in;
static struct { uint8_t key, down; } q[64]; static unsigned q_r, q_w;

static void q_put(int key, int down) { if (key && q_w - q_r < 64) { q[q_w % 64].key = key; q[q_w % 64].down = down; q_w++; } }

static void keys_open(void)
{
	DIR *d = opendir("/dev/input"); struct dirent *e; char p[280];
	if (!d) return;
	while ((e = readdir(d)) && n_in < MAXIN) {
		unsigned long bits[(L_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {0};
		if (strncmp(e->d_name, "event", 5)) continue;
		snprintf(p, sizeof(p), "/dev/input/%s", e->d_name);
		int fd = open(p, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0) continue;
		/* keyboards only: must have the Enter key */
		if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0 ||
		    !(bits[L_ENTER / (8 * sizeof(long))] >> (L_ENTER % (8 * sizeof(long))) & 1)) { close(fd); continue; }
		in_fd[n_in++] = fd;
	}
	closedir(d);
}

/* Keymap.  The Cardputer has no arrow keys of its own: F/X/Z/C are the arrows, as in the
 * NimbleZero launcher.  Real arrow keys (USB keyboard) work too. */
static void key_event(int code, int down)
{
	static const char row1[] = "1234567890", qw[] = "qwertyuiop", as[] = "asdfghjkl", zx[] = "zxcvbnm";
	switch (code) {
	case L_F: case L_UP:    q_put(KEY_UPARROW, down); return;
	case L_X: case L_DOWN:  q_put(KEY_DOWNARROW, down); return;
	case L_Z: case L_LEFT:  q_put(KEY_LEFTARROW, down); return;
	case L_C: case L_RIGHT: q_put(KEY_RIGHTARROW, down); return;
	case L_ENTER: case L_KPENTER: q_put(KEY_ENTER, down); q_put(KEY_FIRE, down); return;   /* menu: select; game: fire */
	case L_L: case L_LEFTCTRL: case L_RIGHTCTRL: q_put(KEY_FIRE, down); return;
	case L_SPACE: case L_K: q_put(KEY_USE, down); return;
	case L_COMMA: q_put(KEY_STRAFE_L, down); return;
	case L_DOT:   q_put(KEY_STRAFE_R, down); return;
	case L_LEFTSHIFT: case L_RIGHTSHIFT: q_put(KEY_RSHIFT, down); return;
	case L_LEFTALT: case L_RIGHTALT: q_put(KEY_RALT, down); return;
	case L_ESC: case L_GRAVE: q_put(KEY_ESCAPE, down); return;
	case L_BACKSPACE: q_put(KEY_BACKSPACE, down); return;
	case L_TAB: q_put(KEY_TAB, down); return;
	case L_MINUS: q_put(KEY_MINUS, down); return;
	case L_EQUAL: q_put(KEY_EQUALS, down); return;
	case L_0: if (down) show_fps = !show_fps; return;
	}
	if (code >= L_1 && code <= L_9) q_put(row1[code - L_1], down);
	else if (code >= L_Q && code <= L_P) q_put(qw[code - L_Q], down);
	else if (code >= L_A && code <= L_L) q_put(as[code - L_A], down);
	else if (code >= L_Z && code <= L_M) q_put(zx[code - L_Z], down);
}

static void keys_poll(void)
{
	struct input_event ev[32];
	for (int i = 0; i < n_in; i++) {
		ssize_t n;
		while ((n = read(in_fd[i], ev, sizeof(ev))) > 0)
			for (unsigned k = 0; k < n / sizeof(ev[0]); k++)
				if (ev[k].type == EV_KEY && ev[k].value != 2) key_event(ev[k].code, ev[k].value);
	}
}

/* ---------- doomgeneric interface ---------- */
static const char *arg_value(const char *name)
{
	extern int myargc; extern char **myargv;
	for (int i = 1; i < myargc - 1; i++) if (!strcmp(myargv[i], name)) return myargv[i + 1];
	return NULL;
}
static int arg_present(const char *name)
{
	extern int myargc; extern char **myargv;
	for (int i = 1; i < myargc; i++) if (!strcmp(myargv[i], name)) return 1;
	return 0;
}

void DG_Init(void)
{
	const char *v;
	if ((v = arg_value("-nzfps"))) show_fps = atoi(v);
	for (int y = 0; y < PH; y++) {
		/* centre of output line y, in source lines, x256 */
		int pos = ((2 * y + 1) * DOOMGENERIC_RESY * 256) / (2 * PH) - 128;
		if (pos < 0) pos = 0;
		row_a[y] = pos >> 8; row_w[y] = pos & 255;
		if (row_a[y] >= DOOMGENERIC_RESY - 1) { row_a[y] = DOOMGENERIC_RESY - 1; row_w[y] = 0; }
	}
	if (!arg_present("-nzfb")) {
		if ((v = arg_value("-nzdev"))) { if (!drm_try(v)) out_name = "DRM"; }
		else if (!drm_try("/dev/dri/cardputer-zero-internal") || !drm_try("/dev/dri/card0") ||
			 !drm_try("/dev/dri/card1") || !drm_try("/dev/dri/card2")) out_name = "DRM";
	}
	if (drm_fd < 0) {
		if (fb_try()) { fprintf(stderr, "nz-doom: no usable display (DRM and /dev/fb0 both failed)\n"); exit(1); }
		out_name = "fbdev";
	}
	fprintf(stderr, "nz-doom: output %s %dx%d, %d bits\n", out_name, out_w, out_h, out_bpp);
	keys_open();
	fprintf(stderr, "nz-doom: %d keyboard(s)\n", n_in);
	signal(SIGTERM, on_signal); signal(SIGINT, on_signal); signal(SIGHUP, on_signal);
	atexit(summary);
}

void DG_DrawFrame(void)
{
	const uint32_t *src = (const uint32_t *)DG_ScreenBuffer;
	uint64_t t0, t1;
	int ox = (out_w - PW) / 2, oy = (out_h - PH) / 2;

	for (int y = 0; y < PH; y++) {
		const uint32_t *a = src + row_a[y] * DOOMGENERIC_RESX, *b = a + (row_w[y] ? DOOMGENERIC_RESX : 0);
		uint32_t w = row_w[y], iw = 256 - w; uint16_t *o = frame + y * PW;
		for (int x = 0; x < PW; x++) {
			uint32_t p = a[x], n = b[x];
			uint32_t rb = (((p & 0xFF00FF) * iw + (n & 0xFF00FF) * w) >> 8) & 0xFF00FF;
			uint32_t g  = (((p & 0x00FF00) * iw + (n & 0x00FF00) * w) >> 8) & 0x00FF00;
			o[x] = (uint16_t)(((rb >> 8) & 0xF800) | ((g >> 5) & 0x07E0) | ((rb >> 3) & 0x001F));
		}
	}
	t0 = now_us();
	if (!sec_start) sec_start = t0;
	if (t0 - sec_start >= 1000000) { fps_now = (unsigned)((sec_frames * 1000000ull + (t0 - sec_start) / 2) / (t0 - sec_start)); sec_frames = 0; sec_start = t0; }
	sec_frames++;
	if (show_fps) draw_number(fps_now, PW - 2, 2);

	if (out_bpp == 16) {
		for (int y = 0; y < PH; y++) memcpy(out_px + (size_t)(y + oy) * out_pitch + ox * 2, frame + y * PW, PW * 2);
	} else {
		for (int y = 0; y < PH; y++) {
			uint32_t *o = (uint32_t *)(out_px + (size_t)(y + oy) * out_pitch) + ox;
			for (int x = 0; x < PW; x++) {
				uint16_t c = frame[y * PW + x];
				o[x] = ((c & 0xF800) << 8) | ((c & 0x07E0) << 5) | ((c & 0x001F) << 3);
			}
		}
	}
	if (drm_fd >= 0) {
		struct drm_mode_fb_dirty_cmd d = { .fb_id = drm_fb_id };
		ioctl(drm_fd, DRM_IOCTL_MODE_DIRTYFB, &d);
	}
	t1 = now_us();
	if (!n_frames) t_first = t1;
	else if (t1 - t_last < 20000) n_one++; else if (t1 - t_last < 36000) n_two++; else n_more++;
	t_last = t1; n_frames++;
	flush_sum += t1 - t0; if (t1 - t0 > flush_max) flush_max = t1 - t0;
	keys_poll();
}

void DG_SleepMs(uint32_t ms) { usleep(ms * 1000); }
uint32_t DG_GetTicksMs(void) { return (uint32_t)(now_us() / 1000); }

int DG_GetKey(int *pressed, unsigned char *key)
{
	if (q_r == q_w) keys_poll();
	if (q_r == q_w) return 0;
	*pressed = q[q_r % 64].down; *key = q[q_r % 64].key; q_r++;
	return 1;
}

void DG_SetWindowTitle(const char *title) { (void)title; }

int main(int argc, char **argv)
{
	doomgeneric_Create(argc, argv);
	for (;;) doomgeneric_Tick();
	return 0;
}
