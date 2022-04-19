CFLAGS := -O2 -Wall -Wextra -pedantic
LDFLAGS :=

ifeq ($(shell uname),Darwin)
CC := xcrun clang
endif

# cross-platform compilation, e.g. make CC='zig cc --target=x86_64-linux-musl'

pbzx: pbzx.c Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) \
		-I xz-embedded/linux/include/linux \
		-I xz-embedded/userspace \
		xz-embedded/linux/lib/xz/xz_crc32.c \
		xz-embedded/linux/lib/xz/xz_dec_lzma2.c \
		xz-embedded/linux/lib/xz/xz_dec_stream.c \
		pbzx.c \
		-o pbzx

pbzy: pbzy.c Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -lAppleArchive -o pbzy pbzy.c

pbzz: pbzz.c Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -mmacosx-version-min=10.11 -lcompression -o pbzz pbzz.c

test: pbzx pbzy pbzz test.sh
	./test.sh

clean:
	rm -v -f -- pbzx pbzy pbzz
	rm -v -f -R -- tests

.PHONEY: clean test
