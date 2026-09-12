CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Werror

.PHONY: all clean

all: exploit

exploit: exploit.c payload.S
	$(CC) $(CFLAGS) -static $^ -o $@

clean:
	$(RM) exploit
