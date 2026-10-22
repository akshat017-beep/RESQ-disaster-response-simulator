#ifndef RESQ_RESOURCE_MANAGER_H
#define RESQ_RESOURCE_MANAGER_H

#include "emergency_manager.h"
#include "resource.h"

namespace resq {

class ResourceManager {
public:
    std::string add(const Resource& resource);
    Resource* find(const std::string& id);
    std::vector<const Resource*> list() const;
    std::vector<const Resource*> search(int field, const std::string& query) const;
    std::vector<const Resource*> assignedTo(const std::string& emergencyId) const;
    std::vector<const Resource*> suitableFor(const Emergency& emergency) const;
    static std::vector<ResourceType> recommended(Category category);

    std::string assign(Emergency& emergency, const std::string& resourceId, std::time_t now);
    std::string release(Emergency& emergency, const std::string& resourceId, std::time_t now);
    std::string changeStatus(Emergency& emergency, Status to, std::time_t now);
    int releaseAll(const std::string& emergencyId);
    void removeDemo(EmergencyManager& emergencies, std::time_t now);

    std::string nextId() const;
    void clear() { items_.clear(); }
    size_t size() const { return items_.size(); }

private:
    std::map<std::string, Resource> items_;
};

}

#endif
