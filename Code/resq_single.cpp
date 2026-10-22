#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef RESQ_C_H
#define RESQ_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int resq_enable_ansi(void);
int resq_is_interactive(void);
int resq_make_dir(const char* path);
int resq_encode(const char* in, char* out, size_t size);
int resq_decode(const char* in, char* out, size_t size);

#ifdef __cplusplus
}
#endif

#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define RESQ_ISATTY _isatty
#else
#include <sys/stat.h>
#include <unistd.h>
#define RESQ_ISATTY isatty
#endif

int resq_enable_ansi(void) {
    const char* noColor = getenv("NO_COLOR");
    if ((noColor != NULL && noColor[0] != '\0') || !RESQ_ISATTY(1)) return 0;
#ifdef _WIN32
    {
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (!GetConsoleMode(handle, &mode)) return 0;
        return SetConsoleMode(handle, mode | 0x0004) ? 1 : 0;
    }
#else
    {
        const char* term = getenv("TERM");
        return (term != NULL && strcmp(term, "dumb") == 0) ? 0 : 1;
    }
#endif
}

int resq_is_interactive(void) { return RESQ_ISATTY(0) ? 1 : 0; }

int resq_make_dir(const char* path) {
#ifdef _WIN32
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    return (result == 0 || errno == EEXIST) ? 1 : 0;
}

static unsigned char resq_key(size_t i) { return (unsigned char)(0x5A ^ ((i * 31u) & 0xFFu)); }

int resq_encode(const char* in, char* out, size_t size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t i;
    size_t n = strlen(in);
    if (size < n * 2 + 1) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char b = (unsigned char)((unsigned char)in[i] ^ resq_key(i));
        out[2 * i] = hex[b >> 4];
        out[2 * i + 1] = hex[b & 15];
    }
    out[2 * n] = '\0';
    return 1;
}

