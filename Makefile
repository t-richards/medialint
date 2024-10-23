# CFLAGS=-Wall -Werror -g -O3 -fsanitize=address -fno-omit-frame-pointer $(shell pkg-config --cflags libavcodec libavformat libavutil glib-2.0)
CFLAGS=-Wall -Werror -g -O3 $(shell pkg-config --cflags libavcodec libavformat libavutil glib-2.0)
LDFLAGS=$(shell pkg-config --libs libavcodec libavformat libavutil glib-2.0)

bin/medialint: src/*.c
	mkdir -p bin
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f bin/*

.PHONY: dist
dist: bin/medialint
	strip bin/medialint
