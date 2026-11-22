/*
 * phase3_tests.cpp  --  RESQ Phase III: Integration and Unit Tests
 *
 * Team member : Nitin Singh (ML-1)
 * Responsibility: Scenario testing, boundary tests, algorithm validation.
 *
 * Tests are implemented as plain assert() calls so that the test binary
 * exits with code 0 on success and a non-zero abort on the first failure.
 * Each test block prints a label before execution so failures are easy to
 * locate in the console output.
 *
 * Test groups:
 *   T1. Empty-system invariants
 *   T2. Graph: add locations, roads, blocking, BFS, Dijkstra
 *   T3. Data store: victim add, find, enqueue, peek-without-pop, pop
 *   T4. Sorting: insertion sort and quick sort produce correct order
 *   T5. Search: linear and binary search agree and find / not-find correctly
 *   T6. Priority queue: RED before YELLOW before GREEN; FIFO within tier
 *   T7. dispatchNext() does not drop victims on failure (peek-before-pop)
 *   T8. printHistory() does not invert the stack (non-destructive read)
 *   T9. Scenario re-run is idempotent (resetScenario clears state)
 *   T10. Full earthquake scenario: 7 victims, 6 rescued, 1 unresolved
 */

#include "resq.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

/* Helper: print a test-group label */
static void begin_test(const char *label)
{
    std::cout << "[TEST] " << label << " ...\n";
}

/* Helper: print success for a test group */
static void pass_test(const char *label)
{
    std::cout << "       " << label << " PASSED\n";
}

/* ================================================================== */
/*  T1. Empty-system invariants                                        */
/* ================================================================== */
static void test_empty_system()
{
    begin_test("T1: Empty system invariants");

    ResqSystem sys;
    assert(sys.victimCount()    == 0);
    assert(sys.resolvedCount()  == 0);
    assert(sys.operationCount() == 0);
    assert(sys.pendingCount()   == 0);
    assert(sys.validateInvariants());

    /* dispatchNext on empty system must return false without crashing */
    assert(!sys.dispatchNext());

    pass_test("T1");
}

/* ================================================================== */
/*  T2. Graph operations                                               */
/* ================================================================== */
static void test_graph()
{
    begin_test("T2: Graph operations (Ayush Kandwal -- graph.c)");

    ResqGraph g{};
    resq_graph_init(&g);

    /* Add locations */
    assert(resq_graph_add_location(&g, "Alpha") == 0);
    assert(resq_graph_add_location(&g, "Beta")  == 1);
    assert(resq_graph_add_location(&g, "Gamma") == 2);

    /* Duplicate name must fail */
    assert(resq_graph_add_location(&g, "Alpha") == -1);

    /* Empty name must fail */
    assert(resq_graph_add_location(&g, "")  == -1);

    /* Add roads */
    assert(resq_graph_add_road(&g, 0, 1, 5));
    assert(resq_graph_add_road(&g, 1, 2, 3));

    /* BFS: 0 -> 2 reachable via 1 */
    assert(resq_graph_bfs_reachable(&g, 0, 2));

    /* Block the 0->1 road; now 0->2 is unreachable */
    assert(resq_graph_block_road(&g, 0, 1, 1));
    assert(!resq_graph_bfs_reachable(&g, 0, 2));

    /* Reopen; 0->2 reachable again */
    assert(resq_graph_block_road(&g, 0, 1, 0));
    assert(resq_graph_bfs_reachable(&g, 0, 2));

    /* Dijkstra: 0->2, expected cost 8 */
    int path[RESQ_MAX_NODES], len = 0;
    int cost = resq_graph_dijkstra(&g, 0, 2, path, &len);
    assert(cost == 8);
    assert(len  == 3);
    assert(path[0] == 0 && path[1] == 1 && path[2] == 2);

    /* Dijkstra on blocked path: should return -1 */
    resq_graph_block_road(&g, 0, 1, 1);
    assert(resq_graph_dijkstra(&g, 0, 2, path, &len) == -1);

    pass_test("T2");
}

