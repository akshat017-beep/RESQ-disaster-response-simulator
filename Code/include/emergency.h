#ifndef RESQ_EMERGENCY_H
#define RESQ_EMERGENCY_H

#include "types.h"

namespace resq {

struct Priority {
    int severity = 0;
    int people = 0;
    int waiting = 0;
    int category = 0;
    long long minutes = 0;

    int total() const { return severity + people + waiting + category; }
};

class Emergency {
public:
    Emergency(std::string id, std::string reporter, Category category, std::string location, std::string contact,
              std::string description, Severity severity, int affected, std::time_t registered, bool demo = false);

    const std::string& id() const { return id_; }
    const std::string& reporter() const { return reporter_; }
    Category category() const { return category_; }
    const std::string& location() const { return location_; }
    const std::string& contact() const { return contact_; }
    const std::string& description() const { return description_; }
    Severity severity() const { return severity_; }
    int affected() const { return affected_; }
    Status status() const { return status_; }
    std::time_t registered() const { return registered_; }
    std::time_t assignedAt() const { return assignedAt_; }
    std::time_t startedAt() const { return startedAt_; }
    std::time_t closedAt() const { return closedAt_; }
    bool demo() const { return demo_; }
    int score() const { return score_; }
    bool active() const;

    Priority priority(std::time_t now) const;
    void refresh(std::time_t now);

    void setSeverity(Severity severity);
    void setLocation(const std::string& location);
    void setDescription(const std::string& description);
    void setAffected(int affected);

    static bool canMove(Status from, Status to);
    bool moveTo(Status to, std::time_t now);
    void restore(Status status, std::time_t assignedAt, std::time_t startedAt, std::time_t closedAt);

    static std::string checkContact(const std::string& contact);
    static std::string checkAffected(long long affected);

private:
    void requireActive() const;

    std::string id_;
    std::string reporter_;
    Category category_;
    std::string location_;
    std::string contact_;
    std::string description_;
    Severity severity_;
    int affected_;
    Status status_ = Status::Pending;
    std::time_t registered_;
    std::time_t assignedAt_ = 0;
    std::time_t startedAt_ = 0;
    std::time_t closedAt_ = 0;
    bool demo_;
    int score_ = 0;
};

}

#endif
