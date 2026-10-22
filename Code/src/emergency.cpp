#include "emergency.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace resq {

static_assert(40 + 40 + 10 < 100, "Extra points must stay below one severity band");

namespace {

void require(const std::string& error) {
    if (!error.empty()) throw std::invalid_argument(error);
}

}

Emergency::Emergency(std::string id, std::string reporter, Category category, std::string location, std::string contact,
                     std::string description, Severity severity, int affected, std::time_t registered, bool demo)
    : id_(util::upper(util::trim(id))), reporter_(util::trim(reporter)), category_(category),
      location_(util::trim(location)), contact_(util::trim(contact)), description_(util::trim(description)),
      severity_(severity), affected_(affected), registered_(registered), demo_(demo) {
    require(util::validId(id_) ? "" : "ID must be 3-8 letters, digits or '-', starting with a letter.");
    require(util::checkText(reporter_, "Reporter name", 40));
    require(util::checkText(location_, "Location", 40));
    require(checkContact(contact_));
    require(util::checkText(description_, "Description", 200));
    require(checkAffected(affected_));
    require(registered_ > 0 ? "" : "Registration time is invalid.");
    refresh(registered_);
}

bool Emergency::active() const {
    return status_ == Status::Pending || status_ == Status::Assigned || status_ == Status::InProgress;
}

Priority Emergency::priority(std::time_t now) const {
    static const int categoryPoints[] = {8, 10, 6, 8, 8, 6, 0};
    Priority p;
    p.severity = (5 - static_cast<int>(severity_)) * 100;
    p.people = affected_ <= 0 ? 0 : std::min(40, static_cast<int>(std::lround(10 * std::log2(affected_ + 1.0))));
    const std::time_t end =
        status_ == Status::Pending ? now : (assignedAt_ > 0 ? assignedAt_ : (closedAt_ > 0 ? closedAt_ : now));
    p.minutes = std::max<long long>(0, static_cast<long long>(end - registered_) / 60);
    p.waiting = static_cast<int>(std::min<long long>(40, p.minutes / 3));
    p.category = categoryPoints[idx(category_)];
    return p;
}

void Emergency::refresh(std::time_t now) { score_ = priority(now).total(); }

void Emergency::requireActive() const {
    if (!active()) throw std::logic_error("Case " + id_ + " is closed; only its description can be changed.");
}

void Emergency::setSeverity(Severity severity) {
    requireActive();
    severity_ = severity;
}

void Emergency::setLocation(const std::string& location) {
    requireActive();
    require(util::checkText(util::trim(location), "Location", 40));
    location_ = util::trim(location);
}

void Emergency::setDescription(const std::string& description) {
    require(util::checkText(util::trim(description), "Description", 200));
    description_ = util::trim(description);
}

void Emergency::setAffected(int affected) {
    requireActive();
    require(checkAffected(affected));
    affected_ = affected;
}

bool Emergency::canMove(Status from, Status to) {
    switch (from) {
    case Status::Pending: return to == Status::Assigned || to == Status::Cancelled;
    case Status::Assigned: return to == Status::Pending || to == Status::InProgress || to == Status::Cancelled;
    case Status::InProgress: return to == Status::Resolved || to == Status::Cancelled;
    default: return false;
    }
}

bool Emergency::moveTo(Status to, std::time_t now) {
    if (!canMove(status_, to)) return false;
    if (to == Status::Assigned)
        assignedAt_ = now;
    else if (to == Status::Pending)
        assignedAt_ = 0;
    else if (to == Status::InProgress)
        startedAt_ = now;
    else
        closedAt_ = now;
    status_ = to;
    refresh(now);
    return true;
}

void Emergency::restore(Status status, std::time_t assignedAt, std::time_t startedAt, std::time_t closedAt) {
    status_ = status;
    assignedAt_ = assignedAt;
    startedAt_ = startedAt;
    closedAt_ = closedAt;
}

std::string Emergency::checkContact(const std::string& contact) {
    const std::string error = util::checkText(contact, "Contact", 32);
    if (!error.empty()) return error;
    const long digits = std::count_if(contact.begin(), contact.end(),
                                      [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
    if (digits < 5 && contact.find('@') == std::string::npos)
        return "Enter a phone number (at least 5 digits) or an e-mail address.";
    return "";
}

std::string Emergency::checkAffected(long long affected) {
    return (affected < 0 || affected > 1000000) ? "People affected must be between 0 and 1000000." : "";
}

}
