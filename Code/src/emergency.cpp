#include "emergency.h"

#include "util.h"

#include <cctype>
#include <stdexcept>

namespace resq {

namespace {

void require(const std::string& error)
{
    if (!error.empty()) {
        throw std::invalid_argument(error);
    }
}

std::string validateText(const std::string& value, const char* label, size_t maxLength)
{
    if (value.empty()) {
        return std::string(label) + " is required.";
    }
    if (util::utf8Length(value) > maxLength) {
        return std::string(label) + " must be at most " + std::to_string(maxLength) + " characters.";
    }
    if (!util::isPrintableText(value)) {
        return std::string(label) + " contains invalid control characters.";
    }
    return "";
}

}

Emergency::Emergency(std::string id, std::string reporter, Category category, std::string location,
                     std::string contact, std::string description, Severity severity, int affected,
                     std::time_t registeredAt, bool demo)
    : id_(util::toUpper(util::trim(id))),
      reporter_(util::trim(reporter)),
      category_(category),
      location_(util::trim(location)),
      contact_(util::trim(contact)),
      description_(util::trim(description)),
      severity_(severity),
      affected_(affected),
      registeredAt_(registeredAt),
      demo_(demo)
{
    require(validateId(id_));
    require(validateReporter(reporter_));
    require(validateLocation(location_));
    require(validateContact(contact_));
    require(validateDescription(description_));
    require(validateAffected(affected_));
    if (registeredAt_ <= 0) {
        throw std::invalid_argument("Registration time is missing or invalid.");
    }
    recalculatePriority(registeredAt_);
}

bool Emergency::isActive() const
{
    return status_ == Status::Pending || status_ == Status::Assigned || status_ == Status::InProgress;
}

std::time_t Emergency::waitEnd(std::time_t now) const
{
    if (status_ == Status::Pending) {
        return now;
    }
    if (assignedAt_ > 0) {
        return assignedAt_;
    }
    if (closedAt_ > 0) {
        return closedAt_;
    }
    return now;
}

PriorityBreakdown Emergency::priorityBreakdown(std::time_t now) const
{
    return PriorityCalculator::compute(severity_, affected_, category_, registeredAt_, waitEnd(now));
}

int Emergency::recalculatePriority(std::time_t now)
{
    priorityScore_ = priorityBreakdown(now).total();
    return priorityScore_;
}

void Emergency::requireOpen(const char* field) const
{
    if (isClosed()) {
        throw std::logic_error(std::string("Case ") + id_ + " is " + toString(status_) + "; the " + field +
                               " can no longer be changed (only the description can be corrected).");
    }
}

void Emergency::setSeverity(Severity severity)
{
    requireOpen("severity");
    severity_ = severity;
}

void Emergency::setLocation(const std::string& location)
{
    requireOpen("location");
    const std::string value = util::trim(location);
    require(validateLocation(value));
    location_ = value;
}

void Emergency::setDescription(const std::string& description)
{
    const std::string value = util::trim(description);
    require(validateDescription(value));
    description_ = value;
}

void Emergency::setAffected(int affected)
{
    requireOpen("number of people affected");
    require(validateAffected(affected));
    affected_ = affected;
}

bool Emergency::isTransitionAllowed(Status from, Status to)
{
    switch (from) {
    case Status::Pending: return to == Status::Assigned || to == Status::Cancelled;
    case Status::Assigned: return to == Status::Pending || to == Status::InProgress || to == Status::Cancelled;
    case Status::InProgress: return to == Status::Resolved || to == Status::Cancelled;
    case Status::Resolved:
    case Status::Cancelled: return false;
    }
    return false;
}

void Emergency::transitionTo(Status target, std::time_t now)
{
    if (!isTransitionAllowed(status_, target)) {
        throw std::logic_error("Cannot change the status of " + id_ + " from " + toString(status_) + " to " +
                               toString(target) + ".");
    }
    switch (target) {
    case Status::Pending: assignedAt_ = 0; break;
    case Status::Assigned: assignedAt_ = now; break;
    case Status::InProgress: startedAt_ = now; break;
    case Status::Resolved:
    case Status::Cancelled: closedAt_ = now; break;
    }
    status_ = target;
    recalculatePriority(now);
}

std::string Emergency::restoreState(Status status, std::time_t assignedAt, std::time_t startedAt,
                                    std::time_t closedAt)
{
    std::string repair;
    status_ = status;
    assignedAt_ = assignedAt > 0 ? assignedAt : 0;
    startedAt_ = startedAt > 0 ? startedAt : 0;
    closedAt_ = closedAt > 0 ? closedAt : 0;

    if ((status_ == Status::Assigned || status_ == Status::InProgress) && assignedAt_ == 0) {
        assignedAt_ = registeredAt_;
        repair = "missing assignment time set to registration time";
    }
    if (status_ == Status::InProgress && startedAt_ == 0) {
        startedAt_ = assignedAt_;
        repair = "missing start time repaired";
    }
    if (isClosed() && closedAt_ == 0) {
        closedAt_ = registeredAt_;
        repair = "missing closing time set to registration time (excluded from timing statistics)";
    }
    if (status_ == Status::Pending) {
        assignedAt_ = 0;
        startedAt_ = 0;
        closedAt_ = 0;
    }
    return repair;
}

std::string Emergency::validateId(const std::string& id)
{
    if (id.empty()) {
        return "Emergency ID is required.";
    }
    if (!util::isValidId(id)) {
        return "Emergency ID must be 3-8 characters (letters, digits or '-') and start with a letter.";
    }
    return "";
}

std::string Emergency::validateReporter(const std::string& reporter)
{
    return validateText(reporter, "Reporter name", kMaxReporter);
}

std::string Emergency::validateLocation(const std::string& location)
{
    return validateText(location, "Location", kMaxLocation);
}

std::string Emergency::validateContact(const std::string& contact)
{
    std::string error = validateText(contact, "Contact information", kMaxContact);
    if (!error.empty()) {
        return error;
    }
    int digits = 0;
    for (unsigned char c : contact) {
        if (std::isdigit(c)) {
            ++digits;
        } else if (!(c == '+' || c == '-' || c == ' ' || c == '(' || c == ')' || c == '@' || c == '.' ||
                     c == '_' || std::isalpha(c))) {
            return "Contact may contain only digits, letters, spaces and + - ( ) @ . _";
        }
    }
    const size_t at = contact.find('@');
    const bool looksLikeEmail = at != std::string::npos && at > 0 && contact.find('.', at) != std::string::npos;
    if (digits < 5 && !looksLikeEmail) {
        return "Enter a phone number (at least 5 digits) or an e-mail address.";
    }
    return "";
}

std::string Emergency::validateDescription(const std::string& description)
{
    return validateText(description, "Description", kMaxDescription);
}

std::string Emergency::validateAffected(long long affected)
{
    if (affected < 0 || affected > kMaxAffected) {
        return "People affected must be between 0 and " + std::to_string(kMaxAffected) + ".";
    }
    return "";
}

}
