/*
 * ResourceModel.cpp  --  RESQ Phase III: C++ Resource Class Implementations
 *
 * Team member : Nitin Singh (ML-1)
 * Responsibility: C++ resource classes, scenario testing, dashboard and reports.
 *
 * This file implements all concrete method bodies for the domain model classes
 * declared in resq.hpp:
 *   - Location
 *   - RescueUnit (abstract base -- dispatch and release)
 *   - RescueTeam (describe, constructor)
 *   - Ambulance  (describe, constructor)
 *   - Hospital   (admit, discharge, availableBeds, print)
 *   - Disaster   (accessors, print)
 *   - RescueOperation (start, complete, cancel, print)
 *
 * Polymorphism is exercised through RescueUnit::describe(), which is a pure
 * virtual method in the base class and is overridden in RescueTeam and
 * Ambulance.  The SystemController calls describe() in dispatch confirmation
 * messages so the printed output correctly identifies the resource type.
 */

#include "resq.hpp"

#include <iostream>
#include <iomanip>
#include <stdexcept>

/* ================================================================== */
/*  Location                                                            */
/* ================================================================== */

Location::Location(int id, std::string name)
    : id_(id), name_(std::move(name))
{
}

int Location::id() const
{
    return id_;
}

const std::string& Location::name() const
{
    return name_;
}

/* ================================================================== */
/*  RescueUnit (abstract base)                                         */
/* ================================================================== */

RescueUnit::RescueUnit(int id, std::string name, std::string location, int capacity)
    : id_(id),
      name_(std::move(name)),
      location_(std::move(location)),
      capacity_(capacity),
      available_(true)
{
}

int RescueUnit::id() const
{
    return id_;
}

const std::string& RescueUnit::name() const
{
    return name_;
}

const std::string& RescueUnit::location() const
{
    return location_;
}

int RescueUnit::capacity() const
{
    return capacity_;
}

bool RescueUnit::available() const
{
    return available_;
}

/*
 * dispatch -- attempt to deploy this unit to handle 'people' victims.
 *
 * Preconditions:
 *   - The unit must currently be available.
 *   - people must be in the range [1, capacity_].
 *
 * On success, the unit is marked unavailable and returns true.
 * Returns false if any precondition is violated.
 */
bool RescueUnit::dispatch(int people)
{
    if (!available_) {
        return false; /* already deployed */
    }
    if (people < 1 || people > capacity_) {
        return false; /* people count out of range */
    }

    available_ = false;
    return true;
}

/*
 * release -- mark the unit as available again after an operation completes.
 * Always succeeds; calling release() on an already-available unit is harmless.
 */
void RescueUnit::release()
{
    available_ = true;
}

/* ================================================================== */
/*  RescueTeam                                                          */
/* ================================================================== */

RescueTeam::RescueTeam(int id, std::string name, std::string location, int capacity)
    : RescueUnit(id, std::move(name), std::move(location), capacity)
{
}

/*
 * describe -- return a human-readable type label for dispatch confirmation.
 * Polymorphically called through a RescueUnit* pointer.
 */
std::string RescueTeam::describe() const
{
    return "Rescue Team";
}

/* ================================================================== */
/*  Ambulance                                                           */
/* ================================================================== */

Ambulance::Ambulance(int id, std::string name, std::string location, int capacity)
    : RescueUnit(id, std::move(name), std::move(location), capacity)
{
}

/*
 * describe -- overrides RescueUnit::describe() with the ambulance type label.
 */
std::string Ambulance::describe() const
{
    return "Ambulance";
}

/* ================================================================== */
/*  Hospital                                                            */
/* ================================================================== */

Hospital::Hospital(int id, std::string name, std::string location, int beds)
    : id_(id),
      name_(std::move(name)),
      location_(std::move(location)),
      total_beds_(beds),
      occupied_beds_(0)
{
}

int Hospital::id() const
{
    return id_;
}

const std::string& Hospital::name() const
{
    return name_;
}

