#include "types.h"

namespace resq {

namespace {

template <typename E>
bool matchCode(const std::vector<E>& values, const std::string& code, E& out)
{
    for (E value : values) {
        if (toCode(value) == code) {
            out = value;
            return true;
        }
    }
    return false;
}

template <typename E>
bool matchNumber(const std::vector<E>& values, int number, E& out)
{
    if (number < 1 || number > static_cast<int>(values.size())) {
        return false;
    }
    out = values[static_cast<size_t>(number - 1)];
    return true;
}

}

const std::vector<Category>& allCategories()
{
    static const std::vector<Category> values{Category::Medical, Category::Fire, Category::RoadAccident,
                                              Category::NaturalDisaster, Category::Flood, Category::Rescue,
                                              Category::Other};
    return values;
}

const std::vector<Severity>& allSeverities()
{
    static const std::vector<Severity> values{Severity::Critical, Severity::High, Severity::Medium, Severity::Low};
    return values;
}

const std::vector<Status>& allStatuses()
{
    static const std::vector<Status> values{Status::Pending, Status::Assigned, Status::InProgress, Status::Resolved,
                                            Status::Cancelled};
    return values;
}

const std::vector<ResourceType>& allResourceTypes()
{
    static const std::vector<ResourceType> values{ResourceType::Ambulance,  ResourceType::MedicalTeam,
                                                  ResourceType::FireEngine, ResourceType::RescueTeam,
                                                  ResourceType::ReliefVehicle, ResourceType::SupplyKit};
    return values;
}

const std::vector<ResourceStatus>& allResourceStatuses()
{
    static const std::vector<ResourceStatus> values{ResourceStatus::Available, ResourceStatus::Deployed,
                                                    ResourceStatus::OutOfService};
    return values;
}

std::string toString(Category value)
{
    switch (value) {
    case Category::Medical: return "Medical emergency";
    case Category::Fire: return "Fire";
    case Category::RoadAccident: return "Road accident";
    case Category::NaturalDisaster: return "Natural disaster";
    case Category::Flood: return "Flood";
    case Category::Rescue: return "Rescue operation";
    case Category::Other: return "Other";
    }
    return "Unknown";
}

std::string toString(Severity value)
{
    switch (value) {
    case Severity::Critical: return "Critical";
    case Severity::High: return "High";
    case Severity::Medium: return "Medium";
    case Severity::Low: return "Low";
    }
    return "Unknown";
}

std::string toString(Status value)
{
    switch (value) {
    case Status::Pending: return "Pending";
    case Status::Assigned: return "Assigned";
    case Status::InProgress: return "In Progress";
    case Status::Resolved: return "Resolved";
    case Status::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

std::string toString(ResourceType value)
{
    switch (value) {
    case ResourceType::Ambulance: return "Ambulance";
    case ResourceType::MedicalTeam: return "Medical team";
    case ResourceType::FireEngine: return "Fire engine";
    case ResourceType::RescueTeam: return "Rescue team";
    case ResourceType::ReliefVehicle: return "Disaster-relief vehicle";
    case ResourceType::SupplyKit: return "Emergency supply kit";
    }
    return "Unknown";
}

std::string toString(ResourceStatus value)
{
    switch (value) {
    case ResourceStatus::Available: return "Available";
    case ResourceStatus::Deployed: return "Deployed";
    case ResourceStatus::OutOfService: return "Out of service";
    }
    return "Unknown";
}

std::string shortName(Category value)
{
    switch (value) {
    case Category::Medical: return "Medical";
    case Category::Fire: return "Fire";
    case Category::RoadAccident: return "Road acc.";
    case Category::NaturalDisaster: return "Nat.disaster";
    case Category::Flood: return "Flood";
    case Category::Rescue: return "Rescue op.";
    case Category::Other: return "Other";
    }
    return "Unknown";
}

std::string shortName(ResourceType value)
{
    switch (value) {
    case ResourceType::Ambulance: return "Ambulance";
    case ResourceType::MedicalTeam: return "Medical team";
    case ResourceType::FireEngine: return "Fire engine";
    case ResourceType::RescueTeam: return "Rescue team";
    case ResourceType::ReliefVehicle: return "Relief vehicle";
    case ResourceType::SupplyKit: return "Supply kit";
    }
    return "Unknown";
}

std::string toCode(Category value)
{
    switch (value) {
    case Category::Medical: return "MEDICAL";
    case Category::Fire: return "FIRE";
    case Category::RoadAccident: return "ROAD_ACCIDENT";
    case Category::NaturalDisaster: return "NATURAL_DISASTER";
    case Category::Flood: return "FLOOD";
    case Category::Rescue: return "RESCUE";
    case Category::Other: return "OTHER";
    }
    return "UNKNOWN";
}

std::string toCode(Severity value)
{
    switch (value) {
    case Severity::Critical: return "CRITICAL";
    case Severity::High: return "HIGH";
    case Severity::Medium: return "MEDIUM";
    case Severity::Low: return "LOW";
    }
    return "UNKNOWN";
}

std::string toCode(Status value)
{
    switch (value) {
    case Status::Pending: return "PENDING";
    case Status::Assigned: return "ASSIGNED";
    case Status::InProgress: return "IN_PROGRESS";
    case Status::Resolved: return "RESOLVED";
    case Status::Cancelled: return "CANCELLED";
    }
    return "UNKNOWN";
}

std::string toCode(ResourceType value)
{
    switch (value) {
    case ResourceType::Ambulance: return "AMBULANCE";
    case ResourceType::MedicalTeam: return "MEDICAL_TEAM";
    case ResourceType::FireEngine: return "FIRE_ENGINE";
    case ResourceType::RescueTeam: return "RESCUE_TEAM";
    case ResourceType::ReliefVehicle: return "RELIEF_VEHICLE";
    case ResourceType::SupplyKit: return "SUPPLY_KIT";
    }
    return "UNKNOWN";
}

bool fromCode(const std::string& code, Category& out) { return matchCode(allCategories(), code, out); }
bool fromCode(const std::string& code, Severity& out) { return matchCode(allSeverities(), code, out); }
bool fromCode(const std::string& code, Status& out) { return matchCode(allStatuses(), code, out); }
bool fromCode(const std::string& code, ResourceType& out) { return matchCode(allResourceTypes(), code, out); }

bool fromNumber(int number, Category& out) { return matchNumber(allCategories(), number, out); }
bool fromNumber(int number, Severity& out) { return matchNumber(allSeverities(), number, out); }
bool fromNumber(int number, Status& out) { return matchNumber(allStatuses(), number, out); }
bool fromNumber(int number, ResourceType& out) { return matchNumber(allResourceTypes(), number, out); }

}
