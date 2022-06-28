/*==========================================================================*\
) Copyright (C) 2022 by J.W https://github.com/jakwings/unpkg                (
)                                                                            (
)   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION          (
)                                                                            (
)  0. You just DO WHAT THE FUCK YOU WANT TO.                                 (
\*==========================================================================*/


/*\
 / http://newosxbook.com/articles/OTA.html
 / http://newosxbook.com/articles/OTA9.html
 / https://tukaani.org/xz/format.html
\*/

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <machine/endian.h>
#elif defined(__FreeBSD__)
#include <sys/endian.h>
#else
#include <endian.h>
#endif
// be64toh: Big-Endian (Network Byte Order) to Native (Host) 64-bit
#ifndef be64toh
#define be64toh(x) ntohll(x)
#endif

#include "xz.h"

#if !defined(PAGE_SIZE) || PAGE_SIZE < 1024
#define PAGE_SIZE 4096
#endif

/* DONT PANIC */
#define MIN(x, y) ((x) <= (y) ? (x) : (y))

#define PBZX_MAGIC "pbzx"
#define PBZX_MAGIC_SIZE 4


#define LABEL "[pbzx] Error:"
#define OMG(message) \
        do { \
            fflush(stdout); \
            fprintf(stderr, "%s %s\n", LABEL, (message)); \
        } while (0)
#define WTF(exitcode) \
        do { \
            fflush(stdout); \
            fprintf(stderr, "\n%s at pbzx.c line#%d %s()\n", LABEL, __LINE__, __func__); \
            print_stack_trace(); \
            error_exit(exitcode); \
        } while (0)

#if 1 + 1 == 1
#include <execinfo.h>
static void print_stack_trace(void)
{
    void *callstack[128];
    int i, frames = backtrace(callstack, 128);
    char **symbols = backtrace_symbols(callstack, frames);
    for (i = 0; i < frames; ++i) {
        fprintf(stderr, "[pbzx] Trace: %s\n", symbols[i]);
    }
    free(symbols);
}
#else
#define print_stack_trace() NULL
#endif

enum ExitCode {
    ExitCode_Success,
    ExitCode_Magic,
    ExitCode_Bug,
    ExitCode_Open,
    ExitCode_Close,
    ExitCode_Read,
    ExitCode_Write,
    ExitCode_Decompression,
};

static void
error_exit(enum ExitCode exitcode)
{
    fprintf(stderr, "\n");
    switch (exitcode) {
    case ExitCode_Magic: OMG("magic header not found"); break;
    case ExitCode_Bug: OMG("please report to developers"); break;
    case ExitCode_Open: OMG("failed to open file"); break;
    case ExitCode_Close: OMG("failed to close file"); break;
    case ExitCode_Read: OMG("failed to read input"); break;
    case ExitCode_Write: OMG("failed to write output"); break;
    case ExitCode_Decompression: OMG("decompression failed"); break;
    default: OMG("exitcode unknown");
    }
    exit(exitcode != 0 ? exitcode : -1);
}

static size_t
byte_write(FILE *stream, void *buffer, size_t size)
{
    if (size > 0 && fwrite(buffer, size, 1, stream) != 1) {
        WTF(ExitCode_Write);
    }
    return size;
}

static size_t
byte_read(FILE *stream, void *buffer, size_t size)
{
    if (size > 0 && fread(buffer, size, 1, stream) != 1) {
        WTF(ExitCode_Read);
    }
    return size;
}


static uint8_t in_buf[PAGE_SIZE];
static uint8_t out_buf[16777216];