/* ================================================================== */
/*  T3. Data store operations                                          */
/* ================================================================== */
static void test_data_store()
{
    begin_test("T3: Data store (Akshat Dobhal -- dsa.c)");

    ResqDataStore store{};
    resq_store_init(&store);

    /* Add a victim */
    ResqVictim v{};
    v.id       = 1;
    v.seq      = 0;
    v.severity = RESQ_RED;
    v.status   = 0;
    v.affected = 5;
    std::snprintf(v.name,     sizeof(v.name),     "Test Victim");
    std::snprintf(v.location, sizeof(v.location), "Test Location");
    std::snprintf(v.description, sizeof(v.description), "Test desc");

    assert(resq_store_add(&store, &v));
    assert(store.records.size == 1);

    /* Duplicate add must fail */
    assert(!resq_store_add(&store, &v));
    assert(store.records.size == 1);

    /* Find by ID */
    ResqVictim *found = resq_store_find(&store, 1);
    assert(found != NULL);
    assert(found->id == 1);

    /* Find non-existent */
    assert(resq_store_find(&store, 99) == NULL);

    /* Enqueue */
    assert(resq_store_enqueue(&store, &v));
    assert(store.critical.size == 1); /* RED goes to heap */

    /* Peek must succeed without removing */
    ResqVictim peeked{};
    assert(resq_store_peek_next(&store, &peeked));
    assert(peeked.id == 1);
    assert(store.critical.size == 1); /* still there */

    /* Pop must remove */
    ResqVictim popped{};
    assert(resq_store_pop_next(&store, &popped));
    assert(popped.id == 1);
    assert(store.critical.size == 0); /* now gone */

    /* Second pop must fail */
    assert(!resq_store_pop_next(&store, &popped));

    resq_store_clear(&store);
    assert(store.records.size == 0);

    pass_test("T3");
}

/* ================================================================== */
/*  T4. Sorting algorithms                                             */
/* ================================================================== */
static void test_sorting()
{
    begin_test("T4: Sorting algorithms -- insertion sort and quick sort (Akshat Dobhal -- dsa.c)");

    /* Build a small array */
    ResqVictim arr[5]{};

    /* arr[0]: YELLOW, 4 people */
    arr[0].id = 1; arr[0].severity = RESQ_YELLOW; arr[0].affected = 4; arr[0].status = 0;
    /* arr[1]: RED, 10 people */
    arr[1].id = 2; arr[1].severity = RESQ_RED;    arr[1].affected = 10; arr[1].status = 0;
    /* arr[2]: GREEN, 2 people */
    arr[2].id = 3; arr[2].severity = RESQ_GREEN;  arr[2].affected = 2;  arr[2].status = 0;
    /* arr[3]: RED, 5 people */
    arr[3].id = 4; arr[3].severity = RESQ_RED;    arr[3].affected = 5;  arr[3].status = 0;
    /* arr[4]: YELLOW, 8 people */
    arr[4].id = 5; arr[4].severity = RESQ_YELLOW; arr[4].affected = 8;  arr[4].status = 0;

    /* Copy for two separate sorts */
    ResqVictim ins_arr[5], qk_arr[5];
    for (int i = 0; i < 5; ++i) { ins_arr[i] = arr[i]; qk_arr[i] = arr[i]; }

    /* Insertion sort */
    resq_sort_insertion(ins_arr, 5);
    for (int i = 0; i < 4; ++i) {
        assert(resq_priority_score(&ins_arr[i]) >= resq_priority_score(&ins_arr[i + 1]));
    }

    /* Quick sort */
    resq_sort_quick(qk_arr, 0, 4);
    for (int i = 0; i < 4; ++i) {
        assert(resq_priority_score(&qk_arr[i]) >= resq_priority_score(&qk_arr[i + 1]));
    }

    /* Both produce the same ordering */
    for (int i = 0; i < 5; ++i) {
        assert(resq_priority_score(&ins_arr[i]) == resq_priority_score(&qk_arr[i]));
    }

    /* Single-element -- must not crash */
    resq_sort_insertion(arr, 1);
    resq_sort_quick(arr, 0, 0);

    pass_test("T4");
}

