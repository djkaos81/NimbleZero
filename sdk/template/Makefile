# Cardputer Zero application template
#   make sim     desktop simulator (needs libsdl2-dev)        -> ./hello-sim
#   make         cross-compile for the device (SDK in PATH)   -> ./hello
#   make deb     package as hello_1.0_arm64.deb
# Cross compiler: put the SDK's bin/ directory in PATH (see docs/APP-DEVELOPMENT.md)
# or pass CROSS_COMPILE=/path/to/sdk/bin/aarch64-linux-
NAME    = hello
VERSION = 1.0
NZSDK  ?= ..
CROSS_COMPILE ?= aarch64-linux-
CFLAGS ?= -O2 -Wall -std=gnu11

$(NAME): main.c
	$(CROSS_COMPILE)gcc $(CFLAGS) -I$(NZSDK)/nzui -o $@ main.c

sim: main.c
	$(CC) $(CFLAGS) -DNZUI_SDL -I$(NZSDK)/nzui $$(sdl2-config --cflags) -o $(NAME)-sim main.c $$(sdl2-config --libs)

deb: $(NAME)
	$(NZSDK)/tools/nz-mkdeb --name $(NAME) --version $(VERSION) --exec $(NAME) --title "Hello" --comment "Template application"

clean:
	rm -f $(NAME) $(NAME)-sim *.deb

.PHONY: sim deb clean
