/*
 * resq.hpp  --  RESQ Phase III: C++ Domain Model and System Controller API
 *
 * Team member : Vanya Rathi (Team Lead, DS-2)
 * Responsibility: Architecture, triage logic, integration and documentation.
 *
 * This header defines:
 *   - Five domain-model classes modelling the rescue world:
 *       Location, RescueUnit (abstract base), RescueTeam, Ambulance,
 *       Hospital, Disaster, RescueOperation
 *   - ResqSystem: the central controller that integrates the C data-structure
 *     layer (ResqDataStore, ResqGraph) with the C++ domain objects.
 *
 * Design principles:
 *   - All class method bodies are in ResourceModel.cpp (Nitin Singh).
 *   - All ResqSystem method bodies are in SystemController.cpp (Vanya Rathi).
 *   - The C layer (resq.h / dsa.c / graph.c) is included via extern "C" linkage.
 *   - Polymorphism: RescueUnit is abstract; RescueTeam and Ambulance each
 *     override describe() with their own type description; this virtual method
 *     is called in dispatch confirmation output.
 */

#ifndef RESQ_PHASE3_CPP_H
#define RESQ_PHASE3_CPP_H

#include "resq.h"

#include <memory>
#include <ostream>
#include <string>
#include <vector>

/* ================================================================== */
/*  Location                                                            */
/* ================================================================== */

/*
 * Location -- a named geographical point on the rescue map.
 * Locations are stored in the graph layer by name; this class acts as
 * a lightweight domain wrapper used in reports and confirmation output.
 */
class Location {
private:
    int         id_;
    std::string name_;

public:
    Location(int id, std::string name);

    int                id()   const;
    const std::string& name() const;
};

/* ================================================================== */
/*  RescueUnit (abstract base)                                         */
/* ================================================================== */

/*
 * RescueUnit -- abstract base class for all rescue resources.
 *
 * Derived classes (RescueTeam, Ambulance) must implement describe() so
 * that dispatch confirmation messages identify the type of unit sent.
 *
 * dispatch() marks the unit unavailable and succeeds only when:
 *   - the unit is currently available
 *   - the number of people is in [1, capacity]
 *
 * release() marks the unit available again after the operation completes.
 */
class RescueUnit {
protected:
    int         id_;
    std::string name_;
    std::string location_;
    int         capacity_;
    bool        available_;

    RescueUnit(int id, std::string name, std::string location, int capacity);

public:
    virtual ~RescueUnit() = default;

    int                id()        const;
    const std::string& name()      const;
    const std::string& location()  const;
    int                capacity()  const;
    bool               available() const;

    /* Pure virtual -- each subtype provides its own type label */
    virtual std::string describe() const = 0;

    bool dispatch(int people);
    void release();
};

/* ================================================================== */
/*  RescueTeam                                                          */
/* ================================================================== */

/*
 * RescueTeam -- a ground-based urban search and rescue team.
 * Capacity represents the maximum number of people the team can assist
 * in one deployment.
 */
class RescueTeam : public RescueUnit {
public:
    RescueTeam(int id, std::string name, std::string location, int capacity);

    std::string describe() const override;
};

/* ================================================================== */
/*  Ambulance                                                           */
/* ================================================================== */

/*
 * Ambulance -- a medical response vehicle.
 * Capacity represents maximum patient load per trip.
 */
class Ambulance : public RescueUnit {
public:
    Ambulance(int id, std::string name, std::string location, int capacity);

    std::string describe() const override;
};

/* ================================================================== */
/*  Hospital                                                            */
/* ================================================================== */

/*
 * Hospital -- a medical facility with a finite number of beds.
 * admit() reduces available beds; discharge() frees them.
 * availableBeds() reports how many beds are still open.
 */
class Hospital {
private:
    int         id_;
    std::string name_;
    std::string location_;
    int         total_beds_;
    int         occupied_beds_;

public:
    Hospital(int id, std::string name, std::string location, int beds);

    int                id()            const;
    const std::string& name()          const;
    const std::string& location()      const;
    int                availableBeds() const;
    int                totalBeds()     const;

    bool admit(int patient_count);
    bool discharge(int patient_count);
    void print(std::ostream& out) const;
};

/* ================================================================== */
/*  Disaster                                                            */
/* ================================================================== */

/*
 * Disaster -- a registered disaster event that contextualises the scenario.
 * The severity field mirrors the ResqSeverity enum range (1-3).
 * Disaster objects are stored separately from individual victim records.
 */
class Disaster {
private:
    int         id_;
    std::string type_;
    std::string area_;
    int         severity_;
    int         affected_;

public:
    Disaster(int id, std::string type, std::string area, int severity, int affected);

    int                id()       const;
    const std::string& type()     const;
    const std::string& area()     const;
    int                severity() const;
    int                affected() const;
    void print(std::ostream& out) const;
};

/* ================================================================== */
/*  RescueOperation                                                     */
/* ================================================================== */