int main(int argc, char **argv)
{
    FILE *input = stdin, *output = stdout;
    uint64_t block_size, delta, in_size, out_size;
    struct xz_buf stream;
    struct xz_dec *state;
    enum xz_ret status;

    if (argc > 1) {
        if ((input = fopen(argv[1], "rb")) == NULL) {
            OMG(strerror(errno));
            WTF(ExitCode_Open);
        }
    }

    if (fread(&in_buf, PBZX_MAGIC_SIZE, 1, input) != 1) {
        OMG(strerror(errno));
        WTF(ExitCode_Magic);
    }
    if (memcmp(in_buf, PBZX_MAGIC, PBZX_MAGIC_SIZE) != 0) {
        WTF(ExitCode_Magic);
    }

    byte_read(input, &block_size, sizeof(block_size));
    // block size of uncompressed data
    block_size = be64toh(block_size);

    // is support for CRC64 necessary ?
    xz_crc32_init();
#ifdef XZ_USE_CRC64
    xz_crc64_init();
#endif

    // maximum dictionary size: 64MiB (1 << 26) for preset compression level 9
    if ((state = xz_dec_init(XZ_DYNALLOC, 1 << 26)) == NULL) {
        OMG("failed to initialize decompressor");
        WTF(ExitCode_Decompression);
    }

//size_t it = 0;
    // iterating through chunks of data
    while (fread(&out_size, sizeof(out_size), 1, input) == 1) {
//++it;
        // size of uncompressed data
        out_size = be64toh(out_size);

        byte_read(input, &in_size, sizeof(in_size));
        // size of compressed data
        in_size = be64toh(in_size);

//fprintf(stderr, "[%03zu][ci:%llu/%lu]\n", it, in_size, sizeof(in_buf));
//fprintf(stderr, "[%03zu][co:%llu/%lu]\n", it, out_size, sizeof(out_buf));
        if (out_size > block_size) {
            OMG("recorded size of uncompressed data exceeds specified block size (ignored)");
        }
        if (in_size > block_size) {
            OMG("recorded size of compressed data exceeds specified block size (ignored)");
        }
        if (in_size > out_size) {
            OMG("recorded size of compressed data exceeds recorded size of uncompressed data (ignored)");
        }
        // XXX: uncompressed data ?
        if (in_size == out_size) {
            while (in_size > 0) {
                delta = byte_read(input, in_buf, MIN(sizeof(in_buf), in_size));
//fprintf(stderr, "[%03zu][io:%llu/%llu]\n", it, delta, in_size);
                in_size -= delta;
                byte_write(output, in_buf, delta);
            }
            continue;
        }

        stream.out = out_buf;
        stream.out_size = sizeof(out_buf);
        stream.out_pos = 0;
        while (in_size > 0) {
            delta = byte_read(input, in_buf, MIN(sizeof(in_buf), in_size));
//fprintf(stderr, "[%03zu][i:%llu/%llu]\n", it, delta, in_size);
            in_size -= delta;
            stream.in = in_buf;
            stream.in_size = delta;
            stream.in_pos = 0;
            do {
                if (stream.in_pos < stream.in_size) {
                    status = xz_dec_run(state, &stream);
//fprintf(stderr, "[%03zu][s:%d]\n", it, status);
                    switch (status) {
                    case XZ_OK:  // fall through
                    case XZ_STREAM_END:
                        break;
#ifdef XZ_DEC_ANY_CHECK
                    case XZ_UNSUPPORTED_CHECK:
                        OMG("unsupported integrity check (ignored)");
                        if (stream.out_pos == 0) {
                            continue;
                        }
                        break;
#endif
                    case XZ_MEM_ERROR:
                        OMG("failted to allocate memory");
                        goto xz_error;
                    case XZ_MEMLIMIT_ERROR:
                        OMG("exceeded maximum size of memory for xz dictionary");
                        WTF(ExitCode_Bug);
                    case XZ_FORMAT_ERROR:
                        OMG("invalid file format");
                        goto xz_error;
                    case XZ_OPTIONS_ERROR:
                        OMG("unsupported options in xz header");
                        goto xz_error;
                    case XZ_DATA_ERROR:  // fall through
                    case XZ_BUF_ERROR:
                        OMG("compressed data truncated or corrupted");
                        goto xz_error;
                    default:
                        OMG("unknown error during decompression");
xz_error:
                        WTF(ExitCode_Decompression);
                    }
                } else if (in_size == 0) {
//fprintf(stderr, "[%03zu][s:%d]\n", it, status);
                    if (status != XZ_STREAM_END) {
                        OMG("unexpected end of compressed data");
                        WTF(ExitCode_Decompression);
                    }
                }
//fprintf(stderr, "[%03zu][d:%lu/%llu]\n", it, stream.out_pos, out_size);
                if (in_size == 0 || stream.out_pos == stream.out_size) {
                    delta = stream.out_pos;
                    if (delta == 0 && out_size > 0) {
                        // TODO: enlarge buffer if data is not corrupted ?
                        WTF(ExitCode_Bug);
                    }
                    byte_write(output, out_buf, delta);
                    out_size -= delta;
                    stream.out = out_buf;
                    stream.out_size = sizeof(out_buf);
                    stream.out_pos = 0;
                }
            } while (stream.in_pos < stream.in_size || (in_size == 0 && out_size > 0));
        }
        if (status != XZ_STREAM_END) {
            OMG("unexpected end of compressed data");
            WTF(ExitCode_Decompression);
        }
        xz_dec_reset(state);
    }
    if (!feof(input)) {
        OMG(strerror(errno));
        WTF(ExitCode_Read);
    }
    if (input != stdin && fclose(input) == EOF) {
        OMG(strerror(errno));
        WTF(ExitCode_Close);
    }
    xz_dec_end(state);

    return ExitCode_Success;
}
