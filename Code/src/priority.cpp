#include "priority.h"

#include <algorithm>
#include <cmath>

namespace resq {

PriorityBreakdown PriorityCalculator::compute(Severity severity, int affected, Category category,
                                              std::time_t registeredAt, std::time_t waitEnd)
{
    PriorityBreakdown result;
    result.severityPoints = severityPoints(severity);
    result.affectedPoints = affectedPoints(affected);
    long long seconds = static_cast<long long>(waitEnd) - static_cast<long long>(registeredAt);
    if (seconds < 0) {
        seconds = 0;
    }
    result.waitingMinutes = seconds / 60;
    result.waitingPoints = waitingPoints(result.waitingMinutes);
    result.categoryPoints = categoryPoints(category);
    return result;
}

int PriorityCalculator::severityPoints(Severity severity)
{
    switch (severity) {
    case Severity::Critical: return 4 * kBandSize;
    case Severity::High: return 3 * kBandSize;
    case Severity::Medium: return 2 * kBandSize;
    case Severity::Low: return 1 * kBandSize;
    }
    return 0;
}

int PriorityCalculator::affectedPoints(int affected)
{
    if (affected <= 0) {
        return 0;
    }
    const long points = std::lround(10.0 * std::log2(static_cast<double>(affected) + 1.0));
    return static_cast<int>(std::min<long>(points, kMaxAffectedPoints));
}

int PriorityCalculator::waitingPoints(long long waitingMinutes)
{
    if (waitingMinutes <= 0) {
        return 0;
    }
    const long long points = waitingMinutes / kMinutesPerWaitingPoint;
    return static_cast<int>(std::min<long long>(points, kMaxWaitingPoints));
}

int PriorityCalculator::categoryPoints(Category category)
{
    switch (category) {
    case Category::Fire: return 10;
    case Category::Medical: return 8;
    case Category::NaturalDisaster: return 8;
    case Category::Flood: return 8;
    case Category::RoadAccident: return 6;
    case Category::Rescue: return 6;
    case Category::Other: return 0;
    }
    return 0;
}

std::vector<std::string> PriorityCalculator::explanation()
{
    return {
        "Score = Severity band + People affected + Waiting time + Category",
        "",
        "Severity band  : Critical 400, High 300, Medium 200, Low 100",
        "People affected: round(10 x log2(n + 1)), maximum 40",
        "                 (1 person = 10, 3 = 20, 7 = 30, 15 or more = 40)",
        "Waiting time   : 1 point per 3 minutes while unassigned, maximum 40",
        "Category       : Fire 10; Medical, Natural disaster, Flood 8;",
        "                 Road accident, Rescue operation 6; Other 0",
        "",
        "The extra points can add at most 90, which is less than one severity",
        "band (100). A Critical case therefore always ranks above any High,",
        "Medium or Low case. Ties are broken by the earlier registration time.",
        "",
        "LIMITATION: the score only orders the queue. It is not a prediction",
        "of risk or outcome. The weights are illustrative, not validated",
        "clinically or operationally. A human operator makes every decision.",
    };
}

}