/* ================================================================== */
/*  T5. Search algorithms                                              */
/* ================================================================== */
static void test_search()
{
    begin_test("T5: Search algorithms -- linear and binary (Akshat Dobhal -- dsa.c)");

    ResqVictim arr[4]{};
    for (int i = 0; i < 4; ++i) {
        arr[i].id = i + 10; /* IDs: 10, 11, 12, 13 */
    }

    /* Linear search -- found */
    assert(resq_search_linear(arr, 4, 12) == 2);
    /* Linear search -- not found */
    assert(resq_search_linear(arr, 4, 99) == -1);
    /* Linear search on empty */
    assert(resq_search_linear(arr, 0, 10) == -1);

    /* Binary search on sorted-by-id array -- found */
    assert(resq_search_binary(arr, 4, 10) == 0);
    assert(resq_search_binary(arr, 4, 13) == 3);
    /* Binary search -- not found */
    assert(resq_search_binary(arr, 4, 15) == -1);
    /* Binary search on empty */
    assert(resq_search_binary(arr, 0, 10) == -1);

    pass_test("T5");
}

/* ================================================================== */
/*  T6. Priority queue: tier ordering and FIFO within tier            */
/* ================================================================== */
static void test_priority_queue()
{
    begin_test("T6: Priority queue tier ordering and FIFO tie-breaking");

    ResqDataStore store{};
    resq_store_init(&store);

    /* Add GREEN, YELLOW, RED in that order; expect RED first, then YELLOW, GREEN */
    ResqVictim green{};  green.id  = 1; green.severity  = RESQ_GREEN;  green.seq  = 0; green.affected = 2; green.status = 0;
    ResqVictim yellow{}; yellow.id = 2; yellow.severity = RESQ_YELLOW; yellow.seq = 1; yellow.affected = 2; yellow.status = 0;
    ResqVictim red{};    red.id    = 3; red.severity    = RESQ_RED;    red.seq    = 2; red.affected = 2;    red.status = 0;

    std::snprintf(green.location,  sizeof(green.location),  "A");
    std::snprintf(yellow.location, sizeof(yellow.location), "B");
    std::snprintf(red.location,    sizeof(red.location),    "C");
    std::snprintf(green.description,  sizeof(green.description),  "d");
    std::snprintf(yellow.description, sizeof(yellow.description), "d");
    std::snprintf(red.description,    sizeof(red.description),    "d");
    std::snprintf(green.name,  sizeof(green.name),  "Green");
    std::snprintf(yellow.name, sizeof(yellow.name), "Yellow");
    std::snprintf(red.name,    sizeof(red.name),    "Red");

    assert(resq_store_enqueue(&store, &green));
    assert(resq_store_enqueue(&store, &yellow));
    assert(resq_store_enqueue(&store, &red));

    ResqVictim out{};
    /* First pop: RED (critical heap) */
    assert(resq_store_pop_next(&store, &out));
    assert(out.severity == RESQ_RED);

    /* Second pop: YELLOW (urgent FIFO) */
    assert(resq_store_pop_next(&store, &out));
    assert(out.severity == RESQ_YELLOW);

    /* Third pop: GREEN (routine FIFO) */
    assert(resq_store_pop_next(&store, &out));
    assert(out.severity == RESQ_GREEN);

    /* Queue now empty */
    assert(!resq_store_pop_next(&store, &out));

    /* FIFO within YELLOW tier: two YELLOWs, first registered comes first */
    ResqVictim y1{}, y2{};
    y1.id = 10; y1.severity = RESQ_YELLOW; y1.seq = 0; y1.affected = 2; y1.status = 0;
    y2.id = 11; y2.severity = RESQ_YELLOW; y2.seq = 1; y2.affected = 2; y2.status = 0;
    std::snprintf(y1.name, sizeof(y1.name), "Y1");
    std::snprintf(y2.name, sizeof(y2.name), "Y2");
    std::snprintf(y1.location, sizeof(y1.location), "X");
    std::snprintf(y2.location, sizeof(y2.location), "X");
    std::snprintf(y1.description, sizeof(y1.description), "d");
    std::snprintf(y2.description, sizeof(y2.description), "d");

    assert(resq_store_enqueue(&store, &y1));
    assert(resq_store_enqueue(&store, &y2));
    assert(resq_store_pop_next(&store, &out));
    assert(out.id == 10);  /* y1 registered first */
    assert(resq_store_pop_next(&store, &out));
    assert(out.id == 11);

    resq_store_clear(&store);
    pass_test("T6");
}

