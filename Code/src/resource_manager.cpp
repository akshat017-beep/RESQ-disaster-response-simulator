#include "resource_manager.h"

#include "util.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace resq {

namespace {

std::string resourceKey(const std::string& id) { return util::toUpper(util::trim(id)); }

bool sameArea(const std::string& a, const std::string& b)
{
    const std::string x = util::toLower(util::trim(a));
    const std::string y = util::toLower(util::trim(b));
    if (x.size() < 3 || y.size() < 3) {
        return x == y && !x.empty();
    }
    return x.find(y) != std::string::npos || y.find(x) != std::string::npos;
}

}

std::string ResourceManager::add(Resource resource)
{
    if (exists(resource.id())) {
        return "A resource with ID " + resource.id() + " already exists. IDs must be unique.";
    }
    const std::string id = resource.id();
    resources_.emplace(id, std::move(resource));
    return "";
}

bool ResourceManager::remove(const std::string& id) { return resources_.erase(resourceKey(id)) > 0; }

bool ResourceManager::exists(const std::string& id) const { return resources_.count(resourceKey(id)) > 0; }

Resource* ResourceManager::find(const std::string& id)
{
    const auto it = resources_.find(resourceKey(id));
    return it == resources_.end() ? nullptr : &it->second;
}

const Resource* ResourceManager::find(const std::string& id) const
{
    const auto it = resources_.find(resourceKey(id));
    return it == resources_.end() ? nullptr : &it->second;
}

std::vector<const Resource*> ResourceManager::all() const
{
    std::vector<const Resource*> result;
    result.reserve(resources_.size());
    for (const auto& entry : resources_) {
        result.push_back(&entry.second);
    }
    return result;
}

std::vector<const Resource*> ResourceManager::search(ResourceSearchField field, const std::string& query) const
{
    const std::string q = util::trim(query);
    std::vector<const Resource*> result;
    if (q.empty()) {
        return result;
    }
    if (field == ResourceSearchField::Id) {
        if (const Resource* exact = find(q)) {
            result.push_back(exact);
            return result;
        }
    }
    for (const auto& entry : resources_) {
        const Resource& r = entry.second;
        bool match = false;
        switch (field) {
        case ResourceSearchField::Id: match = util::icontains(r.id(), q); break;
        case ResourceSearchField::Type:
            match = util::icontains(toString(r.type()), q) || util::icontains(shortName(r.type()), q);
            break;
        case ResourceSearchField::Location: match = util::icontains(r.location(), q); break;
        case ResourceSearchField::Status: match = util::icontains(toString(r.status()), q); break;
        }
        if (match) {
            result.push_back(&r);
        }
    }
    return result;
}

