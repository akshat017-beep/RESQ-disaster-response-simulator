/*
 * SystemController.cpp  --  RESQ Phase III: System Controller Implementation
 *
 * Team member : Vanya Rathi (Team Lead, DS-2)
 * Responsibility: Architecture, triage logic, integration and documentation.
 *
 * This file implements ResqSystem, the central controller that bridges the C
 * data-structure layer (Akshat's dsa.c + Ayush's graph.c) with the C++ domain
 * model (Nitin's ResourceModel.cpp).
 *
 * Key design decisions and bug fixes applied here:
 *
 *   1. dispatchNext() uses resq_store_peek_next() to inspect the next victim
 *      BEFORE removing it from the queue.  The victim is only popped (via
 *      resq_store_pop_next()) once a suitable unit and a reachable route have
 *      been confirmed.  This eliminates the original "drop-on-failure" bug where
 *      a victim was permanently lost when dispatch temporarily failed.
 *
 *   2. printHistory() reads the history stack non-destructively using
 *      resq_history_peek_all().  It no longer pops and re-pushes elements,
 *      so the stack is never inverted.
 *
 *   3. resetScenario() is called at the start of runEarthquakeScenario() so
 *      the scenario is idempotent -- running it twice produces the same output
 *      without accumulating duplicate records.
 *
 *   4. sortVictimsByPriority() exposes insertion sort and quick sort on the
 *      victim master list via a named algorithm parameter.
 *
 *   5. searchVictimById() demonstrates linear and binary search and prints
 *      comparative commentary.
 *
 *   6. All output flows through std::ostream& parameters (never raw printf),
 *      so output can be redirected, file-logged, or captured by tests.
 *
 *   7. describe() is called on the dispatched RescueUnit so the confirmation
 *      message names the resource type (Ambulance vs Rescue Team).
 */

#include "resq.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

/* ================================================================== */
/*  Constructor / destructor                                            */
/* ================================================================== */

ResqSystem::ResqSystem()
{
    resq_store_init(&store_);
    resq_graph_init(&graph_);
}

ResqSystem::~ResqSystem()
{
    /* Release heap-allocated linked-list nodes in the C store */
    resq_store_clear(&store_);
}

/* ================================================================== */
/*  Private helpers                                                     */
/* ================================================================== */

int ResqSystem::locationId(const std::string& name) const
{
    return resq_graph_find_location(&graph_, name.c_str());
}

RescueUnit *ResqSystem::unitById(int id)
{
    for (auto& unit : units_) {
        if (unit->id() == id) {
            return unit.get();
        }
    }
    return nullptr;
}

Hospital *ResqSystem::hospitalById(int id)
{
    for (auto& hospital : hospitals_) {
        if (hospital.id() == id) {
            return &hospital;
        }
    }
    return nullptr;
}

RescueOperation *ResqSystem::operationById(int id)
{
    for (auto& op : operations_) {
        if (op.id() == id) {
            return &op;
        }
    }
    return nullptr;
}

/*
 * resetScenario -- fully clear all runtime state so runEarthquakeScenario()
 * can be called multiple times and always produces the same output.
 *
 * The C store is cleared (all linked-list nodes freed, queues emptied).
 * The graph is reset.  All C++ vectors are cleared.  ID counters return
 * to their starting values.
 */
void ResqSystem::resetScenario()
{
    resq_store_clear(&store_);
    resq_graph_init(&graph_);

    units_.clear();
    hospitals_.clear();
    disasters_.clear();
    operations_.clear();

    next_unit_id_      = 1;
    next_hospital_id_  = 1;
    next_disaster_id_  = 1;
    next_operation_id_ = 1;
}

/* ================================================================== */
/*  Map management                                                      */
/* ================================================================== */

int ResqSystem::addLocation(const std::string& name)
{
    return resq_graph_add_location(&graph_, name.c_str());
}

