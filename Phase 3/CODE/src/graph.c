/*
 * graph.c  --  RESQ Phase III: Weighted Graph, BFS Reachability, Dijkstra Routing
 *
 * Team member : Ayush Kandwal (ML-4)
 * Responsibility: Weighted adjacency-matrix graph, road-block toggle,
 *                 BFS reachability check, and Dijkstra shortest-path planning.
 *
 * Data structure:
 *   ResqGraph -- square adjacency matrix of size RESQ_MAX_NODES x RESQ_MAX_NODES.
 *   weights[u][v] = travel-time in simulated minutes (0 = no direct road).
 *   blocked[u][v] = 1 when the road exists but is currently impassable.
 *
 * Algorithms implemented:
 *   resq_graph_bfs_reachable -- Breadth-First Search reachability check.
 *     Determines whether a destination can be reached from the source
 *     without traversing any blocked roads.  O(V + E).
 *
 *   resq_graph_dijkstra      -- Classic Dijkstra shortest-path.
 *     Uses an O(V^2) matrix scan instead of a priority queue; appropriate
 *     for the small graphs used in this classroom simulator (up to 30 nodes).
 *     Returns the minimum travel cost and fills a path array.
 *     Skips blocked roads so the route is always physically traversable.
 *     Includes an INT_MAX overflow guard to prevent arithmetic wrap-around.
 */

#include "resq.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* ================================================================== */
/*  Graph initialisation                                                */
/* ================================================================== */

/*
 * resq_graph_init -- zero every field in the graph structure.
 * Zero-initialised weights mean "no road"; zero-initialised blocked
 * means "not blocked".
 */
void resq_graph_init(ResqGraph *graph)
{
    if (graph) {
        memset(graph, 0, sizeof(*graph));
    }
}

/* ================================================================== */
/*  Location management                                                 */
/* ================================================================== */

/*
 * resq_graph_find_location -- linear search for a location name.
 * Returns the node index (0-based), or -1 when not found.
 */
int resq_graph_find_location(const ResqGraph *graph, const char *name)
{
    int i;

    if (!graph || !name) {
        return -1;
    }

    for (i = 0; i < graph->count; ++i) {
        if (strcmp(graph->names[i], name) == 0) {
            return i;
        }
    }

    return -1;
}

/*
 * resq_graph_add_location -- add a new named location to the graph.
 *
 * Rejects empty names, names that are too long, and duplicate names.
 * Returns the new node index on success, -1 on failure.
 */
int resq_graph_add_location(ResqGraph *graph, const char *name)
{
    int new_index;

    if (!graph || !name) {
        return -1;
    }
    if (!resq_valid_text(name, RESQ_MAX_LOCATION)) {
        return -1; /* empty, too long, or contains control characters */
    }
    if (graph->count >= RESQ_MAX_NODES) {
        return -1; /* graph is full */
    }
    if (resq_graph_find_location(graph, name) >= 0) {
        return -1; /* duplicate location name */
    }

    new_index = graph->count;
    snprintf(graph->names[new_index], RESQ_MAX_LOCATION, "%s", name);
    graph->count++;
    return new_index;
}

/* ================================================================== */
/*  Road management                                                     */
/* ================================================================== */

/*
 * resq_graph_add_road -- add a bidirectional weighted road between two nodes.
 *
 * Both node indices must be valid and distinct.  Weight must be positive.
 * Adding a road also clears any existing block on that road pair.
 * Returns 1 on success, 0 on invalid arguments.
 */
int resq_graph_add_road(ResqGraph *graph, int from, int to, int weight)
{
    if (!graph) {
        return 0;
    }
    if (from < 0 || to < 0 || from >= graph->count || to >= graph->count) {
        return 0; /* invalid node indices */
    }
    if (from == to || weight <= 0) {
        return 0; /* self-loops and non-positive weights are not allowed */
    }

    graph->weights[from][to] = weight;
    graph->weights[to][from] = weight;
    graph->blocked[from][to] = 0;
    graph->blocked[to][from] = 0;
    return 1;
}

