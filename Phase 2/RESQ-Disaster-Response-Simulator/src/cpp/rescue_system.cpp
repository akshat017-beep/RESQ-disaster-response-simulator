#include "rescue_system.h"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <limits>

Location::Location(int id, const std::string& name) : id_(id), name_(name) {}
int Location::id() const { return id_; }
const std::string& Location::name() const { return name_; }
bool Location::setName(const std::string& name) {
    if (name.empty() || name.size() > 79) return false;
    name_ = name;
    return true;
}

Disaster::Disaster(int id, const std::string& type, const std::string& area, int severity, int affected)
    : id_(id), type_(type), area_(area), severity_(severity), affected_(affected) {}
int Disaster::id() const { return id_; }
const std::string& Disaster::type() const { return type_; }
const std::string& Disaster::area() const { return area_; }
int Disaster::severity() const { return severity_; }
int Disaster::affected() const { return affected_; }
bool Disaster::updateSeverity(int severity) {
    if (severity < 1 || severity > 3) return false;
    severity_ = severity;
    return true;
}
bool Disaster::updateAffected(int affected) {
    if (affected < 0 || affected > 1000000) return false;
    affected_ = affected;
    return true;
}

Victim::Victim(int id, const std::string& name, const std::string& location, const std::string& description, int affected, int severity)
    : id_(id), name_(name), location_(location), description_(description), affected_(affected), severity_(severity), status_(0) {}
int Victim::id() const { return id_; }
const std::string& Victim::name() const { return name_; }
const std::string& Victim::location() const { return location_; }
const std::string& Victim::description() const { return description_; }
int Victim::affected() const { return affected_; }
int Victim::severity() const { return severity_; }
int Victim::status() const { return status_; }
int Victim::priorityScore() const {
    int base = severity_ == 3 ? 300 : severity_ == 2 ? 200 : 100;
    int people = affected_ * 2;
    if (people > 80) people = 80;
    return base + people + (status_ == 0 ? 10 : 0);
}
bool Victim::setStatus(int status) {
    if (status < 0 || status > 4) return false;
    if (status_ == 3 || status_ == 4) return false;
    if (status == 2 && status_ != 1) return false;
    if (status == 3 && status_ != 2) return false;
    status_ = status;
    return true;
}
void Victim::print() const {
    const char* levels[] = {"", "GREEN", "YELLOW", "RED"};
    const char* states[] = {"Waiting", "Assigned", "In Progress", "Resolved", "Cancelled"};
    std::cout << "Victim #" << id_ << " | " << name_ << " | " << levels[severity_]
              << " | affected " << affected_ << " | " << location_ << " | "
              << states[status_] << " | score " << priorityScore() << "\n";
    std::cout << "  Details: " << description_ << "\n";
}

RescueTeam::RescueTeam(int id, const std::string& name, const std::string& location, int capacity)
    : id_(id), name_(name), location_(location), capacity_(capacity), available_(true) {}
RescueTeam::~RescueTeam() = default;
int RescueTeam::id() const { return id_; }
const std::string& RescueTeam::name() const { return name_; }
const std::string& RescueTeam::location() const { return location_; }
int RescueTeam::capacity() const { return capacity_; }
bool RescueTeam::available() const { return available_; }
std::string RescueTeam::dispatchType() const { return "General Rescue Team"; }
bool RescueTeam::dispatch(int people) {
    if (!available_ || people < 0 || people > capacity_) return false;
    available_ = false;
    return true;
}
void RescueTeam::print() const {
    std::cout << "Team #" << id_ << " | " << name_ << " | " << dispatchType()
              << " | location " << location_ << " | capacity " << capacity_
              << " | " << (available_ ? "Available" : "Deployed") << "\n";
}
bool RescueTeam::moveTo(const std::string& location) {
    if (!available_ || location.empty()) return false;
    location_ = location;
    return true;
}
bool RescueTeam::release() {
    if (available_) return false;
    available_ = true;
    return true;
}

Ambulance::Ambulance(int id, const std::string& name, const std::string& location, int patientCapacity)
    : RescueTeam(id, name, location, patientCapacity), patientCapacity_(patientCapacity) {}
