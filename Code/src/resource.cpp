#include "resource.h"

#include <algorithm>
#include <stdexcept>

namespace resq {

Resource::Resource(std::string id, ResourceType type, std::string location, int capacity, int maxShared, bool demo)
    : id_(util::upper(util::trim(id))), type_(type), location_(util::trim(location)), capacity_(capacity),
      maxShared_(maxShared), demo_(demo) {
    std::string error = util::validId(id_) ? util::checkText(location_, "Location", 40)
                                           : "ID must be 3-8 letters, digits or '-', starting with a letter.";
    if (error.empty() && (capacity_ < 0 || capacity_ > 10000)) error = "Capacity must be between 0 and 10000.";
    if (error.empty() && (maxShared_ < 1 || maxShared_ > 20))
        error = "Simultaneous emergencies must be between 1 and 20.";
    if (!error.empty()) throw std::invalid_argument(error);
}

bool Resource::available() const { return !outOfService_ && static_cast<int>(assigned_.size()) < maxShared_; }

bool Resource::assignedTo(const std::string& emergencyId) const {
    return std::find(assigned_.begin(), assigned_.end(), emergencyId) != assigned_.end();
}

std::string Resource::statusText() const {
    if (outOfService_) return "Out of service";
    if (assigned_.empty()) return "Available";
    return "Deployed (" + std::to_string(assigned_.size()) + "/" + std::to_string(maxShared_) + ")";
}

std::string Resource::assign(const std::string& emergencyId) {
    if (outOfService_) return "Resource " + id_ + " is out of service and cannot be assigned.";
    if (assignedTo(emergencyId)) return "Resource " + id_ + " is already assigned to " + emergencyId + ".";
    if (!available())
        return "Resource " + id_ + " is already deployed to " + assigned_.front() + " and cannot take another case.";
    assigned_.push_back(emergencyId);
    return "";
}

bool Resource::release(const std::string& emergencyId) {
    const auto it = std::find(assigned_.begin(), assigned_.end(), emergencyId);
    if (it == assigned_.end()) return false;
    assigned_.erase(it);
    return true;
}

std::string Resource::setOutOfService(bool value) {
    if (value && !assigned_.empty())
        return "Resource " + id_ + " is deployed; release it before marking it out of service.";
    outOfService_ = value;
    return "";
}

void Resource::setLocation(const std::string& location) {
    const std::string error = util::checkText(util::trim(location), "Location", 40);
    if (!error.empty()) throw std::invalid_argument(error);
    location_ = util::trim(location);
}

}
