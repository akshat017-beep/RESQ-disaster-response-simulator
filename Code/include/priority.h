#ifndef RESQ_PRIORITY_H
#define RESQ_PRIORITY_H

#include "types.h"

#include <ctime>
#include <string>
#include <vector>

namespace resq {

struct PriorityBreakdown {
    int severityPoints = 0;
    int affectedPoints = 0;
    int waitingPoints = 0;
    int categoryPoints = 0;
    long long waitingMinutes = 0;

    int total() const { return severityPoints + affectedPoints + waitingPoints + categoryPoints; }
};

class PriorityCalculator {
public:
    static constexpr int kBandSize = 100;
    static constexpr int kMaxAffectedPoints = 40;
    static constexpr int kMaxWaitingPoints = 40;
    static constexpr int kMinutesPerWaitingPoint = 3;
    static constexpr int kMaxCategoryPoints = 10;
    static constexpr int kMaxBonus = kMaxAffectedPoints + kMaxWaitingPoints + kMaxCategoryPoints;

    static PriorityBreakdown compute(Severity severity, int affected, Category category, std::time_t registeredAt,
                                     std::time_t waitEnd);

    static int severityPoints(Severity severity);
    static int affectedPoints(int affected);
    static int waitingPoints(long long waitingMinutes);
    static int categoryPoints(Category category);

    static std::vector<std::string> explanation();
};

static_assert(PriorityCalculator::kMaxBonus < PriorityCalculator::kBandSize,
              "Bonus points must never let a lower severity overtake a higher one");

}

#endif
