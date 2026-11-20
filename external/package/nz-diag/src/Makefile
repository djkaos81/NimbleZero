# nz-diag - built against the kernel tree's own 6x8 console font
CFLAGS ?= -O2
CFLAGS += -Wall -Wextra -Wno-format-truncation -std=gnu11
KERNEL_FONT ?= font_6x8.c

all: nz-diag nz-growpart

nz-diag: nz-diag.c $(KERNEL_FONT)
	$(CC) $(CFLAGS) -Ishim $(EXTRA_INC) -o $@ nz-diag.c $(KERNEL_FONT) $(LDFLAGS)

nz-growpart: nz-growpart.c
	$(CC) $(CFLAGS) -o $@ nz-growpart.c $(LDFLAGS)

clean:
	rm -f nz-diag nz-growpart