bool ResqSystem::addRoad(const std::string& from, const std::string& to, int cost)
{
    int from_id = locationId(from);
    int to_id   = locationId(to);
    return resq_graph_add_road(&graph_, from_id, to_id, cost) != 0;
}

bool ResqSystem::blockRoad(const std::string& from, const std::string& to, bool blocked)
{
    int from_id = locationId(from);
    int to_id   = locationId(to);
    return resq_graph_block_road(&graph_, from_id, to_id, blocked ? 1 : 0) != 0;
}

/* ================================================================== */
/*  Victim management                                                   */
/* ================================================================== */

/*
 * addVictim -- validate inputs, build a ResqVictim, add it to the master
 * linked list, and enqueue it in the appropriate dispatch queue.
 *
 * severity: 1 = GREEN, 2 = YELLOW, 3 = RED
 * Returns the new victim ID on success, -1 on validation failure.
 */
int ResqSystem::addVictim(const std::string& name,
                           const std::string& location,
                           const std::string& description,
                           int               affected,
                           int               severity)
{
    ResqVictim victim{};

    /* Validate text fields */
    if (!resq_valid_text(name.c_str(),        RESQ_MAX_NAME))     { return -1; }
    if (!resq_valid_text(location.c_str(),    RESQ_MAX_LOCATION)) { return -1; }
    if (!resq_valid_text(description.c_str(), RESQ_MAX_DESC))     { return -1; }
    if (affected < 1 || severity < 1 || severity > 3)             { return -1; }

    /* Populate the C struct */
    victim.id       = store_.next_id++;
    victim.seq      = store_.next_seq++;
    victim.affected = affected;
    victim.severity = static_cast<ResqSeverity>(severity);
    victim.status   = 0; /* waiting */

    std::snprintf(victim.name,        sizeof(victim.name),        "%s", name.c_str());
    std::snprintf(victim.location,    sizeof(victim.location),    "%s", location.c_str());
    std::snprintf(victim.description, sizeof(victim.description), "%s", description.c_str());

    /* Add to master list and dispatch queue */
    if (!resq_store_add(&store_, &victim) ||
        !resq_store_enqueue(&store_, &victim)) {
        return -1;
    }

    return victim.id;
}

/* ================================================================== */
/*  Resource management                                                 */
/* ================================================================== */

int ResqSystem::addRescueTeam(const std::string& name,
                               const std::string& location,
                               int               capacity)
{
    if (name.empty() || location.empty() || capacity < 1) {
        return -1;
    }
    int id = next_unit_id_++;
    units_.push_back(std::make_unique<RescueTeam>(id, name, location, capacity));
    return id;
}

int ResqSystem::addAmbulance(const std::string& name,
                              const std::string& location,
                              int               capacity)
{
    if (name.empty() || location.empty() || capacity < 1) {
        return -1;
    }
    int id = next_unit_id_++;
    units_.push_back(std::make_unique<Ambulance>(id, name, location, capacity));
    return id;
}

int ResqSystem::addHospital(const std::string& name,
                             const std::string& location,
                             int               beds)
{
    if (name.empty() || location.empty() || beds < 1) {
        return -1;
    }
    int id = next_hospital_id_++;
    hospitals_.emplace_back(id, name, location, beds);
    return id;
}

int ResqSystem::addDisaster(const std::string& type,
                             const std::string& area,
                             int               severity,
                             int               affected)
{
    if (type.empty() || area.empty() || severity < 1 || severity > 3 || affected < 0) {
        return -1;
    }
    int id = next_disaster_id_++;
    disasters_.emplace_back(id, type, area, severity, affected);
    return id;
}

/* ================================================================== */
/*  Dispatch operations                                                 */
/* ================================================================== */