/*
 * resq_graph_block_road -- set or clear the blocked flag on an existing road.
 *
 * is_blocked = 1 to block, 0 to reopen.
 * Returns 1 on success, 0 when the road does not exist or indices are invalid.
 */
int resq_graph_block_road(ResqGraph *graph, int from, int to, int is_blocked)
{
    if (!graph) {
        return 0;
    }
    if (from < 0 || to < 0 || from >= graph->count || to >= graph->count) {
        return 0;
    }
    if (graph->weights[from][to] <= 0) {
        return 0; /* cannot block a road that does not exist */
    }

    graph->blocked[from][to] = is_blocked ? 1 : 0;
    graph->blocked[to][from] = is_blocked ? 1 : 0;
    return 1;
}

/* ================================================================== */
/*  BFS reachability                                                    */
/* ================================================================== */

/*
 * resq_graph_bfs_reachable -- determine whether 'target' is reachable from
 * 'start' using only open (non-blocked) roads.
 *
 * Uses an iterative BFS with a local queue array.  Nodes are visited at
 * most once, so the algorithm runs in O(V + E) time.
 *
 * Returns 1 if reachable, 0 otherwise (including invalid arguments).
 *
 * This check is called before Dijkstra to quickly gate the dispatch:
 * if BFS returns 0, Dijkstra is skipped entirely and the victim is
 * left unresolved (Hill Colony case in the earthquake scenario).
 */
int resq_graph_bfs_reachable(const ResqGraph *graph, int start, int target)
{
    int visited[RESQ_MAX_NODES]; /* 1 = already enqueued */
    int queue[RESQ_MAX_NODES];   /* BFS frontier          */
    int front;
    int rear;
    int current_node;
    int neighbour;

    if (!graph) {
        return 0;
    }
    if (start < 0 || target < 0 || start >= graph->count || target >= graph->count) {
        return 0;
    }
    if (start == target) {
        return 1; /* trivially reachable */
    }

    memset(visited, 0, sizeof(visited));
    front = 0;
    rear  = 0;

    visited[start]  = 1;
    queue[rear++]   = start;

    while (front < rear) {
        current_node = queue[front++];

        if (current_node == target) {
            return 1; /* reached the destination */
        }

        for (neighbour = 0; neighbour < graph->count; ++neighbour) {
            if (graph->weights[current_node][neighbour] > 0 &&
                !graph->blocked[current_node][neighbour] &&
                !visited[neighbour]) {
                visited[neighbour] = 1;
                queue[rear++]      = neighbour;
            }
        }
    }

    return 0; /* target not reachable */
}

/* ================================================================== */
/*  Dijkstra shortest-path                                              */
/* ================================================================== */

/*
 * resq_graph_dijkstra -- compute the shortest path from 'start' to 'target'
 * using Dijkstra's algorithm on an adjacency matrix.
 *
 * Parameters:
 *   graph    -- the graph to search
 *   start    -- source node index
 *   target   -- destination node index
 *   path     -- caller-allocated array of at least RESQ_MAX_NODES ints;
 *               filled with the sequence of node indices on the shortest path
 *   path_len -- set to the number of nodes in 'path' on success
 *
 * Returns the total travel cost (non-negative integer) on success, or -1
 * when the target is unreachable (all roads from source to target are blocked
 * or simply absent) or when arguments are invalid.
 *
 * Implementation notes:
 *   - Uses an O(V^2) visited-array scan for the minimum-distance extraction
 *     step, adequate for RESQ_MAX_NODES = 30.
 *   - Skips edges whose blocked flag is set.
 *   - Guards against INT_MAX arithmetic overflow during relaxation.
 *   - Reconstructs the path by tracing 'prev[]' back from target to start,
 *     then reversing the reversed path into 'path[]'.
 */
