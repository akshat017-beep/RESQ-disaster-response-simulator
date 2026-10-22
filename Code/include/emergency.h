#ifndef RESQ_EMERGENCY_H
#define RESQ_EMERGENCY_H

#include "priority.h"
#include "types.h"

#include <ctime>
#include <string>

namespace resq {

class Emergency {
public:
    static constexpr size_t kMaxReporter = 40;
    static constexpr size_t kMaxLocation = 40;
    static constexpr size_t kMaxContact = 32;
    static constexpr size_t kMaxDescription = 200;
    static constexpr long long kMaxAffected = 1000000;

    Emergency(std::string id, std::string reporter, Category category, std::string location, std::string contact,
              std::string description, Severity severity, int affected, std::time_t registeredAt,
              bool demo = false);

    const std::string& id() const { return id_; }
    const std::string& reporter() const { return reporter_; }
    Category category() const { return category_; }
    const std::string& location() const { return location_; }
    const std::string& contact() const { return contact_; }
    const std::string& description() const { return description_; }
    Severity severity() const { return severity_; }
    int affected() const { return affected_; }
    Status status() const { return status_; }
    std::time_t registeredAt() const { return registeredAt_; }
    std::time_t assignedAt() const { return assignedAt_; }
    std::time_t startedAt() const { return startedAt_; }
    std::time_t closedAt() const { return closedAt_; }
    bool isDemo() const { return demo_; }
    int priorityScore() const { return priorityScore_; }

    bool isActive() const;
    bool isClosed() const { return !isActive(); }

    std::time_t waitEnd(std::time_t now) const;
    PriorityBreakdown priorityBreakdown(std::time_t now) const;
    int recalculatePriority(std::time_t now);

    void setSeverity(Severity severity);
    void setLocation(const std::string& location);
    void setDescription(const std::string& description);
    void setAffected(int affected);

    static bool isTransitionAllowed(Status from, Status to);
    void transitionTo(Status target, std::time_t now);

    std::string restoreState(Status status, std::time_t assignedAt, std::time_t startedAt, std::time_t closedAt);

    static std::string validateId(const std::string& id);
    static std::string validateReporter(const std::string& reporter);
    static std::string validateLocation(const std::string& location);
    static std::string validateContact(const std::string& contact);
    static std::string validateDescription(const std::string& description);
    static std::string validateAffected(long long affected);

private:
    void requireOpen(const char* field) const;

    std::string id_;
    std::string reporter_;
    Category category_;
    std::string location_;
    std::string contact_;
    std::string description_;
    Severity severity_;
    int affected_;
    Status status_ = Status::Pending;
    std::time_t registeredAt_;
    std::time_t assignedAt_ = 0;
    std::time_t startedAt_ = 0;
    std::time_t closedAt_ = 0;
    bool demo_;
    int priorityScore_ = 0;
};

}

#endif
