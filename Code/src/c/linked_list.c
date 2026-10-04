/* Linked List - victim & rescue records (RESQ) */
#include <stdlib.h>
#include <string.h>
#include "resq_c.h"

void vl_init(VictimList *l) { l->head = NULL; l->count = 0; }

void vl_push(VictimList *l, VictimRecord v) {
    VictimNode *n = (VictimNode *)malloc(sizeof(VictimNode));
    if (!n) return;
    n->data = v;
    n->next = NULL;
    if (!l->head) {
        l->head = n;
    } else {
        VictimNode *cur = l->head;
        while (cur->next) cur = cur->next;
        cur->next = n;
    }
    l->count++;
}

VictimRecord *vl_find(VictimList *l, int id) {
    VictimNode *cur = l->head;
    while (cur) {
        if (cur->data.id == id) return &cur->data;
        cur = cur->next;
    }
    return NULL;
}

int vl_remove(VictimList *l, int id) {
    VictimNode *cur = l->head, *prev = NULL;
    while (cur) {
        if (cur->data.id == id) {
            if (prev) prev->next = cur->next;
            else      l->head    = cur->next;
            free(cur);
            l->count--;
            return 1;
        }
        prev = cur;
        cur  = cur->next;
    }
    return 0;
}

int vl_to_array(const VictimList *l, VictimRecord *out, int cap) {
    int i = 0;
    const VictimNode *cur = l->head;
    while (cur && i < cap) { out[i++] = cur->data; cur = cur->next; }
    return i;
}

void vl_free(VictimList *l) {
    VictimNode *cur = l->head;
    while (cur) { VictimNode *nx = cur->next; free(cur); cur = nx; }
    l->head = NULL;
    l->count = 0;
}
