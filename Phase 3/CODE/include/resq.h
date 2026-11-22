/*
 * resq.h  --  RESQ Phase III: C data-structure and graph ABI
 *
 * This header is the shared interface between the C data-structure / graph
 * modules (dsa.c, graph.c) and the C++ controller layer (resq.hpp).
 *
 * Key design points:
 *   - ResqVictim carries a 'seq' field used as a secondary sort key so that
 *     equal-priority victims are served in first-in-first-out (arrival) order.
 *   - resq_store_enqueue() routes YELLOW and GREEN victims into two separate
 *     FIFO queues so YELLOW (urgent) is always served before GREEN (routine).
 *   - Insertion Sort, Quick Sort, Binary Search and Linear Search are exposed
 *     as public functions that operate on flat ResqVictim arrays; the
 *     controller layer calls these to present sorted record views and to
 *     search records efficiently.
 *   - resq_store_peek_next() lets the controller inspect the highest-priority
 *     pending victim WITHOUT removing it from the queue, eliminating the
 *     original "drop-on-failure" bug in dispatchNext().
 *
 * Team assignment:
 *   resq.h (this file)   -- Vanya Rathi (architecture + documentation)
 *   dsa.c                -- Akshat Dobhal (data structures + sorting/search)
 *   graph.c              -- Ayush Kandwal (graph, BFS, Dijkstra)
 */

#ifndef RESQ_PHASE3_C_H
#define RESQ_PHASE3_C_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Compile-time capacity constants                                     */
/* ------------------------------------------------------------------ */
#define RESQ_MAX_VICTIMS  200   /* max victims in linked list / queues  */
#define RESQ_MAX_NAME      80   /* max chars in a victim name           */
#define RESQ_MAX_LOCATION  80   /* max chars in a location name         */
#define RESQ_MAX_DESC     160   /* max chars in a description           */
#define RESQ_MAX_NODES     30   /* max graph nodes                      */
#define RESQ_MAX_HISTORY  500   /* max entries in operation-history stack*/

/* ------------------------------------------------------------------ */
/*  Triage severity levels (1 = lowest, 3 = highest)                   */
/* ------------------------------------------------------------------ */
typedef enum {
    RESQ_GREEN  = 1,   /* non-urgent  */
    RESQ_YELLOW = 2,   /* urgent      */
    RESQ_RED    = 3    /* critical    */
} ResqSeverity;

/* ------------------------------------------------------------------ */
/*  Core victim record                                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    int          id;                        /* unique victim identifier     */
    char         name[RESQ_MAX_NAME];       /* victim / reporter name       */
    char         location[RESQ_MAX_LOCATION]; /* incident location          */
    char         description[RESQ_MAX_DESC];  /* short incident description */
    int          affected;                  /* number of people affected    */
    ResqSeverity severity;                  /* triage category              */
    int          status;  /* 0 waiting | 1 assigned | 2 in-progress
                             | 3 resolved | 4 cancelled                     */
    int          assigned_team;             /* ID of dispatched rescue unit */
    int          route_cost;                /* Dijkstra cost to reach victim */
    int          seq;    /* insertion sequence number for FIFO tie-breaking */
} ResqVictim;

/* ------------------------------------------------------------------ */
/*  Linked list (master victim records – dynamically allocated nodes)  */
/* ------------------------------------------------------------------ */
typedef struct ResqVictimNode {
    ResqVictim            value;
    struct ResqVictimNode *next;
} ResqVictimNode;

typedef struct {
    ResqVictimNode *head;   /* first node                      */
    ResqVictimNode *tail;   /* last node  (O(1) append)        */
    int             size;   /* number of nodes in the list     */
} ResqLinkedList;

/* ------------------------------------------------------------------ */
/*  Dispatch queues                                                     */
/*  critical  -- binary max-heap for RED victims                       */
/*  urgent    -- circular FIFO for YELLOW victims                      */
/*  routine   -- circular FIFO for GREEN victims                       */
/* ------------------------------------------------------------------ */
typedef struct {
    ResqVictim items[RESQ_MAX_VICTIMS];
    int        size;
} ResqPriorityQueue;    /* max-heap keyed on (priority_score, -seq) */

typedef struct {
    ResqVictim items[RESQ_MAX_VICTIMS];
    int        front, rear, count;
} ResqQueue;            /* generic circular FIFO */

/* ------------------------------------------------------------------ */
/*  Operation-history stack (LIFO, newest-first display)              */
/* ------------------------------------------------------------------ */
typedef struct {
    ResqVictim items[RESQ_MAX_HISTORY];
    int        top;    /* index of top element; -1 when empty */
} ResqStack;