int resq_graph_dijkstra(const ResqGraph *graph,
                        int              start,
                        int              target,
                        int             *path,
                        int             *path_len)
{
    int dist[RESQ_MAX_NODES];     /* shortest known distance from start  */
    int prev[RESQ_MAX_NODES];     /* predecessor of each node on best path*/
    int finalized[RESQ_MAX_NODES]; /* 1 when node's distance is final     */
    int reversed[RESQ_MAX_NODES]; /* temporary: path from target to start */
    int reversed_len;             /* number of nodes in reversed[]        */

    int iteration;
    int node;
    int neighbour;
    int best_node;
    int best_dist;
    int new_dist;
    int i;

    if (path_len) {
        *path_len = 0;
    }

    if (!graph || !path || !path_len) {
        return -1;
    }
    if (start < 0 || target < 0 || start >= graph->count || target >= graph->count) {
        return -1;
    }

    /* Initialise distances to "infinity", predecessors to "none" */
    for (node = 0; node < graph->count; ++node) {
        dist[node]      = INT_MAX;
        prev[node]      = -1;
        finalized[node] = 0;
    }
    dist[start] = 0;

    for (iteration = 0; iteration < graph->count; ++iteration) {
        /* Find the unfinalized node with the smallest known distance */
        best_node = -1;
        best_dist = INT_MAX;
        for (node = 0; node < graph->count; ++node) {
            if (!finalized[node] && dist[node] < best_dist) {
                best_dist = dist[node];
                best_node = node;
            }
        }

        if (best_node < 0) {
            break; /* no reachable unfinalized nodes remain */
        }

        finalized[best_node] = 1;

        if (best_node == target) {
            break; /* shortest path to target is now final */
        }

        /* Relax all outgoing edges from best_node */
        for (neighbour = 0; neighbour < graph->count; ++neighbour) {
            if (graph->weights[best_node][neighbour] <= 0) {
                continue; /* no road */
            }
            if (graph->blocked[best_node][neighbour]) {
                continue; /* road is blocked */
            }
            if (finalized[neighbour]) {
                continue; /* distance already final */
            }
            if (dist[best_node] == INT_MAX) {
                continue; /* guard against overflow */
            }

            new_dist = dist[best_node] + graph->weights[best_node][neighbour];
            if (new_dist < dist[neighbour]) {
                dist[neighbour] = new_dist;
                prev[neighbour] = best_node;
            }
        }
    }

    /* Target was not reached */
    if (dist[target] == INT_MAX) {
        return -1;
    }

    /* Reconstruct path by tracing predecessors from target back to start */
    reversed_len = 0;
    node = target;
    while (node >= 0) {
        if (reversed_len >= RESQ_MAX_NODES) {
            return -1; /* safety: path too long (should never happen) */
        }
        reversed[reversed_len++] = node;
        if (node == start) {
            break;
        }
        node = prev[node];
    }

    /* Verify we actually traced back to start */
    if (reversed[reversed_len - 1] != start) {
        return -1;
    }

    /* Reverse into path[] so it runs from start to target */
    for (i = 0; i < reversed_len; ++i) {
        path[i] = reversed[reversed_len - 1 - i];
    }
    *path_len = reversed_len;

    return dist[target];
}

/* ================================================================== */
/*  Map display                                                         */
/* ================================================================== */

/*
 * resq_graph_print -- print all roads in the graph to stdout.
 *
 * Each road is printed once (upper triangle of the symmetric matrix).
 * Blocked roads are labelled "BLOCKED"; open roads show "OPEN".
 *
 * The use_ostream_flag parameter is reserved for future C++ stream
 * routing and is currently unused at the C level.
 */
void resq_graph_print(const ResqGraph *graph, int use_ostream_flag)
{
    int i, j;
    int any_road;

    (void)use_ostream_flag; /* reserved; unused at C level */

    if (!graph || graph->count == 0) {
        puts("Map is empty.");
        return;
    }

    any_road = 0;
    for (i = 0; i < graph->count; ++i) {
        for (j = i + 1; j < graph->count; ++j) {
            if (graph->weights[i][j] > 0) {
                printf("  %-24s <-> %-24s : %3d min  [%s]\n",
                       graph->names[i],
                       graph->names[j],
                       graph->weights[i][j],
                       graph->blocked[i][j] ? "BLOCKED" : "OPEN   ");
                any_road = 1;
            }
        }
    }

    if (!any_road) {
        puts("No roads have been added yet.");
    }
}
