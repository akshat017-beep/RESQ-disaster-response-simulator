# RESQ Phase III – Team Contributions

The assignments below reproduce the team responsibilities stated in the Phase III
report and reflect the actual implemented features in every source file.

---

## Contribution table

| Team member | Role | Source files | Implemented features |
|---|---|---|---|
| **Vanya Rathi** (Team Lead, DS-2) | Architecture, interface design, triage logic, integration, documentation | `include/resq.h`, `include/resq.hpp`, `src/SystemController.cpp`, `README.md`, `TEAM_CONTRIBUTIONS.md` | Shared C ABI (`resq.h`): all struct definitions, constants, and public API declarations; C++ domain API (`resq.hpp`): abstract `RescueUnit`, `RescueTeam`, `Ambulance`, `Hospital`, `Disaster`, `RescueOperation`, `ResqSystem`; `SystemController.cpp`: peek-before-pop dispatch bug fix, tier-aware dispatch with blocked-tier fall-through, non-destructive history read, idempotent scenario via `resetScenario()`, `sortVictimsByPriority()` (calls Insertion Sort / Quick Sort), `searchVictimById()` (calls Linear + Binary Search with comparison), `planRoute()`, `printDashboard()`, `printHistory()`, `listHospitals()`, `listDisasters()`, `listUnits()`, `validateInvariants()`, `runEarthquakeScenario()` (Dehradun 10-node map, 7 victims, 6 rescues, 1 unresolved), all C++ stream output |
| **Akshat Dobhal** (ML-2) | Data structures and algorithms | `src/dsa.c` | Dynamic singly-linked list with tail pointer (O(1) append); binary max-heap with `heap_compare()` using `(priority_score DESC, seq ASC)` for FIFO tie-breaking; `heap_sift_up()`, `heap_sift_down()`, `heap_push()`, `heap_pop()`, `heap_peek()`; circular FIFO queues for YELLOW (`urgent`) and GREEN (`routine`) victims; three-tier `resq_store_enqueue()` routing; `resq_store_peek_next()` and `resq_store_pop_next()` with critical→urgent→routine priority; tier-selective `resq_store_peek_tier()` and `resq_store_pop_tier()` to avoid YELLOW-queue starvation of GREEN victims; LIFO history stack with `resq_history_push()` and non-destructive `resq_history_peek_all()`; `resq_list_to_array()` for flat-array conversions; **Insertion Sort** (`resq_sort_insertion`) stable O(N²); **Quick Sort** (`resq_sort_quick`) median-of-three O(N log N); **Linear Search** (`resq_search_linear`) O(N); **Binary Search** (`resq_search_binary`) O(log N); `resq_priority_score()`, `resq_valid_text()`, `resq_print_victim()` |
| **Ayush Kandwal** (ML-4) | Graph, BFS, Dijkstra | `src/graph.c` | Weighted undirected adjacency-matrix graph (`ResqGraph`) with per-edge block state; `resq_graph_add_location()` with duplicate rejection; `resq_graph_add_road()` bidirectional with positive-weight guard; `resq_graph_block_road()` bidirectional blocking; **BFS reachability** (`resq_graph_bfs_reachable`) with `memset`-initialised visited array and queue-based traversal; **Dijkstra shortest path** (`resq_graph_dijkstra`) with `dist[]`, `prev[]`, `finalized[]` arrays, INT_MAX overflow guard, path reconstruction in correct order; `resq_graph_print()` formatted adjacency listing |
| **Nitin Singh** (ML-1) | C++ resource model, test suite, interactive console | `src/ResourceModel.cpp`, `src/main.cpp`, `tests/phase3_tests.cpp` | `ResourceModel.cpp`: `Location` constructor and display; abstract `RescueUnit` with `dispatch()` / `release()` guard logic; `RescueTeam` constructor and `describe()`; `Ambulance` constructor and `describe()`; `Hospital` with `admit()`, `discharge()`, `availableBeds()`, `print()`; `Disaster` with `escalate()`, `deescalate()`, `print()`; `RescueOperation` with `start()`, `complete()`, `cancel()`, `status()`, `print()`; `main.cpp`: 17-option interactive console, `readInt()` with retry on bad input, `readText()` with length validation, all menu handlers with `std::ostringstream` capture and formatted output; `phase3_tests.cpp`: 10 test groups T1–T10 covering empty-system invariants, graph, data store, sorting, search, priority-tier ordering, dispatch no-drop, non-destructive history, idempotent scenario, and full earthquake scenario correctness |

---

## Interface contracts

| Header | Purpose | Owner |
|--------|---------|-------|
| `include/resq.h` | C ABI: all C struct definitions, constants, `dsa.c` and `graph.c` public API | Vanya Rathi |
| `include/resq.hpp` | C++ API: domain-model classes and `ResqSystem` controller declaration | Vanya Rathi |

The C headers are consumed by both `dsa.c` / `graph.c` (C translation units) and
`SystemController.cpp` / `ResourceModel.cpp` / `main.cpp` / `phase3_tests.cpp`
(C++ translation units, wrapped in `extern "C"` guards).

## Build dependency graph

```
resq.h ──────────────── dsa.c (Akshat)
                 │
                 └────── graph.c (Ayush)
                 │
resq.hpp ────────┼────── ResourceModel.cpp (Nitin)
                 │
                 └────── SystemController.cpp (Vanya)
                 │
                 └────── main.cpp (Nitin)
                 │
                 └────── phase3_tests.cpp (Nitin)
```

