#ifndef RESQ_FILE_MANAGER_H
#define RESQ_FILE_MANAGER_H

#include "emergency_manager.h"
#include "resource_manager.h"

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
