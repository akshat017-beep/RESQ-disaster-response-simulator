#ifndef RESQ_TYPES_H
#define RESQ_TYPES_H

#include <string>
#include <vector>

namespace resq {

enum class Category { Medical = 1, Fire, RoadAccident, NaturalDisaster, Flood, Rescue, Other };
enum class Severity { Critical = 1, High, Medium, Low };
enum class Status { Pending = 1, Assigned, InProgress, Resolved, Cancelled };
enum class ResourceType { Ambulance = 1, MedicalTeam, FireEngine, RescueTeam, ReliefVehicle, SupplyKit };
enum class ResourceStatus { Available = 1, Deployed, OutOfService };

constexpr int kCategoryCount = 7;
constexpr int kSeverityCount = 4;
constexpr int kStatusCount = 5;
constexpr int kResourceTypeCount = 6;

template <typename E>
constexpr int indexOf(E value) { return static_cast<int>(value) - 1; }

const std::vector<Category>& allCategories();
const std::vector<Severity>& allSeverities();
const std::vector<Status>& allStatuses();
const std::vector<ResourceType>& allResourceTypes();
const std::vector<ResourceStatus>& allResourceStatuses();

std::string toString(Category value);
std::string toString(Severity value);
std::string toString(Status value);
std::string toString(ResourceType value);
std::string toString(ResourceStatus value);

std::string shortName(Category value);
std::string shortName(ResourceType value);

std::string toCode(Category value);
std::string toCode(Severity value);
std::string toCode(Status value);
std::string toCode(ResourceType value);

bool fromCode(const std::string& code, Category& out);
bool fromCode(const std::string& code, Severity& out);
bool fromCode(const std::string& code, Status& out);
bool fromCode(const std::string& code, ResourceType& out);

bool fromNumber(int number, Category& out);
bool fromNumber(int number, Severity& out);
bool fromNumber(int number, Status& out);
bool fromNumber(int number, ResourceType& out);

}

#endif
