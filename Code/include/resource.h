#ifndef RESQ_RESOURCE_H
#define RESQ_RESOURCE_H

#include "types.h"

#include <ctime>
#include <string>
#include <vector>

namespace resq {

class Resource {
public:
    static constexpr size_t kMaxLocation = 40;
    static constexpr int kMaxCapacity = 10000;
    static constexpr int kMaxConcurrent = 20;

    Resource(std::string id, ResourceType type, std::string location, int capacity, int maxConcurrent = 1,
             bool demo = false);

    const std::string& id() const { return id_; }
    ResourceType type() const { return type_; }
    const std::string& location() const { return location_; }
    int capacity() const { return capacity_; }
    int maxConcurrent() const { return maxConcurrent_; }
    bool isDemo() const { return demo_; }
    bool isOutOfService() const { return outOfService_; }
    const std::vector<std::string>& assignedEmergencies() const { return assigned_; }

    ResourceStatus status() const;
    int activeAssignments() const { return static_cast<int>(assigned_.size()); }
    bool hasFreeSlot() const { return activeAssignments() < maxConcurrent_; }
    bool canAccept() const { return !outOfService_ && hasFreeSlot(); }
    bool isAssignedTo(const std::string& emergencyId) const;
    std::string statusLabel() const;

    void setLocation(const std::string& location);
    void setOutOfService(bool outOfService);
    void assignTo(const std::string& emergencyId);
    bool releaseFrom(const std::string& emergencyId);

    static std::string validateId(const std::string& id);
    static std::string validateLocation(const std::string& location);
    static std::string validateCapacity(long long capacity);
    static std::string validateMaxConcurrent(long long maxConcurrent);

private:
    std::string id_;
    ResourceType type_;
    std::string location_;
    int capacity_;
    int maxConcurrent_;
    bool demo_;
    bool outOfService_ = false;
    std::vector<std::string> assigned_;
};

struct AssignmentRecord {
    long id = 0;
    std::string emergencyId;
    std::string resourceId;
    std::time_t assignedAt = 0;
    std::time_t releasedAt = 0;
    std::string note;

    bool isActive() const { return releasedAt == 0; }
};

}

#endif
