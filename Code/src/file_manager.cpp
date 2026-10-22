#include "file_manager.h"

#include "resq_c.h"
#include "util.h"

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
