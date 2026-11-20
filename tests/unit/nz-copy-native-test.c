#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8; typedef uint16_t u16;
#define swab16(x) ((u16)(((x) << 8) | ((x) >> 8)))
struct drm_rect { int x1, y1, x2, y2; };
struct iosys_map { void *vaddr; };
struct drm_framebuffer { unsigned width, height, pitches[4], offsets[4]; };
struct panel_mipi_dbi_device { void *nz_dst; };
static void nz_copy_native(struct panel_mipi_dbi_device *panel, const struct iosys_map *src,
               struct drm_framebuffer *fb, const struct drm_rect *rect, int variant,
               unsigned int *r0, unsigned int *r1, unsigned int *c0, unsigned int *c1)
{
    const u8 *base = src->vaddr + fb->offsets[0];
    unsigned int W = fb->width, H = fb->height, pitch = fb->pitches[0];
    bool flipx = variant == 2 || variant == 4, flipy = variant == 3 || variant == 4;
    u16 *dst = panel->nz_dst;
    unsigned int r, c;

    *r0 = flipx ? W - rect->x2 : rect->x1;
    *r1 = flipx ? W - 1 - rect->x1 : rect->x2 - 1;
    *c0 = flipy ? H - rect->y2 : rect->y1;
    *c1 = flipy ? H - 1 - rect->y1 : rect->y2 - 1;

    for (r = *r0; r <= *r1; r++) {
        unsigned int x = flipx ? W - 1 - r : r;

        for (c = *c0; c <= *c1; c++) {
            unsigned int y = flipy ? H - 1 - c : c;

            *dst++ = swab16(*(const u16 *)(base + y * pitch + x * 2));
        }
    }
}


#define W 320
#define H 170
static u16 fbmem[H][W + 8], panelmem[W][H], ref[W][H], buf[W * H];
int main(void)
{
	struct drm_framebuffer fb = { W, H, { (W + 8) * 2 }, { 0 } };
	struct iosys_map m = { fbmem }; struct panel_mipi_dbi_device p = { buf };
	srand(1);
	for (int v = 1; v <= 4; v++) {
		unsigned r0, r1, c0, c1; long checks = 0;
		memset(panelmem, 0, sizeof(panelmem));
		for (int it = 0; it < 3000; it++) {
			struct drm_rect r; r.x1 = rand() % W; r.x2 = r.x1 + 1 + rand() % (W - r.x1); r.y1 = rand() % H; r.y2 = r.y1 + 1 + rand() % (H - r.y1);
			if (it % 500 == 0) { r.x1 = 0; r.y1 = 0; r.x2 = W; r.y2 = H; }
			/* the application changes pixels inside the rectangle only */
			for (int y = r.y1; y < r.y2; y++) for (int x = r.x1; x < r.x2; x++) fbmem[y][x] = rand();
			nz_copy_native(&p, &m, &fb, &r, v, &r0, &r1, &c0, &c1);
			if (r1 >= W || c1 >= H || r0 > r1 || c0 > c1) { printf("variant %d: window out of range\n", v); return 1; }
			if ((r1 - r0 + 1) * (c1 - c0 + 1) != (unsigned)((r.x2 - r.x1) * (r.y2 - r.y1))) { printf("variant %d: size mismatch\n", v); return 1; }
			/* the panel stores what it is sent, line by line inside the window */
			u16 *q = buf; for (unsigned a = r0; a <= r1; a++) for (unsigned b = c0; b <= c1; b++) panelmem[a][b] = *q++;
			/* reference: the whole framebuffer through the full-frame rule */
			bool fx = v == 2 || v == 4, fy = v == 3 || v == 4;
			for (int a = 0; a < W; a++) for (int b = 0; b < H; b++) ref[a][b] = swab16(fbmem[fy ? H - 1 - b : b][fx ? W - 1 - a : a]);
			if (it >= 0 && memcmp(panelmem, ref, sizeof(ref)) && it % 500 != 0 && it > 0) { printf("variant %d: partial update %d differs from a full send\n", v, it); return 1; }
			checks++;
		}
		printf("variant %d: %ld partial updates, panel content always equal to a full send\n", v, checks);
	}
	return 0;
}
