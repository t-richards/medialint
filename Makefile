override CC = clang
override CFLAGS += -fsanitize=bounds -fsanitize-trap=bounds -fcf-protection=full -fsanitize=safe-stack -fstack-clash-protection -std=c23 -Wl,-z,relro,-z,now -Wall -Werror -g -O3 -D_FORTIFY_SOURCE=3 $(shell pkg-config --cflags libavcodec libavformat libavutil glib-2.0 gio-2.0)
override LDFLAGS += $(shell pkg-config --libs libavcodec libavformat libavutil glib-2.0 gio-2.0)

bin/medialint: src/*.c
	mkdir -p bin
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f bin/*

.PHONY: dist
dist: bin/medialint
	strip bin/medialint

.PHONY: asan
asan: CFLAGS+=-fsanitize=address -fno-omit-frame-pointer
asan: LDFLAGS+=-fsanitize=address
asan: clean bin/medialint

.PHONY: lint
lint:
	clang-format --verbose -i --style=Microsoft src/*
