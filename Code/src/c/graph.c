/* Graph of the disaster area + BFS exploration + Dijkstra shortest route */
#include <string.h>
#include "resq_c.h"

void g_init(Graph *g, int n) {
    int i, j;
    g->n = (n > RESQ_MAX_NODES) ? RESQ_MAX_NODES : n;
    for (i = 0; i < RESQ_MAX_NODES; i++) {
        g->names[i][0] = '\0';
        for (j = 0; j < RESQ_MAX_NODES; j++) g->w[i][j] = 0;
    }
}

void g_set_name(Graph *g, int node, const char *name) {
    if (node < 0 || node >= g->n) return;
    strncpy(g->names[node], name, RESQ_NAME_LEN - 1);
    g->names[node][RESQ_NAME_LEN - 1] = '\0';
}

const char *g_name(const Graph *g, int node) {
    if (node < 0 || node >= g->n) return "?";
    return g->names[node];
}

void g_add_edge(Graph *g, int a, int b, int minutes) {
    if (a < 0 || b < 0 || a >= g->n || b >= g->n || minutes <= 0) return;
    g->w[a][b] = minutes;
    g->w[b][a] = minutes;
}

void g_block_edge(Graph *g, int a, int b) {
    if (a < 0 || b < 0 || a >= g->n || b >= g->n) return;
    g->w[a][b] = 0;
    g->w[b][a] = 0;
}

/* BFS - explore every location reachable from src (hop distance). */
int g_bfs(const Graph *g, int src, int *order, int *distHops) {
    int visited[RESQ_MAX_NODES];
    int queue[RESQ_MAX_NODES];
    int head = 0, tail = 0, count = 0, i;

    for (i = 0; i < g->n; i++) { visited[i] = 0; distHops[i] = -1; }
    if (src < 0 || src >= g->n) return 0;

    visited[src]  = 1;
    distHops[src] = 0;
    queue[tail++] = src;

    while (head < tail) {
        int u = queue[head++];
        order[count++] = u;
        for (i = 0; i < g->n; i++) {
            if (g->w[u][i] > 0 && !visited[i]) {
                visited[i]  = 1;
                distHops[i] = distHops[u] + 1;
                queue[tail++] = i;
            }
        }
    }
    return count;                                 /* nodes reachable */
}

/* Dijkstra - fastest travel time from src to every other location. */
int g_dijkstra(const Graph *g, int src, int *dist, int *prev) {
    int done[RESQ_MAX_NODES];
    int i, it;

    for (i = 0; i < g->n; i++) { dist[i] = RESQ_INF; prev[i] = -1; done[i] = 0; }
    if (src < 0 || src >= g->n) return 0;
    dist[src] = 0;

    for (it = 0; it < g->n; it++) {
        int u = -1, best = RESQ_INF, v;
        for (i = 0; i < g->n; i++)
            if (!done[i] && dist[i] < best) { best = dist[i]; u = i; }
        if (u == -1) break;                       /* rest unreachable */
        done[u] = 1;
        for (v = 0; v < g->n; v++) {
            int wt = g->w[u][v];
            if (wt > 0 && !done[v] && dist[u] + wt < dist[v]) {
                dist[v] = dist[u] + wt;
                prev[v] = u;
            }
        }
    }
    return 1;
}

/* Rebuild src -> dst path from the Dijkstra predecessor array. */
int g_path(const int *prev, int src, int dst, int *path) {
    int tmp[RESQ_MAX_NODES], len = 0, i, cur = dst;
    while (cur != -1 && cur != src) { tmp[len++] = cur; cur = prev[cur]; }
    if (cur != src) return 0;                     /* no route */
    tmp[len++] = src;
    for (i = 0; i < len; i++) path[i] = tmp[len - 1 - i];
    return len;
}
