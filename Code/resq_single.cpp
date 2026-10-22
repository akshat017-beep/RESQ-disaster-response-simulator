#if !defined(_WIN32)
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#  endif
#  ifndef _DEFAULT_SOURCE
#    define _DEFAULT_SOURCE 1
#  endif
#  if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#    define _DARWIN_C_SOURCE 1
#  endif
#endif

#ifndef RESQ_C_H
#define RESQ_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int resq_term_init(void);

int resq_term_enable_utf8(void);

int resq_stdout_is_tty(void);
int resq_stdin_is_tty(void);

int resq_term_columns(void);

int resq_file_exists(const char *path);
int resq_make_dirs(const char *path);
int resq_copy_file(const char *src, const char *dst);
int resq_replace_file(const char *src, const char *dst);
int resq_remove_file(const char *path);
int resq_remove_dir(const char *path);

uint32_t resq_crc32(uint32_t crc, const void *data, size_t len);

size_t resq_obfuscate_hex(const char *in, char *out, size_t out_size);

int resq_deobfuscate_hex(const char *in, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <io.h>
#  ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif
#else
#  include <errno.h>
#  include <sys/ioctl.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

int resq_stdout_is_tty(void)
{
#ifdef _WIN32
    return _isatty(_fileno(stdout)) ? 1 : 0;
#else
    return isatty(STDOUT_FILENO) ? 1 : 0;
#endif
}

int resq_stdin_is_tty(void)
{
#ifdef _WIN32
    return _isatty(_fileno(stdin)) ? 1 : 0;
#else
    return isatty(STDIN_FILENO) ? 1 : 0;
#endif
}

int resq_term_init(void)
{
    const char *no_color = getenv("NO_COLOR");
    if (no_color != NULL && no_color[0] != '\0') {
        return 0;
    }
    if (!resq_stdout_is_tty()) {
        return 0;
    }
#ifdef _WIN32
    {
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (handle == INVALID_HANDLE_VALUE || !GetConsoleMode(handle, &mode)) {
            return 0;
        }
        if (!SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            return 0;
        }
        return 1;
    }
#else
    {
        const char *term = getenv("TERM");
        if (term != NULL && strcmp(term, "dumb") == 0) {
            return 0;
        }
        return 1;
    }
#endif
}

#ifndef _WIN32
static int resq_contains_utf8(const char *value)
{
    const char *p;
    if (value == NULL) {
        return 0;
    }
    for (p = value; *p != '\0'; ++p) {
        if ((p[0] == 'U' || p[0] == 'u') && (p[1] == 'T' || p[1] == 't') &&
            (p[2] == 'F' || p[2] == 'f')) {
            const char *q = p + 3;
            if (*q == '-') {
                ++q;
            }
            if (*q == '8') {
                return 1;
            }
        }
    }
    return 0;
}
#endif

int resq_term_enable_utf8(void)
{
#ifdef _WIN32
    int ok_out = SetConsoleOutputCP(CP_UTF8) ? 1 : 0;
    SetConsoleCP(CP_UTF8);
    return ok_out;
#else
    const char *vars[3];
    int i;
    vars[0] = getenv("LC_ALL");
    vars[1] = getenv("LC_CTYPE");
    vars[2] = getenv("LANG");
    for (i = 0; i < 3; ++i) {
        if (vars[i] != NULL && vars[i][0] != '\0') {
            return resq_contains_utf8(vars[i]);
        }
    }
    return 0;
#endif
}

int resq_term_columns(void)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        return (int)(info.srWindow.Right - info.srWindow.Left + 1);
    }
    return 0;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return (int)ws.ws_col;
    }
    return 0;
#endif
}

int resq_file_exists(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
#else
    {
        struct stat info;
        return stat(path, &info) == 0 ? 1 : 0;
    }
#endif
}

static int resq_is_dir(const char *path)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

static int resq_make_one_dir(const char *path)
{
    if (resq_is_dir(path)) {
        return 1;
    }
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

int resq_make_dirs(const char *path)
{
    char buffer[1024];
    size_t len;
    size_t i;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    len = strlen(path);
    if (len >= sizeof(buffer)) {
        return 0;
    }
    memcpy(buffer, path, len + 1);
    for (i = 1; i < len; ++i) {
        if (buffer[i] == '/' || buffer[i] == '\\') {
            if (buffer[i - 1] == ':' || buffer[i - 1] == '/' || buffer[i - 1] == '\\') {
                continue;
            }
            buffer[i] = '\0';
            if (!resq_make_one_dir(buffer)) {
                return 0;
            }
            buffer[i] = path[i];
        }
    }
    return resq_make_one_dir(buffer) && resq_is_dir(buffer);
}

int resq_copy_file(const char *src, const char *dst)
{
    char chunk[8192];
    size_t got;
    int ok = 1;
    FILE *in;
    FILE *out;

    in = fopen(src, "rb");
    if (in == NULL) {
        return 0;
    }
    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return 0;
    }
    while ((got = fread(chunk, 1, sizeof(chunk), in)) > 0) {
        if (fwrite(chunk, 1, got, out) != got) {
            ok = 0;
            break;
        }
    }
    if (ferror(in)) {
        ok = 0;
    }
    fclose(in);
    if (fclose(out) != 0) {
        ok = 0;
    }
    return ok;
}

int resq_replace_file(const char *src, const char *dst)
{
#ifdef _WIN32
    return MoveFileExA(src, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 1 : 0;
#else
    return rename(src, dst) == 0 ? 1 : 0;
#endif
}

int resq_remove_file(const char *path)
{
    return remove(path) == 0 ? 1 : 0;
}

int resq_remove_dir(const char *path)
{
#ifdef _WIN32
    return RemoveDirectoryA(path) ? 1 : 0;
#else
    return rmdir(path) == 0 ? 1 : 0;
#endif
}

uint32_t resq_crc32(uint32_t crc, const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t i;
    int bit;

    crc = ~crc;
    for (i = 0; i < len; ++i) {
        crc ^= (uint32_t)p[i];
        for (bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static unsigned int resq_next_key(uint32_t *state)
{
    *state = (*state) * 1103515245u + 12345u;
    return (unsigned int)((*state >> 16) & 0xFFu);
}

static uint32_t resq_seed(size_t length)
{
    return 0x52455351u ^ (uint32_t)((uint32_t)length * 2654435761u);
}

size_t resq_obfuscate_hex(const char *in, char *out, size_t out_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t len;
    size_t i;
    uint32_t state;

    if (in == NULL || out == NULL) {
        return 0;
    }
    len = strlen(in);
    if (out_size < len * 2 + 1) {
        return 0;
    }
    state = resq_seed(len);
    for (i = 0; i < len; ++i) {
        unsigned int b = ((unsigned int)(unsigned char)in[i]) ^ resq_next_key(&state);
        out[2 * i] = hex[(b >> 4) & 0xFu];
        out[2 * i + 1] = hex[b & 0xFu];
    }
    out[2 * len] = '\0';
    return len * 2;
}

static int resq_hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

int resq_deobfuscate_hex(const char *in, char *out, size_t out_size)
{
    size_t len;
    size_t count;
    size_t i;
    uint32_t state;

    if (in == NULL || out == NULL) {
        return 0;
    }
    len = strlen(in);
    if (len % 2 != 0) {
        return 0;
    }
    count = len / 2;
    if (out_size < count + 1) {
        return 0;
    }
    state = resq_seed(count);
    for (i = 0; i < count; ++i) {
        int hi = resq_hex_value(in[2 * i]);
        int lo = resq_hex_value(in[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return 0;
        }
        out[i] = (char)(((unsigned int)((hi << 4) | lo)) ^ resq_next_key(&state));
    }
    out[count] = '\0';
    return 1;
}
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
#ifndef RESQ_UTIL_H
#define RESQ_UTIL_H

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace resq {
namespace util {

std::string trim(const std::string& text);
std::string toUpper(std::string text);
std::string toLower(std::string text);
bool iequals(const std::string& a, const std::string& b);
bool icontains(const std::string& haystack, const std::string& needle);

size_t utf8Length(const std::string& text);
size_t utf8Offset(const std::string& text, size_t chars);

bool isPrintableText(const std::string& text);

bool isValidId(const std::string& id);

bool parseInteger(const std::string& text, long long& out);

std::string escapeField(const std::string& field);
std::vector<std::string> splitEscaped(const std::string& line);
std::vector<std::string> splitPlain(const std::string& text, char delimiter);

std::vector<std::string> wrapText(const std::string& text, size_t width);

std::string formatDateTime(std::time_t value);
std::string formatCompact(std::time_t value);
std::string formatClock(std::time_t value);
std::string formatDuration(long long seconds);
bool parseDateTime(const std::string& text, std::time_t& out);

std::string obfuscate(const std::string& plain);
bool reveal(const std::string& stored, std::string& plain);
std::string maskContact(const std::string& contact);

std::string hex32(std::uint32_t value);

}
}

#endif
#ifndef RESQ_PRIORITY_H
#define RESQ_PRIORITY_H


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
#ifndef RESQ_EMERGENCY_H
#define RESQ_EMERGENCY_H


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
#ifndef RESQ_RESOURCE_H
#define RESQ_RESOURCE_H


#include <ctime>
#include <string>
#include <vector>

namespace resq {

class Resource {
public:
    static constexpr size_t kMaxLocation = 40;
    static constexpr int kMaxCapacity = 10000;
    static constexpr int kMaxConcurrent = 20;

    Resource(std::string id, ResourceType type, std::string location, int capacity, int maxConcurrent = 1,
             bool demo = false);

    const std::string& id() const { return id_; }
    ResourceType type() const { return type_; }
    const std::string& location() const { return location_; }
    int capacity() const { return capacity_; }
    int maxConcurrent() const { return maxConcurrent_; }
    bool isDemo() const { return demo_; }
    bool isOutOfService() const { return outOfService_; }
    const std::vector<std::string>& assignedEmergencies() const { return assigned_; }

    ResourceStatus status() const;
    int activeAssignments() const { return static_cast<int>(assigned_.size()); }
    bool hasFreeSlot() const { return activeAssignments() < maxConcurrent_; }
    bool canAccept() const { return !outOfService_ && hasFreeSlot(); }
    bool isAssignedTo(const std::string& emergencyId) const;
    std::string statusLabel() const;

    void setLocation(const std::string& location);
    void setOutOfService(bool outOfService);
    void assignTo(const std::string& emergencyId);
    bool releaseFrom(const std::string& emergencyId);

    static std::string validateId(const std::string& id);
    static std::string validateLocation(const std::string& location);
    static std::string validateCapacity(long long capacity);
    static std::string validateMaxConcurrent(long long maxConcurrent);

private:
    std::string id_;
    ResourceType type_;
    std::string location_;
    int capacity_;
    int maxConcurrent_;
    bool demo_;
    bool outOfService_ = false;
    std::vector<std::string> assigned_;
};

struct AssignmentRecord {
    long id = 0;
    std::string emergencyId;
    std::string resourceId;
    std::time_t assignedAt = 0;
    std::time_t releasedAt = 0;
    std::string note;

    bool isActive() const { return releasedAt == 0; }
};

}

#endif
#ifndef RESQ_EMERGENCY_MANAGER_H
#define RESQ_EMERGENCY_MANAGER_H


#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace resq {

enum class EmergencySortKey { Severity, Priority, RegistrationTime, Id };
enum class EmergencySearchField { Id, Reporter, Location };

class EmergencyManager {
public:
    std::string add(Emergency emergency);
    bool remove(const std::string& id);
    bool exists(const std::string& id) const;
    Emergency* find(const std::string& id);
    const Emergency* find(const std::string& id) const;

    std::vector<const Emergency*> all() const;
    std::vector<const Emergency*> sorted(EmergencySortKey key, bool reverse = false) const;
    std::vector<const Emergency*> search(EmergencySearchField field, const std::string& query) const;
    std::vector<const Emergency*> searchByCategory(Category category) const;
    std::vector<const Emergency*> withStatus(Status status) const;
    std::vector<std::string> demoIds() const;

    void recalculateAll(std::time_t now);
    std::vector<const Emergency*> pendingByPriority(std::time_t now);
    const Emergency* highestPriorityPending(std::time_t now);
    int queuePosition(const std::string& id, std::time_t now);

    std::string updateSeverity(const std::string& id, Severity severity, std::time_t now);
    std::string updateLocation(const std::string& id, const std::string& location, std::time_t now);
    std::string updateDescription(const std::string& id, const std::string& description, std::time_t now);
    std::string updateAffected(const std::string& id, int affected, std::time_t now);

    std::string generateId() const;

    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }
    void clear() { items_.clear(); }

    static bool comesFirst(const Emergency& a, const Emergency& b);

private:
    std::string modify(const std::string& id, std::time_t now, const std::function<void(Emergency&)>& change);

    std::map<std::string, Emergency> items_;
};

}

#endif
#ifndef RESQ_RESOURCE_MANAGER_H
#define RESQ_RESOURCE_MANAGER_H


#include <ctime>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace resq {

enum class ResourceSearchField { Id, Type, Location, Status };

struct SuitableResource {
    const Resource* resource = nullptr;
    int typeRank = 0;
    bool sameArea = false;
    std::string reason;
};

class ResourceManager {
public:
    std::string add(Resource resource);
    bool remove(const std::string& id);
    bool exists(const std::string& id) const;
    Resource* find(const std::string& id);
    const Resource* find(const std::string& id) const;

    std::vector<const Resource*> all() const;
    std::vector<const Resource*> search(ResourceSearchField field, const std::string& query) const;
    std::vector<const Resource*> byType(ResourceType type) const;
    std::vector<const Resource*> byStatus(ResourceStatus status) const;
    std::vector<const Resource*> assignedTo(const std::string& emergencyId) const;
    std::vector<const Resource*> acceptingAssignments() const;
    std::vector<std::string> demoIds() const;

    static std::vector<ResourceType> recommendedTypes(Category category);
    static bool isRecommended(ResourceType type, Category category);
    std::vector<SuitableResource> suitableFor(const Emergency& emergency) const;
    int capacityAssignedTo(const std::string& emergencyId) const;

    std::string assign(const std::string& resourceId, const std::string& emergencyId, std::time_t now);
    std::string release(const std::string& resourceId, const std::string& emergencyId, std::time_t now,
                        const std::string& note);
    int releaseAll(const std::string& emergencyId, std::time_t now, const std::string& note);
    std::string setOutOfService(const std::string& resourceId, bool outOfService);
    std::string setLocation(const std::string& resourceId, const std::string& location);

    const std::vector<AssignmentRecord>& history() const { return history_; }
    bool restoreRecord(const AssignmentRecord& record);
    std::vector<std::string> reconcileHistory(std::time_t now);
    void purgeHistory(const std::set<std::string>& ids);
    std::vector<std::pair<std::string, int>> deploymentCounts() const;

    std::string generateId() const;
    size_t size() const { return resources_.size(); }
    bool empty() const { return resources_.empty(); }
    void clear();

private:
    std::map<std::string, Resource> resources_;
    std::vector<AssignmentRecord> history_;
    long nextRecordId_ = 1;
};

}

#endif
#ifndef RESQ_DISPATCHER_H
#define RESQ_DISPATCHER_H


#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace resq {

struct OpResult {
    bool ok = false;
    std::string message;

    static OpResult success(std::string text) { return {true, std::move(text)}; }
    static OpResult failure(std::string text) { return {false, std::move(text)}; }
};

class Dispatcher {
public:
    Dispatcher(EmergencyManager& emergencies, ResourceManager& resources);

    OpResult assign(const std::string& emergencyId, const std::string& resourceId, std::time_t now);
    OpResult release(const std::string& emergencyId, const std::string& resourceId, std::time_t now,
                     bool force = false);
    OpResult startOperation(const std::string& emergencyId, std::time_t now);
    OpResult resolve(const std::string& emergencyId, std::time_t now);
    OpResult cancel(const std::string& emergencyId, std::time_t now);
    OpResult returnToQueue(const std::string& emergencyId, std::time_t now);
    OpResult changeStatus(const std::string& emergencyId, Status target, std::time_t now);
    std::vector<Status> allowedNextStatuses(const std::string& emergencyId) const;

    std::vector<std::string> reconcile(std::time_t now);

private:
    EmergencyManager& emergencies_;
    ResourceManager& resources_;
};

}

#endif
#ifndef RESQ_FILE_MANAGER_H
#define RESQ_FILE_MANAGER_H


#include <string>
#include <vector>

namespace resq {

struct LoadReport {
    int emergenciesLoaded = 0;
    int resourcesLoaded = 0;
    int assignmentsLoaded = 0;
    bool emergenciesFileFound = false;
    bool resourcesFileFound = false;
    bool assignmentsFileFound = false;
    std::vector<std::string> warnings;
    std::vector<std::string> preserved;

    bool anyFileFound() const { return emergenciesFileFound || resourcesFileFound || assignmentsFileFound; }
};

class FileManager {
public:
    explicit FileManager(std::string dataDirectory);

    const std::string& dataDirectory() const { return dataDirectory_; }
    std::string emergenciesPath() const;
    std::string resourcesPath() const;
    std::string assignmentsPath() const;

    LoadReport loadAll(EmergencyManager& emergencies, ResourceManager& resources) const;

    bool saveAll(const EmergencyManager& emergencies, const ResourceManager& resources, std::string& error) const;

    std::vector<std::string> archiveAll(const std::string& tag, std::string& error) const;

private:
    struct RawFile {
        bool found = false;
        bool usable = true;
        std::vector<std::pair<int, std::string>> records;
    };

    RawFile readFile(const std::string& path, const std::string& header, const std::string& label,
                     LoadReport& report) const;
    std::string pathFor(const char* fileName) const;
    void preserveDamaged(const std::string& path, LoadReport& report) const;
    bool writeFile(const std::string& path, const std::string& header, const std::vector<std::string>& comments,
                   const std::vector<std::string>& records, std::string& error) const;

    std::string dataDirectory_;
};

}

#endif
#ifndef RESQ_ANALYTICS_H
#define RESQ_ANALYTICS_H


#include <array>
#include <string>
#include <utility>
#include <vector>

namespace resq {

struct TypeUsage {
    int total = 0;
    int available = 0;
    int deployed = 0;
    int outOfService = 0;
};

struct Statistics {
    int totalEmergencies = 0;
    std::array<int, kStatusCount> byStatus{};
    std::array<int, kSeverityCount> bySeverity{};
    std::array<int, kCategoryCount> byCategory{};
    int activeCases = 0;
    int activeCritical = 0;
    int peopleAffectedActive = 0;

    int totalResources = 0;
    int availableResources = 0;
    int deployedResources = 0;
    int outOfServiceResources = 0;
    std::array<TypeUsage, kResourceTypeCount> byType{};
    int activeAssignments = 0;
    int totalAssignmentsLogged = 0;

    int responseSamples = 0;
    double averageResponseSeconds = 0.0;
    int resolutionSamples = 0;
    double averageResolutionSeconds = 0.0;

    int count(Status s) const { return byStatus[static_cast<size_t>(indexOf(s))]; }
    double utilizationPercent() const;
};

class AnalyticsManager {
public:
    AnalyticsManager(const EmergencyManager& emergencies, const ResourceManager& resources);

    Statistics compute() const;
    std::vector<std::pair<std::string, int>> mostDeployed(size_t limit) const;

private:
    const EmergencyManager& emergencies_;
    const ResourceManager& resources_;
};

}

#endif
#ifndef RESQ_DEMO_DATA_H
#define RESQ_DEMO_DATA_H


#include <ctime>

namespace resq {

struct DemoResult {
    int emergenciesAdded = 0;
    int resourcesAdded = 0;
    int skipped = 0;
};

class DemoData {
public:
    static DemoResult load(EmergencyManager& emergencies, ResourceManager& resources, Dispatcher& dispatcher,
                           std::time_t now);

    static int removeAll(EmergencyManager& emergencies, ResourceManager& resources, Dispatcher& dispatcher,
                         std::time_t now);

    static bool present(const EmergencyManager& emergencies, const ResourceManager& resources);
};

}

#endif
#ifndef RESQ_CONSOLE_H
#define RESQ_CONSOLE_H

#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

namespace resq {

enum class Style {
    Plain,
    Bold,
    Heading,
    Accent,
    Critical,
    Danger,
    High,
    Warning,
    Success,
    Info,
    Progress,
    Muted,
    Highlight,
    Badge,
};

class InputClosed : public std::runtime_error {
public:
    InputClosed() : std::runtime_error("input closed") {}
};

class InputCancelled : public std::runtime_error {
public:
    InputCancelled() : std::runtime_error("cancelled by user") {}
};

struct Glyphs {
    const char* topLeft;
    const char* topRight;
    const char* bottomLeft;
    const char* bottomRight;
    const char* horizontal;
    const char* vertical;
    const char* doubleTopLeft;
    const char* doubleTopRight;
    const char* doubleBottomLeft;
    const char* doubleBottomRight;
    const char* doubleHorizontal;
    const char* doubleVertical;
    const char* block;
    const char* shade;
    const char* ellipsis;
    const char* separator;
    const char* bullet;
};

class Console {
public:
    static constexpr int kWidth = 78;

    Console(std::istream& in, std::ostream& out, bool ansi, bool color, bool unicode, bool animations,
            bool interactiveInput, bool interactiveOutput);

    bool ansi() const { return ansi_; }
    bool color() const { return color_ && ansi_; }
    void setColor(bool enabled) { color_ = enabled; }
    bool unicode() const { return unicode_; }
    void setUnicode(bool enabled) { unicode_ = enabled; }
    bool animations() const { return animations_; }
    void setAnimations(bool enabled) { animations_ = enabled; }
    bool interactiveInput() const { return interactiveInput_; }
    const Glyphs& glyphs() const;
    std::ostream& out() const { return out_; }

    std::string paint(const std::string& text, Style style) const;
    static int visibleWidth(const std::string& text);
    std::string fit(const std::string& text, int width, bool alignRight = false) const;
    static std::string repeat(const std::string& unit, int count);

    void clearScreen() const;
    void printLogo() const;
    void printHeader(const std::string& title) const;
    void printSection(const std::string& title) const;
    void printRule() const;
    std::vector<std::string> renderBox(const std::string& title, const std::vector<std::string>& lines, int width,
                                       Style borderStyle = Style::Accent) const;
    void printBox(const std::string& title, const std::vector<std::string>& lines,
                  Style borderStyle = Style::Accent) const;
    void printSideBySide(const std::vector<std::string>& left, const std::vector<std::string>& right) const;
    void printBar(const std::string& label, int value, int maxValue, Style style, int labelWidth = 18,
                  int barWidth = 40) const;
    void printMenu(const std::vector<std::pair<std::string, std::string>>& items) const;

    void success(const std::string& message) const;
    void warning(const std::string& message) const;
    void error(const std::string& message) const;
    void info(const std::string& message) const;
    void spinner(const std::string& label, int milliseconds) const;

    std::string readLine(const std::string& prompt) const;
    std::string readChoice(const std::string& prompt) const;
    std::string readText(const std::string& prompt, size_t maxLength, bool required = true) const;
    long long readNumber(const std::string& prompt, long long minimum, long long maximum) const;
    long long readNumberOr(const std::string& prompt, long long minimum, long long maximum, long long fallback) const;
    bool confirm(const std::string& prompt, bool defaultYes = false) const;
    bool confirmWord(const std::string& prompt, const std::string& word) const;
    void pause() const;

    void pushScreen(const std::string& name) { screens_.push_back(name); }
    void popScreen()
    {
        if (!screens_.empty()) {
            screens_.pop_back();
        }
    }
    std::string breadcrumb() const;

private:
    std::string prompt(const std::string& text) const;
    void message(const std::string& tag, Style style, const std::string& text) const;

    std::istream& in_;
    std::ostream& out_;
    bool ansi_;
    bool color_;
    bool unicode_;
    bool animations_;
    bool interactiveInput_;
    bool interactiveOutput_;
    std::vector<std::string> screens_;
};

class ScreenScope {
public:
    ScreenScope(Console& console, const std::string& name) : console_(console) { console_.pushScreen(name); }
    ~ScreenScope() { console_.popScreen(); }
    ScreenScope(const ScreenScope&) = delete;
    ScreenScope& operator=(const ScreenScope&) = delete;

private:
    Console& console_;
};

struct Column {
    std::string title;
    int width;
    bool alignRight = false;
};

struct Cell {
    std::string text;
    Style style = Style::Plain;
};

class Table {
public:
    explicit Table(std::vector<Column> columns, int gap = 1);
    void addRow(std::vector<Cell> cells);
    bool empty() const { return rows_.empty(); }
    void print(const Console& console) const;

private:
    std::vector<Column> columns_;
    std::vector<std::vector<Cell>> rows_;
    int gap_;
};

}

#endif
#ifndef RESQ_APP_H
#define RESQ_APP_H


#include <ctime>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace resq {

constexpr const char* kVersion = "1.0.0";

struct AppOptions {
    std::string dataDirectory = "data";
    int color = -1;
    int unicode = -1;
    bool animations = true;
    bool showHelp = false;
    bool showVersion = false;

    static bool parse(int argc, char* argv[], AppOptions& options, std::string& error);
    static std::string usage();
};

class RESQApplication {
public:
    explicit RESQApplication(const AppOptions& options);
    int run();

private:
    void startup();
    void mainLoop();
    void shutdown();
    bool persist();
    void log(const std::string& activity);
    std::time_t now() const;

    void showDashboard();
    void registerEmergency();
    void viewEmergencies();
    void searchEmergencies();
    void updateEmergency();
    void changeStatusFlow(const std::string& emergencyId);

    void dispatchMenu();
    void processNextEmergency();
    void viewPriorityQueue();
    void manageActiveOperations();
    void manageOperation(const std::string& emergencyId);
    bool assignResourceFlow(const std::string& emergencyId);

    void resourceMenu();
    void addResource();
    void viewResources();
    void searchResources();
    void updateResourceAvailability();
    void assignResourceMenu();
    void releaseResourceMenu();
    void viewAssignmentHistory();

    void reportsMenu();
    void reportOverview();
    void reportBreakdown();
    void reportResources();
    void reportTiming();
    void demoMenu();
    void settingsMenu();
    void showDataStatus();
    void showPriorityHelp();
    void showAbout();
    void showHelp();

    void printEmergencyCard(const Emergency& emergency);
    void printScoreBreakdown(const Emergency& emergency);
    void printEmergencyTable(const std::vector<const Emergency*>& list);
    void printResourceTable(const std::vector<const Resource*>& list);
    void printResourceCard(const Resource& resource);
    std::vector<SuitableResource> printSuitableResources(const Emergency& emergency);
    std::vector<std::string> statusLines(const std::vector<std::pair<std::string, std::pair<std::string, Style>>>& rows,
                                         int width) const;

    Category pickCategory();
    Severity pickSeverity();
    ResourceType pickResourceType();
    std::string readValidated(const std::string& prompt, size_t maxLength,
                              const std::function<std::string(const std::string&)>& validate);
    std::time_t readRegistrationTime();
    Emergency* promptEmergency(const std::string& prompt);
    Resource* promptResource(const std::string& prompt);

    AppOptions options_;
    Console console_;
    EmergencyManager emergencies_;
    ResourceManager resources_;
    FileManager files_;
    Dispatcher dispatcher_;
    AnalyticsManager analytics_;
    LoadReport loadReport_;
    std::vector<std::string> reconcileNotes_;
    std::deque<std::string> recentActivity_;
    std::time_t lastSaved_ = 0;
    int exitCode_ = 0;
};

}