/* ------------------------------------------------------------------ */
/*  Unified data store (C layer)                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    ResqLinkedList    records;   /* master list of all victims       */
    ResqPriorityQueue critical;  /* RED victims (max-heap)           */
    ResqQueue         urgent;    /* YELLOW victims (FIFO)            */
    ResqQueue         routine;   /* GREEN victims (FIFO)             */
    ResqStack         history;   /* completed-operation stack        */
    int               next_id;   /* next victim ID to assign         */
    int               next_seq;  /* monotone insertion counter       */
} ResqDataStore;

/* ------------------------------------------------------------------ */
/*  Weighted adjacency-matrix graph                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    char names[RESQ_MAX_NODES][RESQ_MAX_LOCATION];
    int  weights[RESQ_MAX_NODES][RESQ_MAX_NODES]; /* 0 = no road      */
    int  blocked[RESQ_MAX_NODES][RESQ_MAX_NODES]; /* 1 = road blocked */
    int  count;                                   /* number of nodes   */
} ResqGraph;

/* ================================================================== */
/*  DSA module public API  (implemented in dsa.c -- Akshat Dobhal)    */
/* ================================================================== */

/* Store lifecycle */
void       resq_store_init          (ResqDataStore *store);
void       resq_store_clear         (ResqDataStore *store);

/* Input validation */
int        resq_valid_text          (const char *text, int max_len);

/* Priority scoring */
int        resq_priority_score      (const ResqVictim *victim);

/* Linked-list operations */
int        resq_store_add           (ResqDataStore *store, const ResqVictim *victim);
ResqVictim *resq_store_find         (ResqDataStore *store, int id);
int        resq_store_update_status (ResqDataStore *store, int id, int new_status);

/* Dispatch queue operations */
int        resq_store_enqueue       (ResqDataStore *store, const ResqVictim *victim);
int        resq_store_peek_next     (const ResqDataStore *store, ResqVictim *out);
int        resq_store_pop_next      (ResqDataStore *store, ResqVictim *out);

/*
 * Tier-aware peek/pop: tier 0 = critical heap (RED),
 *                      tier 1 = urgent FIFO (YELLOW),
 *                      tier 2 = routine FIFO (GREEN).
 *
 * resq_store_peek_tier -- non-destructive read of one tier's head.
 * resq_store_pop_tier  -- destructive pop from one specific tier.
 *
 * These let the controller try each tier independently, so a blocked YELLOW
 * victim does not prevent reachable GREEN victims from being dispatched.
 */
int        resq_store_peek_tier     (const ResqDataStore *store, int tier, ResqVictim *out);
int        resq_store_pop_tier      (ResqDataStore *store, int tier, ResqVictim *out);

/* History stack operations */
int        resq_history_push        (ResqDataStore *store, const ResqVictim *victim);
int        resq_history_peek_all    (const ResqDataStore *store,
                                     ResqVictim *out_array, int *out_count);

/* Sorting algorithms (operate on a flat array copy)
 *   resq_sort_insertion -- stable O(N^2) sort, good for small/nearly sorted data
 *   resq_sort_quick     -- O(N log N) average, good for large arrays
 * Both sort by (priority_score DESC, id ASC). */
void       resq_sort_insertion      (ResqVictim *arr, int n);
void       resq_sort_quick          (ResqVictim *arr, int low, int high);

/* Search algorithms (operate on a flat array).
 *   resq_search_linear -- O(N) unsorted scan, returns index or -1
 *   resq_search_binary -- O(log N) on array sorted by id ASC, returns index or -1 */
int        resq_search_linear       (const ResqVictim *arr, int n, int victim_id);
int        resq_search_binary       (const ResqVictim *arr, int n, int victim_id);

/* Helper: copy linked list to flat array for sorting/search, returns count */
int        resq_list_to_array       (const ResqDataStore *store,
                                     ResqVictim *out_arr, int max_count);

/* Printing utilities */
void       resq_print_severity      (ResqSeverity severity);
void       resq_print_status        (int status);
void       resq_print_victim        (const ResqVictim *victim);

/* ================================================================== */
/*  Graph module public API  (implemented in graph.c -- Ayush Kandwal) */
/* ================================================================== */

void  resq_graph_init          (ResqGraph *graph);
int   resq_graph_add_location  (ResqGraph *graph, const char *name);
int   resq_graph_find_location (const ResqGraph *graph, const char *name);
int   resq_graph_add_road      (ResqGraph *graph, int from, int to, int weight);
int   resq_graph_block_road    (ResqGraph *graph, int from, int to, int is_blocked);
int   resq_graph_bfs_reachable (const ResqGraph *graph, int start, int target);
int   resq_graph_dijkstra      (const ResqGraph *graph, int start, int target,
                                 int *path, int *path_len);
void  resq_graph_print         (const ResqGraph *graph, int use_ostream_flag);

#ifdef __cplusplus
}
#endif
#endif /* RESQ_PHASE3_C_H */