/* ================================================================== */
/*  T7. dispatchNext() does not drop victims on failure               */
/* ================================================================== */
static void test_dispatch_no_drop()
{
    begin_test("T7: dispatchNext() leaves victim in queue when dispatch fails");

    ResqSystem sys;

    /* Add a location and a road */
    sys.addLocation("Base");
    sys.addLocation("Site A");
    sys.addRoad("Base", "Site A", 5);

    /* Add a victim at a location that IS reachable */
    int vid = sys.addVictim("Alice", "Site A", "Test victim", 100, 3);
    assert(vid > 0);

    /* No rescue units registered -- dispatch will fail */
    assert(sys.pendingCount() == 1);  /* one victim waiting */
    bool ok = sys.dispatchNext();
    assert(!ok);                       /* dispatch fails */
    assert(sys.pendingCount() == 1);  /* victim STILL in queue */

    /* Now add a unit with insufficient capacity */
    sys.addAmbulance("Small Ambulance", "Base", 5); /* capacity 5; victim needs 100 */
    ok = sys.dispatchNext();
    assert(!ok);                       /* dispatch fails (capacity mismatch) */
    assert(sys.pendingCount() == 1);  /* victim still in queue */

    /* Add a unit with sufficient capacity; dispatch must succeed */
    sys.addAmbulance("Big Ambulance", "Base", 100);
    ok = sys.dispatchNext();
    assert(ok);
    assert(sys.pendingCount() == 0);  /* victim was popped only on success */

    pass_test("T7");
}

/* ================================================================== */
/*  T8. printHistory() does not invert the stack                      */
/* ================================================================== */
static void test_history_non_destructive()
{
    begin_test("T8: printHistory() is non-destructive (stack not inverted)");

    ResqSystem sys;

    /* Run the earthquake scenario to populate history */
    std::ostringstream dummy;
    sys.runEarthquakeScenario(dummy);

    /* Capture first print */
    std::ostringstream first;
    sys.printHistory(first);

    /* Capture second print -- must be identical if stack was not modified */
    std::ostringstream second;
    sys.printHistory(second);

    assert(first.str() == second.str());

    pass_test("T8");
}

/* ================================================================== */
/*  T9. Scenario re-run is idempotent                                 */
/* ================================================================== */
static void test_scenario_idempotent()
{
    begin_test("T9: runEarthquakeScenario() is idempotent across two runs");

    ResqSystem sys;

    std::ostringstream out1, out2;
    sys.runEarthquakeScenario(out1);

    /* Second run must reset all state and produce the same result counts */
    sys.runEarthquakeScenario(out2);

    /* Both runs should report 7 victims and 6 resolved */
    assert(sys.victimCount()   == 7);
    assert(sys.resolvedCount() == 6);
    assert(sys.operationCount() == 6);

    pass_test("T9");
}

/* ================================================================== */
/*  T10. Full earthquake scenario correctness                         */
/* ================================================================== */
static void test_full_earthquake_scenario()
{
    begin_test("T10: Full earthquake scenario (7 calls, 6 rescued, 1 unresolved)");

    ResqSystem sys;
    std::ostringstream output;
    sys.runEarthquakeScenario(output);

    std::string text = output.str();

    assert(sys.victimCount()    == 7);
    assert(sys.resolvedCount()  == 6);
    assert(sys.operationCount() == 6);

    /* Report contains the expected result summary */
    assert(text.find("6 rescued") != std::string::npos);
    assert(text.find("1 unresolved") != std::string::npos);

    /* Invariants hold after scenario */
    assert(sys.validateInvariants());

    /* Sorting runs without error after scenario */
    std::ostringstream sort_out;
    sys.sortVictimsByPriority(sort_out, "insertion");
    sys.sortVictimsByPriority(sort_out, "quick");
    assert(!sort_out.str().empty());

    /* Search exercises both paths */
    std::ostringstream search_out;
    sys.searchVictimById(search_out, 1); /* exists */
    sys.searchVictimById(search_out, 99); /* does not exist */
    assert(!search_out.str().empty());

    pass_test("T10");
}

/* ================================================================== */
/*  main -- run all test groups                                        */
/* ================================================================== */
int main()
{
    std::cout << "\n=== RESQ Phase III Test Suite ===\n\n";

    test_empty_system();
    test_graph();
    test_data_store();
    test_sorting();
    test_search();
    test_priority_queue();
    test_dispatch_no_drop();
    test_history_non_destructive();
    test_scenario_idempotent();
    test_full_earthquake_scenario();

    std::cout << "\n=== All 10 test groups PASSED ===\n\n";
    return 0;
}
