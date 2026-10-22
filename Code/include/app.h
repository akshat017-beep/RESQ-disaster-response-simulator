#ifndef RESQ_APP_H
#define RESQ_APP_H

#include "analytics.h"
#include "console.h"
#include "file_manager.h"

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
