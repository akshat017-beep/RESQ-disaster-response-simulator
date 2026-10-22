#ifndef RESQ_DEMO_DATA_H
#define RESQ_DEMO_DATA_H

#include "dispatcher.h"
#include "emergency_manager.h"
#include "resource_manager.h"

#include <ctime>

namespace resq {

struct DemoResult {
    int emergenciesAdded = 0;
    int resourcesAdded = 0;
    int skipped = 0;
};

class DemoData {
public:
    static DemoResult load(EmergencyManager& emergencies, ResourceManager& resources, Dispatcher& dispatcher,
                           std::time_t now);

    static int removeAll(EmergencyManager& emergencies, ResourceManager& resources, Dispatcher& dispatcher,
                         std::time_t now);

    static bool present(const EmergencyManager& emergencies, const ResourceManager& resources);
};

}

#endif
