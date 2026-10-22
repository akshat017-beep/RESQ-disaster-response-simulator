#ifndef RESQ_C_H
#define RESQ_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int resq_term_init(void);

int resq_term_enable_utf8(void);

int resq_stdout_is_tty(void);
int resq_stdin_is_tty(void);

int resq_term_columns(void);

int resq_file_exists(const char *path);
int resq_make_dirs(const char *path);
int resq_copy_file(const char *src, const char *dst);
int resq_replace_file(const char *src, const char *dst);
int resq_remove_file(const char *path);
int resq_remove_dir(const char *path);

uint32_t resq_crc32(uint32_t crc, const void *data, size_t len);

size_t resq_obfuscate_hex(const char *in, char *out, size_t out_size);

int resq_deobfuscate_hex(const char *in, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
