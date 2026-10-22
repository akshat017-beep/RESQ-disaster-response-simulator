#ifndef RESQ_TYPES_H
#define RESQ_TYPES_H

#include <ctime>
#include <string>
#include <vector>

namespace resq {

enum class Category { Medical = 1, Fire, RoadAccident, NaturalDisaster, Flood, Rescue, Other };
enum class Severity { Critical = 1, High, Medium, Low };
enum class Status { Pending = 1, Assigned, InProgress, Resolved, Cancelled };
enum class ResourceType { Ambulance = 1, MedicalTeam, FireEngine, RescueTeam, ReliefVehicle, SupplyKit };

const int kCategories = 7;
const int kSeverities = 4;
const int kStatuses = 5;
const int kResourceTypes = 6;

template <typename E> int idx(E value) { return static_cast<int>(value) - 1; }

std::string name(Category value);
std::string name(Severity value);
std::string name(Status value);
std::string name(ResourceType value);

namespace util {

std::string trim(const std::string& text);
std::string upper(std::string text);
bool icontains(const std::string& text, const std::string& part);
bool toInt(const std::string& text, long long& out);
bool validId(const std::string& id);
std::string checkText(const std::string& value, const std::string& field, size_t maxLength);
std::string timeText(std::time_t value);
std::string duration(long long seconds);
bool parseTime(const std::string& text, std::time_t& out);
std::string escape(const std::string& text);
std::vector<std::string> split(const std::string& line, char separator = '|');

}

}

#endif
