CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2

all: mouse keyboard mouse_absolute

mouse: mouse.c hid_common.h
	$(CC) $(CFLAGS) -o mouse mouse.c

keyboard: keyboard.c hid_common.h
	$(CC) $(CFLAGS) -o keyboard keyboard.c

mouse_absolute: mouse_absolute.c hid_common.h
	$(CC) $(CFLAGS) -o mouse_absolute mouse_absolute.c -lm

clean:
	rm -f mouse keyboard mouse_absolute

install: all
	install -m 755 mouse /usr/local/bin/
	install -m 755 keyboard /usr/local/bin/
	install -m 755 mouse_absolute /usr/local/bin/

.PHONY: all clean install
