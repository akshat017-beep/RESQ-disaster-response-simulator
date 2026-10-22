#ifndef RESQ_DISPATCHER_H
#define RESQ_DISPATCHER_H

#include "emergency_manager.h"
#include "resource_manager.h"

#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace resq {

struct OpResult {
    bool ok = false;
    std::string message;

    static OpResult success(std::string text) { return {true, std::move(text)}; }
    static OpResult failure(std::string text) { return {false, std::move(text)}; }
};

class Dispatcher {
public:
    Dispatcher(EmergencyManager& emergencies, ResourceManager& resources);

    OpResult assign(const std::string& emergencyId, const std::string& resourceId, std::time_t now);
    OpResult release(const std::string& emergencyId, const std::string& resourceId, std::time_t now,
                     bool force = false);
    OpResult startOperation(const std::string& emergencyId, std::time_t now);
    OpResult resolve(const std::string& emergencyId, std::time_t now);
    OpResult cancel(const std::string& emergencyId, std::time_t now);
    OpResult returnToQueue(const std::string& emergencyId, std::time_t now);
    OpResult changeStatus(const std::string& emergencyId, Status target, std::time_t now);
    std::vector<Status> allowedNextStatuses(const std::string& emergencyId) const;

    std::vector<std::string> reconcile(std::time_t now);

private:
    EmergencyManager& emergencies_;
    ResourceManager& resources_;
};

}

#endif