std::vector<const Resource*> ResourceManager::byType(ResourceType type) const
{
    std::vector<const Resource*> result;
    for (const auto& entry : resources_) {
        if (entry.second.type() == type) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<const Resource*> ResourceManager::byStatus(ResourceStatus status) const
{
    std::vector<const Resource*> result;
    for (const auto& entry : resources_) {
        if (entry.second.status() == status) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<const Resource*> ResourceManager::assignedTo(const std::string& emergencyId) const
{
    std::vector<const Resource*> result;
    for (const auto& entry : resources_) {
        if (entry.second.isAssignedTo(resourceKey(emergencyId))) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<const Resource*> ResourceManager::acceptingAssignments() const
{
    std::vector<const Resource*> result;
    for (const auto& entry : resources_) {
        if (entry.second.canAccept()) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<std::string> ResourceManager::demoIds() const
{
    std::vector<std::string> ids;
    for (const auto& entry : resources_) {
        if (entry.second.isDemo()) {
            ids.push_back(entry.first);
        }
    }
    return ids;
}

std::vector<ResourceType> ResourceManager::recommendedTypes(Category category)
{
    using T = ResourceType;
    switch (category) {
    case Category::Medical: return {T::Ambulance, T::MedicalTeam};
    case Category::Fire: return {T::FireEngine, T::Ambulance, T::RescueTeam};
    case Category::RoadAccident: return {T::Ambulance, T::RescueTeam, T::MedicalTeam};
    case Category::NaturalDisaster: return {T::RescueTeam, T::ReliefVehicle, T::MedicalTeam, T::SupplyKit};
    case Category::Flood: return {T::RescueTeam, T::ReliefVehicle, T::SupplyKit, T::Ambulance};
    case Category::Rescue: return {T::RescueTeam, T::MedicalTeam, T::Ambulance};
    case Category::Other: return allResourceTypes();
    }
    return allResourceTypes();
}

bool ResourceManager::isRecommended(ResourceType type, Category category)
{
    const std::vector<ResourceType> types = recommendedTypes(category);
    return std::find(types.begin(), types.end(), type) != types.end();
}

std::vector<SuitableResource> ResourceManager::suitableFor(const Emergency& emergency) const
{
    const std::vector<ResourceType> types = recommendedTypes(emergency.category());
    std::vector<SuitableResource> result;
    for (const auto& entry : resources_) {
        const Resource& r = entry.second;
        if (!r.canAccept() || r.isAssignedTo(emergency.id())) {
            continue;
        }
        const auto it = std::find(types.begin(), types.end(), r.type());
        if (it == types.end()) {
            continue;
        }
        SuitableResource s;
        s.resource = &r;
        s.typeRank = static_cast<int>(it - types.begin());
        s.sameArea = sameArea(r.location(), emergency.location());
        s.reason = std::string(s.typeRank == 0 ? "Primary" : "Secondary") + (s.sameArea ? ", same area" : " type");
        result.push_back(s);
    }
    std::sort(result.begin(), result.end(), [](const SuitableResource& a, const SuitableResource& b) {
        if (a.typeRank != b.typeRank) {
            return a.typeRank < b.typeRank;
        }
        if (a.sameArea != b.sameArea) {
            return a.sameArea;
        }
        if (a.resource->activeAssignments() != b.resource->activeAssignments()) {
            return a.resource->activeAssignments() < b.resource->activeAssignments();
        }
        return a.resource->id() < b.resource->id();
    });
    return result;
}

int ResourceManager::capacityAssignedTo(const std::string& emergencyId) const
{
    int total = 0;
    for (const Resource* r : assignedTo(emergencyId)) {
        total += r->capacity();
    }
    return total;
}

std::string ResourceManager::assign(const std::string& resourceId, const std::string& emergencyId, std::time_t now)
{
    Resource* resource = find(resourceId);
    if (resource == nullptr) {
        return "No resource with ID " + resourceKey(resourceId) + " exists.";
    }
    try {
        resource->assignTo(emergencyId);
    } catch (const std::exception& ex) {
        return ex.what();
    }
    AssignmentRecord record;
    record.id = nextRecordId_++;
    record.emergencyId = resourceKey(emergencyId);
    record.resourceId = resource->id();
    record.assignedAt = now;
    history_.push_back(record);
    return "";
}

std::string ResourceManager::release(const std::string& resourceId, const std::string& emergencyId,
                                     std::time_t now, const std::string& note)
{
    Resource* resource = find(resourceId);
    if (resource == nullptr) {
        return "No resource with ID " + resourceKey(resourceId) + " exists.";
    }
    if (!resource->releaseFrom(emergencyId)) {
        return "Resource " + resource->id() + " is not assigned to " + resourceKey(emergencyId) + ".";
    }
    for (AssignmentRecord& record : history_) {
        if (record.isActive() && record.resourceId == resource->id() && record.emergencyId == resourceKey(emergencyId)) {
            record.releasedAt = std::max(now, record.assignedAt);
            record.note = note;
        }
    }
    return "";
}

int ResourceManager::releaseAll(const std::string& emergencyId, std::time_t now, const std::string& note)
{
    int released = 0;
    for (const Resource* r : assignedTo(emergencyId)) {
        if (release(r->id(), emergencyId, now, note).empty()) {
            ++released;
        }
    }
    return released;
}

std::string ResourceManager::setOutOfService(const std::string& resourceId, bool outOfService)
{
    Resource* resource = find(resourceId);
    if (resource == nullptr) {
        return "No resource with ID " + resourceKey(resourceId) + " exists.";
    }
    try {
        resource->setOutOfService(outOfService);
    } catch (const std::exception& ex) {
        return ex.what();
    }
    return "";
}

std::string ResourceManager::setLocation(const std::string& resourceId, const std::string& location)
{
    Resource* resource = find(resourceId);
    if (resource == nullptr) {
        return "No resource with ID " + resourceKey(resourceId) + " exists.";
    }
    try {
        resource->setLocation(location);
    } catch (const std::exception& ex) {
        return ex.what();
    }
    return "";
}

bool ResourceManager::restoreRecord(const AssignmentRecord& record)
{
    for (const AssignmentRecord& existing : history_) {
        if (existing.id == record.id) {
            return false;
        }
    }
    history_.push_back(record);
    nextRecordId_ = std::max(nextRecordId_, record.id + 1);
    return true;
}

std::vector<std::string> ResourceManager::reconcileHistory(std::time_t now)
{
    std::vector<std::string> notes;
    for (AssignmentRecord& record : history_) {
        if (!record.isActive()) {
            continue;
        }
        const Resource* resource = find(record.resourceId);
        if (resource == nullptr || !resource->isAssignedTo(record.emergencyId)) {
            record.releasedAt = std::max(now, record.assignedAt);
            record.note = "Closed during data check";
            notes.push_back("Assignment #" + std::to_string(record.id) + " (" + record.resourceId + " -> " +
                            record.emergencyId + ") had no matching resource assignment and was closed.");
        }
    }
    for (const auto& entry : resources_) {
        for (const std::string& emergencyId : entry.second.assignedEmergencies()) {
            const bool recorded = std::any_of(history_.begin(), history_.end(), [&](const AssignmentRecord& r) {
                return r.isActive() && r.resourceId == entry.first && r.emergencyId == emergencyId;
            });
            if (!recorded) {
                AssignmentRecord record;
                record.id = nextRecordId_++;
                record.resourceId = entry.first;
                record.emergencyId = emergencyId;
                record.assignedAt = now;
                record.note = "Recreated during data check";
                history_.push_back(record);
                notes.push_back("Missing history entry recreated for " + entry.first + " -> " + emergencyId + ".");
            }
        }
    }
    return notes;
}

void ResourceManager::purgeHistory(const std::set<std::string>& ids)
{
    history_.erase(std::remove_if(history_.begin(), history_.end(),
                                  [&ids](const AssignmentRecord& r) {
                                      return ids.count(r.resourceId) > 0 || ids.count(r.emergencyId) > 0;
                                  }),
                   history_.end());
}

std::vector<std::pair<std::string, int>> ResourceManager::deploymentCounts() const
{
    std::map<std::string, int> counts;
    for (const AssignmentRecord& record : history_) {
        ++counts[record.resourceId];
    }
    std::vector<std::pair<std::string, int>> result(counts.begin(), counts.end());
    std::stable_sort(result.begin(), result.end(),
                     [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                         return a.second > b.second;
                     });
    return result;
}

std::string ResourceManager::generateId() const
{
    long long highest = 100;
    for (const auto& entry : resources_) {
        const std::string& id = entry.first;
        long long number = 0;
        if (id.size() > 2 && id.compare(0, 2, "R-") == 0 && std::isdigit(static_cast<unsigned char>(id[2])) &&
            util::parseInteger(id.substr(2), number)) {
            highest = std::max(highest, number);
        }
    }
    for (long long candidate = highest + 1; candidate < 100000; ++candidate) {
        const std::string id = "R-" + std::to_string(candidate);
        if (!exists(id)) {
            return id;
        }
    }
    throw std::runtime_error("No automatic resource IDs left; please enter an ID manually.");
}

void ResourceManager::clear()
{
    resources_.clear();
    history_.clear();
    nextRecordId_ = 1;
}

}