#endif

namespace resq {

namespace {

template <typename E>
bool matchCode(const std::vector<E>& values, const std::string& code, E& out)
{
    for (E value : values) {
        if (toCode(value) == code) {
            out = value;
            return true;
        }
    }
    return false;
}

template <typename E>
bool matchNumber(const std::vector<E>& values, int number, E& out)
{
    if (number < 1 || number > static_cast<int>(values.size())) {
        return false;
    }
    out = values[static_cast<size_t>(number - 1)];
    return true;
}

}

const std::vector<Category>& allCategories()
{
    static const std::vector<Category> values{Category::Medical, Category::Fire, Category::RoadAccident,
                                              Category::NaturalDisaster, Category::Flood, Category::Rescue,
                                              Category::Other};
    return values;
}

const std::vector<Severity>& allSeverities()
{
    static const std::vector<Severity> values{Severity::Critical, Severity::High, Severity::Medium, Severity::Low};
    return values;
}

const std::vector<Status>& allStatuses()
{
    static const std::vector<Status> values{Status::Pending, Status::Assigned, Status::InProgress, Status::Resolved,
                                            Status::Cancelled};
    return values;
}

const std::vector<ResourceType>& allResourceTypes()
{
    static const std::vector<ResourceType> values{ResourceType::Ambulance,  ResourceType::MedicalTeam,
                                                  ResourceType::FireEngine, ResourceType::RescueTeam,
                                                  ResourceType::ReliefVehicle, ResourceType::SupplyKit};
    return values;
}

const std::vector<ResourceStatus>& allResourceStatuses()
{
    static const std::vector<ResourceStatus> values{ResourceStatus::Available, ResourceStatus::Deployed,
                                                    ResourceStatus::OutOfService};
    return values;
}

std::string toString(Category value)
{
    switch (value) {
    case Category::Medical: return "Medical emergency";
    case Category::Fire: return "Fire";
    case Category::RoadAccident: return "Road accident";
    case Category::NaturalDisaster: return "Natural disaster";
    case Category::Flood: return "Flood";
    case Category::Rescue: return "Rescue operation";
    case Category::Other: return "Other";
    }
    return "Unknown";
}

std::string toString(Severity value)
{
    switch (value) {
    case Severity::Critical: return "Critical";
    case Severity::High: return "High";
    case Severity::Medium: return "Medium";
    case Severity::Low: return "Low";
    }
    return "Unknown";
}

std::string toString(Status value)
{
    switch (value) {
    case Status::Pending: return "Pending";
    case Status::Assigned: return "Assigned";
    case Status::InProgress: return "In Progress";
    case Status::Resolved: return "Resolved";
    case Status::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

std::string toString(ResourceType value)
{
    switch (value) {
    case ResourceType::Ambulance: return "Ambulance";
    case ResourceType::MedicalTeam: return "Medical team";
    case ResourceType::FireEngine: return "Fire engine";
    case ResourceType::RescueTeam: return "Rescue team";
    case ResourceType::ReliefVehicle: return "Disaster-relief vehicle";
    case ResourceType::SupplyKit: return "Emergency supply kit";
    }
    return "Unknown";
}

std::string toString(ResourceStatus value)
{
    switch (value) {
    case ResourceStatus::Available: return "Available";
    case ResourceStatus::Deployed: return "Deployed";
    case ResourceStatus::OutOfService: return "Out of service";
    }
    return "Unknown";
}

std::string shortName(Category value)
{
    switch (value) {
    case Category::Medical: return "Medical";
    case Category::Fire: return "Fire";
    case Category::RoadAccident: return "Road acc.";
    case Category::NaturalDisaster: return "Nat.disaster";
    case Category::Flood: return "Flood";
    case Category::Rescue: return "Rescue op.";
    case Category::Other: return "Other";
    }
    return "Unknown";
}

std::string shortName(ResourceType value)
{
    switch (value) {
    case ResourceType::Ambulance: return "Ambulance";
    case ResourceType::MedicalTeam: return "Medical team";
    case ResourceType::FireEngine: return "Fire engine";
    case ResourceType::RescueTeam: return "Rescue team";
    case ResourceType::ReliefVehicle: return "Relief vehicle";
    case ResourceType::SupplyKit: return "Supply kit";
    }
    return "Unknown";
}

std::string toCode(Category value)
{
    switch (value) {
    case Category::Medical: return "MEDICAL";
    case Category::Fire: return "FIRE";
    case Category::RoadAccident: return "ROAD_ACCIDENT";
    case Category::NaturalDisaster: return "NATURAL_DISASTER";
    case Category::Flood: return "FLOOD";
    case Category::Rescue: return "RESCUE";
    case Category::Other: return "OTHER";
    }
    return "UNKNOWN";
}

std::string toCode(Severity value)
{
    switch (value) {
    case Severity::Critical: return "CRITICAL";
    case Severity::High: return "HIGH";
    case Severity::Medium: return "MEDIUM";
    case Severity::Low: return "LOW";
    }
    return "UNKNOWN";
}

std::string toCode(Status value)
{
    switch (value) {
    case Status::Pending: return "PENDING";
    case Status::Assigned: return "ASSIGNED";
    case Status::InProgress: return "IN_PROGRESS";
    case Status::Resolved: return "RESOLVED";
    case Status::Cancelled: return "CANCELLED";
    }
    return "UNKNOWN";
}

std::string toCode(ResourceType value)
{
    switch (value) {
    case ResourceType::Ambulance: return "AMBULANCE";
    case ResourceType::MedicalTeam: return "MEDICAL_TEAM";
    case ResourceType::FireEngine: return "FIRE_ENGINE";
    case ResourceType::RescueTeam: return "RESCUE_TEAM";
    case ResourceType::ReliefVehicle: return "RELIEF_VEHICLE";
    case ResourceType::SupplyKit: return "SUPPLY_KIT";
    }
    return "UNKNOWN";
}

bool fromCode(const std::string& code, Category& out) { return matchCode(allCategories(), code, out); }
bool fromCode(const std::string& code, Severity& out) { return matchCode(allSeverities(), code, out); }
bool fromCode(const std::string& code, Status& out) { return matchCode(allStatuses(), code, out); }
bool fromCode(const std::string& code, ResourceType& out) { return matchCode(allResourceTypes(), code, out); }

bool fromNumber(int number, Category& out) { return matchNumber(allCategories(), number, out); }
bool fromNumber(int number, Severity& out) { return matchNumber(allSeverities(), number, out); }
bool fromNumber(int number, Status& out) { return matchNumber(allStatuses(), number, out); }
bool fromNumber(int number, ResourceType& out) { return matchNumber(allResourceTypes(), number, out); }

}


#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace resq {
namespace util {

std::string trim(const std::string& text)
{
    const char* spaces = " \t\r\n\v\f";
    const size_t first = text.find_first_not_of(spaces);
    if (first == std::string::npos) {
        return "";
    }
    const size_t last = text.find_last_not_of(spaces);
    return text.substr(first, last - first + 1);
}

std::string toUpper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

std::string toLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool iequals(const std::string& a, const std::string& b)
{
    return toLower(a) == toLower(b);
}

bool icontains(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) {
        return true;
    }
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

size_t utf8Length(const std::string& text)
{
    size_t count = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

size_t utf8Offset(const std::string& text, size_t chars)
{
    size_t seen = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c & 0xC0) != 0x80) {
            if (seen == chars) {
                return i;
            }
            ++seen;
        }
    }
    return text.size();
}

bool isPrintableText(const std::string& text)
{
    for (unsigned char c : text) {
        if (c < 0x20 || c == 0x7F) {
            return false;
        }
    }
    return true;
}

bool isValidId(const std::string& id)
{
    if (id.size() < 3 || id.size() > 8) {
        return false;
    }
    if (!std::isupper(static_cast<unsigned char>(id[0]))) {
        return false;
    }
    for (unsigned char c : id) {
        if (!(std::isupper(c) || std::isdigit(c) || c == '-')) {
            return false;
        }
    }
    return true;
}

bool parseInteger(const std::string& text, long long& out)
{
    const std::string t = trim(text);
    if (t.empty()) {
        return false;
    }
    size_t start = (t[0] == '+' || t[0] == '-') ? 1 : 0;
    if (start == t.size() || t.size() - start > 18) {
        return false;
    }
    for (size_t i = start; i < t.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(t[i]))) {
            return false;
        }
    }
    out = std::stoll(t);
    return true;
}

std::string escapeField(const std::string& field)
{
    std::string out;
    out.reserve(field.size());
    for (char c : field) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '|': out += "\\p"; break;
        case '\n': out += "\\n"; break;
        case '\r': break;
        default: out += c; break;
        }
    }
    return out;
}

std::vector<std::string> splitEscaped(const std::string& line)
{
    std::vector<std::string> fields;
    std::string current;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '\\' && i + 1 < line.size()) {
            const char next = line[++i];
            if (next == 'p') {
                current += '|';
            } else if (next == 'n') {
                current += '\n';
            } else {
                current += next;
            }
        } else if (c == '|') {
            fields.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    fields.push_back(current);
    return fields;
}

std::vector<std::string> splitPlain(const std::string& text, char delimiter)
{
    std::vector<std::string> parts;
    std::string current;
    std::istringstream in(text);
    while (std::getline(in, current, delimiter)) {
        current = trim(current);
        if (!current.empty()) {
            parts.push_back(current);
        }
    }
    return parts;
}

std::vector<std::string> wrapText(const std::string& text, size_t width)
{
    std::vector<std::string> lines;
    if (width == 0) {
        lines.push_back(text);
        return lines;
    }
    std::istringstream in(text);
    std::string word;
    std::string current;
    while (in >> word) {
        while (utf8Length(word) > width) {
            if (!current.empty()) {
                lines.push_back(current);
                current.clear();
            }
            const size_t cut = utf8Offset(word, width);
            lines.push_back(word.substr(0, cut));
            word = word.substr(cut);
        }
        if (word.empty()) {
            continue;
        }
        if (current.empty()) {
            current = word;
        } else if (utf8Length(current) + 1 + utf8Length(word) <= width) {
            current += " " + word;
        } else {
            lines.push_back(current);
            current = word;
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    if (lines.empty()) {
        lines.push_back("");
    }
    return lines;
}

namespace {

std::string formatWith(std::time_t value, const char* pattern)
{
    if (value <= 0) {
        return "-";
    }
    const std::tm* local = std::localtime(&value);
    if (local == nullptr) {
        return "-";
    }
    char buffer[64];
    if (std::strftime(buffer, sizeof(buffer), pattern, local) == 0) {
        return "-";
    }
    return buffer;
}

}

std::string formatDateTime(std::time_t value) { return formatWith(value, "%Y-%m-%d %H:%M"); }
std::string formatCompact(std::time_t value) { return formatWith(value, "%m-%d %H:%M"); }
std::string formatClock(std::time_t value) { return formatWith(value, "%H:%M"); }

std::string formatDuration(long long seconds)
{
    if (seconds < 0) {
        return "-";
    }
    if (seconds < 60) {
        return "<1m";
    }
    const long long minutes = seconds / 60;
    const long long days = minutes / (24 * 60);
    const long long hours = (minutes / 60) % 24;
    const long long mins = minutes % 60;
    std::ostringstream out;
    if (days > 0) {
        out << days << "d " << std::setw(2) << std::setfill('0') << hours << "h";
    } else if (hours > 0) {
        out << hours << "h " << std::setw(2) << std::setfill('0') << mins << "m";
    } else {
        out << mins << "m";
    }
    return out.str();
}

bool parseDateTime(const std::string& text, std::time_t& out)
{
    const std::string t = trim(text);
    int year = 0, month = 0, day = 0, hour = 0, minute = 0;
    char extra = '\0';
    if (std::sscanf(t.c_str(), "%4d-%2d-%2d %2d:%2d%c", &year, &month, &day, &hour, &minute, &extra) != 5) {
        return false;
    }
    if (year < 1971 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59) {
        return false;
    }
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_isdst = -1;
    const std::time_t result = std::mktime(&tm);
    if (result == static_cast<std::time_t>(-1)) {
        return false;
    }
    if (tm.tm_mday != day || tm.tm_mon != month - 1 || tm.tm_year != year - 1900) {
        return false;
    }
    out = result;
    return true;
}

std::string obfuscate(const std::string& plain)
{
    std::vector<char> buffer(plain.size() * 2 + 1, '\0');
    resq_obfuscate_hex(plain.c_str(), buffer.data(), buffer.size());
    return "X1:" + std::string(buffer.data());
}

bool reveal(const std::string& stored, std::string& plain)
{
    if (stored.rfind("X1:", 0) != 0) {
        plain = stored;
        return true;
    }
    const std::string hex = stored.substr(3);
    std::vector<char> buffer(hex.size() / 2 + 1, '\0');
    if (!resq_deobfuscate_hex(hex.c_str(), buffer.data(), buffer.size())) {
        return false;
    }
    plain.assign(buffer.data());
    return true;
}

std::string maskContact(const std::string& contact)
{
    const size_t length = utf8Length(contact);
    if (length <= 3) {
        return std::string(length, '*');
    }
    const size_t keepFrom = utf8Offset(contact, length - 3);
    return std::string(length - 3, '*') + contact.substr(keepFrom);
}

std::string hex32(std::uint32_t value)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%08X", static_cast<unsigned int>(value));
    return buffer;
}

}
}

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


#include <algorithm>
#include <stdexcept>

namespace resq {

namespace {

void requireValid(const std::string& error)
{
    if (!error.empty()) {
        throw std::invalid_argument(error);
    }
}

}

Resource::Resource(std::string id, ResourceType type, std::string location, int capacity, int maxConcurrent,
                   bool demo)
    : id_(util::toUpper(util::trim(id))),
      type_(type),
      location_(util::trim(location)),
      capacity_(capacity),
      maxConcurrent_(maxConcurrent),
      demo_(demo)
{
    requireValid(validateId(id_));
    requireValid(validateLocation(location_));
    requireValid(validateCapacity(capacity_));
    requireValid(validateMaxConcurrent(maxConcurrent_));
}

ResourceStatus Resource::status() const
{
    if (outOfService_) {
        return ResourceStatus::OutOfService;
    }
    return assigned_.empty() ? ResourceStatus::Available : ResourceStatus::Deployed;
}

bool Resource::isAssignedTo(const std::string& emergencyId) const
{
    return std::find(assigned_.begin(), assigned_.end(), util::toUpper(emergencyId)) != assigned_.end();
}

std::string Resource::statusLabel() const
{
    switch (status()) {
    case ResourceStatus::Available:
        return maxConcurrent_ > 1 ? "Available (0/" + std::to_string(maxConcurrent_) + ")" : "Available";
    case ResourceStatus::Deployed:
        return "Deployed (" + std::to_string(activeAssignments()) + "/" + std::to_string(maxConcurrent_) + ")";
    case ResourceStatus::OutOfService: return "Out of service";
    }
    return "Unknown";
}

void Resource::setLocation(const std::string& location)
{
    const std::string value = util::trim(location);
    requireValid(validateLocation(value));
    location_ = value;
}

void Resource::setOutOfService(bool outOfService)
{
    if (outOfService && !assigned_.empty()) {
        throw std::logic_error("Resource " + id_ +
                               " is deployed. Release it from its emergencies before marking it out of service.");
    }
    outOfService_ = outOfService;
}

void Resource::assignTo(const std::string& emergencyId)
{
    const std::string id = util::toUpper(util::trim(emergencyId));
    if (outOfService_) {
        throw std::logic_error("Resource " + id_ + " is out of service and cannot be assigned.");
    }
    if (isAssignedTo(id)) {
        throw std::logic_error("Resource " + id_ + " is already assigned to " + id + ".");
    }
    if (!hasFreeSlot()) {
        throw std::logic_error("Resource " + id_ + " is already deployed to " + assigned_.front() +
                               " and is configured for at most " + std::to_string(maxConcurrent_) +
                               " simultaneous emergenc" + (maxConcurrent_ == 1 ? "y." : "ies."));
    }
    assigned_.push_back(id);
}

bool Resource::releaseFrom(const std::string& emergencyId)
{
    const auto it = std::find(assigned_.begin(), assigned_.end(), util::toUpper(emergencyId));
    if (it == assigned_.end()) {
        return false;
    }
    assigned_.erase(it);
    return true;
}

std::string Resource::validateId(const std::string& id)
{
    if (id.empty()) {
        return "Resource ID is required.";
    }
    if (!util::isValidId(id)) {
        return "Resource ID must be 3-8 characters (letters, digits or '-') and start with a letter.";
    }
    return "";
}

std::string Resource::validateLocation(const std::string& location)
{
    if (location.empty()) {
        return "Location is required.";
    }
    if (util::utf8Length(location) > kMaxLocation) {
        return "Location must be at most " + std::to_string(kMaxLocation) + " characters.";
    }
    if (!util::isPrintableText(location)) {
        return "Location contains invalid control characters.";
    }
    return "";
}

std::string Resource::validateCapacity(long long capacity)
{
    if (capacity < 0 || capacity > kMaxCapacity) {
        return "Capacity must be between 0 and " + std::to_string(kMaxCapacity) + ".";
    }
    return "";
}

std::string Resource::validateMaxConcurrent(long long maxConcurrent)
{
    if (maxConcurrent < 1 || maxConcurrent > kMaxConcurrent) {
        return "Simultaneous assignments must be between 1 and " + std::to_string(kMaxConcurrent) + ".";
    }
    return "";
}

}


#include <algorithm>
#include <cctype>
#include <queue>
#include <stdexcept>

