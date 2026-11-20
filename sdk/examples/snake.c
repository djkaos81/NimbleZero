// SPDX-License-Identifier: GPL-2.0-only
/* snake - example graphical application for the Cardputer Zero built on nzui.h.
 * Arrow keys (or WASD) steer, P pauses, Q/Esc quits.  Runs unchanged in the
 * desktop simulator (-DNZUI_SDL) and on the device (fbdev + evdev). */
#define NZUI_IMPLEMENTATION
#include "nzui.h"
#include <time.h>
#include <linux/input-event-codes.h>

#define CELL 10
#define GW (320 / CELL)
#define GH ((170 - 10) / CELL)
static struct { int x, y; } body[GW * GH];
static int len, dx, dy, fx, fy, score, paused, over;

static void food(void) { fx = rand() % GW; fy = rand() % GH; }
static void reset(void) { len = 4; dx = 1; dy = 0; score = 0; over = 0; for (int i = 0; i < len; i++) { body[i].x = GW / 2 - i; body[i].y = GH / 2; } food(); }

static void step(void)
{
	int nx = (body[0].x + dx + GW) % GW, ny = (body[0].y + dy + GH) % GH;
	for (int i = 0; i < len; i++) if (body[i].x == nx && body[i].y == ny) { over = 1; return; }
	if (nx == fx && ny == fy) { len++; score++; food(); }
	for (int i = len - 1; i > 0; i--) body[i] = body[i - 1];
	body[0].x = nx; body[0].y = ny;
}

static void draw(void)
{
	nzui_clear(NZUI_BLACK);
	nzui_rect(0, 0, nzui_w, 10, NZUI_RGB(30, 70, 170));
	nzui_text(2, 1, NZUI_WHITE, "snake  score %d%s", score, paused ? "  [paused]" : over ? "  GAME OVER - Enter" : "");
	nzui_rect(fx * CELL + 2, 10 + fy * CELL + 2, CELL - 4, CELL - 4, NZUI_RGB(240, 60, 60));
	for (int i = 0; i < len; i++)
		nzui_rect(body[i].x * CELL + 1, 10 + body[i].y * CELL + 1, CELL - 2, CELL - 2, i ? NZUI_RGB(60, 200, 90) : NZUI_RGB(250, 220, 60));
	nzui_flush();
}

int main(void)
{
	struct nzui_event ev; int r;
	if (nzui_init("snake")) { fprintf(stderr, "snake: no display\n"); return 1; }
	srand((unsigned)time(NULL)); reset(); draw();
	while ((r = nzui_poll(&ev, 120)) >= 0) {
		if (r == 1 && ev.value == 1) {
			switch (ev.key) {
			case KEY_UP: case KEY_W: if (!dy) { dx = 0; dy = -1; } break;
			case KEY_DOWN: case KEY_S: if (!dy) { dx = 0; dy = 1; } break;
			case KEY_LEFT: case KEY_A: if (!dx) { dx = -1; dy = 0; } break;
			case KEY_RIGHT: case KEY_D: if (!dx) { dx = 1; dy = 0; } break;
			case KEY_P: paused = !paused; break;
			case KEY_ENTER: if (over) reset(); break;
			case KEY_Q: case KEY_ESC: nzui_quit(); return 0;
			}
		}
		if (r == 0 && !paused && !over) step();
		draw();
	}
	nzui_quit();
	return 0;
}
