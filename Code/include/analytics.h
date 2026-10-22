#ifndef RESQ_ANALYTICS_H
#define RESQ_ANALYTICS_H

#include "resource_manager.h"

namespace resq {

struct Stats {
    int total = 0;
    int activeCritical = 0;
    int status[kStatuses] = {};
    int severity[kSeverities] = {};
    int category[kCategories] = {};
    int resources = 0;
    int available = 0;
    int deployed = 0;
    int outOfService = 0;
    int responseCount = 0;
    double averageResponse = 0;
    int resolvedCount = 0;
    double averageResolution = 0;

    int count(Status s) const { return status[idx(s)]; }
    int open() const { return count(Status::Pending) + count(Status::Assigned) + count(Status::InProgress); }
};

class AnalyticsManager {
public:
    static Stats compute(const EmergencyManager& emergencies, const ResourceManager& resources);
};

}

#endif
