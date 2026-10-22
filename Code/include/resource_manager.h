#ifndef RESQ_RESOURCE_MANAGER_H
#define RESQ_RESOURCE_MANAGER_H

#include "emergency.h"
#include "resource.h"

#include <ctime>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace resq {

enum class ResourceSearchField { Id, Type, Location, Status };

struct SuitableResource {
    const Resource* resource = nullptr;
    int typeRank = 0;
    bool sameArea = false;
    std::string reason;
};

class ResourceManager {
public:
    std::string add(Resource resource);
    bool remove(const std::string& id);
    bool exists(const std::string& id) const;
    Resource* find(const std::string& id);
    const Resource* find(const std::string& id) const;

    std::vector<const Resource*> all() const;
    std::vector<const Resource*> search(ResourceSearchField field, const std::string& query) const;
    std::vector<const Resource*> byType(ResourceType type) const;
    std::vector<const Resource*> byStatus(ResourceStatus status) const;
    std::vector<const Resource*> assignedTo(const std::string& emergencyId) const;
    std::vector<const Resource*> acceptingAssignments() const;
    std::vector<std::string> demoIds() const;

    static std::vector<ResourceType> recommendedTypes(Category category);
    static bool isRecommended(ResourceType type, Category category);
    std::vector<SuitableResource> suitableFor(const Emergency& emergency) const;
    int capacityAssignedTo(const std::string& emergencyId) const;

    std::string assign(const std::string& resourceId, const std::string& emergencyId, std::time_t now);
    std::string release(const std::string& resourceId, const std::string& emergencyId, std::time_t now,
                        const std::string& note);
    int releaseAll(const std::string& emergencyId, std::time_t now, const std::string& note);
    std::string setOutOfService(const std::string& resourceId, bool outOfService);
    std::string setLocation(const std::string& resourceId, const std::string& location);

    const std::vector<AssignmentRecord>& history() const { return history_; }
    bool restoreRecord(const AssignmentRecord& record);
    std::vector<std::string> reconcileHistory(std::time_t now);
    void purgeHistory(const std::set<std::string>& ids);
    std::vector<std::pair<std::string, int>> deploymentCounts() const;

    std::string generateId() const;
    size_t size() const { return resources_.size(); }
    bool empty() const { return resources_.empty(); }
    void clear();

private:
    std::map<std::string, Resource> resources_;
    std::vector<AssignmentRecord> history_;
    long nextRecordId_ = 1;
};

}

#endif
