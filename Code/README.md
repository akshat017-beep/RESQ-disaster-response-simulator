# RESQ — Intelligent Disaster Rescue & Emergency Response Simulator
**“Algorithms That Save Lives.”**

Team Leader: Vanya Rathi (2510383410)
Members: Akshat Dobhal (2510370353), Ayush Kandwal (2510380235), Nitin Singh (2510370229)
Mentor: Mr. Prabhdeep Singh

## Build & run

```bash
make        # builds ./resq  (gcc for C, g++ for C++, C11 / C++17)
make run    # builds and runs the earthquake scenario
make clean
```

## What it does
Simulates an earthquake in a city sector: emergency calls arrive, victims are triaged,
critical cases jump the queue, the nearest free rescue unit is chosen by shortest travel
time, the victim is moved to a suitable hospital, and every action is logged for audit.

## Project structure

```
include/resq_c.h      C DSA layer  (structs + function prototypes, extern "C")
include/resq.hpp      C++ OOP layer (classes)
src/c/linked_list.c   Linked List    -> victim & rescue records
src/c/queue.c         Circular Queue -> ordinary (non-critical) requests
src/c/priority_queue.c Binary max-heap -> critical victims first
src/c/stack.c         Stack          -> rescue operation history / audit trail
src/c/graph.c         Graph + BFS (area exploration) + Dijkstra (shortest route)
src/c/sort_search.c   Insertion sort, quick sort, binary & linear search
src/cpp/resq.cpp      Class implementations + simulation engine
src/cpp/main.cpp      Scenario driver (map, resources, incoming calls)
```

## C — Data Structures & Algorithms
| Structure | File | Role in RESQ |
|---|---|---|
| Linked List | `linked_list.c` | Master register of all victim records |
| Queue | `queue.c` | FIFO for GREEN/YELLOW (non-critical) requests |
| Priority Queue | `priority_queue.c` | Max-heap on severity; ties broken by arrival time |
| Stack | `stack.c` | LIFO operation history, replayed as an audit trail |
| Graph | `graph.c` | Weighted undirected map; weight = travel minutes; blocked roads = weight 0 |
| BFS | `graph.c` | Finds which areas are reachable / cut off after the disaster |
| Dijkstra | `graph.c` | Fastest route from a unit to a victim and from a victim to a hospital |
| Sorting / Searching | `sort_search.c` | Triage ordering, ID lookup (binary), name lookup (linear) |

## C++ — Object-Oriented Programming
| Concept | Where |
|---|---|
| Classes & Objects | `Location`, `Victim`, `Hospital`, `Disaster`, `RescueOperation` |
| Encapsulation | All data members private, accessed through getters/setters (`Victim::setSeverity` clamps 1–10) |
| Inheritance | `RescueUnit` (abstract base) → `RescueTeam`, `Ambulance` |
| Polymorphism | `virtual travelTime()`, `describe()`; pure virtual `type()`, `capability()`; dispatch through base pointers |
| Constructors / Destructors | Unit counter increments in the constructor, decrements in the destructor; `~RescueOperation` frees units and the C linked list |
| Operator overloading | `Location::operator==` |
| Static member | `RescueUnit::unitCount_` |

## Dispatch rules modelled
- Severity 7–10 = RED (priority queue), 4–6 = YELLOW, 1–3 = GREEN (normal queue).
- Ambulances travel faster (siren priority) but cannot extract trapped RED victims — a ground team is required.
- Heavy-equipment teams are slower on debris-covered roads.
- RED victims may only be admitted to a trauma centre; hospitals refuse admission when beds run out.
- Locations disconnected by a destroyed road are flagged by BFS as needing air support.

## Extending
Add locations/roads with `addLocation()` / `addRoad()`, block roads with `blockRoad()`,
add responders with `addUnit(new RescueTeam(...))`, and feed calls with `reportVictim()`.
A flood or fire scenario is just a different `Disaster` object and a different map in `main.cpp`.
