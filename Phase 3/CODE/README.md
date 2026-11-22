# RESQ – Phase III Implementation

This directory contains the native C/C++ Phase III implementation described in
`RESQ-PBL-Phase-3-Report_v2.pdf`. 

---

## What is new in Phase III

| Feature | Phase II | Phase III |
|---------|----------|-----------|
| Victim records | Simple array | Dynamically allocated linked list |
| Dispatch queue | Single FIFO | Three-tier: RED max-heap · YELLOW FIFO · GREEN FIFO |
| FIFO tie-breaking | None | `seq` counter ensures earlier calls are served first |
| Dispatch bug | Victim dropped on failure | Peek-before-pop: victim stays in queue if dispatch fails |
| Blocked-tier fall-through | N/A | YELLOW head blocked → GREEN tier tried next (no starvation) |
| Sorting | None | Insertion Sort + Quick Sort on flat victim arrays |
| Search | None | Linear Search + Binary Search with comparative output |
| History | Stack inverted on every read | Non-destructive `peek_all` — stack never modified |
| Re-run safety | State accumulates | `resetScenario()` makes scenario idempotent |
| Interactive console | 5 hardcoded options | 17-option menu with full input validation |
| Launcher | Simple compile-and-run | Test option · process detection · build validation |
| Resource polymorphism | Absent | `RescueUnit` abstract base · `Ambulance` · `RescueTeam` |

---

## Directory layout

```text
Phase 3/
├── include/
│   ├── resq.h          C data-structure and graph ABI  (Vanya Rathi)
│   └── resq.hpp        C++ domain-model and controller API  (Vanya Rathi)
├── src/
│   ├── dsa.c           Linked list, queues, heap, stack, sort, search  (Akshat Dobhal)
│   ├── graph.c         Weighted graph, BFS, Dijkstra  (Ayush Kandwal)
│   ├── ResourceModel.cpp  C++ resource classes  (Nitin Singh)
│   ├── SystemController.cpp  Triage, dispatch, integration  (Vanya Rathi)
│   └── main.cpp        Interactive console and entry point  (Nitin Singh)
├── tests/
│   └── phase3_tests.cpp  10-group integration test suite  (Nitin Singh)
├── build/              Compiled object files and executables (generated)
├── Makefile
├── run_resq_phase3.bat  One-click Windows launcher
├── TEAM_CONTRIBUTIONS.md
└── RESQ-PBL-Phase-3-Report_v2.pdf
```

---

## Build and run

### One-click (Windows)

Double-click **`run_resq_phase3.bat`**.  
The launcher will:
1. Check that GCC/G++ are on `PATH`.
2. Detect and terminate any stale `phase3.exe` process.
3. Optionally run the full test suite (10 groups) before launching.
4. Compile all six modules with strict flags (`-Wall -Wextra -Werror -pedantic`).
5. Link `phase3.exe` and launch the interactive simulator.

### Command line (PowerShell / CMD)

```powershell
# Build the simulator
make
.\phase3.exe

# Build and run the test suite
make tests
```

### Without `make` (PowerShell)

```powershell
New-Item -ItemType Directory -Force build
gcc  -std=c11  -Wall -Wextra -Werror -pedantic -Iinclude -c src/dsa.c          -o build/dsa.o
gcc  -std=c11  -Wall -Wextra -Werror -pedantic -Iinclude -c src/graph.c         -o build/graph.o
g++  -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -c src/ResourceModel.cpp   -o build/ResourceModel.o
g++  -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -c src/SystemController.cpp -o build/SystemController.o
g++  -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude -c src/main.cpp       -o build/main.o
g++  -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude `
     build/dsa.o build/graph.o build/ResourceModel.o build/SystemController.o build/main.o `
     -o phase3.exe
.\phase3.exe
```

---

## Interactive menu (17 options)

| # | Option | Algorithm / Feature |
|---|--------|---------------------|
| 1 | Run fixed earthquake scenario | Full pipeline: map build, victim registration, three-tier dispatch, Dijkstra routing, hospital transfer |
| 2 | Show disaster-area map | Adjacency matrix printout with blocked-road indicators |
| 3 | Plan route between two locations | Dijkstra shortest path (BFS reachability pre-check) |
| 4 | Register a new victim call | Validated input → linked list → dispatch queue |
| 5 | Show dashboard | Tabular victim overview with priority scores and status |
| 6 | Sort victims – Insertion Sort | Stable O(N²) sort on flat victim array |
| 7 | Sort victims – Quick Sort | O(N log N) avg, median-of-three pivot, same ordering as option 6 |
| 8 | Search for victim by ID | Linear Search (O(N)) vs Binary Search (O(log N)) with comparison |
| 9 | Dispatch next pending victim | Tier-aware peek-before-pop dispatch; falls through blocked tiers |
| 10 | Complete an in-progress operation | Victim status → Resolved; unit released; pushed to history |
| 11 | Transfer resolved victim to hospital | Hospital bed capacity check and admission |
| 12 | Show operation history | Non-destructive stack read (newest first) |
| 13 | Show all operations | Full list of created operations with status |
| 14 | List rescue units | Availability and type for each unit |
| 15 | List hospitals | Bed capacity and current admissions |
| 16 | List active disasters | Registered disaster events |
| 17 | Validate system invariants | Consistency checks on all data structures |
| 0 | Exit | — |

---

## Verification results

| Check | Result |
|-------|--------|
| C11 compilation (`-Wall -Wextra -Werror -pedantic`) | ✅ 0 warnings |
| C++17 compilation (`-Wall -Wextra -Werror -pedantic`) | ✅ 0 warnings |
| T1  Empty-system invariants | ✅ PASSED |
| T2  Graph (BFS + Dijkstra) | ✅ PASSED |
| T3  Data store (linked list, queues, heap, stack) | ✅ PASSED |
| T4  Sorting (Insertion Sort = Quick Sort order) | ✅ PASSED |
| T5  Search (Linear Search = Binary Search result) | ✅ PASSED |
| T6  Priority queue tier ordering + FIFO tie-breaking | ✅ PASSED |
| T7  `dispatchNext()` does not drop victims on failure | ✅ PASSED |
| T8  `printHistory()` non-destructive (stack not inverted) | ✅ PASSED |
| T9  `runEarthquakeScenario()` idempotent across two runs | ✅ PASSED |
| T10 Full scenario: 7 calls, 6 rescued, 1 unresolved | ✅ PASSED |

---

## Limitations

This is an educational simulation only. It has no persistence, authentication,
GPS feed, real-time sensor integration, external emergency API, or production
safety certification. It is not suitable for use in real emergency operations.