std::string Ambulance::dispatchType() const { return "Ambulance"; }
bool Ambulance::dispatch(int people) {
    if (people > patientCapacity_) return false;
    return RescueTeam::dispatch(people);
}
void Ambulance::print() const {
    std::cout << "Ambulance #" << id_ << " | " << name_ << " | " << location_
              << " | patient capacity " << patientCapacity_ << " | "
              << (available_ ? "Available" : "Deployed") << "\n";
}

RescueCrew::RescueCrew(int id, const std::string& name, const std::string& location, int crewMembers)
    : RescueTeam(id, name, location, crewMembers), crewMembers_(crewMembers) {}
std::string RescueCrew::dispatchType() const { return "Rescue Crew"; }
bool RescueCrew::dispatch(int people) {
    if (people > crewMembers_) return false;
    return RescueTeam::dispatch(people);
}
void RescueCrew::print() const {
    std::cout << "Rescue Crew #" << id_ << " | " << name_ << " | " << location_
              << " | crew size " << crewMembers_ << " | "
              << (available_ ? "Available" : "Deployed") << "\n";
}

Hospital::Hospital(int id, const std::string& name, const std::string& location, int beds)
    : id_(id), name_(name), location_(location), beds_(beds), occupied_(0) {}
int Hospital::id() const { return id_; }
const std::string& Hospital::name() const { return name_; }
const std::string& Hospital::location() const { return location_; }
int Hospital::availableBeds() const { return beds_ - occupied_; }
bool Hospital::admit(int people) {
    if (people <= 0 || people > availableBeds()) return false;
    occupied_ += people;
    return true;
}
bool Hospital::discharge(int people) {
    if (people <= 0 || people > occupied_) return false;
    occupied_ -= people;
    return true;
}
void Hospital::print() const {
    std::cout << "Hospital #" << id_ << " | " << name_ << " | " << location_
              << " | beds " << occupied_ << "/" << beds_ << " occupied | available "
              << availableBeds() << "\n";
}

RescueOperation::RescueOperation(int id, int victimId, int teamId, const std::string& route, int routeCost)
    : id_(id), victimId_(victimId), teamId_(teamId), route_(route), routeCost_(routeCost),
      status_("Assigned"), createdAt_(std::time(nullptr)) {}
int RescueOperation::id() const { return id_; }
int RescueOperation::victimId() const { return victimId_; }
int RescueOperation::teamId() const { return teamId_; }
const std::string& RescueOperation::route() const { return route_; }
int RescueOperation::routeCost() const { return routeCost_; }
const std::string& RescueOperation::status() const { return status_; }
bool RescueOperation::start() {
    if (status_ != "Assigned") return false;
    status_ = "In Progress";
    return true;
}
bool RescueOperation::complete() {
    if (status_ != "In Progress") return false;
    status_ = "Resolved";
    return true;
}
bool RescueOperation::cancel() {
    if (status_ == "Resolved" || status_ == "Cancelled") return false;
    status_ = "Cancelled";
    return true;
}
void RescueOperation::print() const {
    std::cout << "Operation #" << id_ << " | victim #" << victimId_ << " | team #" << teamId_
              << " | route: " << route_ << " | cost " << routeCost_ << " | " << status_ << "\n";
}

ResqScenarioManager::ResqScenarioManager()
    : nextVictimId_(1001), nextTeamId_(201), nextHospitalId_(301), nextOperationId_(401) {}