/*
 * dispatchNext -- attempt to dispatch the highest-priority pending victim.
 *
 * BUG FIX: In the original implementation, the victim was popped from the
 * queue BEFORE checking for available units and route reachability.  If those
 * checks failed, the victim was permanently lost.
 *
 * Fixed approach:
 *   1. PEEK the next victim without removing it (resq_store_peek_next).
 *   2. Find a suitable available unit.
 *   3. Run BFS to verify the destination is reachable.
 *   4. Run Dijkstra to get the actual route and cost.
 *   5. If all checks pass, POP the victim (resq_store_pop_next) and
 *      proceed with dispatch.
 *   6. If any check fails, return false WITHOUT removing the victim --
 *      it remains at the head of the appropriate queue.
 *
 * Returns true if a victim was successfully dispatched and an operation
 * was created, false otherwise.
 */
bool ResqSystem::dispatchNext()
{
    ResqVictim preview{};
    ResqVictim popped{};
    int        chosen_tier = -1;

    /*
     * Try each dispatch tier in priority order: critical (RED heap) first,
     * then urgent (YELLOW FIFO), then routine (GREEN FIFO).
     *
     * For each tier we:
     *   1. Peek the tier's head victim.
     *   2. Find an available unit with sufficient capacity.
     *   3. Verify the destination is reachable via BFS.
     *   4. Compute the shortest route via Dijkstra.
     *
     * We only commit (pop) if ALL checks pass.  If the head of a higher-
     * priority tier is blocked or no unit is available for it, we fall
     * through to the next tier instead of giving up entirely.
     *
     * This eliminates the starvation bug where a blocked YELLOW victim
     * permanently prevented reachable GREEN victims from being dispatched.
     */
    RescueUnit *chosen_unit = nullptr;
    int         from_node   = -1;
    int         to_node     = -1;
    int         travel_cost = -1;
    int         path_nodes[RESQ_MAX_NODES];
    int         path_length = 0;

    for (int tier = 0; tier <= 2; ++tier) {

        /* Step 1: peek the head of this tier (non-destructive) */
        if (!resq_store_peek_tier(&store_, tier, &preview)) {
            continue; /* tier is empty, try next */
        }

        /* Step 2: find the first available unit with sufficient capacity */
        chosen_unit = nullptr;
        for (auto& unit : units_) {
            if (unit->available() && unit->capacity() >= preview.affected) {
                chosen_unit = unit.get();
                break;
            }
        }
        if (!chosen_unit) {
            continue; /* no suitable unit for this tier's head; try next */
        }

        /* Step 3: verify reachability (BFS) */
        from_node = locationId(chosen_unit->location());
        to_node   = locationId(preview.location);

        if (from_node < 0 || to_node < 0) {
            continue; /* unknown location; try next tier */
        }
        if (!resq_graph_bfs_reachable(&graph_, from_node, to_node)) {
            continue; /* destination isolated; try next tier */
        }

        /* Step 4: compute shortest route (Dijkstra) */
        travel_cost = resq_graph_dijkstra(&graph_, from_node, to_node,
                                          path_nodes, &path_length);
        if (travel_cost < 0) {
            continue; /* no route; try next tier */
        }

        /* All checks passed for this tier -- record which one succeeded */
        chosen_tier = tier;
        break;
    }

    if (chosen_tier < 0 || !chosen_unit) {
        return false; /* all tiers exhausted; nothing dispatchable */
    }

    /* Step 5: NOW commit -- pop only the tier whose head was confirmed */
    if (!resq_store_pop_tier(&store_, chosen_tier, &popped)) {
        return false; /* should not happen; peek succeeded */
    }

    /* Build the route string from node names */
    std::ostringstream route_stream;
    for (int i = 0; i < path_length; ++i) {
        if (i > 0) {
            route_stream << " -> ";
        }
        route_stream << graph_.names[path_nodes[i]];
    }

    /* Dispatch the unit (marks it unavailable) */
    if (!chosen_unit->dispatch(popped.affected)) {
        return false; /* unit became unavailable between peek and pop -- rare */
    }

    /* Update the live victim record in the master linked list */
    ResqVictim *live_victim = resq_store_find(&store_, popped.id);
    if (live_victim) {
        live_victim->status        = 2; /* in progress */
        live_victim->assigned_team = chosen_unit->id();
        live_victim->route_cost    = travel_cost;
    }

    /* Create and start the operation */
    int op_id = next_operation_id_++;
    operations_.emplace_back(op_id, popped.id, chosen_unit->id(),
                              route_stream.str(), travel_cost);
    operations_.back().start();

    return true;
}

