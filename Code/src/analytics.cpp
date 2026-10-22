#include "analytics.h"

namespace resq {

Stats AnalyticsManager::compute(const EmergencyManager& emergencies, const ResourceManager& resources) {
    Stats s;
    double responseTotal = 0;
    double resolutionTotal = 0;
    for (const Emergency* e : emergencies.list()) {
        ++s.total;
        ++s.status[idx(e->status())];
        ++s.severity[idx(e->severity())];
        ++s.category[idx(e->category())];
        if (e->active() && e->severity() == Severity::Critical) ++s.activeCritical;
        if (e->assignedAt() >= e->registered()) {
            responseTotal += static_cast<double>(e->assignedAt() - e->registered());
            ++s.responseCount;
        }
        if (e->status() == Status::Resolved && e->closedAt() >= e->registered()) {
            resolutionTotal += static_cast<double>(e->closedAt() - e->registered());
            ++s.resolvedCount;
        }
    }
    s.averageResponse = s.responseCount > 0 ? responseTotal / s.responseCount : 0;
    s.averageResolution = s.resolvedCount > 0 ? resolutionTotal / s.resolvedCount : 0;

    for (const Resource* r : resources.list()) {
        ++s.resources;
        if (r->outOfService())
            ++s.outOfService;
        else if (r->assigned().empty())
            ++s.available;
        else
            ++s.deployed;
    }
    return s;
}

}