namespace resq {

namespace {

std::string caseKey(const std::string& id) { return util::toUpper(util::trim(id)); }

struct QueueOrder {
    bool operator()(const Emergency* a, const Emergency* b) const { return EmergencyManager::comesFirst(*b, *a); }
};

}

bool EmergencyManager::comesFirst(const Emergency& a, const Emergency& b)
{
    if (a.priorityScore() != b.priorityScore()) {
        return a.priorityScore() > b.priorityScore();
    }
    if (a.registeredAt() != b.registeredAt()) {
        return a.registeredAt() < b.registeredAt();
    }
    return a.id() < b.id();
}

std::string EmergencyManager::add(Emergency emergency)
{
    if (exists(emergency.id())) {
        return "An emergency with ID " + emergency.id() + " already exists. IDs must be unique.";
    }
    const std::string id = emergency.id();
    items_.emplace(id, std::move(emergency));
    return "";
}

bool EmergencyManager::remove(const std::string& id) { return items_.erase(caseKey(id)) > 0; }

bool EmergencyManager::exists(const std::string& id) const { return items_.count(caseKey(id)) > 0; }

Emergency* EmergencyManager::find(const std::string& id)
{
    const auto it = items_.find(caseKey(id));
    return it == items_.end() ? nullptr : &it->second;
}

const Emergency* EmergencyManager::find(const std::string& id) const
{
    const auto it = items_.find(caseKey(id));
    return it == items_.end() ? nullptr : &it->second;
}

std::vector<const Emergency*> EmergencyManager::all() const
{
    std::vector<const Emergency*> result;
    result.reserve(items_.size());
    for (const auto& entry : items_) {
        result.push_back(&entry.second);
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::sorted(EmergencySortKey sortKey, bool reverse) const
{
    std::vector<const Emergency*> result = all();
    const auto byPriority = [](const Emergency* a, const Emergency* b) { return comesFirst(*a, *b); };
    switch (sortKey) {
    case EmergencySortKey::Severity:
        std::sort(result.begin(), result.end(), [&](const Emergency* a, const Emergency* b) {
            if (a->severity() != b->severity()) {
                return a->severity() < b->severity();
            }
            return byPriority(a, b);
        });
        break;
    case EmergencySortKey::Priority: std::sort(result.begin(), result.end(), byPriority); break;
    case EmergencySortKey::RegistrationTime:
        std::sort(result.begin(), result.end(), [](const Emergency* a, const Emergency* b) {
            if (a->registeredAt() != b->registeredAt()) {
                return a->registeredAt() < b->registeredAt();
            }
            return a->id() < b->id();
        });
        break;
    case EmergencySortKey::Id: break;
    }
    if (reverse) {
        std::reverse(result.begin(), result.end());
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::search(EmergencySearchField field, const std::string& query) const
{
    const std::string q = util::trim(query);
    std::vector<const Emergency*> result;
    if (q.empty()) {
        return result;
    }
    if (field == EmergencySearchField::Id) {
        if (const Emergency* exact = find(q)) {
            result.push_back(exact);
            return result;
        }
    }
    for (const auto& entry : items_) {
        const Emergency& e = entry.second;
        const std::string& text = field == EmergencySearchField::Id         ? e.id()
                                  : field == EmergencySearchField::Reporter ? e.reporter()
                                                                            : e.location();
        if (util::icontains(text, q)) {
            result.push_back(&e);
        }
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::searchByCategory(Category category) const
{
    std::vector<const Emergency*> result;
    for (const auto& entry : items_) {
        if (entry.second.category() == category) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<const Emergency*> EmergencyManager::withStatus(Status status) const
{
    std::vector<const Emergency*> result;
    for (const auto& entry : items_) {
        if (entry.second.status() == status) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<std::string> EmergencyManager::demoIds() const
{
    std::vector<std::string> ids;
    for (const auto& entry : items_) {
        if (entry.second.isDemo()) {
            ids.push_back(entry.first);
        }
    }
    return ids;
}

void EmergencyManager::recalculateAll(std::time_t now)
{
    for (auto& entry : items_) {
        entry.second.recalculatePriority(now);
    }
}

std::vector<const Emergency*> EmergencyManager::pendingByPriority(std::time_t now)
{
    recalculateAll(now);
    std::priority_queue<const Emergency*, std::vector<const Emergency*>, QueueOrder> queue;
    for (const auto& entry : items_) {
        if (entry.second.status() == Status::Pending) {
            queue.push(&entry.second);
        }
    }
    std::vector<const Emergency*> ordered;
    ordered.reserve(queue.size());
    while (!queue.empty()) {
        ordered.push_back(queue.top());
        queue.pop();
    }
    return ordered;
}

const Emergency* EmergencyManager::highestPriorityPending(std::time_t now)
{
    const std::vector<const Emergency*> queue = pendingByPriority(now);
    return queue.empty() ? nullptr : queue.front();
}

int EmergencyManager::queuePosition(const std::string& id, std::time_t now)
{
    const std::vector<const Emergency*> queue = pendingByPriority(now);
    for (size_t i = 0; i < queue.size(); ++i) {
        if (queue[i]->id() == caseKey(id)) {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

std::string EmergencyManager::modify(const std::string& id, std::time_t now,
                                     const std::function<void(Emergency&)>& change)
{
    Emergency* emergency = find(id);
    if (emergency == nullptr) {
        return "No emergency with ID " + caseKey(id) + " exists.";
    }
    try {
        change(*emergency);
    } catch (const std::exception& ex) {
        return ex.what();
    }
    emergency->recalculatePriority(now);
    return "";
}

std::string EmergencyManager::updateSeverity(const std::string& id, Severity severity, std::time_t now)
{
    return modify(id, now, [severity](Emergency& e) { e.setSeverity(severity); });
}

std::string EmergencyManager::updateLocation(const std::string& id, const std::string& location, std::time_t now)
{
    return modify(id, now, [&location](Emergency& e) { e.setLocation(location); });
}

std::string EmergencyManager::updateDescription(const std::string& id, const std::string& description,
                                                std::time_t now)
{
    return modify(id, now, [&description](Emergency& e) { e.setDescription(description); });
}

std::string EmergencyManager::updateAffected(const std::string& id, int affected, std::time_t now)
{
    return modify(id, now, [affected](Emergency& e) { e.setAffected(affected); });
}

std::string EmergencyManager::generateId() const
{
    long long highest = 1000;
    for (const auto& entry : items_) {
        const std::string& id = entry.first;
        long long number = 0;
        if (id.size() > 2 && id.compare(0, 2, "E-") == 0 && util::parseInteger(id.substr(2), number) &&
            std::isdigit(static_cast<unsigned char>(id[2]))) {
            highest = std::max(highest, number);
        }
    }
    for (long long candidate = highest + 1; candidate < 100000; ++candidate) {
        const std::string id = "E-" + std::to_string(candidate);
        if (!exists(id)) {
            return id;
        }
    }
    throw std::runtime_error("No automatic emergency IDs left; please enter an ID manually.");
}

}


#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace resq {

namespace {

std::string resourceKey(const std::string& id) { return util::toUpper(util::trim(id)); }

bool sameArea(const std::string& a, const std::string& b)
{
    const std::string x = util::toLower(util::trim(a));
    const std::string y = util::toLower(util::trim(b));
    if (x.size() < 3 || y.size() < 3) {
        return x == y && !x.empty();
    }
    return x.find(y) != std::string::npos || y.find(x) != std::string::npos;
}

}

std::string ResourceManager::add(Resource resource)
{
    if (exists(resource.id())) {
        return "A resource with ID " + resource.id() + " already exists. IDs must be unique.";
    }
    const std::string id = resource.id();
    resources_.emplace(id, std::move(resource));
    return "";
}

bool ResourceManager::remove(const std::string& id) { return resources_.erase(resourceKey(id)) > 0; }

bool ResourceManager::exists(const std::string& id) const { return resources_.count(resourceKey(id)) > 0; }

Resource* ResourceManager::find(const std::string& id)
{
    const auto it = resources_.find(resourceKey(id));
    return it == resources_.end() ? nullptr : &it->second;
}

const Resource* ResourceManager::find(const std::string& id) const
{
    const auto it = resources_.find(resourceKey(id));
    return it == resources_.end() ? nullptr : &it->second;
}

std::vector<const Resource*> ResourceManager::all() const
{
    std::vector<const Resource*> result;
    result.reserve(resources_.size());
    for (const auto& entry : resources_) {
        result.push_back(&entry.second);
    }
    return result;
}

std::vector<const Resource*> ResourceManager::search(ResourceSearchField field, const std::string& query) const
{
    const std::string q = util::trim(query);
    std::vector<const Resource*> result;
    if (q.empty()) {
        return result;
    }
    if (field == ResourceSearchField::Id) {
        if (const Resource* exact = find(q)) {
            result.push_back(exact);
            return result;
        }
    }
    for (const auto& entry : resources_) {
        const Resource& r = entry.second;
        bool match = false;
        switch (field) {
        case ResourceSearchField::Id: match = util::icontains(r.id(), q); break;
        case ResourceSearchField::Type:
            match = util::icontains(toString(r.type()), q) || util::icontains(shortName(r.type()), q);
            break;
        case ResourceSearchField::Location: match = util::icontains(r.location(), q); break;
        case ResourceSearchField::Status: match = util::icontains(toString(r.status()), q); break;
        }
        if (match) {
            result.push_back(&r);
        }
    }
    return result;
}

std::vector<const Resource*> ResourceManager::byType(ResourceType type) const
{
    std::vector<const Resource*> result;
    for (const auto& entry : resources_) {
        if (entry.second.type() == type) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<const Resource*> ResourceManager::byStatus(ResourceStatus status) const
{
    std::vector<const Resource*> result;
    for (const auto& entry : resources_) {
        if (entry.second.status() == status) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<const Resource*> ResourceManager::assignedTo(const std::string& emergencyId) const
{
    std::vector<const Resource*> result;
    for (const auto& entry : resources_) {
        if (entry.second.isAssignedTo(resourceKey(emergencyId))) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<const Resource*> ResourceManager::acceptingAssignments() const
{
    std::vector<const Resource*> result;
    for (const auto& entry : resources_) {
        if (entry.second.canAccept()) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<std::string> ResourceManager::demoIds() const
{
    std::vector<std::string> ids;
    for (const auto& entry : resources_) {
        if (entry.second.isDemo()) {
            ids.push_back(entry.first);
        }
    }
    return ids;
}

std::vector<ResourceType> ResourceManager::recommendedTypes(Category category)
{
    using T = ResourceType;
    switch (category) {
    case Category::Medical: return {T::Ambulance, T::MedicalTeam};
    case Category::Fire: return {T::FireEngine, T::Ambulance, T::RescueTeam};
    case Category::RoadAccident: return {T::Ambulance, T::RescueTeam, T::MedicalTeam};
    case Category::NaturalDisaster: return {T::RescueTeam, T::ReliefVehicle, T::MedicalTeam, T::SupplyKit};
    case Category::Flood: return {T::RescueTeam, T::ReliefVehicle, T::SupplyKit, T::Ambulance};
    case Category::Rescue: return {T::RescueTeam, T::MedicalTeam, T::Ambulance};
    case Category::Other: return allResourceTypes();
    }
    return allResourceTypes();
}

bool ResourceManager::isRecommended(ResourceType type, Category category)
{
    const std::vector<ResourceType> types = recommendedTypes(category);
    return std::find(types.begin(), types.end(), type) != types.end();
}

std::vector<SuitableResource> ResourceManager::suitableFor(const Emergency& emergency) const
{
    const std::vector<ResourceType> types = recommendedTypes(emergency.category());
    std::vector<SuitableResource> result;
    for (const auto& entry : resources_) {
        const Resource& r = entry.second;
        if (!r.canAccept() || r.isAssignedTo(emergency.id())) {
            continue;
        }
        const auto it = std::find(types.begin(), types.end(), r.type());
        if (it == types.end()) {
            continue;
        }
        SuitableResource s;
        s.resource = &r;
        s.typeRank = static_cast<int>(it - types.begin());
        s.sameArea = sameArea(r.location(), emergency.location());
        s.reason = std::string(s.typeRank == 0 ? "Primary" : "Secondary") + (s.sameArea ? ", same area" : " type");
        result.push_back(s);
    }
    std::sort(result.begin(), result.end(), [](const SuitableResource& a, const SuitableResource& b) {
        if (a.typeRank != b.typeRank) {
            return a.typeRank < b.typeRank;
        }
        if (a.sameArea != b.sameArea) {
            return a.sameArea;
        }
        if (a.resource->activeAssignments() != b.resource->activeAssignments()) {
            return a.resource->activeAssignments() < b.resource->activeAssignments();
        }
        return a.resource->id() < b.resource->id();
    });
    return result;
}

int ResourceManager::capacityAssignedTo(const std::string& emergencyId) const
{
    int total = 0;
    for (const Resource* r : assignedTo(emergencyId)) {
        total += r->capacity();
    }
    return total;
}

std::string ResourceManager::assign(const std::string& resourceId, const std::string& emergencyId, std::time_t now)
{
    Resource* resource = find(resourceId);
    if (resource == nullptr) {
        return "No resource with ID " + resourceKey(resourceId) + " exists.";
    }
    try {
        resource->assignTo(emergencyId);
    } catch (const std::exception& ex) {
        return ex.what();
    }
    AssignmentRecord record;
    record.id = nextRecordId_++;
    record.emergencyId = resourceKey(emergencyId);
    record.resourceId = resource->id();
    record.assignedAt = now;
    history_.push_back(record);
    return "";
}

std::string ResourceManager::release(const std::string& resourceId, const std::string& emergencyId,
                                     std::time_t now, const std::string& note)
{
    Resource* resource = find(resourceId);
    if (resource == nullptr) {
        return "No resource with ID " + resourceKey(resourceId) + " exists.";
    }
    if (!resource->releaseFrom(emergencyId)) {
        return "Resource " + resource->id() + " is not assigned to " + resourceKey(emergencyId) + ".";
    }
    for (AssignmentRecord& record : history_) {
        if (record.isActive() && record.resourceId == resource->id() && record.emergencyId == resourceKey(emergencyId)) {
            record.releasedAt = std::max(now, record.assignedAt);
            record.note = note;
        }
    }
    return "";
}

int ResourceManager::releaseAll(const std::string& emergencyId, std::time_t now, const std::string& note)
{
    int released = 0;
    for (const Resource* r : assignedTo(emergencyId)) {
        if (release(r->id(), emergencyId, now, note).empty()) {
            ++released;
        }
    }
    return released;
}

std::string ResourceManager::setOutOfService(const std::string& resourceId, bool outOfService)
{
    Resource* resource = find(resourceId);
    if (resource == nullptr) {
        return "No resource with ID " + resourceKey(resourceId) + " exists.";
    }
    try {
        resource->setOutOfService(outOfService);
    } catch (const std::exception& ex) {
        return ex.what();
    }
    return "";
}

std::string ResourceManager::setLocation(const std::string& resourceId, const std::string& location)
{
    Resource* resource = find(resourceId);
    if (resource == nullptr) {
        return "No resource with ID " + resourceKey(resourceId) + " exists.";
    }
    try {
        resource->setLocation(location);
    } catch (const std::exception& ex) {
        return ex.what();
    }
    return "";
}

bool ResourceManager::restoreRecord(const AssignmentRecord& record)
{
    for (const AssignmentRecord& existing : history_) {
        if (existing.id == record.id) {
            return false;
        }
    }
    history_.push_back(record);
    nextRecordId_ = std::max(nextRecordId_, record.id + 1);
    return true;
}

std::vector<std::string> ResourceManager::reconcileHistory(std::time_t now)
{
    std::vector<std::string> notes;
    for (AssignmentRecord& record : history_) {
        if (!record.isActive()) {
            continue;
        }
        const Resource* resource = find(record.resourceId);
        if (resource == nullptr || !resource->isAssignedTo(record.emergencyId)) {
            record.releasedAt = std::max(now, record.assignedAt);
            record.note = "Closed during data check";
            notes.push_back("Assignment #" + std::to_string(record.id) + " (" + record.resourceId + " -> " +
                            record.emergencyId + ") had no matching resource assignment and was closed.");
        }
    }
    for (const auto& entry : resources_) {
        for (const std::string& emergencyId : entry.second.assignedEmergencies()) {
            const bool recorded = std::any_of(history_.begin(), history_.end(), [&](const AssignmentRecord& r) {
                return r.isActive() && r.resourceId == entry.first && r.emergencyId == emergencyId;
            });
            if (!recorded) {
                AssignmentRecord record;
                record.id = nextRecordId_++;
                record.resourceId = entry.first;
                record.emergencyId = emergencyId;
                record.assignedAt = now;
                record.note = "Recreated during data check";
                history_.push_back(record);
                notes.push_back("Missing history entry recreated for " + entry.first + " -> " + emergencyId + ".");
            }
        }
    }
    return notes;
}

void ResourceManager::purgeHistory(const std::set<std::string>& ids)
{
    history_.erase(std::remove_if(history_.begin(), history_.end(),
                                  [&ids](const AssignmentRecord& r) {
                                      return ids.count(r.resourceId) > 0 || ids.count(r.emergencyId) > 0;
                                  }),
                   history_.end());
}

std::vector<std::pair<std::string, int>> ResourceManager::deploymentCounts() const
{
    std::map<std::string, int> counts;
    for (const AssignmentRecord& record : history_) {
        ++counts[record.resourceId];
    }
    std::vector<std::pair<std::string, int>> result(counts.begin(), counts.end());
    std::stable_sort(result.begin(), result.end(),
                     [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                         return a.second > b.second;
                     });
    return result;
}

std::string ResourceManager::generateId() const
{
    long long highest = 100;
    for (const auto& entry : resources_) {
        const std::string& id = entry.first;
        long long number = 0;
        if (id.size() > 2 && id.compare(0, 2, "R-") == 0 && std::isdigit(static_cast<unsigned char>(id[2])) &&
            util::parseInteger(id.substr(2), number)) {
            highest = std::max(highest, number);
        }
    }
    for (long long candidate = highest + 1; candidate < 100000; ++candidate) {
        const std::string id = "R-" + std::to_string(candidate);
        if (!exists(id)) {
            return id;
        }
    }
    throw std::runtime_error("No automatic resource IDs left; please enter an ID manually.");
}

void ResourceManager::clear()
{
    resources_.clear();
    history_.clear();
    nextRecordId_ = 1;
}

}


#include <stdexcept>

namespace resq {

namespace {

std::string notFound(const std::string& id) { return "No emergency with ID " + util::toUpper(id) + " exists."; }

}

Dispatcher::Dispatcher(EmergencyManager& emergencies, ResourceManager& resources)
    : emergencies_(emergencies), resources_(resources)
{
}

OpResult Dispatcher::assign(const std::string& emergencyId, const std::string& resourceId, std::time_t now)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return OpResult::failure(notFound(emergencyId));
    }
    if (!emergency->isActive()) {
        return OpResult::failure("Case " + emergency->id() + " is " + toString(emergency->status()) +
                                 "; resources can only be assigned to active cases.");
    }
    const std::string error = resources_.assign(resourceId, emergency->id(), now);
    if (!error.empty()) {
        return OpResult::failure(error);
    }
    if (emergency->status() == Status::Pending) {
        emergency->transitionTo(Status::Assigned, now);
    }
    return OpResult::success("Resource " + util::toUpper(util::trim(resourceId)) + " assigned to " +
                             emergency->id() + ". Case status: " + toString(emergency->status()) + ".");
}

OpResult Dispatcher::release(const std::string& emergencyId, const std::string& resourceId, std::time_t now,
                             bool force)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    const std::string eid = util::toUpper(util::trim(emergencyId));
    const std::string rid = util::toUpper(util::trim(resourceId));
    const auto assigned = resources_.assignedTo(eid);
    const bool isLast = assigned.size() == 1 && assigned.front()->id() == rid;

    if (!force && emergency != nullptr && emergency->status() == Status::InProgress && isLast) {
        return OpResult::failure("Resource " + rid + " is the last resource on In Progress case " + eid +
                                 ". Mark the case Resolved or Cancelled instead.");
    }
    const std::string error = resources_.release(rid, eid, now, "Released by operator");
    if (!error.empty()) {
        return OpResult::failure(error);
    }
    std::string message = "Resource " + rid + " released from " + eid + " and is available again.";
    if (emergency != nullptr && emergency->status() == Status::Assigned && resources_.assignedTo(eid).empty()) {
        emergency->transitionTo(Status::Pending, now);
        message += " No resources remain, so the case returned to the Pending queue.";
    }
    return OpResult::success(message);
}

OpResult Dispatcher::startOperation(const std::string& emergencyId, std::time_t now)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return OpResult::failure(notFound(emergencyId));
    }
    if (emergency->status() != Status::Assigned) {
        return OpResult::failure("Only Assigned cases can be marked In Progress (current status: " +
                                 toString(emergency->status()) + ").");
    }
    if (resources_.assignedTo(emergency->id()).empty()) {
        return OpResult::failure("Case " + emergency->id() + " has no assigned resources.");
    }
    emergency->transitionTo(Status::InProgress, now);
    return OpResult::success("Case " + emergency->id() + " is now In Progress.");
}

OpResult Dispatcher::resolve(const std::string& emergencyId, std::time_t now)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return OpResult::failure(notFound(emergencyId));
    }
    if (emergency->status() != Status::InProgress) {
        return OpResult::failure("A case must be In Progress before it can be resolved (current status: " +
                                 toString(emergency->status()) + ").");
    }
    const int released = resources_.releaseAll(emergency->id(), now, "Operation resolved");
    emergency->transitionTo(Status::Resolved, now);
    return OpResult::success("Case " + emergency->id() + " marked Resolved. " + std::to_string(released) +
                             " resource(s) released and available again.");
}

OpResult Dispatcher::cancel(const std::string& emergencyId, std::time_t now)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return OpResult::failure(notFound(emergencyId));
    }
    if (!emergency->isActive()) {
        return OpResult::failure("Case " + emergency->id() + " is already " + toString(emergency->status()) + ".");
    }
    const int released = resources_.releaseAll(emergency->id(), now, "Case cancelled");
    emergency->transitionTo(Status::Cancelled, now);
    return OpResult::success("Case " + emergency->id() + " cancelled. " + std::to_string(released) +
                             " resource(s) released.");
}

OpResult Dispatcher::returnToQueue(const std::string& emergencyId, std::time_t now)
{
    Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return OpResult::failure(notFound(emergencyId));
    }
    if (emergency->status() != Status::Assigned) {
        return OpResult::failure("Only Assigned cases can be returned to Pending (current status: " +
                                 toString(emergency->status()) + ").");
    }
    const int released = resources_.releaseAll(emergency->id(), now, "Case returned to queue");
    emergency->transitionTo(Status::Pending, now);
    return OpResult::success("Case " + emergency->id() + " returned to the Pending queue. " +
                             std::to_string(released) + " resource(s) released.");
}

OpResult Dispatcher::changeStatus(const std::string& emergencyId, Status target, std::time_t now)
{
    switch (target) {
    case Status::Pending: return returnToQueue(emergencyId, now);
    case Status::Assigned:
        return OpResult::failure("A case becomes Assigned when a resource is assigned to it.");
    case Status::InProgress: return startOperation(emergencyId, now);
    case Status::Resolved: return resolve(emergencyId, now);
    case Status::Cancelled: return cancel(emergencyId, now);
    }
    return OpResult::failure("Unknown status.");
}

std::vector<Status> Dispatcher::allowedNextStatuses(const std::string& emergencyId) const
{
    std::vector<Status> result;
    const Emergency* emergency = emergencies_.find(emergencyId);
    if (emergency == nullptr) {
        return result;
    }
    for (Status s : allStatuses()) {
        if (Emergency::isTransitionAllowed(emergency->status(), s)) {
            result.push_back(s);
        }
    }
    return result;
}

std::vector<std::string> Dispatcher::reconcile(std::time_t now)
{
    std::vector<std::string> notes;
    for (const Resource* resource : resources_.all()) {
        const std::vector<std::string> assigned = resource->assignedEmergencies();
        for (const std::string& eid : assigned) {
            const Emergency* emergency = emergencies_.find(eid);
            if (emergency == nullptr || !emergency->isActive()) {
                resources_.release(resource->id(), eid, now, "Released during data check");
                notes.push_back("Resource " + resource->id() + " was linked to " +
                                (emergency == nullptr ? "unknown case " : "closed case ") + eid +
                                "; the link was removed.");
            }
        }
    }
    for (const Emergency* e : emergencies_.withStatus(Status::Assigned)) {
        if (resources_.assignedTo(e->id()).empty()) {
            emergencies_.find(e->id())->transitionTo(Status::Pending, now);
            notes.push_back("Case " + e->id() + " was Assigned without resources and returned to Pending.");
        }
    }
    for (const Emergency* e : emergencies_.withStatus(Status::InProgress)) {
        if (resources_.assignedTo(e->id()).empty()) {
            notes.push_back("Case " + e->id() +
                            " is In Progress without assigned resources. Review it in Active Operations.");
        }
    }
    for (const std::string& note : resources_.reconcileHistory(now)) {
        notes.push_back(note);
    }
    emergencies_.recalculateAll(now);
    return notes;
}

}


#include <ctime>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace resq {

namespace {

const char* const kEmergencyHeader = "RESQ|EMERGENCIES|1";
const char* const kResourceHeader = "RESQ|RESOURCES|1";
const char* const kAssignmentHeader = "RESQ|ASSIGNMENTS|1";

constexpr size_t kEmergencyFields = 14;
constexpr size_t kResourceFields = 8;
constexpr size_t kAssignmentFields = 6;

std::uint32_t crcOfLine(std::uint32_t crc, const std::string& line)
{
    crc = resq_crc32(crc, line.data(), line.size());
    return resq_crc32(crc, "\n", 1);
}

bool parseTime(const std::string& text, std::time_t& out)
{
    long long value = 0;
    if (!util::parseInteger(text, value) || value < 0) {
        return false;
    }
    out = static_cast<std::time_t>(value);
    return true;
}

bool parseFlag(const std::string& text, bool& out)
{
    if (text == "0" || text == "1") {
        out = text == "1";
        return true;
    }
    return false;
}

std::string at(const std::string& label, int line) { return label + " line " + std::to_string(line) + ": "; }

std::string timestampForFileName()
{
    const std::time_t now = std::time(nullptr);
    char buffer[32];
    const std::tm* local = std::localtime(&now);
    if (local == nullptr || std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", local) == 0) {
        return std::to_string(static_cast<long long>(now));
    }
    return buffer;
}

}

FileManager::FileManager(std::string dataDirectory) : dataDirectory_(std::move(dataDirectory))
{
    if (dataDirectory_.empty()) {
        dataDirectory_ = ".";
    }
}

std::string FileManager::pathFor(const char* fileName) const
{
    const char last = dataDirectory_.back();
    return (last == '/' || last == '\\') ? dataDirectory_ + fileName : dataDirectory_ + "/" + fileName;
}

std::string FileManager::emergenciesPath() const { return pathFor("emergencies.txt"); }
std::string FileManager::resourcesPath() const { return pathFor("resources.txt"); }
std::string FileManager::assignmentsPath() const { return pathFor("assignments.txt"); }

FileManager::RawFile FileManager::readFile(const std::string& path, const std::string& header,
                                           const std::string& label, LoadReport& report) const
{
    RawFile raw;
    if (!resq_file_exists(path.c_str())) {
        return raw;
    }
    raw.found = true;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        report.warnings.push_back(label + ": file exists but cannot be opened; it was not loaded.");
        raw.usable = false;
        return raw;
    }

    std::string line;
    int lineNumber = 0;
    bool headerSeen = false;
    bool endSeen = false;
    std::uint32_t crc = 0;
    long long expectedCount = -1;
    std::string expectedCrc;

    while (std::getline(in, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (util::trim(line).empty()) {
            continue;
        }
        if (!headerSeen) {
            if (line != header) {
                report.warnings.push_back(label + ": unrecognised header \"" + line.substr(0, 40) +
                                          "\"; the file was not loaded.");
                raw.usable = false;
                return raw;
            }
            headerSeen = true;
            continue;
        }
        if (line.rfind("#END|", 0) == 0) {
            const std::vector<std::string> parts = util::splitEscaped(line);
            if (parts.size() == 3) {
                util::parseInteger(parts[1], expectedCount);
                expectedCrc = parts[2];
            }
            endSeen = true;
            continue;
        }
        if (line[0] == '#') {
            continue;
        }
        if (endSeen) {
            report.warnings.push_back(at(label, lineNumber) + "data found after the end marker.");
        }
        crc = crcOfLine(crc, line);
        raw.records.emplace_back(lineNumber, line);
    }

    if (!headerSeen) {
        return raw;
    }
    if (!endSeen) {
        report.warnings.push_back(label + ": end marker missing - the file may be truncated. Valid records "
                                          "were loaded.");
    } else {
        if (expectedCount != static_cast<long long>(raw.records.size())) {
            report.warnings.push_back(label + ": record count does not match the end marker.");
        }
        if (expectedCrc != util::hex32(crc)) {
            report.warnings.push_back(label + ": checksum mismatch - the file was edited or damaged. Each "
                                              "record was still validated individually.");
        }
    }
    return raw;
}

void FileManager::preserveDamaged(const std::string& path, LoadReport& report) const
{
    const std::string copy = path + "." + timestampForFileName() + ".damaged";
    if (resq_copy_file(path.c_str(), copy.c_str())) {
        report.preserved.push_back(copy);
    } else {
        report.warnings.push_back("Could not keep a copy of " + path + ": " + std::strerror(errno));
    }
}

LoadReport FileManager::loadAll(EmergencyManager& emergencies, ResourceManager& resources) const
{
    LoadReport report;
    emergencies.clear();
    resources.clear();

    {
        const size_t before = report.warnings.size();
        const std::string label = "emergencies.txt";
        RawFile raw = readFile(emergenciesPath(), kEmergencyHeader, label, report);
        report.emergenciesFileFound = raw.found;
        for (const auto& entry : raw.records) {
            const std::vector<std::string> f = util::splitEscaped(entry.second);
            if (f.size() != kEmergencyFields) {
                report.warnings.push_back(at(label, entry.first) + "expected " + std::to_string(kEmergencyFields) +
                                          " fields, found " + std::to_string(f.size()) + "; skipped.");
                continue;
            }
            Category category;
            Severity severity;
            Status status;
            long long affected = 0;
            std::time_t registered = 0, assigned = 0, started = 0, closed = 0;
            bool demo = false;
            std::string reporter, contact;
            if (!fromCode(f[2], category) || !fromCode(f[6], severity) || !fromCode(f[8], status) ||
                !util::parseInteger(f[7], affected) || affected > Emergency::kMaxAffected ||
                !parseTime(f[9], registered) || !parseTime(f[10], assigned) || !parseTime(f[11], started) ||
                !parseTime(f[12], closed) || !parseFlag(f[13], demo) || !util::reveal(f[1], reporter) ||
                !util::reveal(f[4], contact)) {
                report.warnings.push_back(at(label, entry.first) + "invalid field value; skipped.");
                continue;
            }
            try {
                Emergency e(f[0], reporter, category, f[3], contact, f[5], severity, static_cast<int>(affected),
                            registered, demo);
                const std::string repair = e.restoreState(status, assigned, started, closed);
                if (!repair.empty()) {
                    report.warnings.push_back(at(label, entry.first) + e.id() + ": " + repair + ".");
                }
                const std::string error = emergencies.add(std::move(e));
                if (!error.empty()) {
                    report.warnings.push_back(at(label, entry.first) + error + " Duplicate skipped.");
                    continue;
                }
                ++report.emergenciesLoaded;
            } catch (const std::exception& ex) {
                report.warnings.push_back(at(label, entry.first) + ex.what() + " Skipped.");
            }
        }
        if (raw.found && report.warnings.size() > before) {
            preserveDamaged(emergenciesPath(), report);
        }
    }

    {
        const size_t before = report.warnings.size();
        const std::string label = "resources.txt";
        RawFile raw = readFile(resourcesPath(), kResourceHeader, label, report);
        report.resourcesFileFound = raw.found;
        for (const auto& entry : raw.records) {
            const std::vector<std::string> f = util::splitEscaped(entry.second);
            if (f.size() != kResourceFields) {
                report.warnings.push_back(at(label, entry.first) + "expected " + std::to_string(kResourceFields) +
                                          " fields, found " + std::to_string(f.size()) + "; skipped.");
                continue;
            }
            ResourceType type;
            long long capacity = 0, maxConcurrent = 0;
            bool outOfService = false, demo = false;
            if (!fromCode(f[1], type) || !util::parseInteger(f[3], capacity) ||
                !util::parseInteger(f[4], maxConcurrent) || !parseFlag(f[5], outOfService) ||
                !parseFlag(f[7], demo) || capacity > Resource::kMaxCapacity ||
                maxConcurrent > Resource::kMaxConcurrent) {
                report.warnings.push_back(at(label, entry.first) + "invalid field value; skipped.");
                continue;
            }
            try {
                Resource r(f[0], type, f[2], static_cast<int>(capacity), static_cast<int>(maxConcurrent), demo);
                for (const std::string& eid : util::splitPlain(f[6], ',')) {
                    try {
                        r.assignTo(eid);
                    } catch (const std::exception& ex) {
                        report.warnings.push_back(at(label, entry.first) + ex.what() + " Link ignored.");
                    }
                }
                if (outOfService) {
                    try {
                        r.setOutOfService(true);
                    } catch (const std::exception&) {
                        report.warnings.push_back(at(label, entry.first) + r.id() +
                                                  " was both deployed and out of service; kept as deployed.");
                    }
                }
                const std::string error = resources.add(std::move(r));
                if (!error.empty()) {
                    report.warnings.push_back(at(label, entry.first) + error + " Duplicate skipped.");
                    continue;
                }
                ++report.resourcesLoaded;
            } catch (const std::exception& ex) {
                report.warnings.push_back(at(label, entry.first) + ex.what() + " Skipped.");
            }
        }
        if (raw.found && report.warnings.size() > before) {
            preserveDamaged(resourcesPath(), report);
        }
    }

    {
        const size_t before = report.warnings.size();
        const std::string label = "assignments.txt";
        RawFile raw = readFile(assignmentsPath(), kAssignmentHeader, label, report);
        report.assignmentsFileFound = raw.found;
        for (const auto& entry : raw.records) {
            const std::vector<std::string> f = util::splitEscaped(entry.second);
            if (f.size() != kAssignmentFields) {
                report.warnings.push_back(at(label, entry.first) + "expected " +
                                          std::to_string(kAssignmentFields) + " fields, found " +
                                          std::to_string(f.size()) + "; skipped.");
                continue;
            }
            AssignmentRecord record;
            long long id = 0;
            record.emergencyId = util::toUpper(f[1]);
            record.resourceId = util::toUpper(f[2]);
            record.note = f[5];
            if (!util::parseInteger(f[0], id) || id <= 0 || !util::isValidId(record.emergencyId) ||
                !util::isValidId(record.resourceId) || !parseTime(f[3], record.assignedAt) ||
                !parseTime(f[4], record.releasedAt) || record.assignedAt == 0 ||
                (record.releasedAt != 0 && record.releasedAt < record.assignedAt)) {
                report.warnings.push_back(at(label, entry.first) + "invalid field value; skipped.");
                continue;
            }
            record.id = static_cast<long>(id);
            if (!resources.restoreRecord(record)) {
                report.warnings.push_back(at(label, entry.first) + "duplicate record #" + f[0] + "; skipped.");
                continue;
            }
            ++report.assignmentsLoaded;
        }
        if (raw.found && report.warnings.size() > before) {
            preserveDamaged(assignmentsPath(), report);
        }
    }
    return report;
}

std::vector<std::string> FileManager::archiveAll(const std::string& tag, std::string& error) const
{
    std::vector<std::string> copies;
    const std::string stamp = timestampForFileName();
    for (const std::string& path : {emergenciesPath(), resourcesPath(), assignmentsPath()}) {
        if (!resq_file_exists(path.c_str())) {
            continue;
        }
        const std::string copy = path + "." + stamp + "." + tag;
        if (!resq_copy_file(path.c_str(), copy.c_str())) {
            error = "Could not archive " + path + ": " + std::strerror(errno);
            return copies;
        }
        copies.push_back(copy);
    }
    return copies;
}

bool FileManager::writeFile(const std::string& path, const std::string& header,
                            const std::vector<std::string>& comments, const std::vector<std::string>& records,
                            std::string& error) const
{
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "Cannot open " + temp + " for writing.";
            return false;
        }
        out << header << '\n';
        for (const std::string& comment : comments) {
            out << "# " << comment << '\n';
        }
        std::uint32_t crc = 0;
        for (const std::string& record : records) {
            out << record << '\n';
            crc = crcOfLine(crc, record);
        }
        out << "#END|" << records.size() << '|' << util::hex32(crc) << '\n';
        out.flush();
        if (!out) {
            error = "Writing " + temp + " failed (disk full or no permission).";
            out.close();
            resq_remove_file(temp.c_str());
            return false;
        }
    }
    const std::string backup = path + ".bak";
    if (resq_file_exists(path.c_str()) && !resq_copy_file(path.c_str(), backup.c_str())) {
        error = "Could not back up " + path + ": " + std::strerror(errno);
        resq_remove_file(temp.c_str());
        return false;
    }
    if (!resq_replace_file(temp.c_str(), path.c_str())) {
        error = "Could not replace " + path + ". The new data is in " + temp + " and the previous version in " +
                backup + ".";
        return false;
    }
    return true;
}

bool FileManager::saveAll(const EmergencyManager& emergencies, const ResourceManager& resources,
                          std::string& error) const
{
    if (!resq_make_dirs(dataDirectory_.c_str())) {
        error = "Cannot create data directory " + dataDirectory_ + " (check the path and permissions).";
        return false;
    }

    std::vector<std::string> lines;
    for (const Emergency* e : emergencies.all()) {
        const std::vector<std::string> fields{
            e->id(),
            util::obfuscate(e->reporter()),
            toCode(e->category()),
            e->location(),
            util::obfuscate(e->contact()),
            e->description(),
            toCode(e->severity()),
            std::to_string(e->affected()),
            toCode(e->status()),
            std::to_string(static_cast<long long>(e->registeredAt())),
            std::to_string(static_cast<long long>(e->assignedAt())),
            std::to_string(static_cast<long long>(e->startedAt())),
            std::to_string(static_cast<long long>(e->closedAt())),
            e->isDemo() ? "1" : "0",
        };
        std::string line;
        for (size_t i = 0; i < fields.size(); ++i) {
            line += (i == 0 ? "" : "|") + util::escapeField(fields[i]);
        }
        lines.push_back(line);
    }
    if (!writeFile(emergenciesPath(), kEmergencyHeader,
                   {"RESQ emergency records. Fields separated by '|' (escaped as \\p inside values):",
                    "id|reporter*|category|location|contact*|description|severity|affected|status|",
                    "registered|assigned|started|closed|demo   (* = obfuscated, times = Unix seconds)"},
                   lines, error)) {
        return false;
    }

    lines.clear();
    for (const Resource* r : resources.all()) {
        std::string assigned;
        for (const std::string& eid : r->assignedEmergencies()) {
            assigned += (assigned.empty() ? "" : ",") + eid;
        }
        const std::vector<std::string> fields{
            r->id(),
            toCode(r->type()),
            r->location(),
            std::to_string(r->capacity()),
            std::to_string(r->maxConcurrent()),
            r->isOutOfService() ? "1" : "0",
            assigned,
            r->isDemo() ? "1" : "0",
        };
        std::string line;
        for (size_t i = 0; i < fields.size(); ++i) {
            line += (i == 0 ? "" : "|") + util::escapeField(fields[i]);
        }
        lines.push_back(line);
    }
    if (!writeFile(resourcesPath(), kResourceHeader,
                   {"RESQ resources. Fields:",
                    "id|type|location|capacity|max_simultaneous|out_of_service|assigned_ids(comma)|demo"},
                   lines, error)) {
        return false;
    }

    lines.clear();
    for (const AssignmentRecord& record : resources.history()) {
        lines.push_back(std::to_string(record.id) + "|" + util::escapeField(record.emergencyId) + "|" +
                        util::escapeField(record.resourceId) + "|" +
                        std::to_string(static_cast<long long>(record.assignedAt)) + "|" +
                        std::to_string(static_cast<long long>(record.releasedAt)) + "|" +
                        util::escapeField(record.note));
    }
    return writeFile(assignmentsPath(), kAssignmentHeader,
                     {"RESQ assignment history. Fields:",
                      "record_id|emergency_id|resource_id|assigned_at|released_at(0 = active)|note"},
                     lines, error);
}

}

namespace resq {

double Statistics::utilizationPercent() const
{
    const int inService = totalResources - outOfServiceResources;
    if (inService <= 0) {
        return -1.0;
    }
    return 100.0 * static_cast<double>(deployedResources) / static_cast<double>(inService);
}

AnalyticsManager::AnalyticsManager(const EmergencyManager& emergencies, const ResourceManager& resources)
    : emergencies_(emergencies), resources_(resources)
{
}

Statistics AnalyticsManager::compute() const
{
    Statistics s;
    double responseTotal = 0.0;
    double resolutionTotal = 0.0;

    for (const Emergency* e : emergencies_.all()) {
        ++s.totalEmergencies;
        ++s.byStatus[static_cast<size_t>(indexOf(e->status()))];
        ++s.bySeverity[static_cast<size_t>(indexOf(e->severity()))];
        ++s.byCategory[static_cast<size_t>(indexOf(e->category()))];
        if (e->isActive()) {
            ++s.activeCases;
            s.peopleAffectedActive += e->affected();
            if (e->severity() == Severity::Critical) {
                ++s.activeCritical;
            }
        }
        if (e->assignedAt() > 0 && e->assignedAt() >= e->registeredAt()) {
            responseTotal += static_cast<double>(e->assignedAt() - e->registeredAt());
            ++s.responseSamples;
        }
        if (e->status() == Status::Resolved && e->closedAt() > e->registeredAt()) {
            resolutionTotal += static_cast<double>(e->closedAt() - e->registeredAt());
            ++s.resolutionSamples;
        }
    }
    if (s.responseSamples > 0) {
        s.averageResponseSeconds = responseTotal / s.responseSamples;
    }
    if (s.resolutionSamples > 0) {
        s.averageResolutionSeconds = resolutionTotal / s.resolutionSamples;
    }

    for (const Resource* r : resources_.all()) {
        ++s.totalResources;
        TypeUsage& usage = s.byType[static_cast<size_t>(indexOf(r->type()))];
        ++usage.total;
        switch (r->status()) {
        case ResourceStatus::Available:
            ++s.availableResources;
            ++usage.available;
            break;
        case ResourceStatus::Deployed:
            ++s.deployedResources;
            ++usage.deployed;
            break;
        case ResourceStatus::OutOfService:
            ++s.outOfServiceResources;
            ++usage.outOfService;
            break;
        }
        s.activeAssignments += r->activeAssignments();
    }
    s.totalAssignmentsLogged = static_cast<int>(resources_.history().size());
    return s;
}

std::vector<std::pair<std::string, int>> AnalyticsManager::mostDeployed(size_t limit) const
{
    std::vector<std::pair<std::string, int>> counts = resources_.deploymentCounts();
    if (counts.size() > limit) {
        counts.resize(limit);
    }
    return counts;
}

}

#include <initializer_list>
#include <set>
#include <string>

namespace resq {

namespace {

constexpr std::time_t kMinute = 60;
constexpr std::time_t kHour = 60 * kMinute;

struct DemoCase {
    const char* id;
    const char* reporter;
    Category category;
    const char* location;
    const char* contact;
    const char* description;
    Severity severity;
    int affected;
    std::time_t minutesAgo;
};

struct DemoResource {
    const char* id;
    ResourceType type;
    const char* location;
    int capacity;
    int maxConcurrent;
    bool outOfService;
};

const DemoCase kCases[] = {
    {"E-D01", "Riya Sharma (demo)", Category::Fire, "Old Market Square", "000-555-0101",
     "[FICTIONAL DEMO] Fire in a two-storey shop building; smoke on the upper floor.", Severity::Critical, 12, 25},
    {"E-D02", "Arjun Mehta (demo)", Category::Medical, "Riverside Ward 3", "000-555-0102",
     "[FICTIONAL DEMO] Elderly resident with chest pain, conscious and breathing.", Severity::High, 1, 40},
    {"E-D03", "Kavya Rao (demo)", Category::RoadAccident, "Hill Road Km 12", "000-555-0103",
     "[FICTIONAL DEMO] Two-car collision, four occupants, one trapped.", Severity::High, 4, 15},
    {"E-D04", "Imran Khan (demo)", Category::Flood, "Lakeview Colony", "000-555-0104",
     "[FICTIONAL DEMO] Water entering ground-floor homes after heavy rain.", Severity::Medium, 30, 90},
    {"E-D05", "Neha Joshi (demo)", Category::Other, "Central Park Gate 2", "000-555-0105",
     "[FICTIONAL DEMO] Fallen tree blocking the footpath, no injuries.", Severity::Low, 0, 10},
    {"E-D06", "Vikram Singh (demo)", Category::NaturalDisaster, "Northridge Village", "000-555-0106",
     "[FICTIONAL DEMO] Landslide damaged several houses; families displaced.", Severity::Critical, 60, 180},
    {"E-D07", "Ananya Gupta (demo)", Category::Medical, "Sector 9 Housing", "000-555-0107",
     "[FICTIONAL DEMO] Child with high fever and breathing difficulty.", Severity::Medium, 1, 300},
    {"E-D08", "Rahul Verma (demo)", Category::Fire, "Industrial Estate Plot 4", "000-555-0108",
     "[FICTIONAL DEMO] Small warehouse fire, contained to one unit.", Severity::High, 3, 26 * 60},
    {"E-D09", "Sara Thomas (demo)", Category::Rescue, "Quarry Lake", "000-555-0109",
     "[FICTIONAL DEMO] Two hikers stranded on a ledge above the lake.", Severity::Medium, 2, 60},
    {"E-D10", "Dev Patel (demo)", Category::RoadAccident, "Ring Road Junction 5", "000-555-0110",
     "[FICTIONAL DEMO] Minor scooter skid; duplicate of another report.", Severity::Low, 2, 8 * 60},
};

const DemoResource kResources[] = {
    {"R-D01", ResourceType::Ambulance, "Riverside", 2, 1, false},
    {"R-D02", ResourceType::Ambulance, "Old Market", 2, 1, false},
    {"R-D03", ResourceType::Ambulance, "Hill Road Depot", 2, 1, true},
    {"R-D04", ResourceType::MedicalTeam, "District Hospital", 10, 1, false},
    {"R-D05", ResourceType::FireEngine, "Old Market Fire Stn", 6, 1, false},
    {"R-D06", ResourceType::FireEngine, "Industrial Estate", 6, 1, false},
    {"R-D07", ResourceType::RescueTeam, "Northridge", 8, 1, false},
    {"R-D08", ResourceType::RescueTeam, "Central Depot", 8, 1, false},
    {"R-D09", ResourceType::ReliefVehicle, "Central Depot", 20, 1, false},
    {"R-D10", ResourceType::ReliefVehicle, "Lakeview", 20, 1, false},
    {"R-D11", ResourceType::SupplyKit, "Central Store", 25, 3, false},
    {"R-D12", ResourceType::MedicalTeam, "Riverside Clinic", 6, 1, false},
};

}

DemoResult DemoData::load(EmergencyManager& emergencies, ResourceManager& resources, Dispatcher& dispatcher,
                          std::time_t now)
{
    DemoResult result;
    std::set<std::string> addedCases;
    std::set<std::string> addedResources;

    for (const DemoResource& d : kResources) {
        if (resources.exists(d.id)) {
            ++result.skipped;
            continue;
        }
        Resource r(d.id, d.type, d.location, d.capacity, d.maxConcurrent, true);
        if (d.outOfService) {
            r.setOutOfService(true);
        }
        if (resources.add(std::move(r)).empty()) {
            ++result.resourcesAdded;
            addedResources.insert(d.id);
        }
    }
    for (const DemoCase& d : kCases) {
        if (emergencies.exists(d.id)) {
            ++result.skipped;
            continue;
        }
        Emergency e(d.id, d.reporter, d.category, d.location, d.contact, d.description, d.severity, d.affected,
                    now - d.minutesAgo * kMinute, true);
        if (emergencies.add(std::move(e)).empty()) {
            ++result.emergenciesAdded;
            addedCases.insert(d.id);
        }
    }

    const auto ready = [&](const char* eid, std::initializer_list<const char*> rids) {
        if (addedCases.count(eid) == 0) {
            return false;
        }
        for (const char* rid : rids) {
            if (addedResources.count(rid) == 0) {
                return false;
            }
        }
        return true;
    };
    if (ready("E-D06", {"R-D07", "R-D09"})) {
        dispatcher.assign("E-D06", "R-D07", now - 170 * kMinute);
        dispatcher.assign("E-D06", "R-D09", now - 165 * kMinute);
        dispatcher.startOperation("E-D06", now - 150 * kMinute);
    }
    if (ready("E-D07", {"R-D01"})) {
        dispatcher.assign("E-D07", "R-D01", now - 290 * kMinute);
        dispatcher.startOperation("E-D07", now - 280 * kMinute);
        dispatcher.resolve("E-D07", now - 4 * kHour);
    }
    if (ready("E-D08", {"R-D06", "R-D02"})) {
        dispatcher.assign("E-D08", "R-D06", now - (26 * kHour - 6 * kMinute));
        dispatcher.assign("E-D08", "R-D02", now - (26 * kHour - 8 * kMinute));
        dispatcher.startOperation("E-D08", now - (25 * kHour + 45 * kMinute));
        dispatcher.resolve("E-D08", now - 23 * kHour);
    }
    if (ready("E-D09", {"R-D08"})) {
        dispatcher.assign("E-D09", "R-D08", now - 50 * kMinute);
    }
    if (ready("E-D10", {})) {
        dispatcher.cancel("E-D10", now - (8 * kHour - 10 * kMinute));
    }
    emergencies.recalculateAll(now);
    return result;
}

int DemoData::removeAll(EmergencyManager& emergencies, ResourceManager& resources, Dispatcher& dispatcher,
                        std::time_t now)
{
    const std::vector<std::string> caseIds = emergencies.demoIds();
    const std::vector<std::string> resourceIds = resources.demoIds();
    const std::set<std::string> demoCases(caseIds.begin(), caseIds.end());

    for (const std::string& rid : resourceIds) {
        const std::vector<std::string> assigned = resources.find(rid)->assignedEmergencies();
        for (const std::string& eid : assigned) {
            if (demoCases.count(eid) == 0) {
                dispatcher.release(eid, rid, now, true);
            }
        }
    }
    for (const std::string& eid : caseIds) {
        resources.releaseAll(eid, now, "Demo data removed");
    }

    std::set<std::string> removed;
    for (const std::string& eid : caseIds) {
        if (emergencies.remove(eid)) {
            removed.insert(eid);
        }
    }
    for (const std::string& rid : resourceIds) {
        if (resources.remove(rid)) {
            removed.insert(rid);
        }
    }
    resources.purgeHistory(removed);
    return static_cast<int>(removed.size());
}

bool DemoData::present(const EmergencyManager& emergencies, const ResourceManager& resources)
{
    return !emergencies.demoIds().empty() || !resources.demoIds().empty();
}

}


#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <istream>
#include <ostream>
#include <thread>

namespace resq {

namespace {

const Glyphs kUnicodeGlyphs{"┌", "┐", "└", "┘", "─", "│", "╔", "╗", "╚", "╝", "═", "║", "█", "░", "…", "›", "•"};
const Glyphs kAsciiGlyphs{"+", "+", "+", "+", "-", "|", "+", "+", "+", "+", "=", "|", "#", " ", "~", ">", "*"};

const char* ansiCode(Style style)
{
    switch (style) {
    case Style::Plain: return "";
    case Style::Bold: return "\033[1m";
    case Style::Heading: return "\033[1;96m";
    case Style::Accent: return "\033[36m";
    case Style::Critical: return "\033[1;91m";
    case Style::Danger: return "\033[91m";
    case Style::High: return "\033[1;93m";
    case Style::Warning: return "\033[33m";
    case Style::Success: return "\033[92m";
    case Style::Info: return "\033[94m";
    case Style::Progress: return "\033[95m";
    case Style::Muted: return "\033[90m";
    case Style::Highlight: return "\033[1;97m";
    case Style::Badge: return "\033[1;97;41m";
    }
    return "";
}

const char* const kLogoR[5] = {"#### ", "#   #", "#### ", "#  # ", "#   #"};
const char* const kLogoE[5] = {"#####", "#    ", "#### ", "#    ", "#####"};
const char* const kLogoS[5] = {" ####", "#    ", " ### ", "    #", "#### "};
const char* const kLogoQ[5] = {" ### ", "#   #", "#   #", "#  # ", " ## #"};

size_t utf8SequenceLength(unsigned char c)
{
    if (c < 0x80) {
        return 1;
    }
    if ((c >> 5) == 0x6) {
        return 2;
    }
    if ((c >> 4) == 0xE) {
        return 3;
    }
    if ((c >> 3) == 0x1E) {
        return 4;
    }
    return 1;
}

}

Console::Console(std::istream& in, std::ostream& out, bool ansi, bool color, bool unicode, bool animations,
                 bool interactiveInput, bool interactiveOutput)
    : in_(in),
      out_(out),
      ansi_(ansi),
      color_(color),
      unicode_(unicode),
      animations_(animations),
      interactiveInput_(interactiveInput),
      interactiveOutput_(interactiveOutput)
{
}

const Glyphs& Console::glyphs() const { return unicode_ ? kUnicodeGlyphs : kAsciiGlyphs; }

std::string Console::paint(const std::string& text, Style style) const
{
    if (!color() || style == Style::Plain || text.empty()) {
        return text;
    }
    return std::string(ansiCode(style)) + text + "\033[0m";
}

int Console::visibleWidth(const std::string& text)
{
    int width = 0;
    size_t i = 0;
    while (i < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == 0x1B) {
            while (i < text.size() && text[i] != 'm') {
                ++i;
            }
            ++i;
            continue;
        }
        i += utf8SequenceLength(c);
        ++width;
    }
    return width;
}

std::string Console::fit(const std::string& text, int width, bool alignRight) const
{
    if (width <= 0) {
        return "";
    }
    std::string result = text;
    int visible = visibleWidth(text);
    if (visible > width) {
        result.clear();
        int count = 0;
        bool styled = false;
        size_t i = 0;
        while (i < text.size()) {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            if (c == 0x1B) {
                size_t j = i;
                while (j < text.size() && text[j] != 'm') {
                    ++j;
                }
                result.append(text, i, j - i + 1);
                i = j + 1;
                styled = true;
                continue;
            }
            if (count >= width - 1) {
                break;
            }
            const size_t length = std::min(utf8SequenceLength(c), text.size() - i);
            result.append(text, i, length);
            i += length;
            ++count;
        }
        result += glyphs().ellipsis;
        if (styled) {
            result += "\033[0m";
        }
        visible = width;
    }
    const std::string padding(static_cast<size_t>(width - visible), ' ');
    return alignRight ? padding + result : result + padding;
}

std::string Console::repeat(const std::string& unit, int count)
{
    std::string result;
    for (int i = 0; i < count; ++i) {
        result += unit;
    }
    return result;
}

void Console::clearScreen() const
{
    if (ansi_ && interactiveOutput_) {
        out_ << "\033[2J\033[H";
    } else {
        out_ << '\n' << repeat(glyphs().doubleHorizontal, kWidth) << '\n';
    }
}

void Console::printLogo() const
{
    const Glyphs& g = glyphs();
    const int inner = kWidth - 2;
    const auto center = [&](const std::string& text, Style style) {
        const int width = visibleWidth(text);
        const int left = std::max(0, (inner - width) / 2);
        const int right = std::max(0, inner - width - left);
        out_ << paint(g.doubleVertical, Style::Danger) << std::string(static_cast<size_t>(left), ' ')
             << paint(text, style) << std::string(static_cast<size_t>(right), ' ')
             << paint(g.doubleVertical, Style::Danger) << '\n';
    };

    out_ << paint(std::string(g.doubleTopLeft) + repeat(g.doubleHorizontal, inner) + g.doubleTopRight,
                  Style::Danger)
         << '\n';
    center("", Style::Plain);
    const char* const* letters[4] = {kLogoR, kLogoE, kLogoS, kLogoQ};
    for (int row = 0; row < 5; ++row) {
        std::string line;
        for (int letter = 0; letter < 4; ++letter) {
            if (letter > 0) {
                line += "  ";
            }
            for (const char* p = letters[letter][row]; *p != '\0'; ++p) {
                line += (*p == '#') ? std::string(g.block) + g.block : std::string("  ");
            }
        }
        center(line, Style::Critical);
    }
    center("", Style::Plain);
    center("RESCUE AND EMERGENCY SUPPORT SYSTEM", Style::Heading);
    center("Protecting Lives Through Smart Coordination", Style::Highlight);
    center("Academic prototype - not connected to any real emergency service", Style::Muted);
    out_ << paint(std::string(g.doubleBottomLeft) + repeat(g.doubleHorizontal, inner) + g.doubleBottomRight,
                  Style::Danger)
         << '\n';
}

void Console::printHeader(const std::string& title) const
{
    clearScreen();
    const Glyphs& g = glyphs();
    const std::string left = paint(" RESQ ", Style::Badge) + " " + paint("Rescue and Emergency Support System", Style::Heading);
    const std::string right = paint(util::formatDateTime(std::time(nullptr)), Style::Muted);
    const int space = std::max(1, kWidth - visibleWidth(left) - visibleWidth(right));
    out_ << left << std::string(static_cast<size_t>(space), ' ') << right << '\n';
    out_ << paint(repeat(g.doubleHorizontal, kWidth), Style::Danger) << '\n';
    const std::string crumbs = breadcrumb();
    if (!crumbs.empty()) {
        out_ << ' ' << paint(crumbs, Style::Muted) << '\n';
    }
    out_ << '\n' << ' ' << paint(util::toUpper(title), Style::Heading) << '\n';
    out_ << ' ' << paint(repeat(g.horizontal, visibleWidth(title)), Style::Accent) << "\n\n";
}

void Console::printSection(const std::string& title) const
{
    const Glyphs& g = glyphs();
    const std::string label = " " + util::toUpper(title) + " ";
    const int remaining = std::max(2, kWidth - 4 - visibleWidth(label));
    out_ << '\n'
         << ' ' << paint(repeat(g.horizontal, 2), Style::Accent) << paint(label, Style::Heading)
         << paint(repeat(g.horizontal, remaining), Style::Accent) << '\n';
}

void Console::printRule() const { out_ << ' ' << paint(repeat(glyphs().horizontal, kWidth - 2), Style::Muted) << '\n'; }

std::vector<std::string> Console::renderBox(const std::string& title, const std::vector<std::string>& lines,
                                            int width, Style borderStyle) const
{
    const Glyphs& g = glyphs();
    std::vector<std::string> result;
    const int inner = width - 4;
    if (title.empty()) {
        result.push_back(paint(std::string(g.topLeft) + repeat(g.horizontal, width - 2) + g.topRight, borderStyle));
    } else {
        const std::string label = fit(" " + title + " ", std::min(width - 4, visibleWidth(title) + 2));
        const int rest = width - 3 - visibleWidth(label);
        result.push_back(paint(std::string(g.topLeft) + g.horizontal, borderStyle) + paint(label, Style::Highlight) +
                         paint(repeat(g.horizontal, rest) + g.topRight, borderStyle));
    }
    for (const std::string& line : lines) {
        result.push_back(paint(g.vertical, borderStyle) + " " + fit(line, inner) + " " +
                         paint(g.vertical, borderStyle));
    }
    result.push_back(paint(std::string(g.bottomLeft) + repeat(g.horizontal, width - 2) + g.bottomRight, borderStyle));
    return result;
}

void Console::printBox(const std::string& title, const std::vector<std::string>& lines, Style borderStyle) const
{
    for (const std::string& line : renderBox(title, lines, kWidth, borderStyle)) {
        out_ << line << '\n';
    }
}

void Console::printSideBySide(const std::vector<std::string>& left, const std::vector<std::string>& right) const
{
    int leftWidth = 0;
    for (const std::string& line : left) {
        leftWidth = std::max(leftWidth, visibleWidth(line));
    }
    const size_t rows = std::max(left.size(), right.size());
    for (size_t i = 0; i < rows; ++i) {
        const std::string l = i < left.size() ? left[i] : "";
        const std::string r = i < right.size() ? right[i] : "";
        out_ << fit(l, leftWidth) << "  " << r << '\n';
    }
}

void Console::printBar(const std::string& label, int value, int maxValue, Style style, int labelWidth,
                       int barWidth) const
{
    const Glyphs& g = glyphs();
    int filled = 0;
    if (maxValue > 0 && value > 0) {
        filled = static_cast<int>(std::lround(static_cast<double>(value) * barWidth / maxValue));
        filled = std::max(1, std::min(filled, barWidth));
    }
    out_ << "  " << fit(label, labelWidth) << ' ' << paint(g.vertical, Style::Muted) << ' '
         << paint(repeat(g.block, filled), style) << paint(repeat(g.shade, barWidth - filled), Style::Muted) << ' '
         << paint(std::to_string(value), Style::Highlight) << '\n';
}

void Console::printMenu(const std::vector<std::pair<std::string, std::string>>& items) const
{
    const size_t half = (items.size() + 1) / 2;
    const auto render = [&](const std::pair<std::string, std::string>& item) {
        return paint("[" + item.first + "]", Style::Heading) + " " + item.second;
    };
    for (size_t i = 0; i < half; ++i) {
        std::string line = "  " + fit(render(items[i]), 36);
        if (i + half < items.size()) {
            line += "  " + render(items[i + half]);
        }
        out_ << line << '\n';
    }
}

void Console::message(const std::string& tag, Style style, const std::string& text) const
{
    out_ << "  " << paint(tag, style) << ' ' << text << '\n';
}

void Console::success(const std::string& text) const { message("[ OK ]", Style::Success, text); }
void Console::warning(const std::string& text) const { message("[WARN]", Style::Warning, text); }
void Console::error(const std::string& text) const { message("[FAIL]", Style::Danger, text); }
void Console::info(const std::string& text) const { message("[INFO]", Style::Info, text); }

void Console::spinner(const std::string& label, int milliseconds) const
{
    if (!animations_ || !ansi_ || !interactiveOutput_) {
        return;
    }
    static const char* const unicodeFrames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    static const char* const asciiFrames[] = {"|", "/", "-", "\\"};
    const char* const* frames = unicode_ ? unicodeFrames : asciiFrames;
    const int frameCount = unicode_ ? 10 : 4;
    const int steps = std::max(1, milliseconds / 70);
    for (int i = 0; i < steps; ++i) {
        out_ << "\r  " << paint(frames[i % frameCount], Style::Accent) << ' ' << label << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
    }
    out_ << "\r\033[2K" << std::flush;
}

std::string Console::prompt(const std::string& text) const
{
    return "  " + paint(text, Style::Bold) + paint(std::string(" ") + glyphs().separator + " ", Style::Accent);
}

std::string Console::readLine(const std::string& text) const
{
    out_ << prompt(text) << std::flush;
    std::string line;
    if (!std::getline(in_, line)) {
        out_ << '\n';
        throw InputClosed();
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (!interactiveInput_) {
        out_ << line << '\n';
    }
    return line;
}

std::string Console::readChoice(const std::string& text) const
{
    std::string choice = util::toUpper(util::trim(readLine(text)));
    if (choice == ":Q") {
        choice = "0";
    }
    return choice;
}

std::string Console::readText(const std::string& text, size_t maxLength, bool required) const
{
    while (true) {
        const std::string value = util::trim(readLine(text));
        if (value == ":q" || value == ":Q") {
            throw InputCancelled();
        }
        if (value.empty()) {
            if (!required) {
                return value;
            }
            error("This field is required (type :q to cancel).");
            continue;
        }
        if (util::utf8Length(value) > maxLength) {
            error("Too long: at most " + std::to_string(maxLength) + " characters are allowed.");
            continue;
        }
        if (!util::isPrintableText(value)) {
            error("Control characters are not allowed.");
            continue;
        }
        return value;
    }
}

long long Console::readNumber(const std::string& text, long long minimum, long long maximum) const
{
    while (true) {
        const std::string value = util::trim(readLine(text));
        if (value == ":q" || value == ":Q") {
            throw InputCancelled();
        }
        long long number = 0;
        if (!util::parseInteger(value, number) || number < minimum || number > maximum) {
            error("Enter a whole number from " + std::to_string(minimum) + " to " + std::to_string(maximum) +
                  " (or :q to cancel).");
            continue;
        }
        return number;
    }
}

long long Console::readNumberOr(const std::string& text, long long minimum, long long maximum,
                                long long fallback) const
{
    while (true) {
        const std::string value = util::trim(readLine(text + " [" + std::to_string(fallback) + "]"));
        if (value.empty()) {
            return fallback;
        }
        if (value == ":q" || value == ":Q") {
            throw InputCancelled();
        }
        long long number = 0;
        if (!util::parseInteger(value, number) || number < minimum || number > maximum) {
            error("Enter a whole number from " + std::to_string(minimum) + " to " + std::to_string(maximum) +
                  " (Enter keeps " + std::to_string(fallback) + ").");
            continue;
        }
        return number;
    }
}

bool Console::confirm(const std::string& text, bool defaultYes) const
{
    while (true) {
        const std::string value = util::toLower(util::trim(readLine(text + (defaultYes ? " (Y/n)" : " (y/N)"))));
        if (value.empty()) {
            return defaultYes;
        }
        if (value == "y" || value == "yes") {
            return true;
        }
        if (value == "n" || value == "no" || value == ":q") {
            return false;
        }
        error("Please answer y or n.");
    }
}

bool Console::confirmWord(const std::string& text, const std::string& word) const
{
    const std::string value = util::trim(readLine(text + " - type " + word + " to confirm"));
    return value == word;
}

void Console::pause() const
{
    if (!interactiveInput_) {
        return;
    }
    out_ << '\n';
    readLine(paint("Press Enter to continue", Style::Muted));
}

std::string Console::breadcrumb() const
{
    std::string result;
    for (size_t i = 0; i < screens_.size(); ++i) {
        if (i > 0) {
            result += std::string(" ") + glyphs().separator + " ";
        }
        result += screens_[i];
    }
    return result;
}

Table::Table(std::vector<Column> columns, int gap) : columns_(std::move(columns)), gap_(gap) {}

void Table::addRow(std::vector<Cell> cells)
{
    cells.resize(columns_.size());
    rows_.push_back(std::move(cells));
}

void Table::print(const Console& console) const
{
    std::ostream& out = console.out();
    const std::string gap(static_cast<size_t>(gap_), ' ');
    std::string header = " ";
    std::string rule = " ";
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (i > 0) {
            header += gap;
            rule += gap;
        }
        header += console.paint(console.fit(columns_[i].title, columns_[i].width, columns_[i].alignRight),
                                Style::Heading);
        rule += Console::repeat(console.glyphs().horizontal, columns_[i].width);
    }
    out << header << '\n' << console.paint(rule, Style::Muted) << '\n';
    if (rows_.empty()) {
        out << "  " << console.paint("(no records)", Style::Muted) << '\n';
        return;
    }
    for (const auto& row : rows_) {
        std::string line = " ";
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) {
                line += gap;
            }
            line += console.paint(console.fit(row[i].text, columns_[i].width, columns_[i].alignRight), row[i].style);
        }
        out << line << '\n';
    }
}

}


#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

namespace resq {

namespace {

using Row = std::pair<std::string, std::pair<std::string, Style>>;

Style severityStyle(Severity severity)
{
    switch (severity) {
    case Severity::Critical: return Style::Critical;
    case Severity::High: return Style::High;
    case Severity::Medium: return Style::Warning;
    case Severity::Low: return Style::Plain;
    }
    return Style::Plain;
}

Style statusStyle(Status status)
{
    switch (status) {
    case Status::Pending: return Style::Warning;
    case Status::Assigned: return Style::Accent;
    case Status::InProgress: return Style::Progress;
    case Status::Resolved: return Style::Success;
    case Status::Cancelled: return Style::Muted;
    }
    return Style::Plain;
}

Style resourceStyle(ResourceStatus status)
{
    switch (status) {
    case ResourceStatus::Available: return Style::Success;
    case ResourceStatus::Deployed: return Style::Warning;
    case ResourceStatus::OutOfService: return Style::Danger;
    }
    return Style::Plain;
}

int defaultCapacity(ResourceType type)
{
    switch (type) {
    case ResourceType::Ambulance: return 2;
    case ResourceType::MedicalTeam: return 10;
    case ResourceType::FireEngine: return 6;
    case ResourceType::RescueTeam: return 8;
    case ResourceType::ReliefVehicle: return 20;
    case ResourceType::SupplyKit: return 25;
    }
    return 0;
}

std::string statusHint(Status status)
{
    switch (status) {
    case Status::Pending: return "Return to the queue (releases all resources)";
    case Status::Assigned: return "Assign a resource now";
    case Status::InProgress: return "Crews have started work on scene";
    case Status::Resolved: return "Operation complete (releases all resources)";
    case Status::Cancelled: return "False alarm / duplicate / not needed (releases resources)";
    }
    return "";
}

std::string percent(int part, int whole)
{
    if (whole <= 0) {
        return "0%";
    }
    return std::to_string(static_cast<int>(std::lround(100.0 * part / whole))) + "%";
}

std::string resourceList(const std::vector<const Resource*>& list)
{
    std::string text;
    for (const Resource* r : list) {
        text += (text.empty() ? "" : ", ") + r->id() + " (" + shortName(r->type()) + ")";
    }
    return text.empty() ? "none" : text;
}

}

bool AppOptions::parse(int argc, char* argv[], AppOptions& options, std::string& error)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--data-dir") {
            if (i + 1 >= argc) {
                error = "--data-dir needs a directory name.";
                return false;
            }
            options.dataDirectory = argv[++i];
        } else if (arg.rfind("--data-dir=", 0) == 0) {
            options.dataDirectory = arg.substr(11);
        } else if (arg == "--no-color") {
            options.color = 0;
        } else if (arg == "--color") {
            options.color = 1;
        } else if (arg == "--ascii") {
            options.unicode = 0;
        } else if (arg == "--unicode") {
            options.unicode = 1;
        } else if (arg == "--no-anim") {
            options.animations = false;
        } else if (arg == "-h" || arg == "--help") {
            options.showHelp = true;
        } else if (arg == "-v" || arg == "--version") {
            options.showVersion = true;
        } else {
            error = "Unknown option: " + arg;
            return false;
        }
    }
    if (util::trim(options.dataDirectory).empty()) {
        error = "The data directory name cannot be empty.";
        return false;
    }
    return true;
}

std::string AppOptions::usage()
{
    return "RESQ - Rescue and Emergency Support System (academic prototype)\n"
           "\n"
           "Usage: resq [options]\n"
           "\n"
           "  --data-dir DIR   Folder for emergencies.txt, resources.txt and\n"
           "                   assignments.txt (default: ./data)\n"
           "  --no-color       Disable ANSI colors (also honours NO_COLOR)\n"
           "  --color          Force colors on\n"
           "  --ascii          Use plain ASCII borders instead of box drawing\n"
           "  --unicode        Force Unicode box drawing\n"
           "  --no-anim        Disable loading animations\n"
           "  -v, --version    Show the version\n"
           "  -h, --help       Show this help\n";
}

RESQApplication::RESQApplication(const AppOptions& options)
    : options_(options),
      console_(std::cin, std::cout, resq_term_init() == 1 || options.color == 1,
               options.color != 0, options.unicode == -1 ? resq_term_enable_utf8() == 1 : options.unicode == 1,
               options.animations, resq_stdin_is_tty() == 1, resq_stdout_is_tty() == 1),
      files_(options.dataDirectory),
      dispatcher_(emergencies_, resources_),
      analytics_(emergencies_, resources_)
{
    if (options.unicode == 1) {
        resq_term_enable_utf8();
    }
    console_.pushScreen("Home");
}

std::time_t RESQApplication::now() const { return std::time(nullptr); }

int RESQApplication::run()
{
    try {
        startup();
        mainLoop();
    } catch (const InputClosed&) {
        console_.info("Input closed - saving and exiting.");
    }
    shutdown();
    return exitCode_;
}

void RESQApplication::startup()
{
    console_.clearScreen();
    console_.printLogo();
    console_.out() << '\n';
    console_.spinner("Loading records from " + files_.dataDirectory() + " ...", 700);

    loadReport_ = files_.loadAll(emergencies_, resources_);
    reconcileNotes_ = dispatcher_.reconcile(now());

    if (loadReport_.anyFileFound()) {
        console_.success("Loaded " + std::to_string(loadReport_.emergenciesLoaded) + " emergencies, " +
                         std::to_string(loadReport_.resourcesLoaded) + " resources and " +
                         std::to_string(loadReport_.assignmentsLoaded) + " assignment records from " +
                         files_.dataDirectory() + ".");
    } else {
        console_.info("No saved data found in \"" + files_.dataDirectory() +
                      "\" - starting with an empty system. Files are created on the first save.");
    }
    const size_t shown = std::min<size_t>(loadReport_.warnings.size(), 6);
    for (size_t i = 0; i < shown; ++i) {
        console_.warning(loadReport_.warnings[i]);
    }
    if (loadReport_.warnings.size() > shown) {
        console_.info(std::to_string(loadReport_.warnings.size() - shown) +
                      " more file warning(s): see Settings > Data file status.");
    }
    for (const std::string& path : loadReport_.preserved) {
        console_.info("The original file was kept unchanged as " + path);
    }
    for (const std::string& note : reconcileNotes_) {
        console_.warning("Data check: " + note);
    }
    if (!reconcileNotes_.empty()) {
        persist();
    }
    const int columns = resq_term_columns();
    if (columns > 0 && columns < Console::kWidth) {
        console_.warning("This terminal is " + std::to_string(columns) +
                         " columns wide. RESQ is laid out for 80 columns; widen the window or use landscape mode.");
    }
    if (!loadReport_.anyFileFound() && emergencies_.empty() && resources_.empty() && console_.interactiveInput()) {
        console_.out() << '\n';
        if (console_.confirm("Load FICTIONAL demonstration data to explore RESQ?")) {
            const DemoResult result = DemoData::load(emergencies_, resources_, dispatcher_, now());
            persist();
            log("Demo data loaded");
            console_.success("Added " + std::to_string(result.emergenciesAdded) + " fictional cases and " +
                             std::to_string(result.resourcesAdded) + " fictional resources.");
        }
    }
    console_.pause();
}

void RESQApplication::mainLoop()
{
    while (true) {
        showDashboard();
        const std::string choice = console_.readChoice("Select an option");
        if (choice == "0") {
            return;
        }
        try {
            if (choice == "1") {
                registerEmergency();
            } else if (choice == "2") {
                viewEmergencies();
            } else if (choice == "3") {
                searchEmergencies();
            } else if (choice == "4") {
                updateEmergency();
            } else if (choice == "5") {
                dispatchMenu();
            } else if (choice == "6") {
                resourceMenu();
            } else if (choice == "7") {
                reportsMenu();
            } else if (choice == "8") {
                demoMenu();
            } else if (choice == "9") {
                settingsMenu();
            } else if (choice == "H" || choice == "?") {
                showHelp();
            } else {
                console_.error("\"" + choice + "\" is not a menu option. Enter a number from 0 to 9, or H for help.");
                console_.pause();
            }
        } catch (const InputCancelled&) {
            console_.warning("Cancelled - no further changes were made.");
            console_.pause();
        } catch (const InputClosed&) {
            throw;
        } catch (const std::exception& ex) {
            console_.error(std::string("Unexpected error: ") + ex.what() + ". The action was stopped; data is safe.");
            console_.pause();
        }
    }
}

void RESQApplication::shutdown()
{
    console_.out() << '\n';
    console_.spinner("Saving records ...", 500);
    if (persist()) {
        console_.success("All records saved to " + files_.dataDirectory() + ".");
    } else {
        exitCode_ = 1;
    }
    console_.out() << '\n';
    console_.printBox("RESQ SESSION CLOSED",
                      {console_.paint("Thank you for using RESQ.", Style::Highlight),
                       "Emergencies: " + std::to_string(emergencies_.size()) +
                           "   Resources: " + std::to_string(resources_.size()),
                       console_.paint("Stay safe. In a real emergency, contact your local emergency number.",
                                      Style::Muted)},
                      Style::Danger);
}

bool RESQApplication::persist()
{
    std::string error;
    if (files_.saveAll(emergencies_, resources_, error)) {
        lastSaved_ = now();
        return true;
    }
    console_.error("Could not save data: " + error);
    console_.warning("Changes are kept in memory; RESQ will retry on the next change and at exit.");
    return false;
}

void RESQApplication::log(const std::string& activity)
{
    recentActivity_.push_front(util::formatClock(now()) + "  " + activity);
    while (recentActivity_.size() > 4) {
        recentActivity_.pop_back();
    }
}

std::vector<std::string> RESQApplication::statusLines(const std::vector<Row>& rows, int width) const
{
    std::vector<std::string> lines;
    for (const Row& row : rows) {
        const std::string& value = row.second.first;
        const int labelWidth = width - Console::visibleWidth(value) - 1;
        std::string label = row.first + " ";
        while (Console::visibleWidth(label) < labelWidth) {
            label += '.';
        }
        lines.push_back(console_.paint(label, Style::Plain) + " " + console_.paint(value, row.second.second));
    }
    return lines;
}

void RESQApplication::showDashboard()
{
    console_.clearScreen();
    console_.printLogo();
    const std::time_t t = now();
    emergencies_.recalculateAll(t);
    const Statistics s = analytics_.compute();
    const double utilization = s.utilizationPercent();

    const int boxWidth = 38;
    const std::vector<std::string> left = console_.renderBox(
        "EMERGENCY CASES",
        statusLines({{"Total cases", {std::to_string(s.totalEmergencies), Style::Highlight}},
                     {"Pending", {std::to_string(s.count(Status::Pending)), Style::Warning}},
                     {"Assigned", {std::to_string(s.count(Status::Assigned)), Style::Accent}},
                     {"In progress", {std::to_string(s.count(Status::InProgress)), Style::Progress}},
                     {"Resolved", {std::to_string(s.count(Status::Resolved)), Style::Success}},
                     {"Cancelled", {std::to_string(s.count(Status::Cancelled)), Style::Muted}},
                     {"Critical (still active)", {std::to_string(s.activeCritical), Style::Critical}}},
                    boxWidth - 4),
        boxWidth, Style::Accent);
    const std::vector<std::string> right = console_.renderBox(
        "RESCUE RESOURCES",
        statusLines({{"Total resources", {std::to_string(s.totalResources), Style::Highlight}},
                     {"Available", {std::to_string(s.availableResources), Style::Success}},
                     {"Deployed", {std::to_string(s.deployedResources), Style::Warning}},
                     {"Out of service", {std::to_string(s.outOfServiceResources), Style::Danger}},
                     {"Active assignments", {std::to_string(s.activeAssignments), Style::Accent}},
                     {"Utilization (in service)",
                      {utilization < 0 ? "-" : std::to_string(static_cast<int>(std::lround(utilization))) + "%",
                       Style::Highlight}},
                     {"Assignments logged", {std::to_string(s.totalAssignmentsLogged), Style::Muted}}},
                    boxWidth - 4),
        boxWidth, Style::Accent);
    console_.printSideBySide(left, right);

    const Emergency* next = emergencies_.highestPriorityPending(t);
    console_.out() << '\n';
    if (next != nullptr) {
        console_.out() << "  " << console_.paint(" NEXT IN QUEUE ", Style::Badge) << "  "
                       << console_.paint(next->id(), Style::Highlight) << "  "
                       << console_.paint(util::toUpper(toString(next->severity())), severityStyle(next->severity()))
                       << "  " << toString(next->category()) << "  score "
                       << console_.paint(std::to_string(next->priorityScore()), Style::Highlight) << '\n'
                       << std::string(19, ' ')
                       << console_.fit(next->location() + "  |  waiting " +
                                           util::formatDuration(t - next->registeredAt()) +
                                           "  |  dispatch: menu 5 > 1",
                                       Console::kWidth - 19)
                       << '\n';
    } else {
        console_.out() << "  " << console_.paint("Queue clear: no pending emergencies.", Style::Success) << '\n';
    }
    if (!recentActivity_.empty()) {
        console_.out() << "  " << console_.paint("Recent:", Style::Muted);
        for (size_t i = 0; i < recentActivity_.size() && i < 2; ++i) {
            console_.out() << (i == 0 ? " " : "\n          ") << console_.paint(recentActivity_[i], Style::Muted);
        }
        console_.out() << '\n';
    }

    console_.printSection("Main menu");
    console_.printMenu({{"1", "Register emergency"},
                        {"2", "View all emergencies"},
                        {"3", "Search emergencies"},
                        {"4", "Update emergency"},
                        {"5", "Dispatch & operations"},
                        {"6", "Resource management"},
                        {"7", "Reports & analytics"},
                        {"8", "Demonstration data"},
                        {"9", "Settings & data files"},
                        {"H", "Help & about"},
                        {"0", "Save & exit"}});
    console_.out() << "  "
                   << console_.paint("Data folder: " + files_.dataDirectory() + "  |  changes are saved automatically" +
                                         (lastSaved_ > 0 ? "  |  last save " + util::formatClock(lastSaved_) : ""),
                                     Style::Muted)
                   << "\n\n";
}

void RESQApplication::printEmergencyCard(const Emergency& e)
{
    const std::time_t t = now();
    const auto kv = [this](const std::string& label, const std::string& value) {
        return console_.paint(console_.fit(label, 16), Style::Muted) + ": " + value;
    };
    std::string timing;
    switch (e.status()) {
    case Status::Pending: timing = "waiting " + util::formatDuration(t - e.registeredAt()); break;
    case Status::Assigned:
    case Status::InProgress:
        timing = "first response after " + util::formatDuration(e.assignedAt() - e.registeredAt());
        break;
    case Status::Resolved: timing = "resolved after " + util::formatDuration(e.closedAt() - e.registeredAt()); break;
    case Status::Cancelled: timing = "cancelled"; break;
    }

    std::vector<std::string> lines{
        kv("Category", toString(e.category())),
        kv("Severity", console_.paint(util::toUpper(toString(e.severity())), severityStyle(e.severity()))) +
            "      " + console_.paint("Priority score: ", Style::Muted) +
            console_.paint(std::to_string(e.priorityScore()), Style::Highlight),
        kv("Status", console_.paint(toString(e.status()), statusStyle(e.status()))),
        kv("Location", e.location()),
        kv("Reported by", e.reporter()),
        kv("Contact", e.contact()),
        kv("People affected", std::to_string(e.affected())),
        kv("Registered", util::formatDateTime(e.registeredAt()) + "  (" + timing + ")"),
    };
    if (e.assignedAt() > 0) {
        lines.push_back(kv("Assigned", util::formatDateTime(e.assignedAt())));
    }
    if (e.startedAt() > 0) {
        lines.push_back(kv("Work started", util::formatDateTime(e.startedAt())));
    }
    if (e.closedAt() > 0) {
        lines.push_back(kv(e.status() == Status::Resolved ? "Resolved" : "Closed", util::formatDateTime(e.closedAt())));
    }
    const std::vector<std::string> description = util::wrapText(e.description(), 54);
    for (size_t i = 0; i < description.size(); ++i) {
        lines.push_back(i == 0 ? kv("Description", description[i]) : std::string(18, ' ') + description[i]);
    }
    lines.push_back(kv("Resources", resourceList(resources_.assignedTo(e.id()))));

    const std::string title = "EMERGENCY " + e.id() + (e.isDemo() ? "  [FICTIONAL DEMO RECORD]" : "");
    console_.printBox(title, lines, e.severity() == Severity::Critical && e.isActive() ? Style::Danger : Style::Accent);
}

void RESQApplication::printScoreBreakdown(const Emergency& e)
{
    const PriorityBreakdown b = e.priorityBreakdown(now());
    console_.out() << "  " << console_.paint("Priority " + std::to_string(b.total()), Style::Highlight) << " = severity "
                   << b.severityPoints << " + people " << b.affectedPoints << " + waiting " << b.waitingPoints << " ("
                   << b.waitingMinutes << " min) + category " << b.categoryPoints << '\n'
                   << "  "
                   << console_.paint("The score only orders the queue; it is not a risk prediction. "
                                     "The operator decides.",
                                     Style::Muted)
                   << '\n';
}

void RESQApplication::printEmergencyTable(const std::vector<const Emergency*>& list)
{
    Table table({{"ID", 9},
                 {"Category", 12},
                 {"Location", 15},
                 {"Severity", 8},
                 {"Score", 5, true},
                 {"Status", 11},
                 {"Registered", 11}});
    bool demo = false;
    for (const Emergency* e : list) {
        demo = demo || e->isDemo();
        table.addRow({{e->id() + (e->isDemo() ? "*" : ""), Style::Bold},
                      {shortName(e->category())},
                      {e->location()},
                      {toString(e->severity()), severityStyle(e->severity())},
                      {std::to_string(e->priorityScore()), Style::Highlight},
                      {toString(e->status()), statusStyle(e->status())},
                      {util::formatCompact(e->registeredAt()), Style::Muted}});
    }
    table.print(console_);
    if (demo) {
        console_.out() << "  " << console_.paint("* fictional demonstration record", Style::Muted) << '\n';
    }
}

void RESQApplication::printResourceTable(const std::vector<const Resource*>& list)
{
    Table table({{"ID", 9}, {"Type", 14}, {"Location", 15}, {"Status", 15}, {"Cap", 4, true}, {"Assigned to", 15}});
    bool demo = false;
    for (const Resource* r : list) {
        demo = demo || r->isDemo();
        std::string assigned;
        for (const std::string& eid : r->assignedEmergencies()) {
            assigned += (assigned.empty() ? "" : ",") + eid;
        }
        table.addRow({{r->id() + (r->isDemo() ? "*" : ""), Style::Bold},
                      {shortName(r->type())},
                      {r->location()},
                      {r->statusLabel(), resourceStyle(r->status())},
                      {std::to_string(r->capacity())},
                      {assigned.empty() ? "-" : assigned, assigned.empty() ? Style::Muted : Style::Plain}});
    }
    table.print(console_);
    if (demo) {
        console_.out() << "  " << console_.paint("* fictional demonstration record", Style::Muted) << '\n';
    }
}

void RESQApplication::printResourceCard(const Resource& r)
{
    const auto kv = [this](const std::string& label, const std::string& value) {
        return console_.paint(console_.fit(label, 18), Style::Muted) + ": " + value;
    };
    int deployments = 0;
    for (const AssignmentRecord& record : resources_.history()) {
        deployments += record.resourceId == r.id() ? 1 : 0;
    }
    std::string assigned;
    for (const std::string& eid : r.assignedEmergencies()) {
        const Emergency* e = emergencies_.find(eid);
        assigned += (assigned.empty() ? "" : ", ") + eid + (e ? " (" + toString(e->status()) + ")" : "");
    }
    console_.printBox("RESOURCE " + r.id() + (r.isDemo() ? "  [FICTIONAL DEMO RECORD]" : ""),
                      {kv("Type", toString(r.type())),
                       kv("Current location", r.location()),
                       kv("Availability", console_.paint(r.statusLabel(), resourceStyle(r.status()))),
                       kv("Capacity", r.capacity() == 0 ? "not applicable" : std::to_string(r.capacity()) + " people"),
                       kv("Max simultaneous", std::to_string(r.maxConcurrent()) +
                                                  (r.maxConcurrent() == 1 ? " (exclusive use)" : " (shared use)")),
                       kv("Assigned to", assigned.empty() ? "-" : assigned),
                       kv("Deployments logged", std::to_string(deployments))});
}

std::vector<SuitableResource> RESQApplication::printSuitableResources(const Emergency& e)
{
    const std::vector<SuitableResource> list = resources_.suitableFor(e);
    const int capacity = resources_.capacityAssignedTo(e.id());
    console_.out() << "  People affected: " << e.affected() << "   Capacity already assigned: " << capacity << '\n';
    if (e.affected() > 0 && capacity < e.affected() && !resources_.assignedTo(e.id()).empty()) {
        console_.warning("Assigned capacity is below the number of people affected; consider more resources.");
    }
    if (list.empty()) {
        console_.warning("No suitable resource is available right now. Options: release one from a lower-priority "
                         "case, return one to service, or add a new resource (A lists all available).");
        return list;
    }
    Table table({{"#", 2, true}, {"ID", 9}, {"Type", 14}, {"Location", 16}, {"Cap", 4, true}, {"Load", 5}, {"Match", 21}});
    for (size_t i = 0; i < list.size(); ++i) {
        const Resource& r = *list[i].resource;
        table.addRow({{std::to_string(i + 1), Style::Heading},
                      {r.id(), Style::Bold},
                      {shortName(r.type())},
                      {r.location()},
                      {std::to_string(r.capacity())},
                      {std::to_string(r.activeAssignments()) + "/" + std::to_string(r.maxConcurrent())},
                      {list[i].reason, list[i].typeRank == 0 ? Style::Success : Style::Plain}});
    }
    table.print(console_);
    console_.out() << "  "
                   << console_.paint("Matching uses resource type and location names only (no maps or GPS).",
                                     Style::Muted)
                   << '\n';
    return list;
}

Category RESQApplication::pickCategory()
{
    std::vector<std::pair<std::string, std::string>> items;
    for (Category c : allCategories()) {
        items.emplace_back(std::to_string(static_cast<int>(c)), toString(c));
    }
    console_.out() << "  " << console_.paint("Emergency category:", Style::Accent) << '\n';
    console_.printMenu(items);
    Category result = Category::Other;
    fromNumber(static_cast<int>(console_.readNumber("Category (1-7)", 1, kCategoryCount)), result);
    return result;
}

Severity RESQApplication::pickSeverity()
{
    const std::vector<std::pair<Severity, std::string>> options{
        {Severity::Critical, "immediate threat to life or a spreading hazard"},
        {Severity::High, "serious and urgent, not immediately life-threatening"},
        {Severity::Medium, "needs a response; situation currently stable"},
        {Severity::Low, "minor; no immediate danger"},
    };
    console_.out() << "  " << console_.paint("Severity level:", Style::Accent) << '\n';
    for (const auto& option : options) {
        console_.out() << "  " << console_.paint("[" + std::to_string(static_cast<int>(option.first)) + "]", Style::Heading)
                       << ' ' << console_.paint(console_.fit(toString(option.first), 9), severityStyle(option.first))
                       << "- " << option.second << '\n';
    }
    Severity result = Severity::Low;
    fromNumber(static_cast<int>(console_.readNumber("Severity (1-4)", 1, kSeverityCount)), result);
    return result;
}

ResourceType RESQApplication::pickResourceType()
{
    std::vector<std::pair<std::string, std::string>> items;
    for (ResourceType type : allResourceTypes()) {
        items.emplace_back(std::to_string(static_cast<int>(type)), toString(type));
    }
    console_.out() << "  " << console_.paint("Resource type:", Style::Accent) << '\n';
    console_.printMenu(items);
    ResourceType result = ResourceType::Ambulance;
    fromNumber(static_cast<int>(console_.readNumber("Type (1-6)", 1, kResourceTypeCount)), result);
    return result;
}

std::string RESQApplication::readValidated(const std::string& prompt, size_t maxLength,
                                           const std::function<std::string(const std::string&)>& validate)
{
    while (true) {
        const std::string value = console_.readText(prompt, maxLength);
        const std::string error = validate(value);
        if (error.empty()) {
            return value;
        }
        console_.error(error);
    }
}

std::time_t RESQApplication::readRegistrationTime()
{
    const std::time_t current = now();
    console_.out() << "  " << console_.paint("Registration time: press Enter for now (" + util::formatDateTime(current) +
                                                 ") or type YYYY-MM-DD HH:MM",
                                             Style::Muted)
                   << '\n';
    while (true) {
        const std::string value = console_.readText("Registration time", 16, false);
        if (value.empty()) {
            return current;
        }
        std::time_t parsed = 0;
        if (!util::parseDateTime(value, parsed)) {
            console_.error("Use the format YYYY-MM-DD HH:MM, for example 2026-10-18 14:30.");
            continue;
        }
        if (parsed > current + 300) {
            console_.error("The registration time cannot be in the future.");
            continue;
        }
        if (current - parsed > 7 * 24 * 3600 &&
            !console_.confirm("That is more than 7 days ago. Use it anyway?")) {
            continue;
        }
        return parsed;
    }
}

Emergency* RESQApplication::promptEmergency(const std::string& prompt)
{
    const std::string id = console_.readText(prompt, 16);
    Emergency* e = emergencies_.find(id);
    if (e == nullptr) {
        console_.error("No emergency with ID " + util::toUpper(id) + " exists. Use Search (menu 3) to look it up.");
    }
    return e;
}

Resource* RESQApplication::promptResource(const std::string& prompt)
{
    const std::string id = console_.readText(prompt, 16);
    Resource* r = resources_.find(id);
    if (r == nullptr) {
        console_.error("No resource with ID " + util::toUpper(id) + " exists. Use View or Search to find it.");
    }
    return r;
}

void RESQApplication::registerEmergency()
{
    ScreenScope scope(console_, "Register emergency");
    console_.printHeader("Register a new emergency");
    console_.info("Type :q at any prompt to cancel. All fields are required unless a default is shown.");
    console_.out() << '\n';

    const std::string suggested = emergencies_.generateId();
    std::string id;
    while (true) {
        const std::string input = console_.readText("Emergency ID [Enter = " + suggested + "]", 8, false);
        id = input.empty() ? suggested : util::toUpper(input);
        std::string error = Emergency::validateId(id);
        if (error.empty() && emergencies_.exists(id)) {
            error = "An emergency with ID " + id + " already exists. IDs must be unique.";
        }
        if (error.empty()) {
            break;
        }
        console_.error(error);
    }
    const std::string reporter = readValidated("Reporter's name", Emergency::kMaxReporter, Emergency::validateReporter);
    const Category category = pickCategory();
    const std::string location = readValidated("Location", Emergency::kMaxLocation, Emergency::validateLocation);
    const std::string contact =
        readValidated("Contact phone or e-mail", Emergency::kMaxContact, Emergency::validateContact);
    const std::string description =
        readValidated("Description (max 200 chars)", Emergency::kMaxDescription, Emergency::validateDescription);
    const Severity severity = pickSeverity();
    const int affected = static_cast<int>(console_.readNumber("People affected (0 if none)", 0, Emergency::kMaxAffected));
    const std::time_t registered = readRegistrationTime();

    Emergency emergency(id, reporter, category, location, contact, description, severity, affected, registered);
    emergency.recalculatePriority(now());
    console_.out() << '\n';
    printEmergencyCard(emergency);
    if (!console_.confirm("Register this emergency?", true)) {
        console_.warning("Not registered. Nothing was saved.");
        console_.pause();
        return;
    }
    const std::string error = emergencies_.add(emergency);
    if (!error.empty()) {
        console_.error(error);
        console_.pause();
        return;
    }
    persist();
    log("Registered " + id + " (" + shortName(category) + ", " + toString(severity) + ")");
    const int position = emergencies_.queuePosition(id, now());
    const int queueSize = static_cast<int>(emergencies_.pendingByPriority(now()).size());
    console_.success("Emergency " + id + " registered with priority score " +
                     std::to_string(emergencies_.find(id)->priorityScore()) + " - position " +
                     std::to_string(position) + " of " + std::to_string(queueSize) + " in the pending queue.");
    printScoreBreakdown(*emergencies_.find(id));
    if (severity == Severity::Critical) {
        console_.warning("CRITICAL case: dispatch resources as soon as possible.");
    }
    if (console_.confirm("Assign a resource to " + id + " now?")) {
        if (assignResourceFlow(id) && console_.confirm("Have crews started work on scene? Mark In Progress now?")) {
            const OpResult result = dispatcher_.startOperation(id, now());
            result.ok ? console_.success(result.message) : console_.error(result.message);
            if (result.ok) {
                persist();
                log(id + " in progress");
            }
        }
    }
    console_.pause();
}

void RESQApplication::viewEmergencies()
{
    ScreenScope scope(console_, "All emergencies");
    EmergencySortKey key = EmergencySortKey::Priority;
    bool reverse = false;
    while (true) {
        console_.printHeader("All emergencies");
        if (emergencies_.empty()) {
            console_.info("No emergencies registered yet. Register one (menu 1) or load demo data (menu 8).");
            console_.pause();
            return;
        }
        emergencies_.recalculateAll(now());
        std::string order;
        switch (key) {
        case EmergencySortKey::Severity: order = "severity (Critical first)"; break;
        case EmergencySortKey::Priority: order = "priority score (highest first)"; break;
        case EmergencySortKey::RegistrationTime: order = "registration time (oldest first)"; break;
        case EmergencySortKey::Id: order = "emergency ID (A-Z)"; break;
        }
        console_.out() << "  " << emergencies_.size() << " record(s), sorted by " << order
                       << (reverse ? " - reversed" : "") << "\n\n";
        printEmergencyTable(emergencies_.sorted(key, reverse));
        console_.out() << '\n';
        console_.printMenu({{"1", "Sort by severity"},
                            {"2", "Sort by priority score"},
                            {"3", "Sort by registration time"},
                            {"4", "Sort by emergency ID"},
                            {"R", "Reverse the order"},
                            {"D", "Show full details of a case"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            key = EmergencySortKey::Severity;
            reverse = false;
        } else if (choice == "2") {
            key = EmergencySortKey::Priority;
            reverse = false;
        } else if (choice == "3") {
            key = EmergencySortKey::RegistrationTime;
            reverse = false;
        } else if (choice == "4") {
            key = EmergencySortKey::Id;
            reverse = false;
        } else if (choice == "R") {
            reverse = !reverse;
        } else if (choice == "D") {
            if (const Emergency* e = promptEmergency("Emergency ID")) {
                printEmergencyCard(*e);
                printScoreBreakdown(*e);
            }
            console_.pause();
        } else {
            console_.error("Invalid option.");
            console_.pause();
        }
    }
}

void RESQApplication::searchEmergencies()
{
    ScreenScope scope(console_, "Search");
    while (true) {
        console_.printHeader("Search emergencies");
        if (emergencies_.empty()) {
            console_.info("There are no emergencies to search yet.");
            console_.pause();
            return;
        }
        console_.printMenu({{"1", "By emergency ID"},
                            {"2", "By reporter's name"},
                            {"3", "By location"},
                            {"4", "By category"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Search by");
        std::vector<const Emergency*> results;
        std::string description;
        if (choice == "0") {
            return;
        } else if (choice == "1" || choice == "2" || choice == "3") {
            const EmergencySearchField field = choice == "1"   ? EmergencySearchField::Id
                                               : choice == "2" ? EmergencySearchField::Reporter
                                                               : EmergencySearchField::Location;
            const std::string query = console_.readText("Search text (part of a word is enough)", 40);
            results = emergencies_.search(field, query);
            description = "\"" + query + "\"";
        } else if (choice == "4") {
            const Category category = pickCategory();
            results = emergencies_.searchByCategory(category);
            description = "category " + toString(category);
        } else {
            console_.error("Invalid option.");
            console_.pause();
            continue;
        }

        emergencies_.recalculateAll(now());
        console_.out() << '\n';
        if (results.empty()) {
            console_.warning("No emergencies match " + description + ".");
        } else {
            console_.success(std::to_string(results.size()) + " match(es) for " + description + ".");
            console_.out() << '\n';
            if (results.size() <= 5) {
                for (const Emergency* e : results) {
                    printEmergencyCard(*e);
                }
            } else {
                printEmergencyTable(results);
                console_.out() << '\n';
                const std::string id = console_.readText("ID for full details (Enter to skip)", 16, false);
                if (!id.empty()) {
                    if (const Emergency* e = emergencies_.find(id)) {
                        printEmergencyCard(*e);
                    } else {
                        console_.error("No emergency with ID " + util::toUpper(id) + ".");
                    }
                }
            }
        }
        console_.pause();
    }
}

void RESQApplication::updateEmergency()
{
    ScreenScope scope(console_, "Update emergency");
    console_.printHeader("Update emergency");
    if (emergencies_.empty()) {
        console_.info("There are no emergencies to update yet.");
        console_.pause();
        return;
    }
    Emergency* found = promptEmergency("Emergency ID to update");
    if (found == nullptr) {
        console_.pause();
        return;
    }
    const std::string id = found->id();
    while (true) {
        const Emergency* e = emergencies_.find(id);
        console_.printHeader("Update emergency " + id);
        emergencies_.recalculateAll(now());
        printEmergencyCard(*e);
        console_.out() << '\n';
        console_.printMenu({{"1", "Change severity"},
                            {"2", "Update location"},
                            {"3", "Edit description"},
                            {"4", "Update people affected"},
                            {"5", "Change case status"},
                            {"0", "Done"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        }
        const int before = e->priorityScore();
        std::string error;
        std::string change;
        if ((choice == "1" || choice == "2" || choice == "4") && e->isClosed()) {
            console_.error("Case " + id + " is " + toString(e->status()) +
                           "; only its description can still be corrected.");
            console_.pause();
            continue;
        }
        if (choice == "1") {
            const Severity old = e->severity();
            const Severity severity = pickSeverity();
            error = emergencies_.updateSeverity(id, severity, now());
            change = "Severity " + toString(old) + " -> " + toString(severity);
        } else if (choice == "2") {
            const std::string location = readValidated("New location", Emergency::kMaxLocation, Emergency::validateLocation);
            error = emergencies_.updateLocation(id, location, now());
            change = "Location updated to " + location;
        } else if (choice == "3") {
            const std::string text =
                readValidated("New description", Emergency::kMaxDescription, Emergency::validateDescription);
            error = emergencies_.updateDescription(id, text, now());
            change = "Description updated";
        } else if (choice == "4") {
            const int old = e->affected();
            const int affected =
                static_cast<int>(console_.readNumber("People affected", 0, Emergency::kMaxAffected));
            error = emergencies_.updateAffected(id, affected, now());
            change = "People affected " + std::to_string(old) + " -> " + std::to_string(affected);
        } else if (choice == "5") {
            changeStatusFlow(id);
            console_.pause();
            continue;
        } else {
            console_.error("Invalid option.");
            console_.pause();
            continue;
        }
        if (!error.empty()) {
            console_.error(error);
        } else {
            persist();
            log(id + ": " + change);
            const int after = emergencies_.find(id)->priorityScore();
            console_.success(change + ". Priority score " + std::to_string(before) + " -> " + std::to_string(after) +
                             ".");
        }
        console_.pause();
    }
}

void RESQApplication::changeStatusFlow(const std::string& emergencyId)
{
    const Emergency* e = emergencies_.find(emergencyId);
    const std::vector<Status> allowed = dispatcher_.allowedNextStatuses(emergencyId);
    if (e == nullptr) {
        return;
    }
    if (allowed.empty()) {
        console_.info("Case " + e->id() + " is " + toString(e->status()) + "; closed cases cannot change status.");
        return;
    }
    console_.out() << "  Current status: " << console_.paint(toString(e->status()), statusStyle(e->status()))
                   << ". Allowed next steps:\n";
    for (size_t i = 0; i < allowed.size(); ++i) {
        console_.out() << "  " << console_.paint("[" + std::to_string(i + 1) + "]", Style::Heading) << ' '
                       << console_.paint(console_.fit(toString(allowed[i]), 12), statusStyle(allowed[i]))
                       << statusHint(allowed[i]) << '\n';
    }
    console_.out() << "  " << console_.paint("[0]", Style::Heading) << " Keep the current status\n";
    const long long choice = console_.readNumber("Choose", 0, static_cast<long long>(allowed.size()));
    if (choice == 0) {
        return;
    }
    const Status target = allowed[static_cast<size_t>(choice - 1)];
    if (target == Status::Assigned) {
        assignResourceFlow(emergencyId);
        return;
    }
    if ((target == Status::Resolved || target == Status::Cancelled || target == Status::Pending) &&
        !console_.confirm("Change " + e->id() + " to " + toString(target) + "? Assigned resources will be released.")) {
        console_.warning("Status unchanged.");
        return;
    }
    const OpResult result = dispatcher_.changeStatus(emergencyId, target, now());
    if (result.ok) {
        persist();
        log(util::toUpper(emergencyId) + " -> " + toString(target));
        console_.success(result.message);
    } else {
        console_.error(result.message);
    }
}

void RESQApplication::dispatchMenu()
{
    ScreenScope scope(console_, "Dispatch");
    while (true) {
        console_.printHeader("Dispatch & operations");
        const size_t pending = emergencies_.withStatus(Status::Pending).size();
        const size_t active =
            emergencies_.withStatus(Status::Assigned).size() + emergencies_.withStatus(Status::InProgress).size();
        console_.out() << "  Pending in queue: " << console_.paint(std::to_string(pending), Style::Warning)
                       << "    Active operations: " << console_.paint(std::to_string(active), Style::Accent)
                       << "    Resources ready: "
                       << console_.paint(std::to_string(resources_.acceptingAssignments().size()), Style::Success)
                       << "\n\n";
        console_.printMenu({{"1", "Process next highest-priority case"},
                            {"2", "View the pending priority queue"},
                            {"3", "Manage active operations"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            processNextEmergency();
        } else if (choice == "2") {
            viewPriorityQueue();
        } else if (choice == "3") {
            manageActiveOperations();
        } else {
            console_.error("Invalid option.");
            console_.pause();
        }
    }
}

void RESQApplication::viewPriorityQueue()
{
    ScreenScope scope(console_, "Priority queue");
    console_.printHeader("Pending priority queue");
    const std::vector<const Emergency*> queue = emergencies_.pendingByPriority(now());
    if (queue.empty()) {
        console_.success("No pending emergencies - the queue is clear.");
    } else {
        console_.out() << "  " << queue.size() << " pending case(s), highest priority first:\n\n";
        printEmergencyTable(queue);
        console_.out() << '\n';
        console_.out() << "  " << console_.paint("Ordered by a std::priority_queue on score, then registration time.",
                                                 Style::Muted)
                       << '\n';
    }
    console_.pause();
}

void RESQApplication::processNextEmergency()
{
    ScreenScope scope(console_, "Process next");
    std::string selected;
    while (true) {
        const std::vector<const Emergency*> queue = emergencies_.pendingByPriority(now());
        console_.printHeader("Process next emergency");
        if (queue.empty()) {
            console_.success("The pending queue is empty - every case is assigned, in progress or closed.");
            console_.pause();
            return;
        }
        const Emergency* e = selected.empty() ? nullptr : emergencies_.find(selected);
        if (e == nullptr || e->status() != Status::Pending) {
            e = queue.front();
            selected.clear();
        }
        const std::string id = e->id();
        const int position = emergencies_.queuePosition(id, now());
        console_.out() << "  Queue position " << console_.paint(std::to_string(position), Style::Highlight) << " of "
                       << queue.size()
                       << (position == 1 ? " - highest priority pending case" : " - selected by the operator")
                       << "\n";
        printEmergencyCard(*e);
        printScoreBreakdown(*e);
        console_.printSection("Suitable available resources");
        printSuitableResources(*e);
        console_.out() << '\n';
        console_.printMenu({{"1", "Assign resource(s) to this case"},
                            {"2", "Choose a different pending case"},
                            {"3", "Cancel this case (false alarm)"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            if (assignResourceFlow(id) &&
                console_.confirm("Have crews started work on scene? Mark " + id + " In Progress now?")) {
                const OpResult result = dispatcher_.startOperation(id, now());
                result.ok ? console_.success(result.message) : console_.error(result.message);
                if (result.ok) {
                    persist();
                    log(id + " in progress");
                }
            }
            selected.clear();
            console_.pause();
        } else if (choice == "2") {
            printEmergencyTable(queue);
            const std::string other = console_.readText("Pending case ID", 16);
            const Emergency* candidate = emergencies_.find(other);
            if (candidate == nullptr || candidate->status() != Status::Pending) {
                console_.error(util::toUpper(other) + " is not a pending case.");
                console_.pause();
            } else {
                selected = candidate->id();
            }
        } else if (choice == "3") {
            if (console_.confirm("Cancel " + id + " as a false alarm or duplicate?")) {
                const OpResult result = dispatcher_.cancel(id, now());
                result.ok ? console_.success(result.message) : console_.error(result.message);
                if (result.ok) {
                    persist();
                    log(id + " cancelled");
                }
                console_.pause();
            }
        } else {
            console_.error("Invalid option.");
            console_.pause();
        }
    }
}

bool RESQApplication::assignResourceFlow(const std::string& emergencyId)
{
    bool assignedAny = false;
    while (true) {
        const Emergency* e = emergencies_.find(emergencyId);
        if (e == nullptr || !e->isActive()) {
            console_.error("Resources can only be assigned to an existing, active case.");
            return assignedAny;
        }
        console_.printSection("Assign resources to " + e->id());
        console_.out() << "  " << console_.paint(util::toUpper(toString(e->severity())), severityStyle(e->severity()))
                       << "  " << toString(e->category()) << "  |  " << e->location() << "  |  status "
                       << console_.paint(toString(e->status()), statusStyle(e->status())) << '\n'
                       << "  Already assigned: " << resourceList(resources_.assignedTo(e->id())) << '\n';
        const std::vector<SuitableResource> list = printSuitableResources(*e);
        const std::string input = util::toUpper(
            console_.readText("Resource ID or list # (A = all available, Enter = finish)", 16, false));
        if (input.empty()) {
            return assignedAny;
        }
        if (input == "A") {
            console_.out() << '\n';
            printResourceTable(resources_.acceptingAssignments());
            continue;
        }
        std::string resourceId = input;
        long long number = 0;
        if (util::parseInteger(input, number)) {
            if (number < 1 || number > static_cast<long long>(list.size())) {
                console_.error("There is no entry #" + input + " in the list.");
                continue;
            }
            resourceId = list[static_cast<size_t>(number - 1)].resource->id();
        }
        const Resource* r = resources_.find(resourceId);
        if (r == nullptr) {
            console_.error("No resource with ID " + resourceId + " exists.");
            continue;
        }
        if (r->canAccept() && !ResourceManager::isRecommended(r->type(), e->category()) &&
            !console_.confirm(r->id() + " is a " + toString(r->type()) + ", not a recommended type for " +
                              toString(e->category()) + ". Assign anyway?")) {
            continue;
        }
        const OpResult result = dispatcher_.assign(emergencyId, resourceId, now());
        if (!result.ok) {
            console_.error(result.message);
            continue;
        }
        persist();
        log("Assigned " + r->id() + " to " + e->id());
        console_.success(result.message);
        assignedAny = true;
        if (!console_.confirm("Assign another resource to " + e->id() + "?")) {
            return true;
        }
    }
}

void RESQApplication::manageActiveOperations()
{
    ScreenScope scope(console_, "Active operations");
    while (true) {
        console_.printHeader("Active operations");
        std::vector<const Emergency*> active;
        for (const Emergency* e : emergencies_.sorted(EmergencySortKey::Priority)) {
            if (e->status() == Status::Assigned || e->status() == Status::InProgress) {
                active.push_back(e);
            }
        }
        if (active.empty()) {
            console_.info("No active operations. Process a pending case first (Dispatch > 1).");
            console_.pause();
            return;
        }
        Table table({{"ID", 9}, {"Category", 12}, {"Location", 15}, {"Severity", 8}, {"Status", 11}, {"Res", 3, true},
                     {"Assigned", 12}});
        for (const Emergency* e : active) {
            table.addRow({{e->id() + (e->isDemo() ? "*" : ""), Style::Bold},
                          {shortName(e->category())},
                          {e->location()},
                          {toString(e->severity()), severityStyle(e->severity())},
                          {toString(e->status()), statusStyle(e->status())},
                          {std::to_string(resources_.assignedTo(e->id()).size())},
                          {util::formatCompact(e->assignedAt()), Style::Muted}});
        }
        table.print(console_);
        console_.out() << '\n';
        const std::string id = console_.readText("Case ID to manage (Enter = back)", 16, false);
        if (id.empty()) {
            return;
        }
        const Emergency* e = emergencies_.find(id);
        if (e == nullptr || !e->isActive()) {
            console_.error(util::toUpper(id) + " is not an active case.");
            console_.pause();
            continue;
        }
        manageOperation(e->id());
    }
}

void RESQApplication::manageOperation(const std::string& emergencyId)
{
    ScreenScope scope(console_, emergencyId);
    while (true) {
        const Emergency* e = emergencies_.find(emergencyId);
        if (e == nullptr) {
            return;
        }
        console_.printHeader("Operation " + emergencyId);
        printEmergencyCard(*e);
        if (e->isClosed()) {
            console_.info("This case is closed.");
            console_.pause();
            return;
        }
        console_.out() << '\n';
        console_.printMenu({{"1", "Assign an additional resource"},
                            {"2", "Release a resource"},
                            {"3", "Mark In Progress (work started)"},
                            {"4", "Mark Resolved (work complete)"},
                            {"5", "Return case to Pending queue"},
                            {"6", "Cancel case"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        OpResult result;
        bool attempted = true;
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            assignResourceFlow(emergencyId);
            attempted = false;
        } else if (choice == "2") {
            const auto assigned = resources_.assignedTo(emergencyId);
            if (assigned.empty()) {
                result = OpResult::failure("No resources are assigned to " + emergencyId + ".");
            } else {
                const std::string rid = assigned.size() == 1
                                            ? assigned.front()->id()
                                            : console_.readText("Resource ID to release (" + resourceList(assigned) + ")", 16);
                result = console_.confirm("Release " + util::toUpper(rid) + " from " + emergencyId + "?")
                             ? dispatcher_.release(emergencyId, rid, now())
                             : OpResult::failure("Release cancelled.");
            }
        } else if (choice == "3") {
            result = dispatcher_.startOperation(emergencyId, now());
        } else if (choice == "4") {
            result = console_.confirm("Confirm the operation for " + emergencyId +
                                      " is complete. All its resources will be released.")
                         ? dispatcher_.resolve(emergencyId, now())
                         : OpResult::failure("Case not resolved.");
        } else if (choice == "5") {
            result = console_.confirm("Return " + emergencyId + " to the queue and release its resources?")
                         ? dispatcher_.returnToQueue(emergencyId, now())
                         : OpResult::failure("Case unchanged.");
        } else if (choice == "6") {
            result = console_.confirm("Cancel " + emergencyId + "? All its resources will be released.")
                         ? dispatcher_.cancel(emergencyId, now())
                         : OpResult::failure("Case not cancelled.");
        } else {
            result = OpResult::failure("Invalid option.");
        }
        if (attempted) {
            if (result.ok) {
                persist();
                log(emergencyId + ": " + toString(emergencies_.find(emergencyId)->status()));
                console_.success(result.message);
            } else {
                console_.error(result.message);
            }
        }
        console_.pause();
    }
}

void RESQApplication::resourceMenu()
{
    ScreenScope scope(console_, "Resources");
    while (true) {
        console_.printHeader("Resource management");
        const Statistics s = analytics_.compute();
        console_.out() << "  Total: " << console_.paint(std::to_string(s.totalResources), Style::Highlight)
                       << "   Available: " << console_.paint(std::to_string(s.availableResources), Style::Success)
                       << "   Deployed: " << console_.paint(std::to_string(s.deployedResources), Style::Warning)
                       << "   Out of service: "
                       << console_.paint(std::to_string(s.outOfServiceResources), Style::Danger) << "\n\n";
        console_.printMenu({{"1", "Add resource"},
                            {"2", "View all resources"},
                            {"3", "Search resources"},
                            {"4", "Update availability / location"},
                            {"5", "Assign resource to emergency"},
                            {"6", "Release resource"},
                            {"7", "Assignment history"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            addResource();
        } else if (choice == "2") {
            viewResources();
        } else if (choice == "3") {
            searchResources();
        } else if (choice == "4") {
            updateResourceAvailability();
        } else if (choice == "5") {
            assignResourceMenu();
        } else if (choice == "6") {
            releaseResourceMenu();
        } else if (choice == "7") {
            viewAssignmentHistory();
        } else {
            console_.error("Invalid option.");
            console_.pause();
        }
    }
}

void RESQApplication::addResource()
{
    ScreenScope scope(console_, "Add");
    console_.printHeader("Add a rescue resource");
    console_.info("Type :q at any prompt to cancel.");
    console_.out() << '\n';
    const std::string suggested = resources_.generateId();
    std::string id;
    while (true) {
        const std::string input = console_.readText("Resource ID [Enter = " + suggested + "]", 8, false);
        id = input.empty() ? suggested : util::toUpper(input);
        std::string error = Resource::validateId(id);
        if (error.empty() && resources_.exists(id)) {
            error = "A resource with ID " + id + " already exists. IDs must be unique.";
        }
        if (error.empty()) {
            break;
        }
        console_.error(error);
    }
    const ResourceType type = pickResourceType();
    const std::string location = readValidated("Current location (base)", Resource::kMaxLocation, Resource::validateLocation);
    const int capacity = static_cast<int>(console_.readNumberOr("Capacity - people it can carry or serve (0 = n/a)", 0,
                                                                Resource::kMaxCapacity, defaultCapacity(type)));
    console_.out() << "  " << console_.paint("Most resources serve one emergency at a time. Allow more only if this "
                                             "resource can genuinely be split (e.g. stock of supply kits).",
                                             Style::Muted)
                   << '\n';
    const int maxConcurrent = static_cast<int>(
        console_.readNumberOr("Max simultaneous emergencies (1 = exclusive)", 1, Resource::kMaxConcurrent, 1));

    Resource resource(id, type, location, capacity, maxConcurrent);
    console_.out() << '\n';
    printResourceCard(resource);
    if (!console_.confirm("Add this resource?", true)) {
        console_.warning("Not added. Nothing was saved.");
        console_.pause();
        return;
    }
    const std::string error = resources_.add(resource);
    if (!error.empty()) {
        console_.error(error);
    } else {
        persist();
        log("Added resource " + id + " (" + shortName(type) + ")");
        console_.success("Resource " + id + " added and available for assignment.");
    }
    console_.pause();
}

void RESQApplication::viewResources()
{
    ScreenScope scope(console_, "All resources");
    console_.printHeader("All rescue resources");
    if (resources_.empty()) {
        console_.info("No resources yet. Add one (Resources > 1) or load demo data (menu 8).");
    } else {
        printResourceTable(resources_.all());
    }
    console_.pause();
}

void RESQApplication::searchResources()
{
    ScreenScope scope(console_, "Search");
    console_.printHeader("Search resources");
    if (resources_.empty()) {
        console_.info("There are no resources to search yet.");
        console_.pause();
        return;
    }
    console_.printMenu({{"1", "By resource ID"},
                        {"2", "By type"},
                        {"3", "By location"},
                        {"4", "By availability"},
                        {"0", "Back"}});
    const std::string choice = console_.readChoice("Search by");
    std::vector<const Resource*> results;
    std::string description;
    if (choice == "0") {
        return;
    } else if (choice == "1" || choice == "3") {
        const std::string query = console_.readText("Search text", 40);
        results = resources_.search(choice == "1" ? ResourceSearchField::Id : ResourceSearchField::Location, query);
        description = "\"" + query + "\"";
    } else if (choice == "2") {
        const ResourceType type = pickResourceType();
        results = resources_.byType(type);
        description = "type " + toString(type);
    } else if (choice == "4") {
        console_.printMenu({{"1", "Available"}, {"2", "Deployed"}, {"3", "Out of service"}});
        const int n = static_cast<int>(console_.readNumber("Availability (1-3)", 1, 3));
        const ResourceStatus status = allResourceStatuses()[static_cast<size_t>(n - 1)];
        results = resources_.byStatus(status);
        description = "status " + toString(status);
    } else {
        console_.error("Invalid option.");
        console_.pause();
        return;
    }
    console_.out() << '\n';
    if (results.empty()) {
        console_.warning("No resources match " + description + ".");
    } else {
        console_.success(std::to_string(results.size()) + " match(es) for " + description + ".");
        if (results.size() == 1) {
            printResourceCard(*results.front());
        } else {
            printResourceTable(results);
        }
    }
    console_.pause();
}

void RESQApplication::updateResourceAvailability()
{
    ScreenScope scope(console_, "Update");
    console_.printHeader("Update resource availability");
    if (resources_.empty()) {
        console_.info("There are no resources yet.");
        console_.pause();
        return;
    }
    Resource* r = promptResource("Resource ID");
    if (r == nullptr) {
        console_.pause();
        return;
    }
    const std::string id = r->id();
    printResourceCard(*r);
    console_.printMenu({{"1", "Mark out of service"}, {"2", "Return to service"}, {"3", "Update current location"}, {"0", "Back"}});
    const std::string choice = console_.readChoice("Choose");
    std::string error;
    std::string change;
    if (choice == "0") {
        return;
    } else if (choice == "1") {
        error = resources_.setOutOfService(id, true);
        change = id + " marked out of service; it cannot be assigned until returned to service.";
    } else if (choice == "2") {
        if (!r->isOutOfService()) {
            console_.info(id + " is already in service.");
            console_.pause();
            return;
        }
        error = resources_.setOutOfService(id, false);
        change = id + " is back in service.";
    } else if (choice == "3") {
        const std::string location = readValidated("New location", Resource::kMaxLocation, Resource::validateLocation);
        error = resources_.setLocation(id, location);
        change = id + " location updated to " + location + ".";
    } else {
        error = "Invalid option.";
    }
    if (!error.empty()) {
        console_.error(error);
    } else {
        persist();
        log(change.substr(0, 40));
        console_.success(change);
    }
    console_.pause();
}

void RESQApplication::assignResourceMenu()
{
    ScreenScope scope(console_, "Assign");
    console_.printHeader("Assign a resource to an emergency");
    const std::vector<const Emergency*> queue = emergencies_.pendingByPriority(now());
    if (!queue.empty()) {
        console_.out() << "  Pending cases (highest priority first):\n";
        printEmergencyTable(queue);
        console_.out() << '\n';
    }
    Emergency* e = promptEmergency("Emergency ID");
    if (e != nullptr) {
        if (!e->isActive()) {
            console_.error("Case " + e->id() + " is " + toString(e->status()) + "; only active cases can receive resources.");
        } else {
            assignResourceFlow(e->id());
        }
    }
    console_.pause();
}

void RESQApplication::releaseResourceMenu()
{
    ScreenScope scope(console_, "Release");
    console_.printHeader("Release a resource");
    const std::vector<const Resource*> deployed = resources_.byStatus(ResourceStatus::Deployed);
    if (deployed.empty()) {
        console_.info("No resources are currently deployed.");
        console_.pause();
        return;
    }
    printResourceTable(deployed);
    console_.out() << '\n';
    Resource* r = promptResource("Resource ID to release");
    if (r == nullptr) {
        console_.pause();
        return;
    }
    const std::vector<std::string> assigned = r->assignedEmergencies();
    if (assigned.empty()) {
        console_.info(r->id() + " is not assigned to any emergency.");
        console_.pause();
        return;
    }
    std::string eid = assigned.front();
    if (assigned.size() > 1) {
        eid = util::toUpper(console_.readText("Release from which emergency? (" + util::trim(eid) + ", ...)", 16));
    }
    if (console_.confirm("Release " + r->id() + " from " + eid + "?")) {
        const OpResult result = dispatcher_.release(eid, r->id(), now());
        if (result.ok) {
            persist();
            log("Released " + r->id() + " from " + eid);
            console_.success(result.message);
        } else {
            console_.error(result.message);
        }
    }
    console_.pause();
}

void RESQApplication::viewAssignmentHistory()
{
    ScreenScope scope(console_, "History");
    console_.printHeader("Assignment history");
    const std::vector<AssignmentRecord>& history = resources_.history();
    if (history.empty()) {
        console_.info("No assignments have been recorded yet.");
        console_.pause();
        return;
    }
    console_.out() << "  Latest " << std::min<size_t>(history.size(), 15) << " of " << history.size()
                   << " record(s), newest first:\n\n";
    Table table({{"#", 4, true}, {"Case", 9}, {"Resource", 9}, {"Assigned", 11}, {"Released", 11}, {"Duration", 8},
                 {"Note", 19}});
    const std::time_t t = now();
    for (size_t shown = 0; shown < history.size() && shown < 15; ++shown) {
        const AssignmentRecord& record = history[history.size() - 1 - shown];
        const std::time_t end = record.isActive() ? t : record.releasedAt;
        table.addRow({{std::to_string(record.id), Style::Muted},
                      {record.emergencyId, Style::Bold},
                      {record.resourceId},
                      {util::formatCompact(record.assignedAt)},
                      {record.isActive() ? "active" : util::formatCompact(record.releasedAt),
                       record.isActive() ? Style::Warning : Style::Plain},
                      {util::formatDuration(end - record.assignedAt)},
                      {record.note, Style::Muted}});
    }
    table.print(console_);
    console_.pause();
}

void RESQApplication::reportsMenu()
{
    ScreenScope scope(console_, "Reports");
    while (true) {
        console_.printHeader("Reports & analytics");
        console_.info("All figures are calculated from the records stored in \"" + files_.dataDirectory() + "\".");
        console_.out() << '\n';
        console_.printMenu({{"1", "Overview & case status"},
                            {"2", "By severity & category"},
                            {"3", "Resource utilization"},
                            {"4", "Response & resolution times"},
                            {"5", "Full report"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        }
        console_.printHeader("Reports & analytics");
        if (choice == "1") {
            reportOverview();
        } else if (choice == "2") {
            reportBreakdown();
        } else if (choice == "3") {
            reportResources();
        } else if (choice == "4") {
            reportTiming();
        } else if (choice == "5") {
            reportOverview();
            reportBreakdown();
            reportResources();
            reportTiming();
        } else {
            console_.error("Invalid option.");
        }
        console_.pause();
    }
}

void RESQApplication::reportOverview()
{
    const Statistics s = analytics_.compute();
    console_.printSection("Overview");
    const int resolved = s.count(Status::Resolved);
    const int open = s.activeCases;
    console_.out() << "  Total emergencies registered   : "
                   << console_.paint(std::to_string(s.totalEmergencies), Style::Highlight)
                   << "\n  Open (pending/assigned/active) : " << open
                   << "\n  Critical cases still open      : "
                   << console_.paint(std::to_string(s.activeCritical), Style::Critical)
                   << "\n  People affected in open cases  : " << s.peopleAffectedActive << '\n';
    if (s.totalEmergencies == 0) {
        console_.info("No emergencies recorded yet - charts appear once cases are registered.");
        return;
    }
    console_.printSection("Cases by status");
    int maxValue = 0;
    for (int v : s.byStatus) {
        maxValue = std::max(maxValue, v);
    }
    for (Status status : allStatuses()) {
        console_.printBar(toString(status), s.count(status), maxValue, statusStyle(status));
    }
    console_.printSection("Resolved versus pending");
    const int pending = s.count(Status::Pending);
    console_.printBar("Resolved", resolved, std::max(resolved, pending), Style::Success);
    console_.printBar("Pending", pending, std::max(resolved, pending), Style::Warning);
    console_.out() << "  " << resolved << " of " << s.totalEmergencies << " cases resolved ("
                   << percent(resolved, s.totalEmergencies) << "); " << open << " still open.\n";
}

void RESQApplication::reportBreakdown()
{
    const Statistics s = analytics_.compute();
    if (s.totalEmergencies == 0) {
        console_.info("No emergencies recorded yet.");
        return;
    }
    console_.printSection("Emergencies by severity");
    int maxSeverity = 0;
    for (int v : s.bySeverity) {
        maxSeverity = std::max(maxSeverity, v);
    }
    for (Severity severity : allSeverities()) {
        const Style style = severity == Severity::Low ? Style::Success : severityStyle(severity);
        console_.printBar(toString(severity), s.bySeverity[static_cast<size_t>(indexOf(severity))], maxSeverity, style);
    }
    console_.printSection("Emergencies by category");
    int maxCategory = 0;
    for (int v : s.byCategory) {
        maxCategory = std::max(maxCategory, v);
    }
    for (Category category : allCategories()) {
        console_.printBar(toString(category), s.byCategory[static_cast<size_t>(indexOf(category))], maxCategory,
                          Style::Accent);
    }
}

void RESQApplication::reportResources()
{
    const Statistics s = analytics_.compute();
    console_.printSection("Resource utilization");
    if (s.totalResources == 0) {
        console_.info("No resources recorded yet.");
        return;
    }
    const double utilization = s.utilizationPercent();
    console_.out() << "  " << s.totalResources << " resources: " << console_.paint(std::to_string(s.availableResources) + " available", Style::Success)
                   << ", " << console_.paint(std::to_string(s.deployedResources) + " deployed", Style::Warning) << ", "
                   << console_.paint(std::to_string(s.outOfServiceResources) + " out of service", Style::Danger) << "\n";
    if (utilization >= 0) {
        console_.printBar("Utilization %", static_cast<int>(std::lround(utilization)), 100, Style::Warning);
    }
    console_.out() << '\n';
    Table table({{"Resource type", 24}, {"Total", 5, true}, {"Available", 9, true}, {"Deployed", 8, true}, {"Out of svc", 10, true}});
    for (ResourceType type : allResourceTypes()) {
        const TypeUsage& u = s.byType[static_cast<size_t>(indexOf(type))];
        table.addRow({{toString(type)},
                      {std::to_string(u.total), Style::Highlight},
                      {std::to_string(u.available), u.available > 0 ? Style::Success : Style::Muted},
                      {std::to_string(u.deployed), u.deployed > 0 ? Style::Warning : Style::Muted},
                      {std::to_string(u.outOfService), u.outOfService > 0 ? Style::Danger : Style::Muted}});
    }
    table.print(console_);
    const auto top = analytics_.mostDeployed(5);
    if (!top.empty()) {
        console_.printSection("Most deployed resources (assignment history)");
        for (const auto& entry : top) {
            console_.printBar(entry.first, entry.second, top.front().second, Style::Accent);
        }
    }
}

void RESQApplication::reportTiming()
{
    const Statistics s = analytics_.compute();
    console_.printSection("Response & resolution times");
    if (s.responseSamples > 0) {
        console_.out() << "  Average time to first assignment : "
                       << console_.paint(util::formatDuration(static_cast<long long>(s.averageResponseSeconds)), Style::Highlight)
                       << "  (" << s.responseSamples << " case(s))\n";
    } else {
        console_.out() << "  Average time to first assignment : not enough data (no case assigned yet)\n";
    }
    if (s.resolutionSamples > 0) {
        console_.out() << "  Average time to resolution       : "
                       << console_.paint(util::formatDuration(static_cast<long long>(s.averageResolutionSeconds)), Style::Highlight)
                       << "  (" << s.resolutionSamples << " resolved case(s))\n";
    } else {
        console_.out() << "  Average time to resolution       : not enough data (no resolved case with valid times)\n";
    }
    console_.out() << "  "
                   << console_.paint("Times come from timestamps recorded in RESQ and depend on operators updating "
                                     "case status promptly.",
                                     Style::Muted)
                   << '\n';
}

void RESQApplication::demoMenu()
{
    ScreenScope scope(console_, "Demo data");
    while (true) {
        console_.printHeader("Demonstration data");
        console_.warning("Demo records are FICTIONAL. Their IDs start with E-D / R-D and are marked * in tables.");
        const size_t demoCases = emergencies_.demoIds().size();
        const size_t demoResources = resources_.demoIds().size();
        console_.out() << "  Demo records loaded: " << demoCases << " cases, " << demoResources << " resources.  "
                       << "Your own records: " << emergencies_.size() - demoCases << " cases, "
                       << resources_.size() - demoResources << " resources.\n\n";
        console_.printMenu({{"1", "Load demo data (keeps your data)"},
                            {"2", "Reset demo data to original"},
                            {"3", "Remove all demo data"},
                            {"4", "Erase ALL data (incl. yours)"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            if (DemoData::present(emergencies_, resources_)) {
                console_.warning("Demo data is already loaded. Use option 2 to restore it to its original state.");
            } else if ((emergencies_.empty() && resources_.empty()) ||
                       console_.confirm("Your existing records are kept and demo records are added beside them. Continue?")) {
                const DemoResult result = DemoData::load(emergencies_, resources_, dispatcher_, now());
                persist();
                log("Demo data loaded");
                console_.success("Added " + std::to_string(result.emergenciesAdded) + " fictional cases and " +
                                 std::to_string(result.resourcesAdded) + " fictional resources.");
                if (result.skipped > 0) {
                    console_.warning(std::to_string(result.skipped) + " demo record(s) skipped because the ID is already used by your data.");
                }
            }
        } else if (choice == "2") {
            if (console_.confirm("Remove every demo record (including changes made to them) and load a fresh copy? "
                                 "Your own records are not affected.")) {
                const int removed = DemoData::removeAll(emergencies_, resources_, dispatcher_, now());
                const DemoResult result = DemoData::load(emergencies_, resources_, dispatcher_, now());
                persist();
                log("Demo data reset");
                console_.success("Removed " + std::to_string(removed) + " demo record(s); loaded " +
                                 std::to_string(result.emergenciesAdded + result.resourcesAdded) + " fresh ones.");
            }
        } else if (choice == "3") {
            if (!DemoData::present(emergencies_, resources_)) {
                console_.info("No demo records are loaded.");
            } else if (console_.confirm("Remove all demo records? Your own records are kept; your cases that used a "
                                        "demo resource lose that assignment.")) {
                const int removed = DemoData::removeAll(emergencies_, resources_, dispatcher_, now());
                persist();
                log("Demo data removed");
                console_.success("Removed " + std::to_string(removed) + " demo record(s).");
            }
        } else if (choice == "4") {
            if (emergencies_.empty() && resources_.empty()) {
                console_.info("There is no data to erase.");
            } else {
                console_.warning("This erases ALL emergencies, resources and history, including your own records.");
                if (console_.confirmWord("Erase everything", "DELETE")) {
                    std::string error;
                    const std::vector<std::string> copies = files_.archiveAll("before-erase", error);
                    if (!error.empty()) {
                        console_.error(error + " Nothing was erased.");
                    } else {
                        emergencies_.clear();
                        resources_.clear();
                        persist();
                        log("All data erased");
                        console_.success("All data erased.");
                        for (const std::string& copy : copies) {
                            console_.info("Previous file archived as " + copy);
                        }
                    }
                } else {
                    console_.info("Confirmation word not entered - nothing was erased.");
                }
            }
        } else {
            console_.error("Invalid option.");
        }
        console_.pause();
    }
}

void RESQApplication::settingsMenu()
{
    ScreenScope scope(console_, "Settings");
    while (true) {
        console_.printHeader("Settings & data files");
        const auto onOff = [this](bool value) {
            return value ? console_.paint("ON", Style::Success) : console_.paint("OFF", Style::Muted);
        };
        console_.out() << "  Colors        : " << onOff(console_.color())
                       << (console_.ansi() ? "" : "  (terminal without ANSI support)") << '\n'
                       << "  Box drawing   : " << (console_.unicode() ? "Unicode" : "ASCII") << '\n'
                       << "  Animations    : " << onOff(console_.animations()) << '\n'
                       << "  Data folder   : " << files_.dataDirectory() << "\n\n";
        console_.printMenu({{"1", "Toggle colors"},
                            {"2", "Toggle Unicode / ASCII borders"},
                            {"3", "Toggle animations"},
                            {"4", "Save all data now"},
                            {"5", "Data file status & load report"},
                            {"6", "How priority is calculated"},
                            {"7", "About & disclaimer"},
                            {"0", "Back"}});
        const std::string choice = console_.readChoice("Choose");
        if (choice == "0") {
            return;
        } else if (choice == "1") {
            if (!console_.ansi()) {
                console_.warning("This terminal does not appear to support ANSI colors (start with --color to force).");
                console_.pause();
            }
            console_.setColor(!console_.color());
        } else if (choice == "2") {
            console_.setUnicode(!console_.unicode());
        } else if (choice == "3") {
            console_.setAnimations(!console_.animations());
        } else if (choice == "4") {
            console_.spinner("Saving ...", 400);
            if (persist()) {
                console_.success("All data saved to " + files_.dataDirectory() + ".");
            }
            console_.pause();
        } else if (choice == "5") {
            showDataStatus();
        } else if (choice == "6") {
            showPriorityHelp();
        } else if (choice == "7") {
            showAbout();
        } else {
            console_.error("Invalid option.");
            console_.pause();
        }
    }
}

void RESQApplication::showDataStatus()
{
    ScreenScope scope(console_, "Data files");
    console_.printHeader("Data file status");
    const auto describe = [](bool found) { return found ? "found at start-up" : "not found at start-up (created on save)"; };
    console_.printBox("FILES",
                      {"emergencies.txt : " + std::string(describe(loadReport_.emergenciesFileFound)),
                       "resources.txt   : " + std::string(describe(loadReport_.resourcesFileFound)),
                       "assignments.txt : " + std::string(describe(loadReport_.assignmentsFileFound)),
                       "Folder          : " + files_.dataDirectory(),
                       "Loaded          : " + std::to_string(loadReport_.emergenciesLoaded) + " emergencies, " +
                           std::to_string(loadReport_.resourcesLoaded) + " resources, " +
                           std::to_string(loadReport_.assignmentsLoaded) + " history records",
                       "Last save       : " + (lastSaved_ > 0 ? util::formatDateTime(lastSaved_) : std::string("-")),
                       "Safety          : previous version kept as <file>.bak on every save"});
    if (loadReport_.warnings.empty() && reconcileNotes_.empty()) {
        console_.success("No problems were found when the files were loaded.");
    }
    for (const std::string& warning : loadReport_.warnings) {
        console_.warning(warning);
    }
    for (const std::string& note : reconcileNotes_) {
        console_.warning("Data check: " + note);
    }
    for (const std::string& path : loadReport_.preserved) {
        console_.info("Original kept as " + path);
    }
    console_.pause();
}

void RESQApplication::showPriorityHelp()
{
    ScreenScope scope(console_, "Priority");
    console_.printHeader("How the priority score is calculated");
    console_.printBox("PRIORITY SCORING RULES", PriorityCalculator::explanation());
    console_.pause();
}

void RESQApplication::showAbout()
{
    ScreenScope scope(console_, "About");
    console_.printHeader("About RESQ");
    console_.printBox(
        "RESQ " + std::string(kVersion),
        {"Rescue and Emergency Support System - an academic C/C++ prototype.",
         "",
         console_.paint("IMPORTANT DISCLAIMER", Style::Critical),
         "- RESQ is NOT connected to any real emergency service.",
         "- It does not predict disasters or risk. Priority scores only order",
         "  the queue using fixed, illustrative rules.",
         "- It has no live locations or real-time resource availability. All",
         "  data is what operators type in, or the fictional demo records.",
         "- Human operators remain responsible for every decision.",
         "",
         "Privacy: names and contact details are obfuscated in the data files",
         "so they are not readable at a glance. This is NOT encryption - protect",
         "the data folder with operating-system permissions."},
        Style::Danger);
    console_.pause();
}

void RESQApplication::showHelp()
{
    ScreenScope scope(console_, "Help");
    console_.printHeader("Help - quick guide");
    console_.printBox("TYPICAL WORKFLOW",
                      {"1. Register each incoming emergency (menu 1).",
                       "2. Open Dispatch (menu 5) > Process next: RESQ shows the",
                       "   highest-priority pending case and suitable resources.",
                       "3. Assign one or more resources - the case becomes Assigned.",
                       "4. When crews start work, mark the case In Progress.",
                       "5. When the operation is complete, mark it Resolved -",
                       "   its resources are released automatically.",
                       "",
                       "Tips: type :q in any form to cancel it. Tables mark fictional",
                       "demo records with *. Data is saved after every change."});
    console_.out() << '\n';
    console_.printBox("PRIORITY SCORE", PriorityCalculator::explanation());
    console_.out() << "\n  " << console_.paint("RESQ is an academic prototype and is not connected to real emergency "
                                               "services.",
                                               Style::Muted)
                   << '\n';
    console_.pause();
}

}

#include <exception>
#include <iostream>

int main(int argc, char* argv[])
{
    resq::AppOptions options;
    std::string error;
    if (!resq::AppOptions::parse(argc, argv, options, error)) {
        std::cerr << "resq: " << error << "\n\n" << resq::AppOptions::usage();
        return 2;
    }
    if (options.showHelp) {
        std::cout << resq::AppOptions::usage();
        return 0;
    }
    if (options.showVersion) {
        std::cout << "RESQ " << resq::kVersion << '\n';
        return 0;
    }
    try {
        resq::RESQApplication app(options);
        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "RESQ stopped because of an unexpected error: " << ex.what() << '\n';
        return 1;
    }
}
