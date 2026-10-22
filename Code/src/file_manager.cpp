#include "file_manager.h"

#include "resq_c.h"

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
