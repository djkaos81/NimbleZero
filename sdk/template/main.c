// SPDX-License-Identifier: GPL-2.0-only
/* Application template: a counter you change with the arrow keys.
 * Build for the desktop simulator with "make sim", for the device with "make". */
#define NZUI_IMPLEMENTATION
#include "nzui.h"
#include <linux/input-event-codes.h>

int main(void)
{
	struct nzui_event ev; int r, n = 0;
	if (nzui_init("hello")) return 1;
	do {
		nzui_clear(NZUI_BLACK);
		nzui_rect(0, 0, nzui_w, 12, NZUI_RGB(30, 70, 170));
		nzui_text(4, 2, NZUI_WHITE, "Hello, Cardputer Zero");
		nzui_text(40, 70, NZUI_RGB(250, 220, 60), "counter: %d", n);
		nzui_text(4, nzui_h - 10, NZUI_GREY, "Up/Down change   Q quit");
		nzui_flush();
		r = nzui_poll(&ev, 1000);
		if (r == 1 && ev.value) {
			if (ev.key == KEY_UP) n++;
			if (ev.key == KEY_DOWN) n--;
			if (ev.key == KEY_Q || ev.key == KEY_ESC) break;
		}
	} while (r >= 0);
	nzui_quit();
	return 0;
}
