#ifndef RESQ_C_H
#define RESQ_C_H

#define RESQ_MAX_VICTIMS 200
#define RESQ_MAX_NAME 80
#define RESQ_MAX_LOCATION 80
#define RESQ_MAX_DESC 160
#define RESQ_MAX_NODES 30
#define RESQ_MAX_EDGES 100
#define RESQ_MAX_TEAMS 50
#define RESQ_MAX_HISTORY 500

typedef enum {
    RESQ_GREEN = 1,
    RESQ_YELLOW = 2,
    RESQ_RED = 3
} ResqSeverity;

typedef struct {
    int id;
    char name[RESQ_MAX_NAME];
    char location[RESQ_MAX_LOCATION];
    char description[RESQ_MAX_DESC];
    int affected;
    ResqSeverity severity;
    int status;
    int assigned_team;
    int route_cost;
} ResqVictim;

typedef struct {
    ResqVictim items[RESQ_MAX_VICTIMS];
    int count;
} ResqVictimStore;

typedef struct ResqListNode {
    ResqVictim value;
    struct ResqListNode *next;
} ResqListNode;

typedef struct {
    ResqListNode *head;
    int size;
} ResqLinkedList;

typedef struct {
    ResqVictim items[RESQ_MAX_VICTIMS];
    int front;
    int rear;
    int count;
} ResqQueue;

typedef struct {
    ResqVictim items[RESQ_MAX_VICTIMS];
    int size;
} ResqPriorityQueue;

typedef struct {
    ResqVictim items[RESQ_MAX_HISTORY];
    int top;
} ResqStack;

typedef struct {
    char names[RESQ_MAX_NODES][RESQ_MAX_LOCATION];
    int weights[RESQ_MAX_NODES][RESQ_MAX_NODES];
    int blocked[RESQ_MAX_NODES][RESQ_MAX_NODES];
    int count;
} ResqGraph;

typedef struct {
    int id;
    char name[RESQ_MAX_NAME];
    char current_location[RESQ_MAX_LOCATION];
    int capacity;
    int available;
} ResqTeam;

int resq_priority_score(const ResqVictim *victim);
int resq_add_victim(ResqVictimStore *store, const ResqVictim *victim);
ResqVictim *resq_find_victim(ResqVictimStore *store, int id);
int resq_update_status(ResqVictimStore *store, int id, int status);
void resq_print_victim(const ResqVictim *victim);
void resq_print_victims(const ResqVictimStore *store);
int resq_list_push(ResqLinkedList *list, const ResqVictim *victim);
int resq_list_remove(ResqLinkedList *list, int id);
void resq_list_print(const ResqLinkedList *list);
void resq_list_clear(ResqLinkedList *list);
int resq_queue_push(ResqQueue *queue, const ResqVictim *victim);
int resq_queue_pop(ResqQueue *queue, ResqVictim *out);
void resq_queue_print(const ResqQueue *queue);
int resq_pq_push(ResqPriorityQueue *queue, const ResqVictim *victim);
int resq_pq_pop(ResqPriorityQueue *queue, ResqVictim *out);
void resq_pq_print(const ResqPriorityQueue *queue);
int resq_stack_push(ResqStack *stack, const ResqVictim *victim);
int resq_stack_pop(ResqStack *stack, ResqVictim *out);
void resq_stack_print(const ResqStack *stack);
int resq_graph_add_location(ResqGraph *graph, const char *name);
int resq_graph_add_road(ResqGraph *graph, int from, int to, int weight);
int resq_graph_block_road(ResqGraph *graph, int from, int to, int blocked);
int resq_graph_find_location(const ResqGraph *graph, const char *name);
void resq_graph_print(const ResqGraph *graph);
void resq_graph_bfs(const ResqGraph *graph, int start);
int resq_graph_dijkstra(const ResqGraph *graph, int start, int target, int *path, int *path_len);
void resq_print_severity(ResqSeverity severity);
void resq_print_status(int status);
int resq_valid_text(const char *text, int max_len);

#endif
