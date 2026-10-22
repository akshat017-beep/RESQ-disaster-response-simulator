#ifndef RESQ_FILE_MANAGER_H
#define RESQ_FILE_MANAGER_H

#include "resource_manager.h"

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