ResqScenarioManager::~ResqScenarioManager() {
    for (RescueTeam* team : teams_) delete team;
}
int ResqScenarioManager::addVictim(const std::string& name, const std::string& location, const std::string& description, int affected, int severity) {
    if (name.empty() || location.empty() || affected < 1 || affected > 10000 || severity < 1 || severity > 3) return -1;
    int id = nextVictimId_++;
    victims_.emplace_back(id, name, location, description, affected, severity);
    return id;
}
int ResqScenarioManager::addAmbulance(const std::string& name, const std::string& location, int capacity) {
    if (name.empty() || location.empty() || capacity < 1 || capacity > 10000) return -1;
    int id = nextTeamId_++;
    teams_.push_back(new Ambulance(id, name, location, capacity));
    return id;
}
int ResqScenarioManager::addRescueCrew(const std::string& name, const std::string& location, int members) {
    if (name.empty() || location.empty() || members < 1 || members > 10000) return -1;
    int id = nextTeamId_++;
    teams_.push_back(new RescueCrew(id, name, location, members));
    return id;
}
int ResqScenarioManager::addHospital(const std::string& name, const std::string& location, int beds) {
    if (name.empty() || location.empty() || beds < 1 || beds > 100000) return -1;
    int id = nextHospitalId_++;
    hospitals_.emplace_back(id, name, location, beds);
    return id;
}
int ResqScenarioManager::addDisaster(const std::string& type, const std::string& area, int severity, int affected) {
    if (type.empty() || area.empty() || severity < 1 || severity > 3 || affected < 0) return -1;
    int id = static_cast<int>(disasters_.size()) + 501;
    disasters_.emplace_back(id, type, area, severity, affected);
    return id;
}
int ResqScenarioManager::addLocation(const std::string& name) {
    if (name.empty()) return -1;
    int id = static_cast<int>(locations_.size()) + 1;
    locations_.emplace_back(id, name);
    return id;
}
bool ResqScenarioManager::addRoad(int fromIndex, int toIndex, int weight) {
    return fromIndex >= 0 && toIndex >= 0 && fromIndex < static_cast<int>(locations_.size()) &&
           toIndex < static_cast<int>(locations_.size()) && fromIndex != toIndex && weight > 0;
}
bool ResqScenarioManager::dispatchVictim(int victimId, int teamId, const std::string& route, int routeCost) {
    Victim* victim = findVictim(victimId);
    RescueTeam* team = findTeam(teamId);
    if (!victim || !team || victim->status() != 0 || !team->available() || route.empty() || routeCost < 0) return false;
    if (!team->dispatch(victim->affected())) return false;
    if (!victim->setStatus(1)) {
        team->release();
        return false;
    }
    operations_.emplace_back(nextOperationId_++, victimId, teamId, route, routeCost);
    return true;
}
bool ResqScenarioManager::startOperation(int operationId) {
    RescueOperation* operation = findOperation(operationId);
    Victim* victim = operation ? findVictim(operation->victimId()) : nullptr;
    return operation && victim && operation->start() && victim->setStatus(2);
}
bool ResqScenarioManager::completeOperation(int operationId) {
    RescueOperation* operation = findOperation(operationId);
    if (!operation || !operation->complete()) return false;
    Victim* victim = findVictim(operation->victimId());
    RescueTeam* team = findTeam(operation->teamId());
    bool victimChanged = victim && victim->setStatus(3);
    bool teamReleased = team && team->release();
    return victimChanged && teamReleased;
}
bool ResqScenarioManager::cancelOperation(int operationId) {
    RescueOperation* operation = findOperation(operationId);
    if (!operation || !operation->cancel()) return false;
    Victim* victim = findVictim(operation->victimId());
    RescueTeam* team = findTeam(operation->teamId());
    if (victim) victim->setStatus(4);
    if (team) team->release();
    return true;
}
bool ResqScenarioManager::admitVictim(int victimId, int hospitalId, int people) {
    Victim* victim = findVictim(victimId);
    Hospital* hospital = findHospital(hospitalId);
    return victim && hospital && victim->status() == 3 && hospital->admit(people);
}
void ResqScenarioManager::listVictims() const {
    if (victims_.empty()) { std::cout << "No victims registered.\n"; return; }
    std::vector<const Victim*> ordered;
    for (const auto& victim : victims_) ordered.push_back(&victim);
    std::sort(ordered.begin(), ordered.end(), [](const Victim* a, const Victim* b) {
        if (a->priorityScore() != b->priorityScore()) return a->priorityScore() > b->priorityScore();
        return a->id() < b->id();
    });
    for (const Victim* victim : ordered) victim->print();
}
void ResqScenarioManager::listTeams() const {
    if (teams_.empty()) { std::cout << "No rescue teams registered.\n"; return; }
    for (const RescueTeam* team : teams_) team->print();
}
void ResqScenarioManager::listHospitals() const {
    if (hospitals_.empty()) { std::cout << "No hospitals registered.\n"; return; }
    for (const auto& hospital : hospitals_) hospital.print();
}
void ResqScenarioManager::listDisasters() const {
    if (disasters_.empty()) { std::cout << "No disaster scenarios registered.\n"; return; }
    for (const auto& disaster : disasters_) {
        std::cout << "Disaster #" << disaster.id() << " | " << disaster.type() << " | "
                  << disaster.area() << " | severity " << disaster.severity()
                  << " | affected " << disaster.affected() << "\n";
    }
}
void ResqScenarioManager::listLocations() const {
    if (locations_.empty()) { std::cout << "No locations registered.\n"; return; }
    for (const auto& location : locations_) std::cout << location.id() << ". " << location.name() << "\n";
}
void ResqScenarioManager::listOperations() const {
    if (operations_.empty()) { std::cout << "No rescue operations registered.\n"; return; }
    for (const auto& operation : operations_) operation.print();
}
void ResqScenarioManager::showSummary() const {
    int waiting = 0, assigned = 0, active = 0, resolved = 0, cancelled = 0, available = 0;
    for (const auto& victim : victims_) {
        if (victim.status() == 0) waiting++;
        else if (victim.status() == 1) assigned++;
        else if (victim.status() == 2) active++;
        else if (victim.status() == 3) resolved++;
        else if (victim.status() == 4) cancelled++;
    }
    for (const RescueTeam* team : teams_) if (team->available()) available++;
    std::cout << "\nRESQ SUMMARY\n";
    std::cout << "Victims registered : " << victims_.size() << "\n";
    std::cout << "Waiting            : " << waiting << "\n";
    std::cout << "Assigned           : " << assigned << "\n";
    std::cout << "Active operations  : " << active << "\n";
    std::cout << "Resolved           : " << resolved << "\n";
    std::cout << "Cancelled          : " << cancelled << "\n";
    std::cout << "Rescue teams        : " << teams_.size() << "\n";
    std::cout << "Available teams     : " << available << "\n";
    std::cout << "Hospitals           : " << hospitals_.size() << "\n";
    std::cout << "Disaster records    : " << disasters_.size() << "\n";
    std::cout << "Operations logged   : " << operations_.size() << "\n";
}
Victim* ResqScenarioManager::findVictim(int id) {
    for (auto& victim : victims_) if (victim.id() == id) return &victim;
    return nullptr;
}
RescueTeam* ResqScenarioManager::findTeam(int id) {
    for (auto* team : teams_) if (team->id() == id) return team;
    return nullptr;
}
Hospital* ResqScenarioManager::findHospital(int id) {
    for (auto& hospital : hospitals_) if (hospital.id() == id) return &hospital;
    return nullptr;
}
RescueOperation* ResqScenarioManager::findOperation(int id) {
    for (auto& operation : operations_) if (operation.id() == id) return &operation;
    return nullptr;
}
void ResqScenarioManager::runScenarioTests() const {
    int passed = 0;
    int total = 0;
    auto check = [&](bool condition, const std::string& name) {
        total++;
        if (condition) {
            passed++;
            std::cout << "[PASS] " << name << "\n";
        } else {
            std::cout << "[FAIL] " << name << "\n";
        }
    };
    Victim red(1, "Test Red", "CMD", "Critical case", 2, 3);
    Victim yellow(2, "Test Yellow", "Market", "Urgent case", 2, 2);
    Victim green(3, "Test Green", "Clock", "Non-critical case", 1, 1);
    check(red.priorityScore() > yellow.priorityScore(), "RED priority exceeds YELLOW");
    check(yellow.priorityScore() > green.priorityScore(), "YELLOW priority exceeds GREEN");
    check(red.setStatus(1), "Victim can move from waiting to assigned");
    check(red.setStatus(2), "Victim can move from assigned to in progress");
    check(red.setStatus(3), "Victim can move from in progress to resolved");
    check(!red.setStatus(0), "Resolved victim cannot return to waiting");
    Ambulance ambulance(1, "Test Ambulance", "CMD", 3);
    check(ambulance.dispatch(2), "Ambulance accepts a valid assignment");
    check(!ambulance.dispatch(2), "Deployed ambulance rejects another assignment");
    check(ambulance.release(), "Ambulance can be released");
    Hospital hospital(1, "Test Hospital", "CMD", 5);
    check(hospital.admit(3), "Hospital admits within capacity");
    check(!hospital.admit(3), "Hospital rejects admission beyond capacity");
    check(hospital.discharge(2), "Hospital discharges valid patient count");
    std::cout << "Scenario tests: " << passed << "/" << total << " passed.\n";
}
