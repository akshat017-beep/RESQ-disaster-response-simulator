#include "analytics.h"
#include "console.h"
#include "file_manager.h"
#include "resq_c.h"

#include <cstdio>
#include <fstream>
#include <sstream>

using namespace resq;

namespace {

int failures = 0;
int checks = 0;

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        ++checks;                                                                                                      \
        if (!(condition)) {                                                                                            \
            ++failures;                                                                                                \
            std::cout << "    FAILED line " << __LINE__ << ": " #condition << '\n';                                    \
        }                                                                                                              \
    } while (0)

const std::time_t T = 1790000000;

Emergency make(const std::string& id, Severity s, int people = 1, Category c = Category::Medical, std::time_t at = T) {
    return Emergency(id, "Tester", c, "Test Street", "98765 43210", "Test case", s, people, at);
}

void run(const char* title, void (*test)()) {
    const int before = failures;
    test();
    std::cout << (failures == before ? "[PASS] " : "[FAIL] ") << title << '\n';
}

void cleanFolder(const std::string& folder) {
    resq_make_dir(folder.c_str());
    for (const char* file : {"emergencies.txt", "resources.txt"})
        for (const char* suffix : {"", ".bak", ".tmp", ".damaged"}) std::remove((folder + "/" + file + suffix).c_str());
}

}