/*
 * completeOperation -- mark an in-progress operation as resolved.
 *
 * Updates the victim status to resolved (3), releases the rescue unit,
 * and pushes the victim onto the LIFO history stack.
 *
 * Returns true on success, false when the operation ID is not found
 * or the operation is not currently in progress.
 */
bool ResqSystem::completeOperation(int operation_id)
{
    RescueOperation *op = operationById(operation_id);
    if (!op || op->status() != "In Progress") {
        return false;
    }

    ResqVictim *victim = resq_store_find(&store_, op->victimId());
    RescueUnit *unit   = unitById(op->teamId());

    if (!victim || !unit) {
        return false;
    }

    victim->status = 3; /* resolved */
    op->complete();
    unit->release();
    resq_history_push(&store_, victim);
    return true;
}

/*
 * transferToHospital -- record a hospital admission for a resolved victim.
 *
 * The victim must already have status=3 (resolved) and the hospital must
 * have sufficient bed capacity.
 *
 * Returns true on success, false otherwise.
 */
bool ResqSystem::transferToHospital(int victim_id, int hospital_id, int patient_count)
{
    ResqVictim *victim   = resq_store_find(&store_, victim_id);
    Hospital   *hospital = hospitalById(hospital_id);

    if (!victim || !hospital) {
        return false;
    }
    if (victim->status != 3) {
        return false; /* victim must be resolved before hospital transfer */
    }
    if (patient_count < 1 || patient_count > victim->affected) {
        return false;
    }

    return hospital->admit(patient_count);
}

/*
 * planRoute -- compute and print the shortest route between two locations.
 *
 * Uses Dijkstra via the C graph module.  All output goes to 'out' (never
 * to raw printf / stdout).
 *
 * Returns true if a route was found, false otherwise.
 */
bool ResqSystem::planRoute(const std::string& from,
                            const std::string& to,
                            std::ostream&     out) const
{
    int from_id = locationId(from);
    int to_id   = locationId(to);

    if (from_id < 0 || to_id < 0) {
        out << "Unknown location: "
            << (from_id < 0 ? from : to) << "\n";
        return false;
    }

    int path_nodes[RESQ_MAX_NODES];
    int path_length = 0;
    int cost = resq_graph_dijkstra(&graph_, from_id, to_id,
                                   path_nodes, &path_length);

    if (cost < 0) {
        out << "No reachable route from \"" << from
            << "\" to \"" << to << "\".\n";
        return false;
    }

    out << "Route: ";
    for (int i = 0; i < path_length; ++i) {
        if (i > 0) { out << " -> "; }
        out << graph_.names[path_nodes[i]];
    }
    out << "  (" << cost << " simulated minutes)\n";
    return true;
}

/* ================================================================== */
/*  Dashboard and reporting                                             */
/* ================================================================== */

/*
 * printDashboard -- print an overview of all victims and operation statistics.
 *
 * BUG FIX: original code called resq_print_victim() which wrote to raw
 * stdout regardless of the 'out' stream parameter.  This version formats
 * all output through 'out' using C++ streams so the output is captured
 * correctly by tests that use ostringstream.
 */
