# RESQ — Intelligent Disaster Rescue & Emergency Response Simulator

**"Algorithms That Save Lives."**

## Team — RESQ Force

| Name | Role |
|---|---|
| Vanya Rathi | Team Lead |
| Nitin Singh | Team Member |
| Akshat Dobhal | Team Member |
| Ayush Kandwal | Team Member |

**Project Mentor:** Dr. Prabhdeep Singh

## Motivation

Disasters such as earthquakes, floods, fires, and multi-vehicle accidents create a sudden surge of emergency calls that exceeds available rescue teams, ambulances, and hospital beds. In these critical "golden hours," dispatch decisions are still made manually — over radio and phone — with no consolidated view of victim severity, team availability, or road passability.

This is fundamentally a computing problem: prioritizing a stream of requests, allocating limited resources, and finding shortest paths on a damaged road network. RESQ is a lightweight, offline-capable decision-support simulator that models a disaster area as a weighted graph and processes rescue requests through a severity-based priority queue, demonstrating measurably better response times than first-come-first-served dispatch.

## State of the Art

- **112/108 call-centre model (India):** manual, largely first-come-first-served dispatch
- **Commercial CAD suites** (Hexagon, Motorola PremierOne) and **GIS platforms** (ArcGIS, Google Maps APIs): expensive, proprietary, cloud-dependent, and assume intact road networks
- **Paper triage protocols** (START, SALT): no machine-readable, globally optimizable priority queue
- **Gap:** few systems combine triage + resource allocation + routing in one free, offline, auditable tool — this is what RESQ targets

## Project Goals

Build a menu-driven C/C++ simulator that:

1. Models a disaster area as a graph of locations
2. Accepts and stores rescue requests
3. Triages victims by severity using a priority queue
4. Allocates rescue teams, ambulances, and hospital beds
5. Computes shortest rescue routes with Dijkstra's algorithm
6. Maintains a stack-based undoable history of operations
7. Reports statistics comparing priority dispatch vs. FCFS dispatch

## Architecture

RESQ is a two-layer hybrid application:

- **C Layer (Data Structures & Algorithms):** Linked lists, FIFO queue, priority queue (min-heap), stack, graph (adjacency list), BFS, Dijkstra's algorithm, sorting/searching
- **C++ Layer (OOP Engine):** `Person` (abstract base) → `Victim`, `RescueTeam`; `Resource` (base with virtual `dispatch()`) → `Ambulance`, `RescueTeam`; plus `Hospital`, `Location`, `Disaster`, `RescueOperation`

```
Console UI / Dashboard
        │
Simulation Controller (C++ engine loop)
        │
   ┌────┼─────────────┬──────────────────┐
Request  Triage Module   Resource         Routing Engine
Intake   (Priority       Allocator        (Graph + BFS +
Queue    Queue)          (Teams,          Dijkstra)
                         Ambulances,
                         Hospitals)
        │
Records Store (Linked List) ── Operation History (Stack)
        │
File Persistence (map.txt, victims.csv, log.txt)
```

## Tech Stack

| Component | Choice |
|---|---|
| Languages | C (C11), C++ (C++17) |
| Compiler | GCC/G++ (MinGW / Linux) |
| IDE | VS Code / Code::Blocks |
| Build | Makefile |
| Version Control | Git & GitHub |
| Data Persistence | File I/O (.txt / .csv) |
| Diagrams | draw.io |

## Phase 1 — Proposal & Design

- Problem study, motivation, and state-of-the-art analysis
- Project goals and system architecture defined
- Class diagram and module split planned
- Milestone roadmap laid out

## Phase 2 — Core Implementation

- C structures for Victim and Location records; linked-list based victim registry
- FIFO queue for normal requests; priority queue (min-heap on severity, tie-broken by arrival time) for critical victims
- Graph module — adjacency list of the disaster map, BFS area exploration, Dijkstra shortest-route engine
- C++ OOP layer — Victim, RescueTeam, Ambulance, Hospital, Disaster, Location, RescueOperation classes with inheritance and polymorphism

## Phase 3 — Integration, Testing & Delivery

- Team/ambulance allocation logic; stack-based operation history with undo
- Sorting and searching of victims and resources
- File persistence, statistics module, and comparison report (priority dispatch vs. FCFS)
- Console dashboard
- Testing with three disaster scenarios: earthquake, flood, fire
- Final documentation and live demonstration

## Deliverables

- Fully working, menu-driven RESQ simulator (C + C++, GCC-compiled)
- Modular, reusable source code: linked list, queue, priority queue (min-heap), stack, graph, BFS, Dijkstra, sorting and searching, plus the C++ class hierarchy
- Three ready-to-run disaster scenarios — earthquake, flood, fire — with sample map and victim data files
- Statistics module and comparison report (priority dispatch vs. FCFS): average response time, victims rescued, resource utilization
- Persistent log files (rescue history, operation stack dump) for post-simulation audit
- Documentation: class diagram, system architecture diagram, algorithm complexity table, user manual, README
- Final presentation and live demonstration of a complete rescue cycle

## Assumptions

- Disaster map is a static weighted graph supplied as input; road changes occur only through simulated events
- Victim severity is a known integer score (1–5) assigned at registration
- Travel time is proportional to edge weight (no traffic/weather modeling)
- Rescue teams, ambulances, and hospital beds are finite and fixed per run
- Each ambulance serves one victim at a time
- No live GPS/sensor/network feed — all data is manual or file-based
- Single-user, single-machine system

## References

1. Cormen, Leiserson, Rivest & Stein, *Introduction to Algorithms*, 4th ed., MIT Press
2. Bjarne Stroustrup, *The C++ Programming Language*, 4th ed.
3. Kernighan & Ritchie, *The C Programming Language*, 2nd ed.
4. [NDMA India — Guidelines on Medical Preparedness and Mass Casualty Management](https://ndma.gov.in)
5. [START / SALT Mass-Casualty Triage Protocols](https://chemm.hhs.gov/startadult.htm)
6. [GeeksforGeeks — Graphs, Dijkstra, Heaps](https://www.geeksforgeeks.org)
7. E. W. Dijkstra, "A note on two problems in connexion with graphs," *Numerische Mathematik*, 1959
8. [Emergency Response Support System (ERSS-112), Ministry of Home Affairs, India](https://112.gov.in)
