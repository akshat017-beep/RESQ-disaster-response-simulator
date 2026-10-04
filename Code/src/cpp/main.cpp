/* ============================================================
   RESQ - Intelligent Disaster Rescue & Emergency Response Simulator
   src/cpp/main.cpp - scenario driver
   Team : Vanya Rathi (lead), Akshat Dobhal, Ayush Kandwal, Nitin Singh
   Mentor : Mr. Prabhdeep Singh
   ============================================================ */
#include <iostream>
#include "resq.hpp"

using namespace resq;

/* Location ids for the simulated disaster zone */
enum Node {
    CMD = 0, CLOCK_TOWER, RAJPUR_RD, SCHOOL, MARKET, BRIDGE,
    RIVERSIDE, HOSP_A, HOSP_B, HILL_COLONY, N_NODES
};

int main() {
    Disaster quake("Earthquake", "Dehradun - Central Sector", 7);
    quake.banner();

    RescueOperation op(quake);

    /* ---------------- 1. build the disaster-area map (Graph) ------------- */
    op.addLocation(CMD,         "Command Centre");
    op.addLocation(CLOCK_TOWER, "Clock Tower");
    op.addLocation(RAJPUR_RD,   "Rajpur Road");
    op.addLocation(SCHOOL,      "Govt School");
    op.addLocation(MARKET,      "Paltan Bazaar");
    op.addLocation(BRIDGE,      "Bindal Bridge");
    op.addLocation(RIVERSIDE,   "Riverside Slum");
    op.addLocation(HOSP_A,      "Doon Trauma Ctr");
    op.addLocation(HOSP_B,      "City Clinic");
    op.addLocation(HILL_COLONY, "Hill Colony");

    op.addRoad(CMD,         CLOCK_TOWER,  4);
    op.addRoad(CMD,         RAJPUR_RD,    7);
    op.addRoad(CLOCK_TOWER, MARKET,       3);
    op.addRoad(CLOCK_TOWER, SCHOOL,       6);
    op.addRoad(MARKET,      BRIDGE,       5);
    op.addRoad(RAJPUR_RD,   SCHOOL,       2);
    op.addRoad(SCHOOL,      HOSP_A,       4);
    op.addRoad(MARKET,      HOSP_B,       6);
    op.addRoad(BRIDGE,      RIVERSIDE,    3);
    op.addRoad(RIVERSIDE,   HOSP_B,       9);
    op.addRoad(BRIDGE,      HILL_COLONY, 11);

    /* the quake destroys one link - the map must adapt */
    std::cout << "\n-- Live map updates --\n";
    op.blockRoad(BRIDGE, HILL_COLONY);

    /* ---------------- 2. resources (OOP objects) ------------------------- */
    Location cmd(CMD, "Command Centre");
    Location hospA(HOSP_A, "Doon Trauma Ctr");
    Location mkt(MARKET, "Paltan Bazaar");

    op.addUnit(new RescueTeam(1, "ALPHA-1", cmd,   8, true));
    op.addUnit(new RescueTeam(2, "BRAVO-2", mkt,   5, false));
    op.addUnit(new Ambulance (3, "AMB-108", hospA, true,  1));
    op.addUnit(new Ambulance (4, "AMB-102", cmd,   false, 2));

    op.addHospital(Hospital(1, "Doon Trauma Ctr", hospA, 3, true));
    op.addHospital(Hospital(2, "City Clinic", Location(HOSP_B, "City Clinic"), 4, false));

    op.showResources();

    /* ---------------- 3. incoming emergency calls ------------------------ */
    std::cout << "\n-- Incoming emergency calls --\n";
    op.reportVictim(Victim(101, "Meera Joshi",  34, 9, Location(SCHOOL,      "Govt School")));
    op.reportVictim(Victim(102, "Rohit Negi",   12, 5, Location(MARKET,      "Paltan Bazaar")));
    op.reportVictim(Victim(103, "Anita Bisht",  67, 8, Location(RIVERSIDE,   "Riverside Slum")));
    op.reportVictim(Victim(104, "Karan Rawat",  25, 3, Location(CLOCK_TOWER, "Clock Tower")));
    op.reportVictim(Victim(105, "Sunil Thapa",  48, 7, Location(BRIDGE,      "Bindal Bridge")));
    op.reportVictim(Victim(106, "Priya Sharma", 19, 2, Location(RAJPUR_RD,   "Rajpur Road")));
    op.reportVictim(Victim(107, "Dev Uniyal",   55, 6, Location(HILL_COLONY, "Hill Colony")));

    /* ---------------- 4. algorithms in action ---------------------------- */
    op.showMapExploration(CMD);      /* BFS      */
    op.showRouteTable(CMD);          /* Dijkstra */
    op.runDispatch();                /* PQ + Queue + Dijkstra */
    op.showTriageTable();            /* sorting + searching   */
    op.showSummary();
    op.showHistory();                /* stack    */

    std::cout << "\nRESQ simulation complete.\n";
    return 0;
}
