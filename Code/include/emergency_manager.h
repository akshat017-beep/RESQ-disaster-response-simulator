#ifndef RESQ_EMERGENCY_MANAGER_H
#define RESQ_EMERGENCY_MANAGER_H

#include "emergency.h"

#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace resq {

enum class EmergencySortKey { Severity, Priority, RegistrationTime, Id };
enum class EmergencySearchField { Id, Reporter, Location };

class EmergencyManager {
public:
    std::string add(Emergency emergency);
    bool remove(const std::string& id);
    bool exists(const std::string& id) const;
    Emergency* find(const std::string& id);
    const Emergency* find(const std::string& id) const;

    std::vector<const Emergency*> all() const;
    std::vector<const Emergency*> sorted(EmergencySortKey key, bool reverse = false) const;
    std::vector<const Emergency*> search(EmergencySearchField field, const std::string& query) const;
    std::vector<const Emergency*> searchByCategory(Category category) const;
    std::vector<const Emergency*> withStatus(Status status) const;
    std::vector<std::string> demoIds() const;

    void recalculateAll(std::time_t now);
    std::vector<const Emergency*> pendingByPriority(std::time_t now);
    const Emergency* highestPriorityPending(std::time_t now);
    int queuePosition(const std::string& id, std::time_t now);

    std::string updateSeverity(const std::string& id, Severity severity, std::time_t now);
    std::string updateLocation(const std::string& id, const std::string& location, std::time_t now);
    std::string updateDescription(const std::string& id, const std::string& description, std::time_t now);
    std::string updateAffected(const std::string& id, int affected, std::time_t now);

    std::string generateId() const;

    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }
    void clear() { items_.clear(); }

    static bool comesFirst(const Emergency& a, const Emergency& b);

private:
    std::string modify(const std::string& id, std::time_t now, const std::function<void(Emergency&)>& change);

    std::map<std::string, Emergency> items_;
};

}

#endif
