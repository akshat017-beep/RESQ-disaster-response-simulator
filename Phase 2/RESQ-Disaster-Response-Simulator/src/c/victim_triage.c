#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "resq_c.h"

int resq_priority_score(const ResqVictim *victim) {
    int score = 0;
    if (victim == NULL) return -1;
    if (victim->severity == RESQ_RED) score += 300;
    else if (victim->severity == RESQ_YELLOW) score += 200;
    else score += 100;
    if (victim->affected > 0) {
        int extra = victim->affected * 2;
        if (extra > 80) extra = 80;
        score += extra;
    }
    if (victim->status == 0) score += 10;
    return score;
}

int resq_valid_text(const char *text, int max_len) {
    size_t n;
    if (text == NULL) return 0;
    n = strlen(text);
    if (n == 0 || n >= (size_t)max_len) return 0;
    for (size_t i = 0; i < n; ++i) {
        if (text[i] == '\n' || text[i] == '\r') return 0;
    }
    return 1;
}

int resq_add_victim(ResqVictimStore *store, const ResqVictim *victim) {
    if (store == NULL || victim == NULL) return 0;
    if (store->count >= RESQ_MAX_VICTIMS) return 0;
    if (resq_find_victim(store, victim->id) != NULL) return 0;
    store->items[store->count++] = *victim;
    return 1;
}

ResqVictim *resq_find_victim(ResqVictimStore *store, int id) {
    if (store == NULL) return NULL;
    for (int i = 0; i < store->count; ++i) {
        if (store->items[i].id == id) return &store->items[i];
    }
    return NULL;
}

int resq_update_status(ResqVictimStore *store, int id, int status) {
    ResqVictim *victim = resq_find_victim(store, id);
    if (victim == NULL) return 0;
    victim->status = status;
    return 1;
}

void resq_print_severity(ResqSeverity severity) {
    if (severity == RESQ_RED) printf("RED");
    else if (severity == RESQ_YELLOW) printf("YELLOW");
    else printf("GREEN");
}

void resq_print_status(int status) {
    switch (status) {
        case 0: printf("Waiting"); break;
        case 1: printf("Assigned"); break;
        case 2: printf("In Progress"); break;
        case 3: printf("Resolved"); break;
        case 4: printf("Cancelled"); break;
        default: printf("Unknown"); break;
    }
}

void resq_print_victim(const ResqVictim *victim) {
    if (victim == NULL) return;
    printf("ID: %d | Name: %s | Severity: ", victim->id, victim->name);
    resq_print_severity(victim->severity);
    printf(" | Affected: %d | Location: %s | Status: ", victim->affected, victim->location);
    resq_print_status(victim->status);
    printf(" | Priority: %d\n", resq_priority_score(victim));
    printf("Description: %s\n", victim->description);
}

void resq_print_victims(const ResqVictimStore *store) {
    if (store == NULL || store->count == 0) {
        printf("No victim records available.\n");
        return;
    }
    for (int i = 0; i < store->count; ++i) {
        resq_print_victim(&store->items[i]);
    }
}

int resq_compare_victim_priority(const void *left, const void *right) {
    const ResqVictim *a = (const ResqVictim *)left;
    const ResqVictim *b = (const ResqVictim *)right;
    int sa = resq_priority_score(a);
    int sb = resq_priority_score(b);
    if (sa != sb) return sb - sa;
    return a->id - b->id;
}

void resq_print_priority_order(const ResqVictimStore *store) {
    ResqVictim copy[RESQ_MAX_VICTIMS];
    if (store == NULL || store->count == 0) {
        printf("No waiting victims.\n");
        return;
    }
    int count = 0;
    for (int i = 0; i < store->count; ++i) {
        if (store->items[i].status == 0) copy[count++] = store->items[i];
    }
    if (count == 0) {
        printf("No waiting victims.\n");
        return;
    }
    qsort(copy, (size_t)count, sizeof(ResqVictim), resq_compare_victim_priority);
    printf("\nWaiting list in priority order\n");
    for (int i = 0; i < count; ++i) {
        printf("%d. ", i + 1);
        resq_print_victim(&copy[i]);
    }
}
