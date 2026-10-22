#ifndef RESQ_EMERGENCY_MANAGER_H
#define RESQ_EMERGENCY_MANAGER_H

#include "emergency.h"

#include <map>

namespace resq {

enum class SortBy { Severity, Priority, Time, Id };

class EmergencyManager {
public:
    std::string add(const Emergency& emergency);
    Emergency* find(const std::string& id);
    std::vector<const Emergency*> list() const;
    std::vector<const Emergency*> sorted(SortBy by) const;
    std::vector<const Emergency*> search(int field, const std::string& query) const;
    std::vector<const Emergency*> byCategory(Category category) const;
    std::vector<const Emergency*> queue(std::time_t now);
    void refresh(std::time_t now);
    std::string nextId() const;
    void removeDemo();
    void clear() { items_.clear(); }
    size_t size() const { return items_.size(); }

    static bool before(const Emergency* a, const Emergency* b);

private:
    std::map<std::string, Emergency> items_;
};

}

#endif