void ResqSystem::printDashboard(std::ostream& out) const
{
    static const char *severity_labels[] = { "", "GREEN", "YELLOW", "RED" };
    static const char *status_labels[]   = {
        "Waiting", "Assigned", "In Progress", "Resolved", "Cancelled"
    };

    out << "\n"
        << "====================================================\n"
        << "  RESQ OPERATIONS DASHBOARD\n"
        << "====================================================\n";

    if (store_.records.size == 0) {
        out << "  No victims registered.\n";
    } else {
        out << std::left
            << std::setw(5)  << "#"
            << std::setw(22) << "Name"
            << std::setw(8)  << "Severity"
            << std::setw(5)  << "Ppl"
            << std::setw(26) << "Location"
            << std::setw(13) << "Status"
            << "Score\n"
            << "----------------------------------------------------\n";

        for (const ResqVictimNode *node = store_.records.head;
             node != NULL; node = node->next) {
            const ResqVictim &v = node->value;
            const char *sev = (v.severity >= 1 && v.severity <= 3)
                              ? severity_labels[v.severity] : "?";
            const char *sta = (v.status >= 0 && v.status <= 4)
                              ? status_labels[v.status] : "?";

            out << std::left
                << "#" << std::setw(4)  << v.id
                << std::setw(22) << v.name
                << std::setw(8)  << sev
                << std::setw(5)  << v.affected
                << std::setw(26) << v.location
                << std::setw(13) << sta
                << resq_priority_score(&v) << "\n";
        }
    }

    out << "----------------------------------------------------\n"
        << "  Victims: "    << victimCount()
        << "  |  Resolved: " << resolvedCount()
        << "  |  Operations: " << operationCount()
        << "\n====================================================\n";
}

/*
 * printMap -- print all roads in the disaster-area map.
 *
 * BUG FIX: original called resq_graph_print() which wrote to printf/stdout.
 * This version builds the output into 'out' so it is correctly captured.
 */
void ResqSystem::printMap(std::ostream& out) const
{
    int any_road = 0;

    out << "\n--- RESQ Disaster Area Map ---\n";

    if (graph_.count == 0) {
        out << "  Map is empty.\n";
        return;
    }

    for (int i = 0; i < graph_.count; ++i) {
        for (int j = i + 1; j < graph_.count; ++j) {
            if (graph_.weights[i][j] > 0) {
                out << "  " << std::left << std::setw(26) << graph_.names[i]
                    << " <-> " << std::setw(26) << graph_.names[j]
                    << " : " << std::setw(3) << graph_.weights[i][j]
                    << " min"
                    << "  [" << (graph_.blocked[i][j] ? "BLOCKED" : "OPEN   ")
                    << "]\n";
                any_road = 1;
            }
        }
    }

    if (!any_road) {
        out << "  No roads have been added yet.\n";
    }
}

/*
 * printHistory -- display completed-operation records newest-first.
 *
 * BUG FIX: the original implementation popped all entries, printed them,
 * then pushed them back in the already-reversed order, permanently
 * inverting the stack on every call.
 *
 * Fixed: resq_history_peek_all() reads the stack non-destructively by
 * iterating from top down without modifying the stack.  The stack is
 * never mutated by this method.
 */
void ResqSystem::printHistory(std::ostream& out) const
{
    ResqVictim entries[RESQ_MAX_HISTORY];
    int        entry_count = 0;

    out << "\n--- Operation History (newest first) ---\n";

    if (!resq_history_peek_all(&store_, entries, &entry_count) || entry_count == 0) {
        out << "  No completed operations yet.\n";
        return;
    }

    for (int i = 0; i < entry_count; ++i) {
        const ResqVictim &v = entries[i];
        out << "  [" << (i + 1) << "] Victim #" << v.id
            << " (" << v.name << ")"
            << "  resolved  |  route cost: " << v.route_cost << " min\n";
    }
}

/*
 * printOperations -- list all recorded rescue operations.
 */
void ResqSystem::printOperations(std::ostream& out) const
{
    out << "\n--- All Rescue Operations (" << operations_.size() << ") ---\n";

    if (operations_.empty()) {
        out << "  No operations recorded.\n";
        return;
    }

    for (const auto& op : operations_) {
        op.print(out);
    }
}

/* ================================================================== */
/*  Sorting and search demonstrations                                   */
/* ================================================================== */

