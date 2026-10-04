/* ============================================================
   RESQ - Intelligent Disaster Rescue & Emergency Response Simulator
   include/resq.hpp  -  C++ OOP layer

   Demonstrates: Classes & Objects, Encapsulation, Inheritance,
   Polymorphism (virtual functions), Constructors & Destructors,
   Operator overloading and Static members.
   The heavy lifting (queues, heap, graph, Dijkstra) is delegated
   to the pure-C DSA layer declared in resq_c.h.
   ============================================================ */
#ifndef RESQ_HPP
#define RESQ_HPP

#include <string>
#include <vector>
#include <iostream>
#include "resq_c.h"

namespace resq {

/* ------------------------------- Location ------------------------------- */
class Location {
    int         id_;
    std::string name_;
public:
    Location() : id_(-1), name_("unknown") {}
    Location(int id, const std::string &name) : id_(id), name_(name) {}
    int                id()   const { return id_; }
    const std::string &name() const { return name_; }
    bool operator==(const Location &o) const { return id_ == o.id_; }
};

/* -------------------------------- Victim -------------------------------- */
class Victim {
    int         id_;
    std::string name_;
    int         age_;
    int         severity_;      /* 1..10 */
    Location    location_;
    bool        rescued_;
public:
    Victim();
    Victim(int id, const std::string &name, int age, int severity, const Location &loc);
    ~Victim() = default;

    int                id()       const { return id_; }
    const std::string &name()     const { return name_; }
    int                age()      const { return age_; }
    int                severity() const { return severity_; }
    const Location    &location() const { return location_; }
    bool               rescued()  const { return rescued_; }

    void setSeverity(int s);
    void markRescued() { rescued_ = true; }

    bool isCritical() const { return severity_ >= 7; }
    std::string triageTag() const;              /* RED / YELLOW / GREEN */

    VictimRecord toRecord() const;              /* bridge to the C layer */
};

/* --------------------------- RescueUnit (base) --------------------------- */
/* Abstract base -> Inheritance + Polymorphism for every responder type.    */
class RescueUnit {
protected:
    int         id_;
    std::string callSign_;
    Location    base_;
    int         speedFactor_;       /* percent of nominal road speed */
    bool        busy_;
    static int  unitCount_;         /* static member: total units created */
public:
    RescueUnit(int id, const std::string &callSign, const Location &base, int speedFactor);
    virtual ~RescueUnit();

    int                id()       const { return id_; }
    const std::string &callSign() const { return callSign_; }
    const Location    &base()     const { return base_; }
    bool               busy()     const { return busy_; }
    void               setBusy(bool b)  { busy_ = b; }

    /* travel time in minutes for a route of `roadMinutes` nominal minutes */
    virtual int travelTime(int roadMinutes) const;

    virtual std::string type()      const = 0;   /* pure virtual */
    virtual std::string capability() const = 0;
    virtual void describe() const;               /* overridden below */

    static int unitCount() { return unitCount_; }
};

/* ------------------------------ RescueTeam ------------------------------ */
class RescueTeam : public RescueUnit {
    int members_;
    bool heavyEquipment_;
public:
    RescueTeam(int id, const std::string &cs, const Location &base,
               int members, bool heavyEquipment);
    std::string type()       const override { return "RescueTeam"; }
    std::string capability() const override;
    void        describe()   const override;
    int         travelTime(int roadMinutes) const override;
};

/* ------------------------------- Ambulance ------------------------------- */
class Ambulance : public RescueUnit {
    bool advancedLifeSupport_;
    int  hospitalId_;
public:
    Ambulance(int id, const std::string &cs, const Location &base,
              bool als, int hospitalId);
    int         hospitalId() const { return hospitalId_; }
    std::string type()       const override { return "Ambulance"; }
    std::string capability() const override;
    void        describe()   const override;
    int         travelTime(int roadMinutes) const override;
};

/* -------------------------------- Hospital ------------------------------- */
class Hospital {
    int         id_;
    std::string name_;
    Location    location_;
    int         totalBeds_;
    int         freeBeds_;
    bool        trauma_;
public:
    Hospital(int id, const std::string &name, const Location &loc,
             int beds, bool trauma);
    int                id()        const { return id_; }
    const std::string &name()      const { return name_; }
    const Location    &location()  const { return location_; }
    int                freeBeds()  const { return freeBeds_; }
    int                totalBeds() const { return totalBeds_; }
    bool               isTrauma()  const { return trauma_; }
    bool admit(const Victim &v);
    void discharge();
};

/* -------------------------------- Disaster ------------------------------- */
class Disaster {
    std::string type_;          /* Earthquake / Flood / Fire / Accident */
    std::string zone_;
    int         magnitude_;     /* 1..10 */
public:
    Disaster(const std::string &type, const std::string &zone, int magnitude);
    const std::string &type()      const { return type_; }
    const std::string &zone()      const { return zone_; }
    int                magnitude() const { return magnitude_; }
    void banner() const;
};

/* ---------------------------- RescueOperation ---------------------------- */
/* The simulation engine: owns the map, the victims, the units and every
   C data structure used to schedule the rescue.                            */
class RescueOperation {
    Disaster                disaster_;
    Graph                   map_{};
    VictimList              records_{};       /* linked list  */
    ReqQueue                normalQueue_{};   /* queue        */
    PriorityQueue           criticalQueue_{}; /* priority q   */
    OpStack                 history_{};       /* stack        */

    std::vector<Location>   locations_;
    std::vector<Victim>     victims_;
    std::vector<RescueUnit*> units_;          /* polymorphic  */
    std::vector<Hospital>   hospitals_;

    int  totalMinutes_   = 0;
    int  rescuedCount_   = 0;
    int  unreachable_    = 0;

    Victim     *findVictim(int id);
    RescueUnit *bestUnitFor(const Victim &v, int &etaOut, std::vector<int> &pathOut);
    Hospital   *nearestHospital(int fromNode, bool needTrauma, int &etaOut, std::vector<int> &pathOut);
    void        log(const std::string &text);
    std::string routeToString(const std::vector<int> &path) const;

public:
    explicit RescueOperation(const Disaster &d);
    ~RescueOperation();                        /* frees units + linked list */

    /* --- map ------------------------------------------------------- */
    void addLocation(int id, const std::string &name);
    void addRoad(int a, int b, int minutes);
    void blockRoad(int a, int b);

    /* --- resources -------------------------------------------------- */
    void addUnit(RescueUnit *u);               /* takes ownership */
    void addHospital(const Hospital &h);
    void showResources() const;            /* polymorphic describe() */

    /* --- incoming calls --------------------------------------------- */
    void reportVictim(const Victim &v);        /* routes into queue or heap */

    /* --- simulation -------------------------------------------------- */
    void showMapExploration(int commandCentre);   /* BFS  */
    void showRouteTable(int commandCentre);       /* Dijkstra */
    void runDispatch();                           /* main loop */
    void showTriageTable();                       /* sorting + searching */
    void showHistory();                           /* stack unwind */
    void showSummary();
};

} /* namespace resq */
#endif /* RESQ_HPP */
