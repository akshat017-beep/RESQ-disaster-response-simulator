/* ============================================================
   RESQ - Intelligent Disaster Rescue & Emergency Response Simulator
   "Algorithms That Save Lives."

   include/resq_c.h
   Pure-C layer : Data Structures & Algorithms
     Linked List    -> victim / rescue records
     Queue          -> normal (non-critical) emergency requests
     Priority Queue -> critical victims (max-heap on severity)
     Stack          -> rescue operation history (undo / audit trail)
     Graph          -> disaster area map
     BFS            -> area exploration / reachability
     Dijkstra       -> shortest (fastest) rescue route
     Sorting/Search -> victim & resource management
   ============================================================ */
#ifndef RESQ_C_H
#define RESQ_C_H

#ifdef __cplusplus
extern "C" {
#endif

#define RESQ_NAME_LEN   48
#define RESQ_MAX_NODES  64
#define RESQ_MAX_TEXT   96
#define RESQ_INF        1000000000

/* ---------- shared plain record used by every C structure ---------- */
typedef struct VictimRecord {
    int  id;
    char name[RESQ_NAME_LEN];
    int  age;
    int  severity;      /* 1 = minor ... 10 = critical */
    int  locationId;    /* graph node index            */
    int  rescued;       /* 0 = waiting, 1 = rescued     */
} VictimRecord;

/* ============================ LINKED LIST ============================ */
typedef struct VictimNode {
    VictimRecord       data;
    struct VictimNode *next;
} VictimNode;

typedef struct {
    VictimNode *head;
    int         count;
} VictimList;

void          vl_init   (VictimList *l);
void          vl_push   (VictimList *l, VictimRecord v);   /* append at tail */
VictimRecord *vl_find   (VictimList *l, int id);
int           vl_remove (VictimList *l, int id);
int           vl_to_array(const VictimList *l, VictimRecord *out, int cap);
void          vl_free   (VictimList *l);

/* =============================== QUEUE =============================== */
/* FIFO of victim ids - ordinary requests handled first-come-first-served */
typedef struct {
    int ids[RESQ_MAX_NODES * 4];
    int front, rear, size;
} ReqQueue;

void q_init   (ReqQueue *q);
int  q_enqueue(ReqQueue *q, int victimId);
int  q_dequeue(ReqQueue *q, int *victimId);
int  q_empty  (const ReqQueue *q);
int  q_size   (const ReqQueue *q);

/* =========================== PRIORITY QUEUE =========================== */
/* Binary max-heap. Higher severity first; ties broken by earlier arrival. */
typedef struct {
    int severity;
    int arrival;     /* monotonically increasing sequence number */
    int victimId;
} PQItem;

typedef struct {
    PQItem items[RESQ_MAX_NODES * 4];
    int    size;
    int    seq;
} PriorityQueue;

void pq_init (PriorityQueue *pq);
int  pq_push (PriorityQueue *pq, int victimId, int severity);
int  pq_pop  (PriorityQueue *pq, PQItem *out);
int  pq_peek (const PriorityQueue *pq, PQItem *out);
int  pq_empty(const PriorityQueue *pq);

/* =============================== STACK =============================== */
typedef struct {
    char entries[256][RESQ_MAX_TEXT];
    int  top;
} OpStack;

void st_init(OpStack *s);
int  st_push(OpStack *s, const char *text);
int  st_pop (OpStack *s, char *out);
int  st_peek(const OpStack *s, char *out);
int  st_empty(const OpStack *s);

/* =============================== GRAPH =============================== */
/* Weighted undirected graph: weight = travel time in minutes.
   A blocked edge (collapsed road / flooded street) has weight <= 0.     */
typedef struct {
    int  n;
    char names[RESQ_MAX_NODES][RESQ_NAME_LEN];
    int  w[RESQ_MAX_NODES][RESQ_MAX_NODES];   /* 0 = no edge */
} Graph;

void g_init      (Graph *g, int n);
void g_set_name  (Graph *g, int node, const char *name);
const char *g_name(const Graph *g, int node);
void g_add_edge  (Graph *g, int a, int b, int minutes);
void g_block_edge(Graph *g, int a, int b);          /* road destroyed */

int  g_bfs       (const Graph *g, int src, int *order, int *distHops);
int  g_dijkstra  (const Graph *g, int src, int *dist, int *prev);
int  g_path      (const int *prev, int src, int dst, int *path); /* len or 0 */

/* ====================== SORTING & SEARCHING ====================== */
void sort_by_severity_desc(VictimRecord *a, int n);   /* insertion sort */
void sort_by_id_asc       (VictimRecord *a, int n);   /* quick sort     */
int  binary_search_by_id  (const VictimRecord *a, int n, int id);
int  linear_search_by_name(const VictimRecord *a, int n, const char *name);

#ifdef __cplusplus
}
#endif
#endif /* RESQ_C_H */