int main() {
    run("T01 register a valid emergency", [] {
        EmergencyManager em;
        CHECK(em.add(make("E-1001", Severity::High)).empty());
        CHECK(em.find("e-1001") != nullptr && em.find("E-1001")->status() == Status::Pending);
        CHECK(em.nextId() == "E-1002");
        bool rejected = false;
        try {
            Emergency("E1", "", Category::Fire, "", "12", "", Severity::Low, -1, T);
        } catch (const std::exception&) {
            rejected = true;
        }
        CHECK(rejected);
    });
    run("T02 reject a duplicate ID", [] {
        EmergencyManager em;
        CHECK(em.add(make("E-2001", Severity::High)).empty());
        CHECK(!em.add(make("E-2001", Severity::Low)).empty());
        CHECK(em.size() == 1 && em.find("E-2001")->severity() == Severity::High);
    });
    run("T03 different severity levels", [] {
        EmergencyManager em;
        em.add(make("E-3001", Severity::Low));
        em.add(make("E-3002", Severity::Critical));
        em.add(make("E-3003", Severity::Medium));
        const auto list = em.sorted(SortBy::Severity);
        CHECK(list[0]->id() == "E-3002" && list[1]->id() == "E-3003" && list[2]->id() == "E-3001");
    });
    run("T04 priority ordering", [] {
        EmergencyManager em;
        em.add(make("E-4001", Severity::Low, 5000, Category::Fire, T - 36000));
        em.add(make("E-4002", Severity::Critical, 1, Category::Other, T));
        em.add(make("E-4003", Severity::High, 2, Category::Medical, T - 3600));
        em.add(make("E-4004", Severity::High, 2, Category::Medical, T - 300));
        const auto queue = em.queue(T);
        CHECK(queue[0]->id() == "E-4002" && queue[1]->id() == "E-4003" && queue[2]->id() == "E-4004" &&
              queue[3]->id() == "E-4001");
        CHECK(em.find("E-4003")->priority(T).total() == 300 + 16 + 20 + 8);
    });
    run("T05 search for an existing emergency", [] {
        EmergencyManager em;
        em.add(
            Emergency("E-5001", "Asha Rao", Category::Flood, "Lakeview", "12345 67890", "Water", Severity::High, 3, T));
        CHECK(em.search(1, "5001").size() == 1 && em.search(2, "asha").size() == 1);
        CHECK(em.search(3, "LAKE").size() == 1 && em.byCategory(Category::Flood).size() == 1);
    });
    run("T06 search for a nonexistent emergency", [] {
        EmergencyManager em;
        em.add(make("E-6001", Severity::Low));
        CHECK(em.search(1, "X-404").empty() && em.search(2, "Nobody").empty() && em.find("E-0000") == nullptr);
    });
    run("T07 update an emergency", [] {
        EmergencyManager em;
        ResourceManager rm;
        em.add(make("E-7001", Severity::Low));
        Emergency& e = *em.find("E-7001");
        const int before = e.score();
        e.setSeverity(Severity::Critical);
        e.refresh(T);
        CHECK(e.score() == before + 300);
        e.setLocation("New Road");
        e.setAffected(15);
        CHECK(e.location() == "New Road" && e.affected() == 15);
        CHECK(!Emergency::canMove(Status::Pending, Status::Resolved));
        CHECK(rm.changeStatus(e, Status::Cancelled, T).empty());
        bool locked = false;
        try {
            e.setSeverity(Severity::Low);
        } catch (const std::exception&) {
            locked = true;
        }
        CHECK(locked);
    });
    run("T08 add and search a resource", [] {
        ResourceManager rm;
        CHECK(rm.add(Resource("R-101", ResourceType::Ambulance, "Riverside", 2)).empty());
        CHECK(!rm.add(Resource("R-101", ResourceType::FireEngine, "Depot", 6)).empty());
        CHECK(rm.search(1, "R-101").size() == 1 && rm.search(3, "river").size() == 1 && rm.nextId() == "R-102");
    });
    run("T09 assign an available resource", [] {
        EmergencyManager em;
        ResourceManager rm;
        em.add(make("E-9001", Severity::Critical));
        rm.add(Resource("R-901", ResourceType::Ambulance, "Base", 2));
        rm.add(Resource("R-902", ResourceType::FireEngine, "Base", 6));
        Emergency& e = *em.find("E-9001");
        CHECK(rm.suitableFor(e).size() == 1);
        CHECK(rm.assign(e, "R-901", T + 60).empty());
        CHECK(e.status() == Status::Assigned && rm.find("R-901")->statusText() == "Deployed (1/1)");
    });
    run("T10 reject an unavailable resource", [] {
        EmergencyManager em;
        ResourceManager rm;
        em.add(make("E-1101", Severity::High));
        rm.add(Resource("R-111", ResourceType::Ambulance, "Base", 2));
        rm.find("R-111")->setOutOfService(true);
        Emergency& e = *em.find("E-1101");
        CHECK(rm.assign(e, "R-111", T).find("out of service") != std::string::npos);
        CHECK(e.status() == Status::Pending && !rm.assign(e, "R-NONE", T).empty());
    });
    run("T11 prevent duplicate assignments", [] {
        EmergencyManager em;
        ResourceManager rm;
        em.add(make("E-1201", Severity::High));
        em.add(make("E-1202", Severity::High));
        em.add(make("E-1203", Severity::High));
        rm.add(Resource("R-121", ResourceType::Ambulance, "Base", 2));
        rm.add(Resource("R-122", ResourceType::SupplyKit, "Store", 25, 2));
        CHECK(rm.assign(*em.find("E-1201"), "R-121", T).empty());
        CHECK(!rm.assign(*em.find("E-1201"), "R-121", T).empty());
        CHECK(!rm.assign(*em.find("E-1202"), "R-121", T).empty());
        CHECK(rm.assign(*em.find("E-1201"), "R-122", T).empty() && rm.assign(*em.find("E-1202"), "R-122", T).empty());
        CHECK(!rm.assign(*em.find("E-1203"), "R-122", T).empty());
    });
    run("T12 resolve and release resources", [] {
        EmergencyManager em;
        ResourceManager rm;
        em.add(make("E-1301", Severity::Critical, 3, Category::Fire));
        rm.add(Resource("R-131", ResourceType::FireEngine, "Station", 6));
        Emergency& e = *em.find("E-1301");
        rm.assign(e, "R-131", T + 60);
        CHECK(!rm.changeStatus(e, Status::Resolved, T + 120).empty());
        CHECK(rm.changeStatus(e, Status::InProgress, T + 120).empty());
        CHECK(!rm.release(e, "R-131", T + 180).empty());
        CHECK(rm.changeStatus(e, Status::Resolved, T + 3600).empty());
        CHECK(e.status() == Status::Resolved && rm.find("R-131")->statusText() == "Available");
    });
    run("T13 save, restart and verify records", [] {
        cleanFolder("resq_test");
        {
            EmergencyManager em;
            ResourceManager rm;
            em.add(Emergency("E-1401", "Meera | Test", Category::Rescue, "Hill Top", "+91 90000 11111", "Pipe | text",
                             Severity::High, 6, T));
            rm.add(Resource("R-141", ResourceType::RescueTeam, "Base", 8));
            rm.assign(*em.find("E-1401"), "R-141", T + 60);
            std::string error;
            CHECK(FileManager("resq_test").save(em, rm, error));
            std::ifstream in("resq_test/emergencies.txt");
            std::stringstream text;
            text << in.rdbuf();
            CHECK(text.str().find("90000 11111") == std::string::npos);
        }
        EmergencyManager em;
        ResourceManager rm;
        FileManager files("resq_test");
        for (int pass = 0; pass < 2; ++pass)
            CHECK(files.load(em, rm, T + 120).empty() && em.size() == 1 && rm.size() == 1);
        const Emergency* e = em.find("E-1401");
        CHECK(e != nullptr && e->reporter() == "Meera | Test" && e->contact() == "+91 90000 11111");
        CHECK(e->status() == Status::Assigned && rm.find("R-141")->assignedTo("E-1401"));
        cleanFolder("resq_test");
    });
    run("T14 start without data files", [] {
        cleanFolder("resq_empty");
        EmergencyManager em;
        ResourceManager rm;
        FileManager files("resq_empty");
        CHECK(files.load(em, rm, T).empty() && !files.foundData() && em.size() == 0);
    });
    run("T15 invalid input and malformed files", [] {
        std::istringstream in("abc\n-5\n99\n3\n");
        std::ostringstream out;
        Console console(in, out, false, true);
        CHECK(console.number("Pick", 1, 5) == 3);
        std::istringstream empty("");
        Console closed(empty, out, false, true);
        bool threw = false;
        try {
            closed.number("Pick", 1, 5);
        } catch (const InputClosed&) {
            threw = true;
        }
        CHECK(threw);
        cleanFolder("resq_bad");
        std::ofstream("resq_bad/emergencies.txt")
            << "RESQ-EMERGENCIES-1\nE-1|bad\nE-1502|A|9|X|3939|D|1|1|1|1790000000|0|0|0|0\n";
        EmergencyManager em;
        ResourceManager rm;
        CHECK(FileManager("resq_bad").load(em, rm, T).size() >= 2 && em.size() == 0);
        cleanFolder("resq_bad");
    });
    run("T16 reports from stored records", [] {
        EmergencyManager em;
        ResourceManager rm;
        em.add(make("E-1601", Severity::Critical, 2, Category::Fire));
        em.add(make("E-1602", Severity::Low));
        rm.add(Resource("R-161", ResourceType::FireEngine, "Base", 6));
        rm.add(Resource("R-162", ResourceType::Ambulance, "Base", 2));
        Emergency& e = *em.find("E-1601");
        rm.assign(e, "R-161", T + 600);
        rm.changeStatus(e, Status::InProgress, T + 900);
        rm.changeStatus(e, Status::Resolved, T + 4200);
        const Stats s = AnalyticsManager::compute(em, rm);
        CHECK(s.total == 2 && s.count(Status::Resolved) == 1 && s.count(Status::Pending) == 1);
        CHECK(s.severity[0] == 1 && s.available == 2 && s.averageResolution == 4200 && s.averageResponse == 600);
    });
    std::cout << '\n' << checks << " checks, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
