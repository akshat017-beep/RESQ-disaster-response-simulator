#include "demo_data.h"

#include <initializer_list>
#include <set>
#include <string>

namespace resq {

namespace {

constexpr std::time_t kMinute = 60;
constexpr std::time_t kHour = 60 * kMinute;

struct DemoCase {
    const char* id;
    const char* reporter;
    Category category;
    const char* location;
    const char* contact;
    const char* description;
    Severity severity;
    int affected;
    std::time_t minutesAgo;
};

struct DemoResource {
    const char* id;
    ResourceType type;
    const char* location;
    int capacity;
    int maxConcurrent;
    bool outOfService;
};

const DemoCase kCases[] = {
    {"E-D01", "Riya Sharma (demo)", Category::Fire, "Old Market Square", "000-555-0101",
     "[FICTIONAL DEMO] Fire in a two-storey shop building; smoke on the upper floor.", Severity::Critical, 12, 25},
    {"E-D02", "Arjun Mehta (demo)", Category::Medical, "Riverside Ward 3", "000-555-0102",
     "[FICTIONAL DEMO] Elderly resident with chest pain, conscious and breathing.", Severity::High, 1, 40},
    {"E-D03", "Kavya Rao (demo)", Category::RoadAccident, "Hill Road Km 12", "000-555-0103",
     "[FICTIONAL DEMO] Two-car collision, four occupants, one trapped.", Severity::High, 4, 15},
    {"E-D04", "Imran Khan (demo)", Category::Flood, "Lakeview Colony", "000-555-0104",
     "[FICTIONAL DEMO] Water entering ground-floor homes after heavy rain.", Severity::Medium, 30, 90},
    {"E-D05", "Neha Joshi (demo)", Category::Other, "Central Park Gate 2", "000-555-0105",
     "[FICTIONAL DEMO] Fallen tree blocking the footpath, no injuries.", Severity::Low, 0, 10},
    {"E-D06", "Vikram Singh (demo)", Category::NaturalDisaster, "Northridge Village", "000-555-0106",
     "[FICTIONAL DEMO] Landslide damaged several houses; families displaced.", Severity::Critical, 60, 180},
    {"E-D07", "Ananya Gupta (demo)", Category::Medical, "Sector 9 Housing", "000-555-0107",
     "[FICTIONAL DEMO] Child with high fever and breathing difficulty.", Severity::Medium, 1, 300},
    {"E-D08", "Rahul Verma (demo)", Category::Fire, "Industrial Estate Plot 4", "000-555-0108",
     "[FICTIONAL DEMO] Small warehouse fire, contained to one unit.", Severity::High, 3, 26 * 60},
    {"E-D09", "Sara Thomas (demo)", Category::Rescue, "Quarry Lake", "000-555-0109",
     "[FICTIONAL DEMO] Two hikers stranded on a ledge above the lake.", Severity::Medium, 2, 60},
    {"E-D10", "Dev Patel (demo)", Category::RoadAccident, "Ring Road Junction 5", "000-555-0110",
     "[FICTIONAL DEMO] Minor scooter skid; duplicate of another report.", Severity::Low, 2, 8 * 60},
};

const DemoResource kResources[] = {
    {"R-D01", ResourceType::Ambulance, "Riverside", 2, 1, false},
    {"R-D02", ResourceType::Ambulance, "Old Market", 2, 1, false},
    {"R-D03", ResourceType::Ambulance, "Hill Road Depot", 2, 1, true},
    {"R-D04", ResourceType::MedicalTeam, "District Hospital", 10, 1, false},
    {"R-D05", ResourceType::FireEngine, "Old Market Fire Stn", 6, 1, false},
    {"R-D06", ResourceType::FireEngine, "Industrial Estate", 6, 1, false},
    {"R-D07", ResourceType::RescueTeam, "Northridge", 8, 1, false},
    {"R-D08", ResourceType::RescueTeam, "Central Depot", 8, 1, false},
    {"R-D09", ResourceType::ReliefVehicle, "Central Depot", 20, 1, false},
    {"R-D10", ResourceType::ReliefVehicle, "Lakeview", 20, 1, false},
    {"R-D11", ResourceType::SupplyKit, "Central Store", 25, 3, false},
    {"R-D12", ResourceType::MedicalTeam, "Riverside Clinic", 6, 1, false},
};

}

DemoResult DemoData::load(EmergencyManager& emergencies, ResourceManager& resources, Dispatcher& dispatcher,
                          std::time_t now)
{
    DemoResult result;
    std::set<std::string> addedCases;
    std::set<std::string> addedResources;

    for (const DemoResource& d : kResources) {
        if (resources.exists(d.id)) {
            ++result.skipped;
            continue;
        }
        Resource r(d.id, d.type, d.location, d.capacity, d.maxConcurrent, true);
        if (d.outOfService) {
            r.setOutOfService(true);
        }
        if (resources.add(std::move(r)).empty()) {
            ++result.resourcesAdded;
            addedResources.insert(d.id);
        }
    }
    for (const DemoCase& d : kCases) {
        if (emergencies.exists(d.id)) {
            ++result.skipped;
            continue;
        }
        Emergency e(d.id, d.reporter, d.category, d.location, d.contact, d.description, d.severity, d.affected,
                    now - d.minutesAgo * kMinute, true);
        if (emergencies.add(std::move(e)).empty()) {
            ++result.emergenciesAdded;
            addedCases.insert(d.id);
        }
    }

    const auto ready = [&](const char* eid, std::initializer_list<const char*> rids) {
        if (addedCases.count(eid) == 0) {
            return false;
        }
        for (const char* rid : rids) {
            if (addedResources.count(rid) == 0) {
                return false;
            }
        }
        return true;
    };
    if (ready("E-D06", {"R-D07", "R-D09"})) {
        dispatcher.assign("E-D06", "R-D07", now - 170 * kMinute);
        dispatcher.assign("E-D06", "R-D09", now - 165 * kMinute);
        dispatcher.startOperation("E-D06", now - 150 * kMinute);
    }
    if (ready("E-D07", {"R-D01"})) {
        dispatcher.assign("E-D07", "R-D01", now - 290 * kMinute);
        dispatcher.startOperation("E-D07", now - 280 * kMinute);
        dispatcher.resolve("E-D07", now - 4 * kHour);
    }
    if (ready("E-D08", {"R-D06", "R-D02"})) {
        dispatcher.assign("E-D08", "R-D06", now - (26 * kHour - 6 * kMinute));
        dispatcher.assign("E-D08", "R-D02", now - (26 * kHour - 8 * kMinute));
        dispatcher.startOperation("E-D08", now - (25 * kHour + 45 * kMinute));
        dispatcher.resolve("E-D08", now - 23 * kHour);
    }
    if (ready("E-D09", {"R-D08"})) {
        dispatcher.assign("E-D09", "R-D08", now - 50 * kMinute);
    }
    if (ready("E-D10", {})) {
        dispatcher.cancel("E-D10", now - (8 * kHour - 10 * kMinute));
    }
    emergencies.recalculateAll(now);
    return result;
}

int DemoData::removeAll(EmergencyManager& emergencies, ResourceManager& resources, Dispatcher& dispatcher,
                        std::time_t now)
{
    const std::vector<std::string> caseIds = emergencies.demoIds();
    const std::vector<std::string> resourceIds = resources.demoIds();
    const std::set<std::string> demoCases(caseIds.begin(), caseIds.end());

    for (const std::string& rid : resourceIds) {
        const std::vector<std::string> assigned = resources.find(rid)->assignedEmergencies();
        for (const std::string& eid : assigned) {
            if (demoCases.count(eid) == 0) {
                dispatcher.release(eid, rid, now, true);
            }
        }
    }
    for (const std::string& eid : caseIds) {
        resources.releaseAll(eid, now, "Demo data removed");
    }

    std::set<std::string> removed;
    for (const std::string& eid : caseIds) {
        if (emergencies.remove(eid)) {
            removed.insert(eid);
        }
    }
    for (const std::string& rid : resourceIds) {
        if (resources.remove(rid)) {
            removed.insert(rid);
        }
    }
    resources.purgeHistory(removed);
    return static_cast<int>(removed.size());
}

bool DemoData::present(const EmergencyManager& emergencies, const ResourceManager& resources)
{
    return !emergencies.demoIds().empty() || !resources.demoIds().empty();
}

}