static int resq_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int resq_decode(const char* in, char* out, size_t size) {
    size_t i;
    size_t n = strlen(in);
    if (n % 2 != 0 || size < n / 2 + 1) return 0;
    for (i = 0; i < n / 2; ++i) {
        int hi = resq_hex(in[2 * i]);
        int lo = resq_hex(in[2 * i + 1]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = (char)(((hi << 4) | lo) ^ resq_key(i));
    }
    out[n / 2] = '\0';
    return 1;
}
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
#ifndef RESQ_EMERGENCY_H
#define RESQ_EMERGENCY_H


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
#ifndef RESQ_RESOURCE_H
#define RESQ_RESOURCE_H


namespace resq {

class Resource {
public:
    Resource(std::string id, ResourceType type, std::string location, int capacity, int maxShared = 1,
             bool demo = false);

    const std::string& id() const { return id_; }
    ResourceType type() const { return type_; }
    const std::string& location() const { return location_; }
    int capacity() const { return capacity_; }
    int maxShared() const { return maxShared_; }
    bool outOfService() const { return outOfService_; }
    bool demo() const { return demo_; }
    const std::vector<std::string>& assigned() const { return assigned_; }

    bool available() const;
    bool assignedTo(const std::string& emergencyId) const;
    std::string statusText() const;

    std::string assign(const std::string& emergencyId);
    bool release(const std::string& emergencyId);
    std::string setOutOfService(bool value);
    void setLocation(const std::string& location);

private:
    std::string id_;
    ResourceType type_;
    std::string location_;
    int capacity_;
    int maxShared_;
    bool demo_;
    bool outOfService_ = false;
    std::vector<std::string> assigned_;
};

}

#endif
#ifndef RESQ_EMERGENCY_MANAGER_H
#define RESQ_EMERGENCY_MANAGER_H


#include <map>

namespace resq {

enum class SortBy { Severity, Priority, Time, Id };

class EmergencyManager {
public:
    std::string add(const Emergency& emergency);
    Emergency* find(const std::string& id);
    std::vector<const Emergency*> list() const;
    std::vector<const Emergency*> sorted(SortBy by) const;
    std::vector<const Emergency*> search(int field, const std::string& query) const;
    std::vector<const Emergency*> byCategory(Category category) const;
    std::vector<const Emergency*> queue(std::time_t now);
    void refresh(std::time_t now);
    std::string nextId() const;
    void removeDemo();
    void clear() { items_.clear(); }
    size_t size() const { return items_.size(); }

    static bool before(const Emergency* a, const Emergency* b);

private:
    std::map<std::string, Emergency> items_;
};

}

#endif
#ifndef RESQ_RESOURCE_MANAGER_H
#define RESQ_RESOURCE_MANAGER_H


namespace resq {

class ResourceManager {
public:
    std::string add(const Resource& resource);
    Resource* find(const std::string& id);
    std::vector<const Resource*> list() const;
    std::vector<const Resource*> search(int field, const std::string& query) const;
    std::vector<const Resource*> assignedTo(const std::string& emergencyId) const;
    std::vector<const Resource*> suitableFor(const Emergency& emergency) const;
    static std::vector<ResourceType> recommended(Category category);

    std::string assign(Emergency& emergency, const std::string& resourceId, std::time_t now);
    std::string release(Emergency& emergency, const std::string& resourceId, std::time_t now);
    std::string changeStatus(Emergency& emergency, Status to, std::time_t now);
    int releaseAll(const std::string& emergencyId);
    void removeDemo(EmergencyManager& emergencies, std::time_t now);

    std::string nextId() const;
    void clear() { items_.clear(); }
    size_t size() const { return items_.size(); }

private:
    std::map<std::string, Resource> items_;
};

}

#endif
#ifndef RESQ_FILE_MANAGER_H
#define RESQ_FILE_MANAGER_H


#include <functional>

namespace resq {

class FileManager {
public:
    explicit FileManager(std::string folder);

    std::vector<std::string> load(EmergencyManager& emergencies, ResourceManager& resources, std::time_t now);
    bool save(const EmergencyManager& emergencies, const ResourceManager& resources, std::string& error) const;
    bool foundData() const { return found_; }
    const std::string& folder() const { return folder_; }

private:
    using Handler = std::function<std::string(const std::vector<std::string>&)>;

    std::string path(const std::string& file) const { return folder_ + "/" + file; }
    void read(const std::string& file, const std::string& header, std::vector<std::string>& warnings,
              const Handler& handle);
    bool write(const std::string& file, const std::string& header, const std::vector<std::string>& lines,
               std::string& error) const;

    std::string folder_;
    bool found_ = false;
};

}

#endif
#ifndef RESQ_ANALYTICS_H
#define RESQ_ANALYTICS_H


namespace resq {

struct Stats {
    int total = 0;
    int activeCritical = 0;
    int status[kStatuses] = {};
    int severity[kSeverities] = {};
    int category[kCategories] = {};
    int resources = 0;
    int available = 0;
    int deployed = 0;
    int outOfService = 0;
    int responseCount = 0;
    double averageResponse = 0;
    int resolvedCount = 0;
    double averageResolution = 0;

    int count(Status s) const { return status[idx(s)]; }
    int open() const { return count(Status::Pending) + count(Status::Assigned) + count(Status::InProgress); }
};

class AnalyticsManager {
public:
    static Stats compute(const EmergencyManager& emergencies, const ResourceManager& resources);
};

}

#endif
#ifndef RESQ_CONSOLE_H
#define RESQ_CONSOLE_H

#include <iostream>
#include <string>
#include <vector>

namespace resq {

enum class Color { None, Red, Green, Yellow, Cyan, Magenta, Gray, Bold };

struct InputClosed {};
struct InputCancelled {};

class Console {
public:
    Console(std::istream& in, std::ostream& out, bool color, bool interactive);

    std::ostream& out;

    std::string paint(const std::string& text, Color color) const;
    static std::string pad(const std::string& text, size_t width);
    void clear() const;
    void logo() const;
    void title(const std::string& text) const;
    void box(const std::string& heading, const std::vector<std::string>& lines, Color color = Color::Cyan) const;
    void bar(const std::string& label, int value, int maximum, Color color) const;
    void menu(const std::vector<std::string>& items) const;

    void ok(const std::string& message) const;
    void warn(const std::string& message) const;
    void error(const std::string& message) const;

    std::string line(const std::string& prompt) const;
    std::string choice(const std::string& prompt) const;
    std::string text(const std::string& prompt, size_t maxLength, bool required = true) const;
    long long number(const std::string& prompt, long long minimum, long long maximum) const;
    long long numberOr(const std::string& prompt, long long minimum, long long maximum, long long fallback) const;
    bool confirm(const std::string& prompt, bool yesByDefault = false) const;
    void pause() const;

private:
    std::istream& in_;
    bool color_;
    bool interactive_;
};

}

#endif
#ifndef RESQ_APP_H
#define RESQ_APP_H


namespace resq {

class RESQApplication {
public:
    RESQApplication(const std::string& folder, bool color);
    int run();

private:
    std::time_t now() const { return std::time(nullptr); }
    bool save();
    void dashboard();
    void registerEmergency();
    void viewEmergencies();
    void searchEmergencies();
    void updateEmergency();
    void changeStatus(Emergency& emergency);
    void dispatchMenu();
    void processNext();
    void activeOperations();
    void manageOperation(Emergency& emergency);
    void assignFlow(Emergency& emergency);
    void resourceMenu();
    void addResource();
    void searchResources();
    void updateResource();
    void releaseResource();
    void reports();
    void demoMenu();
    void loadDemo();

    void showCase(const Emergency& emergency);
    void emergencyTable(const std::vector<const Emergency*>& list);
    void resourceTable(const std::vector<const Resource*>& list);
    Emergency* askEmergency(const std::string& prompt);
    Resource* askResource(const std::string& prompt);
    template <typename E> E pick(const std::string& label, int count);

    Console console_;
    EmergencyManager emergencies_;
    ResourceManager resources_;
    FileManager files_;
};

}

#endif

#include <cctype>
#include <cstdio>
#include <sstream>

namespace resq {

namespace {

const char* kCategoryNames[] = {"Medical emergency", "Fire", "Road accident", "Natural disaster", "Flood",
                                "Rescue operation",  "Other"};
const char* kSeverityNames[] = {"Critical", "High", "Medium", "Low"};
const char* kStatusNames[] = {"Pending", "Assigned", "In Progress", "Resolved", "Cancelled"};
const char* kTypeNames[] = {"Ambulance", "Medical team", "Fire engine", "Rescue team", "Relief vehicle", "Supply kit"};

}

std::string name(Category value) { return kCategoryNames[idx(value)]; }
std::string name(Severity value) { return kSeverityNames[idx(value)]; }
std::string name(Status value) { return kStatusNames[idx(value)]; }
std::string name(ResourceType value) { return kTypeNames[idx(value)]; }

namespace util {

std::string trim(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

std::string upper(std::string text) {
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

bool icontains(const std::string& text, const std::string& part) {
    return upper(text).find(upper(part)) != std::string::npos;
}

bool toInt(const std::string& text, long long& out) {
    const std::string t = trim(text);
    if (t.empty() || t == "-" || t.size() > 12) return false;
    for (size_t i = 0; i < t.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(t[i])) && !(i == 0 && t[i] == '-')) return false;
    out = std::stoll(t);
    return true;
}

bool validId(const std::string& id) {
    if (id.size() < 3 || id.size() > 8 || !std::isalpha(static_cast<unsigned char>(id[0]))) return false;
    for (char c : id)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-') return false;
    return true;
}

std::string checkText(const std::string& value, const std::string& field, size_t maxLength) {
    if (value.empty()) return field + " is required.";
    if (value.size() > maxLength) return field + " must be at most " + std::to_string(maxLength) + " characters.";
    for (unsigned char c : value)
        if (c < 32) return field + " contains invalid characters.";
    return "";
}

std::string timeText(std::time_t value) {
    const std::tm* local = value > 0 ? std::localtime(&value) : nullptr;
    char buffer[32];
    if (local == nullptr || std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", local) == 0) return "-";
    return buffer;
}

std::string duration(long long seconds) {
    if (seconds < 0) return "-";
    const long long minutes = seconds / 60;
    std::ostringstream out;
    if (minutes >= 1440)
        out << minutes / 1440 << "d " << (minutes % 1440) / 60 << "h";
    else if (minutes >= 60)
        out << minutes / 60 << "h " << minutes % 60 << "m";
    else
        out << minutes << "m";
    return out.str();
}

bool parseTime(const std::string& text, std::time_t& out) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0;
    char extra = 0;
    if (std::sscanf(trim(text).c_str(), "%d-%d-%d %d:%d%c", &year, &month, &day, &hour, &minute, &extra) != 5 ||
        year < 2000 || year > 2100 || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_isdst = -1;
    out = std::mktime(&tm);
    return out != static_cast<std::time_t>(-1) && tm.tm_mday == day && tm.tm_mon == month - 1;
}

std::string escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\\' || c == '|') out += '\\';
        if (c != '\n' && c != '\r') out += c;
    }
    return out;
}

std::vector<std::string> split(const std::string& line, char separator) {
    std::vector<std::string> parts(1);
    for (size_t i = 0; i < line.size(); ++i)
        if (line[i] == '\\' && i + 1 < line.size())
            parts.back() += line[++i];
        else if (line[i] == separator)
            parts.emplace_back();
        else
            parts.back() += line[i];
    return parts;
}

}

}

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

#include <algorithm>
#include <queue>

namespace resq {

std::string EmergencyManager::add(const Emergency& emergency) {
    if (items_.count(emergency.id()) > 0) return "An emergency with ID " + emergency.id() + " already exists.";
    items_.emplace(emergency.id(), emergency);
    return "";
}

Emergency* EmergencyManager::find(const std::string& id) {
    const auto it = items_.find(util::upper(util::trim(id)));
    return it == items_.end() ? nullptr : &it->second;
}

std::vector<const Emergency*> EmergencyManager::list() const {
    std::vector<const Emergency*> result;
    for (const auto& item : items_) result.push_back(&item.second);
    return result;
}

bool EmergencyManager::before(const Emergency* a, const Emergency* b) {
    if (a->score() != b->score()) return a->score() > b->score();
    if (a->registered() != b->registered()) return a->registered() < b->registered();
    return a->id() < b->id();
}

std::vector<const Emergency*> EmergencyManager::sorted(SortBy by) const {
    std::vector<const Emergency*> result = list();
    if (by == SortBy::Priority) {
        std::sort(result.begin(), result.end(), before);
    } else if (by == SortBy::Severity) {
        std::sort(result.begin(), result.end(), [](const Emergency* a, const Emergency* b) {
            return a->severity() != b->severity() ? a->severity() < b->severity() : before(a, b);
        });
    } else if (by == SortBy::Time) {
        std::sort(result.begin(), result.end(), [](const Emergency* a, const Emergency* b) {
            return a->registered() != b->registered() ? a->registered() < b->registered() : a->id() < b->id();
        });
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::search(int field, const std::string& query) const {
    std::vector<const Emergency*> result;
    const std::string q = util::trim(query);
    for (const Emergency* e : list()) {
        const std::string& text = field == 1 ? e->id() : (field == 2 ? e->reporter() : e->location());
        if (!q.empty() && util::icontains(text, q)) result.push_back(e);
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::byCategory(Category category) const {
    std::vector<const Emergency*> result;
    for (const Emergency* e : list())
        if (e->category() == category) result.push_back(e);
    return result;
}

std::vector<const Emergency*> EmergencyManager::queue(std::time_t now) {
    refresh(now);
    const auto lower = [](const Emergency* a, const Emergency* b) { return before(b, a); };
    std::priority_queue<const Emergency*, std::vector<const Emergency*>, decltype(lower)> pending(lower);
    for (const auto& item : items_)
        if (item.second.status() == Status::Pending) pending.push(&item.second);
    std::vector<const Emergency*> result;
    while (!pending.empty()) {
        result.push_back(pending.top());
        pending.pop();
    }
    return result;
}

void EmergencyManager::refresh(std::time_t now) {
    for (auto& item : items_) item.second.refresh(now);
}

std::string EmergencyManager::nextId() const {
    long long highest = 1000;
    for (const auto& item : items_) {
        long long number = 0;
        if (item.first.compare(0, 2, "E-") == 0 && util::toInt(item.first.substr(2), number))
            highest = std::max(highest, number);
    }
    return "E-" + std::to_string(highest + 1);
}

void EmergencyManager::removeDemo() {
    for (auto it = items_.begin(); it != items_.end();) it = it->second.demo() ? items_.erase(it) : std::next(it);
}

}

#include <algorithm>

namespace resq {

std::string ResourceManager::add(const Resource& resource) {
    if (items_.count(resource.id()) > 0) return "A resource with ID " + resource.id() + " already exists.";
    items_.emplace(resource.id(), resource);
    return "";
}

Resource* ResourceManager::find(const std::string& id) {
    const auto it = items_.find(util::upper(util::trim(id)));
    return it == items_.end() ? nullptr : &it->second;
}

std::vector<const Resource*> ResourceManager::list() const {
    std::vector<const Resource*> result;
    for (const auto& item : items_) result.push_back(&item.second);
    return result;
}

std::vector<const Resource*> ResourceManager::search(int field, const std::string& query) const {
    std::vector<const Resource*> result;
    const std::string q = util::trim(query);
    for (const Resource* r : list()) {
        const std::string text = field == 1   ? r->id()
                                 : field == 2 ? name(r->type())
                                 : field == 3 ? r->location()
                                              : r->statusText();
        if (!q.empty() && util::icontains(text, q)) result.push_back(r);
    }
    return result;
}

std::vector<const Resource*> ResourceManager::assignedTo(const std::string& emergencyId) const {
    std::vector<const Resource*> result;
    for (const Resource* r : list())
        if (r->assignedTo(emergencyId)) result.push_back(r);
    return result;
}

std::vector<ResourceType> ResourceManager::recommended(Category category) {
    using T = ResourceType;
    switch (category) {
    case Category::Medical: return {T::Ambulance, T::MedicalTeam};
    case Category::Fire: return {T::FireEngine, T::Ambulance, T::RescueTeam};
    case Category::RoadAccident: return {T::Ambulance, T::RescueTeam, T::MedicalTeam};
    case Category::NaturalDisaster: return {T::RescueTeam, T::ReliefVehicle, T::MedicalTeam, T::SupplyKit};
    case Category::Flood: return {T::RescueTeam, T::ReliefVehicle, T::SupplyKit, T::Ambulance};
    case Category::Rescue: return {T::RescueTeam, T::MedicalTeam, T::Ambulance};
    default: return {T::Ambulance, T::MedicalTeam, T::FireEngine, T::RescueTeam, T::ReliefVehicle, T::SupplyKit};
    }
}

std::vector<const Resource*> ResourceManager::suitableFor(const Emergency& emergency) const {
    std::vector<const Resource*> result;
    for (ResourceType type : recommended(emergency.category())) {
        for (const Resource* r : list())
            if (r->type() == type && r->available() && !r->assignedTo(emergency.id())) result.push_back(r);
    }
    return result;
}

std::string ResourceManager::assign(Emergency& emergency, const std::string& resourceId, std::time_t now) {
    if (!emergency.active())
        return "Case " + emergency.id() + " is " + name(emergency.status()) + " and cannot receive resources.";
    Resource* resource = find(resourceId);
    if (resource == nullptr) return "No resource with ID " + util::upper(util::trim(resourceId)) + " exists.";
    const std::string error = resource->assign(emergency.id());
    if (error.empty() && emergency.status() == Status::Pending) emergency.moveTo(Status::Assigned, now);
    return error;
}

std::string ResourceManager::release(Emergency& emergency, const std::string& resourceId, std::time_t now) {
    Resource* resource = find(resourceId);
    if (resource == nullptr || !resource->assignedTo(emergency.id()))
        return "That resource is not assigned to " + emergency.id() + ".";
    if (emergency.status() == Status::InProgress && assignedTo(emergency.id()).size() == 1)
        return "It is the last resource of an In Progress case. Resolve or cancel the case instead.";
    resource->release(emergency.id());
    if (emergency.status() == Status::Assigned && assignedTo(emergency.id()).empty())
        emergency.moveTo(Status::Pending, now);
    return "";
}

std::string ResourceManager::changeStatus(Emergency& emergency, Status to, std::time_t now) {
    if (!Emergency::canMove(emergency.status(), to))
        return "Cannot change " + emergency.id() + " from " + name(emergency.status()) + " to " + name(to) + ".";
    if (to == Status::Assigned) return "A case becomes Assigned when a resource is assigned to it.";
    if (to == Status::InProgress && assignedTo(emergency.id()).empty()) return "Assign a resource first.";
    if (to != Status::InProgress) releaseAll(emergency.id());
    emergency.moveTo(to, now);
    return "";
}

int ResourceManager::releaseAll(const std::string& emergencyId) {
    int released = 0;
    for (auto& item : items_) released += item.second.release(emergencyId) ? 1 : 0;
    return released;
}

void ResourceManager::removeDemo(EmergencyManager& emergencies, std::time_t now) {
    for (auto it = items_.begin(); it != items_.end();) {
        if (!it->second.demo()) {
            ++it;
            continue;
        }
        for (const std::string& emergencyId : std::vector<std::string>(it->second.assigned())) {
            it->second.release(emergencyId);
            Emergency* e = emergencies.find(emergencyId);
            if (e != nullptr && e->status() == Status::Assigned && assignedTo(emergencyId).empty())
                e->moveTo(Status::Pending, now);
        }
        it = items_.erase(it);
    }
    for (const Emergency* e : emergencies.list())
        if (e->demo()) releaseAll(e->id());
    emergencies.removeDemo();
}

std::string ResourceManager::nextId() const {
    long long highest = 100;
    for (const auto& item : items_) {
        long long number = 0;
        if (item.first.compare(0, 2, "R-") == 0 && util::toInt(item.first.substr(2), number))
            highest = std::max(highest, number);
    }
    return "R-" + std::to_string(highest + 1);
}

}


#include <cstdio>
#include <fstream>

namespace resq {

namespace {

const char* kEmergencyHeader = "RESQ-EMERGENCIES-1";
const char* kResourceHeader = "RESQ-RESOURCES-1";

std::string encode(const std::string& text) {
    std::vector<char> buffer(text.size() * 2 + 1);
    resq_encode(text.c_str(), buffer.data(), buffer.size());
    return buffer.data();
}

bool decode(const std::string& text, std::string& out) {
    std::vector<char> buffer(text.size() / 2 + 1);
    if (!resq_decode(text.c_str(), buffer.data(), buffer.size())) return false;
    out = buffer.data();
    return true;
}

std::string join(const std::vector<std::string>& fields) {
    std::string line;
    for (size_t i = 0; i < fields.size(); ++i) line += (i == 0 ? "" : "|") + util::escape(fields[i]);
    return line;
}

bool numbers(const std::vector<std::string>& fields, const std::vector<int>& positions, std::vector<long long>& out) {
    out.assign(fields.size(), 0);
    for (int p : positions) {
        if (!util::toInt(fields[static_cast<size_t>(p)], out[static_cast<size_t>(p)]) ||
            out[static_cast<size_t>(p)] < 0) {
            return false;
        }
    }
    return true;
}

}

FileManager::FileManager(std::string folder) : folder_(folder.empty() ? "data" : folder) {}

void FileManager::read(const std::string& file, const std::string& header, std::vector<std::string>& warnings,
                       const Handler& handle) {
    std::ifstream in(path(file));
    if (!in) return;
    found_ = true;
    const size_t before = warnings.size();
    std::string line;
    int number = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (++number == 1 && line != header) {
            warnings.push_back(file + ": unknown file format, file not loaded.");
            break;
        }
        if (number == 1 || util::trim(line).empty()) continue;
        std::string error;
        try {
            error = handle(util::split(line));
        } catch (const std::exception& ex) {
            error = ex.what();
        }
        if (!error.empty()) warnings.push_back(file + " line " + std::to_string(number) + ": " + error + " (skipped)");
    }
    in.close();
    if (warnings.size() > before) {
        std::ifstream original(path(file), std::ios::binary);
        std::ofstream copy(path(file) + ".damaged", std::ios::binary);
        copy << original.rdbuf();
        warnings.push_back("A copy of the original file was kept as " + path(file) + ".damaged");
    }
}

std::vector<std::string> FileManager::load(EmergencyManager& emergencies, ResourceManager& resources, std::time_t now) {
    std::vector<std::string> warnings;
    emergencies.clear();
    resources.clear();
    found_ = false;

    read("emergencies.txt", kEmergencyHeader, warnings, [&](const std::vector<std::string>& f) -> std::string {
        std::vector<long long> n;
        if (f.size() != 14 || !numbers(f, {2, 6, 7, 8, 9, 10, 11, 12, 13}, n))
            return "wrong number of fields or invalid number";
        if (n[2] < 1 || n[2] > kCategories || n[6] < 1 || n[6] > kSeverities || n[8] < 1 || n[8] > kStatuses ||
            n[7] > 1000000 || n[13] > 1) {
            return "value out of range";
        }
        std::string contact;
        if (!decode(f[4], contact)) return "unreadable contact field";
        Emergency e(f[0], f[1], static_cast<Category>(n[2]), f[3], contact, f[5], static_cast<Severity>(n[6]),
                    static_cast<int>(n[7]), static_cast<std::time_t>(n[9]), n[13] == 1);
        e.restore(static_cast<Status>(n[8]), n[10], n[11], n[12]);
        return emergencies.add(e);
    });

    read("resources.txt", kResourceHeader, warnings, [&](const std::vector<std::string>& f) -> std::string {
        std::vector<long long> n;
        if (f.size() != 8 || !numbers(f, {1, 3, 4, 5, 7}, n)) return "wrong number of fields or invalid number";
        if (n[1] < 1 || n[1] > kResourceTypes || n[3] > 10000 || n[4] > 20 || n[5] > 1 || n[7] > 1)
            return "value out of range";
        Resource r(f[0], static_cast<ResourceType>(n[1]), f[2], static_cast<int>(n[3]), static_cast<int>(n[4]),
                   n[7] == 1);
        for (const std::string& id : util::split(f[6], ',')) {
            const Emergency* e = emergencies.find(id);
            if (!id.empty() && e != nullptr && e->active()) r.assign(e->id());
        }
        if (n[5] == 1) r.setOutOfService(true);
        return resources.add(r);
    });

    for (const Emergency* e : emergencies.list()) {
        if (e->status() == Status::Assigned && resources.assignedTo(e->id()).empty()) {
            emergencies.find(e->id())->moveTo(Status::Pending, now);
            warnings.push_back("Case " + e->id() + " had no resources and was returned to Pending.");
        }
    }
    emergencies.refresh(now);
    return warnings;
}

bool FileManager::write(const std::string& file, const std::string& header, const std::vector<std::string>& lines,
                        std::string& error) const {
    const std::string target = path(file);
    const std::string temp = target + ".tmp";
    const std::string backup = target + ".bak";
    {
        std::ofstream out(temp, std::ios::trunc);
        out << header << '\n';
        for (const std::string& line : lines) out << line << '\n';
        out.flush();
        if (!out) {
            error = "Cannot write " + temp;
            std::remove(temp.c_str());
            return false;
        }
    }
    std::remove(backup.c_str());
    std::rename(target.c_str(), backup.c_str());
    if (std::rename(temp.c_str(), target.c_str()) != 0) {
        std::rename(backup.c_str(), target.c_str());
        error = "Cannot replace " + target;
        return false;
    }
    return true;
}

bool FileManager::save(const EmergencyManager& emergencies, const ResourceManager& resources,
                       std::string& error) const {
    if (!resq_make_dir(folder_.c_str())) {
        error = "Cannot create folder " + folder_;
        return false;
    }
    std::vector<std::string> lines;
    for (const Emergency* e : emergencies.list()) {
        lines.push_back(join({e->id(), e->reporter(), std::to_string(static_cast<int>(e->category())), e->location(),
                              encode(e->contact()), e->description(), std::to_string(static_cast<int>(e->severity())),
                              std::to_string(e->affected()), std::to_string(static_cast<int>(e->status())),
                              std::to_string(e->registered()), std::to_string(e->assignedAt()),
                              std::to_string(e->startedAt()), std::to_string(e->closedAt()), e->demo() ? "1" : "0"}));
    }
    if (!write("emergencies.txt", kEmergencyHeader, lines, error)) return false;
    lines.clear();
    for (const Resource* r : resources.list()) {
        std::string assigned;
        for (const std::string& id : r->assigned()) assigned += (assigned.empty() ? "" : ",") + id;
        lines.push_back(
            join({r->id(), std::to_string(static_cast<int>(r->type())), r->location(), std::to_string(r->capacity()),
                  std::to_string(r->maxShared()), r->outOfService() ? "1" : "0", assigned, r->demo() ? "1" : "0"}));
    }
    return write("resources.txt", kResourceHeader, lines, error);
}

}

namespace resq {

Stats AnalyticsManager::compute(const EmergencyManager& emergencies, const ResourceManager& resources) {
    Stats s;
    double responseTotal = 0;
    double resolutionTotal = 0;
    for (const Emergency* e : emergencies.list()) {
        ++s.total;
        ++s.status[idx(e->status())];
        ++s.severity[idx(e->severity())];
        ++s.category[idx(e->category())];
        if (e->active() && e->severity() == Severity::Critical) ++s.activeCritical;
        if (e->assignedAt() >= e->registered()) {
            responseTotal += static_cast<double>(e->assignedAt() - e->registered());
            ++s.responseCount;
        }
        if (e->status() == Status::Resolved && e->closedAt() >= e->registered()) {
            resolutionTotal += static_cast<double>(e->closedAt() - e->registered());
            ++s.resolvedCount;
        }
    }
    s.averageResponse = s.responseCount > 0 ? responseTotal / s.responseCount : 0;
    s.averageResolution = s.resolvedCount > 0 ? resolutionTotal / s.resolvedCount : 0;

    for (const Resource* r : resources.list()) {
        ++s.resources;
        if (r->outOfService())
            ++s.outOfService;
        else if (r->assigned().empty())
            ++s.available;
        else
            ++s.deployed;
    }
    return s;
}

}


#include <algorithm>

namespace resq {

namespace {

const int kWidth = 74;

size_t visible(const std::string& text) {
    size_t count = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\033') {
            i = text.find('m', i);
            if (i == std::string::npos) break;
        } else {
            ++count;
        }
    }
    return count;
}

}

Console::Console(std::istream& in, std::ostream& output, bool color, bool interactive)
    : out(output), in_(in), color_(color), interactive_(interactive) {}

std::string Console::paint(const std::string& text, Color color) const {
    static const char* codes[] = {"",         "\033[1;91m", "\033[92m", "\033[93m",
                                  "\033[96m", "\033[95m",   "\033[90m", "\033[1m"};
    if (!color_ || color == Color::None) return text;
    return codes[static_cast<int>(color)] + text + "\033[0m";
}

std::string Console::pad(const std::string& text, size_t width) {
    if (text.size() > width) return text.substr(0, width - 1) + "~";
    return text + std::string(width - text.size(), ' ');
}

void Console::clear() const { out << (color_ && interactive_ ? "\033[2J\033[H" : "\n") << std::flush; }

void Console::logo() const {
    static const char* letters[4][5] = {
        {"#### ", "#   #", "#### ", "#  # ", "#   #"},
        {"#####", "#    ", "#### ", "#    ", "#####"},
        {" ####", "#    ", " ### ", "    #", "#### "},
        {" ### ", "#   #", "#   #", "#  # ", " ## #"},
    };
    const std::string border = "+" + std::string(kWidth, '=') + "+";
    out << paint(border, Color::Red) << '\n';
    for (int row = 0; row < 5; ++row) {
        std::string text;
        for (int letter = 0; letter < 4; ++letter) {
            for (const char* p = letters[letter][row]; *p != '\0'; ++p) text += *p == '#' ? "##" : "  ";
            text += "  ";
        }
        out << paint("|", Color::Red) << std::string(14, ' ') << paint(pad(text, kWidth - 14), Color::Red)
            << paint("|", Color::Red) << '\n';
    }
    out << paint("|", Color::Red)
        << paint(pad("          RESQ - Rescue and Emergency Support System", kWidth), Color::Cyan)
        << paint("|", Color::Red) << '\n'
        << paint("|", Color::Red)
        << paint(pad("          Academic prototype - not connected to real emergency services", kWidth), Color::Gray)
        << paint("|", Color::Red) << '\n'
        << paint(border, Color::Red) << '\n';
}

void Console::title(const std::string& text) const {
    clear();
    out << paint(" RESQ ", Color::Red) << "| " << paint(util::upper(text), Color::Cyan) << '\n'
        << paint(std::string(kWidth + 2, '-'), Color::Gray) << '\n';
}

void Console::box(const std::string& heading, const std::vector<std::string>& lines, Color color) const {
    out << paint("+-- ", color) << paint(heading, Color::Bold) << ' '
        << paint(std::string(kWidth - 4 - heading.size(), '-') + "+", color) << '\n';
    for (const std::string& line : lines) {
        const size_t width = visible(line);
        out << paint("| ", color) << line << std::string(width < kWidth - 2 ? kWidth - 2 - width : 0, ' ')
            << paint(" |", color) << '\n';
    }
    out << paint("+" + std::string(kWidth, '-') + "+", color) << '\n';
}

void Console::bar(const std::string& label, int value, int maximum, Color color) const {
    const int width = maximum > 0 ? std::max(value > 0 ? 1 : 0, value * 40 / maximum) : 0;
    out << "  " << pad(label, 18) << "| " << paint(std::string(static_cast<size_t>(width), '#'), color)
        << std::string(static_cast<size_t>(41 - width), ' ') << value << '\n';
}

void Console::menu(const std::vector<std::string>& items) const {
    for (size_t i = 0; i < items.size(); i += 2)
        out << "  " << pad(items[i], 36) << (i + 1 < items.size() ? items[i + 1] : "") << '\n';
}

void Console::ok(const std::string& message) const {
    out << "  " << paint("[ OK ]", Color::Green) << ' ' << message << '\n';
}
void Console::warn(const std::string& message) const {
    out << "  " << paint("[WARN]", Color::Yellow) << ' ' << message << '\n';
}
void Console::error(const std::string& message) const {
    out << "  " << paint("[FAIL]", Color::Red) << ' ' << message << '\n';
}

std::string Console::line(const std::string& prompt) const {
    out << "  " << paint(prompt, Color::Bold) << paint(" > ", Color::Cyan) << std::flush;
    std::string value;
    if (!std::getline(in_, value)) {
        out << '\n';
        throw InputClosed();
    }
    if (!interactive_) out << value << '\n';
    return util::trim(value);
}

std::string Console::choice(const std::string& prompt) const { return util::upper(line(prompt)); }

std::string Console::text(const std::string& prompt, size_t maxLength, bool required) const {
    while (true) {
        const std::string value = line(prompt);
        if (value == ":q") throw InputCancelled();
        if (value.empty() && !required) return value;
        const std::string problem = util::checkText(value, "This field", maxLength);
        if (problem.empty()) return value;
        error(problem + " (type :q to cancel)");
    }
}

long long Console::number(const std::string& prompt, long long minimum, long long maximum) const {
    while (true) {
        const std::string value = line(prompt);
        long long result = 0;
        if (value == ":q") throw InputCancelled();
        if (util::toInt(value, result) && result >= minimum && result <= maximum) return result;
        error("Enter a whole number from " + std::to_string(minimum) + " to " + std::to_string(maximum) + ".");
    }
}

long long Console::numberOr(const std::string& prompt, long long minimum, long long maximum, long long fallback) const {
    while (true) {
        const std::string value = line(prompt + " [" + std::to_string(fallback) + "]");
        long long result = 0;
        if (value.empty()) return fallback;
        if (util::toInt(value, result) && result >= minimum && result <= maximum) return result;
        error("Enter a whole number from " + std::to_string(minimum) + " to " + std::to_string(maximum) + ".");
    }
}

bool Console::confirm(const std::string& prompt, bool yesByDefault) const {
    while (true) {
        const std::string value = util::upper(line(prompt + (yesByDefault ? " (Y/n)" : " (y/N)")));
        if (value.empty()) return yesByDefault;
        if (value == "Y" || value == "YES") return true;
        if (value == "N" || value == "NO") return false;
        error("Please answer y or n.");
    }
}

void Console::pause() const {
    if (interactive_) line("Press Enter to continue");
}

}


#include <algorithm>

namespace resq {

namespace {

Color severityColor(Severity s) {
    return s == Severity::Critical ? Color::Red : (s == Severity::Low ? Color::None : Color::Yellow);
}

Color statusColor(Status s) {
    static const Color colors[] = {Color::Yellow, Color::Cyan, Color::Magenta, Color::Green, Color::Gray};
    return colors[idx(s)];
}

std::string resourceNames(const std::vector<const Resource*>& list) {
    std::string text;
    for (const Resource* r : list) text += (text.empty() ? "" : ", ") + r->id() + " (" + name(r->type()) + ")";
    return text.empty() ? "none" : text;
}

}

RESQApplication::RESQApplication(const std::string& folder, bool color)
    : console_(std::cin, std::cout, color, resq_is_interactive() == 1), files_(folder) {}

template <typename E> E RESQApplication::pick(const std::string& label, int count) {
    std::vector<std::string> items;
    for (int i = 1; i <= count; ++i) items.push_back("[" + std::to_string(i) + "] " + name(static_cast<E>(i)));
    console_.out << "  " << label << ":\n";
    console_.menu(items);
    return static_cast<E>(console_.number(label + " (1-" + std::to_string(count) + ")", 1, count));
}

bool RESQApplication::save() {
    std::string error;
    if (files_.save(emergencies_, resources_, error)) return true;
    console_.error("Could not save: " + error);
    return false;
}

int RESQApplication::run() {
    bool saved = false;
    try {
        console_.clear();
        console_.logo();
        const std::vector<std::string> warnings = files_.load(emergencies_, resources_, now());
        if (files_.foundData()) {
            console_.ok("Loaded " + std::to_string(emergencies_.size()) + " emergencies and " +
                        std::to_string(resources_.size()) + " resources from \"" + files_.folder() + "\".");
        } else {
            console_.warn("No saved data found - starting empty. Use menu 8 to load demo data.");
        }
        for (const std::string& warning : warnings) console_.warn(warning);
        console_.pause();
        while (true) {
            dashboard();
            const std::string c = console_.choice("Select an option");
            if (c == "0") break;
            try {
                if (c == "1")
                    registerEmergency();
                else if (c == "2")
                    viewEmergencies();
                else if (c == "3")
                    searchEmergencies();
                else if (c == "4")
                    updateEmergency();
                else if (c == "5")
                    dispatchMenu();
                else if (c == "6")
                    resourceMenu();
                else if (c == "7")
                    reports();
                else if (c == "8")
                    demoMenu();
                else {
                    console_.error("\"" + c + "\" is not a menu option.");
                    console_.pause();
                }
            } catch (const InputCancelled&) {
                console_.warn("Cancelled.");
                console_.pause();
            }
        }
    } catch (const InputClosed&) {
        console_.warn("Input closed - saving and exiting.");
    }
    saved = save();
    if (saved) console_.ok("All records saved to \"" + files_.folder() + "\".");
    console_.box("RESQ CLOSED",
                 {"Thank you for using RESQ. Stay safe.", "In a real emergency, contact your local emergency number."},
                 Color::Red);
    return saved ? 0 : 1;
}

void RESQApplication::dashboard() {
    console_.clear();
    console_.logo();
    const Stats s = AnalyticsManager::compute(emergencies_, resources_);
    const auto row = [&](const std::string& left, int a, Color ca, const std::string& right, int b, Color cb) {
        return console_.paint(Console::pad(left + std::to_string(a), 36), ca) +
               console_.paint(right + std::to_string(b), cb);
    };
    console_.box(
        "DASHBOARD",
        {
            row("Total cases        : ", s.total, Color::Bold, "Total resources    : ", s.resources, Color::Bold),
            row("Pending            : ", s.count(Status::Pending), Color::Yellow, "Available          : ", s.available,
                Color::Green),
            row("Assigned           : ", s.count(Status::Assigned), Color::Cyan, "Deployed           : ", s.deployed,
                Color::Yellow),
            row("In progress        : ", s.count(Status::InProgress), Color::Magenta,
                "Out of service     : ", s.outOfService, Color::Red),
            row("Resolved           : ", s.count(Status::Resolved), Color::Green,
                "Cancelled cases    : ", s.count(Status::Cancelled), Color::Gray),
            row("Critical (open)    : ", s.activeCritical, Color::Red, "Open cases         : ", s.open(), Color::Bold),
        });
    const std::vector<const Emergency*> queue = emergencies_.queue(now());
    if (!queue.empty()) {
        const Emergency* e = queue.front();
        console_.out << "  Next in queue: "
                     << console_.paint(e->id() + " " + name(e->severity()), severityColor(e->severity())) << " | "
                     << name(e->category()) << " | " << e->location() << " | score " << e->score() << "\n\n";
    } else {
        console_.out << "  " << console_.paint("Queue clear: no pending emergencies.", Color::Green) << "\n\n";
    }
    console_.menu({"[1] Register emergency", "[5] Dispatch & operations", "[2] View & sort emergencies",
                   "[6] Resource management", "[3] Search emergencies", "[7] Reports & analytics",
                   "[4] Update emergency", "[8] Demo data", "", "[0] Save & exit"});
}

void RESQApplication::showCase(const Emergency& e) {
    const Priority p = e.priority(now());
    std::vector<std::string> lines{
        "Category        : " + name(e.category()),
        "Severity        : " + console_.paint(name(e.severity()), severityColor(e.severity())) +
            "     Priority score: " + std::to_string(e.score()),
        "Status          : " + console_.paint(name(e.status()), statusColor(e.status())),
        "Location        : " + e.location(),
        "Reported by     : " + e.reporter() + "  (" + e.contact() + ")",
        "People affected : " + std::to_string(e.affected()),
        "Registered      : " + util::timeText(e.registered()) +
            (e.status() == Status::Pending ? "  (waiting " + util::duration(now() - e.registered()) + ")" : ""),
    };
    if (e.assignedAt() > 0) lines.push_back("Assigned        : " + util::timeText(e.assignedAt()));
    if (e.startedAt() > 0) lines.push_back("Work started    : " + util::timeText(e.startedAt()));
    if (e.closedAt() > 0) lines.push_back("Closed          : " + util::timeText(e.closedAt()));
    for (size_t i = 0; i < e.description().size(); i += 54)
        lines.push_back((i == 0 ? "Description     : " : "                  ") + e.description().substr(i, 54));
    lines.push_back("Resources       : " + resourceNames(resources_.assignedTo(e.id())));
    console_.box("EMERGENCY " + e.id() + (e.demo() ? " [FICTIONAL DEMO]" : ""), lines,
                 e.severity() == Severity::Critical && e.active() ? Color::Red : Color::Cyan);
    console_.out << "  Score " << p.total() << " = severity " << p.severity << " + people " << p.people << " + waiting "
                 << p.waiting << " (" << p.minutes << " min) + category " << p.category << '\n'
                 << "  " << console_.paint("The score only orders the queue; it is not a risk prediction.", Color::Gray)
                 << '\n';
}

void RESQApplication::emergencyTable(const std::vector<const Emergency*>& list) {
    console_.out << "  "
                 << console_.paint(Console::pad("ID", 9) + Console::pad("Category", 18) + Console::pad("Location", 18) +
                                       Console::pad("Severity", 9) + Console::pad("Score", 6) + "Status",
                                   Color::Cyan)
                 << '\n';
    if (list.empty()) console_.out << "  (no records)\n";
    for (const Emergency* e : list) {
        console_.out << "  " << Console::pad(e->id() + (e->demo() ? "*" : ""), 9)
                     << Console::pad(name(e->category()), 18) << Console::pad(e->location(), 18)
                     << console_.paint(Console::pad(name(e->severity()), 9), severityColor(e->severity()))
                     << Console::pad(std::to_string(e->score()), 6)
                     << console_.paint(name(e->status()), statusColor(e->status())) << '\n';
    }
}

void RESQApplication::resourceTable(const std::vector<const Resource*>& list) {
    console_.out << "  "
                 << console_.paint(Console::pad("#", 4) + Console::pad("ID", 8) + Console::pad("Type", 16) +
                                       Console::pad("Location", 18) + Console::pad("Cap", 5) + "Status",
                                   Color::Cyan)
                 << '\n';
    if (list.empty()) console_.out << "  (no records)\n";
    for (size_t i = 0; i < list.size(); ++i) {
        const Resource* r = list[i];
        std::string status = r->statusText();
        for (const std::string& id : r->assigned()) status += " " + id;
        console_.out << "  " << Console::pad(std::to_string(i + 1), 4)
                     << Console::pad(r->id() + (r->demo() ? "*" : ""), 8) << Console::pad(name(r->type()), 16)
                     << Console::pad(r->location(), 18) << Console::pad(std::to_string(r->capacity()), 5)
                     << console_.paint(status, r->outOfService()
                                                   ? Color::Red
                                                   : (r->assigned().empty() ? Color::Green : Color::Yellow))
                     << '\n';
    }
}

Emergency* RESQApplication::askEmergency(const std::string& prompt) {
    Emergency* e = emergencies_.find(console_.text(prompt, 8));
    if (e == nullptr) console_.error("No emergency with that ID.");
    return e;
}

Resource* RESQApplication::askResource(const std::string& prompt) {
    Resource* r = resources_.find(console_.text(prompt, 8));
    if (r == nullptr) console_.error("No resource with that ID.");
    return r;
}

void RESQApplication::registerEmergency() {
    console_.title("Register emergency");
    console_.out << "  Type :q at any prompt to cancel.\n";
    std::string id;
    while (true) {
        const std::string next = emergencies_.nextId();
        id = util::upper(console_.text("Emergency ID [Enter = " + next + "]", 8, false));
        id = id.empty() ? next : id;
        if (!util::validId(id))
            console_.error("ID must be 3-8 letters, digits or '-', starting with a letter.");
        else if (emergencies_.find(id) != nullptr)
            console_.error("An emergency with ID " + id + " already exists. IDs must be unique.");
        else
            break;
    }
    const std::string reporter = console_.text("Reporter name", 40);
    const Category category = pick<Category>("Category", kCategories);
    const std::string location = console_.text("Location", 40);
    std::string contact = console_.text("Contact phone or e-mail", 32);
    while (!Emergency::checkContact(contact).empty()) {
        console_.error(Emergency::checkContact(contact));
        contact = console_.text("Contact phone or e-mail", 32);
    }
    const std::string description = console_.text("Description", 200);
    const Severity severity = pick<Severity>("Severity", kSeverities);
    const int affected = static_cast<int>(console_.number("People affected", 0, 1000000));
    std::time_t registered = now();
    while (true) {
        const std::string when = console_.text("Registration time [Enter = now, or YYYY-MM-DD HH:MM]", 16, false);
        if (when.empty()) break;
        if (util::parseTime(when, registered) && registered <= now()) break;
        console_.error("Use YYYY-MM-DD HH:MM, not in the future.");
        registered = now();
    }
    if (!emergencies_
             .add(Emergency(id, reporter, category, location, contact, description, severity, affected, registered))
             .empty()) {
        return;
    }
    Emergency* e = emergencies_.find(id);
    const std::vector<const Emergency*> queue = emergencies_.queue(now());
    const auto position = std::find(queue.begin(), queue.end(), e) - queue.begin() + 1;
    save();
    console_.ok("Emergency " + id + " registered. Priority score " + std::to_string(e->score()) + ", position " +
                std::to_string(position) + " of " + std::to_string(queue.size()) + " in the queue.");
    if (severity == Severity::Critical) console_.warn("CRITICAL case: dispatch resources as soon as possible.");
    if (console_.confirm("Assign a resource now?")) assignFlow(*e);
    console_.pause();
}

void RESQApplication::viewEmergencies() {
    SortBy sortBy = SortBy::Priority;
    while (true) {
        console_.title("All emergencies");
        emergencies_.refresh(now());
        emergencyTable(emergencies_.sorted(sortBy));
        console_.out << '\n';
        console_.menu({"[1] Sort by severity", "[2] Sort by priority score", "[3] Sort by registration time",
                       "[4] Sort by ID", "[D] Show details of a case", "[0] Back"});
        const std::string c = console_.choice("Choose");
        if (c == "0") return;
        if (c == "1")
            sortBy = SortBy::Severity;
        else if (c == "2")
            sortBy = SortBy::Priority;
        else if (c == "3")
            sortBy = SortBy::Time;
        else if (c == "4")
            sortBy = SortBy::Id;
        else if (c == "D") {
            if (const Emergency* e = askEmergency("Emergency ID")) showCase(*e);
            console_.pause();
        }
    }
}

void RESQApplication::searchEmergencies() {
    console_.title("Search emergencies");
    console_.menu({"[1] By ID", "[2] By reporter name", "[3] By location", "[4] By category"});
    const int field = static_cast<int>(console_.number("Search by", 1, 4));
    const std::vector<const Emergency*> results = field == 4
                                                      ? emergencies_.byCategory(pick<Category>("Category", kCategories))
                                                      : emergencies_.search(field, console_.text("Search text", 40));
    if (results.empty()) {
        console_.warn("No emergencies found.");
    } else if (results.size() <= 5) {
        console_.ok(std::to_string(results.size()) + " match(es) found.");
        for (const Emergency* e : results) showCase(*e);
    } else {
        console_.ok(std::to_string(results.size()) + " matches found.");
        emergencyTable(results);
    }
    console_.pause();
}

void RESQApplication::updateEmergency() {
    console_.title("Update emergency");
    Emergency* e = askEmergency("Emergency ID");
    while (e != nullptr) {
        e->refresh(now());
        showCase(*e);
        console_.menu({"[1] Change severity", "[2] Update location", "[3] Edit description",
                       "[4] Update people affected", "[5] Change status", "[0] Done"});
        const std::string c = console_.choice("Choose");
        if (c == "0") return;
        try {
            if (c == "1")
                e->setSeverity(pick<Severity>("New severity", kSeverities));
            else if (c == "2")
                e->setLocation(console_.text("New location", 40));
            else if (c == "3")
                e->setDescription(console_.text("New description", 200));
            else if (c == "4")
                e->setAffected(static_cast<int>(console_.number("People affected", 0, 1000000)));
            else if (c == "5")
                changeStatus(*e);
            else
                console_.error("Invalid option.");
            if (c == "1" || c == "2" || c == "3" || c == "4") {
                e->refresh(now());
                save();
                console_.ok("Updated. New priority score: " + std::to_string(e->score()));
            }
        } catch (const std::exception& ex) {
            console_.error(ex.what());
        }
        console_.pause();
    }
    console_.pause();
}

void RESQApplication::changeStatus(Emergency& e) {
    std::vector<Status> allowed;
    for (int i = 1; i <= kStatuses; ++i) {
        if (Emergency::canMove(e.status(), static_cast<Status>(i))) {
            allowed.push_back(static_cast<Status>(i));
            console_.out << "  [" << allowed.size() << "] " << name(static_cast<Status>(i)) << '\n';
        }
    }
    if (allowed.empty()) {
        console_.warn("This case is closed; its status cannot change.");
        return;
    }
    const Status to =
        allowed[static_cast<size_t>(console_.number("New status", 1, static_cast<long long>(allowed.size())) - 1)];
    if (to == Status::Assigned) {
        assignFlow(e);
        return;
    }
    if (to != Status::InProgress &&
        !console_.confirm("Change to " + name(to) + "? Assigned resources will be released.")) {
        return;
    }
    const std::string error = resources_.changeStatus(e, to, now());
    if (error.empty()) {
        save();
        console_.ok("Case " + e.id() + " is now " + name(to) + ".");
    } else {
        console_.error(error);
    }
}

void RESQApplication::dispatchMenu() {
    while (true) {
        console_.title("Dispatch & operations");
        console_.menu({"[1] Process next highest-priority case", "[2] Manage active operations", "[0] Back"});
        const std::string c = console_.choice("Choose");
        if (c == "0") return;
        if (c == "1")
            processNext();
        else if (c == "2")
            activeOperations();
    }
}

void RESQApplication::processNext() {
    while (true) {
        const std::vector<const Emergency*> queue = emergencies_.queue(now());
        console_.title("Process next emergency");
        if (queue.empty()) {
            console_.ok("The pending queue is empty.");
            console_.pause();
            return;
        }
        Emergency* e = emergencies_.find(queue.front()->id());
        console_.out << "  Highest-priority pending case (" << queue.size() << " waiting):\n";
        showCase(*e);
        console_.menu({"[1] Assign resource(s)", "[2] Cancel case (false alarm)", "[0] Back"});
        const std::string c = console_.choice("Choose");
        if (c == "0") return;
        if (c == "1") {
            assignFlow(*e);
            if (e->status() == Status::Assigned && console_.confirm("Have crews started work? Mark In Progress?")) {
                resources_.changeStatus(*e, Status::InProgress, now());
                save();
                console_.ok("Case " + e->id() + " is now In Progress.");
            }
            console_.pause();
        } else if (c == "2" && console_.confirm("Cancel " + e->id() + "?")) {
            resources_.changeStatus(*e, Status::Cancelled, now());
            save();
        }
    }
}

void RESQApplication::assignFlow(Emergency& e) {
    while (e.active()) {
        const std::vector<const Resource*> suitable = resources_.suitableFor(e);
        console_.out << "\n  Suitable available resources for " << e.id() << " (" << name(e.category()) << ", "
                     << e.affected() << " people):\n";
        resourceTable(suitable);
        const std::string input = console_.text("Resource ID or # (Enter = done)", 8, false);
        if (input.empty()) return;
        long long number = 0;
        const std::string id =
            util::toInt(input, number) && number >= 1 && number <= static_cast<long long>(suitable.size())
                ? suitable[static_cast<size_t>(number - 1)]->id()
                : input;
        const std::string error = resources_.assign(e, id, now());
        if (!error.empty()) {
            console_.error(error);
            continue;
        }
        save();
        console_.ok("Resource " + util::upper(id) + " assigned to " + e.id() + ". Status: " + name(e.status()) + ".");
        if (!console_.confirm("Assign another resource?")) return;
    }
}

void RESQApplication::activeOperations() {
    while (true) {
        console_.title("Active operations");
        std::vector<const Emergency*> active;
        for (const Emergency* e : emergencies_.sorted(SortBy::Priority))
            if (e->status() == Status::Assigned || e->status() == Status::InProgress) active.push_back(e);
        emergencyTable(active);
        const std::string id = console_.text("Case ID to manage (Enter = back)", 8, false);
        if (id.empty()) return;
        Emergency* e = emergencies_.find(id);
        if (e == nullptr || !e->active()) {
            console_.error("That is not an active case.");
            console_.pause();
        } else {
            manageOperation(*e);
        }
    }
}

void RESQApplication::manageOperation(Emergency& e) {
    while (e.active()) {
        console_.title("Operation " + e.id());
        showCase(e);
        console_.menu({"[1] Assign another resource", "[2] Release a resource", "[3] Mark In Progress",
                       "[4] Mark Resolved", "[5] Cancel case", "[0] Back"});
        const std::string c = console_.choice("Choose");
        std::string error;
        if (c == "0") return;
        if (c == "1") {
            assignFlow(e);
            continue;
        }
        if (c == "2")
            error = resources_.release(e, console_.text("Resource ID", 8), now());
        else if (c == "3")
            error = resources_.changeStatus(e, Status::InProgress, now());
        else if (c == "4")
            error = console_.confirm("Is the operation complete?") ? resources_.changeStatus(e, Status::Resolved, now())
                                                                   : "Not resolved.";
        else if (c == "5")
            error = console_.confirm("Cancel this case?") ? resources_.changeStatus(e, Status::Cancelled, now())
                                                          : "Not cancelled.";
        else
            error = "Invalid option.";
        if (error.empty()) {
            save();
            console_.ok("Done. Case " + e.id() + " is " + name(e.status()) + ".");
        } else {
            console_.error(error);
        }
        console_.pause();
    }
}

void RESQApplication::resourceMenu() {
    while (true) {
        console_.title("Resource management");
        console_.menu({"[1] Add resource", "[2] View all resources", "[3] Search resources",
                       "[4] Change availability / location", "[5] Assign resource to emergency", "[6] Release resource",
                       "[0] Back"});
        const std::string c = console_.choice("Choose");
        if (c == "0") return;
        if (c == "1")
            addResource();
        else if (c == "2") {
            resourceTable(resources_.list());
            console_.pause();
        } else if (c == "3")
            searchResources();
        else if (c == "4")
            updateResource();
        else if (c == "5") {
            Emergency* e = askEmergency("Emergency ID");
            if (e != nullptr && e->active())
                assignFlow(*e);
            else if (e != nullptr)
                console_.error("Only open cases can receive resources.");
            console_.pause();
        } else if (c == "6")
            releaseResource();
    }
}

void RESQApplication::addResource() {
    static const int defaultCapacity[] = {2, 10, 6, 8, 20, 25};
    console_.title("Add resource");
    std::string id;
    while (true) {
        id = util::upper(console_.text("Resource ID [Enter = " + resources_.nextId() + "]", 8, false));
        id = id.empty() ? resources_.nextId() : id;
        if (!util::validId(id))
            console_.error("ID must be 3-8 letters, digits or '-', starting with a letter.");
        else if (resources_.find(id) != nullptr)
            console_.error("A resource with ID " + id + " already exists.");
        else
            break;
    }
    const ResourceType type = pick<ResourceType>("Type", kResourceTypes);
    const std::string location = console_.text("Current location", 40);
    const int capacity = static_cast<int>(console_.numberOr("Capacity (people)", 0, 10000, defaultCapacity[idx(type)]));
    const int shared = static_cast<int>(console_.numberOr("Max simultaneous emergencies (1 = exclusive)", 1, 20, 1));
    resources_.add(Resource(id, type, location, capacity, shared));
    save();
    console_.ok("Resource " + id + " added and available.");
    console_.pause();
}

void RESQApplication::searchResources() {
    console_.menu({"[1] By ID", "[2] By type", "[3] By location", "[4] By status (available/deployed/out)"});
    const int field = static_cast<int>(console_.number("Search by", 1, 4));
    const std::vector<const Resource*> results = resources_.search(field, console_.text("Search text", 40));
    if (results.empty())
        console_.warn("No resources found.");
    else
        resourceTable(results);
    console_.pause();
}

void RESQApplication::updateResource() {
    Resource* r = askResource("Resource ID");
    if (r != nullptr) {
        console_.menu({"[1] Mark out of service", "[2] Return to service", "[3] Change location"});
        const int c = static_cast<int>(console_.number("Choose", 1, 3));
        std::string error;
        if (c == 3) {
            try {
                r->setLocation(console_.text("New location", 40));
            } catch (const std::exception& ex) {
                error = ex.what();
            }
        } else {
            error = r->setOutOfService(c == 1);
        }
        if (error.empty()) {
            save();
            console_.ok(r->id() + " is now: " + r->statusText() + " at " + r->location() + ".");
        } else {
            console_.error(error);
        }
    }
    console_.pause();
}

void RESQApplication::releaseResource() {
    Resource* r = askResource("Resource ID");
    if (r != nullptr && r->assigned().empty()) {
        console_.warn(r->id() + " is not assigned to any emergency.");
    } else if (r != nullptr) {
        const std::string eid = r->assigned().size() == 1 ? r->assigned().front() : console_.text("Emergency ID", 8);
        Emergency* e = emergencies_.find(eid);
        const std::string error = e == nullptr ? "No emergency with that ID." : resources_.release(*e, r->id(), now());
        if (error.empty()) {
            save();
            console_.ok(r->id() + " released and available again.");
        } else {
            console_.error(error);
        }
    }
    console_.pause();
}

void RESQApplication::reports() {
    console_.title("Reports & analytics");
    const Stats s = AnalyticsManager::compute(emergencies_, resources_);
    console_.out << "  Calculated from the records stored in \"" << files_.folder() << "\".\n"
                 << "  Total emergencies: " << s.total << "   Open: " << s.open()
                 << "   Critical still open: " << s.activeCritical << "\n\n  BY STATUS\n";
    const int maxStatus = *std::max_element(s.status, s.status + kStatuses);
    for (int i = 1; i <= kStatuses; ++i)
        console_.bar(name(static_cast<Status>(i)), s.status[i - 1], maxStatus, statusColor(static_cast<Status>(i)));
    console_.out << "\n  BY SEVERITY\n";
    const int maxSeverity = *std::max_element(s.severity, s.severity + kSeverities);
    for (int i = 1; i <= kSeverities; ++i) {
        console_.bar(name(static_cast<Severity>(i)), s.severity[i - 1], maxSeverity,
                     severityColor(static_cast<Severity>(i)));
    }
    console_.out << "\n  BY CATEGORY\n";
    const int maxCategory = *std::max_element(s.category, s.category + kCategories);
    for (int i = 1; i <= kCategories; ++i)
        console_.bar(name(static_cast<Category>(i)), s.category[i - 1], maxCategory, Color::Cyan);
    console_.out << "\n  RESOLVED VS PENDING\n";
    const int resolved = s.count(Status::Resolved);
    const int pending = s.count(Status::Pending);
    console_.bar("Resolved", resolved, std::max(resolved, pending), Color::Green);
    console_.bar("Pending", pending, std::max(resolved, pending), Color::Yellow);
    const int inService = s.resources - s.outOfService;
    console_.out << "\n  RESOURCES: " << s.available << " available, " << s.deployed << " deployed, " << s.outOfService
                 << " out of service. Utilization: " << (inService > 0 ? s.deployed * 100 / inService : 0) << "%\n"
                 << "  Average time to first assignment: "
                 << (s.responseCount > 0 ? util::duration(static_cast<long long>(s.averageResponse)) : "no data")
                 << '\n'
                 << "  Average time to resolution      : "
                 << (s.resolvedCount > 0 ? util::duration(static_cast<long long>(s.averageResolution)) : "no data")
                 << '\n';
    console_.pause();
}

void RESQApplication::demoMenu() {
    console_.title("Demo data (fictional)");
    console_.menu({"[1] Load demo data", "[2] Reset demo data", "[3] Remove demo data", "[0] Back"});
    const std::string c = console_.choice("Choose");
    const bool hasData = emergencies_.size() + resources_.size() > 0;
    if (c == "1" && (!hasData || console_.confirm("Your records are kept; demo records are added. Continue?"))) {
        loadDemo();
    } else if (c == "2" && console_.confirm("Remove demo records and load a fresh copy? Your records are kept.")) {
        resources_.removeDemo(emergencies_, now());
        loadDemo();
    } else if (c == "3" && console_.confirm("Remove all demo records? Your records are kept.")) {
        resources_.removeDemo(emergencies_, now());
        save();
        console_.ok("Demo records removed.");
    }
    console_.pause();
}

void RESQApplication::loadDemo() {
    struct Case {
        const char* id;
        const char* who;
        Category c;
        const char* where;
        Severity s;
        int people;
        int minutesAgo;
        const char* what;
    };
    static const Case cases[] = {
        {"E-D01", "Riya Sharma", Category::Fire, "Old Market Square", Severity::Critical, 12, 25,
         "Shop fire, smoke on upper floor"},
        {"E-D02", "Arjun Mehta", Category::Medical, "Riverside Ward 3", Severity::High, 1, 40,
         "Elderly resident with chest pain"},
        {"E-D03", "Kavya Rao", Category::RoadAccident, "Hill Road Km 12", Severity::High, 4, 15,
         "Two-car collision, one trapped"},
        {"E-D04", "Imran Khan", Category::Flood, "Lakeview Colony", Severity::Medium, 30, 90,
         "Water entering ground-floor homes"},
        {"E-D05", "Neha Joshi", Category::Other, "Central Park Gate", Severity::Low, 0, 10,
         "Fallen tree blocking footpath"},
        {"E-D06", "Vikram Singh", Category::NaturalDisaster, "Northridge Village", Severity::Critical, 60, 180,
         "Landslide damaged houses"},
        {"E-D07", "Ananya Gupta", Category::Medical, "Sector 9 Housing", Severity::Medium, 1, 300,
         "Child with high fever"},
    };
    struct Unit {
        const char* id;
        ResourceType t;
        const char* where;
        int capacity;
        bool out;
    };
    static const Unit units[] = {
        {"R-D01", ResourceType::Ambulance, "Riverside", 2, false},
        {"R-D02", ResourceType::Ambulance, "Hill Road Depot", 2, true},
        {"R-D03", ResourceType::MedicalTeam, "District Hospital", 10, false},
        {"R-D04", ResourceType::FireEngine, "Old Market Station", 6, false},
        {"R-D05", ResourceType::RescueTeam, "Northridge", 8, false},
        {"R-D06", ResourceType::RescueTeam, "Central Depot", 8, false},
        {"R-D07", ResourceType::ReliefVehicle, "Central Depot", 20, false},
        {"R-D08", ResourceType::SupplyKit, "Central Store", 25, false},
    };
    const std::time_t t = now();
    int added = 0;
    for (const Unit& u : units) {
        Resource r(u.id, u.t, u.where, u.capacity, u.t == ResourceType::SupplyKit ? 3 : 1, true);
        r.setOutOfService(u.out);
        added += resources_.add(r).empty() ? 1 : 0;
    }
    for (const Case& c : cases) {
        added +=
            emergencies_
                    .add(Emergency(c.id, std::string(c.who) + " (demo)", c.c, c.where, "000-555-0100",
                                   std::string("[FICTIONAL] ") + c.what, c.s, c.people, t - c.minutesAgo * 60, true))
                    .empty()
                ? 1
                : 0;
    }
    Emergency* landslide = emergencies_.find("E-D06");
    if (landslide != nullptr && landslide->status() == Status::Pending &&
        resources_.assign(*landslide, "R-D05", t - 170 * 60).empty()) {
        resources_.assign(*landslide, "R-D07", t - 165 * 60);
        resources_.changeStatus(*landslide, Status::InProgress, t - 150 * 60);
    }
    Emergency* fever = emergencies_.find("E-D07");
    if (fever != nullptr && fever->status() == Status::Pending &&
        resources_.assign(*fever, "R-D01", t - 290 * 60).empty()) {
        resources_.changeStatus(*fever, Status::InProgress, t - 280 * 60);
        resources_.changeStatus(*fever, Status::Resolved, t - 240 * 60);
    }
    emergencies_.refresh(t);
    save();
    console_.ok("Added " + std::to_string(added) + " fictional demo records (marked * in tables).");
}

}


int main(int argc, char* argv[]) {
    std::string folder = "data";
    bool color = resq_enable_ansi() == 1;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-color") {
            color = false;
        } else if (arg == "--help") {
            std::cout << "Usage: resq [--no-color] [data-folder]\n";
            return 0;
        } else {
            folder = arg;
        }
    }
    resq::RESQApplication app(folder, color);
    return app.run();
}
