#include "analytics.h"
#include "console.h"
#include "demo_data.h"
#include "dispatcher.h"
#include "emergency_manager.h"
#include "file_manager.h"
#include "resource_manager.h"
#include "resq_c.h"
#include "util.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace resq;

namespace {

int g_checks = 0;
int g_failures = 0;
int g_testsFailed = 0;

#define CHECK(condition)                                                                  \
    do {                                                                                  \
        ++g_checks;                                                                       \
        if (!(condition)) {                                                               \
            ++g_failures;                                                                 \
            std::cout << "    FAILED line " << __LINE__ << ": " #condition << '\n';      \
        }                                                                                 \
    } while (0)

void run(const char* name, void (*test)())
{
    const int before = g_failures;
    std::cout << "[TEST] " << name << '\n';
    try {
        test();
    } catch (const std::exception& ex) {
        ++g_failures;
        std::cout << "    FAILED with exception: " << ex.what() << '\n';
    }
    const bool passed = g_failures == before;
    if (!passed) {
        ++g_testsFailed;
    }
    std::cout << (passed ? "       PASS\n" : "       FAIL\n");
}

const std::time_t T0 = 1790000000;
constexpr std::time_t MIN = 60;

Emergency makeCase(const std::string& id, Severity severity, int affected = 1, Category category = Category::Medical,
                   std::time_t registered = T0, const std::string& location = "Test Street")
{
    return Emergency(id, "Test Reporter", category, location, "98765 43210", "Test description", severity, affected,
                     registered);
}

bool exists(const std::string& path) { return resq_file_exists(path.c_str()) == 1; }

void removeDir(const std::string& dir, const std::vector<std::string>& extra = {})
{
    for (const char* name : {"emergencies.txt", "resources.txt", "assignments.txt"}) {
        for (const char* suffix : {"", ".bak", ".tmp"}) {
            resq_remove_file((dir + "/" + name + suffix).c_str());
        }
    }
    for (const std::string& path : extra) {
        resq_remove_file(path.c_str());
    }
    resq_remove_dir(dir.c_str());
}

std::string tempDir(const std::string& name)
{
    const std::string dir = "resq_test_" + name;
    removeDir(dir);
    return dir;
}

std::string readAll(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

struct World {
    EmergencyManager em;
    ResourceManager rm;
    Dispatcher dispatcher{em, rm};
};

void t01_registerValid()
{
    EmergencyManager em;
    CHECK(em.add(makeCase("E-1001", Severity::High, 3)).empty());
    const Emergency* e = em.find("e-1001");
    CHECK(e != nullptr);
    CHECK(e->status() == Status::Pending);
    CHECK(e->affected() == 3);
    CHECK(e->priorityScore() > 0);
    CHECK(em.generateId() == "E-1002");

    bool rejected = false;
    try {
        Emergency bad("E-1", "", Category::Fire, "", "12", "", Severity::Low, -4, T0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CHECK(rejected);
    CHECK(!Emergency::validateContact("abc").empty());
    CHECK(Emergency::validateContact("+91 98765-43210").empty());
    CHECK(Emergency::validateContact("someone@example.org").empty());
    CHECK(!Emergency::validateId("1ABC").empty());
    CHECK(!Emergency::validateId("TOO-LONG-ID").empty());
}

void t02_rejectDuplicateId()
{
    EmergencyManager em;
    CHECK(em.add(makeCase("E-2001", Severity::High)).empty());
    const std::string error = em.add(makeCase("e-2001", Severity::Low));
    CHECK(!error.empty());
    CHECK(error.find("already exists") != std::string::npos);
    CHECK(em.size() == 1);
    CHECK(em.find("E-2001")->severity() == Severity::High);
}

void t03_multipleSeverities()
{
    EmergencyManager em;
    CHECK(em.add(makeCase("E-3001", Severity::Low)).empty());
    CHECK(em.add(makeCase("E-3002", Severity::Critical)).empty());
    CHECK(em.add(makeCase("E-3003", Severity::Medium)).empty());
    CHECK(em.add(makeCase("E-3004", Severity::High)).empty());
    CHECK(em.size() == 4);
    const auto bySeverity = em.sorted(EmergencySortKey::Severity);
    CHECK(bySeverity[0]->id() == "E-3002");
    CHECK(bySeverity[1]->id() == "E-3004");
    CHECK(bySeverity[2]->id() == "E-3003");
    CHECK(bySeverity[3]->id() == "E-3001");
    const auto byId = em.sorted(EmergencySortKey::Id, true);
    CHECK(byId.front()->id() == "E-3004");
}

void t04_priorityOrdering()
{
    EmergencyManager em;
    CHECK(em.add(makeCase("E-4001", Severity::Low, 5000, Category::Fire, T0 - 600 * MIN)).empty());
    CHECK(em.add(makeCase("E-4002", Severity::Critical, 1, Category::Other, T0)).empty());
    CHECK(em.add(makeCase("E-4003", Severity::High, 2, Category::Medical, T0 - 60 * MIN)).empty());
    CHECK(em.add(makeCase("E-4004", Severity::High, 2, Category::Medical, T0 - 5 * MIN)).empty());

    const auto queue = em.pendingByPriority(T0);
    CHECK(queue.size() == 4);
    CHECK(queue[0]->id() == "E-4002");
    CHECK(queue[1]->id() == "E-4003");
    CHECK(queue[2]->id() == "E-4004");
    CHECK(queue[3]->id() == "E-4001");
    CHECK(em.highestPriorityPending(T0)->id() == "E-4002");
    CHECK(em.queuePosition("E-4004", T0) == 3);

    const PriorityBreakdown b = em.find("E-4003")->priorityBreakdown(T0);
    CHECK(b.severityPoints == 300);
    CHECK(b.affectedPoints == 16);
    CHECK(b.waitingPoints == 20);
    CHECK(b.categoryPoints == 8);
    CHECK(b.total() == 344);
    CHECK(PriorityCalculator::waitingPoints(100000) == PriorityCalculator::kMaxWaitingPoints);
    CHECK(PriorityCalculator::affectedPoints(1000000) == PriorityCalculator::kMaxAffectedPoints);
    CHECK(PriorityCalculator::affectedPoints(0) == 0);
}

void t05_searchExisting()
{
    EmergencyManager em;
    Emergency a("E-5001", "Asha Rao", Category::Flood, "Lakeview Colony", "12345 67890", "Water", Severity::High, 4, T0);
    Emergency b("E-5002", "Ravi Kumar", Category::Fire, "Old Market", "12345 67891", "Smoke", Severity::Critical, 2, T0);
    CHECK(em.add(a).empty());
    CHECK(em.add(b).empty());
    CHECK(em.search(EmergencySearchField::Id, "e-5002").size() == 1);
    CHECK(em.search(EmergencySearchField::Id, "500").size() == 2);
    CHECK(em.search(EmergencySearchField::Reporter, "asha").front()->id() == "E-5001");
    CHECK(em.search(EmergencySearchField::Location, "MARKET").front()->id() == "E-5002");
    CHECK(em.searchByCategory(Category::Flood).size() == 1);
}

void t06_searchNonexistent()
{
    EmergencyManager em;
    CHECK(em.search(EmergencySearchField::Id, "E-9999").empty());
    CHECK(em.add(makeCase("E-6001", Severity::Low)).empty());
    CHECK(em.search(EmergencySearchField::Id, "X-404").empty());
    CHECK(em.search(EmergencySearchField::Reporter, "Nobody").empty());
    CHECK(em.search(EmergencySearchField::Location, "Atlantis").empty());
    CHECK(em.search(EmergencySearchField::Location, "   ").empty());
    CHECK(em.searchByCategory(Category::Fire).empty());
    CHECK(em.find("E-0000") == nullptr);
}

void t07_updateEmergency()
{
    World w;
    CHECK(w.em.add(makeCase("E-7001", Severity::Low, 1)).empty());
    const int before = w.em.find("E-7001")->priorityScore();
    CHECK(w.em.updateSeverity("E-7001", Severity::Critical, T0).empty());
    CHECK(w.em.find("E-7001")->priorityScore() == before + 300);
    CHECK(w.em.updateLocation("E-7001", "New Road 5", T0).empty());
    CHECK(w.em.find("E-7001")->location() == "New Road 5");
    CHECK(w.em.updateDescription("E-7001", "Updated text", T0).empty());
    CHECK(w.em.updateAffected("E-7001", 15, T0).empty());
    CHECK(w.em.find("E-7001")->priorityBreakdown(T0).affectedPoints == 40);
    CHECK(!w.em.updateAffected("E-7001", -1, T0).empty());
    CHECK(!w.em.updateLocation("E-7001", "", T0).empty());
    CHECK(w.em.find("E-7001")->affected() == 15);
    CHECK(!w.em.updateSeverity("NOPE", Severity::Low, T0).empty());
    CHECK(w.dispatcher.cancel("E-7001", T0).ok);
    CHECK(!w.em.updateSeverity("E-7001", Severity::Low, T0).empty());
    CHECK(w.em.updateDescription("E-7001", "Corrected after closing", T0).empty());
    CHECK(!Emergency::isTransitionAllowed(Status::Pending, Status::Resolved));
    CHECK(!Emergency::isTransitionAllowed(Status::Resolved, Status::Pending));
    CHECK(!Emergency::isTransitionAllowed(Status::Cancelled, Status::InProgress));
    CHECK(Emergency::isTransitionAllowed(Status::Assigned, Status::InProgress));
    CHECK(!w.dispatcher.changeStatus("E-7001", Status::InProgress, T0).ok);
}

void t08_addAndSearchResource()
{
    ResourceManager rm;
    CHECK(rm.add(Resource("R-101", ResourceType::Ambulance, "Riverside", 2)).empty());
    CHECK(rm.add(Resource("R-102", ResourceType::FireEngine, "Old Market", 6)).empty());
    CHECK(!rm.add(Resource("r-101", ResourceType::RescueTeam, "Elsewhere", 4)).empty());
    CHECK(rm.size() == 2);
    CHECK(rm.search(ResourceSearchField::Id, "R-102").size() == 1);
    CHECK(rm.search(ResourceSearchField::Location, "river").front()->id() == "R-101");
    CHECK(rm.search(ResourceSearchField::Type, "fire").size() == 1);
    CHECK(rm.byStatus(ResourceStatus::Available).size() == 2);
    CHECK(rm.search(ResourceSearchField::Id, "R-999").empty());
    CHECK(rm.generateId() == "R-103");
    bool rejected = false;
    try {
        Resource bad("R-9", ResourceType::SupplyKit, "Store", -1, 0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CHECK(rejected);
}

void t09_assignAvailable()
{
    World w;
    CHECK(w.em.add(makeCase("E-9001", Severity::Critical, 2, Category::Medical)).empty());
    CHECK(w.rm.add(Resource("R-901", ResourceType::Ambulance, "Test Street", 2)).empty());
    CHECK(w.rm.add(Resource("R-902", ResourceType::FireEngine, "Depot", 6)).empty());
    const auto suitable = w.rm.suitableFor(*w.em.find("E-9001"));
    CHECK(suitable.size() == 1);
    CHECK(suitable.front().resource->id() == "R-901");
    CHECK(suitable.front().sameArea);

    const OpResult result = w.dispatcher.assign("E-9001", "R-901", T0 + 5 * MIN);
    CHECK(result.ok);
    CHECK(w.em.find("E-9001")->status() == Status::Assigned);
    CHECK(w.em.find("E-9001")->assignedAt() == T0 + 5 * MIN);
    CHECK(w.rm.find("R-901")->status() == ResourceStatus::Deployed);
    CHECK(w.rm.history().size() == 1);
    CHECK(w.rm.history().front().isActive());
    CHECK(w.rm.capacityAssignedTo("E-9001") == 2);
}

void t10_rejectUnavailable()
{
    World w;
    CHECK(w.em.add(makeCase("E-1101", Severity::High)).empty());
    CHECK(w.rm.add(Resource("R-111", ResourceType::Ambulance, "Base", 2)).empty());
    CHECK(w.rm.setOutOfService("R-111", true).empty());
    const OpResult result = w.dispatcher.assign("E-1101", "R-111", T0);
    CHECK(!result.ok);
    CHECK(result.message.find("out of service") != std::string::npos);
    CHECK(w.em.find("E-1101")->status() == Status::Pending);
    CHECK(w.rm.suitableFor(*w.em.find("E-1101")).empty());
    CHECK(!w.dispatcher.assign("E-1101", "R-NONE", T0).ok);
    CHECK(!w.dispatcher.assign("E-NONE", "R-111", T0).ok);
    CHECK(w.rm.setOutOfService("R-111", false).empty());
    CHECK(w.dispatcher.assign("E-1101", "R-111", T0).ok);
    CHECK(!w.rm.setOutOfService("R-111", true).empty());
    CHECK(w.em.add(makeCase("E-1102", Severity::Low)).empty());
    CHECK(w.dispatcher.cancel("E-1102", T0).ok);
    CHECK(w.rm.add(Resource("R-112", ResourceType::Ambulance, "Base", 2)).empty());
    CHECK(!w.dispatcher.assign("E-1102", "R-112", T0).ok);
}

void t11_preventDuplicateAssignment()
{
    World w;
    CHECK(w.em.add(makeCase("E-1201", Severity::High)).empty());
    CHECK(w.em.add(makeCase("E-1202", Severity::High)).empty());
    CHECK(w.em.add(makeCase("E-1203", Severity::High)).empty());
    CHECK(w.rm.add(Resource("R-121", ResourceType::Ambulance, "Base", 2)).empty());
    CHECK(w.rm.add(Resource("R-122", ResourceType::SupplyKit, "Store", 25, 2)).empty());

    CHECK(w.dispatcher.assign("E-1201", "R-121", T0).ok);
    CHECK(!w.dispatcher.assign("E-1201", "R-121", T0).ok);
    const OpResult second = w.dispatcher.assign("E-1202", "R-121", T0);
    CHECK(!second.ok);
    CHECK(second.message.find("already deployed") != std::string::npos);
    CHECK(w.em.find("E-1202")->status() == Status::Pending);

    CHECK(w.dispatcher.assign("E-1201", "R-122", T0).ok);
    CHECK(w.dispatcher.assign("E-1202", "R-122", T0).ok);
    CHECK(!w.dispatcher.assign("E-1203", "R-122", T0).ok);
    CHECK(w.rm.find("R-122")->statusLabel() == "Deployed (2/2)");
}

void t12_resolveAndRelease()
{
    World w;
    CHECK(w.em.add(makeCase("E-1301", Severity::Critical, 3, Category::Fire)).empty());
    CHECK(w.rm.add(Resource("R-131", ResourceType::FireEngine, "Station", 6)).empty());
    CHECK(w.rm.add(Resource("R-132", ResourceType::Ambulance, "Station", 2)).empty());
    CHECK(w.dispatcher.assign("E-1301", "R-131", T0 + 2 * MIN).ok);
    CHECK(w.dispatcher.assign("E-1301", "R-132", T0 + 3 * MIN).ok);
    CHECK(!w.dispatcher.resolve("E-1301", T0 + 4 * MIN).ok);
    CHECK(w.dispatcher.startOperation("E-1301", T0 + 10 * MIN).ok);
    CHECK(w.em.find("E-1301")->status() == Status::InProgress);
    CHECK(w.dispatcher.release("E-1301", "R-132", T0 + 20 * MIN).ok);
    CHECK(!w.dispatcher.release("E-1301", "R-131", T0 + 21 * MIN).ok);
    CHECK(w.dispatcher.resolve("E-1301", T0 + 60 * MIN).ok);
    const Emergency* e = w.em.find("E-1301");
    CHECK(e->status() == Status::Resolved);
    CHECK(e->closedAt() == T0 + 60 * MIN);
    CHECK(w.rm.find("R-131")->status() == ResourceStatus::Available);
    CHECK(w.rm.assignedTo("E-1301").empty());
    for (const AssignmentRecord& r : w.rm.history()) {
        CHECK(!r.isActive());
    }
    CHECK(!w.dispatcher.resolve("E-1301", T0 + 61 * MIN).ok);
    CHECK(w.em.add(makeCase("E-1302", Severity::Medium)).empty());
    CHECK(w.dispatcher.assign("E-1302", "R-132", T0).ok);
    CHECK(w.dispatcher.release("E-1302", "R-132", T0 + MIN).ok);
    CHECK(w.em.find("E-1302")->status() == Status::Pending);
}

void t13_restartAndVerify()
{
    const std::string dir = tempDir("restart");
    {
        World w;
        CHECK(w.em.add(Emergency("E-1401", "Meera | Pipe \\ Test", Category::Rescue, "Hill Top", "+91 90000 11111",
                                 "Line with | pipe", Severity::High, 6, T0))
                  .empty());
        CHECK(w.em.add(makeCase("E-1402", Severity::Low)).empty());
        CHECK(w.rm.add(Resource("R-141", ResourceType::RescueTeam, "Base", 8)).empty());
        CHECK(w.rm.add(Resource("R-142", ResourceType::SupplyKit, "Store", 25, 3)).empty());
        CHECK(w.rm.setOutOfService("R-142", true).empty());
        CHECK(w.dispatcher.assign("E-1401", "R-141", T0 + MIN).ok);
        CHECK(w.dispatcher.startOperation("E-1401", T0 + 2 * MIN).ok);
        FileManager files(dir);
        std::string error;
        CHECK(files.saveAll(w.em, w.rm, error));
        CHECK(error.empty());
        const std::string raw = readAll(files.emergenciesPath());
        CHECK(raw.find("90000 11111") == std::string::npos);
        CHECK(raw.find("Meera") == std::string::npos);
        CHECK(raw.find("#END|2|") != std::string::npos);
        CHECK(files.saveAll(w.em, w.rm, error));
        CHECK(exists(files.emergenciesPath() + ".bak"));
    }
    World w;
    FileManager files(dir);
    for (int pass = 0; pass < 2; ++pass) {
        const LoadReport report = files.loadAll(w.em, w.rm);
        CHECK(report.warnings.empty());
        CHECK(report.emergenciesLoaded == 2);
        CHECK(report.resourcesLoaded == 2);
        CHECK(report.assignmentsLoaded == 1);
        CHECK(w.em.size() == 2);
        CHECK(w.rm.size() == 2);
    }
    CHECK(w.dispatcher.reconcile(T0 + 3 * MIN).empty());
    const Emergency* e = w.em.find("E-1401");
    CHECK(e != nullptr);
    CHECK(e->reporter() == "Meera | Pipe \\ Test");
    CHECK(e->contact() == "+91 90000 11111");
    CHECK(e->description() == "Line with | pipe");
    CHECK(e->status() == Status::InProgress);
    CHECK(e->assignedAt() == T0 + MIN);
    CHECK(e->startedAt() == T0 + 2 * MIN);
    CHECK(w.rm.find("R-141")->isAssignedTo("E-1401"));
    CHECK(w.rm.find("R-142")->isOutOfService());
    CHECK(w.rm.find("R-142")->maxConcurrent() == 3);
    removeDir(dir);
}

void t14_noDataFiles()
{
    const std::string dir = tempDir("empty");
    World w;
    FileManager files(dir);
    const LoadReport report = files.loadAll(w.em, w.rm);
    CHECK(!report.anyFileFound());
    CHECK(report.warnings.empty());
    CHECK(w.em.empty());
    CHECK(w.rm.empty());
    CHECK(w.em.highestPriorityPending(T0) == nullptr);
    CHECK(AnalyticsManager(w.em, w.rm).compute().utilizationPercent() < 0);
    std::string error;
    CHECK(files.saveAll(w.em, w.rm, error));
    CHECK(exists(files.resourcesPath()));
    std::ofstream(files.emergenciesPath(), std::ios::trunc).close();
    const LoadReport again = files.loadAll(w.em, w.rm);
    CHECK(again.warnings.empty());
    removeDir(dir);
}

void t15_invalidInput()
{
    {
        std::istringstream in("abc\n-5\n99\n 3 \n");
        std::ostringstream out;
        Console console(in, out, false, false, false, false, true, false);
        CHECK(console.readNumber("Pick", 1, 5) == 3);
        CHECK(out.str().find("whole number") != std::string::npos);
    }
    {
        std::istringstream in("\n   \nValid text\n");
        std::ostringstream out;
        Console console(in, out, false, false, false, false, true, false);
        CHECK(console.readText("Name", 20) == "Valid text");
    }
    {
        std::istringstream in("maybe\nY\n");
        std::ostringstream out;
        Console console(in, out, false, false, false, false, true, false);
        CHECK(console.confirm("Sure?"));
    }
    {
        std::istringstream in(":q\n");
        std::ostringstream out;
        Console console(in, out, false, false, false, false, true, false);
        bool cancelled = false;
        try {
            console.readText("Name", 20);
        } catch (const InputCancelled&) {
            cancelled = true;
        }
        CHECK(cancelled);
    }
    {
        std::istringstream in("");
        std::ostringstream out;
        Console console(in, out, false, false, false, false, true, false);
        bool closed = false;
        try {
            console.readNumber("Pick", 1, 5);
        } catch (const InputClosed&) {
            closed = true;
        }
        CHECK(closed);
    }
    const std::string dir = tempDir("malformed");
    CHECK(resq_make_dirs(dir.c_str()) == 1);
    {
        std::ofstream out(dir + "/emergencies.txt");
        out << "RESQ|EMERGENCIES|1\n"
            << "# hand-edited file\n"
            << "E-1501|Asha|FIRE|Old Market|98765 43210|Smoke|CRITICAL|3|PENDING|1790000000|0|0|0|0\n"
            << "E-1502|too|few|fields\n"
            << "E-1503|Ravi|VOLCANO|Hill|98765 43210|Lava|HIGH|1|PENDING|1790000000|0|0|0|0\n"
            << "E-1504|Ravi|FIRE|Hill|98765 43210|Smoke|HIGH|abc|PENDING|1790000000|0|0|0|0\n"
            << "E-1501|Dup|FIRE|Old Market|98765 43210|Smoke|LOW|3|PENDING|1790000000|0|0|0|0\n"
            << "#END|5|DEADBEEF\n";
        std::ofstream res(dir + "/resources.txt");
        res << "SOMETHING ELSE\nR-1|x\n";
    }
    World w;
    FileManager files(dir);
    const LoadReport report = files.loadAll(w.em, w.rm);
    CHECK(report.emergenciesLoaded == 1);
    CHECK(w.em.exists("E-1501"));
    CHECK(w.em.find("E-1501")->severity() == Severity::Critical);
    CHECK(report.warnings.size() >= 5);
    CHECK(report.resourcesLoaded == 0);
    CHECK(report.preserved.size() == 2);
    for (const std::string& path : report.preserved) {
        CHECK(exists(path));
    }
    long long n = 0;
    CHECK(!util::parseInteger("12a", n));
    CHECK(!util::parseInteger("", n));
    CHECK(util::parseInteger("-42", n) && n == -42);
    std::time_t t = 0;
    CHECK(!util::parseDateTime("2026-02-30 10:00", t));
    CHECK(!util::parseDateTime("18/10/2026", t));
    CHECK(util::parseDateTime("2026-10-18 14:30", t));
    removeDir(dir, report.preserved);
}

void t16_reportsFromRecords()
{
    World w;
    CHECK(w.em.add(makeCase("E-1601", Severity::Critical, 2, Category::Fire, T0)).empty());
    CHECK(w.em.add(makeCase("E-1602", Severity::Critical, 1, Category::Medical, T0)).empty());
    CHECK(w.em.add(makeCase("E-1603", Severity::Low, 1, Category::Medical, T0)).empty());
    CHECK(w.rm.add(Resource("R-161", ResourceType::FireEngine, "Base", 6)).empty());
    CHECK(w.rm.add(Resource("R-162", ResourceType::Ambulance, "Base", 2)).empty());
    CHECK(w.rm.add(Resource("R-163", ResourceType::Ambulance, "Base", 2)).empty());
    CHECK(w.rm.setOutOfService("R-163", true).empty());
    CHECK(w.dispatcher.assign("E-1601", "R-161", T0 + 10 * MIN).ok);
    CHECK(w.dispatcher.startOperation("E-1601", T0 + 15 * MIN).ok);
    CHECK(w.dispatcher.resolve("E-1601", T0 + 70 * MIN).ok);
    CHECK(w.dispatcher.assign("E-1602", "R-162", T0 + 20 * MIN).ok);

    const AnalyticsManager analytics(w.em, w.rm);
    const Statistics s = analytics.compute();
    CHECK(s.totalEmergencies == 3);
    CHECK(s.count(Status::Resolved) == 1);
    CHECK(s.count(Status::Assigned) == 1);
    CHECK(s.count(Status::Pending) == 1);
    CHECK(s.bySeverity[static_cast<size_t>(indexOf(Severity::Critical))] == 2);
    CHECK(s.byCategory[static_cast<size_t>(indexOf(Category::Medical))] == 2);
    CHECK(s.activeCritical == 1);
    CHECK(s.totalResources == 3);
    CHECK(s.deployedResources == 1);
    CHECK(s.outOfServiceResources == 1);
    CHECK(static_cast<int>(s.utilizationPercent()) == 50);
    CHECK(s.responseSamples == 2);
    CHECK(static_cast<long long>(s.averageResponseSeconds) == 15 * MIN);
    CHECK(s.resolutionSamples == 1);
    CHECK(static_cast<long long>(s.averageResolutionSeconds) == 70 * MIN);
    CHECK(s.totalAssignmentsLogged == 2);
    CHECK(analytics.mostDeployed(5).size() == 2);
}

void tUnitHelpers()
{
    const char* check = "123456789";
    CHECK(resq_crc32(0, check, std::strlen(check)) == 0xCBF43926u);
    const std::string secret = "+91 98765 43210";
    const std::string hidden = util::obfuscate(secret);
    CHECK(hidden.rfind("X1:", 0) == 0);
    CHECK(hidden.find("98765") == std::string::npos);
    std::string back;
    CHECK(util::reveal(hidden, back) && back == secret);
    CHECK(!util::reveal("X1:ZZ", back));
    const std::string field = "a|b\\c";
    CHECK(util::splitEscaped(util::escapeField(field) + "|x").size() == 2);
    CHECK(util::splitEscaped(util::escapeField(field)).front() == field);
    CHECK(util::maskContact("9876543210") == "*******210");
    CHECK(util::formatDuration(3 * 3600 + 5 * 60) == "3h 05m");
    CHECK(util::wrapText("one two three four", 9).size() == 3);
    CHECK(util::wrapText("one two three four", 9).front() == "one two");
}

void tDemoData()
{
    World w;
    CHECK(w.em.add(makeCase("E-1001", Severity::High)).empty());
    const DemoResult first = DemoData::load(w.em, w.rm, w.dispatcher, T0);
    CHECK(first.emergenciesAdded == 10);
    CHECK(first.resourcesAdded == 12);
    CHECK(w.em.find("E-D06")->status() == Status::InProgress);
    CHECK(w.em.find("E-D07")->status() == Status::Resolved);
    CHECK(w.dispatcher.reconcile(T0).empty());
    const DemoResult second = DemoData::load(w.em, w.rm, w.dispatcher, T0);
    CHECK(second.emergenciesAdded == 0);
    CHECK(second.skipped == 22);
    CHECK(w.dispatcher.assign("E-1001", "R-D04", T0).ok);
    CHECK(DemoData::removeAll(w.em, w.rm, w.dispatcher, T0) == 22);
    CHECK(w.em.size() == 1);
    CHECK(w.rm.empty());
    CHECK(w.em.find("E-1001")->status() == Status::Pending);
    CHECK(w.rm.history().empty());
}

}

int main()
{
    std::cout << "RESQ automated tests\n====================\n";
    run("T01 Register a valid emergency (and reject invalid fields)", t01_registerValid);
    run("T02 Reject a duplicate emergency ID", t02_rejectDuplicateId);
    run("T03 Register emergencies with different severities", t03_multipleSeverities);
    run("T04 Verify priority ordering", t04_priorityOrdering);
    run("T05 Search for an existing emergency", t05_searchExisting);
    run("T06 Search for a nonexistent emergency", t06_searchNonexistent);
    run("T07 Update emergency information", t07_updateEmergency);
    run("T08 Add and search for a resource", t08_addAndSearchResource);
    run("T09 Assign an available resource", t09_assignAvailable);
    run("T10 Reject an unavailable resource", t10_rejectUnavailable);
    run("T11 Prevent invalid duplicate assignments", t11_preventDuplicateAssignment);
    run("T12 Resolve an emergency and release its resources", t12_resolveAndRelease);
    run("T13 Save, restart (reload) and verify records", t13_restartAndVerify);
    run("T14 Start with no existing data files", t14_noDataFiles);
    run("T15 Handle invalid input and malformed files", t15_invalidInput);
    run("T16 Generate reports from stored records", t16_reportsFromRecords);
    run("U01 Helpers: CRC-32, obfuscation, escaping", tUnitHelpers);
    run("U02 Demo data load / reset safety", tDemoData);

    std::cout << "\n" << g_checks << " checks, " << g_failures << " failed; " << g_testsFailed
              << " test(s) failed.\n";
    std::cout << (g_failures == 0 ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return g_failures == 0 ? 0 : 1;
}
