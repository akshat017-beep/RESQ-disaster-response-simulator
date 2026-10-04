/* RESQ - C++ OOP layer implementation */
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include "resq.hpp"

namespace resq {

/* ================================ Victim ================================ */
Victim::Victim() : id_(-1), name_("unknown"), age_(0), severity_(1), rescued_(false) {}

Victim::Victim(int id, const std::string &name, int age, int severity, const Location &loc)
    : id_(id), name_(name), age_(age), severity_(severity), location_(loc), rescued_(false) {
    setSeverity(severity);
}

void Victim::setSeverity(int s) {
    if (s < 1)  s = 1;
    if (s > 10) s = 10;
    severity_ = s;
}

std::string Victim::triageTag() const {
    if (severity_ >= 7) return "RED";
    if (severity_ >= 4) return "YELLOW";
    return "GREEN";
}

VictimRecord Victim::toRecord() const {
    VictimRecord r{};
    r.id         = id_;
    r.age        = age_;
    r.severity   = severity_;
    r.locationId = location_.id();
    r.rescued    = rescued_ ? 1 : 0;
    std::snprintf(r.name, RESQ_NAME_LEN, "%s", name_.c_str());
    return r;
}

/* ============================== RescueUnit ============================== */
int RescueUnit::unitCount_ = 0;

RescueUnit::RescueUnit(int id, const std::string &callSign, const Location &base, int speedFactor)
    : id_(id), callSign_(callSign), base_(base), speedFactor_(speedFactor), busy_(false) {
    ++unitCount_;
}

RescueUnit::~RescueUnit() { --unitCount_; }

int RescueUnit::travelTime(int roadMinutes) const {
    if (roadMinutes >= RESQ_INF) return RESQ_INF;
    return (roadMinutes * 100) / (speedFactor_ > 0 ? speedFactor_ : 100);
}

void RescueUnit::describe() const {
    std::cout << "  [" << type() << "] " << callSign_
              << " | base: " << base_.name()
              << " | " << capability() << "\n";
}

/* ============================== RescueTeam ============================== */
RescueTeam::RescueTeam(int id, const std::string &cs, const Location &base,
                       int members, bool heavyEquipment)
    : RescueUnit(id, cs, base, heavyEquipment ? 80 : 100),
      members_(members), heavyEquipment_(heavyEquipment) {}

std::string RescueTeam::capability() const {
    std::ostringstream os;
    os << members_ << " responders"
       << (heavyEquipment_ ? ", heavy cutting/lifting gear" : ", light rescue kit");
    return os.str();
}

void RescueTeam::describe() const {
    std::cout << "  [Ground Team] " << callSign() << " | base: " << base().name()
              << " | " << capability() << "\n";
}

/* Heavy trucks lose time on debris-covered roads. */
int RescueTeam::travelTime(int roadMinutes) const {
    int t = RescueUnit::travelTime(roadMinutes);
    return (t >= RESQ_INF) ? RESQ_INF : t + (heavyEquipment_ ? 2 : 0);
}

/* =============================== Ambulance =============================== */
Ambulance::Ambulance(int id, const std::string &cs, const Location &base,
                     bool als, int hospitalId)
    : RescueUnit(id, cs, base, 130), advancedLifeSupport_(als), hospitalId_(hospitalId) {}

std::string Ambulance::capability() const {
    return advancedLifeSupport_ ? "advanced life support, siren priority"
                                : "basic life support, siren priority";
}

void Ambulance::describe() const {
    std::cout << "  [Ambulance]   " << callSign() << " | base: " << base().name()
              << " | " << capability() << "\n";
}

/* Ambulances get right of way -> one minute saved on any real route. */
int Ambulance::travelTime(int roadMinutes) const {
    int t = RescueUnit::travelTime(roadMinutes);
    if (t >= RESQ_INF) return RESQ_INF;
    return (t > 1) ? t - 1 : t;
}

/* ================================ Hospital ================================ */
Hospital::Hospital(int id, const std::string &name, const Location &loc, int beds, bool trauma)
    : id_(id), name_(name), location_(loc), totalBeds_(beds), freeBeds_(beds), trauma_(trauma) {}

bool Hospital::admit(const Victim &v) {
    if (freeBeds_ <= 0) return false;
    if (v.isCritical() && !trauma_) return false;   /* RED needs trauma care */
    --freeBeds_;
    return true;
}

void Hospital::discharge() { if (freeBeds_ < totalBeds_) ++freeBeds_; }

/* ================================ Disaster ================================ */
Disaster::Disaster(const std::string &type, const std::string &zone, int magnitude)
    : type_(type), zone_(zone), magnitude_(magnitude) {}

void Disaster::banner() const {
    std::cout << "\n==============================================================\n"
              << "  RESQ SIMULATOR  -  Algorithms That Save Lives\n"
              << "  Disaster : " << type_ << "   (severity " << magnitude_ << "/10)\n"
              << "  Zone     : " << zone_ << "\n"
              << "==============================================================\n";
}

/* ============================ RescueOperation ============================ */
RescueOperation::RescueOperation(const Disaster &d) : disaster_(d) {
    g_init(&map_, RESQ_MAX_NODES);
    vl_init(&records_);
    q_init(&normalQueue_);
    pq_init(&criticalQueue_);
    st_init(&history_);
}

RescueOperation::~RescueOperation() {
    for (RescueUnit *u : units_) delete u;
    units_.clear();
    vl_free(&records_);
}

void RescueOperation::addLocation(int id, const std::string &name) {
    g_set_name(&map_, id, name.c_str());
    if ((int)locations_.size() <= id) locations_.resize(id + 1);
    locations_[id] = Location(id, name);
}

void RescueOperation::addRoad(int a, int b, int minutes) { g_add_edge(&map_, a, b, minutes); }

void RescueOperation::blockRoad(int a, int b) {
    g_block_edge(&map_, a, b);
    std::ostringstream os;
    os << "ROAD BLOCKED: " << g_name(&map_, a) << " <-> " << g_name(&map_, b);
    log(os.str());
    std::cout << "  ! " << os.str() << "\n";
}

void RescueOperation::addUnit(RescueUnit *u)          { units_.push_back(u); }
void RescueOperation::addHospital(const Hospital &h)  { hospitals_.push_back(h); }

void RescueOperation::showResources() const {
    std::cout << "\n-- Deployed resources (polymorphic describe()) --\n";
    for (const RescueUnit *u : units_) u->describe();   /* base pointer -> derived impl */
    for (const Hospital &h : hospitals_)
        std::cout << "  [Hospital]    " << h.name() << " | " << h.totalBeds() << " beds | "
                  << (h.isTrauma() ? "trauma centre" : "general care") << "\n";
}

void RescueOperation::log(const std::string &text) { st_push(&history_, text.c_str()); }

Victim *RescueOperation::findVictim(int id) {
    for (Victim &v : victims_) if (v.id() == id) return &v;
    return nullptr;
}

void RescueOperation::reportVictim(const Victim &v) {
    victims_.push_back(v);
    VictimRecord r = v.toRecord();
    vl_push(&records_, r);                       /* linked list record */

    if (v.isCritical()) pq_push(&criticalQueue_, v.id(), v.severity());  /* heap  */
    else                q_enqueue(&normalQueue_, v.id());                /* queue */

    std::cout << "  + Call received: #" << v.id() << " " << v.name()
              << " at " << v.location().name()
              << " | severity " << v.severity() << " (" << v.triageTag() << ") -> "
              << (v.isCritical() ? "PRIORITY QUEUE" : "normal queue") << "\n";

    std::ostringstream os;
    os << "REGISTERED victim #" << v.id() << " (" << v.triageTag() << ")";
    log(os.str());
}

std::string RescueOperation::routeToString(const std::vector<int> &path) const {
    std::ostringstream os;
    for (size_t i = 0; i < path.size(); ++i) {
        if (i) os << " -> ";
        os << g_name(&map_, path[i]);
    }
    return os.str();
}

/* Choose the free unit with the smallest Dijkstra-based ETA. */
RescueUnit *RescueOperation::bestUnitFor(const Victim &v, int &etaOut, std::vector<int> &pathOut) {
    RescueUnit *best = nullptr;
    etaOut = RESQ_INF;
    pathOut.clear();

    for (RescueUnit *u : units_) {
        if (u->busy()) continue;
        /* an ambulance alone cannot extract a trapped RED victim */
        if (v.isCritical() && u->type() == "Ambulance") continue;

        int dist[RESQ_MAX_NODES], prev[RESQ_MAX_NODES], raw[RESQ_MAX_NODES];
        g_dijkstra(&map_, u->base().id(), dist, prev);
        int nominal = dist[v.location().id()];
        if (nominal >= RESQ_INF) continue;

        int eta = u->travelTime(nominal);
        if (eta < etaOut) {
            int len = g_path(prev, u->base().id(), v.location().id(), raw);
            etaOut = eta;
            best   = u;
            pathOut.assign(raw, raw + len);
        }
    }
    return best;
}

Hospital *RescueOperation::nearestHospital(int fromNode, bool needTrauma, int &etaOut, std::vector<int> &pathOut) {
    int dist[RESQ_MAX_NODES], prev[RESQ_MAX_NODES], raw[RESQ_MAX_NODES];
    g_dijkstra(&map_, fromNode, dist, prev);

    Hospital *best = nullptr;
    etaOut = RESQ_INF;
    pathOut.clear();
    for (Hospital &h : hospitals_) {
        int d = dist[h.location().id()];
        if (h.freeBeds() <= 0 || d >= RESQ_INF || d >= etaOut) continue;
        if (needTrauma && !h.isTrauma()) continue;   /* RED victims need trauma care */
        int len = g_path(prev, fromNode, h.location().id(), raw);
        etaOut = d;
        best   = &h;
        pathOut.assign(raw, raw + len);
    }
    return best;
}

/* ------------------------------- BFS view ------------------------------- */
void RescueOperation::showMapExploration(int commandCentre) {
    int order[RESQ_MAX_NODES], hops[RESQ_MAX_NODES];
    int n = g_bfs(&map_, commandCentre, order, hops);

    std::cout << "\n-- BFS area exploration from " << g_name(&map_, commandCentre) << " --\n";
    std::cout << "   reachable locations: " << n << " of " << locations_.size() << "\n";
    for (int i = 0; i < n; ++i)
        std::cout << "   hop " << hops[order[i]] << "  " << g_name(&map_, order[i]) << "\n";

    for (size_t i = 0; i < locations_.size(); ++i)
        if (hops[i] == -1)
            std::cout << "   !! CUT OFF: " << g_name(&map_, (int)i)
                      << " (needs air support)\n";
}

/* ---------------------------- Dijkstra view ---------------------------- */
void RescueOperation::showRouteTable(int commandCentre) {
    int dist[RESQ_MAX_NODES], prev[RESQ_MAX_NODES], raw[RESQ_MAX_NODES];
    g_dijkstra(&map_, commandCentre, dist, prev);

    std::cout << "\n-- Dijkstra route table from " << g_name(&map_, commandCentre) << " --\n";
    std::cout << "   " << std::left << std::setw(22) << "LOCATION"
              << std::setw(10) << "MINUTES" << "ROUTE\n";
    for (size_t i = 0; i < locations_.size(); ++i) {
        std::cout << "   " << std::left << std::setw(22) << g_name(&map_, (int)i);
        if (dist[i] >= RESQ_INF) { std::cout << "unreachable\n"; continue; }
        int len = g_path(prev, commandCentre, (int)i, raw);
        std::vector<int> p(raw, raw + len);
        std::cout << std::setw(10) << dist[i] << routeToString(p) << "\n";
    }
}

/* ----------------------------- dispatch loop ----------------------------- */
void RescueOperation::runDispatch() {
    std::cout << "\n-- DISPATCH: critical victims first (priority queue), "
                 "then FIFO queue --\n";

    auto serve = [&](int victimId, const char *source) {
        Victim *v = findVictim(victimId);
        if (!v || v->rescued()) return;

        int eta = 0;
        std::vector<int> path;
        RescueUnit *u = bestUnitFor(*v, eta, path);

        std::cout << "\n  [" << source << "] victim #" << v->id() << " " << v->name()
                  << " (" << v->triageTag() << ", severity " << v->severity() << ") at "
                  << v->location().name() << "\n";

        if (!u) {
            ++unreachable_;
            std::cout << "     x no unit can reach this location - queued for air support\n";
            log("UNREACHABLE victim #" + std::to_string(v->id()));
            return;
        }

        u->setBusy(true);
        std::cout << "     assigned : " << u->callSign() << " (" << u->type() << ")\n"
                  << "     route    : " << routeToString(path) << "\n"
                  << "     ETA      : " << eta << " min\n";

        int hEta = 0;
        std::vector<int> hPath;
        Hospital *h = nearestHospital(v->location().id(), v->isCritical(), hEta, hPath);
        int total = eta;

        if (h && h->admit(*v)) {
            total += hEta;
            std::cout << "     hospital : " << h->name() << " (" << hEta << " min, "
                      << h->freeBeds() << " beds left)\n"
                      << "     transfer : " << routeToString(hPath) << "\n";
        } else {
            std::cout << "     hospital : treated on site (no suitable bed available)\n";
        }

        v->markRescued();
        if (VictimRecord *r = vl_find(&records_, v->id())) r->rescued = 1;
        ++rescuedCount_;
        totalMinutes_ += total;
        u->setBusy(false);                       /* unit returns to service */

        std::cout << "     STATUS   : RESCUED in " << total << " min\n";
        log("RESCUED victim #" + std::to_string(v->id()) + " by " + u->callSign());
    };

    PQItem item;
    while (pq_pop(&criticalQueue_, &item)) serve(item.victimId, "CRITICAL");

    int id;
    while (q_dequeue(&normalQueue_, &id)) serve(id, "ROUTINE ");
}

/* -------------------------- sorting & searching -------------------------- */
void RescueOperation::showTriageTable() {
    VictimRecord arr[RESQ_MAX_NODES * 4];
    int n = vl_to_array(&records_, arr, RESQ_MAX_NODES * 4);

    sort_by_severity_desc(arr, n);
    std::cout << "\n-- Triage list (insertion sort by severity) --\n";
    std::cout << "   " << std::left << std::setw(6) << "ID" << std::setw(18) << "NAME"
              << std::setw(6) << "AGE" << std::setw(6) << "SEV" << "STATUS\n";
    for (int i = 0; i < n; ++i)
        std::cout << "   " << std::left << std::setw(6) << arr[i].id
                  << std::setw(18) << arr[i].name
                  << std::setw(6) << arr[i].age
                  << std::setw(6) << arr[i].severity
                  << (arr[i].rescued ? "rescued" : "WAITING") << "\n";

    sort_by_id_asc(arr, n);
    if (n > 0) {
        int target = arr[n / 2].id;
        int idx = binary_search_by_id(arr, n, target);
        std::cout << "   binary search for id " << target << " -> index " << idx
                  << " (" << arr[idx].name << ")\n";
        int li = linear_search_by_name(arr, n, arr[0].name);
        std::cout << "   linear search for \"" << arr[0].name << "\" -> index "
                  << li << "\n";
    }
}

void RescueOperation::showHistory() {
    std::cout << "\n-- Operation history (stack: most recent first) --\n";
    char buf[RESQ_MAX_TEXT];
    while (st_pop(&history_, buf)) std::cout << "   " << buf << "\n";
}

void RescueOperation::showSummary() {
    std::cout << "\n-- Mission summary --\n"
              << "   disaster        : " << disaster_.type() << " @ " << disaster_.zone() << "\n"
              << "   victims logged  : " << victims_.size() << "\n"
              << "   rescued         : " << rescuedCount_ << "\n"
              << "   unreachable     : " << unreachable_ << "\n"
              << "   units deployed  : " << RescueUnit::unitCount() << "\n";
    if (rescuedCount_)
        std::cout << "   avg response    : " << (totalMinutes_ / rescuedCount_) << " min\n";
    for (const Hospital &h : hospitals_)
        std::cout << "   " << h.name() << " beds: " << h.freeBeds()
                  << "/" << h.totalBeds() << "\n";
}

} /* namespace resq */