const std::string& Hospital::location() const
{
    return location_;
}

int Hospital::availableBeds() const
{
    return total_beds_ - occupied_beds_;
}

int Hospital::totalBeds() const
{
    return total_beds_;
}

/*
 * admit -- reserve beds for 'patient_count' incoming patients.
 *
 * Returns true and increments occupied_beds_ on success.
 * Returns false when patient_count < 1 or exceeds available capacity.
 */
bool Hospital::admit(int patient_count)
{
    if (patient_count < 1 || patient_count > availableBeds()) {
        return false;
    }
    occupied_beds_ += patient_count;
    return true;
}

/*
 * discharge -- free beds when patients are discharged.
 *
 * Returns true and decrements occupied_beds_ on success.
 * Returns false when patient_count < 1 or exceeds currently occupied beds.
 */
bool Hospital::discharge(int patient_count)
{
    if (patient_count < 1 || patient_count > occupied_beds_) {
        return false;
    }
    occupied_beds_ -= patient_count;
    return true;
}

/*
 * print -- display hospital details on a single line to 'out'.
 */
void Hospital::print(std::ostream& out) const
{
    out << "  Hospital #" << id_
        << " | " << name_
        << " | at " << location_
        << " | beds " << availableBeds() << "/" << total_beds_
        << "\n";
}

/* ================================================================== */
/*  Disaster                                                            */
/* ================================================================== */

Disaster::Disaster(int id, std::string type, std::string area,
                   int severity, int affected)
    : id_(id),
      type_(std::move(type)),
      area_(std::move(area)),
      severity_(severity),
      affected_(affected)
{
}

int Disaster::id() const
{
    return id_;
}

const std::string& Disaster::type() const
{
    return type_;
}

const std::string& Disaster::area() const
{
    return area_;
}

int Disaster::severity() const
{
    return severity_;
}

int Disaster::affected() const
{
    return affected_;
}

/*
 * print -- display disaster details to 'out'.
 */
void Disaster::print(std::ostream& out) const
{
    static const char *severity_labels[] = { "", "GREEN", "YELLOW", "RED" };
    const char *label = (severity_ >= 1 && severity_ <= 3)
                        ? severity_labels[severity_] : "UNKNOWN";

    out << "  Disaster #" << id_
        << " | " << type_
        << " | area: " << area_
        << " | severity: " << label
        << " | estimated affected: " << affected_
        << "\n";
}

/* ================================================================== */
/*  RescueOperation                                                     */
/* ================================================================== */

RescueOperation::RescueOperation(int op_id, int victim_id, int team_id,
                                 std::string route, int cost)
    : id_(op_id),
      victim_id_(victim_id),
      team_id_(team_id),
      route_cost_(cost),
      route_(std::move(route)),
      status_("Assigned")
{
}

int RescueOperation::id() const
{
    return id_;
}

int RescueOperation::victimId() const
{
    return victim_id_;
}

int RescueOperation::teamId() const
{
    return team_id_;
}

int RescueOperation::cost() const
{
    return route_cost_;
}

const std::string& RescueOperation::route() const
{
    return route_;
}

const std::string& RescueOperation::status() const
{
    return status_;
}

/*
 * start -- transition operation from "Assigned" to "In Progress".
 * Returns silently; callers should verify status before calling.
 */
void RescueOperation::start()
{
    status_ = "In Progress";
}

/*
 * complete -- transition operation from "In Progress" to "Resolved".
 */
void RescueOperation::complete()
{
    status_ = "Resolved";
}

/*
 * cancel -- transition operation to "Cancelled".
 */
void RescueOperation::cancel()
{
    status_ = "Cancelled";
}

/*
 * print -- display operation details to 'out'.
 */
void RescueOperation::print(std::ostream& out) const
{
    out << "  Op #" << id_
        << " | victim #" << victim_id_
        << " | unit #"   << team_id_
        << " | cost "    << route_cost_ << " min"
        << " | " << status_
        << "\n    route: " << route_
        << "\n";
}
