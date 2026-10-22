#include "resource_manager.h"

#include <algorithm>

namespace resq {

std::string ResourceManager::add(const Resource& resource) {
    if (items_.count(resource.id()) > 0) return "A resource with ID " + resource.id() + " already exists.";
    items_.emplace(resource.id(), resource);
    return "";
}

Resource* ResourceManager::find(const std::string& id) {
    const auto it = items_.find(util::upper(util::trim(id)));
    return it == items_.end() ? nullptr : &it->second;
}

std::vector<const Resource*> ResourceManager::list() const {
    std::vector<const Resource*> result;
    for (const auto& item : items_) result.push_back(&item.second);
    return result;
}

std::vector<const Resource*> ResourceManager::search(int field, const std::string& query) const {
    std::vector<const Resource*> result;
    const std::string q = util::trim(query);
    for (const Resource* r : list()) {
        const std::string text = field == 1   ? r->id()
                                 : field == 2 ? name(r->type())
                                 : field == 3 ? r->location()
                                              : r->statusText();
        if (!q.empty() && util::icontains(text, q)) result.push_back(r);
    }
    return result;
}

std::vector<const Resource*> ResourceManager::assignedTo(const std::string& emergencyId) const {
    std::vector<const Resource*> result;
    for (const Resource* r : list())
        if (r->assignedTo(emergencyId)) result.push_back(r);
    return result;
}

std::vector<ResourceType> ResourceManager::recommended(Category category) {
    using T = ResourceType;
    switch (category) {
    case Category::Medical: return {T::Ambulance, T::MedicalTeam};
    case Category::Fire: return {T::FireEngine, T::Ambulance, T::RescueTeam};
    case Category::RoadAccident: return {T::Ambulance, T::RescueTeam, T::MedicalTeam};
    case Category::NaturalDisaster: return {T::RescueTeam, T::ReliefVehicle, T::MedicalTeam, T::SupplyKit};
    case Category::Flood: return {T::RescueTeam, T::ReliefVehicle, T::SupplyKit, T::Ambulance};
    case Category::Rescue: return {T::RescueTeam, T::MedicalTeam, T::Ambulance};
    default: return {T::Ambulance, T::MedicalTeam, T::FireEngine, T::RescueTeam, T::ReliefVehicle, T::SupplyKit};
    }
}

std::vector<const Resource*> ResourceManager::suitableFor(const Emergency& emergency) const {
    std::vector<const Resource*> result;
    for (ResourceType type : recommended(emergency.category())) {
        for (const Resource* r : list())
            if (r->type() == type && r->available() && !r->assignedTo(emergency.id())) result.push_back(r);
    }
    return result;
}

std::string ResourceManager::assign(Emergency& emergency, const std::string& resourceId, std::time_t now) {
    if (!emergency.active())
        return "Case " + emergency.id() + " is " + name(emergency.status()) + " and cannot receive resources.";
    Resource* resource = find(resourceId);
    if (resource == nullptr) return "No resource with ID " + util::upper(util::trim(resourceId)) + " exists.";
    const std::string error = resource->assign(emergency.id());
    if (error.empty() && emergency.status() == Status::Pending) emergency.moveTo(Status::Assigned, now);
    return error;
}

std::string ResourceManager::release(Emergency& emergency, const std::string& resourceId, std::time_t now) {
    Resource* resource = find(resourceId);
    if (resource == nullptr || !resource->assignedTo(emergency.id()))
        return "That resource is not assigned to " + emergency.id() + ".";
    if (emergency.status() == Status::InProgress && assignedTo(emergency.id()).size() == 1)
        return "It is the last resource of an In Progress case. Resolve or cancel the case instead.";
    resource->release(emergency.id());
    if (emergency.status() == Status::Assigned && assignedTo(emergency.id()).empty())
        emergency.moveTo(Status::Pending, now);
    return "";
}

std::string ResourceManager::changeStatus(Emergency& emergency, Status to, std::time_t now) {
    if (!Emergency::canMove(emergency.status(), to))
        return "Cannot change " + emergency.id() + " from " + name(emergency.status()) + " to " + name(to) + ".";
    if (to == Status::Assigned) return "A case becomes Assigned when a resource is assigned to it.";
    if (to == Status::InProgress && assignedTo(emergency.id()).empty()) return "Assign a resource first.";
    if (to != Status::InProgress) releaseAll(emergency.id());
    emergency.moveTo(to, now);
    return "";
}

int ResourceManager::releaseAll(const std::string& emergencyId) {
    int released = 0;
    for (auto& item : items_) released += item.second.release(emergencyId) ? 1 : 0;
    return released;
}

void ResourceManager::removeDemo(EmergencyManager& emergencies, std::time_t now) {
    for (auto it = items_.begin(); it != items_.end();) {
        if (!it->second.demo()) {
            ++it;
            continue;
        }
        for (const std::string& emergencyId : std::vector<std::string>(it->second.assigned())) {
            it->second.release(emergencyId);
            Emergency* e = emergencies.find(emergencyId);
            if (e != nullptr && e->status() == Status::Assigned && assignedTo(emergencyId).empty())
                e->moveTo(Status::Pending, now);
        }
        it = items_.erase(it);
    }
    for (const Emergency* e : emergencies.list())
        if (e->demo()) releaseAll(e->id());
    emergencies.removeDemo();
}

std::string ResourceManager::nextId() const {
    long long highest = 100;
    for (const auto& item : items_) {
        long long number = 0;
        if (item.first.compare(0, 2, "R-") == 0 && util::toInt(item.first.substr(2), number))
            highest = std::max(highest, number);
    }
    return "R-" + std::to_string(highest + 1);
}

}
