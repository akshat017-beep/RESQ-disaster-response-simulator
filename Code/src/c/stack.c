/* Stack - rescue operation history / audit trail (RESQ) */
#include <string.h>
#include "resq_c.h"

void st_init(OpStack *s) { s->top = -1; }

int st_push(OpStack *s, const char *text) {
    if (s->top >= 255) return 0;              /* stack overflow */
    s->top++;
    strncpy(s->entries[s->top], text, RESQ_MAX_TEXT - 1);
    s->entries[s->top][RESQ_MAX_TEXT - 1] = '\0';
    return 1;
}

int st_pop(OpStack *s, char *out) {
    if (s->top < 0) return 0;                 /* stack underflow */
    strncpy(out, s->entries[s->top], RESQ_MAX_TEXT);
    s->top--;
    return 1;
}

int st_peek(const OpStack *s, char *out) {
    if (s->top < 0) return 0;
    strncpy(out, s->entries[s->top], RESQ_MAX_TEXT);
    return 1;
}

int st_empty(const OpStack *s) { return s->top < 0; }
