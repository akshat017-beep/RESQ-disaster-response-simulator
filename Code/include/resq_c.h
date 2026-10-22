#ifndef RESQ_C_H
#define RESQ_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int resq_enable_ansi(void);
int resq_is_interactive(void);
int resq_make_dir(const char* path);
int resq_encode(const char* in, char* out, size_t size);
int resq_decode(const char* in, char* out, size_t size);

#ifdef __cplusplus
}
#endif

#endif
