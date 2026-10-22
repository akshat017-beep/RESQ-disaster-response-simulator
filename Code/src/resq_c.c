#if !defined(_WIN32)
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#  endif
#  ifndef _DEFAULT_SOURCE
#    define _DEFAULT_SOURCE 1
#  endif
#  if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#    define _DARWIN_C_SOURCE 1
#  endif
#endif

#include "resq_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <io.h>
#  ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif
#else
#  include <errno.h>
#  include <sys/ioctl.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

int resq_stdout_is_tty(void)
{
#ifdef _WIN32
    return _isatty(_fileno(stdout)) ? 1 : 0;
#else
    return isatty(STDOUT_FILENO) ? 1 : 0;
#endif
}

int resq_stdin_is_tty(void)
{
#ifdef _WIN32
    return _isatty(_fileno(stdin)) ? 1 : 0;
#else
    return isatty(STDIN_FILENO) ? 1 : 0;
#endif
}

int resq_term_init(void)
{
    const char *no_color = getenv("NO_COLOR");
    if (no_color != NULL && no_color[0] != '\0') {
        return 0;
    }
    if (!resq_stdout_is_tty()) {
        return 0;
    }
#ifdef _WIN32
    {
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (handle == INVALID_HANDLE_VALUE || !GetConsoleMode(handle, &mode)) {
            return 0;
        }
        if (!SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            return 0;
        }
        return 1;
    }
#else
    {
        const char *term = getenv("TERM");
        if (term != NULL && strcmp(term, "dumb") == 0) {
            return 0;
        }
        return 1;
    }
#endif
}

#ifndef _WIN32
static int resq_contains_utf8(const char *value)
{
    const char *p;
    if (value == NULL) {
        return 0;
    }
    for (p = value; *p != '\0'; ++p) {
        if ((p[0] == 'U' || p[0] == 'u') && (p[1] == 'T' || p[1] == 't') &&
            (p[2] == 'F' || p[2] == 'f')) {
            const char *q = p + 3;
            if (*q == '-') {
                ++q;
            }
            if (*q == '8') {
                return 1;
            }
        }
    }
    return 0;
}
#endif

int resq_term_enable_utf8(void)
{
#ifdef _WIN32
    int ok_out = SetConsoleOutputCP(CP_UTF8) ? 1 : 0;
    SetConsoleCP(CP_UTF8);
    return ok_out;
#else
    const char *vars[3];
    int i;
    vars[0] = getenv("LC_ALL");
    vars[1] = getenv("LC_CTYPE");
    vars[2] = getenv("LANG");
    for (i = 0; i < 3; ++i) {
        if (vars[i] != NULL && vars[i][0] != '\0') {
            return resq_contains_utf8(vars[i]);
        }
    }
    return 0;
#endif
}

int resq_term_columns(void)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        return (int)(info.srWindow.Right - info.srWindow.Left + 1);
    }
    return 0;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return (int)ws.ws_col;
    }
    return 0;
#endif
}

int resq_file_exists(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
#else
    {
        struct stat info;
        return stat(path, &info) == 0 ? 1 : 0;
    }
#endif
}

static int resq_is_dir(const char *path)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

static int resq_make_one_dir(const char *path)
{
    if (resq_is_dir(path)) {
        return 1;
    }
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

int resq_make_dirs(const char *path)
{
    char buffer[1024];
    size_t len;
    size_t i;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    len = strlen(path);
    if (len >= sizeof(buffer)) {
        return 0;
    }
    memcpy(buffer, path, len + 1);
    for (i = 1; i < len; ++i) {
        if (buffer[i] == '/' || buffer[i] == '\\') {
            if (buffer[i - 1] == ':' || buffer[i - 1] == '/' || buffer[i - 1] == '\\') {
                continue;
            }
            buffer[i] = '\0';
            if (!resq_make_one_dir(buffer)) {
                return 0;
            }
            buffer[i] = path[i];
        }
    }
    return resq_make_one_dir(buffer) && resq_is_dir(buffer);
}

int resq_copy_file(const char *src, const char *dst)
{
    char chunk[8192];
    size_t got;
    int ok = 1;
    FILE *in;
    FILE *out;

    in = fopen(src, "rb");
    if (in == NULL) {
        return 0;
    }
    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return 0;
    }
    while ((got = fread(chunk, 1, sizeof(chunk), in)) > 0) {
        if (fwrite(chunk, 1, got, out) != got) {
            ok = 0;
            break;
        }
    }
    if (ferror(in)) {
        ok = 0;
    }
    fclose(in);
    if (fclose(out) != 0) {
        ok = 0;
    }
    return ok;
}

int resq_replace_file(const char *src, const char *dst)
{
#ifdef _WIN32
    return MoveFileExA(src, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 1 : 0;
#else
    return rename(src, dst) == 0 ? 1 : 0;
#endif
}

int resq_remove_file(const char *path)
{
    return remove(path) == 0 ? 1 : 0;
}

int resq_remove_dir(const char *path)
{
#ifdef _WIN32
    return RemoveDirectoryA(path) ? 1 : 0;
#else
    return rmdir(path) == 0 ? 1 : 0;
#endif
}

uint32_t resq_crc32(uint32_t crc, const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t i;
    int bit;

    crc = ~crc;
    for (i = 0; i < len; ++i) {
        crc ^= (uint32_t)p[i];
        for (bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static unsigned int resq_next_key(uint32_t *state)
{
    *state = (*state) * 1103515245u + 12345u;
    return (unsigned int)((*state >> 16) & 0xFFu);
}

static uint32_t resq_seed(size_t length)
{
    return 0x52455351u ^ (uint32_t)((uint32_t)length * 2654435761u);
}

size_t resq_obfuscate_hex(const char *in, char *out, size_t out_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t len;
    size_t i;
    uint32_t state;

    if (in == NULL || out == NULL) {
        return 0;
    }
    len = strlen(in);
    if (out_size < len * 2 + 1) {
        return 0;
    }
    state = resq_seed(len);
    for (i = 0; i < len; ++i) {
        unsigned int b = ((unsigned int)(unsigned char)in[i]) ^ resq_next_key(&state);
        out[2 * i] = hex[(b >> 4) & 0xFu];
        out[2 * i + 1] = hex[b & 0xFu];
    }
    out[2 * len] = '\0';
    return len * 2;
}

static int resq_hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

int resq_deobfuscate_hex(const char *in, char *out, size_t out_size)
{
    size_t len;
    size_t count;
    size_t i;
    uint32_t state;

    if (in == NULL || out == NULL) {
        return 0;
    }
    len = strlen(in);
    if (len % 2 != 0) {
        return 0;
    }
    count = len / 2;
    if (out_size < count + 1) {
        return 0;
    }
    state = resq_seed(count);
    for (i = 0; i < count; ++i) {
        int hi = resq_hex_value(in[2 * i]);
        int lo = resq_hex_value(in[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return 0;
        }
        out[i] = (char)(((unsigned int)((hi << 4) | lo)) ^ resq_next_key(&state));
    }
    out[count] = '\0';
    return 1;
}
