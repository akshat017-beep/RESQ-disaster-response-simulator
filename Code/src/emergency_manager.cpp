#include "emergency_manager.h"

#include <algorithm>
#include <queue>

namespace resq {

std::string EmergencyManager::add(const Emergency& emergency) {
    if (items_.count(emergency.id()) > 0) return "An emergency with ID " + emergency.id() + " already exists.";
    items_.emplace(emergency.id(), emergency);
    return "";
}

Emergency* EmergencyManager::find(const std::string& id) {
    const auto it = items_.find(util::upper(util::trim(id)));
    return it == items_.end() ? nullptr : &it->second;
}

std::vector<const Emergency*> EmergencyManager::list() const {
    std::vector<const Emergency*> result;
    for (const auto& item : items_) result.push_back(&item.second);
    return result;
}

bool EmergencyManager::before(const Emergency* a, const Emergency* b) {
    if (a->score() != b->score()) return a->score() > b->score();
    if (a->registered() != b->registered()) return a->registered() < b->registered();
    return a->id() < b->id();
}

std::vector<const Emergency*> EmergencyManager::sorted(SortBy by) const {
    std::vector<const Emergency*> result = list();
    if (by == SortBy::Priority) {
        std::sort(result.begin(), result.end(), before);
    } else if (by == SortBy::Severity) {
        std::sort(result.begin(), result.end(), [](const Emergency* a, const Emergency* b) {
            return a->severity() != b->severity() ? a->severity() < b->severity() : before(a, b);
        });
    } else if (by == SortBy::Time) {
        std::sort(result.begin(), result.end(), [](const Emergency* a, const Emergency* b) {
            return a->registered() != b->registered() ? a->registered() < b->registered() : a->id() < b->id();
        });
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::search(int field, const std::string& query) const {
    std::vector<const Emergency*> result;
    const std::string q = util::trim(query);
    for (const Emergency* e : list()) {
        const std::string& text = field == 1 ? e->id() : (field == 2 ? e->reporter() : e->location());
        if (!q.empty() && util::icontains(text, q)) result.push_back(e);
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::byCategory(Category category) const {
    std::vector<const Emergency*> result;
    for (const Emergency* e : list())
        if (e->category() == category) result.push_back(e);
    return result;
}

std::vector<const Emergency*> EmergencyManager::queue(std::time_t now) {
    refresh(now);
    const auto lower = [](const Emergency* a, const Emergency* b) { return before(b, a); };
    std::priority_queue<const Emergency*, std::vector<const Emergency*>, decltype(lower)> pending(lower);
    for (const auto& item : items_)
        if (item.second.status() == Status::Pending) pending.push(&item.second);
    std::vector<const Emergency*> result;
    while (!pending.empty()) {
        result.push_back(pending.top());
        pending.pop();
    }
    return result;
}

void EmergencyManager::refresh(std::time_t now) {
    for (auto& item : items_) item.second.refresh(now);
}

std::string EmergencyManager::nextId() const {
    long long highest = 1000;
    for (const auto& item : items_) {
        long long number = 0;
        if (item.first.compare(0, 2, "E-") == 0 && util::toInt(item.first.substr(2), number))
            highest = std::max(highest, number);
    }
    return "E-" + std::to_string(highest + 1);
}

void EmergencyManager::removeDemo() {
    for (auto it = items_.begin(); it != items_.end();) it = it->second.demo() ? items_.erase(it) : std::next(it);
}

}
