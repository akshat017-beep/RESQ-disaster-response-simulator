#ifndef RESQ_RESOURCE_H
#define RESQ_RESOURCE_H

#include "types.h"

namespace resq {

class Resource {
public:
    Resource(std::string id, ResourceType type, std::string location, int capacity, int maxShared = 1,
             bool demo = false);

    const std::string& id() const { return id_; }
    ResourceType type() const { return type_; }
    const std::string& location() const { return location_; }
    int capacity() const { return capacity_; }
    int maxShared() const { return maxShared_; }
    bool outOfService() const { return outOfService_; }
    bool demo() const { return demo_; }
    const std::vector<std::string>& assigned() const { return assigned_; }

    bool available() const;
    bool assignedTo(const std::string& emergencyId) const;
    std::string statusText() const;

    std::string assign(const std::string& emergencyId);
    bool release(const std::string& emergencyId);
    std::string setOutOfService(bool value);
    void setLocation(const std::string& location);

private:
    std::string id_;
    ResourceType type_;
    std::string location_;
    int capacity_;
    int maxShared_;
    bool demo_;
    bool outOfService_ = false;
    std::vector<std::string> assigned_;
};

}

#endif
