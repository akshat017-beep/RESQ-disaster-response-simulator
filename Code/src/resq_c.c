#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "resq_c.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define RESQ_ISATTY _isatty
#else
#include <sys/stat.h>
#include <unistd.h>
#define RESQ_ISATTY isatty
#endif

int resq_enable_ansi(void) {
    const char* noColor = getenv("NO_COLOR");
    if ((noColor != NULL && noColor[0] != '\0') || !RESQ_ISATTY(1)) return 0;
#ifdef _WIN32
    {
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (!GetConsoleMode(handle, &mode)) return 0;
        return SetConsoleMode(handle, mode | 0x0004) ? 1 : 0;
    }
#else
    {
        const char* term = getenv("TERM");
        return (term != NULL && strcmp(term, "dumb") == 0) ? 0 : 1;
    }
#endif
}

int resq_is_interactive(void) { return RESQ_ISATTY(0) ? 1 : 0; }

int resq_make_dir(const char* path) {
#ifdef _WIN32
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    return (result == 0 || errno == EEXIST) ? 1 : 0;
}

static unsigned char resq_key(size_t i) { return (unsigned char)(0x5A ^ ((i * 31u) & 0xFFu)); }

int resq_encode(const char* in, char* out, size_t size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t i;
    size_t n = strlen(in);
    if (size < n * 2 + 1) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char b = (unsigned char)((unsigned char)in[i] ^ resq_key(i));
        out[2 * i] = hex[b >> 4];
        out[2 * i + 1] = hex[b & 15];
    }
    out[2 * n] = '\0';
    return 1;
}

static int resq_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int resq_decode(const char* in, char* out, size_t size) {
    size_t i;
    size_t n = strlen(in);
    if (n % 2 != 0 || size < n / 2 + 1) return 0;
    for (i = 0; i < n / 2; ++i) {
        int hi = resq_hex(in[2 * i]);
        int lo = resq_hex(in[2 * i + 1]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = (char)(((hi << 4) | lo) ^ resq_key(i));
    }
    out[n / 2] = '\0';
    return 1;
}
