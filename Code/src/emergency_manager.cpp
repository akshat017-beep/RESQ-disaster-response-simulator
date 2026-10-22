#include "emergency_manager.h"

#include "util.h"

#include <algorithm>
#include <cctype>
#include <queue>
#include <stdexcept>

namespace resq {

namespace {

std::string caseKey(const std::string& id) { return util::toUpper(util::trim(id)); }

struct QueueOrder {
    bool operator()(const Emergency* a, const Emergency* b) const { return EmergencyManager::comesFirst(*b, *a); }
};

}

bool EmergencyManager::comesFirst(const Emergency& a, const Emergency& b)
{
    if (a.priorityScore() != b.priorityScore()) {
        return a.priorityScore() > b.priorityScore();
    }
    if (a.registeredAt() != b.registeredAt()) {
        return a.registeredAt() < b.registeredAt();
    }
    return a.id() < b.id();
}

std::string EmergencyManager::add(Emergency emergency)
{
    if (exists(emergency.id())) {
        return "An emergency with ID " + emergency.id() + " already exists. IDs must be unique.";
    }
    const std::string id = emergency.id();
    items_.emplace(id, std::move(emergency));
    return "";
}

bool EmergencyManager::remove(const std::string& id) { return items_.erase(caseKey(id)) > 0; }

bool EmergencyManager::exists(const std::string& id) const { return items_.count(caseKey(id)) > 0; }

Emergency* EmergencyManager::find(const std::string& id)
{
    const auto it = items_.find(caseKey(id));
    return it == items_.end() ? nullptr : &it->second;
}

const Emergency* EmergencyManager::find(const std::string& id) const
{
    const auto it = items_.find(caseKey(id));
    return it == items_.end() ? nullptr : &it->second;
}

std::vector<const Emergency*> EmergencyManager::all() const
{
    std::vector<const Emergency*> result;
    result.reserve(items_.size());
    for (const auto& entry : items_) {
        result.push_back(&entry.second);
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::sorted(EmergencySortKey sortKey, bool reverse) const
{
    std::vector<const Emergency*> result = all();
    const auto byPriority = [](const Emergency* a, const Emergency* b) { return comesFirst(*a, *b); };
    switch (sortKey) {
    case EmergencySortKey::Severity:
        std::sort(result.begin(), result.end(), [&](const Emergency* a, const Emergency* b) {
            if (a->severity() != b->severity()) {
                return a->severity() < b->severity();
            }
            return byPriority(a, b);
        });
        break;
    case EmergencySortKey::Priority: std::sort(result.begin(), result.end(), byPriority); break;
    case EmergencySortKey::RegistrationTime:
        std::sort(result.begin(), result.end(), [](const Emergency* a, const Emergency* b) {
            if (a->registeredAt() != b->registeredAt()) {
                return a->registeredAt() < b->registeredAt();
            }
            return a->id() < b->id();
        });
        break;
    case EmergencySortKey::Id: break;
    }
    if (reverse) {
        std::reverse(result.begin(), result.end());
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::search(EmergencySearchField field, const std::string& query) const
{
    const std::string q = util::trim(query);
    std::vector<const Emergency*> result;
    if (q.empty()) {
        return result;
    }
    if (field == EmergencySearchField::Id) {
        if (const Emergency* exact = find(q)) {
            result.push_back(exact);
            return result;
        }
    }
    for (const auto& entry : items_) {
        const Emergency& e = entry.second;
        const std::string& text = field == EmergencySearchField::Id         ? e.id()
                                  : field == EmergencySearchField::Reporter ? e.reporter()
                                                                            : e.location();
        if (util::icontains(text, q)) {
            result.push_back(&e);
        }
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::searchByCategory(Category category) const
{
    std::vector<const Emergency*> result;
    for (const auto& entry : items_) {
        if (entry.second.category() == category) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::withStatus(Status status) const
{
    std::vector<const Emergency*> result;
    for (const auto& entry : items_) {
        if (entry.second.status() == status) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<std::string> EmergencyManager::demoIds() const
{
    std::vector<std::string> ids;
    for (const auto& entry : items_) {
        if (entry.second.isDemo()) {
            ids.push_back(entry.first);
        }
    }
    return ids;
}

void EmergencyManager::recalculateAll(std::time_t now)
{
    for (auto& entry : items_) {
        entry.second.recalculatePriority(now);
    }
}

std::vector<const Emergency*> EmergencyManager::pendingByPriority(std::time_t now)
{
    recalculateAll(now);
    std::priority_queue<const Emergency*, std::vector<const Emergency*>, QueueOrder> queue;
    for (const auto& entry : items_) {
        if (entry.second.status() == Status::Pending) {
            queue.push(&entry.second);
        }
    }
    std::vector<const Emergency*> ordered;
    ordered.reserve(queue.size());
    while (!queue.empty()) {
        ordered.push_back(queue.top());
        queue.pop();
    }
    return ordered;
}

const Emergency* EmergencyManager::highestPriorityPending(std::time_t now)
{
    const std::vector<const Emergency*> queue = pendingByPriority(now);
    return queue.empty() ? nullptr : queue.front();
}

int EmergencyManager::queuePosition(const std::string& id, std::time_t now)
{
    const std::vector<const Emergency*> queue = pendingByPriority(now);
    for (size_t i = 0; i < queue.size(); ++i) {
        if (queue[i]->id() == caseKey(id)) {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

std::string EmergencyManager::modify(const std::string& id, std::time_t now,
                                     const std::function<void(Emergency&)>& change)
{
    Emergency* emergency = find(id);
    if (emergency == nullptr) {
        return "No emergency with ID " + caseKey(id) + " exists.";
    }
    try {
        change(*emergency);
    } catch (const std::exception& ex) {
        return ex.what();
    }
    emergency->recalculatePriority(now);
    return "";
}

std::string EmergencyManager::updateSeverity(const std::string& id, Severity severity, std::time_t now)
{
    return modify(id, now, [severity](Emergency& e) { e.setSeverity(severity); });
}

std::string EmergencyManager::updateLocation(const std::string& id, const std::string& location, std::time_t now)
{
    return modify(id, now, [&location](Emergency& e) { e.setLocation(location); });
}

std::string EmergencyManager::updateDescription(const std::string& id, const std::string& description,
                                                std::time_t now)
{
    return modify(id, now, [&description](Emergency& e) { e.setDescription(description); });
}

std::string EmergencyManager::updateAffected(const std::string& id, int affected, std::time_t now)
{
    return modify(id, now, [affected](Emergency& e) { e.setAffected(affected); });
}

std::string EmergencyManager::generateId() const
{
    long long highest = 1000;
    for (const auto& entry : items_) {
        const std::string& id = entry.first;
        long long number = 0;
        if (id.size() > 2 && id.compare(0, 2, "E-") == 0 && util::parseInteger(id.substr(2), number) &&
            std::isdigit(static_cast<unsigned char>(id[2]))) {
            highest = std::max(highest, number);
        }
    }
    for (long long candidate = highest + 1; candidate < 100000; ++candidate) {
        const std::string id = "E-" + std::to_string(candidate);
        if (!exists(id)) {
            return id;
        }
    }
    throw std::runtime_error("No automatic emergency IDs left; please enter an ID manually.");
}

}