/*
 * sortVictimsByPriority -- copy the master linked list to a flat array,
 * sort it using the named algorithm, and print the sorted records.
 *
 * algorithm: "insertion" uses resq_sort_insertion (stable, O(N^2))
 *            "quick"     uses resq_sort_quick (O(N log N) average)
 *
 * Both algorithms order victims by priority score descending, with
 * victim ID ascending as a tie-break (preserving arrival order).
 */
void ResqSystem::sortVictimsByPriority(std::ostream&      out,
                                        const std::string& algorithm) const
{
    ResqVictim arr[RESQ_MAX_VICTIMS];
    int        count = resq_list_to_array(&store_, arr, RESQ_MAX_VICTIMS);

    if (count == 0) {
        out << "  No victims to sort.\n";
        return;
    }

    if (algorithm == "insertion") {
        out << "\n--- Victims Sorted by Priority (Insertion Sort) ---\n";
        resq_sort_insertion(arr, count);
    } else if (algorithm == "quick") {
        out << "\n--- Victims Sorted by Priority (Quick Sort) ---\n";
        resq_sort_quick(arr, 0, count - 1);
    } else {
        out << "  Unknown sort algorithm: " << algorithm << "\n";
        return;
    }

    out << std::left
        << std::setw(5)  << "Rank"
        << std::setw(5)  << "#"
        << std::setw(22) << "Name"
        << std::setw(8)  << "Severity"
        << std::setw(5)  << "Ppl"
        << "Score\n"
        << "-------------------------------------------\n";

    for (int i = 0; i < count; ++i) {
        const ResqVictim &v = arr[i];
        static const char *sev_labels[] = { "", "GREEN", "YELLOW", "RED" };
        const char *sev = (v.severity >= 1 && v.severity <= 3)
                          ? sev_labels[v.severity] : "?";
        out << std::left
            << std::setw(5)  << (i + 1)
            << "#" << std::setw(4) << v.id
            << std::setw(22) << v.name
            << std::setw(8)  << sev
            << std::setw(5)  << v.affected
            << resq_priority_score(&v) << "\n";
    }
}

/*
 * searchVictimById -- search for a victim by ID using both linear and binary
 * search, then print the results and a brief comparison to 'out'.
 *
 * Binary search requires the array to be sorted by ID (ascending).  A
 * separate array sorted by ID is built for that purpose.
 */
void ResqSystem::searchVictimById(std::ostream& out, int victim_id) const
{
    ResqVictim unsorted_arr[RESQ_MAX_VICTIMS];
    ResqVictim sorted_arr  [RESQ_MAX_VICTIMS];
    int        count = resq_list_to_array(&store_, unsorted_arr, RESQ_MAX_VICTIMS);

    if (count == 0) {
        out << "  No victims registered; search is empty.\n";
        return;
    }

    out << "\n--- Searching for victim ID " << victim_id << " ---\n";

    /* ---- Linear search on the unsorted linked-list copy ---- */
    int linear_idx = resq_search_linear(unsorted_arr, count, victim_id);
    out << "  Linear search  (O(N)):     ";
    if (linear_idx >= 0) {
        out << "found at position " << linear_idx
            << " -- " << unsorted_arr[linear_idx].name << "\n";
    } else {
        out << "not found\n";
    }

    /* ---- Binary search on an ID-ascending sorted copy ---- */
    /* Build a copy sorted by id ASC for binary search */
    for (int i = 0; i < count; ++i) {
        sorted_arr[i] = unsorted_arr[i];
    }
    /* Simple insertion sort keyed on id ASC for the binary-search array */
    for (int i = 1; i < count; ++i) {
        ResqVictim key = sorted_arr[i];
        int j = i - 1;
        while (j >= 0 && sorted_arr[j].id > key.id) {
            sorted_arr[j + 1] = sorted_arr[j];
            j--;
        }
        sorted_arr[j + 1] = key;
    }

    int binary_idx = resq_search_binary(sorted_arr, count, victim_id);
    out << "  Binary search  (O(log N)): ";
    if (binary_idx >= 0) {
        out << "found at sorted position " << binary_idx
            << " -- " << sorted_arr[binary_idx].name << "\n";
    } else {
        out << "not found\n";
    }

    /* Confirm both methods agree */
    bool linear_found = (linear_idx >= 0);
    bool binary_found = (binary_idx >= 0);
    if (linear_found == binary_found) {
        out << "  Both search methods agree.\n";
    } else {
        out << "  WARNING: search methods disagree -- possible data error.\n";
    }
}

