/*==========================================================================*\
) Copyright (C) 2022 by J.W https://github.com/jakwings/unpkg                (
)                                                                            (
)   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION          (
)                                                                            (
)  0. You just DO WHAT THE FUCK YOU WANT TO.                                 (
\*==========================================================================*/


/*\
 / xcrun clang -O2 -Wall -Wextra -pedantic -lAppleArchive -o pbzy pbzy.c
 / https://developer.apple.com/documentation/applearchive
\*/

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
// MacOS 11.0+ (10.16+)
#include <AppleArchive/AppleArchive.h>

#if !defined(PAGE_SIZE) || PAGE_SIZE < 1024
#define PAGE_SIZE 4096
#endif


#define LABEL "[pbzy] Error:"
#define OMG(message) \
        do { \
            fflush(stdout); \
            fprintf(stderr, "%s %s\n", LABEL, (message)); \
        } while (0)
#define WTF(exitcode) \
        do { \
            fflush(stdout); \
            fprintf(stderr, "\n%s at pbzy.c line#%d %s()\n", LABEL, __LINE__, __func__); \
            error_exit(exitcode); \
        } while (0)

enum ExitCode {
    ExitCode_Success,
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

#if 0
static size_t
byte_read(FILE *stream, void *buffer, size_t size)
{
    if (size > 0 && fread(buffer, size, 1, stream) != 1) {
        WTF(ExitCode_Read);
    }
    return size;
}
#endif


int main(int argc, char **argv)
{
    AAByteStream input, stream;
    uint8_t buffer[PAGE_SIZE];
    ssize_t size;

    if (argc > 1) {
        input = AAFileStreamOpenWithPath(argv[1], O_RDONLY | O_SYMLINK, 0);
    } else {
        input = AAFileStreamOpenWithFD(STDIN_FILENO, 0);
    }
    if (input == NULL) {
        WTF(ExitCode_Open);
    }

    // auto multi-threaded decompression
    if ((stream = AADecompressionInputStreamOpen(input, 0, 0)) == NULL) {
        WTF(ExitCode_Decompression);
    }
    while ((size = AAByteStreamRead(stream, buffer, sizeof(buffer))) > 0) {
        byte_write(stdout, buffer, size);
    }
    if (size < 0) {
        WTF(ExitCode_Decompression);
    }
    if (AAByteStreamClose(stream) < 0) {
        WTF(ExitCode_Decompression);
    }

    if (AAByteStreamClose(input) < 0) {
        WTF(ExitCode_Close);
    }

    return ExitCode_Success;
}
