/* Minimal stand-in for <linux/font.h> so lib/fonts/font_6x8.c from the kernel
 * tree being built can be compiled into a userspace program unchanged. */
#ifndef NZ_FONT_SHIM_H
#define NZ_FONT_SHIM_H
#define FONT6x8_IDX 0
struct font_desc {
	int idx; const char *name; unsigned int width, height, charcount;
	const void *data; int pref;
};
struct font_data { unsigned int extra[4]; const unsigned char data[]; };
extern const struct font_desc font_6x8;
#endif
