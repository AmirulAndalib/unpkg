/*==========================================================================*\
) Copyright (C) 2022 by J.W https://github.com/jakwings/unpkg                (
)                                                                            (
)   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION          (
)                                                                            (
)  0. You just DO WHAT THE FUCK YOU WANT TO.                                 (
\*==========================================================================*/


/*\
 / xcrun clang -O2 -Wall -Wextra -pedantic -lcompression -o pbzz pbzz.c
 / https://developer.apple.com/documentation/compression
\*/

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// MacOS 10.11+
#include <compression.h>
// ntohll: Big-Endian (Network Byte Order) to Native (Host) 64-bit
#include <machine/endian.h>

#if !defined(PAGE_SIZE) || PAGE_SIZE < 1024
#define PAGE_SIZE 4096
#endif

/* DONT PANIC */
#define MIN(x, y) ((x) <= (y) ? (x) : (y))

#define PBZX_MAGIC "pbzx"   // lzma
#define PBZZ_MAGIC "pbzz"   // zlib
#define PBZ4_MAGIC "pbz4"   // lz4
#define PBZE_MAGIC "pbze"   // lzfse
#define PBZX_MAGIC_SIZE 4


#define LABEL "[pbzz] Error:"
#define OMG(message) \
        do { \
            fflush(stdout); \
            fprintf(stderr, "%s %s\n", LABEL, (message)); \
        } while (0)
#define WTF(exitcode) \
        do { \
            fflush(stdout); \
            fprintf(stderr, "\n%s at pbzz.c line#%d %s()\n", LABEL, __LINE__, __func__); \
            error_exit(exitcode); \
        } while (0)

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
    fflush(stdout);
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

    compression_stream stream;
    compression_status status;
    compression_algorithm algorithm;

    if (argc > 1) {
        if ((input = fopen(argv[1], "rb")) == NULL) {
            OMG(strerror(errno));
            WTF(ExitCode_Open);
        }
    }

    if (fread(&in_buf, PBZX_MAGIC_SIZE, 1, input) != 1) {
        OMG(strerror(errno));
        WTF(ExitCode_Read);
    }
    if (memcmp(in_buf, PBZX_MAGIC, PBZX_MAGIC_SIZE) == 0) {
        algorithm = COMPRESSION_LZMA;
    } else if (memcmp(in_buf, PBZZ_MAGIC, PBZX_MAGIC_SIZE) == 0) {
        algorithm = COMPRESSION_ZLIB;  // FIXME: did not pass the test yet
    } else if (memcmp(in_buf, PBZ4_MAGIC, PBZX_MAGIC_SIZE) == 0) {
        algorithm = COMPRESSION_LZ4;
    } else if (memcmp(in_buf, PBZE_MAGIC, PBZX_MAGIC_SIZE) == 0) {
        algorithm = COMPRESSION_LZFSE;
    } else {
        // TODO: macOS 12.0+ COMPRESSION_BROTLI ?
        WTF(ExitCode_Magic);
    }

    byte_read(input, &block_size, sizeof(block_size));
    // block size of uncompressed data
    block_size = ntohll(block_size);

//size_t it = 0;
    // iterating through chunks of data
    while (fread(&out_size, sizeof(out_size), 1, input) == 1) {
//++it;
        // size of uncompressed data
        out_size = ntohll(out_size);

        byte_read(input, &in_size, sizeof(in_size));
        // size of compressed data
        in_size = ntohll(in_size);

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

        status = compression_stream_init(&stream, COMPRESSION_STREAM_DECODE, algorithm);
        if (status != COMPRESSION_STATUS_OK) {
            WTF(ExitCode_Decompression);
        }
        stream.dst_ptr = out_buf;
        stream.dst_size = sizeof(out_buf);
        while (in_size > 0) {
            delta = byte_read(input, in_buf, MIN(sizeof(in_buf), in_size));
//fprintf(stderr, "[%03zu][i:%llu/%llu]\n", it, delta, in_size);
            in_size -= delta;
            stream.src_ptr = in_buf;
            stream.src_size = delta;
            do {
                /*\
                 / WTF: COMPRESSION_STATUS_END does NOT
                 /      only occurs if flags is set to COMPRESSION_STREAM_FINALIZE
                \*/
                if (stream.src_size > 0) {
                    status = compression_stream_process(&stream, 0);
//fprintf(stderr, "[%03zu][s:%d|%d,%d,%d]\n", it, status, COMPRESSION_STATUS_OK, COMPRESSION_STATUS_END, COMPRESSION_STATUS_ERROR);
                    if (status != COMPRESSION_STATUS_OK && status != COMPRESSION_STATUS_END) {
                        WTF(ExitCode_Decompression);
                    }
                } else if (in_size == 0) {
                    if (status != COMPRESSION_STATUS_END) {
                        status = compression_stream_process(&stream, COMPRESSION_STREAM_FINALIZE);
                    }
//fprintf(stderr, "[%03zu][s:%d|%d,%d,%d]\n", it, status, COMPRESSION_STATUS_OK, COMPRESSION_STATUS_END, COMPRESSION_STATUS_ERROR);
                    if (status != COMPRESSION_STATUS_OK && status != COMPRESSION_STATUS_END) {
                        WTF(ExitCode_Decompression);
                    }
                }
//fprintf(stderr, "[%03zu][d:%lu/%llu]\n", it, sizeof(out_buf) - stream.dst_size, out_size);
                if (in_size == 0 || stream.dst_size == 0) {
                    delta = sizeof(out_buf) - stream.dst_size;
                    if (delta == 0 && out_size > 0) {
                        // TODO: enlarge buffer if data is not corrupted ?
                        WTF(ExitCode_Bug);
                    }
                    byte_write(output, out_buf, delta);
                    out_size -= delta;
                    stream.dst_ptr = out_buf;
                    stream.dst_size = sizeof(out_buf);
                }
            } while (stream.src_size > 0 || (in_size == 0 && out_size > 0));
        }
        if (status != COMPRESSION_STATUS_END) {
            WTF(ExitCode_Decompression);
        }
        status = compression_stream_destroy(&stream);
        if (status != COMPRESSION_STATUS_OK) {
            WTF(ExitCode_Decompression);
        }
    }
    if (!feof(input)) {
        OMG(strerror(errno));
        WTF(ExitCode_Read);
    }
    if (input != stdin && fclose(input) == EOF) {
        OMG(strerror(errno));
        WTF(ExitCode_Close);
    }

    return ExitCode_Success;
}
