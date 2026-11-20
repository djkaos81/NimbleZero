// SPDX-License-Identifier: GPL-2.0-only
/*
 * nz-pick - full-screen list picker for shell scripts (Options, Services, Store).
 *
 *   nz-pick [-s N] TITLE [HINT] < items        prints the number (1..n) of the chosen line
 *
 * Each input line is one row: "label<TAB>value<TAB>detail".  The value is drawn at the
 * right edge, the detail (if any) on the bottom line while the row is selected.
 * A line starting with "-" is a heading and cannot be chosen.
 * Exit status: 0 chosen, 1 back/cancel, 2 error.  -s N preselects row N.
 *
 * Keys: F / X (or Up/Down, K/J)  move      Z / C (or Left/Right, PgUp/PgDn)  page
 *       Enter  choose                      Esc, Backspace or Q               back
 */
#define _GNU_SOURCE
#define NZUI_IMPLEMENTATION
#include "nzui.h"
#include <linux/input-event-codes.h>

#define MAXROWS 400
struct row { char label[44], value[20], detail[44]; int head; };
static struct row rows[MAXROWS]; static int n, sel, top;

static void field(char *dst, size_t sz, char **p)
{
	char *t = *p ? strchr(*p, '\t') : NULL;
	if (!*p) { dst[0] = 0; return; }
	if (t) *t = 0;
	snprintf(dst, sz, "%s", *p);
	*p = t ? t + 1 : NULL;
}

static int step(int from, int dir)
{
	for (int i = from + dir; i >= 0 && i < n; i += dir) if (!rows[i].head) return i;
	return from;
}

static void draw(const char *title, const char *hint)
{
	const int rh = 12, vis = (nzui_h - 14 - 12) / rh;
	uint16_t bar = NZUI_RGB(30, 70, 170), hi = NZUI_RGB(250, 200, 60);
	nzui_clear(NZUI_BLACK);
	nzui_rect(0, 0, nzui_w, 12, bar);
	nzui_text(4, 2, NZUI_WHITE, "%.38s", title);
	if (sel < top) top = sel;
	if (sel >= top + vis) top = sel - vis + 1;
	if (top > 0 && rows[top - 1].head && sel - (top - 1) < vis) top--;      /* keep a heading visible */
	for (int i = 0; i < vis && top + i < n; i++) {
		struct row *r = &rows[top + i]; int y = 15 + i * rh, s = top + i == sel;
		if (r->head) { nzui_text(4, y + 1, NZUI_RGB(120, 170, 255), "%.38s", r->label); continue; }
		if (s) nzui_rect(0, y - 1, nzui_w, rh, NZUI_RGB(40, 40, 60));
		nzui_text(6, y + 1, s ? hi : NZUI_WHITE, "%c %.*s", s ? '>' : ' ', r->value[0] ? 36 - (int)strlen(r->value) - 1 : 36, r->label);
		if (r->value[0]) nzui_text(nzui_w - 6 - 8 * (int)strlen(r->value), y + 1, s ? hi : NZUI_GREY, "%s", r->value);
	}
	if (n > vis) {       /* scroll position */
		int h = (nzui_h - 26) * vis / n, y = 13 + (nzui_h - 26 - h) * top / (n - vis);
		nzui_rect(nzui_w - 2, y, 2, h < 4 ? 4 : h, NZUI_GREY);
	}
	nzui_rect(0, nzui_h - 11, nzui_w, 11, NZUI_RGB(25, 25, 25));
	nzui_text(4, nzui_h - 9, NZUI_GREY, "%.39s", n && rows[sel].detail[0] ? rows[sel].detail : hint);
	nzui_flush();
}

int main(int argc, char **argv)
{
	char line[256]; struct nzui_event ev; int start = 0, a = 1, r;
	const char *title, *hint;
	if (argc > 2 && !strcmp(argv[1], "-s")) { start = atoi(argv[2]) - 1; a = 3; }
	if (argc <= a) { fprintf(stderr, "usage: nz-pick [-s N] TITLE [HINT] < items\n"); return 2; }
	title = argv[a]; hint = argc > a + 1 ? argv[a + 1] : "Enter:choose  F/X:move  Esc:back";
	while (n < MAXROWS && fgets(line, sizeof(line), stdin)) {
		char *p = line; line[strcspn(line, "\r\n")] = 0;
		if (!line[0]) continue;
		rows[n].head = line[0] == '-';
		if (rows[n].head) p++;
		field(rows[n].label, sizeof(rows[n].label), &p);
		field(rows[n].value, sizeof(rows[n].value), &p);
		field(rows[n].detail, sizeof(rows[n].detail), &p);
		n++;
	}
	if (!n) return 1;
	sel = start >= 0 && start < n ? start : 0;
	if (rows[sel].head) sel = step(sel, 1);
	if (rows[sel].head) return 1;
	if (nzui_init("nz-pick")) { fprintf(stderr, "nz-pick: no display\n"); return 2; }
	draw(title, hint);
	for (;;) {
		if ((r = nzui_poll(&ev, 60000)) < 0) break;
		if (r != 1 || !ev.value) continue;
		switch (ev.key) {
		case KEY_UP: case KEY_F: case KEY_K: sel = step(sel, -1); break;
		case KEY_DOWN: case KEY_X: case KEY_J: sel = step(sel, 1); break;
		case KEY_RIGHT: case KEY_C: case KEY_PAGEDOWN: for (int i = 0; i < 8; i++) sel = step(sel, 1); break;
		case KEY_LEFT: case KEY_Z: case KEY_PAGEUP: for (int i = 0; i < 8; i++) sel = step(sel, -1); break;
		case KEY_ENTER: case KEY_KPENTER:
			if (ev.value != 1) break;
			nzui_quit(); printf("%d\n", sel + 1); return 0;
		case KEY_ESC: case KEY_BACKSPACE: case KEY_Q: case KEY_GRAVE:
			if (ev.value != 1) break;
			nzui_quit(); return 1;
		}
		draw(title, hint);
	}
	nzui_quit();
	return 1;
}