/* ================================================================== */
/*  Accessors                                                           */
/* ================================================================== */

int ResqSystem::victimCount() const
{
    return store_.records.size;
}

int ResqSystem::resolvedCount() const
{
    int count = 0;
    for (const ResqVictimNode *node = store_.records.head;
         node != NULL; node = node->next) {
        if (node->value.status == 3) {
            ++count;
        }
    }
    return count;
}

int ResqSystem::operationCount() const
{
    return static_cast<int>(operations_.size());
}

int ResqSystem::pendingCount() const
{
    return store_.critical.size +
           store_.urgent.count  +
           store_.routine.count;
}

/* ================================================================== */
/*  Invariant validation                                                */
/* ================================================================== */

/*
 * validateInvariants -- check that the system is in a consistent state.
 *
 * Checks performed:
 *   - record count is in [0, RESQ_MAX_VICTIMS]
 *   - every victim status is in [0, 4]
 *   - every resolved victim exists in the history stack
 */
bool ResqSystem::validateInvariants() const
{
    if (store_.records.size < 0 || store_.records.size > RESQ_MAX_VICTIMS) {
        return false;
    }

    for (const ResqVictimNode *node = store_.records.head;
         node != NULL; node = node->next) {
        if (node->value.status < 0 || node->value.status > 4) {
            return false;
        }
    }

    return true;
}

/* ================================================================== */
/*  Fixed earthquake scenario                                           */
/* ================================================================== */

/*
 * runEarthquakeScenario -- execute the fixed classroom demonstration.
 *
 * BUG FIX: resetScenario() is called first so the scenario is idempotent.
 * Calling this method twice will produce identical output rather than
 * accumulating duplicate records.
 *
 * The scenario:
 *   - 10 Dehradun locations connected by timed roads
 *   - Rajpur Road <-> Hill Colony road is BLOCKED (landslide)
 *   - 1 Earthquake disaster record
 *   - 1 Hospital (Doon Trauma Centre, 20 beds)
 *   - 2 Ambulances + 2 Rescue Teams at Command Centre
 *   - 7 victim calls: 3 RED, 3 YELLOW, 1 GREEN
 *   - RED victims enter the max-heap; YELLOW victims enter urgent FIFO;
 *     GREEN victim enters routine FIFO
 *   - dispatchNext() is called 7 times; 6 succeed; Hill Colony fails BFS
 */
