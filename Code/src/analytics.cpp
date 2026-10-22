#include "analytics.h"

namespace resq {

double Statistics::utilizationPercent() const
{
    const int inService = totalResources - outOfServiceResources;
    if (inService <= 0) {
        return -1.0;
    }
    return 100.0 * static_cast<double>(deployedResources) / static_cast<double>(inService);
}

AnalyticsManager::AnalyticsManager(const EmergencyManager& emergencies, const ResourceManager& resources)
    : emergencies_(emergencies), resources_(resources)
{
}

Statistics AnalyticsManager::compute() const
{
    Statistics s;
    double responseTotal = 0.0;
    double resolutionTotal = 0.0;

    for (const Emergency* e : emergencies_.all()) {
        ++s.totalEmergencies;
        ++s.byStatus[static_cast<size_t>(indexOf(e->status()))];
        ++s.bySeverity[static_cast<size_t>(indexOf(e->severity()))];
        ++s.byCategory[static_cast<size_t>(indexOf(e->category()))];
        if (e->isActive()) {
            ++s.activeCases;
            s.peopleAffectedActive += e->affected();
            if (e->severity() == Severity::Critical) {
                ++s.activeCritical;
            }
        }
        if (e->assignedAt() > 0 && e->assignedAt() >= e->registeredAt()) {
            responseTotal += static_cast<double>(e->assignedAt() - e->registeredAt());
            ++s.responseSamples;
        }
        if (e->status() == Status::Resolved && e->closedAt() > e->registeredAt()) {
            resolutionTotal += static_cast<double>(e->closedAt() - e->registeredAt());
            ++s.resolutionSamples;
        }
    }
    if (s.responseSamples > 0) {
        s.averageResponseSeconds = responseTotal / s.responseSamples;
    }
    if (s.resolutionSamples > 0) {
        s.averageResolutionSeconds = resolutionTotal / s.resolutionSamples;
    }

    for (const Resource* r : resources_.all()) {
        ++s.totalResources;
        TypeUsage& usage = s.byType[static_cast<size_t>(indexOf(r->type()))];
        ++usage.total;
        switch (r->status()) {
        case ResourceStatus::Available:
            ++s.availableResources;
            ++usage.available;
            break;
        case ResourceStatus::Deployed:
            ++s.deployedResources;
            ++usage.deployed;
            break;
        case ResourceStatus::OutOfService:
            ++s.outOfServiceResources;
            ++usage.outOfService;
            break;
        }
        s.activeAssignments += r->activeAssignments();
    }
    s.totalAssignmentsLogged = static_cast<int>(resources_.history().size());
    return s;
}

std::vector<std::pair<std::string, int>> AnalyticsManager::mostDeployed(size_t limit) const
{
    std::vector<std::pair<std::string, int>> counts = resources_.deploymentCounts();
    if (counts.size() > limit) {
        counts.resize(limit);
    }
    return counts;
}

}
