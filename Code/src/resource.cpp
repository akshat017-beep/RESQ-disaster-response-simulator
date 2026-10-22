#include "resource.h"

#include "util.h"

#include <algorithm>
#include <stdexcept>

namespace resq {

namespace {

void requireValid(const std::string& error)
{
    if (!error.empty()) {
        throw std::invalid_argument(error);
    }
}

}

Resource::Resource(std::string id, ResourceType type, std::string location, int capacity, int maxConcurrent,
                   bool demo)
    : id_(util::toUpper(util::trim(id))),
      type_(type),
      location_(util::trim(location)),
      capacity_(capacity),
      maxConcurrent_(maxConcurrent),
      demo_(demo)
{
    requireValid(validateId(id_));
    requireValid(validateLocation(location_));
    requireValid(validateCapacity(capacity_));
    requireValid(validateMaxConcurrent(maxConcurrent_));
}

ResourceStatus Resource::status() const
{
    if (outOfService_) {
        return ResourceStatus::OutOfService;
    }
    return assigned_.empty() ? ResourceStatus::Available : ResourceStatus::Deployed;
}

bool Resource::isAssignedTo(const std::string& emergencyId) const
{
    return std::find(assigned_.begin(), assigned_.end(), util::toUpper(emergencyId)) != assigned_.end();
}

std::string Resource::statusLabel() const
{
    switch (status()) {
    case ResourceStatus::Available:
        return maxConcurrent_ > 1 ? "Available (0/" + std::to_string(maxConcurrent_) + ")" : "Available";
    case ResourceStatus::Deployed:
        return "Deployed (" + std::to_string(activeAssignments()) + "/" + std::to_string(maxConcurrent_) + ")";
    case ResourceStatus::OutOfService: return "Out of service";
    }
    return "Unknown";
}

void Resource::setLocation(const std::string& location)
{
    const std::string value = util::trim(location);
    requireValid(validateLocation(value));
    location_ = value;
}

void Resource::setOutOfService(bool outOfService)
{
    if (outOfService && !assigned_.empty()) {
        throw std::logic_error("Resource " + id_ +
                               " is deployed. Release it from its emergencies before marking it out of service.");
    }
    outOfService_ = outOfService;
}

void Resource::assignTo(const std::string& emergencyId)
{
    const std::string id = util::toUpper(util::trim(emergencyId));
    if (outOfService_) {
        throw std::logic_error("Resource " + id_ + " is out of service and cannot be assigned.");
    }
    if (isAssignedTo(id)) {
        throw std::logic_error("Resource " + id_ + " is already assigned to " + id + ".");
    }
    if (!hasFreeSlot()) {
        throw std::logic_error("Resource " + id_ + " is already deployed to " + assigned_.front() +
                               " and is configured for at most " + std::to_string(maxConcurrent_) +
                               " simultaneous emergenc" + (maxConcurrent_ == 1 ? "y." : "ies."));
    }
    assigned_.push_back(id);
}

bool Resource::releaseFrom(const std::string& emergencyId)
{
    const auto it = std::find(assigned_.begin(), assigned_.end(), util::toUpper(emergencyId));
    if (it == assigned_.end()) {
        return false;
    }
    assigned_.erase(it);
    return true;
}

std::string Resource::validateId(const std::string& id)
{
    if (id.empty()) {
        return "Resource ID is required.";
    }
    if (!util::isValidId(id)) {
        return "Resource ID must be 3-8 characters (letters, digits or '-') and start with a letter.";
    }
    return "";
}

std::string Resource::validateLocation(const std::string& location)
{
    if (location.empty()) {
        return "Location is required.";
    }
    if (util::utf8Length(location) > kMaxLocation) {
        return "Location must be at most " + std::to_string(kMaxLocation) + " characters.";
    }
    if (!util::isPrintableText(location)) {
        return "Location contains invalid control characters.";
    }
    return "";
}

std::string Resource::validateCapacity(long long capacity)
{
    if (capacity < 0 || capacity > kMaxCapacity) {
        return "Capacity must be between 0 and " + std::to_string(kMaxCapacity) + ".";
    }
    return "";
}

std::string Resource::validateMaxConcurrent(long long maxConcurrent)
{
    if (maxConcurrent < 1 || maxConcurrent > kMaxConcurrent) {
        return "Simultaneous assignments must be between 1 and " + std::to_string(kMaxConcurrent) + ".";
    }
    return "";
}

}
