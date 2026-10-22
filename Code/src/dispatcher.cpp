#include "dispatcher.h"

#include "util.h"

#include <stdexcept>

namespace resq {

namespace {

std::string notFound(const std::string& id) { return "No emergency with ID " + util::toUpper(id) + " exists."; }

}

Dispatcher::Dispatcher(EmergencyManager& emergencies, ResourceManager& resources)
    : emergencies_(emergencies), resources_(resources)
{
}

OpResult Dispatcher::assign(const std::string& emergencyId, const std::string& resourceId, std::time_t now)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return OpResult::failure(notFound(emergencyId));
    }
    if (!emergency->isActive()) {
        return OpResult::failure("Case " + emergency->id() + " is " + toString(emergency->status()) +
                                 "; resources can only be assigned to active cases.");
    }
    const std::string error = resources_.assign(resourceId, emergency->id(), now);
    if (!error.empty()) {
        return OpResult::failure(error);
    }
    if (emergency->status() == Status::Pending) {
        emergency->transitionTo(Status::Assigned, now);
    }
    return OpResult::success("Resource " + util::toUpper(util::trim(resourceId)) + " assigned to " +
                             emergency->id() + ". Case status: " + toString(emergency->status()) + ".");
}

OpResult Dispatcher::release(const std::string& emergencyId, const std::string& resourceId, std::time_t now,
                             bool force)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    const std::string eid = util::toUpper(util::trim(emergencyId));
    const std::string rid = util::toUpper(util::trim(resourceId));
    const auto assigned = resources_.assignedTo(eid);
    const bool isLast = assigned.size() == 1 && assigned.front()->id() == rid;

    if (!force && emergency != nullptr && emergency->status() == Status::InProgress && isLast) {
        return OpResult::failure("Resource " + rid + " is the last resource on In Progress case " + eid +
                                 ". Mark the case Resolved or Cancelled instead.");
    }
    const std::string error = resources_.release(rid, eid, now, "Released by operator");
    if (!error.empty()) {
        return OpResult::failure(error);
    }
    std::string message = "Resource " + rid + " released from " + eid + " and is available again.";
    if (emergency != nullptr && emergency->status() == Status::Assigned && resources_.assignedTo(eid).empty()) {
        emergency->transitionTo(Status::Pending, now);
        message += " No resources remain, so the case returned to the Pending queue.";
    }
    return OpResult::success(message);
}

OpResult Dispatcher::startOperation(const std::string& emergencyId, std::time_t now)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return OpResult::failure(notFound(emergencyId));
    }
    if (emergency->status() != Status::Assigned) {
        return OpResult::failure("Only Assigned cases can be marked In Progress (current status: " +
                                 toString(emergency->status()) + ").");
    }
    if (resources_.assignedTo(emergency->id()).empty()) {
        return OpResult::failure("Case " + emergency->id() + " has no assigned resources.");
    }
    emergency->transitionTo(Status::InProgress, now);
    return OpResult::success("Case " + emergency->id() + " is now In Progress.");
}

OpResult Dispatcher::resolve(const std::string& emergencyId, std::time_t now)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return OpResult::failure(notFound(emergencyId));
    }
    if (emergency->status() != Status::InProgress) {
        return OpResult::failure("A case must be In Progress before it can be resolved (current status: " +
                                 toString(emergency->status()) + ").");
    }
    const int released = resources_.releaseAll(emergency->id(), now, "Operation resolved");
    emergency->transitionTo(Status::Resolved, now);
    return OpResult::success("Case " + emergency->id() + " marked Resolved. " + std::to_string(released) +
                             " resource(s) released and available again.");
}

OpResult Dispatcher::cancel(const std::string& emergencyId, std::time_t now)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return OpResult::failure(notFound(emergencyId));
    }
    if (!emergency->isActive()) {
        return OpResult::failure("Case " + emergency->id() + " is already " + toString(emergency->status()) + ".");
    }
    const int released = resources_.releaseAll(emergency->id(), now, "Case cancelled");
    emergency->transitionTo(Status::Cancelled, now);
    return OpResult::success("Case " + emergency->id() + " cancelled. " + std::to_string(released) +
                             " resource(s) released.");
}

OpResult Dispatcher::returnToQueue(const std::string& emergencyId, std::time_t now)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return OpResult::failure(notFound(emergencyId));
    }
    if (emergency->status() != Status::Assigned) {
        return OpResult::failure("Only Assigned cases can be returned to Pending (current status: " +
                                 toString(emergency->status()) + ").");
    }
    const int released = resources_.releaseAll(emergency->id(), now, "Case returned to queue");
    emergency->transitionTo(Status::Pending, now);
    return OpResult::success("Case " + emergency->id() + " returned to the Pending queue. " +
                             std::to_string(released) + " resource(s) released.");
}

OpResult Dispatcher::changeStatus(const std::string& emergencyId, Status target, std::time_t now)
{
    switch (target) {
    case Status::Pending: return returnToQueue(emergencyId, now);
    case Status::Assigned:
        return OpResult::failure("A case becomes Assigned when a resource is assigned to it.");
    case Status::InProgress: return startOperation(emergencyId, now);
    case Status::Resolved: return resolve(emergencyId, now);
    case Status::Cancelled: return cancel(emergencyId, now);
    }
    return OpResult::failure("Unknown status.");
}

std::vector<Status> Dispatcher::allowedNextStatuses(const std::string& emergencyId) const
{
    std::vector<Status> result;
    const Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return result;
    }
    for (Status s : allStatuses()) {
        if (Emergency::isTransitionAllowed(emergency->status(), s)) {
            result.push_back(s);
        }
    }
    return result;
}

std::vector<std::string> Dispatcher::reconcile(std::time_t now)
{
    std::vector<std::string> notes;
    for (const Resource* resource : resources_.all()) {
        const std::vector<std::string> assigned = resource->assignedEmergencies();
        for (const std::string& eid : assigned) {
            const Emergency* emergency = emergencies_.find(eid);
            if (emergency == nullptr || !emergency->isActive()) {
                resources_.release(resource->id(), eid, now, "Released during data check");
                notes.push_back("Resource " + resource->id() + " was linked to " +
                                (emergency == nullptr ? "unknown case " : "closed case ") + eid +
                                "; the link was removed.");
            }
        }
    }
    for (const Emergency* e : emergencies_.withStatus(Status::Assigned)) {
        if (resources_.assignedTo(e->id()).empty()) {
            emergencies_.find(e->id())->transitionTo(Status::Pending, now);
            notes.push_back("Case " + e->id() + " was Assigned without resources and returned to Pending.");
        }
    }
    for (const Emergency* e : emergencies_.withStatus(Status::InProgress)) {
        if (resources_.assignedTo(e->id()).empty()) {
            notes.push_back("Case " + e->id() +
                            " is In Progress without assigned resources. Review it in Active Operations.");
        }
    }
    for (const std::string& note : resources_.reconcileHistory(now)) {
        notes.push_back(note);
    }
    emergencies_.recalculateAll(now);
    return notes;
}

}
