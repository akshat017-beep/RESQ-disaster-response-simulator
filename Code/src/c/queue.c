/* Circular Queue - ordinary (non-critical) emergency requests (RESQ) */
#include "resq_c.h"

#define QCAP (RESQ_MAX_NODES * 4)

void q_init(ReqQueue *q) { q->front = 0; q->rear = -1; q->size = 0; }

int q_enqueue(ReqQueue *q, int victimId) {
    if (q->size == QCAP) return 0;            /* queue overflow */
    q->rear = (q->rear + 1) % QCAP;
    q->ids[q->rear] = victimId;
    q->size++;
    return 1;
}

int q_dequeue(ReqQueue *q, int *victimId) {
    if (q->size == 0) return 0;               /* queue underflow */
    *victimId = q->ids[q->front];
    q->front  = (q->front + 1) % QCAP;
    q->size--;
    return 1;
}

int q_empty(const ReqQueue *q) { return q->size == 0; }
int q_size (const ReqQueue *q) { return q->size; }
