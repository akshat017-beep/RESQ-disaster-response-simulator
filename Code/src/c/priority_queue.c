/* Priority Queue (binary max-heap) - critical victims first (RESQ) */
#include "resq_c.h"

#define PCAP (RESQ_MAX_NODES * 4)

void pq_init(PriorityQueue *pq) { pq->size = 0; pq->seq = 0; }

/* a has higher priority than b ?  severity desc, then arrival asc */
static int higher(const PQItem *a, const PQItem *b) {
    if (a->severity != b->severity) return a->severity > b->severity;
    return a->arrival < b->arrival;
}

static void swap(PQItem *a, PQItem *b) { PQItem t = *a; *a = *b; *b = t; }

int pq_push(PriorityQueue *pq, int victimId, int severity) {
    int i;
    if (pq->size == PCAP) return 0;
    i = pq->size++;
    pq->items[i].victimId = victimId;
    pq->items[i].severity = severity;
    pq->items[i].arrival  = pq->seq++;
    while (i > 0) {                                  /* sift up */
        int parent = (i - 1) / 2;
        if (!higher(&pq->items[i], &pq->items[parent])) break;
        swap(&pq->items[i], &pq->items[parent]);
        i = parent;
    }
    return 1;
}

int pq_pop(PriorityQueue *pq, PQItem *out) {
    int i = 0;
    if (pq->size == 0) return 0;
    *out = pq->items[0];
    pq->items[0] = pq->items[--pq->size];
    for (;;) {                                       /* sift down */
        int l = 2 * i + 1, r = 2 * i + 2, best = i;
        if (l < pq->size && higher(&pq->items[l], &pq->items[best])) best = l;
        if (r < pq->size && higher(&pq->items[r], &pq->items[best])) best = r;
        if (best == i) break;
        swap(&pq->items[i], &pq->items[best]);
        i = best;
    }
    return 1;
}

int pq_peek(const PriorityQueue *pq, PQItem *out) {
    if (pq->size == 0) return 0;
    *out = pq->items[0];
    return 1;
}

int pq_empty(const PriorityQueue *pq) { return pq->size == 0; }
