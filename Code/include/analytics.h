#ifndef RESQ_ANALYTICS_H
#define RESQ_ANALYTICS_H

#include "emergency_manager.h"
#include "resource_manager.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace resq {

struct TypeUsage {
    int total = 0;
    int available = 0;
    int deployed = 0;
    int outOfService = 0;
};

struct Statistics {
    int totalEmergencies = 0;
    std::array<int, kStatusCount> byStatus{};
    std::array<int, kSeverityCount> bySeverity{};
    std::array<int, kCategoryCount> byCategory{};
    int activeCases = 0;
    int activeCritical = 0;
    int peopleAffectedActive = 0;

    int totalResources = 0;
    int availableResources = 0;
    int deployedResources = 0;
    int outOfServiceResources = 0;
    std::array<TypeUsage, kResourceTypeCount> byType{};
    int activeAssignments = 0;
    int totalAssignmentsLogged = 0;

    int responseSamples = 0;
    double averageResponseSeconds = 0.0;
    int resolutionSamples = 0;
    double averageResolutionSeconds = 0.0;

    int count(Status s) const { return byStatus[static_cast<size_t>(indexOf(s))]; }
    double utilizationPercent() const;
};

class AnalyticsManager {
public:
    AnalyticsManager(const EmergencyManager& emergencies, const ResourceManager& resources);

    Statistics compute() const;
    std::vector<std::pair<std::string, int>> mostDeployed(size_t limit) const;

private:
    const EmergencyManager& emergencies_;
    const ResourceManager& resources_;
};

}

#endif