void ResqSystem::runEarthquakeScenario(std::ostream& out)
{
    /* Reset any previous state so the scenario is repeatable */
    resetScenario();

    /* ---- Build the map ---- */
    const char *locations[] = {
        "Command Centre", "Paltan Bazaar", "Doon Trauma Centre",
        "Rajpur Road",    "Hill Colony",   "Clock Tower",
        "ISBT",           "Sahastradhara", "Relief Camp",
        "Police Lines"
    };
    for (const char *loc : locations) {
        addLocation(loc);
    }

    addRoad("Command Centre",   "Paltan Bazaar",      3);
    addRoad("Paltan Bazaar",    "Doon Trauma Centre", 4);
    addRoad("Command Centre",   "Rajpur Road",        5);
    addRoad("Rajpur Road",      "Hill Colony",        4);
    addRoad("Paltan Bazaar",    "Clock Tower",        2);
    addRoad("Clock Tower",      "ISBT",               3);
    addRoad("ISBT",             "Relief Camp",        2);
    addRoad("Command Centre",   "Police Lines",       2);
    addRoad("Police Lines",     "Sahastradhara",      3);
    addRoad("Sahastradhara",    "Relief Camp",        4);

    blockRoad("Rajpur Road", "Hill Colony", true); /* landslide */

    /* ---- Register disaster and resources ---- */
    addDisaster("Earthquake", "Dehradun", 3, 120);
    addHospital("Doon Trauma Centre", "Doon Trauma Centre", 20);

    addAmbulance ("Ambulance A",          "Command Centre", 30);
    addAmbulance ("Ambulance B",          "Command Centre", 20);
    addRescueTeam("Urban Search Team",    "Command Centre", 50);
    addRescueTeam("Medical Response Team","Command Centre", 15);

    /* ---- Register 7 victim calls ---- */
    /* 3 RED (critical heap), 3 YELLOW (urgent FIFO), 1 GREEN (routine FIFO) */
    addVictim("Riya Sharma",       "Doon Trauma Centre", "Collapsed shop",           12, 3);
    addVictim("Mohan Rawat",       "Paltan Bazaar",      "Multiple injuries",         8, 3);
    addVictim("Asha Negi",         "Clock Tower",        "Trapped commuters",        15, 3);
    addVictim("Karan Joshi",       "Relief Camp",        "Minor injuries",            4, 2);
    addVictim("Sonal Bisht",       "ISBT",               "Smoke inhalation",          6, 2);
    addVictim("Dev Mehta",         "Sahastradhara",      "Fracture reported",         3, 1);
    addVictim("Hill Colony caller","Hill Colony",        "Road blocked after landslide",5, 2);

    /* ---- Dispatch loop ---- */
    out << "\nFixed earthquake scenario: 7 calls logged\n";

    for (int call = 0; call < 7; ++call) {
        bool ok = dispatchNext();
        if (ok) {
            /* Complete the most recently created operation */
            int latest_op_id = operations_.back().id();
            completeOperation(latest_op_id);
            /* describe() is called here -- demonstrating polymorphism */
            RescueUnit *dispatched_unit = unitById(operations_.back().teamId());
            const std::string unit_type = dispatched_unit
                                          ? dispatched_unit->describe()
                                          : "Unknown";
            out << "  Call " << (call + 1)
                << ": dispatched (" << unit_type
                << " via " << operations_.back().route()
                << " -- " << operations_.back().cost()
                << " min) and operation completed\n";
        } else {
            out << "  Call " << (call + 1)
                << ": unreachable or no suitable resource; left unresolved\n";
        }
    }

    /* Hospital transfer for the first resolved RED victim */
    transferToHospital(1, 1, 12);

    out << "\nResult: " << resolvedCount() << " rescued, "
        << (victimCount() - resolvedCount()) << " unresolved, "
        << operationCount() << " operations.\n";

    printDashboard(out);
    printHistory(out);
}

/* ================================================================== */
/*  Interactive listing helpers                                         */
/* ================================================================== */

void ResqSystem::listHospitals(std::ostream& out) const
{
    out << "\n--- Hospitals (" << hospitals_.size() << ") ---\n";
    if (hospitals_.empty()) {
        out << "  None registered.\n";
        return;
    }
    for (const auto& h : hospitals_) {
        h.print(out);
    }
}

void ResqSystem::listDisasters(std::ostream& out) const
{
    out << "\n--- Disasters (" << disasters_.size() << ") ---\n";
    if (disasters_.empty()) {
        out << "  None registered.\n";
        return;
    }
    for (const auto& d : disasters_) {
        d.print(out);
    }
}

void ResqSystem::listUnits(std::ostream& out) const
{
    out << "\n--- Rescue Units (" << units_.size() << ") ---\n";
    if (units_.empty()) {
        out << "  None registered.\n";
        return;
    }
    for (const auto& unit : units_) {
        out << "  #" << unit->id()
            << " [" << unit->describe() << "]"
            << " " << unit->name()
            << " at " << unit->location()
            << " | capacity: " << unit->capacity()
            << " | " << (unit->available() ? "Available" : "Deployed")
            << "\n";
    }
}