/*
 * RescueOperation -- records a single dispatch from a rescue unit to a victim.
 *
 * Lifecycle:
 *   "Assigned"    -- operation created, unit designated
 *   "In Progress" -- operation started (unit en route)
 *   "Resolved"    -- victim successfully rescued
 *   "Cancelled"   -- operation cancelled before completion
 *
 * The route string is a human-readable path such as
 * "Command Centre -> Paltan Bazaar -> Clock Tower".
 */
class RescueOperation {
private:
    int         id_;
    int         victim_id_;
    int         team_id_;
    int         route_cost_;
    std::string route_;
    std::string status_;

public:
    RescueOperation(int op_id, int victim_id, int team_id,
                    std::string route, int cost);

    int                id()         const;
    int                victimId()   const;
    int                teamId()     const;
    int                cost()       const;
    const std::string& route()      const;
    const std::string& status()     const;

    void start();
    void complete();
    void cancel();
    void print(std::ostream& out) const;
};

/* ================================================================== */
/*  ResqSystem -- central system controller                            */
/* ================================================================== */

/*
 * ResqSystem integrates the C data-structure layer with C++ domain objects.
 *
 * The class is non-copyable (copy constructor and assignment operator are
 * deleted) because ResqDataStore holds heap-allocated linked-list nodes that
 * would need deep-copy semantics.
 *
 * The earthquake scenario and all algorithmic demonstrations are exposed
 * via public methods called from main.cpp; no algorithm logic lives in main.
 */
class ResqSystem {
private:
    /* C data-structure layer (Akshat's + Ayush's modules) */
    ResqDataStore store_{};
    ResqGraph     graph_{};

    /* C++ domain objects */
    std::vector<std::unique_ptr<RescueUnit>> units_;
    std::vector<Hospital>                   hospitals_;
    std::vector<Disaster>                   disasters_;
    std::vector<RescueOperation>            operations_;

    /* ID counters for C++ objects */
    int next_unit_id_      = 1;
    int next_hospital_id_  = 1;
    int next_disaster_id_  = 1;
    int next_operation_id_ = 1;

    /* Private helpers */
    int              locationId (const std::string& name) const;
    RescueUnit      *unitById   (int id);
    Hospital        *hospitalById(int id);
    RescueOperation *operationById(int id);

    /* Reset scenario state so runEarthquakeScenario() is idempotent */
    void resetScenario();

public:
    ResqSystem();
    ~ResqSystem();

    ResqSystem(const ResqSystem&)            = delete;
    ResqSystem& operator=(const ResqSystem&) = delete;

    /* ---- Map management ---- */
    int  addLocation(const std::string& name);
    bool addRoad    (const std::string& from, const std::string& to, int cost);
    bool blockRoad  (const std::string& from, const std::string& to, bool blocked);

    /* ---- Victim management ---- */
    int addVictim(const std::string& name,
                  const std::string& location,
                  const std::string& description,
                  int affected,
                  int severity);  /* severity 1=GREEN 2=YELLOW 3=RED */

    /* ---- Resource management ---- */
    int addRescueTeam(const std::string& name, const std::string& location, int capacity);
    int addAmbulance (const std::string& name, const std::string& location, int capacity);
    int addHospital  (const std::string& name, const std::string& location, int beds);
    int addDisaster  (const std::string& type,  const std::string& area,
                      int severity, int affected);

    /* ---- Operations ---- */
    bool dispatchNext();                            /* dispatch next pending victim */
    bool completeOperation(int operation_id);       /* resolve an in-progress op   */
    bool transferToHospital(int victim_id,
                            int hospital_id,
                            int patient_count);     /* post-rescue hospital admit   */
    bool planRoute(const std::string& from,
                   const std::string& to,
                   std::ostream& out) const;

    /* ---- Dashboard and reporting ---- */
    void printDashboard(std::ostream& out) const;
    void printMap      (std::ostream& out) const;
    void printHistory  (std::ostream& out) const;   /* non-destructive stack read  */
    void printOperations(std::ostream& out) const;

    /* ---- Sorting and search demonstrations ---- */
    /*
     * sortVictimsByPriority -- copy the master list to a local array, sort it
     * using the requested algorithm, and print the sorted records.
     *   algorithm: "insertion" or "quick"
     */
    void sortVictimsByPriority(std::ostream& out,
                               const std::string& algorithm) const;

    /*
     * searchVictimById -- demonstrate linear and binary search for a victim ID.
     * Prints results and timing commentary to 'out'.
     */
    void searchVictimById(std::ostream& out, int victim_id) const;

    /* ---- Accessors used by tests ---- */
    int  victimCount()   const;
    int  resolvedCount() const;
    int  operationCount() const;
    int  pendingCount()  const; /* waiting in any queue  */

    /* ---- Invariant validation ---- */
    bool validateInvariants() const;

    /* ---- Fixed-scenario demonstration ---- */
    void runEarthquakeScenario(std::ostream& out);

    /* ---- Interactive helpers used by main.cpp ---- */
    void listHospitals (std::ostream& out) const;
    void listDisasters (std::ostream& out) const;
    void listUnits     (std::ostream& out) const;
};

#endif /* RESQ_PHASE3_CPP_H */
