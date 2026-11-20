// SPDX-License-Identifier: GPL-2.0
/*
 * nz-growpart DISK PARTNO - grow the last MBR partition to the end of the card.
 *
 * The image ships a small data partition; the first boot enlarges it to whatever card
 * the image was written to.  Only the size field of one partition entry changes, and
 * only when that partition is the last one on the disk.  The running kernel is told
 * with BLKPG_RESIZE_PARTITION, so no reboot is needed; resize2fs then grows the
 * filesystem online.
 *
 *
 * nz-growpart --type DISK PARTNO        print the MBR type byte (two hex digits)
 * nz-growpart --settype DISK PARTNO HEX change it (one-sector write, nothing else)
 * The image ships the data partition with type DA ("needs formatting"); the first boot
 * formats it and only then sets 83.  A power cut before that simply repeats the job.
 *
 * exit status: 0 grown, 3 nothing to do, 1 refused/error
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/blkpg.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

int main(int argc, char **argv)
{
	uint8_t mbr[512]; uint64_t bytes;
	if (argc >= 4 && (!strcmp(argv[1], "--type") || !strcmp(argv[1], "--settype"))) {
		int set = argv[1][2] == 's', pn = atoi(argv[3]);
		int f = open(argv[2], set ? O_RDWR | O_CLOEXEC : O_RDONLY | O_CLOEXEC);
		if (pn < 1 || pn > 4 || (set && argc != 5) || f < 0 || pread(f, mbr, 512, 0) != 512 ||
		    mbr[510] != 0x55 || mbr[511] != 0xaa) { fprintf(stderr, "nz-growpart: cannot read partition table\n"); return 1; }
		uint8_t *t = mbr + 446 + 16 * (pn - 1) + 4;
		if (!set) { printf("%02x\n", *t); return 0; }
		if (!*t) { fprintf(stderr, "nz-growpart: partition %d is empty\n", pn); return 1; }
		*t = (uint8_t)strtoul(argv[4], NULL, 16);
		if (!*t || pwrite(f, mbr, 512, 0) != 512 || fsync(f)) { perror("nz-growpart: settype"); return 1; }
		return 0;
	}
	if (argc != 3) { fprintf(stderr, "usage: nz-growpart DISK PARTNO\n"); return 1; }
	int n = atoi(argv[2]), dry = getenv("NZ_GROWPART_DRYRUN") != NULL;
	if (n < 1 || n > 4) { fprintf(stderr, "nz-growpart: partition must be 1..4\n"); return 1; }
	int fd = open(argv[1], O_RDWR | O_CLOEXEC);
	if (fd < 0 || pread(fd, mbr, 512, 0) != 512 || ioctl(fd, BLKGETSIZE64, &bytes)) { perror("nz-growpart"); return 1; }
	if (mbr[510] != 0x55 || mbr[511] != 0xaa) { fprintf(stderr, "nz-growpart: no MBR signature\n"); return 1; }
	uint8_t *e = mbr + 446 + 16 * (n - 1);
	uint64_t start = le32(e + 8), size = le32(e + 12), disk = bytes / 512;
	if (!e[4] || !start || !size) { fprintf(stderr, "nz-growpart: partition %d is empty\n", n); return 1; }
	if (e[4] == 0xee) { fprintf(stderr, "nz-growpart: GPT disk, not touching it\n"); return 1; }
	for (int i = 0; i < 4; i++) {
		uint8_t *o = mbr + 446 + 16 * i;
		if (i == n - 1 || !o[4]) continue;
		if ((uint64_t)le32(o + 8) + le32(o + 12) > start) { fprintf(stderr, "nz-growpart: partition %d is not the last one\n", n); return 1; }
	}
	if (disk > 0xffffffffull) disk = 0xffffffffull;          /* MBR limit, 2 TiB */
	uint64_t end = disk & ~2047ull;                           /* whole MiB */
	if (end <= start + size + 16 * 2048) {                    /* less than 16 MiB to gain */
		printf("nz-growpart: %s%d already %llu MiB, nothing to do\n", argv[1], n, (unsigned long long)(size / 2048));
		return 3;
	}
	uint64_t nsize = end - start;
	printf("nz-growpart: partition %d: %llu MiB -> %llu MiB%s\n", n,
	       (unsigned long long)(size / 2048), (unsigned long long)(nsize / 2048), dry ? " (dry run)" : "");
	if (dry) return 0;
	put32(e + 12, (uint32_t)nsize);
	if (pwrite(fd, mbr, 512, 0) != 512 || fsync(fd)) { perror("nz-growpart: write"); return 1; }
	struct blkpg_partition p = { .start = (long long)start * 512, .length = (long long)nsize * 512, .pno = n };
	struct blkpg_ioctl_arg a = { .op = BLKPG_RESIZE_PARTITION, .datalen = sizeof(p), .data = &p };
	if (ioctl(fd, BLKPG, &a)) { perror("nz-growpart: BLKPG (new size takes effect after a reboot)"); return 1; }
	return 0;
}
